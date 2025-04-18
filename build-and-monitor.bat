@echo off
setlocal

set ARDUINO_CLI=arduino-cli
set BOARD=arduino:avr:uno
set PORT=COM3
set PROJECT_DIR=%~dp0
set PROJECT_DIR=%PROJECT_DIR:~0,-1%
set BAUD=19200

echo [INFO] Compilazione...
%ARDUINO_CLI% compile --fqbn %BOARD% "%PROJECT_DIR%" || goto :error

echo [INFO] Upload...
%ARDUINO_CLI% upload -p %PORT% --fqbn %BOARD% "%PROJECT_DIR%" || goto :error

echo [INFO] Avvio monitor seriale...
%ARDUINO_CLI% monitor -p %PORT% -c baudrate=%BAUD%

goto :eof

:error
echo [ERRORE] Qualcosa è andato storto.
exit /b 1
