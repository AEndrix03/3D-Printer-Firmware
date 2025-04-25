@echo off
setlocal

set ARDUINO_CLI=arduino-cli
set BOARD=arduino:avr:uno
set PROJECT_DIR=%~dp0
set PROJECT_DIR=%PROJECT_DIR:~0,-1%

echo [INFO] Verifica del codice...
%ARDUINO_CLI% compile --fqbn %BOARD% --verify "%PROJECT_DIR%"

endlocal
