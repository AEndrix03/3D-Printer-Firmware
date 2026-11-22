# Protocollo seriale Firmware <-> Driver

115200 baud, 8N1, nessun flow control. Righe ASCII terminate da `\n`.
Protocollo **stop-and-wait**: l'host invia un comando e attende la sua risposta
terminale prima di inviare il successivo.

## Comando (host -> firmware)

```
N<numero> <categoria><codice> [parametri] *<checksum>
N124 M11 X100.5 Y200.3 E0.4213 F3000 *87
```

- `checksum`: XOR di tutti i caratteri prima di ` *`, in decimale. E' obbligatorio.
- Lunghezza massima della riga: 63 caratteri (buffer RX hardware dell'Arduino Uno).
- `N` deve essere l'ultimo accettato + 1.
- **`N0` e' il comando di sincronizzazione**: viene sempre accettato e riporta la
  sequenza a zero (il successivo e' `N1`). Il driver lo usa come handshake dopo
  un timeout o un riavvio del firmware (`N0 S10`).
- Il carattere `!` e' un comando in tempo reale, valido in qualsiasi momento:
  arresto controllato. La macchina decelera lungo il percorso, i movimenti in coda
  vengono scartati e la posizione resta esatta. Un comando che stava aspettando
  risponde `E06` con `ERR ABORTED`.

## Risposta terminale (firmware -> host)

**Esattamente una per comando**, sempre con il numero del comando:

| Codice | Significato |
|--------|-------------|
| `OK0 N<n> D<d>` | accettato. `d` = ultimo comando la cui esecuzione e' terminata |
| `E01 N<atteso>` | checksum errato o riga non valida: ritrasmettere |
| `E02 N<atteso>` | riga troppo lunga, scartata |
| `E03 N<n>` | duplicato: il comando `n` era gia' stato eseguito (risposta persa) |
| `E04 N<atteso>` | numero fuori sequenza: il firmware si aspetta `atteso` |
| `E05 N<n>` | categoria sconosciuta |
| `E06 N<n>` | comando rifiutato; il motivo e' nella riga `ERR <motivo>` che precede |
| `EM0 N<n>` / `ET0 N<n>` | macchina in stato di errore: movimento / riscaldamento bloccati |
| `ES0 N<n>` / `ES1 N<n>` | `S6`: errore azzerato / nessun errore attivo |

## Movimenti in coda

I comandi di movimento (`M10`, `M11`, `A10`-`A13`) **non aspettano la fine del
movimento**: rispondono `OK` appena il segmento e' nella coda del planner (7 posti)
oppure, a coda piena, appena se ne libera uno. Mentre la macchina si muove l'host
invia gia' i segmenti successivi e il planner li raccorda senza fermate (lookahead
con junction deviation); gli step sono generati in interrupt di timer.

Conseguenze per l'host:

- `OK0 N<n> D<d>`: `d` e' il numero dell'ultimo comando completamente eseguito
  (uguale a `n` quando la macchina e' ferma). Per sapere "a che punto e' davvero"
  si guarda `D`, non l'ultimo `OK`.
- Aspettano la fine dei movimenti prima di rispondere: `M12`, `M99`, `S0`, `S2`,
  `S12`, `A14`, `A15`. Tutti gli altri (temperatura, ventola, stato, `M114`) vengono
  eseguiti subito.
- Un errore durante un movimento gia' accodato (finecorsa, allarme termico) non puo'
  piu' essere riportato sul suo comando: la macchina va in stato di errore
  (`ALM <motivo>`) e il comando successivo riceve `EM0`.

## Dati (firmware -> host)

Tutte le righe hanno il checksum (` *<xor>`), tranne `BUSY` e i due banner di avvio.

- **Informativi**: `STA Idle`, `HOM X OK`, `ERR INV_STATE`, `ALM THERM_ERR`, `PWR IDLE`, ...
- **Critici** (prefisso `CRT`): `CRT POS 10.000 20.000 0.500`, `CRT TMP 205.4 200.0`,
  `CRT END 0 0 1`. L'host risponde `A<checksum a 3 cifre>`; senza ACK corretto il
  firmware ritrasmette (3 tentativi da 400 ms, watchdog alimentato) e poi prosegue.
  L'ACK va inviato **solo** per le righe `CRT`.
- `BUSY`: una volta al secondo mentre un comando e' in esecuzione. Se l'host non
  riceve nulla per alcuni secondi il collegamento e' da considerare perso.

## Avvio

```
Avvio firmware 3DP...
BOOT <POWERON|EXTERNAL|BROWNOUT|WDT|SOFT|UNKNOWN> *cs
Sistema pronto.
```

`Sistema pronto.` viene ripetuto ogni 5 s finche' non arriva un comando valido.
Se l'host vede il banner mentre attende una risposta, il firmware si e' riavviato:
posizione, stato e temperatura target sono persi.

## Comandi

Vedi `contracts/*.json`. In sintesi:

| Cmd | Descrizione |
|-----|-------------|
| `M10 X Y Z E F` | movimento relativo (mm, mm/min) |
| `M11 X Y Z E F` | movimento assoluto; assi omessi invariati; `E` sempre relativo |
| `M12 X Y Z` | dichiara la posizione corrente |
| `M114` | posizione reale in questo istante -> `CRT POS x y z <r>`; `r`=1 se riferita (homing o `M12` dall'accensione) |
| `M0` | emergency stop |
| `A10/A11/A12 E F` | estrusione / ritrazione / spurgo |
| `T10 S<gradi>` | target hotend (rifiutato con `NO_SENSOR` se il termistore e' disabilitato) |
| `T20` | temperatura -> `CRT TMP <attuale> <target>` |
| `S0 [X] [Y] [Z]` | homing degli assi indicati, tutti se omessi (solo da Idle) |
| `S1` / `S12` | inizio / fine stampa (Idle <-> Printing) |
| `S2` / `S3` | pausa / ripresa |
| `S6` / `S7` | azzera / leggi errore |
| `S10` | stato -> `STA <Idle|Homing|Printing|Paused|Error>` |
| `S11` | diagnostica -> `DIA <uptime s> <reset> <ultimo N> <err cs> <err overflow> <ACK persi> <RAM libera>` e `PRF <RAM minima mai rimasta libera> <durata max interrupt step, us> <scadenze step mancate>` |

I movimenti sono segmenti rettilinei; archi (G2/G3) e spline (G5) vengono spezzati
dal driver con errore di corda di 0,02 mm e raccordati dal planner del firmware.

## Tenere d'occhio RAM e CPU

- `tools/check_budget.sh [--symbols]`: compila per Uno e fallisce se flash o RAM
  globale superano i limiti (la RAM non usata dalle globali e' tutto lo stack disponibile).
- `S11` sulla scheda vera: la riga `PRF` riporta il picco reale di stack (RAM minima
  rimasta libera dall'avvio), la durata massima dell'interrupt degli step e quante
  volte ha sforato il proprio intervallo. Con `MAX_STEP_RATE` = 8000 l'intervallo
  minimo e' 125 us: se la durata massima si avvicina o le scadenze mancate non sono
  zero, la CPU e' satura e `MAX_STEP_RATE` va abbassato.
