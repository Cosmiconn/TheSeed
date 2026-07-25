@echo off
REM Stop TheSeed Auto-Sync Daemon

cd /d "%~dp0"
set "LOCKFILE=..\.auto-sync.lock"

if not exist "%LOCKFILE%" (
    echo No auto-sync lock file found.
    echo Auto-sync may not be running, or the window was already closed.
    pause
    exit /b 1
)

set /p PID=<%LOCKFILE%
if "%PID%"=="" (
    echo Lock file empty. Removing stale lock.
    del "%LOCKFILE%"
    pause
    exit /b 1
)

echo Stopping auto-sync daemon (PID: %PID%)...
taskkill /PID %PID% /F >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Auto-sync stopped.
) else (
    echo Process not found. Removing stale lock.
)
del "%LOCKFILE%"
pause
