@echo off
REM Start TheSeed Auto-Sync: Local -> GitHub
REM Pushes local changes to GitHub every 5 minutes.
REM Close this window or press Ctrl+C to stop.

cd /d "%~dp0"
echo Starting Local-to-GitHub Auto-Sync...
echo Mode: Commit + push local, then pull from GitHub.
echo Close this window or press Ctrl+C to stop.
echo.
powershell.exe -ExecutionPolicy Bypass -NoExit -File "auto-sync-local-to-github.ps1" -IntervalMinutes 5
