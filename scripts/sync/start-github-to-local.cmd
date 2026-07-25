@echo off
REM Start TheSeed Auto-Sync: GitHub -> Local
REM Pulls changes from GitHub to local every 5 minutes.
REM Close this window or press Ctrl+C to stop.

cd /d "%~dp0"
echo Starting GitHub-to-Local Auto-Sync...
echo Mode: Pull from GitHub, push local commits if any.
echo Close this window or press Ctrl+C to stop.
echo.
powershell.exe -ExecutionPolicy Bypass -NoExit -File "auto-sync-github-to-local.ps1" -IntervalMinutes 5
