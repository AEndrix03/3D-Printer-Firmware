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
│     └── Buffer FIFO (3–5 comandi) con priorità emergenza
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
│     └── usa StepperDriver, limit check
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

