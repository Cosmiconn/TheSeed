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
"""

import argparse
import configparser
import os
import platform
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
    INSTALL = "\033[96m"
    RESET = "\033[0m"


def log(level: str, msg: str) -> None:
    color = getattr(Colors, level, Colors.INFO)
    timestamp = time.strftime("%H:%M:%S")
    print(f"{color}[{timestamp}] [{level}]{Colors.RESET} {msg}")


def run(cmd: List[str], cwd: Path, check: bool = False, silent: bool = False, timeout: int = 60) -> Tuple[int, str, str]:
    if not silent:
        log("CMD", f"{' '.join(cmd)}  (in {cwd})")
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False, timeout=timeout)
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
    rc, out, _ = run(["git", "pull", "origin", "main"], repo_root, check=False)
    if rc == 0:
        if "Already up to date" in out:
            log("SKIP", "Meta-repo: already up to date")
        else:
            log("OK", "Meta-repo: updated")
    else:
        log("ERROR", "Meta-repo pull failed")
        return False

    # CRITICAL: Update submodules to the new pointers from meta-repo
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
    update_meta_pointer(repo_root, submodules)
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


# =============================================================================
# AUTO-INSTALL PREREQUISITES
# =============================================================================

def ensure_vcpkg(repo_root: Path) -> bool:
    log("INFO", "Checking vcpkg...")
    vcpkg_env = os.environ.get("VCPKG_ROOT", "")
    vcpkg_local = repo_root / "vcpkg"
    vcpkg_env_path = Path(vcpkg_env) / "scripts" / "buildsystems" / "vcpkg.cmake" if vcpkg_env else None

    if vcpkg_env_path and vcpkg_env_path.exists():
        log("OK", f"vcpkg found via VCPKG_ROOT: {vcpkg_env}")
        return True
    if (vcpkg_local / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
        log("OK", f"vcpkg found locally: {vcpkg_local}")
        os.environ["VCPKG_ROOT"] = str(vcpkg_local)
        return True

    log("INSTALL", "vcpkg not found. Auto-installing...")
    rc, out, err = run(
        ["git", "clone", "https://github.com/Microsoft/vcpkg.git", str(vcpkg_local)],
        repo_root, check=False, timeout=120)
    if rc != 0:
        log("ERROR", f"Failed to clone vcpkg: {err}")
        return False

    if platform.system() == "Windows":
        bootstrap = vcpkg_local / "bootstrap-vcpkg.bat"
        rc, out, err = run([str(bootstrap)], repo_root, check=False, timeout=120)
    else:
        bootstrap = vcpkg_local / "bootstrap-vcpkg.sh"
        rc, out, err = run(["bash", str(bootstrap)], repo_root, check=False, timeout=120)

    if rc != 0:
        log("ERROR", f"vcpkg bootstrap failed: {err}")
        return False

    os.environ["VCPKG_ROOT"] = str(vcpkg_local)
    log("OK", f"vcpkg installed at: {vcpkg_local}")
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


def find_vcvarsall() -> Path:
    """Find vcvarsall.bat for MSVC on Windows. Searches 2022, 2026, etc."""
    log("INFO", "Searching for vcvarsall.bat...")
    program_files = os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
    program_files_64 = os.environ.get("ProgramFiles", "C:\\Program Files")
    editions = ["Community", "Professional", "Enterprise", "BuildTools"]
    years = ["2026", "18", "2025", "2024", "2022", "2019"]
    for pf in [program_files_64, program_files]:
        for year in years:
            for edition in editions:
                vcvars = Path(pf) / "Microsoft Visual Studio" / year / edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
                log("INFO", f"  Checking: {vcvars}")
                if vcvars.exists():
                    log("OK", f"  Found: {vcvars}")
                    return vcvars
    log("WARN", "  vcvarsall.bat not found in any standard location")
    return None


def load_vcvars_env(vcvars_path: Path) -> dict:
    """Run vcvarsall.bat and extract environment variables."""
    import tempfile
    log("INFO", f"Loading MSVC environment from: {vcvars_path}")
    env_file = tempfile.mktemp(suffix=".txt")
    try:
        # Build command as raw string for shell execution
        cmd_str = f'"{vcvars_path}" x64 && set > "{env_file}"'
        log("CMD", f"cmd /c {cmd_str}")
        result = subprocess.run(
            cmd_str,
            shell=True,
            capture_output=True, text=True, check=False, timeout=60)
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
        except:
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
        log("WARN", "  MSVC requires Visual Studio Build Tools.")
        log("WARN", "  Download: https://visualstudio.microsoft.com/downloads/")
        log("WARN", "  Select: 'Desktop development with C++' workload")
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
        log("WARN", "  Fix: sudo apt install build-essential")
        return False


def ensure_cmake(repo_root: Path) -> bool:
    log("INFO", "Checking CMake...")
    rc, out, _ = run(["cmake", "--version"], repo_root, check=False, silent=True)
    if rc == 0:
        version_line = out.strip().splitlines()[0] if out.strip() else "unknown"
        log("OK", f"CMake found: {version_line}")
        return True
    log("ERROR", "CMake NOT FOUND")
    log("WARN", "  Download: https://cmake.org/download/")
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
    if ok:
        log("OK", "=== ALL PREREQUISITES READY ===")
    else:
        log("ERROR", "=== SOME PREREQUISITES MISSING ===")
    return ok


# =============================================================================
# BUILD & TEST
# =============================================================================

def get_os_presets_cli():
    system = platform.system()
    if system == "Windows":
        return ["windows-release", "windows-debug"]
    elif system == "Linux":
        return ["linux-release", "linux-debug"]
    else:
        return ["linux-release", "linux-debug", "windows-release", "windows-debug"]


def build_project(repo_root: Path, preset: str) -> bool:
    log("INFO", f"=== BUILD: {preset} ===")

    if not ensure_all_prerequisites(repo_root):
        log("ERROR", "Prerequisites not met. Build aborted.")
        return False

    # On Windows: if not in Developer Prompt, load vcvars env
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
                    return False
            else:
                log("ERROR", "MSVC environment not found. Install VS Build Tools.")
                return False

    # Configure
    env = {**os.environ, **extra_env} if extra_env else None
    rc, out, err = run_env(["cmake", "--preset", preset], repo_root, env=env, check=False)
    if rc != 0:
        log("ERROR", f"CMake configure failed for preset '{preset}'")
        return False

    # Build
    build_dir = repo_root / "build" / preset.replace("-", "_")
    rc, out, err = run_env(["cmake", "--build", str(build_dir), "--parallel"],
                           repo_root, env=env, check=False, timeout=600)
    if rc == 0:
        log("OK", f"Build complete: {preset}")
        return True
    else:
        log("ERROR", f"Build failed: {preset}")
        return False


def run_env(cmd: List[str], cwd: Path, env=None, check: bool = False, silent: bool = False, timeout: int = 60) -> Tuple[int, str, str]:
    """Run command with optional extra environment variables."""
    if not silent:
        log("CMD", f"{' '.join(cmd)}  (in {cwd})")
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False, timeout=timeout, env=env)
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


def main():
    os_presets = get_os_presets_cli()
    default_preset = "windows-release" if platform.system() == "Windows" else "linux-release"

    parser = argparse.ArgumentParser(description="TheSeed Submodule Sync Tool")
    parser.add_argument("--fix-gitmodules", action="store_true", help="Add branch = main")
    parser.add_argument("--init", action="store_true", help="Initialize submodules")
    parser.add_argument("--github-to-local", action="store_true", help="Pull from GitHub")
    parser.add_argument("--local-to-github", action="store_true", help="Push to GitHub")
    parser.add_argument("--full", action="store_true", help="Full bidirectional sync")
    parser.add_argument("--auto-sync", action="store_true", help="Run sync in loop")
    parser.add_argument("--interval", type=int, default=5, help="Auto-sync interval (min)")
    parser.add_argument("--gui", action="store_true", help="Launch GUI")
    parser.add_argument("--build", action="store_true", help="Build project")
    parser.add_argument("--preset", default=default_preset, choices=os_presets,
                        help=f"CMake preset (default: {default_preset})")
    parser.add_argument("--test", action="store_true", help="Run tests")
    parser.add_argument("--test-filter", default="all",
                        choices=["all", "unit", "integration", "property", "fuzz", "benchmark",
                                 "gate_p0", "gate_p1", "gate_p2", "gate_p3",
                                 "gate_p4", "gate_p5", "gate_p6", "gate_p7"],
                        help="Test filter (default: all)")

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
    if args.build:
        build_project(repo_root, args.preset)
    if args.test:
        run_tests(repo_root, args.preset, args.test_filter)


if __name__ == "__main__":
    main()
