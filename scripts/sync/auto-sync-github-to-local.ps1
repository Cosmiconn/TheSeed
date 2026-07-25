#!/usr/bin/env pwsh
# TheSeed Auto-Sync Daemon: GitHub -> Local
# Pulls changes from GitHub to local every 5 minutes.
# Also pushes local commits if they exist.
# Usage: Right-click -> "Run with PowerShell" or double-click start-github-to-local.cmd

param(
    [int]$IntervalMinutes = 5,
    [string]$LogFile = "auto-sync.log"
)

$ErrorActionPreference = "Continue"
$root = Split-Path $PSScriptRoot -Parent
$logPath = Join-Path $root $LogFile
$lockFile = Join-Path $root ".auto-sync.lock"

# Prevent multiple instances
if (Test-Path $lockFile) {
    $pidOld = Get-Content $lockFile -ErrorAction SilentlyContinue
    if ($pidOld -and (Get-Process -Id $pidOld -ErrorAction SilentlyContinue)) {
        Write-Host "Auto-sync already running (PID: $pidOld)" -ForegroundColor Red
        Write-Host "Use stop-auto-sync.cmd to stop it first." -ForegroundColor Yellow
        pause
        exit 1
    }
}

$PID | Set-Content $lockFile
Write-Host "Auto-sync started (PID: $PID). Press Ctrl+C or close window to stop." -ForegroundColor Green
Write-Host "Mode: GitHub -> Local (pull priority)" -ForegroundColor Cyan
Write-Host "Logging to: $logPath" -ForegroundColor Gray
Write-Host ""

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $line = "[$timestamp] [$Level] $Message"
    Add-Content -Path $logPath -Value $line
    switch ($Level) {
        "ERROR" { Write-Host $line -ForegroundColor Red }
        "WARN"  { Write-Host $line -ForegroundColor Yellow }
        "OK"    { Write-Host $line -ForegroundColor Green }
        "SKIP"  { Write-Host $line -ForegroundColor DarkGray }
        default { Write-Host $line }
    }
}

Set-Location $root

while ($true) {
    try {
        Write-Log "========== SYNC CYCLE START =========="

        # 1. Pull main repo (GitHub -> Local)
        Write-Log "Pulling TheSeed from GitHub..."
        $pullMain = git pull origin main 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "TheSeed pull failed: $pullMain" "ERROR"
        } else {
            if ($pullMain -match "Already up to date") {
                Write-Log "TheSeed: Already up to date" "OK"
            } else {
                Write-Log "TheSeed: Updated" "OK"
            }
        }

        # 2. Update submodules (GitHub -> Local)
        Write-Log "Updating submodules..."
        git submodule update --init --recursive 2>$null | Out-Null
        $subUpdate = git submodule update --remote --merge 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Submodule update failed: $subUpdate" "ERROR"
        } else {
            if ($subUpdate -match "Already up to date") {
                Write-Log "Submodules: Already up to date" "OK"
            } else {
                Write-Log "Submodules: Updated" "OK"
            }
        }

        # 3. Check if submodule pointer changed
        $status = git status --short 2>$null
        if ($status -match "submodules/seed-core") {
            Write-Log "Submodule pointer changed, committing..."
            git add submodules/seed-core 2>$null | Out-Null
            git commit -m "chore(submodule): Auto-sync seed-core" 2>$null | Out-Null
            Write-Log "Pointer committed" "OK"
        }

        # 4. Push local commits (Local -> GitHub)
        $localCommits = git log origin/main..main --oneline 2>$null
        if ($localCommits) {
            Write-Log "Pushing local commits to GitHub..."
            git push origin main 2>&1 | Out-Null
            Write-Log "Push complete" "OK"
        } else {
            Write-Log "Nothing to push" "SKIP"
        }

        Write-Log "========== SYNC CYCLE COMPLETE =========="
    }
    catch {
        Write-Log "Unexpected error: $_" "ERROR"
    }

    Write-Log "Sleeping for $IntervalMinutes minutes..."
    Start-Sleep -Seconds ($IntervalMinutes * 60)
}
