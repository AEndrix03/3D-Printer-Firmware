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
- La riga `!` (o il carattere `!` durante un movimento) interrompe il movimento
  in corso: il comando interrotto risponde `E06` con `ERR ABORTED`.

## Risposta terminale (firmware -> host)

**Esattamente una per comando**, sempre con il numero del comando:

| Codice | Significato |
|--------|-------------|
| `OK0 N<n>` | eseguito |
| `E01 N<atteso>` | checksum errato o riga non valida: ritrasmettere |
| `E02 N<atteso>` | riga troppo lunga, scartata |
| `E03 N<n>` | duplicato: il comando `n` era gia' stato eseguito (risposta persa) |
| `E04 N<atteso>` | numero fuori sequenza: il firmware si aspetta `atteso` |
| `E05 N<n>` | categoria sconosciuta |
| `E06 N<n>` | comando rifiutato; il motivo e' nella riga `ERR <motivo>` che precede |
| `EM0 N<n>` / `ET0 N<n>` | macchina in stato di errore: movimento / riscaldamento bloccati |
| `ES0 N<n>` / `ES1 N<n>` | `S6`: errore azzerato / nessun errore attivo |

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
| `M114` | posizione -> `CRT POS x y z` |
| `M0` | emergency stop |
| `A10/A11/A12 E F` | estrusione / ritrazione / spurgo |
| `T10 S<gradi>` | target hotend (rifiutato con `NO_SENSOR` se il termistore e' disabilitato) |
| `T20` | temperatura -> `CRT TMP <attuale> <target>` |
| `S0` | homing (solo da Idle) |
| `S1` / `S12` | inizio / fine stampa (Idle <-> Printing) |
| `S2` / `S3` | pausa / ripresa |
| `S6` / `S7` | azzera / leggi errore |
| `S10` | stato -> `STA <Idle|Homing|Printing|Paused|Error>` |
| `S11` | diagnostica -> `DIA <uptime s> <reset> <ultimo N> <err cs> <err overflow> <ACK persi> <RAM libera>` |

I movimenti sono segmenti rettilinei con rampa di accelerazione; archi (G2/G3) e
spline (G5) vengono spezzati dal driver con errore di corda di 0,02 mm.
