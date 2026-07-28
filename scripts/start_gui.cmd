@echo off
title TheSeed Sync GUI
echo Starting TheSeed Sync GUI...
echo.
cd /d "%~dp0"
python scripts\theseed_sync_gui.py
if errorlevel 1 (
    echo.
    echo Failed to start. Make sure Python is installed.
    pause
)