@echo off
setlocal

rem === Configurazione ===
set "ARDUINO_CLI=arduino-cli"
set "BOARD=arduino:avr:uno"
set "PORT=COM3"
set "PROJECT_DIR=%~dp0"
set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

echo [INFO] Compilazione del progetto...
%ARDUINO_CLI% compile --fqbn %BOARD% "%PROJECT_DIR%"

if %errorlevel% neq 0 (
    echo [ERRORE] Compilazione fallita!
    exit /b %errorlevel%
)

echo [INFO] Forzo chiusura seriale (se attiva)...
taskkill /IM java.exe /F >nul 2>&1
taskkill /IM arduino-cli.exe /F >nul 2>&1


echo [INFO] Upload sulla porta %PORT%...
%ARDUINO_CLI% upload -p %PORT% --fqbn %BOARD% "%PROJECT_DIR%"

if %errorlevel% neq 0 (
    echo [ERRORE] Upload fallito!
    exit /b %errorlevel%
)

echo [SUCCESSO] Upload completato!

endlocal
