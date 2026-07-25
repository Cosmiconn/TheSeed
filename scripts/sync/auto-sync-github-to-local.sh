#!/usr/bin/env bash
# TheSeed Auto-Sync Daemon: GitHub -> Local (Linux/macOS)
# Pulls changes from GitHub to local every 5 minutes.
# Usage: ./auto-sync-github-to-local.sh [interval_minutes] [log_file]

set -uo pipefail

INTERVAL_MINUTES="${1:-5}"
LOG_FILE="${2:-auto-sync.log}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_PATH="$ROOT/$LOG_FILE"
LOCK_FILE="$ROOT/.auto-sync.lock"

# Prevent multiple instances
if [ -f "$LOCK_FILE" ]; then
    OLD_PID=$(cat "$LOCK_FILE" 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "GitHub->Local sync already running (PID: $OLD_PID)"
        echo "Use ./stop-github-to-local.sh to stop it first."
        exit 1
    fi
fi

echo $$ > "$LOCK_FILE"
echo "GitHub->Local sync started (PID: $$). Press Ctrl+C to stop."
echo "Logging to: $LOG_PATH"
echo ""

write_log() {
    local level="$1"
    local message="$2"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local line="[$timestamp] [$level] $message"
    echo "$line" >> "$LOG_PATH"
    case "$level" in
        ERROR) echo -e "\033[31m$line\033[0m" ;;
        WARN)  echo -e "\033[33m$line\033[0m" ;;
        OK)    echo -e "\033[32m$line\033[0m" ;;
        SKIP)  echo -e "\033[90m$line\033[0m" ;;
        *)     echo "$line" ;;
    esac
}

cleanup() {
    write_log "INFO" "Shutting down GitHub->Local sync..."
    rm -f "$LOCK_FILE"
    exit 0
}
trap cleanup INT TERM EXIT

cd "$ROOT" || exit 1

while true; do
    write_log "INFO" "========== GITHUB->LOCAL SYNC START =========="

    # 1. Pull main repo (GitHub -> Local)
    write_log "INFO" "Pulling TheSeed from GitHub..."
    PULL_OUTPUT=$(git pull origin main 2>&1)
    PULL_STATUS=$?
    if [ $PULL_STATUS -ne 0 ]; then
        write_log "ERROR" "TheSeed pull failed: $PULL_OUTPUT"
    else
        if echo "$PULL_OUTPUT" | grep -q "Already up to date"; then
            write_log "SKIP" "TheSeed: Already up to date"
        else
            write_log "OK" "TheSeed: Updated"
        fi
    fi

    # 2. Update submodules (GitHub -> Local)
    write_log "INFO" "Updating submodules..."
    git submodule update --init --recursive >/dev/null 2>&1
    SUB_OUTPUT=$(git submodule update --remote --merge 2>&1)
    SUB_STATUS=$?
    if [ $SUB_STATUS -ne 0 ]; then
        write_log "ERROR" "Submodule update failed: $SUB_OUTPUT"
    else
        if echo "$SUB_OUTPUT" | grep -q "Already up to date"; then
            write_log "SKIP" "Submodules: Already up to date"
        else
            write_log "OK" "Submodules: Updated"
        fi
    fi

    # 3. Check if submodule pointer changed
    STATUS=$(git status --short 2>/dev/null)
    if echo "$STATUS" | grep -q "submodules/seed-core"; then
        write_log "WARN" "Submodule pointer changed, committing..."
        git add submodules/seed-core >/dev/null 2>&1 || true
        git commit -m "chore(submodule): Auto-sync seed-core" >/dev/null 2>&1 || true
        write_log "OK" "Pointer committed"
    fi

    # 4. Push local commits (Local -> GitHub)
    LOCAL_COMMITS=$(git log origin/main..main --oneline 2>/dev/null)
    if [ -n "$LOCAL_COMMITS" ]; then
        write_log "INFO" "Pushing local commits to GitHub..."
        PUSH_OUTPUT=$(git push origin main 2>&1)
        PUSH_STATUS=$?
        if [ $PUSH_STATUS -ne 0 ]; then
            write_log "ERROR" "Push failed: $PUSH_OUTPUT"
        else
            write_log "OK" "Push complete"
        fi
    else
        write_log "SKIP" "Nothing to push"
    fi

    write_log "INFO" "========== GITHUB->LOCAL SYNC COMPLETE =========="
    write_log "INFO" "Sleeping for $INTERVAL_MINUTES minutes..."
    sleep $((INTERVAL_MINUTES * 60))
done
