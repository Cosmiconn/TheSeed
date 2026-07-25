#!/usr/bin/env pwsh
# TheSeed Auto-Sync Daemon
# Safe bidirectional sync: Commit local changes, pull with rebase, push.
# NEVER stashes. Conflicts abort the cycle and log the error.
# Window stays visible. Press Ctrl+C or close window to stop.

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
Write-Host "Logging to: $logPath" -ForegroundColor Gray
Write-Host "Strategy: commit -> pull --rebase -> push" -ForegroundColor Gray
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

function Invoke-SafeGit {
    param(
        [string]$RepoPath,
        [string]$Description,
        [string[]]$Arguments,
        [switch]$AbortOnError
    )
    Set-Location $RepoPath
    $output = & git @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Log "$Description FAILED (exit $exitCode): $output" "ERROR"
        if ($AbortOnError) { return $false }
    } else {
        $trimmed = ($output | Out-String).Trim()
        if ($trimmed) {
            Write-Log "$Description OK: $trimmed" "OK"
        } else {
            Write-Log "$Description OK" "OK"
        }
    }
    return ($exitCode -eq 0)
}

function Sync-Repo {
    param(
        [string]$RepoPath,
        [string]$RepoName,
        [string]$Remote = "origin",
        [string]$Branch = "main"
    )
    Write-Log "--- Syncing $RepoName ---"
    Set-Location $RepoPath

    # 1. Check for local uncommitted changes
    $status = git status --porcelain 2>$null
    $hasLocalChanges = $status -and ($status.Trim().Length -gt 0)

    if ($hasLocalChanges) {
        Write-Log "$RepoName has local changes, committing..." "WARN"
        $commitOk = Invoke-SafeGit -RepoPath $RepoPath -Description "$RepoName add" `
            -Arguments @("add", "-A") -AbortOnError
        if (-not $commitOk) { return $false }

        $msg = "auto: sync $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
        $commitOk = Invoke-SafeGit -RepoPath $RepoPath -Description "$RepoName commit" `
            -Arguments @("commit", "-m", $msg) -AbortOnError
        if (-not $commitOk) { return $false }
    } else {
        Write-Log "$RepoName: no local changes" "SKIP"
    }

    # 2. Pull from remote (rebase to keep linear history)
    Write-Log "$RepoName: pulling from $Remote/$Branch..."
    $pullOk = Invoke-SafeGit -RepoPath $RepoPath -Description "$RepoName pull" `
        -Arguments @("pull", "--rebase", $Remote, $Branch) -AbortOnError
    if (-not $pullOk) {
        Write-Log "$RepoName: pull failed. Aborting rebase and skipping push." "ERROR"
        & git rebase --abort 2>$null | Out-Null
        return $false
    }

    # 3. Check if we have commits to push
    $ahead = & git log "$Remote/$Branch..$Branch" --oneline 2>$null
    if ($ahead) {
        $count = ($ahead | Measure-Object).Count
        Write-Log "$RepoName: pushing $count commit(s)..."
        $pushOk = Invoke-SafeGit -RepoPath $RepoPath -Description "$RepoName push" `
            -Arguments @("push", $Remote, $Branch) -AbortOnError
        if (-not $pushOk) { return $false }
    } else {
        Write-Log "$RepoName: nothing to push" "SKIP"
    }

    return $true
}

# Cleanup on exit (Ctrl+C, window close, or normal exit)
$null = Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action {
    Remove-Item -Path $lockFile -ErrorAction SilentlyContinue
}

try {
    Set-Location $root

    while ($true) {
        Write-Log "========== SYNC CYCLE START =========="

        # Sync main repo
        $mainOk = Sync-Repo -RepoPath $root -RepoName "TheSeed"

        # Sync submodule
        $subPath = Join-Path $root "submodules" "seed-core"
        if (Test-Path $subPath) {
            $subOk = Sync-Repo -RepoPath $subPath -RepoName "seed-core"

            # If submodule got new commits from remote, update pointer in main repo
            if ($subOk) {
                Set-Location $root
                $subDiff = & git diff --submodules 2>$null
                if ($subDiff) {
                    Write-Log "Submodule pointer changed, updating TheSeed..." "WARN"
                    Invoke-SafeGit -RepoPath $root -Description "Update submodule pointer" `
                        -Arguments @("add", "submodules/seed-core") -AbortOnError | Out-Null
                    $msg = "chore(submodule): auto-sync seed-core $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
                    Invoke-SafeGit -RepoPath $root -Description "Commit submodule pointer" `
                        -Arguments @("commit", "-m", $msg) -AbortOnError | Out-Null
                    Invoke-SafeGit -RepoPath $root -Description "Push submodule pointer" `
                        -Arguments @("push", "origin", "main") -AbortOnError | Out-Null
                }
            }
        }

        Write-Log "========== SYNC CYCLE COMPLETE =========="
        Write-Log "Sleeping for $IntervalMinutes minutes..."
        Start-Sleep -Seconds ($IntervalMinutes * 60)
    }
} finally {
    Remove-Item -Path $lockFile -ErrorAction SilentlyContinue
    Write-Log "INFO" "Auto-sync stopped."
}
