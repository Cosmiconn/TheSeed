@echo off
echo Stopping TheSeed Auto-Sync Daemon...

set "LOCKFILE=%~dp0..\.auto-sync.lock"

if exist "%LOCKFILE%" (
    set /p PID=<"%LOCKFILE%"
    taskkill /PID %PID% /F >nul 2>&1
    del "%LOCKFILE%" >nul 2>&1
    echo Auto-sync stopped (PID: %PID%)
) else (
    echo No running auto-sync found.
)

pause