#!/usr/bin/env python3
"""
TheSeed Submodule Sync Tool - GUI Version
==========================================
Complete bidirectional sync between GitHub and local repository.
Build & Test integration with AUTO-INSTALL of missing dependencies.
Roadmap conform.

Usage:
    python theseed_sync_gui.py
"""

import configparser
import os
import platform
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path
from tkinter import (
    Tk, Frame, Button, Label, Text, Scrollbar, END, DISABLED, NORMAL,
    messagebox, ttk, StringVar, IntVar, Checkbutton
)


def get_os_presets():
    """Return presets valid for the current OS."""
    system = platform.system()
    if system == "Windows":
        return ["windows-release", "windows-debug"]
    elif system == "Linux":
        return ["linux-release", "linux-debug"]
    else:
        return ["linux-release", "linux-debug", "windows-release", "windows-debug"]


def get_default_preset():
    """Return the default preset for the current OS."""
    system = platform.system()
    if system == "Windows":
        return "windows-release"
    else:
        return "linux-release"


class SubmoduleSyncGUI:
    def __init__(self, root: Tk):
        self.root = root
        self.root.title("TheSeed Sync - GitHub <> Local")
        self.root.geometry("1000x800")
        self.root.minsize(800, 600)

        self.repo_root = self._find_repo_root()
        self.submodules = []
        self.auto_sync_active = False
        self.auto_sync_thread = None
        self.os_presets = get_os_presets()
        self.default_preset = get_default_preset()

        self._build_ui()
        self._detect_submodules()

    def _find_repo_root(self) -> Path:
        current = Path.cwd().resolve()
        for p in [current] + list(current.parents):
            if (p / ".git").exists() and (p / ".gitmodules").exists():
                return p
        for p in [current] + list(current.parents):
            if p.name.lower() == "these" or (p / "submodules").exists():
                if (p / ".git").exists():
                    return p
        return current

    def _build_ui(self):
        # === TOP BAR ===
        top = Frame(self.root, bg="#1a1a2e", padx=10, pady=8)
        top.pack(fill="x")

        Label(top, text="TheSeed", font=("Segoe UI", 14, "bold"),
              fg="#e94560", bg="#1a1a2e").pack(side="left")
        Label(top, text="Submodule Sync", font=("Segoe UI", 10),
              fg="#a0a0a0", bg="#1a1a2e").pack(side="left", padx=(5, 20))

        Label(top, text="Repo:", font=("Segoe UI", 9),
              fg="#a0a0a0", bg="#1a1a2e").pack(side="left")
        self.lbl_root = Label(top, text=str(self.repo_root),
              font=("Consolas", 9), fg="#4cc9f0", bg="#1a1a2e")
        self.lbl_root.pack(side="left", padx=(5, 0))

        # === SYNC BUTTON BAR ===
        btn_bar = Frame(self.root, bg="#16213e", padx=10, pady=6)
        btn_bar.pack(fill="x")

        btn_cfg = {
            "font": ("Segoe UI", 9, "bold"),
            "padx": 12, "pady": 4,
            "bd": 0, "cursor": "hand2"
        }

        Button(btn_bar, text="Fix .gitmodules", bg="#ff9f1c", fg="black",
               command=self.on_fix_gitmodules, **btn_cfg).pack(side="left", padx=4)

        Button(btn_bar, text="Init Submodules", bg="#2ec4b6", fg="black",
               command=self.on_init, **btn_cfg).pack(side="left", padx=4)

        Button(btn_bar, text="GitHub -> Local", bg="#3a86ff", fg="white",
               command=self.on_github_to_local, **btn_cfg).pack(side="left", padx=4)

        Button(btn_bar, text="Local -> GitHub", bg="#8338ec", fg="white",
               command=self.on_local_to_github, **btn_cfg).pack(side="left", padx=4)

        Button(btn_bar, text="Full Sync", bg="#06d6a0", fg="black",
               command=self.on_full_sync, **btn_cfg).pack(side="left", padx=4)

        Button(btn_bar, text="Refresh", bg="#6c757d", fg="white",
               command=self.on_refresh, **btn_cfg).pack(side="left", padx=4)

        # === AUTO-SYNC CONTROLS ===
        auto_frame = Frame(self.root, bg="#16213e", padx=10, pady=4)
        auto_frame.pack(fill="x")

        self.auto_var = IntVar(value=0)
        self.chk_auto = Checkbutton(auto_frame, text="Auto-Sync every",
            variable=self.auto_var, bg="#16213e", fg="white",
            selectcolor="#1a1a2e", activebackground="#16213e",
            activeforeground="white", command=self.on_auto_toggle)
        self.chk_auto.pack(side="left")

        self.interval_var = StringVar(value="5")
        self.cmb_interval = ttk.Combobox(auto_frame, textvariable=self.interval_var,
            values=["1", "5", "10", "15", "30", "60"], width=4, state="readonly")
        self.cmb_interval.pack(side="left", padx=(2, 2))
        Label(auto_frame, text="min", bg="#16213e", fg="white").pack(side="left")

        self.lbl_status = Label(auto_frame, text="Idle", bg="#16213e",
            fg="#888888", font=("Segoe UI", 9, "italic"))
        self.lbl_status.pack(side="right", padx=10)

        # === BUILD & TEST CONTROLS ===
        bt_frame = Frame(self.root, bg="#16213e", padx=10, pady=6)
        bt_frame.pack(fill="x")

        Label(bt_frame, text="Build:", bg="#16213e", fg="#a0a0a0",
              font=("Segoe UI", 9, "bold")).pack(side="left")
        self.preset_var = StringVar(value=self.default_preset)
        self.cmb_preset = ttk.Combobox(bt_frame, textvariable=self.preset_var,
            values=self.os_presets, width=22, state="readonly")
        self.cmb_preset.pack(side="left", padx=(4, 12))

        Button(bt_frame, text="Build", bg="#f72585", fg="white",
               command=self.on_build, font=("Segoe UI", 9, "bold"),
               padx=14, pady=3, bd=0, cursor="hand2").pack(side="left", padx=2)

        Label(bt_frame, text="Test:", bg="#16213e", fg="#a0a0a0",
              font=("Segoe UI", 9, "bold")).pack(side="left", padx=(20, 0))
        self.test_var = StringVar(value="all")
        self.cmb_test = ttk.Combobox(bt_frame, textvariable=self.test_var,
            values=["all", "unit", "integration", "property", "fuzz", "benchmark",
                    "gate_p0", "gate_p1", "gate_p2", "gate_p3",
                    "gate_p4", "gate_p5", "gate_p6", "gate_p7"],
            width=18, state="readonly")
        self.cmb_test.pack(side="left", padx=(4, 12))

        Button(bt_frame, text="Run Tests", bg="#7209b7", fg="white",
               command=self.on_test, font=("Segoe UI", 9, "bold"),
               padx=14, pady=3, bd=0, cursor="hand2").pack(side="left", padx=2)

        # === SUBMODULE TABLE ===
        table_frame = Frame(self.root, padx=10, pady=5)
        table_frame.pack(fill="both", expand=True)

        columns = ("name", "path", "branch", "ahead", "status")
        self.tree = ttk.Treeview(table_frame, columns=columns, show="headings", height=8)

        self.tree.heading("name", text="Submodule")
        self.tree.heading("path", text="Path")
        self.tree.heading("branch", text="Branch")
        self.tree.heading("ahead", text="Ahead/Behind")
        self.tree.heading("status", text="Status")

        self.tree.column("name", width=140, anchor="w")
        self.tree.column("path", width=220, anchor="w")
        self.tree.column("branch", width=100, anchor="center")
        self.tree.column("ahead", width=120, anchor="center")
        self.tree.column("status", width=180, anchor="w")

        vsb = Scrollbar(table_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=vsb.set)

        self.tree.pack(side="left", fill="both", expand=True)
        vsb.pack(side="right", fill="y")

        # Style the treeview
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("Treeview", background="#0f0f1a", foreground="#e0e0e0",
                       fieldbackground="#0f0f1a", rowheight=26)
        style.configure("Treeview.Heading", background="#1a1a2e", foreground="#e94560",
                       font=("Segoe UI", 9, "bold"))
        style.map("Treeview", background=[("selected", "#e94560")])

        # === LOG AREA ===
        log_frame = Frame(self.root, padx=10, pady=5)
        log_frame.pack(fill="both", expand=True)

        Label(log_frame, text="Log", font=("Segoe UI", 9, "bold"),
              fg="#e94560").pack(anchor="w")

        self.txt_log = Text(log_frame, height=12, wrap="word",
                           font=("Consolas", 9), bg="#0a0a14", fg="#e0e0e0",
                           insertbackground="white", bd=1, relief="solid")
        self.txt_log.pack(side="left", fill="both", expand=True)

        sb = Scrollbar(log_frame, command=self.txt_log.yview)
        self.txt_log.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")

        # Tag colors for log
        self.txt_log.tag_config("OK", foreground="#00ff88")
        self.txt_log.tag_config("WARN", foreground="#ffaa00")
        self.txt_log.tag_config("ERROR", foreground="#ff4444")
        self.txt_log.tag_config("INFO", foreground="#44aaff")
        self.txt_log.tag_config("SKIP", foreground="#888888")
        self.txt_log.tag_config("CMD", foreground="#cc88ff")
        self.txt_log.tag_config("INSTALL", foreground="#00ffff")

        # === PROGRESS BAR ===
        self.progress = ttk.Progressbar(self.root, mode="indeterminate")
        self.progress.pack(fill="x", padx=10, pady=5)

        # Set overall theme
        self.root.configure(bg="#0f0f1a")

    def log(self, level: str, msg: str):
        timestamp = time.strftime("%H:%M:%S")
        self.txt_log.insert(END, f"[{timestamp}] ", "SKIP")
        self.txt_log.insert(END, f"[{level}] ", level)
        self.txt_log.insert(END, f"{msg}\n")
        self.txt_log.see(END)
        self.txt_log.update()

    def _run(self, cmd, cwd, check=False, silent=False, timeout=60) -> tuple:
        if not silent:
            self.log("CMD", f"{' '.join(cmd)}  (in {cwd})")
        try:
            result = subprocess.run(cmd, cwd=cwd, capture_output=True,
                                   text=True, check=False, timeout=timeout)
            if result.stdout and not silent:
                for line in result.stdout.strip().splitlines():
                    self.log("INFO", f"  > {line}")
            if result.stderr and not silent:
                for line in result.stderr.strip().splitlines():
                    if "warning" in line.lower():
                        self.log("WARN", f"  ! {line}")
                    else:
                        self.log("INFO", f"  ! {line}")
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            self.log("ERROR", f"  X Command timed out after {timeout}s")
            return 1, "", "timeout"
        except Exception as e:
            self.log("ERROR", f"  X Exception: {e}")
            return 1, "", str(e)

    def _detect_submodules(self):
        self.submodules = []
        if not self.repo_root:
            return
        gm = self.repo_root / ".gitmodules"
        if not gm.exists():
            self.log("WARN", ".gitmodules not found")
            return

        config = configparser.ConfigParser()
        config.read(gm)

        for section in config.sections():
            if section.startswith("submodule"):
                name = section.replace("submodule ", "").strip('"')
                path = config.get(section, "path", fallback="")
                url = config.get(section, "url", fallback="")
                branch = config.get(section, "branch", fallback="main")
                self.submodules.append({
                    "name": name, "path": path, "url": url, "branch": branch
                })

        self._refresh_table()

    def _refresh_table(self):
        for item in self.tree.get_children():
            self.tree.delete(item)

        for sub in self.submodules:
            full = self.repo_root / sub["path"] if self.repo_root else Path(sub["path"])

            if not (full / ".git").exists():
                self.tree.insert("", END, values=(
                    sub["name"], sub["path"], "N/A", "N/A", "Not initialized"
                ), tags=("notinit",))
                continue

            rc, out, _ = self._run(["git", "rev-parse", "--abbrev-ref", "HEAD"],
                                    full, check=False, silent=True)
            current_branch = out.strip() if rc == 0 else "error"

            ahead_behind = "?"
            if current_branch != "HEAD" and current_branch != "error":
                rc2, out2, _ = self._run(
                    ["git", "rev-list", "--left-right",
                     f"origin/{sub['branch']}...{current_branch}", "--count"],
                    full, check=False, silent=True)
                if rc2 == 0:
                    parts = out2.strip().split()
                    if len(parts) == 2:
                        behind, ahead = parts
                        parts_list = []
                        if int(ahead) > 0:
                            parts_list.append(f"+{ahead}")
                        if int(behind) > 0:
                            parts_list.append(f"-{behind}")
                        ahead_behind = ", ".join(parts_list) if parts_list else "synced"

            rc3, out3, _ = self._run(["git", "status", "--short"], full, check=False, silent=True)
            if out3.strip():
                status = f"Modified ({len(out3.strip().splitlines())} files)"
            elif current_branch == "HEAD":
                status = "Detached HEAD"
            elif current_branch != sub["branch"]:
                status = f"Wrong branch ({current_branch})"
            else:
                status = "Clean"

            tag = "clean" if status == "Clean" and ahead_behind == "synced" else "dirty"
            self.tree.insert("", END, values=(
                sub["name"], sub["path"], current_branch, ahead_behind, status
            ), tags=(tag,))

        self.tree.tag_configure("clean", background="#0a1a0a")
        self.tree.tag_configure("dirty", background="#1a0a0a")
        self.tree.tag_configure("notinit", background="#1a1a0a")

    def _set_busy(self, busy: bool):
        state = DISABLED if busy else NORMAL
        for child in self.root.winfo_children():
            if isinstance(child, Frame):
                for c in child.winfo_children():
                    if isinstance(c, Button):
                        c.configure(state=state)
                    elif isinstance(c, ttk.Combobox):
                        c.configure(state="disabled" if busy else "readonly")
        if busy:
            self.progress.start()
            self.lbl_status.configure(text="Working...", fg="#ff9f1c")
        else:
            self.progress.stop()
            self.lbl_status.configure(text="Idle", fg="#888888")

    def _thread(self, target):
        self._set_busy(True)
        def wrapper():
            try:
                target()
            finally:
                self._set_busy(False)
                self._refresh_table()
        threading.Thread(target=wrapper, daemon=True).start()

    # ============ AUTO-INSTALL HELPERS ============

    def _ensure_vcpkg(self) -> bool:
        """Ensure vcpkg is available. Auto-install if missing."""
        self.log("INFO", "Checking vcpkg...")
        vcpkg_env = os.environ.get("VCPKG_ROOT", "")
        vcpkg_local = self.repo_root / "vcpkg"
        vcpkg_env_path = Path(vcpkg_env) / "scripts" / "buildsystems" / "vcpkg.cmake" if vcpkg_env else None

        # Already available?
        if vcpkg_env_path and vcpkg_env_path.exists():
            self.log("OK", f"vcpkg found via VCPKG_ROOT: {vcpkg_env}")
            return True
        if (vcpkg_local / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            self.log("OK", f"vcpkg found locally: {vcpkg_local}")
            os.environ["VCPKG_ROOT"] = str(vcpkg_local)
            return True

        # Auto-install
        self.log("INSTALL", "vcpkg not found. Auto-installing...")
        rc, out, err = self._run(
            ["git", "clone", "https://github.com/Microsoft/vcpkg.git", str(vcpkg_local)],
            self.repo_root, check=False, timeout=120)
        if rc != 0:
            self.log("ERROR", f"Failed to clone vcpkg: {err}")
            return False

        if platform.system() == "Windows":
            bootstrap = vcpkg_local / "bootstrap-vcpkg.bat"
            rc, out, err = self._run([str(bootstrap)], self.repo_root, check=False, timeout=120)
        else:
            bootstrap = vcpkg_local / "bootstrap-vcpkg.sh"
            rc, out, err = self._run(["bash", str(bootstrap)], self.repo_root, check=False, timeout=120)

        if rc != 0:
            self.log("ERROR", f"vcpkg bootstrap failed: {err}")
            return False

        os.environ["VCPKG_ROOT"] = str(vcpkg_local)
        self.log("OK", f"vcpkg installed at: {vcpkg_local}")
        return True

    def _ensure_ninja(self) -> bool:
        """Ensure Ninja is available. Auto-install if missing."""
        self.log("INFO", "Checking Ninja...")
        rc, _, _ = self._run(["ninja", "--version"], self.repo_root, check=False, silent=True)
        if rc == 0:
            self.log("OK", "Ninja found")
            return True

        self.log("INSTALL", "Ninja not found. Auto-installing via pip...")
        rc, out, err = self._run(
            [sys.executable, "-m", "pip", "install", "ninja"],
            self.repo_root, check=False, timeout=120)
        if rc != 0:
            self.log("ERROR", f"Failed to install Ninja: {err}")
            return False

        # Verify
        rc, out, _ = self._run(["ninja", "--version"], self.repo_root, check=False, silent=True)
        if rc == 0:
            self.log("OK", "Ninja installed successfully")
            return True
        else:
            self.log("ERROR", "Ninja installed but not in PATH. Restart the tool.")
            return False

    def _find_vcvarsall(self) -> Path:
        """Find vcvarsall.bat for MSVC on Windows. Searches 2022, 2026, etc."""
        self.log("INFO", "Searching for vcvarsall.bat...")
        program_files = os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)")
        program_files_64 = os.environ.get("ProgramFiles", "C:\\Program Files")

        editions = ["Community", "Professional", "Enterprise", "BuildTools"]
        years = ["2026", "18", "2025", "2024", "2022", "2019"]

        for pf in [program_files_64, program_files]:
            for year in years:
                for edition in editions:
                    vcvars = Path(pf) / "Microsoft Visual Studio" / year / edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
                    self.log("INFO", f"  Checking: {vcvars}")
                    if vcvars.exists():
                        self.log("OK", f"  Found: {vcvars}")
                        return vcvars
        self.log("WARN", "  vcvarsall.bat not found in any standard location")
        return None

    def _load_vcvars_env(self, vcvars_path: Path) -> dict:
        """Run vcvarsall.bat and extract environment variables."""
        import tempfile
        self.log("INFO", f"Loading MSVC environment from: {vcvars_path}")
        env_file = tempfile.mktemp(suffix=".txt")
        try:
            # Build command as raw string for shell execution
            cmd_str = f'"{vcvars_path}" x64 && set > "{env_file}"'
            self.log("CMD", f"cmd /c {cmd_str}")
            result = subprocess.run(
                cmd_str,
                shell=True,
                capture_output=True, text=True, check=False, timeout=60)
            if result.returncode != 0:
                self.log("ERROR", f"vcvarsall.bat failed: {result.stderr}")
                return {}

            # Parse environment file
            env = {}
            if not Path(env_file).exists():
                self.log("ERROR", "Environment file not created")
                return {}

            with open(env_file, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    line = line.strip()
                    if "=" in line:
                        key, value = line.split("=", 1)
                        env[key] = value

            self.log("OK", f"Loaded {len(env)} environment variables from vcvarsall.bat")
            return env
        except Exception as e:
            self.log("ERROR", f"Failed to load vcvars env: {e}")
            return {}
        finally:
            try:
                Path(env_file).unlink(missing_ok=True)
            except:
                pass

    def _ensure_compiler(self) -> bool:
        """Check compiler. On Windows, auto-detects vcvarsall.bat if not in Developer Prompt."""
        self.log("INFO", "Checking C++ compiler...")
        system = platform.system()

        if system == "Windows":
            # Already in Developer Prompt?
            rc, _, _ = self._run(["cl"], self.repo_root, check=False, silent=True)
            if rc == 0:
                self.log("OK", "MSVC (cl) found in PATH")
                return True

            # Try to find vcvarsall.bat
            vcvars = self._find_vcvarsall()
            if vcvars:
                self.log("OK", f"MSVC found via vcvarsall.bat: {vcvars}")
                return True

            rc2, _, _ = self._run(["g++", "--version"], self.repo_root, check=False, silent=True)
            if rc2 == 0:
                self.log("OK", "GCC (g++) found")
                return True

            self.log("ERROR", "C++ Compiler NOT FOUND")
            self.log("WARN", "  MSVC requires Visual Studio Build Tools.")
            self.log("WARN", "  Download: https://visualstudio.microsoft.com/downloads/")
            self.log("WARN", "  Select: 'Desktop development with C++' workload")
            return False
        else:
            rc, _, _ = self._run(["g++", "--version"], self.repo_root, check=False, silent=True)
            if rc == 0:
                self.log("OK", "GCC (g++) found")
                return True
            rc2, _, _ = self._run(["clang++", "--version"], self.repo_root, check=False, silent=True)
            if rc2 == 0:
                self.log("OK", "Clang (clang++) found")
                return True
            self.log("ERROR", "C++ Compiler NOT FOUND")
            self.log("WARN", "  Fix: sudo apt install build-essential")
            return False

    def _ensure_cmake(self) -> bool:
        """Ensure CMake is available."""
        self.log("INFO", "Checking CMake...")
        rc, out, _ = self._run(["cmake", "--version"], self.repo_root, check=False, silent=True)
        if rc == 0:
            version_line = out.strip().splitlines()[0] if out.strip() else "unknown"
            self.log("OK", f"CMake found: {version_line}")
            return True
        self.log("ERROR", "CMake NOT FOUND")
        self.log("WARN", "  Download: https://cmake.org/download/")
        return False

    def _ensure_vcpkg_deps(self) -> bool:
        """Install vcpkg dependencies from vcpkg.json."""
        self.log("INFO", "Checking vcpkg dependencies...")
        vcpkg_json = self.repo_root / "vcpkg.json"
        if not vcpkg_json.exists():
            self.log("SKIP", "vcpkg.json not found, skipping dependency install")
            return True

        vcpkg_root = os.environ.get("VCPKG_ROOT", "")
        if not vcpkg_root:
            self.log("ERROR", "VCPKG_ROOT not set after vcpkg install")
            return False

        vcpkg_exe = Path(vcpkg_root) / "vcpkg.exe" if platform.system() == "Windows" else Path(vcpkg_root) / "vcpkg"
        if not vcpkg_exe.exists():
            self.log("ERROR", f"vcpkg executable not found: {vcpkg_exe}")
            return False

        triplet = "x64-windows" if platform.system() == "Windows" else "x64-linux"
        self.log("INSTALL", f"Installing vcpkg dependencies for {triplet}...")
        rc, out, err = self._run(
            [str(vcpkg_exe), "install", f"--triplet={triplet}"],
            self.repo_root, check=False, timeout=600)
        if rc == 0:
            self.log("OK", "vcpkg dependencies installed")
            return True
        else:
            self.log("ERROR", f"vcpkg install failed: {err}")
            return False

    def _ensure_all_prerequisites(self) -> bool:
        """Run all prerequisite checks with auto-install."""
        self.log("INFO", "=== ENSURING ALL PREREQUISITES ===")
        ok = True
        ok = self._ensure_vcpkg() and ok
        ok = self._ensure_ninja() and ok
        ok = self._ensure_compiler() and ok
        ok = self._ensure_cmake() and ok
        if ok:
            ok = self._ensure_vcpkg_deps() and ok
        if ok:
            self.log("OK", "=== ALL PREREQUISITES READY ===")
        else:
            self.log("ERROR", "=== SOME PREREQUISITES MISSING ===")
        return ok

    # ============ ACTIONS ============

    def on_fix_gitmodules(self):
        self._thread(self._fix_gitmodules)

    def _fix_gitmodules(self):
        self.log("INFO", "=== FIX .GITMODULES ===")
        gm = self.repo_root / ".gitmodules"
        if not gm.exists():
            self.log("ERROR", ".gitmodules not found!")
            return

        config = configparser.ConfigParser()
        config.read(gm)
        changed = False

        for section in config.sections():
            if not section.startswith("submodule"):
                continue
            name = section.replace("submodule ", "").strip('"')
            if not config.has_option(section, "branch"):
                config.set(section, "branch", "main")
                self.log("WARN", f"  Added 'branch = main' to {name}")
                changed = True
            else:
                branch = config.get(section, "branch")
                if branch != "main":
                    config.set(section, "branch", "main")
                    self.log("WARN", f"  Changed branch from '{branch}' to 'main' in {name}")
                    changed = True

        if changed:
            with open(gm, "w") as f:
                config.write(f)
            self.log("OK", ".gitmodules updated")
            self._run(["git", "add", ".gitmodules"], self.repo_root, check=False)
            rc, _, _ = self._run(
                ["git", "commit", "-m", "chore(gitmodules): enforce branch = main for all submodules"],
                self.repo_root, check=False)
            if rc == 0:
                self.log("OK", ".gitmodules change committed")
            else:
                self.log("WARN", "Commit may have failed (nothing to commit?)")
        else:
            self.log("OK", ".gitmodules already correct")

        self._detect_submodules()

    def on_init(self):
        self._thread(self._init_submodules)

    def _init_submodules(self):
        self.log("INFO", "=== INIT SUBMODULES ===")
        rc, out, err = self._run(
            ["git", "submodule", "update", "--init", "--recursive"],
            self.repo_root, check=False)
        if rc == 0:
            self.log("OK", "All submodules initialized")
        else:
            self.log("ERROR", f"Init failed: {err}")

        for sub in self.submodules:
            full = self.repo_root / sub["path"]
            if not (full / ".git").exists():
                continue

            rc2, out2, _ = self._run(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"], full, check=False, silent=True)
            current = out2.strip()

            if current == "HEAD":
                self.log("WARN", f"{sub['name']}: Detached HEAD, checking out {sub['branch']}...")
                self._run(["git", "checkout", "-B", sub["branch"], f"origin/{sub['branch']}"],
                         full, check=False)
            elif current != sub["branch"]:
                self.log("WARN", f"{sub['name']}: Switching from '{current}' to '{sub['branch']}'...")
                self._run(["git", "checkout", sub["branch"]], full, check=False)
                self._run(["git", "branch", "--set-upstream-to",
                          f"origin/{sub['branch']}", sub["branch"]], full, check=False)

    def on_github_to_local(self):
        self._thread(self._github_to_local)

    def _github_to_local(self):
        self.log("INFO", "=== GITHUB -> LOCAL SYNC ===")

        self.log("INFO", "Pulling TheSeed (meta-repo)...")
        rc, out, _ = self._run(["git", "pull", "origin", "main"], self.repo_root, check=False)
        if rc == 0:
            if "Already up to date" in out:
                self.log("SKIP", "Meta-repo: already up to date")
            else:
                self.log("OK", "Meta-repo: updated from GitHub")
        else:
            self.log("ERROR", "Meta-repo pull failed!")
            return

        # CRITICAL: Update submodules to the new pointers from meta-repo
        self.log("INFO", "Updating submodules to new meta-repo pointers...")
        rc, out, err = self._run(
            ["git", "submodule", "update", "--init", "--recursive"],
            self.repo_root, check=False)
        if rc == 0:
            if "Submodule path" in out:
                self.log("OK", "Submodules updated to new pointers")
            else:
                self.log("SKIP", "Submodules already at correct commits")
        else:
            self.log("ERROR", f"Submodule update failed: {err}")

        for sub in self.submodules:
            full = self.repo_root / sub["path"]
            if not (full / ".git").exists():
                self.log("WARN", f"{sub['name']}: not initialized, skipping")
                continue

            self.log("INFO", f"--- {sub['name']} ---")
            self._run(["git", "fetch", "origin"], full, check=False)

            rc2, out2, _ = self._run(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"], full, check=False, silent=True)
            current = out2.strip()

            if current == "HEAD":
                self.log("WARN", f"  Detached HEAD -> checking out {sub['branch']}")
                self._run(["git", "checkout", "-B", sub["branch"], f"origin/{sub['branch']}"],
                         full, check=False)
            elif current != sub["branch"]:
                self.log("WARN", f"  Switching to {sub['branch']}")
                self._run(["git", "checkout", sub["branch"]], full, check=False)
                self._run(["git", "branch", "--set-upstream-to",
                          f"origin/{sub['branch']}", sub["branch"]], full, check=False)

            rc3, out3, _ = self._run(
                ["git", "pull", "origin", sub["branch"]], full, check=False)
            if rc3 == 0:
                if "Already up to date" in out3:
                    self.log("SKIP", f"  {sub['name']}: already up to date")
                else:
                    self.log("OK", f"  {sub['name']}: updated")
            else:
                self.log("ERROR", f"  {sub['name']}: pull failed")

        self._update_meta_pointer()

    def on_local_to_github(self):
        self._thread(self._local_to_github)

    def _local_to_github(self):
        self.log("INFO", "=== LOCAL -> GITHUB SYNC ===")

        for sub in self.submodules:
            full = self.repo_root / sub["path"]
            if not (full / ".git").exists():
                continue

            rc, out, _ = self._run(["git", "status", "--short"], full, check=False, silent=True)
            if out.strip():
                self.log("WARN", f"{sub['name']}: local changes detected, committing...")
                self._run(["git", "add", "-A"], full, check=False)
                self._run(["git", "commit", "-m",
                          f"auto: sync {sub['name']} {time.strftime('%Y-%m-%d %H:%M:%S')}"],
                         full, check=False)
                self.log("OK", f"  {sub['name']}: committed")

        for sub in self.submodules:
            full = self.repo_root / sub["path"]
            if not (full / ".git").exists():
                continue

            rc, out, _ = self._run(
                ["git", "log", f"origin/{sub['branch']}..{sub['branch']}", "--oneline"],
                full, check=False, silent=True)

            if out.strip():
                count = len(out.strip().splitlines())
                self.log("INFO", f"{sub['name']}: pushing {count} commit(s)...")
                rc2, _, err = self._run(["git", "push", "origin", sub["branch"]], full, check=False)
                if rc2 == 0:
                    self.log("OK", f"  {sub['name']}: pushed")
                else:
                    self.log("ERROR", f"  {sub['name']}: push failed - {err}")
            else:
                self.log("SKIP", f"{sub['name']}: nothing to push")

        self._update_meta_pointer()

        rc, out, _ = self._run(["git", "status", "--short"], self.repo_root, check=False, silent=True)
        if out.strip():
            self.log("WARN", "Meta-repo: local changes detected, committing...")
            self._run(["git", "add", "-A"], self.repo_root, check=False)
            self._run(["git", "commit", "-m",
                      f"auto: meta-repo sync {time.strftime('%Y-%m-%d %H:%M:%S')}"],
                     self.repo_root, check=False)
            self.log("OK", "Meta-repo: committed")

        rc, out, _ = self._run(
            ["git", "log", "origin/main..main", "--oneline"],
            self.repo_root, check=False, silent=True)

        if out.strip():
            count = len(out.strip().splitlines())
            self.log("INFO", f"Meta-repo: pushing {count} commit(s)...")
            rc2, _, err = self._run(["git", "push", "origin", "main"], self.repo_root, check=False)
            if rc2 == 0:
                self.log("OK", "Meta-repo: pushed to GitHub")
            else:
                self.log("ERROR", f"Meta-repo: push failed - {err}")
        else:
            self.log("SKIP", "Meta-repo: nothing to push")

    def on_full_sync(self):
        self._thread(self._full_sync)

    def _full_sync(self):
        self.log("INFO", "=== FULL BIDIRECTIONAL SYNC ===")
        self._fix_gitmodules()
        self._init_submodules()
        self._github_to_local()
        self._local_to_github()
        self.log("OK", "=== FULL SYNC COMPLETE ===")

    def _update_meta_pointer(self):
        self.log("INFO", "Checking meta-repo submodule pointers...")
        changed = []
        for sub in self.submodules:
            full = self.repo_root / sub["path"]
            if not (full / ".git").exists():
                continue
            rc, out, _ = self._run(
                ["git", "diff", "--submodule", sub["path"]],
                self.repo_root, check=False, silent=True)
            if out.strip():
                changed.append(sub["path"])

        if not changed:
            self.log("SKIP", "No submodule pointer changes")
            return

        self.log("WARN", f"Pointer changes: {changed}")
        for c in changed:
            self._run(["git", "add", c], self.repo_root, check=False)

        rc, _, _ = self._run(
            ["git", "commit", "-m", f"chore(submodule): sync pointers {time.strftime('%Y-%m-%d %H:%M:%S')}"],
            self.repo_root, check=False)
        if rc == 0:
            self.log("OK", "Meta-repo pointer updated and committed")
        else:
            self.log("WARN", "Pointer commit may have failed")

    def on_refresh(self):
        self._thread(self._refresh_table)

    # ============ BUILD & TEST ============

    def on_build(self):
        self._thread(self._build)

    def _build(self):
        preset = self.preset_var.get()
        self.log("INFO", f"=== BUILD: {preset} ===")

        # Auto-install prerequisites
        if not self._ensure_all_prerequisites():
            self.log("ERROR", "Prerequisites not met. Build aborted.")
            return

        build_dir = self.repo_root / "build" / preset.replace("-", "_")
        build_dir.mkdir(parents=True, exist_ok=True)

        # On Windows: if not in Developer Prompt, load vcvars env
        extra_env = {}
        if platform.system() == "Windows":
            rc, _, _ = self._run(["cl"], self.repo_root, check=False, silent=True)
            if rc != 0:
                vcvars_path = self._find_vcvarsall()
                if vcvars_path:
                    vcvars_env = self._load_vcvars_env(vcvars_path)
                    if vcvars_env:
                        extra_env = vcvars_env
                        # Merge into os.environ for subprocess
                        os.environ.update(vcvars_env)
                    else:
                        self.log("ERROR", "Failed to load MSVC environment")
                        return
                else:
                    self.log("ERROR", "MSVC environment not found. Install VS Build Tools.")
                    return

        # Configure
        self.log("INFO", f"Configuring with preset: {preset}")
        env = {**os.environ, **extra_env} if extra_env else None
        rc, out, err = self._run_env(
            ["cmake", "--preset", preset],
            self.repo_root, env=env, check=False)
        if rc != 0:
            self.log("ERROR", f"CMake configure failed for preset '{preset}'")
            return

        # Build
        self.log("INFO", "Building...")
        rc, out, err = self._run_env(
            ["cmake", "--build", f"build/{preset.replace('-', '_')}", "--parallel"],
            self.repo_root, env=env, check=False, timeout=600)
        if rc == 0:
            self.log("OK", f"Build complete: {preset}")
        else:
            self.log("ERROR", f"Build failed: {preset}")

    def _run_env(self, cmd, cwd, env=None, check=False, silent=False, timeout=60) -> tuple:
        """Run command with optional extra environment variables."""
        if not silent:
            self.log("CMD", f"{' '.join(cmd)}  (in {cwd})")
        try:
            result = subprocess.run(cmd, cwd=cwd, capture_output=True,
                                   text=True, check=False, timeout=timeout, env=env)
            if result.stdout and not silent:
                for line in result.stdout.strip().splitlines():
                    self.log("INFO", f"  > {line}")
            if result.stderr and not silent:
                for line in result.stderr.strip().splitlines():
                    if "warning" in line.lower():
                        self.log("WARN", f"  ! {line}")
                    else:
                        self.log("INFO", f"  ! {line}")
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            self.log("ERROR", f"  X Command timed out after {timeout}s")
            return 1, "", "timeout"
        except Exception as e:
            self.log("ERROR", f"  X Exception: {e}")
            return 1, "", str(e)

    def on_test(self):
        self._thread(self._test)

    def _test(self):
        test_filter = self.test_var.get()
        preset = self.preset_var.get()
        build_dir = self.repo_root / "build" / preset.replace("-", "_")

        self.log("INFO", f"=== TEST: {test_filter} (preset: {preset}) ===")

        if not build_dir.exists():
            self.log("WARN", f"Build dir not found: {build_dir}. Run Build first.")
            return

        regex = ".*" if test_filter == "all" else test_filter

        self.log("INFO", f"Running ctest -R '{regex}'")
        rc, out, err = self._run(
            ["ctest", "--test-dir", str(build_dir),
             "-R", regex, "--output-on-failure", "-j", str(os.cpu_count() or 4)],
            self.repo_root, check=False, timeout=300)

        if rc == 0:
            self.log("OK", f"Tests passed: {test_filter}")
        else:
            self.log("ERROR", f"Tests failed: {test_filter}")

    # ============ AUTO-SYNC ============

    def on_auto_toggle(self):
        if self.auto_var.get():
            self.auto_sync_active = True
            self.lbl_status.configure(text="Auto-Sync: ON", fg="#06d6a0")
            self.auto_sync_thread = threading.Thread(target=self._auto_sync_loop, daemon=True)
            self.auto_sync_thread.start()
            self.log("INFO", f"Auto-Sync started (interval: {self.interval_var.get()} min)")
        else:
            self.auto_sync_active = False
            self.lbl_status.configure(text="Idle", fg="#888888")
            self.log("INFO", "Auto-Sync stopped")

    def _auto_sync_loop(self):
        while self.auto_sync_active:
            try:
                interval = int(self.interval_var.get()) * 60
            except ValueError:
                interval = 300

            self.log("INFO", "Auto-Sync cycle starting...")
            self._full_sync()
            self.log("INFO", f"Auto-Sync cycle complete. Sleeping {interval//60} min...")

            slept = 0
            while slept < interval and self.auto_sync_active:
                time.sleep(1)
                slept += 1

    def on_closing(self):
        self.auto_sync_active = False
        self.root.destroy()


def main():
    root = Tk()
    app = SubmoduleSyncGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
