"""Banco prova sul firmware reale: protocollo, velocita' della seriale, CPU e stack.

    python tools/hw_bench.py COM3        (serve pyserial)

ATTENZIONE: genera movimenti veri su X, Y, Z ed E. Va usato a scheda nuda oppure con
gli assi liberi di muoversi di 150 mm; non richiede finecorsa ne' termistore.
Le righe PRF riportano: RAM minima rimasta libera, durata massima dell'interrupt
degli step (us), scadenze mancate.
"""
import math
import sys, time, re
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM3'
ser = serial.Serial(PORT, 250000, timeout=0.05, stopbits=serial.STOPBITS_TWO)
n = 0
stats = {'retx': 0, 'e01': 0, 'timeouts': 0, 'rx_corrupt': 0}
TERMINAL = re.compile(r'^(OK0|E0\d|EM0|ET0|ES\d) N(\d+)(?: D(\d+))?')


def readline(timeout):
    end = time.time() + timeout
    buf = b''
    while time.time() < end:
        c = ser.read(1)
        if not c:
            continue
        if c == b'\n':
            return buf.decode('latin1').strip('\r\n \t')  # non strip(): toglierebbe anche byte delle conferme
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
            print('    nessuna risposta a %r; righe ricevute: %r' % (body, info[-4:]))
        stats['retx'] += 1
    raise RuntimeError('no answer to %r' % body)


def checksum_ok(line):
    """Le righe del firmware finiscono con ' *<xor dei caratteri precedenti>'."""
    body, star, tail = line.rpartition(' *')
    if not star or not tail.isdigit():
        return False
    cs = 0
    for ch in body.encode('latin1'):
        cs ^= ch
    return cs == int(tail)


def crc_update(crc, data):
    data ^= crc & 0xFF
    data = (data ^ (data << 4)) & 0xFF
    return ((((data << 8) | (crc >> 8)) ^ (data >> 4) ^ (data << 3)) & 0xFFFF)


def move_frame(num, delta, plan, with_plan):
    """Frame binario di movimento gia' pianificato (vedi src/include/MoveFrame.hpp).
    plan = (velocita' nominale 1/64 mm/s, accelerazione mm/s^2, scala 1/16 step/mm, ingresso 1/4 mm/s)."""
    wide = any(abs(d) > 2047 for d in delta)
    header = 0xC0 | (0x10 if wide else 0) | (0x20 if with_plan else 0)
    for axis, d in enumerate(delta):
        if d:
            header |= 0x08 >> axis
    out = [header, 0x80 | (num & 0x3F)]

    def put(value, sextets):
        out.extend(0x80 | ((value >> (6 * i)) & 0x3F) for i in range(sextets - 1, -1, -1))

    for d in delta:
        if d:
            put(d & 0xFFFFFFFF, 3 if wide else 2)
    if with_plan:
        put(plan[0], 3)
        put(plan[1], 3)
    put(plan[2], 3)
    put(min(plan[3], 4095), 2)
    crc = 0xFFFF
    for b in out:
        crc = crc_update(crc, b)
    put(crc & 0x0FFF, 2)
    return bytes(out)


# Limiti della macchina (MotionConfig.hpp): il banco prova pianifica come fa il driver,
# in forma ridotta (tutta la sequenza in una volta, senza linear advance).
STEPS_PER_MM = (80.0, 80.0, 400.0, 80.0)
MAX_FEED = (5000 / 60.0, 5000 / 60.0, 150 / 60.0, 2000 / 60.0)
MAX_ACCEL = (1000.0, 1000.0, 100.0, 1500.0)
JUNCTION_DEVIATION = 0.02
MAX_STEP_RATE = 10000.0


def plan_moves(moves):
    """moves = [(delta, mm/min)] -> [(delta, piano)] con le velocita' di giunzione gia' decise."""
    segs = []
    for delta, feed in moves:
        mm = [d / k for d, k in zip(delta, STEPS_PER_MM)]
        path = math.sqrt(mm[0] ** 2 + mm[1] ** 2 + mm[2] ** 2)
        cartesian = path > 0
        length = path if cartesian else abs(mm[3])
        count = max(abs(d) for d in delta)
        speed, accel = feed / 60.0, (MAX_ACCEL[0] if cartesian else MAX_ACCEL[3])
        for a in range(4):
            share = abs(mm[a]) / length
            if share > 0:
                speed = min(speed, MAX_FEED[a] / share)
                accel = min(accel, MAX_ACCEL[a] / share)
        speed = min(speed, MAX_STEP_RATE * length / count)
        segs.append({
            'delta': delta, 'length': length, 'cartesian': cartesian,
            'unit': [m / length for m in mm[:3]] if cartesian else [0, 0, 0],
            'nominal': int(speed * 64) / 64.0, 'accel': int(accel),
            'scale': int(round(count / length * 16)), 'entry': 0.0,
        })
    for k in range(1, len(segs)):
        a, b = segs[k - 1], segs[k]
        if not (a['cartesian'] and b['cartesian']):
            continue
        cos = -sum(x * y for x, y in zip(a['unit'], b['unit']))
        limit = min(a['nominal'], b['nominal'])
        if cos < -0.999999:
            b['entry'] = limit
        elif cos < 0.999999:
            half = math.sqrt(0.5 * (1 - cos))
            b['entry'] = min(limit, math.sqrt(0.9 * min(a['accel'], b['accel']) * JUNCTION_DEVIATION * half / (1 - half)))
    exit_speed = 0.0
    for seg in reversed(segs):  # all'indietro: si deve poter frenare
        seg['entry'] = min(seg['entry'], math.sqrt(exit_speed ** 2 + 1.8 * seg['accel'] * seg['length']))
        exit_speed = seg['entry']
    for k in range(1, len(segs)):  # in avanti: non piu' di quanto si riesce ad accelerare
        a = segs[k - 1]
        segs[k]['entry'] = min(segs[k]['entry'], math.sqrt(a['entry'] ** 2 + 1.8 * a['accel'] * a['length']))
    return [(seg['delta'], (int(seg['nominal'] * 64), seg['accel'], seg['scale'], int(seg['entry'] * 4)))
            for seg in segs]


def move_ack(line, pending):
    """Conferma corta di un movimento: 4 byte con il bit 7 a 1. Ritorna il numero del comando
    confermato, 'corrupt' se il controllo non torna, None se la riga non e' una conferma."""
    data = line.encode('latin1')
    if len(data) != 4 or any((b & 0xC0) != 0x80 for b in data):
        return None
    crc = crc_update(crc_update(0xFFFF, data[0]), data[1]) & 0x0FFF
    if crc != (((data[2] & 0x3F) << 6) | (data[3] & 0x3F)):
        return 'corrupt'
    for number, _ in pending:
        if (number & 0x3F) == (data[0] & 0x3F):
            return number
    return None


WINDOW_FRAMES = 8
recent = []
WINDOW_BYTES = 120  # buffer di ricezione del firmware (128) meno un margine


def stream(moves, timeout=8.0):
    """Pianifica e invia i movimenti binari tenendone al massimo WINDOW_FRAMES in volo.
    `moves` = [(delta, mm/min)]."""
    global n
    moves = plan_moves(moves)
    sent_plan = None  # velocita' e accelerazione viaggiano solo quando cambiano
    pending = []  # (numero, frame)
    index = 0
    while index < len(moves) or pending:
        while index < len(moves) and len(pending) < WINDOW_FRAMES:
            delta, plan = moves[index]
            frame = move_frame(n + 1, delta, plan, plan[:2] != sent_plan)
            if sum(len(f) for _, f in pending) + len(frame) > WINDOW_BYTES:
                break
            sent_plan = plan[:2]
            n += 1
            ser.write(frame)
            pending.append((n, frame))
            index += 1
        line = readline(timeout)
        recent.append(line)
        del recent[:-14]
        if line is None:
            stats['timeouts'] += 1
            raise RuntimeError('no answer to move N%d; pending %s; last lines %r'
                               % (pending[0][0], [num for num, _ in pending], recent))
        ack = move_ack(line, pending)
        if ack == 'corrupt':
            stats['rx_corrupt'] += 1
            print('    conferma rovinata: %r' % line)
            continue
        if ack is not None:
            while pending and pending[0][0] <= ack:
                pending.pop(0)
            continue
        m = TERMINAL.match(line)
        if not m:
            continue
        code, rn = m.group(1), int(m.group(2))
        if not checksum_ok(line):
            stats['rx_corrupt'] += 1  # riga arrivata rovinata: si ignora, come fa il driver
            if stats['rx_corrupt'] <= 8:
                print('    riga rovinata: %r' % line)
            continue
        if code in ('OK0', 'E03') and pending and rn >= pending[0][0] and rn <= pending[-1][0]:
            # conferma cumulativa: l'OK di un movimento vale anche per quelli prima di lui
            while pending and pending[0][0] <= rn:
                pending.pop(0)
        elif code in ('E01', 'E04'):
            stats['e01'] += 1
            stats['retx'] += 1
            time.sleep(0.15)
            ser.reset_input_buffer()
            pending = [(num, f) for num, f in pending if num >= rn]
            for _, f in pending:
                ser.write(f)
        elif code != 'OK0':
            raise RuntimeError('move refused: ' + line)


def wait_idle(limit=120.0):
    end = time.time() + limit
    while time.time() < end:
        code, d, _ = send('S10')
        if d == n:
            return True
        time.sleep(0.05)
    return False


# Soglie di accettazione: una modifica al firmware che le supera non va tenuta.
MIN_FREE_RAM = 600      # byte liberi al picco di stack
MAX_ISR_US = 75         # durata massima dell'interrupt degli step
worst = {'ram': 99999, 'isr': 0, 'overruns': 0, 'cs': 0, 'overflow': 0, 'ack': 0, 'line': 0}


def diag(label):
    _, _, info = send('S11')
    dia = next((l for l in info if l.startswith('DIA')), '')
    prf = next((l for l in info if l.startswith('PRF')), '')
    print('%-34s %s | %s' % (label, dia, prf))
    for extra in info:
        if not extra.startswith(('DIA', 'PRF')):
            print('%-34s %s' % ('', extra))
    d = dia.split('*')[0].split()
    p = prf.split('*')[0].split()
    if len(d) >= 8 and len(p) >= 4:
        worst['cs'] = max(worst['cs'], int(d[4]))
        worst['overflow'] = max(worst['overflow'], int(d[5]))
        worst['ack'] = max(worst['ack'], int(d[6]))
        worst['ram'] = min(worst['ram'], int(p[1]))
        worst['isr'] = max(worst['isr'], int(p[2]))
        worst['overruns'] += int(p[3])
        worst['line'] += int(p[4]) if len(p) > 4 and p[4].isdigit() else 0
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
stream([((12000, 12000, 0, 0), 5000)])
wait_idle()
print('diagonale 212 mm F5000: %.2f s' % (time.time() - t))
diag('dopo diagonale veloce')

# --- 2. quattro assi insieme (caso peggiore dell'interrupt: X Y Z E) ---
t = time.time()
stream([((-12000, -12000, -800, 1600), 5000)])
wait_idle()
print('ritorno a 4 assi: %.2f s' % (time.time() - t))
diag('dopo 4 assi')

# --- 3. flusso di segmenti corti in formato binario, 4 in volo: quanti al secondo passano? ---
for seg_mm, feed, count in ((0.5, 3000, 600), (0.2, 3000, 600), (0.2, 4800, 800), (0.1, 4800, 800)):
    before = dict(stats)
    t = time.time()
    moves = []
    for i in range(count):
        dy = 4 if i % 2 == 0 else -4
        moves.append(((int(round(seg_mm * 80)), dy, 0, 1), feed))
    stream(moves)
    stream_time = time.time() - t
    wait_idle()
    total = time.time() - t
    ideal = count * seg_mm / (feed / 60.0)
    print('segmenti %.1f mm F%d: %d in %.2f s = %.0f seg/s (ideale alla velocita\' richiesta %.2f s, reale %.2f s) '
          'ritrasmissioni %d, E01 %d'
          % (seg_mm, feed, count, stream_time, count / stream_time, ideal, total,
             stats['retx'] - before['retx'], stats['e01'] - before['e01']))
    diag('dopo segmenti %.1f mm F%d' % (seg_mm, feed))
    stream([((-int(round(count * seg_mm * 80)), 0, 0, 0), 6000)])
    wait_idle()

# --- 3b. capacita' del collegamento: segmenti minuscoli in rettilineo, chiesti piu' in fretta di
# quanto possano arrivare. Qui non sono gli spigoli a rallentare la macchina: il numero di
# segmenti al secondo e' quello che firmware e seriale riescono a smaltire.
for seg_steps, count in ((4, 1500), (8, 1500)):
    before = dict(stats)
    t = time.time()
    stream([((seg_steps, 0, 0, 0), 5000) for _ in range(count)])
    stream_time = time.time() - t
    wait_idle()
    print('rettilineo, segmenti da %.2f mm a F5000: %d in %.2f s = %.0f seg/s (%.0f mm/s su 83 richiesti) '
          'ritrasmissioni %d' % (seg_steps / 80.0, count, stream_time, count / stream_time,
                                 count / stream_time * seg_steps / 80.0, stats['retx'] - before['retx']))
    diag('dopo rettilineo %.2f mm' % (seg_steps / 80.0))
    stream([((-seg_steps * count, 0, 0, 0), 6000)])
    wait_idle()

# --- 4. arresto in tempo reale durante un movimento ---
stream([((8000, 0, 0, 0), 3000)])
time.sleep(0.4)
t = time.time()
ser.write(b'!')
wait_idle(10)
print('arresto con "!": fermo in %.2f s' % (time.time() - t))

print('end print:', send('S12')[0])
diag('finale')
print('totale ritrasmissioni:', stats)
ser.close()

problems = []
if worst['ram'] < MIN_FREE_RAM:
    problems.append('RAM libera al picco %d < %d' % (worst['ram'], MIN_FREE_RAM))
if worst['isr'] > MAX_ISR_US:
    problems.append('interrupt degli step %d us > %d' % (worst['isr'], MAX_ISR_US))
if worst['overruns']:
    problems.append('%d scadenze degli step mancate' % worst['overruns'])
if worst['line'] or worst['cs'] or worst['overflow'] or worst['ack'] or stats['retx'] or stats['e01'] or stats['rx_corrupt'] > 1:
    problems.append('errori sul collegamento: %s %s' % (worst, stats))

print()
print('RAM libera minima %d byte, interrupt massimo %d us, scadenze mancate %d'
      % (worst['ram'], worst['isr'], worst['overruns']))
if stats['rx_corrupt']:
    print('righe del firmware arrivate rovinate al PC: %d (scartate dal checksum, nessun effetto)' % stats['rx_corrupt'])
if problems:
    print('FALLITO:')
    for line in problems:
        print('  -', line)
    sys.exit(1)
print('OK: tutte le soglie rispettate')
