#!/usr/bin/env bash
# TheSeed Auto-Sync Daemon: Local -> GitHub (Linux/macOS)
# Pushes local changes to GitHub every 5 minutes.
# Usage: ./auto-sync-local-to-github.sh [interval_minutes] [log_file]

set -uo pipefail

INTERVAL_MINUTES="${1:-5}"
LOG_FILE="${2:-auto-sync.log}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_PATH="$ROOT/$LOG_FILE"
LOCK_FILE="$ROOT/.auto-sync-local.lock"

# Prevent multiple instances
if [ -f "$LOCK_FILE" ]; then
    OLD_PID=$(cat "$LOCK_FILE" 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "Local->GitHub sync already running (PID: $OLD_PID)"
        echo "Use ./stop-local-to-github.sh to stop it first."
        exit 1
    fi
fi

echo $$ > "$LOCK_FILE"
echo "Local->GitHub sync started (PID: $$). Press Ctrl+C to stop."
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
    write_log "INFO" "Shutting down Local->GitHub sync..."
    rm -f "$LOCK_FILE"
    exit 0
}
trap cleanup INT TERM EXIT

cd "$ROOT" || exit 1

while true; do
    write_log "INFO" "========== LOCAL->GITHUB SYNC START =========="

    # 1. Commit local uncommitted changes (Local)
    STATUS=$(git status --porcelain 2>/dev/null)
    if [ -n "$STATUS" ]; then
        write_log "WARN" "Local changes detected, committing..."
        git add -A >/dev/null 2>&1 || true
        git commit -m "auto: local sync $(date '+%Y-%m-%d %H:%M:%S')" >/dev/null 2>&1 || true
        write_log "OK" "Local changes committed"
    else
        write_log "SKIP" "No local changes to commit"
    fi

    # 2. Push to GitHub (Local -> GitHub)
    AHEAD=$(git log origin/main..main --oneline 2>/dev/null)
    if [ -n "$AHEAD" ]; then
        COUNT=$(echo "$AHEAD" | wc -l)
        write_log "INFO" "Pushing $COUNT commit(s) to GitHub..."
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

    # 3. Pull from GitHub (GitHub -> Local) to stay in sync
    write_log "INFO" "Pulling from GitHub to stay in sync..."
    PULL_OUTPUT=$(git pull origin main 2>&1)
    PULL_STATUS=$?
    if [ $PULL_STATUS -ne 0 ]; then
        write_log "ERROR" "Pull failed: $PULL_OUTPUT"
    else
        if echo "$PULL_OUTPUT" | grep -q "Already up to date"; then
            write_log "SKIP" "Already up to date with GitHub"
        else
            write_log "OK" "Pulled updates from GitHub"
        fi
    fi

    # 4. Update submodules
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

    write_log "INFO" "========== LOCAL->GITHUB SYNC COMPLETE =========="
    write_log "INFO" "Sleeping for $INTERVAL_MINUTES minutes..."
    sleep $((INTERVAL_MINUTES * 60))
done
