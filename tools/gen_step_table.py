"""Genera src/include/motion/StepTimerTable.hpp.

L'interrupt degli step deve convertire una velocita' (step/s) nell'intervallo del
timer (tick da 0,5 us): ticks = 2.000.000 / rate. Sull'ATmega una divisione a 32 bit
costa circa 40 us, troppo per farla a ogni step. La tabella sostituisce la divisione
con una lettura e una moltiplicazione 16x8 (interpolazione lineare).

Due fasce, per tenere l'errore sotto l'1% su tutto il campo:
  - lenta:  rate <  512, passo 8   -> 64 voci
  - veloce: rate >= 512, passo 128 -> 256 voci (fino a 32767 step/s)
Ogni voce: valore all'inizio della fascia e differenza con la voce successiva.

    python tools/gen_step_table.py
"""
import os

TICKS_PER_SECOND = 2000000
MIN_RATE = 31  # sotto, l'intervallo non starebbe in 16 bit


def ticks(rate):
    return min(65535, round(TICKS_PER_SECOND / max(rate, MIN_RATE)))


def table(step, count, first):
    rows = []
    for i in range(count):
        lo = first + i * step
        base = ticks(lo)
        rows.append((base, base - ticks(lo + step)))
    return rows


def emit(name, rows):
    out = ['    const uint16_t %s[%d][2] PROGMEM = {' % (name, len(rows))]
    for i in range(0, len(rows), 6):
        out.append('            ' + ' '.join('{%d, %d},' % r for r in rows[i:i + 6]))
    out.append('    };')
    return '\n'.join(out)


slow = table(8, 64, 0)
fast = table(128, 256, 0)

# errore massimo dell'interpolazione, per documentarlo nell'header
worst = 0.0
for rate in range(40, 32768):
    if rate < 512:
        base, delta = slow[rate >> 3]
        value = base - ((delta * (rate & 7)) >> 3)
    else:
        base, delta = fast[rate >> 7]
        value = base - ((delta * (rate & 127)) >> 7)
    exact = TICKS_PER_SECOND / rate
    worst = max(worst, abs(value - exact) / exact)

header = '''#pragma once

// GENERATO da tools/gen_step_table.py: non modificare a mano.
// Conversione velocita' (step/s) -> intervallo del timer (tick da 0,5 us) senza divisioni.
// Errore massimo dell'interpolazione tra 40 e 32767 step/s: %.2f%%.

#include <stdint.h>

namespace StepTimerTable {
%s

%s
}
''' % (worst * 100, emit('SLOW', slow), emit('FAST', fast))

path = os.path.join(os.path.dirname(__file__), '..', 'src', 'include', 'motion', 'StepTimerTable.hpp')
with open(path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(header)
print('errore massimo %.3f%%, %d byte di flash' % (worst * 100, (len(slow) + len(fast)) * 4))
