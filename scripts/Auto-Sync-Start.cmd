@echo off
echo Starting TheSeed Auto-Sync Daemon...
echo This window will stay open. Minimize it and let it run.
echo.
powershell -ExecutionPolicy Bypass -File "%~dp0auto-sync.ps1" -IntervalMinutes 5
pause