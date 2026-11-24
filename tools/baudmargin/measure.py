"""Margine di baud tra scheda e adattatore USB (sketch baudmargin.ino caricato sulla scheda).

    python measure.py <porta> [baud della scheda, default 115200] [fine|-] [campioni per bit: 8|16] [bit di stop del PC: 1|2]

"fine" esplora a passi di 0,1% la zona in cui la scheda e' piu' veloce del PC.

La scheda resta al suo baud reale; il PC cambia il proprio e per ogni valore si contano:
  - le righe arrivate rovinate dalla scheda al PC, inviate di fila e con pause tra i byte;
  - i byte arrivati rovinati dal PC alla scheda.
La colonna "scarto" e' la differenza tra il baud chiesto al PC e quello reale della scheda.
"""
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else 'COM3'
NOMINAL = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
F_CPU = 16000000
SAMPLES = int(sys.argv[4]) if len(sys.argv) > 4 else 8
PC_STOP_BITS = int(sys.argv[5]) if len(sys.argv) > 5 else 1
DIVISOR = (F_CPU // (SAMPLES // 2) // NOMINAL - 1) // 2
BOARD = F_CPU / (SAMPLES * (DIVISOR + 1))  # baud che la scheda genera davvero
LINE = b'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabc\n'

ser = serial.Serial(PORT, int(round(BOARD)), timeout=0.05,
                    stopbits=serial.STOPBITS_TWO if PC_STOP_BITS == 2 else serial.STOPBITS_ONE)
time.sleep(2.0)  # bootloader
ser.reset_input_buffer()


def set_baud(value):
    ser.baudrate = int(round(value))
    time.sleep(0.03)


def read_for(seconds):
    data = b''
    end = time.time() + seconds
    while time.time() < end:
        data += ser.read(4096)
    return data


def transmit_test(baud, command):
    """Ritorna (righe buone, byte ricevuti) su 100 righe / 4000 byte attesi."""
    set_baud(BOARD)
    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()
    time.sleep(0.1)
    set_baud(baud)
    data = read_for(1.7 if command == b'G' else 1.0)
    good = sum(1 for line in data.split(b'\n') if line + b'\n' == LINE)
    return good, len(data)


def receive_test(baud):
    """Ritorna (byte ricevuti, rovinati, errori di trama, sovrascritture) su 4000 byte inviati."""
    set_baud(BOARD)
    ser.reset_input_buffer()
    ser.write(b'R')
    ser.flush()
    time.sleep(0.1)
    set_baud(baud)
    ser.write(LINE * 100)
    ser.flush()
    time.sleep(1.7)
    set_baud(BOARD)
    ser.reset_input_buffer()
    ser.write(b'?')
    line = b''
    end = time.time() + 1.0
    while time.time() < end and not line.endswith(b'\n'):
        line += ser.read(1)
    parts = line.split()
    return tuple(int(x) for x in parts[1:5]) if len(parts) == 5 and parts[0] == b'RX' else None


def board_rate():
    """Baud reale della scheda, dalla durata di 240000 byte inviati di fila."""
    set_baud(BOARD)
    ser.reset_input_buffer()
    ser.write(b'L')
    first = last = None
    total = 0
    end = time.time() + 30
    while time.time() < end:
        chunk = ser.read(4096)
        now = time.perf_counter()
        if chunk:
            if first is None:
                first, first_bytes = now, len(chunk)
            last = now
            total += len(chunk)
        elif first is not None and now - last > 0.5:
            break
    if first is None or total < 1000:
        return None, total
    return (total - first_bytes) * 10 / (last - first), total


print('scheda: divisore %d -> %.0f baud nominali (%+.2f%% rispetto a %d)'
      % (DIVISOR, BOARD, (BOARD / NOMINAL - 1) * 100, NOMINAL))
rate, total = board_rate()
if rate:
    print('baud reale misurato: %.0f (%+.2f%% rispetto al nominale della scheda), %d byte su 240000'
          % (rate, (rate / BOARD - 1) * 100, total))

print()
print('baud PC   scarto   scheda->PC di fila   scheda->PC con pause   PC->scheda (rovinati, trama, sovrascr.)')
steps = [-6, -5, -4, -3.5, -3, -2.5, -2.08, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6]
if len(sys.argv) > 3 and sys.argv[3] == 'fine':
    steps = [x / 10.0 for x in range(-45, -14)]
for percent in steps:
    baud = BOARD * (1 + percent / 100.0)
    packed = [transmit_test(baud, b'T') for _ in range(3)]
    gapped = [transmit_test(baud, b'G') for _ in range(2)]
    received = [receive_test(baud) for _ in range(2)]
    good = sum(g for g, _ in packed)
    good_gapped = sum(g for g, _ in gapped)
    rx = [r for r in received if r]
    rx_text = '%d/%d rovinati, %d, %d' % (sum(r[1] for r in rx), sum(r[0] for r in rx),
                                          sum(r[2] for r in rx), sum(r[3] for r in rx)) if rx else 'nessun rapporto'
    mark = '  <- %d chiesti al PC' % NOMINAL if abs(baud - NOMINAL) < 300 else ''
    print('%7.0f  %+5.2f%%   %3d/300 righe buone    %3d/200 righe buone    %s%s'
          % (baud, percent, good, good_gapped, rx_text, mark))
ser.close()
