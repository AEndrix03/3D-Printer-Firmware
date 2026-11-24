# 3D Printer Firmware

Un firmware moderno e modulare per stampanti 3D basato su Arduino UNO, progettato con un'architettura client-server per
gestire in modo efficiente la comunicazione, il movimento e il controllo della temperatura.

## 📋 Panoramica del Progetto

Questo progetto implementa un firmware completo per stampanti 3D con un'architettura divisa in due parti principali:

- **Host** (PC / Raspberry Pi / VPS): gestisce l'interpretazione del G-code, la pianificazione delle traiettorie e
  l'invio dei comandi
- **Arduino UNO**: esegue il firmware che controlla direttamente l'hardware della stampante 3D

L'architettura permette di spostare la complessità computazionale sull'host, lasciando ad Arduino solo l'esecuzione dei
comandi ottimizzati, risultando in movimenti più fluidi e precisi rispetto ai firmware tradizionali.

## 🏗️ Architettura del Sistema

### Architettura Host-Firmware

```
📦 Host (PC / Raspberry / VPS)
│
├── 🧩 GCodeParser
│     └── Riconosce comandi G0, G1, G2, G3, Mxxx
│
├── 🌀 ArcInterpolator
│     └── G2/G3 → G1 multipli (X,Y segmentati)
│
├── 🧠 TrajectoryPlanner
│     └── Pre-calcolo velocità, accelerazioni, buffer
│
├── 📡 SerialCommandSender
│     ├── N#, Checksum, *CS, Ack/Nack
│     └── Flow control, resend, timeouts
│
└── 🗃️ CommandQueue (opzionale)
     └── FIFO comandi da inviare

        │
        ▼
═════════════════════════════════════════════
🔌 SERIAL (TTL/USB 115200 Baud)
═════════════════════════════════════════════
│
▼

📟 Arduino UNO – 3DP Firmware
│
├── 🔄 SerialCommandReceiver
│     ├── LineReader (char buffer, \n)
│     ├── Parser: N, Categoria+Codice, Parametri, *CS
│     └── Validator: checksum, duplicati, sintassi
│
├── 🎯 CommandDispatcher
│     └── Smista a:
│           ├── MotionController (M)
│           ├── TempController   (T)
│           ├── FanController    (F)
│           ├── SystemController (S)
│           └── EndstopController(E)
│
├── 🧾 CommandQueue (opzionale)
│     └── Buffer FIFO (4 comandi) con priorità emergenza. Il numero di comandi è configurabile.
│
├── 📊 StateMachine
│     ├── Idle / Printing / Error / Homing / Paused
│     └── Reagisce a eventi da parser o moduli
│
├── 🧯 SafetyManager
│     ├── Timeout comandi
│     ├── Temperatura fuori range
│     ├── Finecorsa attivi fuori tempo
│     └── Comando STOP immediato (!!!)
│
├── 🔌 WatchdogHandler
│     └── Reset se firmware bloccato > 2s
│
├── 🧩 MotionController
│     ├── moveTo(x,y,z,f)
│     └── usa Planner + Stepper (step in interrupt), limiti di corsa
│
├── 🌡 TempController
│     ├── PID base
│     ├── HeaterDriver
│     └── ThermistorReader
│
├── 💨 FanController
│     └── PWM su pin ventole
│
├── 🧱 SystemController
│     ├── GET STATUS
│     └── M500/M501 per EEPROM config (futuro)
│
├── 📏 EndstopController
│     └── Z_Homing, probing, finecorsa XYZ
│
└── 🧱 Hardware Abstraction Layer (HAL)
     ├── McuHAL.hpp
     ├── ArduinoHAL.cpp
     └── Interfaccia unificata:
          digitalRead(), writePin(), delayUs(), readAnalog()
```

## ✨ Caratteristiche Principali

- **Architettura Client-Server**: Divide il carico computazionale tra host e Arduino
- **Interpolazione Archi**: Converte archi complessi (G2/G3) in movimenti lineari ottimizzati
- **Pianificazione Traiettoria**: Calcola in anticipo velocità e accelerazioni per movimenti fluidi
- **Gestione della Sicurezza**: Sistema di watchdog e controlli di sicurezza integrati
- **Comunicazione Robusta**: Protocollo seriale con checksum e ritrasmissione automatica
- **Design Modulare**: Componenti separati e facilmente estendibili
- **Hardware Abstraction Layer**: Semplifica la portabilità su altre piattaforme hardware

## 🚀 Come Iniziare

### Prerequisiti

- Arduino UNO o compatibile
- Computer host (PC, Raspberry Pi o altro)
- Hardware stampante 3D (motori stepper, driver, endstop, etc.)
- Arduino IDE (versione 1.8.x o superiore)
  <<<<<<< HEAD
  =======
- [Librerie necessarie](#librerie-richieste)

> > > > > > > origin/master

### Installazione Firmware

1. Clona il repository:
   ```bash
   git clone https://github.com/yourusername/3D-Printer-Firmware.git
   ```

2. Apri il file `Firmware/3DPrinterFirmware.ino` con Arduino IDE

3. Configura i parametri della tua stampante nel file `Configuration.h`

4. Compila e carica il firmware sulla tua scheda Arduino UNO
   <<<<<<< HEAD
   =======

### Configurazione Host

1. Naviga nella cartella `HostSoftware`:
   ```bash
   cd HostSoftware
   ```

2. Installa le dipendenze:
   ```bash
   pip install -r requirements.txt
   ```

3. Configura la porta seriale nel file `config.ini`

4. Esegui il software host:
   ```bash
   python host_controller.py

> > > > > > > origin/master

   ```

## 📚 Documentazione

La documentazione dettagliata è disponibile
nella [wiki del progetto](https://github.com/yourusername/3D-Printer-Firmware/wiki).

<<<<<<< HEAD
=======
### Librerie Richieste

- AccelStepper (per il controllo dei motori)
- PID Library (per il controllo della temperatura)
- [Altre librerie specifiche]

>>>>>>> origin/master
## 📝 Protocollo di Comunicazione

Il firmware utilizza un protocollo seriale personalizzato con le seguenti caratteristiche:

- Comunicazione a 115200 baud
- Numerazione dei comandi (N#)
- Checksum (*CS) per verificare l'integrità
- Sistema di acknowledgment (Ack/Nack)
- Ritrasmissione automatica in caso di errori

Esempio di comando:

```

N123 S10 *12

```

## 🔧 Personalizzazione

Il firmware può essere facilmente personalizzato per adattarsi a diverse configurazioni hardware:

1. Modifica i file nella cartella `Configuration/` per impostare:
    - Dimensioni del piano di stampa
    - Tipi di motori e driver
    - Configurazione degli endstop
    - Parametri di temperatura
    - Velocità e accelerazioni

2. Il sistema HAL (Hardware Abstraction Layer) consente di portare il firmware su diverse piattaforme Arduino o
   microcontrollori compatibili.

## 🤝 Contribuire

I contributi sono benvenuti! Per contribuire:

1. Fai un fork del repository
2. Crea un branch per la tua funzionalità (`git checkout -b feature/AmazingFeature`)
3. Commit delle tue modifiche (`git commit -m 'Add some AmazingFeature'`)
4. Push al branch (`git push origin feature/AmazingFeature`)
5. Apri una Pull Request

Per favore, consulta il file [CONTRIBUTING.md](CONTRIBUTING.md) per maggiori dettagli.

## 📅 Roadmap

- [ ] Supporto per display LCD
- [ ] Controllo remoto tramite WiFi/Bluetooth
- [ ] Interfaccia web per il monitoraggio
- [ ] Supporto multi-estrusore
- [ ] Autocalibrazione del piano

## 👥 Team

- Andrea Redegalli - *Sviluppatore principale* - [GitHub](https://github.com/AEndrix03)

---

Fatto con ❤️ per la comunità della stampa 3D

## Protocollo e test

- Il protocollo seriale completo (sequenza, checksum, ACK, resync, diagnostica) e' descritto in [PROTOCOL.md](PROTOCOL.md).
- `test/native/` contiene una scheda simulata (`SimBoard`) che permette di compilare ed eseguire
  il firmware su PC con `-DPLATFORM_NATIVE`. I test di integrazione con il driver stanno nel repo
  `3DP-Driver-Core` (`tests/integration/run.sh`) e non richiedono hardware.
- Prima di abilitare `TEMP_MONITORING_ENABLED` verificare in `src/include/Config.hpp` le costanti
  del termistore (`THERMISTOR_BETA`, `THERMISTOR_R25`, `THERMISTOR_PULLUP`) con l'hardware reale.
