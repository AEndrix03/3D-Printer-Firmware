@echo off
setlocal enabledelayedexpansion

:: CONFIGURAZIONE PORTA
set "PORT=COM3"
set "BAUD=19200"

:: File temporaneo per lo script PowerShell
set "TMPFILE=%TEMP%\send_serial_cmd.ps1"

:loop
:: Richiedi input utente
set /p CMD=Inserisci comando (es. N1 M10 X10 Y10 F1500), vuoto per uscire:
if "%CMD%"=="" goto end

:: Scrivi lo script PowerShell
echo $cmd = "%CMD%" > "%TMPFILE%"
echo $cs = 0 >> "%TMPFILE%"
echo foreach ($c in $cmd.ToCharArray()) { $cs = $cs -bxor [byte][char]$c } >> "%TMPFILE%"
echo $full = "$cmd *$cs" >> "%TMPFILE%"
echo $port = New-Object System.IO.Ports.SerialPort "%PORT%", %BAUD% >> "%TMPFILE%"
echo $port.ReadTimeout = 1000 >> "%TMPFILE%"
echo $port.Open() >> "%TMPFILE%"
echo $port.WriteLine($full) >> "%TMPFILE%"
echo Start-Sleep -Milliseconds 200 >> "%TMPFILE%"
echo try { >> "%TMPFILE%"
echo     $resp = $port.ReadLine() >> "%TMPFILE%"
echo     Write-Host "Arduino dice: $resp" >> "%TMPFILE%"
echo } catch { >> "%TMPFILE%"
echo     Write-Host "Nessuna risposta." >> "%TMPFILE%"
echo } >> "%TMPFILE%"
echo $port.Close() >> "%TMPFILE%"
echo Write-Host "Inviato: $full" >> "%TMPFILE%"

:: Esegui lo script PowerShell
%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -File "%TMPFILE%"

goto loop

:end
:: Pulizia
del "%TMPFILE%" >nul
endlocal
