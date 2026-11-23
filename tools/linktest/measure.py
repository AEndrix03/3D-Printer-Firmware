"""Misura del collegamento: giro del comando al variare di formato, lunghezza e comandi in volo."""
import sys, time
import serial

port, baud, ack_len = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ser = serial.Serial(port, baud, timeout=2.0)
time.sleep(2.2)
ser.reset_input_buffer()

# Comandi rappresentativi
TODAY = b'fN1234 M10 X0.200 Y-0.050 E0.0100 *87\n'                     # oggi: decimali, pianifica il firmware
PLANNED_ASCII = b'aN1234 M20 X16 Y-4 E1 V1200 C4000 W900 A80000 *87\n'  # pianificato dal PC, interi in ASCII
PLANNED_BIN = b'b' + bytes([18]) + bytes(range(18))                     # stesso contenuto in binario (18 byte)
SHORT = b'aN1234 S10 *87\n'


def run(label, frame, count, window):
    """Invia `count` comandi tenendone al massimo `window` senza risposta."""
    sent = acked = 0
    buf = b''
    t = time.perf_counter()
    while acked < count:
        while sent < count and sent - acked < window:
            ser.write(frame)
            sent += 1
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            print('%-44s TIMEOUT dopo %d risposte' % (label, acked))
            ser.reset_input_buffer()
            return None
        buf += chunk
        n = buf.count(b'\n')
        if n:
            acked += n
            buf = buf[buf.rfind(b'\n') + 1:]
    dt = (time.perf_counter() - t) / count * 1000
    print('%-44s %5.2f ms = %4.0f al secondo' % (label, dt, 1000 / dt))
    return dt


TEXT30 = b'xN1234 P' + b'A' * 20 + b' *87' + bytes([10])   # pianificato, interi impacchettati in testo (30 byte)
RXBUF = int(sys.argv[4]) if len(sys.argv) > 4 else 64
print('--- %d baud, risposta di %d byte, buffer di ricezione %d ---' % (baud, ack_len, RXBUF))
for label, frame in (('pianificato testo compatto (%d byte)' % len(TEXT30), TEXT30),
                     ('pianificato binario (%d byte)' % len(PLANNED_BIN), PLANNED_BIN)):
    for window in (1, 2, 3, 4, 6):
        if window * len(frame) > RXBUF - 2:
            continue
        run('finestra %d, %s' % (window, label), frame, 400, window)
ser.close()
