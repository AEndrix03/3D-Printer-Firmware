"""Banco prova sul firmware reale: protocollo, velocita' della seriale, CPU e stack.

    python tools/hw_bench.py COM3        (serve pyserial)

ATTENZIONE: genera movimenti veri su X, Y, Z ed E. Va usato a scheda nuda oppure con
gli assi liberi di muoversi di 150 mm; non richiede finecorsa ne' termistore.
Le righe PRF riportano: RAM minima rimasta libera, durata massima dell'interrupt
degli step (us), scadenze mancate.
"""
import sys, time, re
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM3'
ser = serial.Serial(PORT, 115200, timeout=0.05)
n = 0
stats = {'retx': 0, 'e01': 0, 'timeouts': 0}
TERMINAL = re.compile(r'^(OK0|E0\d|EM0|ET0|ES\d) N(\d+)(?: D(\d+))?')


def readline(timeout):
    end = time.time() + timeout
    buf = b''
    while time.time() < end:
        c = ser.read(1)
        if not c:
            continue
        if c == b'\n':
            return buf.decode('latin1').strip()
        buf += c
    return None


def frame(num, body):
    payload = 'N%d %s' % (num, body)
    cs = 0
    for ch in payload.encode():
        cs ^= ch
    return ('%s *%d\n' % (payload, cs)).encode()


def send(body, sync=False, timeout=8.0, quiet=True):
    """Invia un comando e attende la risposta terminale. Ritorna (codice, D, righe informative)."""
    global n
    num = 0 if sync else n + 1
    info = []
    for attempt in range(4):
        ser.write(frame(num, body))
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = readline(0.3)
            if line is None:
                continue
            m = TERMINAL.match(line)
            if not m:
                if not line.startswith('BUSY'):
                    info.append(line)
                continue
            code, rn, d = m.group(1), int(m.group(2)), m.group(3)
            if code == 'E01':
                stats['e01'] += 1
                break  # ritrasmetti
            if code == 'E03' or rn == num or code == 'OK0':
                n = num
                if not quiet:
                    print('   ', body, '->', line, info)
                return code, int(d) if d else None, info
        else:
            stats['timeouts'] += 1
        stats['retx'] += 1
    raise RuntimeError('no answer to %r' % body)


def wait_idle(limit=120.0):
    end = time.time() + limit
    while time.time() < end:
        code, d, _ = send('S10')
        if d == n:
            return True
        time.sleep(0.05)
    return False


def diag(label):
    _, _, info = send('S11')
    dia = next((l for l in info if l.startswith('DIA')), '')
    prf = next((l for l in info if l.startswith('PRF')), '')
    print('%-34s %s | %s' % (label, dia, prf))
    return dia, prf


# --- avvio ---
print('== boot')
t0 = time.time()
while time.time() - t0 < 6:
    line = readline(0.5)
    if line:
        print('   ', line)
        if 'pronto' in line:
            break
print('sync:', send('S10', sync=True)[0])
diag('a riposo')
print('start print:', send('S1')[0])

# --- 1. movimento lungo alla massima velocita' (X+Y in diagonale: 2 assi a ~6,7 kHz) ---
t = time.time()
send('M10 X150 Y150 F5000')
wait_idle()
print('diagonale 212 mm F5000: %.2f s' % (time.time() - t))
diag('dopo diagonale veloce')

# --- 2. quattro assi insieme (caso peggiore dell'interrupt: X Y Z E) ---
t = time.time()
send('M10 X-150 Y-150 Z-2 E20 F5000')
wait_idle()
print('ritorno a 4 assi: %.2f s' % (time.time() - t))
diag('dopo 4 assi')

# --- 3. flusso di segmenti corti, come in una curva: quanti al secondo passano? ---
for seg_mm, feed, count in ((0.5, 3000, 600), (0.2, 3000, 600), (0.2, 6000, 600)):
    before = dict(stats)
    t = time.time()
    for i in range(count):
        dy = 0.05 if i % 2 == 0 else -0.05
        send('M10 X%.3f Y%.3f E0.01' % (seg_mm, dy) + (' F%d' % feed if i == 0 else ''))
    stream = time.time() - t
    wait_idle()
    total = time.time() - t
    ideal = count * seg_mm / (feed / 60.0)
    print('segmenti %.1f mm F%d: %d in %.2f s = %.0f seg/s (ideale alla velocita\' richiesta %.2f s, reale %.2f s) '
          'ritrasmissioni %d, E01 %d'
          % (seg_mm, feed, count, stream, count / stream, ideal, total,
             stats['retx'] - before['retx'], stats['e01'] - before['e01']))
    diag('dopo segmenti %.1f mm F%d' % (seg_mm, feed))
    send('M10 X-%.3f F6000' % (count * seg_mm))
    wait_idle()

# --- 4. arresto in tempo reale durante un movimento ---
send('M10 X100 F3000')
time.sleep(0.4)
t = time.time()
ser.write(b'!')
wait_idle(10)
print('arresto con "!": fermo in %.2f s' % (time.time() - t))

print('end print:', send('S12')[0])
diag('finale')
print('totale ritrasmissioni:', stats)
ser.close()
