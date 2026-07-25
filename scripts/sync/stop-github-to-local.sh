#!/usr/bin/env bash
# Stop TheSeed Auto-Sync: GitHub -> Local (Linux/macOS)

cd "$(dirname "$0")/.." || exit 1

if [ ! -f .auto-sync.lock ]; then
    echo "No GitHub->Local sync lock file found."
    exit 1
fi

PID=$(cat .auto-sync.lock 2>/dev/null)
if [ -z "$PID" ]; then
    echo "Lock file empty. Removing stale lock."
    rm -f .auto-sync.lock
    exit 1
fi

if kill -0 "$PID" 2>/dev/null; then
    echo "Stopping GitHub->Local sync (PID: $PID)..."
    kill -TERM "$PID" 2>/dev/null
    sleep 1
    if kill -0 "$PID" 2>/dev/null; then
        kill -KILL "$PID" 2>/dev/null
    fi
    echo "Sync stopped."
else
    echo "Process not found. Removing stale lock."
fi

rm -f .auto-sync.lock
