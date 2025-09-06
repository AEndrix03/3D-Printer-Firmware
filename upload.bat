@echo off
setlocal enabledelayedexpansion

rem === Configurazione ===
set "ARDUINO_CLI=arduino-cli"
set "BOARD=arduino:avr:uno"
set "PORT=COM3"
set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

rem === Definizioni in base alla board ===
set "EXTRA_FLAGS="

if "%BOARD%"=="arduino:avr:uno" (
    set "EXTRA_FLAGS=-DPLATFORM_ARDUINO"
)

if "%BOARD%"=="esp32:esp32:esp32" (
    set "EXTRA_FLAGS=-DPLATFORM_ESP32"
)

echo [INFO] Compilazione del progetto per %BOARD%...
%ARDUINO_CLI% compile ^
    --fqbn %BOARD% "%PROJECT_DIR%" ^
    --build-property "compiler.cpp.extra_flags=%EXTRA_FLAGS%" ^
    --output-dir "%PROJECT_DIR%\build"

if %errorlevel% neq 0 (
    echo [ERRORE] Compilazione fallita!
    exit /b %errorlevel%
)

echo [INFO] Forzo chiusura di tutti i processi che usano la porta seriale...
for %%P in (java.exe arduino-cli.exe 3DP_Printer_Firmware.exe putty.exe arduino.exe) do (
    taskkill /IM %%P /F >nul 2>&1
)

echo [INFO] Attendo 3 secondi che la porta %PORT% si liberi...
timeout /t 3 >nul

echo [INFO] Upload sulla porta %PORT%...
%ARDUINO_CLI% upload -p %PORT% --fqbn %BOARD% "%PROJECT_DIR%"

if %errorlevel% neq 0 (
    echo [ERRORE] Upload fallito!
    echo [DEBUG] Verifica che:
    echo  - Il cavo USB sia collegato
    echo  - La board %BOARD% sia corretta
    echo  - Nessun processo stia usando %PORT%
    exit /b %errorlevel%
)

echo [SUCCESSO] Upload completato!
echo [INFO] Puoi riavviare il driver ora.

pause

endlocal
