@echo off
setlocal

rem === Configurazione ===
set ARDUINO_CLI=arduino-cli
set PORT=COM3
set BAUD=19200

echo [INFO] Avvio monitor seriale su %PORT% a %BAUD% baud...
%ARDUINO_CLI% monitor -p %PORT% -c baudrate=%BAUD%

endlocal
