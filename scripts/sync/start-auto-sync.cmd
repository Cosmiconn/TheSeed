@echo off
REM Start TheSeed Auto-Sync Daemon
REM Opens a visible PowerShell window that stays open.
REM Close the window or press Ctrl+C to stop.

cd /d "%~dp0"
echo Starting TheSeed Auto-Sync Daemon...
echo Close this window or press Ctrl+C to stop.
echo.
powershell.exe -ExecutionPolicy Bypass -NoExit -File "auto-sync.ps1" -IntervalMinutes 5
