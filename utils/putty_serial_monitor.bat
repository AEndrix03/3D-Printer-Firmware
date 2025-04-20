@echo off
REM === CONFIGURA QUI ===
set "PUTTY_PATH=C:\"Program Files"\PuTTY\putty.exe"
set "COM_PORT=COM5"
set "BAUD_RATE=19200"

REM === AVVIO ===
%PUTTY_PATH% -serial %COM_PORT% -sercfg %BAUD_RATE%,8,n,1,N
