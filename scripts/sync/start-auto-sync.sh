#!/usr/bin/env bash
# Start TheSeed Auto-Sync Daemon (Linux/macOS)

cd "$(dirname "$0")/.." || exit 1

if [ -f .auto-sync.lock ]; then
    OLD_PID=$(cat .auto-sync.lock 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "Auto-sync already running (PID: $OLD_PID)"
        echo "Use ./scripts/sync/stop-auto-sync.sh to stop it first."
        exit 1
    fi
fi

nohup ./scripts/sync/auto-sync.sh 5 auto-sync.log > /dev/null 2>&1 &
echo "Auto-sync daemon started (PID: $!)."
echo "Check auto-sync.log for status."
