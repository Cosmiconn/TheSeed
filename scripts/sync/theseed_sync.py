#!/usr/bin/env python3
"""
TheSeed Submodule Sync Tool - CLI Version
============================================
Complete bidirectional sync between GitHub and local repository.
Build & Test integration with AUTO-INSTALL of missing dependencies.
Roadmap conform.

Usage:
    python theseed_sync.py --fix-gitmodules
    python theseed_sync.py --init
    python theseed_sync.py --github-to-local
    python theseed_sync.py --local-to-github
    python theseed_sync.py --full
    python theseed_sync.py --build --preset windows-release
    python theseed_sync.py --test --test-filter gate_p0
    python theseed_sync.py --gui
    python theseed_sync.py --health-check
    python theseed_sync.py --dry-run --github-to-local
    python theseed_sync.py --update-submodules
    python theseed_sync.py --release --version 1.2.3
    python theseed_sync.py --clean
    python theseed_sync.py --configure-only --preset windows-debug
"""

import argparse
import configparser
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# =============================================================================
# WORKSPACE PROFILE
# =============================================================================

PROFILE_PATH = Path.home() / ".config" / "theseed" / "sync_profile.json"
if platform.system() == "Windows":
    PROFILE_PATH = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local")) / "TheSeed" / "sync_profile.json"


def load_profile() -> Dict[str, Any]:
    """Load user workspace profile (not committed to repo)."""
    if PROFILE_PATH.exists():
        try:
            with open(PROFILE_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_profile(profile: Dict[str, Any]) -> None:
    """Save user workspace profile."""
    PROFILE_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(PROFILE_PATH, "w", encoding="utf-8") as f:
        json.dump(profile, f, indent=2)


def get_profile_value(key: str, default: Any = None) -> Any:
    return load_profile().get(key, default)


def set_profile_value(key: str, value: Any) -> None:
    profile = load_profile()
    profile[key] = value
    save_profile(profile)


# =============================================================================
# LOGGING & UTILITIES
# =============================================================================

LOG_BUFFER: List[str] = []


class Colors:
    OK = "\033[92m"
    WARN = "\033[93m"
    ERROR = "\033[91m"
    INFO = "\033[94m"
    CMD = "\033[95m"
    SKIP = "\033[90m"
    INSTALL = "\033[96m"
    RESET = "\033[0m"


def log(level: str, msg: str) -> None:
    color = getattr(Colors, level, Colors.INFO)
    timestamp = time.strftime("%H:%M:%S")
    line = f"{color}[{timestamp}] [{level}]{Colors.RESET} {msg}"
    print(line)
    LOG_BUFFER.append(f"[{timestamp}] [{level}] {msg}")


def export_log(path: Path) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(LOG_BUFFER))
    log("OK", f"Log exported to {path}")


# Global dry-run flag
DRY_RUN = False


def run(cmd: List[str], cwd: Path, check: bool = False, silent: bool = False,
        timeout: int = 60, env: Optional[Dict[str, str]] = None) -> Tuple[int, str, str]:
    if not silent:
        log("CMD", f"{' '.join(cmd)}  (in {cwd})")
    if DRY_RUN:
        log("SKIP", "  [DRY-RUN] Command not executed")
        return 0, "", ""
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                                check=False, timeout=timeout, env=env)
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
        log("ERROR", f"  X Command timed out after {timeout}s")
        return 1, "", "timeout"
    except Exception as e:
        log("ERROR", f"  X Exception: {e}")
        return 1, "", str(e)


def run_parallel(tasks: List[Tuple[List[str], Path, int]],
                 max_workers: int = 4) -> List[Tuple[int, str, str]]:
    """Run multiple git commands in parallel."""
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(run, cmd, cwd, False, True, timeout): i
            for i, (cmd, cwd, timeout) in enumerate(tasks)
        }
        for future in as_completed(futures):
            i = futures[future]
            try:
                rc, out, err = future.result()
                results.append((rc, out, err))
            except Exception as e:
                results.append((1, "", str(e)))
    return results


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


def get_submodules(repo_root: Path) -> List[Dict[str, Any]]:
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


def get_submodule_size(repo_root: Path, sub_path: str) -> str:
    """Return human-readable size of a submodule directory."""
    full = repo_root / sub_path
    if not full.exists():
        return "N/A"
    try:
        total = sum(f.stat().st_size for f in full.rglob("*") if f.is_file())
        for unit in ["B", "KB", "MB", "GB"]:
            if total < 1024:
                return f"{total:.1f} {unit}"
            total /= 1024
        return f"{total:.1f} TB"
    except Exception:
        return "?"


def get_submodule_hashes(repo_root: Path, sub: Dict[str, Any]) -> Dict[str, str]:
    """Get current, meta-repo, and origin/main hashes for a submodule."""
    full = repo_root / sub["path"]
    result = {"current": "?", "meta": "?", "origin": "?"}
    if not (full / ".git").exists():
        return result

    # Current HEAD hash
    rc, out, _ = run(["git", "rev-parse", "HEAD"], full, silent=True)
    if rc == 0:
        result["current"] = out.strip()[:8]

    # Meta-repo recorded hash
    rc, out, _ = run(["git", "ls-tree", "HEAD", sub["path"]], repo_root, silent=True)
    if rc == 0 and out.strip():
        parts = out.strip().split()
        if len(parts) >= 3:
            result["meta"] = parts[2][:8]

    # origin/main hash
    rc, out, _ = run(["git", "rev-parse", f"origin/{sub['branch']}"], full, silent=True)
    if rc == 0:
        result["origin"] = out.strip()[:8]

    return result


def check_local_changes(full: Path) -> bool:
    """Return True if submodule has local uncommitted changes."""
    rc, out, _ = run(["git", "status", "--short"], full, silent=True)
    return bool(out.strip())


def stash_changes(full: Path, name: str) -> bool:
    """Stash local changes with a descriptive message."""
    rc, _, _ = run(["git", "stash", "push", "-m", f"theseed-sync-auto-{name}"],
                   full, silent=True)
    return rc == 0


def pop_stash(full: Path) -> bool:
    """Pop the most recent stash."""
    rc, _, _ = run(["git", "stash", "pop"], full, silent=True)
    return rc == 0


def notify(title: str, message: str) -> None:
    """Cross-platform desktop notification."""
    system = platform.system()
    try:
        if system == "Windows":
            from ctypes import windll
            windll.user32.MessageBoxW(0, message, title, 0x40)
        elif system == "Darwin":
            subprocess.run(["osascript", "-e",
                            f'display notification "{message}" with title "{title}"'],
                           check=False, capture_output=True)
        else:
            subprocess.run(["notify-send", title, message],
                           check=False, capture_output=True)
    except Exception:
        pass


# =============================================================================
# .GITMODULES & .GITIGNORE
# =============================================================================

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


def ensure_gitignore(repo_root: Path) -> bool:
    log("INFO", "Checking .gitignore...")
    gitignore = repo_root / ".gitignore"
    required_patterns = [
        "/vcpkg/", "/build/", "/.vs/", "/out/",
        "/CMakeUserPresets.json", "/.cache/", "*.user",
    ]
    existing = set()
    if gitignore.exists():
        with open(gitignore, "r", encoding="utf-8") as f:
            for line in f:
                existing.add(line.strip().rstrip("/"))
    missing = []
    for p in required_patterns:
        clean = p.strip().rstrip("/")
        if clean not in existing and clean.lstrip("/") not in existing:
            missing.append(p)
    if missing:
        log("WARN", f"Missing .gitignore entries: {missing}")
        with open(gitignore, "a", encoding="utf-8") as f:
            f.write("\n# TheSeed sync tool auto-generated ignores\n")
            for p in missing:
                f.write(f"{p}\n")
        log("OK", ".gitignore updated")
        return True
    else:
        log("SKIP", ".gitignore already complete")
        return False


# =============================================================================
# HEALTH CHECK
# =============================================================================

def health_check(repo_root: Path, submodules: List[Dict[str, Any]]) -> bool:
    log("INFO", "=== HEALTH CHECK ===")
    all_ok = True

    for sub in submodules:
        full = repo_root / sub["path"]
        log("INFO", f"--- {sub['name']} ---")

        if not (full / ".git").exists():
            log("ERROR", f"  Not initialized")
            all_ok = False
            continue

        # Check remote reachable
        rc, _, err = run(["git", "ls-remote", "--heads", "origin", sub["branch"]],
                        full, silent=True, timeout=15)
        if rc != 0:
            log("ERROR", f"  Remote unreachable: {err}")
            all_ok = False
        else:
            log("OK", f"  Remote reachable")

        # Check URL matches .gitmodules
        rc, out, _ = run(["git", "remote", "get-url", "origin"], full, silent=True)
        actual_url = out.strip()
        expected_url = sub["url"]
        if actual_url != expected_url:
            log("WARN", f"  URL mismatch: expected {expected_url}, got {actual_url}")
        else:
            log("OK", f"  URL correct")

        # Check for diverged branches
        rc, out, _ = run(["git", "log", f"HEAD...origin/{sub['branch']}", "--oneline"],
                        full, silent=True)
        if out.strip():
            log("WARN", f"  Diverged from origin/{sub['branch']}")
        else:
            log("OK", f"  No divergence")

        # Check LFS status
        rc, out, _ = run(["git", "lfs", "status"], full, silent=True)
        if rc == 0 and out.strip():
            if "clean" not in out.lower():
                log("WARN", f"  LFS: {out.strip()}")
            else:
                log("OK", f"  LFS clean")
        elif rc != 0:
            log("SKIP", f"  LFS not installed/configured")

        # Size
        size = get_submodule_size(repo_root, sub["path"])
        log("INFO", f"  Size: {size}")

    if all_ok:
        log("OK", "=== HEALTH CHECK PASSED ===")
    else:
        log("ERROR", "=== HEALTH CHECK FOUND ISSUES ===")
    return all_ok


# =============================================================================
# AUTO-CLEANUP
# =============================================================================

def auto_cleanup(repo_root: Path, preset: Optional[str] = None) -> bool:
    log("INFO", "=== AUTO CLEANUP ===")
    cleaned = []

    # Clean build dirs
    build_dir = repo_root / "build"
    if build_dir.exists():
        if preset:
            target = build_dir / preset
            if target.exists():
                shutil.rmtree(target)
                cleaned.append(f"build/{preset}")
        else:
            shutil.rmtree(build_dir)
            cleaned.append("build/")

    # Clean CMake cache
    for cache in [repo_root / "CMakeCache.txt", repo_root / "CMakeFiles"]:
        if cache.exists():
            if cache.is_dir():
                shutil.rmtree(cache)
            else:
                cache.unlink()
            cleaned.append(str(cache.name))

    # Clean vcpkg buildtrees (optional, keeps installed packages)
    vcpkg_root = os.environ.get("VCPKG_ROOT", "")
    if vcpkg_root:
        for sub in ["buildtrees", "downloads"]:
            p = Path(vcpkg_root) / sub
            if p.exists():
                # Don't delete, just report
                log("INFO", f"  vcpkg/{sub} can be cleaned manually: {p}")

    if cleaned:
        log("OK", f"Cleaned: {cleaned}")
    else:
        log("SKIP", "Nothing to clean")
    return True


# =============================================================================
# VCPKG & COMPILER
# =============================================================================

def _get_vcpkg_global_dir() -> Path:
    system = platform.system()
    if system == "Windows":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
        return base / "TheSeed" / "vcpkg"
    else:
        return Path.home() / ".local" / "share" / "theseed" / "vcpkg"


def ensure_vcpkg(repo_root: Path) -> bool:
    log("INFO", "Checking vcpkg...")
    vcpkg_env = os.environ.get("VCPKG_ROOT", "")
    vcpkg_env_path = Path(vcpkg_env) / "scripts" / "buildsystems" / "vcpkg.cmake" if vcpkg_env else None
    vcpkg_global = _get_vcpkg_global_dir()
    vcpkg_legacy = repo_root / "vcpkg"

    if vcpkg_env_path and vcpkg_env_path.exists():
        log("OK", f"vcpkg found via VCPKG_ROOT: {vcpkg_env}")
        return True
    if (vcpkg_global / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
        log("OK", f"vcpkg found globally: {vcpkg_global}")
        os.environ["VCPKG_ROOT"] = str(vcpkg_global)
        return True
    if (vcpkg_legacy / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
        log("WARN", f"vcpkg found inside repo: {vcpkg_legacy}")
        log("WARN", "  This can interfere with git sync. Consider moving it outside the repo.")
        os.environ["VCPKG_ROOT"] = str(vcpkg_legacy)
        return True

    log("INSTALL", f"vcpkg not found. Auto-installing to {vcpkg_global}...")
    vcpkg_global.parent.mkdir(parents=True, exist_ok=True)
    rc, out, err = run(
        ["git", "clone", "https://github.com/Microsoft/vcpkg.git", str(vcpkg_global)],
        vcpkg_global.parent, check=False, timeout=120)
    if rc != 0:
        log("ERROR", f"Failed to clone vcpkg: {err}")
        return False

    if platform.system() == "Windows":
        bootstrap = vcpkg_global / "bootstrap-vcpkg.bat"
        rc, out, err = run([str(bootstrap)], vcpkg_global, check=False, timeout=120)
    else:
        bootstrap = vcpkg_global / "bootstrap-vcpkg.sh"
        rc, out, err = run(["bash", str(bootstrap)], vcpkg_global, check=False, timeout=120)

    if rc != 0:
        log("ERROR", f"vcpkg bootstrap failed: {err}")
        return False

    os.environ["VCPKG_ROOT"] = str(vcpkg_global)
    log("OK", f"vcpkg installed at: {vcpkg_global}")
    return True


def ensure_ninja(repo_root: Path) -> bool:
    log("INFO", "Checking Ninja...")
    rc, _, _ = run(["ninja", "--version"], repo_root, check=False, silent=True)
    if rc == 0:
        log("OK", "Ninja found")
        return True
    log("INSTALL", "Ninja not found. Auto-installing via pip...")
    rc, out, err = run(
        [sys.executable, "-m", "pip", "install", "ninja"],
        repo_root, check=False, timeout=120)
    if rc != 0:
        log("ERROR", f"Failed to install Ninja: {err}")
        return False
    rc, out, _ = run(["ninja", "--version"], repo_root, check=False, silent=True)
    if rc == 0:
        log("OK", "Ninja installed successfully")
        return True
    else:
        log("ERROR", "Ninja installed but not in PATH. Restart the tool.")
        return False


def find_vcvarsall() -> Optional[Path]:
    log("INFO", "Searching for vcvarsall.bat...")
    program_files = os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    program_files_64 = os.environ.get("ProgramFiles", "C:\\Program Files")
    editions = ["Community", "Professional", "Enterprise", "BuildTools"]
    years = ["2026", "18", "2025", "2024", "2022", "2019"]
    for pf in [program_files_64, program_files]:
        for year in years:
            for edition in editions:
                vcvars = Path(pf) / "Microsoft Visual Studio" / year / edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
                if vcvars.exists():
                    log("OK", f"  Found: {vcvars}")
                    return vcvars
    log("WARN", "  vcvarsall.bat not found in any standard location")
    return None


def load_vcvars_env(vcvars_path: Path) -> Dict[str, str]:
    import tempfile
    log("INFO", f"Loading MSVC environment from: {vcvars_path}")
    env_file = tempfile.mktemp(suffix=".txt")
    try:
        cmd_str = f'"{vcvars_path}" x64 && set > "{env_file}"'
        result = subprocess.run(cmd_str, shell=True, capture_output=True,
                                text=True, check=False, timeout=60)
        if result.returncode != 0:
            log("ERROR", f"vcvarsall.bat failed: {result.stderr}")
            return {}
        env = {}
        if not Path(env_file).exists():
            log("ERROR", "Environment file not created")
            return {}
        with open(env_file, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    key, value = line.split("=", 1)
                    env[key] = value
        log("OK", f"Loaded {len(env)} environment variables from vcvarsall.bat")
        return env
    except Exception as e:
        log("ERROR", f"Failed to load vcvars env: {e}")
        return {}
    finally:
        try:
            Path(env_file).unlink(missing_ok=True)
        except Exception:
            pass


def ensure_compiler(repo_root: Path) -> bool:
    log("INFO", "Checking C++ compiler...")
    system = platform.system()
    if system == "Windows":
        rc, _, _ = run(["cl"], repo_root, check=False, silent=True)
        if rc == 0:
            log("OK", "MSVC (cl) found in PATH")
            return True
        vcvars = find_vcvarsall()
        if vcvars:
            log("OK", f"MSVC found via vcvarsall.bat: {vcvars}")
            return True
        rc2, _, _ = run(["g++", "--version"], repo_root, check=False, silent=True)
        if rc2 == 0:
            log("OK", "GCC (g++) found")
            return True
        log("ERROR", "C++ Compiler NOT FOUND")
        return False
    else:
        rc, _, _ = run(["g++", "--version"], repo_root, check=False, silent=True)
        if rc == 0:
            log("OK", "GCC (g++) found")
            return True
        rc2, _, _ = run(["clang++", "--version"], repo_root, check=False, silent=True)
        if rc2 == 0:
            log("OK", "Clang (clang++) found")
            return True
        log("ERROR", "C++ Compiler NOT FOUND")
        return False


def ensure_cmake(repo_root: Path) -> bool:
    log("INFO", "Checking CMake...")
    rc, out, _ = run(["cmake", "--version"], repo_root, check=False, silent=True)
    if rc == 0:
        version_line = out.strip().splitlines()[0] if out.strip() else "unknown"
        log("OK", f"CMake found: {version_line}")
        return True
    log("ERROR", "CMake NOT FOUND")
    return False


def ensure_vcpkg_deps(repo_root: Path) -> bool:
    log("INFO", "Checking vcpkg dependencies...")
    vcpkg_json = repo_root / "vcpkg.json"
    if not vcpkg_json.exists():
        log("SKIP", "vcpkg.json not found, skipping dependency install")
        return True
    vcpkg_root = os.environ.get("VCPKG_ROOT", "")
    if not vcpkg_root:
        log("ERROR", "VCPKG_ROOT not set after vcpkg install")
        return False
    vcpkg_exe = Path(vcpkg_root) / "vcpkg.exe" if platform.system() == "Windows" else Path(vcpkg_root) / "vcpkg"
    if not vcpkg_exe.exists():
        log("ERROR", f"vcpkg executable not found: {vcpkg_exe}")
        return False
    triplet = "x64-windows" if platform.system() == "Windows" else "x64-linux"
    log("INSTALL", f"Installing vcpkg dependencies for {triplet}...")
    rc, out, err = run(
        [str(vcpkg_exe), "install", f"--triplet={triplet}"],
        repo_root, check=False, timeout=600)
    if rc == 0:
        log("OK", "vcpkg dependencies installed")
        return True
    else:
        log("ERROR", f"vcpkg install failed: {err}")
        return False


def ensure_all_prerequisites(repo_root: Path) -> bool:
    log("INFO", "=== ENSURING ALL PREREQUISITES ===")
    ok = True
    ok = ensure_vcpkg(repo_root) and ok
    ok = ensure_ninja(repo_root) and ok
    ok = ensure_compiler(repo_root) and ok
    ok = ensure_cmake(repo_root) and ok
    if ok:
        ok = ensure_vcpkg_deps(repo_root) and ok
    ok = ensure_gitignore(repo_root) and ok
    if ok:
        log("OK", "=== ALL PREREQUISITES READY ===")
    else:
        log("ERROR", "=== SOME PREREQUISITES MISSING ===")
    return ok


# =============================================================================
# SYNC OPERATIONS
# =============================================================================

def init_submodules(repo_root: Path, submodules: List[Dict[str, Any]],
                    selected: Optional[List[str]] = None) -> bool:
    log("INFO", "=== INIT SUBMODULES ===")
    targets = [s for s in submodules if selected is None or s["name"] in selected]

    rc, _, _ = run(["git", "submodule", "update", "--init", "--recursive"],
                   repo_root, check=False)
    if rc == 0:
        log("OK", "Submodules initialized")
    else:
        log("ERROR", "Submodule init failed")
        return False

    for sub in targets:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue
        rc2, out2, _ = run(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                          full, check=False, silent=True)
        current = out2.strip()
        if current == "HEAD":
            log("WARN", f"{sub['name']}: Detached HEAD -> checkout {sub['branch']} at meta-repo commit")
            run(["git", "checkout", "-B", sub["branch"]], full, check=False)
        elif current != sub["branch"]:
            log("WARN", f"{sub['name']}: Switching '{current}' -> '{sub['branch']}'")
            run(["git", "checkout", sub["branch"]], full, check=False)
            run(["git", "branch", "--set-upstream-to",
                f"origin/{sub['branch']}", sub["branch"]], full, check=False)
    return True


def github_to_local(repo_root: Path, submodules: List[Dict[str, Any]],
                    selected: Optional[List[str]] = None,
                    auto_stash: bool = False) -> bool:
    log("INFO", "=== GITHUB -> LOCAL ===")

    # Smart conflict handling for meta-repo
    if check_local_changes(repo_root):
        log("WARN", "Meta-repo has local changes!")
        if auto_stash:
            log("INFO", "Auto-stashing meta-repo changes...")
            stash_changes(repo_root, "meta-before-pull")
        else:
            log("ERROR", "Abort: Meta-repo has uncommitted changes. Use --auto-stash or commit manually.")
            return False

    rc, out, _ = run(["git", "pull", "origin", "main"], repo_root, check=False)
    if rc == 0:
        if "Already up to date" in out:
            log("SKIP", "Meta-repo: already up to date")
        else:
            log("OK", "Meta-repo: updated")
    else:
        log("ERROR", "Meta-repo pull failed")
        return False

    log("INFO", "Updating submodules to new meta-repo pointers...")
    rc, out, err = run(
        ["git", "submodule", "update", "--init", "--recursive"],
        repo_root, check=False)
    if rc == 0:
        if "Submodule path" in out:
            log("OK", "Submodules updated to new pointers")
        else:
            log("SKIP", "Submodules already at correct commits")
    else:
        log("ERROR", f"Submodule update failed: {err}")

    targets = [s for s in submodules if selected is None or s["name"] in selected]

    for sub in targets:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            log("WARN", f"{sub['name']}: not initialized")
            continue
        log("INFO", f"--- {sub['name']} ---")

        # Smart conflict handling
        if check_local_changes(full):
            log("WARN", f"  {sub['name']}: local changes detected")
            if auto_stash:
                log("INFO", f"  Auto-stashing...")
                stash_changes(full, sub["name"])
            else:
                log("ERROR", f"  Abort: uncommitted changes. Use --auto-stash or commit manually.")
                continue

        rc2, out2, _ = run(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                          full, check=False, silent=True)
        current = out2.strip()

        if current == "HEAD":
            log("INFO", f"  {sub['name']}: Detached HEAD -> checkout {sub['branch']} at meta-repo commit")
            run(["git", "checkout", "-B", sub["branch"]], full, check=False)
            run(["git", "branch", "--set-upstream-to",
                f"origin/{sub['branch']}", sub["branch"]], full, check=False)
            log("OK", f"  {sub['name']}: on {sub['branch']} at meta-repo commit")
        elif current != sub["branch"]:
            log("WARN", f"  {sub['name']}: Switching '{current}' -> '{sub['branch']}'")
            run(["git", "checkout", sub["branch"]], full, check=False)
            run(["git", "branch", "--set-upstream-to",
                f"origin/{sub['branch']}", sub["branch"]], full, check=False)
            log("OK", f"  {sub['name']}: on {sub['branch']}")
        else:
            log("SKIP", f"  {sub['name']}: already on {sub['branch']} at meta-repo commit")

        # NOTE: Kein 'git pull' hier! GitHub -> Local reproduziert den exakten Stand.

    log("OK", "GitHub -> Local complete. Submodules are at exact meta-repo commits.")
    return True


def local_to_github(repo_root: Path, submodules: List[Dict[str, Any]],
                    selected: Optional[List[str]] = None) -> bool:
    log("INFO", "=== LOCAL -> GITHUB ===")
    targets = [s for s in submodules if selected is None or s["name"] in selected]

    for sub in targets:
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

    for sub in targets:
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

    update_meta_pointer(repo_root, submodules, selected)

    rc, out, _ = run(["git", "status", "--short"], repo_root, check=False, silent=True)
    if out.strip():
        log("WARN", "Meta-repo: committing local changes...")
        run(["git", "add", "-A"], repo_root, check=False)
        run(["git", "commit", "-m",
            f"auto: meta-repo sync {time.strftime('%Y-%m-%d %H:%M:%S')}"],
           repo_root, check=False)
        log("OK", "Meta-repo: committed")

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


def update_meta_pointer(repo_root: Path, submodules: List[Dict[str, Any]],
                        selected: Optional[List[str]] = None) -> None:
    log("INFO", "Checking meta-repo submodule pointers...")
    targets = [s for s in submodules if selected is None or s["name"] in selected]
    changed = []
    for sub in targets:
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


def update_submodules_to_latest(repo_root: Path, submodules: List[Dict[str, Any]],
                                selected: Optional[List[str]] = None,
                                auto_stash: bool = False) -> bool:
    """Explicitly pull all submodules to the latest origin/main commit."""
    log("INFO", "=== UPDATE SUBMODULES TO LATEST ===")
    targets = [s for s in submodules if selected is None or s["name"] in selected]

    for sub in targets:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            log("WARN", f"{sub['name']}: not initialized, skipping")
            continue

        log("INFO", f"--- {sub['name']} ---")

        # Smart conflict handling
        if check_local_changes(full):
            log("WARN", f"  {sub['name']}: local changes detected")
            if auto_stash:
                log("INFO", f"  Auto-stashing...")
                stash_changes(full, sub["name"])
            else:
                log("ERROR", f"  Abort: uncommitted changes. Use --auto-stash or commit manually.")
                continue

        run(["git", "fetch", "origin"], full, check=False, silent=True)

        rc, out, _ = run(["git", "pull", "origin", sub["branch"]], full, check=False)
        if rc == 0:
            if "Already up to date" in out:
                log("SKIP", f"  {sub['name']}: already up to date")
            else:
                log("OK", f"  {sub['name']}: updated to latest")
        else:
            log("ERROR", f"  {sub['name']}: pull failed")

    update_meta_pointer(repo_root, submodules, selected)
    log("OK", "Submodules updated to latest. Meta-repo pointers refreshed.")
    return True


def release_submodules(repo_root: Path, submodules: List[Dict[str, Any]],
                       version: str, selected: Optional[List[str]] = None) -> bool:
    """Tag all submodules with a release version and update meta-repo pointers."""
    log("INFO", f"=== RELEASE SUBMODULES v{version} ===")
    targets = [s for s in submodules if selected is None or s["name"] in selected]

    for sub in targets:
        full = repo_root / sub["path"]
        if not (full / ".git").exists():
            continue
        log("INFO", f"Tagging {sub['name']} with v{version}...")
        rc, _, err = run(["git", "tag", "-a", f"v{version}", "-m", f"Release v{version}"],
                        full, check=False)
        if rc == 0:
            rc2, _, err2 = run(["git", "push", "origin", f"v{version}"], full, check=False)
            if rc2 == 0:
                log("OK", f"  {sub['name']}: tagged and pushed v{version}")
            else:
                log("ERROR", f"  {sub['name']}: push tag failed - {err2}")
        else:
            log("WARN", f"  {sub['name']}: tag may already exist - {err}")

    update_meta_pointer(repo_root, submodules, selected)
    log("OK", f"Release v{version} complete.")
    return True


def full_sync(repo_root: Path, submodules: List[Dict[str, Any]],
              selected: Optional[List[str]] = None,
              auto_stash: bool = False) -> None:
    log("INFO", "=== FULL BIDIRECTIONAL SYNC ===")
    fix_gitmodules(repo_root)
    init_submodules(repo_root, submodules, selected)
    github_to_local(repo_root, submodules, selected, auto_stash)
    local_to_github(repo_root, submodules, selected)
    log("OK", "=== SYNC COMPLETE ===")


def auto_sync(repo_root: Path, submodules: List[Dict[str, Any]],
              interval_min: int, selected: Optional[List[str]] = None,
              auto_stash: bool = False):
    log("INFO", f"Auto-Sync started (every {interval_min} minutes). Press Ctrl+C to stop.")
    try:
        while True:
            full_sync(repo_root, submodules, selected, auto_stash)
            log("INFO", f"Sleeping {interval_min} minutes...")
            time.sleep(interval_min * 60)
    except KeyboardInterrupt:
        log("INFO", "Auto-Sync stopped by user")


# =============================================================================
# BUILD & TEST
# =============================================================================

BUILD_TIMES: Dict[str, List[float]] = {}


def get_os_presets_cli():
    system = platform.system()
    if system == "Windows":
        return ["windows-release", "windows-debug"]
    elif system == "Linux":
        return ["linux-release", "linux-debug"]
    else:
        return ["linux-release", "linux-debug", "windows-release", "windows-debug"]


def get_build_env(repo_root: Path) -> Tuple[Optional[Dict[str, str]], bool]:
    """Return (env_dict, success) for build environment."""
    extra_env = {}
    if platform.system() == "Windows":
        rc, _, _ = run(["cl"], repo_root, check=False, silent=True)
        if rc != 0:
            vcvars_path = find_vcvarsall()
            if vcvars_path:
                vcvars_env = load_vcvars_env(vcvars_path)
                if vcvars_env:
                    extra_env = vcvars_env
                    os.environ.update(vcvars_env)
                else:
                    log("ERROR", "Failed to load MSVC environment")
                    return None, False
            else:
                log("ERROR", "MSVC environment not found. Install VS Build Tools.")
                return None, False
    return extra_env, True


def configure_project(repo_root: Path, preset: str) -> bool:
    log("INFO", f"=== CONFIGURE: {preset} ===")
    if not ensure_all_prerequisites(repo_root):
        log("ERROR", "Prerequisites not met. Configure aborted.")
        return False

    extra_env, ok = get_build_env(repo_root)
    if not ok:
        return False

    env = {**os.environ, **extra_env} if extra_env else None
    rc, out, err = run(["cmake", "--preset", preset], repo_root, env=env, check=False)
    if rc != 0:
        log("ERROR", f"CMake configure failed for preset '{preset}'")
        return False
    log("OK", f"Configure complete: {preset}")
    return True


def build_project(repo_root: Path, preset: str,
                  dependency_aware: bool = False,
                  submodules: Optional[List[Dict[str, Any]]] = None) -> bool:
    log("INFO", f"=== BUILD: {preset} ===")

    if not ensure_all_prerequisites(repo_root):
        log("ERROR", "Prerequisites not met. Build aborted.")
        return False

    extra_env, ok = get_build_env(repo_root)
    if not ok:
        return False

    # Dependency-aware: if seed-core changed, rebuild dependents
    if dependency_aware and submodules:
        log("INFO", "Checking submodule dependencies...")
        # Simple heuristic: if any submodule is ahead of meta-pointer, warn
        for sub in submodules:
            full = repo_root / sub["path"]
            if not (full / ".git").exists():
                continue
            rc, out, _ = run(["git", "log", f"origin/{sub['branch']}..HEAD", "--oneline"],
                            full, silent=True)
            if out.strip():
                log("WARN", f"  {sub['name']}: has unpushed commits. Dependents may need rebuild.")

    build_dir = repo_root / "build" / preset
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure
    env = {**os.environ, **extra_env} if extra_env else None
    rc, out, err = run(["cmake", "--preset", preset], repo_root, env=env, check=False)
    if rc != 0:
        log("ERROR", f"CMake configure failed for preset '{preset}'")
        return False

    # Build with timing
    start = time.time()
    rc, out, err = run(["cmake", "--build", str(build_dir), "--parallel"],
                       repo_root, env=env, check=False, timeout=600)
    elapsed = time.time() - start

    if rc == 0:
        log("OK", f"Build complete: {preset} ({elapsed:.1f}s)")
        # Track build time
        if preset not in BUILD_TIMES:
            BUILD_TIMES[preset] = []
        BUILD_TIMES[preset].append(elapsed)
        avg = sum(BUILD_TIMES[preset]) / len(BUILD_TIMES[preset])
        log("INFO", f"  Build time history: avg={avg:.1f}s, count={len(BUILD_TIMES[preset])}")
        notify("TheSeed Build", f"Build {preset} succeeded in {elapsed:.1f}s")
        return True
    else:
        log("ERROR", f"Build failed: {preset}")
        notify("TheSeed Build", f"Build {preset} FAILED!")
        return False


def run_tests(repo_root: Path, preset: str, test_filter: str) -> bool:
    log("INFO", f"=== TEST: {test_filter} (preset: {preset}) ===")
    build_dir = repo_root / "build" / preset.replace("-", "_")
    if not build_dir.exists():
        log("WARN", f"Build dir not found: {build_dir}. Run --build first.")
        return False
    regex = ".*" if test_filter == "all" else test_filter
    rc, out, err = run(
        ["ctest", "--test-dir", str(build_dir), "-R", regex,
         "--output-on-failure", "-j", str(os.cpu_count() or 4)],
        repo_root, check=False, timeout=300)
    if rc == 0:
        log("OK", f"Tests passed: {test_filter}")
        return True
    else:
        log("ERROR", f"Tests failed: {test_filter}")
        return False


# =============================================================================
# MAIN
# =============================================================================

def main():
    os_presets = get_os_presets_cli()
    default_preset = "windows-release" if platform.system() == "Windows" else "linux-release"

    parser = argparse.ArgumentParser(description="TheSeed Submodule Sync Tool")
    parser.add_argument("--fix-gitmodules", action="store_true", help="Add branch = main")
    parser.add_argument("--init", action="store_true", help="Initialize submodules")
    parser.add_argument("--github-to-local", action="store_true", help="Pull from GitHub")
    parser.add_argument("--local-to-github", action="store_true", help="Push to GitHub")
    parser.add_argument("--update-submodules", action="store_true",
                        help="Pull submodules to latest origin/main and update meta pointers")
    parser.add_argument("--full", action="store_true", help="Full bidirectional sync")
    parser.add_argument("--auto-sync", action="store_true", help="Run sync in loop")
    parser.add_argument("--interval", type=int, default=5, help="Auto-sync interval (min)")
    parser.add_argument("--gui", action="store_true", help="Launch GUI")
    parser.add_argument("--build", action="store_true", help="Build project")
    parser.add_argument("--configure-only", action="store_true", help="Only configure, don't build")
    parser.add_argument("--preset", default=default_preset, choices=os_presets,
                        help=f"CMake preset (default: {default_preset})")
    parser.add_argument("--test", action="store_true", help="Run tests")
    parser.add_argument("--test-filter", default="all",
                        choices=["all", "unit", "integration", "property", "fuzz", "benchmark",
                                 "gate_p0", "gate_p1", "gate_p2", "gate_p3",
                                 "gate_p4", "gate_p5", "gate_p6", "gate_p7"],
                        help="Test filter (default: all)")
    parser.add_argument("--selective", nargs="+", metavar="SUBMODULE",
                        help="Only operate on specific submodules (e.g. --selective seed-core seed-renderer)")
    parser.add_argument("--auto-stash", action="store_true",
                        help="Automatically stash local changes before sync operations")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show what would be done without executing")
    parser.add_argument("--health-check", action="store_true", help="Run health diagnostics")
    parser.add_argument("--clean", action="store_true", help="Clean build artifacts")
    parser.add_argument("--release", action="store_true", help="Tag submodules with release version")
    parser.add_argument("--version", default="", help="Version string for --release (e.g. 1.2.3)")
    parser.add_argument("--export-log", action="store_true", help="Export session log to file")
    parser.add_argument("--dependency-aware", action="store_true",
                        help="Warn about submodule changes that may require dependent rebuilds")

    args = parser.parse_args()

    global DRY_RUN
    DRY_RUN = args.dry_run
    if DRY_RUN:
        log("WARN", "=== DRY-RUN MODE: No commands will be executed ===")

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

    selected = args.selective

    if args.health_check:
        health_check(repo_root, submodules)
    if args.fix_gitmodules:
        fix_gitmodules(repo_root)
    if args.init:
        init_submodules(repo_root, submodules, selected)
    if args.github_to_local:
        github_to_local(repo_root, submodules, selected, args.auto_stash)
    if args.update_submodules:
        update_submodules_to_latest(repo_root, submodules, selected, args.auto_stash)
    if args.local_to_github:
        local_to_github(repo_root, submodules, selected)
    if args.full:
        full_sync(repo_root, submodules, selected, args.auto_stash)
    if args.auto_sync:
        auto_sync(repo_root, submodules, args.interval, selected, args.auto_stash)
    if args.configure_only:
        configure_project(repo_root, args.preset)
    if args.build:
        build_project(repo_root, args.preset, args.dependency_aware, submodules)
    if args.test:
        run_tests(repo_root, args.preset, args.test_filter)
    if args.clean:
        auto_cleanup(repo_root, args.preset if args.preset else None)
    if args.release:
        if not args.version:
            log("ERROR", "--release requires --version (e.g. --version 1.2.3)")
            sys.exit(1)
        release_submodules(repo_root, submodules, args.version, selected)
    if args.export_log:
        export_log(repo_root / f"theseed_sync_log_{time.strftime('%Y%m%d_%H%M%S')}.txt")


if __name__ == "__main__":
    main()
