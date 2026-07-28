#!/usr/bin/env python3
"""
TheSeed Submodule Sync Tool - CLI Version
============================================
Complete bidirectional sync between GitHub and local repository.

Usage:
    python theseed_sync.py --fix-gitmodules
    python theseed_sync.py --init
    python theseed_sync.py --github-to-local
    python theseed_sync.py --local-to-github
    python theseed_sync.py --full
    python theseed_sync.py --full --push
    python theseed_sync.py --auto-sync --interval 5
"""

import argparse
import configparser
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Tuple, Dict


class Colors:
    OK = "\033[92m"
    WARN = "\033[93m"
    ERROR = "\033[91m"
    INFO = "\033[94m"
    CMD = "\033[95m"
    SKIP = "\033[90m"
    RESET = "\033[0m"


def log(level: str, msg: str) -> None:
    color = getattr(Colors, level, Colors.INFO)
    timestamp = time.strftime("%H:%M:%S")
    print(f"{color}[{timestamp}] [{level}]{Colors.RESET} {msg}")


def run(cmd: List[str], cwd: Path, check: bool = False, silent: bool = False) -> Tuple[int, str, str]:
    if not silent:
        log("CMD", f"{' '.join(cmd)}  (in {cwd})")
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False, timeout=120)
        if result.stdout and not silent:
            for line in result.stdout.strip().splitlines():
                log("INFO", f"  > {line}")
        if result.stderr and not silent:
            for line in result.stderr.strip().splitlines():
                if "warning" in line.lower():
                    log("WARN", f"  ! {line}")
                else:
                    log("INFO", f"  ! {line}")
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        log("ERROR", "  X Command timed out after 120s")
        return 1, "", "timeout"
    except Exception as e:
        log("ERROR", f"  X Exception: {e}")
        return 1, "", str(e)


def find_repo_root() -> Path:
    current = Path.cwd().resolve()
    for p in [current] + list(current.parents):
        if (p / ".git").exists() and (p / ".gitmodules").exists():
            return p
    for p in [current] + list(current.parents):
        if p.name.lower() == "these" or (p / "submodules").exists():
            if (p / ".git").exists():
                return p
    log("ERROR", "Not inside a git repository with .gitmodules")
    sys.exit(1)


def get_submodules(repo_root: Path) -> List[Dict]:
    gm = repo_root / ".gitmodules"
    if not gm.exists():
        return []

    config = configparser.ConfigParser()
    config.read(gm)

    submodules = []
    for section in config.sections():
        if section.startswith("submodule"):
            name = section.replace("submodule ", "").strip('"')
            submodules.append({
                "name": name,
                "path": config.get(section, "path", fallback=""),
                "url": config.get(section, "url", fallback=""),
                "branch": config.get(section, "branch", fallback="main"),
            })
    return submodules


def fix_gitmodules(repo_root: Path) -> bool:
    log("INFO", "=== FIX .GITMODULES ===")
    gm = repo_root / ".gitmodules"
    if not gm.exists():
        log("ERROR", ".gitmodules not found!")
        return False

    config = configparser.ConfigParser()
    config.read(gm)
    changed = False

    for section in config.sections():
        if not section.startswith("submodule"):
            continue
        name = section.replace("submodule ", "").strip('"')
        if not config.has_option(section, "branch"):
            config.set(section, "branch", "main")
            log("WARN", f"  Added 'branch = main' to {name}")
            changed = True
        else:
            branch = config.get(section, "branch")
            if branch != "main":
                config.set(section, "branch", "main")
                log("WARN", f"  Changed branch from '{branch}' to 'main' in {name}")
                changed = True

    if changed:
        with open(gm, "w") as f:
            config.write(f)
        log("OK", ".gitmodules updated")
        run(["git", "add", ".gitmodules"], repo_root, check=False)
        rc, _, _ = run(["git", "commit", "-m", "chore(gitmodules): enforce branch = main"],
                      repo_root, check=False)
        if rc == 0:
            log("OK", ".gitmodules committed")
        return True
    else:
        log("OK", ".gitmodules already correct")
        return False


def init_submodules(repo_root: Path, submodules: List[Dict]) -> bool:
    log("INFO", "=== INIT SUBMODULES ===")
    rc, _, _ = run(["git", "submodule", "update", "--init", "--recursive"],
                   repo_root, check=False)
    if rc == 0:
        log("OK", "Submodules initialized")
    else:
        log("ERROR", "Submodule init failed")
        return False

    # Checkout main branch in each
    for sub in submodules:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue

        rc2, out2, _ = run(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                          full, check=False, silent=True)
        current = out2.strip()

        if current == "HEAD":
            log("WARN", f"{sub['name']}: Detached HEAD -> checkout {sub['branch']}")
            run(["git", "checkout", "-B", sub["branch"], f"origin/{sub['branch']}"],
               full, check=False)
        elif current != sub["branch"]:
            log("WARN", f"{sub['name']}: Switching '{current}' -> '{sub['branch']}'")
            run(["git", "checkout", sub["branch"]], full, check=False)
            run(["git", "branch", "--set-upstream-to",
                f"origin/{sub['branch']}", sub["branch"]], full, check=False)

    return True


def github_to_local(repo_root: Path, submodules: List[Dict]) -> bool:
    log("INFO", "=== GITHUB -> LOCAL ===")

    # Pull meta-repo
    rc, out, _ = run(["git", "pull", "origin", "main"], repo_root, check=False)
    if rc == 0:
        if "Already up to date" in out:
            log("SKIP", "Meta-repo: already up to date")
        else:
            log("OK", "Meta-repo: updated")
    else:
        log("ERROR", "Meta-repo pull failed")
        return False

    # Sync each submodule
    for sub in submodules:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            log("WARN", f"{sub['name']}: not initialized")
            continue

        log("INFO", f"--- {sub['name']} ---")
        run(["git", "fetch", "origin"], full, check=False)

        rc2, out2, _ = run(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                          full, check=False, silent=True)
        current = out2.strip()

        if current == "HEAD":
            run(["git", "checkout", "-B", sub["branch"], f"origin/{sub['branch']}"],
               full, check=False)
        elif current != sub["branch"]:
            run(["git", "checkout", sub["branch"]], full, check=False)
            run(["git", "branch", "--set-upstream-to",
                f"origin/{sub['branch']}", sub["branch"]], full, check=False)

        rc3, out3, _ = run(["git", "pull", "origin", sub["branch"]], full, check=False)
        if rc3 == 0:
            if "Already up to date" in out3:
                log("SKIP", f"  {sub['name']}: already up to date")
            else:
                log("OK", f"  {sub['name']}: updated")
        else:
            log("ERROR", f"  {sub['name']}: pull failed")

    update_meta_pointer(repo_root, submodules)
    return True


def local_to_github(repo_root: Path, submodules: List[Dict]) -> bool:
    log("INFO", "=== LOCAL -> GITHUB ===")

    # Commit submodule changes
    for sub in submodules:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue

        rc, out, _ = run(["git", "status", "--short"], full, check=False, silent=True)
        if out.strip():
            log("WARN", f"{sub['name']}: committing local changes...")
            run(["git", "add", "-A"], full, check=False)
            run(["git", "commit", "-m",
                f"auto: sync {sub['name']} {time.strftime('%Y-%m-%d %H:%M:%S')}"],
               full, check=False)
            log("OK", f"  {sub['name']}: committed")

    # Push submodules
    for sub in submodules:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue

        rc, out, _ = run(["git", "log", f"origin/{sub['branch']}..{sub['branch']}", "--oneline"],
                        full, check=False, silent=True)
        if out.strip():
            count = len(out.strip().splitlines())
            log("INFO", f"{sub['name']}: pushing {count} commit(s)...")
            rc2, _, err = run(["git", "push", "origin", sub["branch"]], full, check=False)
            if rc2 == 0:
                log("OK", f"  {sub['name']}: pushed")
            else:
                log("ERROR", f"  {sub['name']}: push failed - {err}")
        else:
            log("SKIP", f"{sub['name']}: nothing to push")

    # Update meta pointer
    update_meta_pointer(repo_root, submodules)

    # Commit meta-repo
    rc, out, _ = run(["git", "status", "--short"], repo_root, check=False, silent=True)
    if out.strip():
        log("WARN", "Meta-repo: committing local changes...")
        run(["git", "add", "-A"], repo_root, check=False)
        run(["git", "commit", "-m",
            f"auto: meta-repo sync {time.strftime('%Y-%m-%d %H:%M:%S')}"],
           repo_root, check=False)
        log("OK", "Meta-repo: committed")

    # Push meta-repo
    rc, out, _ = run(["git", "log", "origin/main..main", "--oneline"],
                    repo_root, check=False, silent=True)
    if out.strip():
        count = len(out.strip().splitlines())
        log("INFO", f"Meta-repo: pushing {count} commit(s)...")
        rc2, _, err = run(["git", "push", "origin", "main"], repo_root, check=False)
        if rc2 == 0:
            log("OK", "Meta-repo: pushed to GitHub")
        else:
            log("ERROR", f"Meta-repo: push failed - {err}")
    else:
        log("SKIP", "Meta-repo: nothing to push")

    return True


def update_meta_pointer(repo_root: Path, submodules: List[Dict]) -> None:
    log("INFO", "Checking meta-repo submodule pointers...")
    changed = []

    for sub in submodules:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue

        rc, out, _ = run(["git", "diff", "--submodule", sub["path"]],
                        repo_root, check=False, silent=True)
        if out.strip():
            changed.append(sub["path"])

    if not changed:
        log("SKIP", "No submodule pointer changes")
        return

    log("WARN", f"Pointer changes: {changed}")
    for c in changed:
        run(["git", "add", c], repo_root, check=False)

    rc, _, _ = run(["git", "commit", "-m",
                   f"chore(submodule): sync pointers {time.strftime('%Y-%m-%d %H:%M:%S')}"],
                  repo_root, check=False)
    if rc == 0:
        log("OK", "Meta-repo pointer updated")


def full_sync(repo_root: Path, submodules: List[Dict]) -> None:
    log("INFO", "=== FULL BIDIRECTIONAL SYNC ===")
    fix_gitmodules(repo_root)
    init_submodules(repo_root, submodules)
    github_to_local(repo_root, submodules)
    local_to_github(repo_root, submodules)
    log("OK", "=== SYNC COMPLETE ===")


def auto_sync(repo_root: Path, submodules: List[Dict], interval_min: int):
    log("INFO", f"Auto-Sync started (every {interval_min} minutes). Press Ctrl+C to stop.")
    try:
        while True:
            full_sync(repo_root, submodules)
            log("INFO", f"Sleeping {interval_min} minutes...")
            time.sleep(interval_min * 60)
    except KeyboardInterrupt:
        log("INFO", "Auto-Sync stopped by user")


def main():
    parser = argparse.ArgumentParser(description="TheSeed Submodule Sync Tool")
    parser.add_argument("--fix-gitmodules", action="store_true", help="Add branch = main")
    parser.add_argument("--init", action="store_true", help="Initialize submodules")
    parser.add_argument("--github-to-local", action="store_true", help="Pull from GitHub")
    parser.add_argument("--local-to-github", action="store_true", help="Push to GitHub")
    parser.add_argument("--full", action="store_true", help="Full bidirectional sync")
    parser.add_argument("--auto-sync", action="store_true", help="Run sync in loop")
    parser.add_argument("--interval", type=int, default=5, help="Auto-sync interval (min)")
    parser.add_argument("--gui", action="store_true", help="Launch GUI")

    args = parser.parse_args()

    if args.gui:
        import theseed_sync_gui as gui
        gui.main()
        return

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    repo_root = find_repo_root()
    os.chdir(repo_root)
    submodules = get_submodules(repo_root)

    if not submodules:
        log("ERROR", "No submodules found in .gitmodules")
        sys.exit(1)

    if args.fix_gitmodules:
        fix_gitmodules(repo_root)

    if args.init:
        init_submodules(repo_root, submodules)

    if args.github_to_local:
        github_to_local(repo_root, submodules)

    if args.local_to_github:
        local_to_github(repo_root, submodules)

    if args.full:
        full_sync(repo_root, submodules)

    if args.auto_sync:
        auto_sync(repo_root, submodules, args.interval)


if __name__ == "__main__":
    main()
