# TheSeed Auto-Sync

Safe bidirectional sync between local workspace and GitHub every 5 minutes.

## Strategy

```
1. Commit local uncommitted changes  (Local)
2. git pull --rebase origin main    (GitHub -> Local)
3. git push origin main             (Local -> GitHub)
```

**NEVER stashes.** Conflicts abort the cycle, log the error, and retry next cycle.
This preserves commit history and avoids merge-conflict hell.

## What it does every 5 minutes

1. **Local → Commit**: Uncommitted changes are committed with message `auto: sync <timestamp>`
2. **GitHub → Local**: `git pull --rebase origin main` (linear history)
3. **Local → GitHub**: `git push origin main` (only if ahead of remote)
4. **Submodule**: Same 3-step process for `seed-core`
5. **Submodule pointer**: If `seed-core` got new commits, the pointer in TheSeed is updated and pushed

## Safety

| Feature | Behavior |
|---------|----------|
| Lock file | Prevents multiple instances (`.auto-sync.lock`) |
| Rebase | Keeps linear history, no merge commits |
| Conflict | `git rebase --abort` + error log + retry next cycle |
| No stash | Real commits with timestamps, traceable in `git log` |
| Graceful stop | Ctrl+C / SIGTERM cleans up lock file |

## Windows

```cmd
# Start (double-click or right-click -> Run with PowerShell)
scripts\sync\start-auto-sync.cmd

# Stop
scripts\sync\stop-auto-sync.cmd

# Or manually in PowerShell:
scripts\sync\auto-sync.ps1 -IntervalMinutes 5
```

## Linux / macOS

```bash
# Start (background)
./scripts/sync/start-auto-sync.sh

# Stop
./scripts/sync/stop-auto-sync.sh

# Or manually (foreground):
./scripts/sync/auto-sync.sh 5 auto-sync.log
```

## Files

| File | Purpose |
|------|---------|
| `auto-sync.ps1` | PowerShell daemon |
| `auto-sync.sh` | Bash daemon |
| `start-auto-sync.cmd` | Windows starter |
| `stop-auto-sync.cmd` | Windows stopper |
| `start-auto-sync.sh` | Linux/macOS starter (nohup) |
| `stop-auto-sync.sh` | Linux/macOS stopper (SIGTERM) |
| `README.md` | This file |
| `auto-sync.log` | Sync log (created at runtime) |
| `.auto-sync.lock` | Lock file (prevents multiple instances) |

## Log Example

```
[2026-07-25 23:30:00] [INFO] ========== SYNC CYCLE START ==========
[2026-07-25 23:30:00] [WARN] TheSeed has local changes, committing...
[2026-07-25 23:30:00] [OK] TheSeed: local changes committed
[2026-07-25 23:30:01] [OK] TheSeed: pulled successfully
[2026-07-25 23:30:02] [OK] TheSeed: pushed 1 commit(s)
[2026-07-25 23:30:02] [SKIP] seed-core: no local changes
[2026-07-25 23:30:03] [SKIP] seed-core: already up to date
[2026-07-25 23:30:03] [SKIP] seed-core: nothing to push
[2026-07-25 23:30:03] [INFO] ========== SYNC CYCLE COMPLETE ==========
[2026-07-25 23:30:03] [INFO] Sleeping for 5 minutes...
```
