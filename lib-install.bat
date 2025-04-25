@echo off
setlocal

set ARDUINO_CLI=arduino-cli

if "%~1"=="" (
    echo Uso: lib-install.bat NomeLibreria
    exit /b
)

%ARDUINO_CLI% lib install %1

endlocal
