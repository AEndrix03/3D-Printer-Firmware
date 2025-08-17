@echo off
setlocal

rem === Configurazione ===
set ARDUINO_CLI=arduino-cli
set PORT=COM4
set BAUD=115200

echo [INFO] Avvio monitor seriale su %PORT% a %BAUD% baud...
%ARDUINO_CLI% monitor -p %PORT% -c baudrate=%BAUD%

endlocal
