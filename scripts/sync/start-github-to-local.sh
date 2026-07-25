#!/usr/bin/env bash
# Start TheSeed Auto-Sync: GitHub -> Local (Linux/macOS)

cd "$(dirname "$0")/.." || exit 1

if [ -f .auto-sync.lock ]; then
    OLD_PID=$(cat .auto-sync.lock 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "GitHub->Local sync already running (PID: $OLD_PID)"
        echo "Use ./stop-github-to-local.sh to stop it first."
        exit 1
    fi
fi

nohup ./scripts/sync/auto-sync-github-to-local.sh 5 auto-sync.log > /dev/null 2>&1 &
echo "GitHub->Local sync started (PID: $!)."
echo "Check auto-sync.log for status."
