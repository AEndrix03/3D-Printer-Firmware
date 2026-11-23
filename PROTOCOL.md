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
| `M10 X Y Z E F` | movimento relativo (mm, mm/min); rifiutato con `ERR LIMIT` se esce dal volume di lavoro |
| `M11 X Y Z E F` | movimento assoluto; assi omessi invariati; `E` sempre relativo |
| `M12 X Y Z` | dichiara la posizione corrente; gli assi indicati diventano riferiti |
| `M114` | posizione reale in questo istante -> `CRT POS x y z <r>`; `r`=1 se riferita (homing o `M12` dall'accensione) |
| `M0` | emergency stop: ferma gli step e toglie corrente ai motori |
| `A10/A11/A12 E F` | estrusione / ritrazione / spurgo |
| `T10 S<gradi>` | target hotend (rifiutato con `NO_SENSOR` se il termistore e' disabilitato) |
| `T20` | temperatura -> `CRT TMP <attuale> <target>` |
| `T40 [P..] [I..] [D..]` | guadagni del PID dell'hotend (quelli omessi restano) -> `PID <kp> <ki> <kd> <uscita 0-255>` |
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
  volte ha sforato il proprio intervallo. Con `MAX_STEP_RATE` = 10000 l'intervallo
  minimo e' 100 us: se la durata massima si avvicina o le scadenze mancate non sono
  zero, la CPU e' satura e `MAX_STEP_RATE` va abbassato.
- `tools/hw_bench.py` fallisce se una soglia viene superata (RAM libera al picco, durata
  dell'interrupt, scadenze mancate, errori sul collegamento): va rieseguito sulla scheda
  dopo ogni modifica al firmware.

### Misure del collegamento

`tools/linktest` contiene uno sketch senza logica di stampa e uno script che misurano il solo
giro del comando (scheda + adattatore USB + PC) al variare di formato, baud e numero di
comandi inviati senza attendere la risposta ("finestra"). Comandi al secondo misurati:

| Comando | 115200, finestra 1 | 115200, finestra 3-4 | 250000, finestra 3 |
|---------|--------------------|----------------------|--------------------|
| oggi: testo con decimali, 38 byte | 111 | (non entra nel buffer da 64) | - |
| pianificato dal PC, testo, 50 byte | 100 | (non entra) | - |
| pianificato dal PC, testo compatto, 33 byte | 125 | 284 | 501 |
| pianificato dal PC, binario, 20 byte | 148 | 497 | 995 |

Cosa dicono:

- attendere ogni risposta costa 4-6 ms fissi di USB, qualunque sia il baud: e' la finestra
  la leva principale, non la velocita' della linea;
- con la finestra il limite diventa il numero di byte del comando, quindi il formato binario
  vale quasi il doppio del testo;
- accorciare la risposta non aiuta (a volte peggiora, per come l'adattatore USB raggruppa i byte);
- oltre 250000 baud non si guadagna nulla: il limite e' il millisecondo del frame USB.

Il firmware reale oggi fa ~79 movimenti al secondo perche' a questi tempi aggiunge ~4,7 ms di
pianificazione per segmento.

- **Guardia sullo stack.** Se lo stack arriva a meno di 128 byte dalle variabili il firmware
  spegne l'heater, ferma i motori e invia `ALM STACK`: sull'ATmega non c'e' protezione di
  memoria, e oltre quel punto le variabili verrebbero sovrascritte in silenzio. Non e' un
  errore da azzerare e proseguire: indica che il firmware va alleggerito.

### Regolazione della temperatura

L'heater e' regolato da un PID (`Config.hpp`: `HEATER_KP`, `HEATER_KI`, `HEATER_KD`), calcolato
4 volte al secondo. L'uscita (0-255) diventa tempo di accensione dentro una finestra di 1 s,
aggiornata a ogni giro del ciclo principale; oltre 10 gradi dal target l'heater e' tutto acceso
o tutto spento. Le protezioni (sensore guasto, temperatura massima, riscaldamento che non sale,
calo a regime, host sparito) restano quelle di prima e hanno la precedenza.

I guadagni di default sono valori tipici per un hotend da 40 W e **vanno tarati sull'hardware
reale**: `T40 P.. I.. D..` (o `M301` dal driver) li cambia senza riprogrammare. Sul modello
termico del simulatore il regolatore arriva a 200 gradi senza superare 205 e resta entro
+-1 grado a regime (scenario e2e "PID temperature regulation").

### Una regola sugli ingressi e le uscite

Nei percorsi eseguiti a ogni giro del ciclo principale (heater, finecorsa) si usa la scrittura
diretta sui registri (`hal::fastWrite` / `fastRead`), mai `digitalWrite()` di Arduino. Sulla
scheda reale `digitalWrite()` chiamata a quella cadenza faceva arrivare al PC righe con
caratteri mancanti: decine per prova, contro zero con la scrittura diretta e zero con un
ritardo puro della stessa durata. Il meccanismo non e' stato chiarito; `tools/hw_bench.py`
verifica il checksum di ogni riga e fallisce se ne arriva rovinata piu' di una.

### Misure sulla scheda reale

`tools/hw_bench.py <porta>` esegue sulla scheda movimenti veloci, a quattro assi e flussi di
segmenti corti, e stampa `DIA`/`PRF` dopo ogni fase. Valori misurati su un ATmega328P a
16 MHz (clone con CH340, 115200 baud), scheda nuda:

| Grandezza | Valore |
|-----------|--------|
| RAM minima rimasta libera (picco di stack) | 848 byte (606 all'inizio) |
| Durata massima dell'interrupt degli step | 67 us (124 prima della tabella degli intervalli) |
| Scadenze degli step mancate | 0 |
| Errori di checksum / ritrasmissioni su ~2000 comandi | 0 |
| Giro completo di un comando senza lavoro (`S10`) | 7,9 ms |
| Giro completo di un movimento (`M10` con E) | 12,6 ms, cioe' ~79 segmenti/s |
| Movimenti binari, 4 in volo | 173-218 segmenti/s |
| Arresto con `!` da 50 mm/s | 0,07 s |

Il collo di bottiglia per le curve fitte e' il giro del comando (stop-and-wait su USB), non
il generatore di step: a 79 segmenti/s, segmenti da 0,2 mm passano a ~16 mm/s.

## Movimenti in formato binario

I movimenti di una stampa non viaggiano come testo: il driver li invia come frame binari,
in step gia' calcolati (nessuna virgola mobile da convertire sul firmware), e ne tiene fino a
quattro in volo senza attendere ogni risposta. I comandi di testo (`M10`, `M11`, ...) restano
validi e sono sempre uno alla volta.

Ogni byte del frame ha il bit 7 a 1 e porta 6 bit utili (un "sestetto"): l'intestazione e'
`11xxxxxx`, gli altri byte `10xxxxxx`. Un byte di movimento non puo' quindi essere scambiato
per testo, per un fine riga o per `!`, e l'intestazione si riconosce da sola: dopo un errore
la ricezione si riallinea al frame successivo, senza attese.

| Campo | Sestetti | Contenuto |
|-------|----------|-----------|
| intestazione | 1 | bit5 velocita' presente, bit4 campi larghi, bit3..0 assi presenti (X Y Z E) |
| sequenza | 1 | numero del comando modulo 64 |
| un campo per asse presente | 2 (3 se larghi) | step relativi con segno: +-2047 (+-131071 se larghi) |
| velocita' | 3, se presente | mm/min interi; assente = resta l'ultima |
| controllo | 2 | 12 bit bassi del CRC-16/CCITT dei byte precedenti |

Un segmento corto su X, Y ed E occupa 10 byte (38 caratteri in testo). La specifica in codice e'
`src/include/MoveFrame.hpp`.

Regole:

- **Risposta**: come per il testo, una riga per frame: `OK0 N<n> D<d>` con il numero completo.
- **Frame corrotto o incompleto**: `E01 N<atteso>`; i frame gia' in volo dietro di lui ricevono
  `E04 N<atteso>`. Il driver rispedisce da `<atteso>`.
- **Frame gia' ricevuto** (risposta andata persa, frame rispedito): `E03`, non viene rieseguito.
- **Rifiuto** (`ERR LIMIT`, `COLD_EXT`, `INV_STATE`) **o arresto `!`**: i frame successivi vengono
  scartati con `ERR SKIPPED` finche' non arriva un comando di testo. I movimenti in volo non
  partono mai da una posizione diversa da quella prevista dal driver.
- **`!`** viene riconosciuto nell'interrupt di ricezione, quindi e' immediato anche con altri
  comandi in attesa nel buffer.
- **`M115`** restituisce `SPM <step/mm X Y Z E>` e `CRT STP <x> <y> <z>`: il driver tiene una
  copia della posizione in step e la rilegge ogni volta che puo' essere cambiata senza che lui
  lo sappia (arresto, allarme, homing, `M12`, rifiuto, riavvio).
- Il firmware elabora **un comando per giro** del ciclo principale: tra un comando e il
  successivo watchdog, sicurezza e temperatura vengono sempre aggiornati, anche con il buffer
  di ricezione sempre pieno.

Misurato sulla scheda reale (`tools/hw_bench.py`): da ~79 a 173-218 segmenti al secondo, senza
errori ne' ritrasmissioni. Il limite e' ora il tempo di pianificazione del firmware per segmento.

## Limiti di corsa e alimentazione dei motori

- **Limiti software.** Un movimento (`M10`, `M11`) che porterebbe un asse riferito fuori da
  `[0, TRAVEL_MAX]` (`MotionConfig.hpp`, 220 x 220 x 200 mm) viene rifiutato per intero con
  `ERR LIMIT` e nessuno step viene eseguito. Lo zero e' il finecorsa. Un asse diventa riferito
  con l'homing (`S0`) o con `M12`; finche' non lo e' i limiti non si applicano. Un asse che si
  trova gia' fuori dal volume puo' sempre rientrare.
- **Pin di enable.** `PIN_STEPPERS_ENABLE` (D8, comune a tutti i driver, attivo basso) toglie
  corrente ai motori in emergenza (`M0`, allarmi `ALM`) e dopo 30 minuti di inattivita' da
  fermi; in pausa restano alimentati. Il primo movimento li riaccende. Senza corrente gli assi
  possono essere spostati a mano, quindi la posizione perde il riferimento (`M114` riporta
  `r`=0) e serve un nuovo homing.
