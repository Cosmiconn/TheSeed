#!/usr/bin/env bash
# TheSeed Auto-Sync Daemon (Linux/macOS)
# Safe bidirectional sync: Commit local changes, pull with rebase, push.
# NEVER stashes. Conflicts abort the cycle and log the error.
# Usage: ./auto-sync.sh [interval_minutes] [log_file]

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
        echo "Auto-sync already running (PID: $OLD_PID)"
        echo "Use ./stop-auto-sync.sh to stop it first."
        exit 1
    fi
fi

echo $$ > "$LOCK_FILE"
echo "Auto-sync started (PID: $$). Press Ctrl+C to stop."
echo "Logging to: $LOG_PATH"
echo "Strategy: commit -> pull --rebase -> push"
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
    write_log "INFO" "Shutting down auto-sync..."
    rm -f "$LOCK_FILE"
    exit 0
}
trap cleanup INT TERM EXIT

sync_repo() {
    local repo_path="$1"
    local repo_name="$2"
    local remote="${3:-origin}"
    local branch="${4:-main}"

    write_log "INFO" "--- Syncing $repo_name ---"
    cd "$repo_path" || return 1

    # 1. Check for local uncommitted changes
    local status_output
    status_output=$(git status --porcelain 2>/dev/null)
    if [ -n "$status_output" ]; then
        write_log "WARN" "$repo_name has local changes, committing..."
        if ! git add -A >/dev/null 2>&1; then
            write_log "ERROR" "$repo_name: git add failed"
            return 1
        fi
        if ! git commit -m "auto: sync $(date '+%Y-%m-%d %H:%M:%S')" >/dev/null 2>&1; then
            write_log "ERROR" "$repo_name: git commit failed"
            return 1
        fi
        write_log "OK" "$repo_name: local changes committed"
    else
        write_log "SKIP" "$repo_name: no local changes"
    fi

    # 2. Pull from remote (rebase to keep linear history)
    write_log "INFO" "$repo_name: pulling from $remote/$branch..."
    local pull_output
    pull_output=$(git pull --rebase "$remote" "$branch" 2>&1)
    local pull_status=$?
    if [ $pull_status -ne 0 ]; then
        write_log "ERROR" "$repo_name: pull failed (exit $pull_status): $pull_output"
        write_log "ERROR" "$repo_name: aborting rebase and skipping push"
        git rebase --abort >/dev/null 2>&1 || true
        return 1
    fi
    if echo "$pull_output" | grep -q "Already up to date"; then
        write_log "SKIP" "$repo_name: already up to date"
    else
        write_log "OK" "$repo_name: pulled successfully"
    fi

    # 3. Check if we have commits to push
    local ahead
    ahead=$(git log "$remote/$branch..$branch" --oneline 2>/dev/null)
    if [ -n "$ahead" ]; then
        local count
        count=$(echo "$ahead" | wc -l)
        write_log "INFO" "$repo_name: pushing $count commit(s)..."
        local push_output
        push_output=$(git push "$remote" "$branch" 2>&1)
        local push_status=$?
        if [ $push_status -ne 0 ]; then
            write_log "ERROR" "$repo_name: push failed (exit $push_status): $push_output"
            return 1
        fi
        write_log "OK" "$repo_name: pushed successfully"
    else
        write_log "SKIP" "$repo_name: nothing to push"
    fi

    return 0
}

cd "$ROOT" || exit 1

while true; do
    write_log "INFO" "========== SYNC CYCLE START =========="

    # Sync main repo
    sync_repo "$ROOT" "TheSeed"

    # Sync submodule
    SUB_PATH="$ROOT/submodules/seed-core"
    if [ -d "$SUB_PATH/.git" ]; then
        if sync_repo "$SUB_PATH" "seed-core"; then
            # If submodule got new commits from remote, update pointer in main repo
            cd "$ROOT" || continue
            local sub_diff
            sub_diff=$(git diff --submodules 2>/dev/null)
            if [ -n "$sub_diff" ]; then
                write_log "WARN" "Submodule pointer changed, updating TheSeed..."
                git add submodules/seed-core >/dev/null 2>&1 || true
                git commit -m "chore(submodule): auto-sync seed-core $(date '+%Y-%m-%d %H:%M:%S')" >/dev/null 2>&1 || true
                git push origin main >/dev/null 2>&1 || true
                write_log "OK" "Submodule pointer updated"
            fi
        fi
    fi

    write_log "INFO" "========== SYNC CYCLE COMPLETE =========="
    write_log "INFO" "Sleeping for $INTERVAL_MINUTES minutes..."
    sleep $((INTERVAL_MINUTES * 60))
done
