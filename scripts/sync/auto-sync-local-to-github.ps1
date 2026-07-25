#!/usr/bin/env pwsh
# TheSeed Auto-Sync Daemon: Local -> GitHub
# Pushes local changes to GitHub every 5 minutes.
# Also pulls from GitHub afterwards to stay in sync.
# Usage: Right-click -> "Run with PowerShell" or double-click start-local-to-github.cmd

param(
    [int]$IntervalMinutes = 5,
    [string]$LogFile = "auto-sync.log"
)

$ErrorActionPreference = "Continue"
# Go TWO levels up: scripts/sync/ -> scripts/ -> TheSeed/
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..") | Select-Object -ExpandProperty Path
$logPath = Join-Path $root $LogFile
$lockFile = Join-Path $root ".auto-sync-local.lock"

# Prevent multiple instances
if (Test-Path $lockFile) {
    $pidOld = Get-Content $lockFile -ErrorAction SilentlyContinue
    if ($pidOld -and (Get-Process -Id $pidOld -ErrorAction SilentlyContinue)) {
        Write-Host "Local-to-GitHub sync already running (PID: $pidOld)" -ForegroundColor Red
        Write-Host "Use stop-local-to-github.cmd to stop it first." -ForegroundColor Yellow
        pause
        exit 1
    }
}

$PID | Set-Content $lockFile
Write-Host "Local-to-GitHub sync started (PID: $PID). Press Ctrl+C or close window to stop." -ForegroundColor Green
Write-Host "Root: $root" -ForegroundColor Gray
Write-Host "Mode: Local -> GitHub (push priority)" -ForegroundColor Cyan
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
        Write-Log "========== LOCAL->GITHUB SYNC START =========="

        # 1. Commit local uncommitted changes (Local)
        $status = git status --porcelain 2>$null
        if ($status) {
            Write-Log "Local changes detected, committing..." "WARN"
            git add -A 2>$null | Out-Null
            $msg = "auto: local sync $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
            git commit -m "$msg" 2>$null | Out-Null
            Write-Log "Local changes committed" "OK"
        } else {
            Write-Log "No local changes to commit" "SKIP"
        }

        # 2. Push to GitHub (Local -> GitHub)
        $ahead = git log origin/main..main --oneline 2>$null
        if ($ahead) {
            $count = ($ahead | Measure-Object).Count
            Write-Log "Pushing $count commit(s) to GitHub..."
            $pushOutput = git push origin main 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Log "Push failed: $pushOutput" "ERROR"
            } else {
                Write-Log "Push complete" "OK"
            }
        } else {
            Write-Log "Nothing to push" "SKIP"
        }

        # 3. Pull from GitHub (GitHub -> Local) to stay in sync
        Write-Log "Pulling from GitHub to stay in sync..."
        $pullOutput = git pull origin main 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Pull failed: $pullOutput" "ERROR"
        } else {
            if ($pullOutput -match "Already up to date") {
                Write-Log "Already up to date with GitHub" "SKIP"
            } else {
                Write-Log "Pulled updates from GitHub" "OK"
            }
        }

        # 4. Update submodules
        Write-Log "Updating submodules..."
        git submodule update --init --recursive 2>$null | Out-Null
        $subOutput = git submodule update --remote --merge 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Submodule update failed: $subOutput" "ERROR"
        } else {
            if ($subOutput -match "Already up to date") {
                Write-Log "Submodules: Already up to date" "SKIP"
            } else {
                Write-Log "Submodules: Updated" "OK"
            }
        }

        Write-Log "========== LOCAL->GITHUB SYNC COMPLETE =========="
    }
    catch {
        Write-Log "Unexpected error: $_" "ERROR"
    }

    Write-Log "Sleeping for $IntervalMinutes minutes..."
    Start-Sleep -Seconds ($IntervalMinutes * 60)
}
