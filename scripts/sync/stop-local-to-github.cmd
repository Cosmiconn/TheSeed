@echo off
REM Stop TheSeed Auto-Sync: Local -> GitHub

cd /d "%~dp0"
set "LOCKFILE=..\.auto-sync-local.lock"

if not exist "%LOCKFILE%" (
    echo No Local->GitHub sync lock file found.
    echo Process may not be running, or the window was already closed.
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

echo Stopping Local->GitHub sync (PID: %PID%)...
taskkill /PID %PID% /F >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Sync stopped.
) else (
    echo Process not found. Removing stale lock.
)
del "%LOCKFILE%"
pause
