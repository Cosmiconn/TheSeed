#!/usr/bin/env bash
# Start TheSeed Auto-Sync: Local -> GitHub (Linux/macOS)

cd "$(dirname "$0")/.." || exit 1

if [ -f .auto-sync-local.lock ]; then
    OLD_PID=$(cat .auto-sync-local.lock 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "Local->GitHub sync already running (PID: $OLD_PID)"
        echo "Use ./stop-local-to-github.sh to stop it first."
        exit 1
    fi
fi

nohup ./scripts/sync/auto-sync-local-to-github.sh 5 auto-sync.log > /dev/null 2>&1 &
echo "Local->GitHub sync started (PID: $!)."
echo "Check auto-sync.log for status."
