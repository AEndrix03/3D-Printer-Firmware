@echo off
setlocal

rem === Configurazione ===
set "ARDUINO_CLI=arduino-cli"
set "BOARD=arduino:avr:uno"
set "PORT=COM5"
set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

echo [INFO] Compilazione del progetto...
%ARDUINO_CLI% compile --fqbn %BOARD% "%PROJECT_DIR%"

if %errorlevel% neq 0 (
    echo [ERRORE] Compilazione fallita!
    exit /b %errorlevel%
)

echo [INFO] Forzo chiusura di tutti i processi che usano la porta seriale...
taskkill /IM java.exe /F >nul 2>&1
taskkill /IM arduino-cli.exe /F >nul 2>&1
taskkill /IM 3DP_Printer_Firmware.exe /F >nul 2>&1
taskkill /IM putty.exe /F >nul 2>&1
taskkill /IM arduino.exe /F >nul 2>&1

echo [INFO] Controllo processi attivi su COM4...
for /f "tokens=2" %%i in ('tasklist /fo table ^| findstr /i "3DP_Printer_Firmware"') do (
    echo Terminando processo %%i
    taskkill /PID %%i /F >nul 2>&1
)

echo [INFO] Attendo liberazione porta...
ping localhost -n 3 >nul

echo [INFO] Upload sulla porta %PORT%...
%ARDUINO_CLI% upload -p %PORT% --fqbn %BOARD% "%PROJECT_DIR%"

if %errorlevel% neq 0 (
    echo [ERRORE] Upload fallito!
    echo [DEBUG] Verifica che:
    echo  - Il cavo USB sia collegato
    echo  - Arduino sia sulla porta %PORT%
    echo  - Nessun processo stia usando %PORT%
    exit /b %errorlevel%
)

echo [SUCCESSO] Upload completato!
echo [INFO] Puoi riavviare il driver ora.

endlocal