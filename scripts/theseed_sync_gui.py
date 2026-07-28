#!/usr/bin/env python3
"""
TheSeed Sync GUI v2.6
Roadmap: 02_REPO_ARCHITECTURE.md §6, §9.3

CRITICAL FIX: Submodule workflow is now:
  1. cd submodules/X
  2. git checkout main (detached -> branch)
  3. git pull origin main
  4. cd ../..
  5. git add submodules/X
  6. git commit -m "update pointer"
"""

import tkinter as tk
from tkinter import ttk, messagebox
import subprocess
import threading
import queue
import json
import os
import shutil
from pathlib import Path
from datetime import datetime


class TheSeedSyncApp(tk.Tk):
    COLORS = {
        'bg': '#1e1e1e', 'fg': '#d4d4d4', 'accent': '#007acc',
        'success': '#4ec9b0', 'warning': '#dcdcaa', 'error': '#f44747',
        'info': '#569cd6', 'cmd': '#ce9178', 'panel': '#252526',
        'border': '#3e3e42', 'conflict': '#ff6b6b',
    }

    def __init__(self):
        super().__init__()
        self.title('TheSeed Sync GUI v2.6')
        self.geometry('1400x950')
        self.configure(bg=self.COLORS['bg'])

        self.script_dir = Path(__file__).parent.resolve()
        self.root_dir = self.script_dir.parent

        self.log_queue = queue.Queue()
        self.progress_queue = queue.Queue()

        self.current_branch = 'unknown'
        self.is_dirty = False
        self.is_running = False
        self._auto_sync_id = None
        self._has_conflicts = False

        self.config_file = self.script_dir / 'theseed_sync_config.json'
        self.app_config = self.load_config()

        self.create_widgets()
        self.apply_styles()
        self.setup_log_tags()

        self.after(100, self.process_queues)
        self.after(500, self.refresh_status)

        self.log('TheSeed Sync GUI v2.6 started', 'info')
        self.log('FIX: checkout branch -> pull -> check hash -> commit pointer', 'ok')

    def load_config(self):
        defaults = {
            'branches': ['main', 'develop'],
            'stash_on_dirty': True,
            'test_after_sync': False,
            'build_preset': 'linux-release',
            'build_dir': 'build',
            'auto_refresh_interval': 30,
            'auto_sync_interval_minutes': 5,
            'discord_webhook': None,
        }
        if self.config_file.exists():
            try:
                with open(self.config_file, 'r') as f:
                    defaults.update(json.load(f))
            except Exception as e:
                print(f'Config load error: {e}')
        return defaults

    def save_config(self):
        try:
            with open(self.config_file, 'w') as f:
                json.dump(self.app_config, f, indent=2)
            self.log('Configuration saved', 'ok')
        except Exception as e:
            self.log(f'Failed to save config: {e}', 'error')

    # ========================================================================
    # UI
    # ========================================================================
    def create_widgets(self):
        self.status_frame = tk.Frame(self, bg=self.COLORS['panel'], height=60)
        self.status_frame.pack(fill=tk.X, padx=5, pady=5)
        self.status_frame.pack_propagate(False)

        self.lbl_branch = tk.Label(self.status_frame, text='Branch: --',
            bg=self.COLORS['panel'], fg=self.COLORS['info'],
            font=('Consolas', 11, 'bold'))
        self.lbl_branch.pack(side=tk.LEFT, padx=15, pady=10)

        self.lbl_dirty = tk.Label(self.status_frame, text='Clean',
            bg=self.COLORS['panel'], fg=self.COLORS['success'],
            font=('Consolas', 10))
        self.lbl_dirty.pack(side=tk.LEFT, padx=10, pady=10)

        self.lbl_conflict = tk.Label(self.status_frame, text='No Conflicts',
            bg=self.COLORS['panel'], fg=self.COLORS['success'],
            font=('Consolas', 10, 'bold'))
        self.lbl_conflict.pack(side=tk.LEFT, padx=10, pady=10)

        self.lbl_submodule = tk.Label(self.status_frame, text='Submodule: --',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 10))
        self.lbl_submodule.pack(side=tk.LEFT, padx=10, pady=10)

        self.lbl_last_sync = tk.Label(self.status_frame, text='Last sync: Never',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9))
        self.lbl_last_sync.pack(side=tk.RIGHT, padx=15, pady=10)

        self.progress = ttk.Progressbar(self, mode='indeterminate')
        self.progress.pack(fill=tk.X, padx=5, pady=2)
        self.progress.stop()

        # Log Window
        self.log_frame = tk.Frame(self, bg=self.COLORS['bg'])
        self.log_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.log_toolbar = tk.Frame(self.log_frame, bg=self.COLORS['panel'])
        self.log_toolbar.pack(fill=tk.X, pady=(0, 2))

        tk.Label(self.log_toolbar, text=' Console Output ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 10, 'bold')).pack(side=tk.LEFT, padx=10, pady=5)

        tk.Button(self.log_toolbar, text='Clear Log', command=self.clear_log,
            bg=self.COLORS['border'], fg=self.COLORS['fg'],
            activebackground=self.COLORS['accent'],
            font=('Consolas', 9), relief=tk.FLAT).pack(side=tk.RIGHT, padx=5, pady=3)

        self.log_text = tk.Text(self.log_frame, bg=self.COLORS['bg'], fg=self.COLORS['fg'],
            font=('Consolas', 10), wrap=tk.WORD, state=tk.DISABLED,
            padx=10, pady=5, relief=tk.FLAT,
            highlightthickness=1, highlightbackground=self.COLORS['border'])
        self.log_text.pack(fill=tk.BOTH, expand=True, side=tk.LEFT)

        scrollbar = ttk.Scrollbar(self.log_frame, command=self.log_text.yview)
        scrollbar.pack(fill=tk.Y, side=tk.RIGHT)
        self.log_text.config(yscrollcommand=scrollbar.set)

        # Button Panel
        self.btn_frame = tk.Frame(self, bg=self.COLORS['panel'])
        self.btn_frame.pack(fill=tk.X, padx=5, pady=5)

        # CONFLICT RESOLVER
        conflict_frame = tk.LabelFrame(self.btn_frame, text=' CONFLICT RESOLVER ',
            bg=self.COLORS['panel'], fg=self.COLORS['conflict'],
            font=('Consolas', 9, 'bold'))
        conflict_frame.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        self.btn_show_conflicts = self.create_button(conflict_frame, 'Show\nConflicts', self.on_show_conflicts, '#5c2a2a')
        self.btn_keep_ours = self.create_button(conflict_frame, 'Keep Mine\n(ours)', self.on_keep_ours, '#5c2a2a')
        self.btn_keep_theirs = self.create_button(conflict_frame, 'Keep Theirs\n(theirs)', self.on_keep_theirs, '#5c2a2a')
        self.btn_abort_merge = self.create_button(conflict_frame, 'Abort\nMerge', self.on_abort_merge, '#5c2a2a')
        self.set_conflict_buttons_state(tk.DISABLED)

        # SYNC
        sync_frame = tk.LabelFrame(self.btn_frame, text=' Bidirectional Sync ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9, 'bold'))
        sync_frame.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        self.btn_sync_down = self.create_button(sync_frame, 'Sync Down\n(GH -> Local)', self.on_sync_down, '#1e3a5f')
        self.btn_sync_up = self.create_button(sync_frame, 'Sync Up\n(Local -> GH)', self.on_sync_up, '#3d5c3d')
        self.btn_full_sync = self.create_button(sync_frame, 'Full Sync', self.on_full_sync, '#5c4033')
        self.btn_auto_sync = self.create_button(sync_frame, 'Auto: OFF', self.on_auto_sync_toggle, '#4a4a4a')

        # Git Meta-Repo
        git_frame = tk.LabelFrame(self.btn_frame, text=' Git (Meta-Repo) ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9, 'bold'))
        git_frame.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        self.btn_pull = self.create_button(git_frame, 'Pull', self.on_pull, '#264f78')
        self.btn_push = self.create_button(git_frame, 'Push', self.on_push, '#264f78')
        self.btn_fetch = self.create_button(git_frame, 'Fetch', self.on_fetch, '#264f78')
        self.btn_status = self.create_button(git_frame, 'Status', self.on_status, '#264f78')

        # Submodule
        sub_frame = tk.LabelFrame(self.btn_frame, text=' Submodule ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9, 'bold'))
        sub_frame.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        self.btn_sub_init = self.create_button(sub_frame, 'Init All', self.on_submodule_init, '#5c4033')
        self.btn_sub_pull = self.create_button(sub_frame, 'Sub Pull', self.on_submodule_pull, '#5c4033')
        self.btn_sub_push = self.create_button(sub_frame, 'Sub Push', self.on_submodule_push, '#5c4033')
        self.btn_sub_commit = self.create_button(sub_frame, 'Commit Ptr', self.on_submodule_commit, '#5c4033')

        # Build & Test
        build_frame = tk.LabelFrame(self.btn_frame, text=' Build & Test ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9, 'bold'))
        build_frame.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        self.btn_build = self.create_button(build_frame, 'Build', self.on_build, '#3d5c3d')
        self.btn_test = self.create_button(build_frame, 'Test', self.on_test, '#3d5c3d')
        self.btn_clean = self.create_button(build_frame, 'Clean', self.on_clean, '#3d5c3d')

        # Settings
        settings_frame = tk.LabelFrame(self.btn_frame, text=' Settings ',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9, 'bold'))
        settings_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=5, pady=5)

        self.create_button(settings_frame, 'Config', self.open_config, '#4a4a4a')
        self.create_button(settings_frame, 'Refresh', self.refresh_status, '#4a4a4a')

        self.status_bar = tk.Label(self, text='Ready',
            bg=self.COLORS['panel'], fg=self.COLORS['fg'],
            font=('Consolas', 9), anchor=tk.W)
        self.status_bar.pack(fill=tk.X, side=tk.BOTTOM)

    def create_button(self, parent, text, command, color):
        btn = tk.Button(parent, text=text,
            command=lambda: self.run_threaded(command),
            bg=color, fg=self.COLORS['fg'],
            activebackground=self.COLORS['accent'],
            activeforeground='#ffffff',
            font=('Consolas', 9, 'bold'),
            width=12, height=2,
            relief=tk.FLAT, cursor='hand2')
        btn.pack(padx=5, pady=3, fill=tk.X)
        return btn

    def set_conflict_buttons_state(self, state):
        for btn in [self.btn_show_conflicts, self.btn_keep_ours,
                    self.btn_keep_theirs, self.btn_abort_merge]:
            btn.config(state=state)

    def apply_styles(self):
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TProgressbar',
            background=self.COLORS['accent'],
            troughcolor=self.COLORS['panel'])

    def setup_log_tags(self):
        for tag, color in [('error', self.COLORS['error']),
                           ('warn', self.COLORS['warning']),
                           ('ok', self.COLORS['success']),
                           ('info', self.COLORS['info']),
                           ('cmd', self.COLORS['cmd']),
                           ('conflict', self.COLORS['conflict']),
                           ('timestamp', '#808080')]:
            self.log_text.tag_config(tag, foreground=color)

    # ========================================================================
    # LOGGING & THREADING
    # ========================================================================
    def log(self, message, level='info'):
        self.log_queue.put((message, level))

    def process_queues(self):
        while not self.log_queue.empty():
            try:
                message, level = self.log_queue.get_nowait()
                self._write_log(message, level)
            except queue.Empty:
                break

        while not self.progress_queue.empty():
            try:
                action = self.progress_queue.get_nowait()
                if action == 'start':
                    self.progress.start(10)
                    self.is_running = True
                    self.set_buttons_state(tk.DISABLED)
                elif action == 'stop':
                    self.progress.stop()
                    self.is_running = False
                    self.set_buttons_state(tk.NORMAL)
            except queue.Empty:
                break

        self.after(50, self.process_queues)

    def _write_log(self, message, level):
        timestamp = datetime.now().strftime('%H:%M:%S')
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, f'[{timestamp}] ', 'timestamp')
        tag = level if level in ('error', 'warn', 'ok', 'info', 'cmd', 'conflict') else 'info'
        self.log_text.insert(tk.END, f'[{level.upper()}] ', tag)
        self.log_text.insert(tk.END, f'{message}\n', tag)
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def set_buttons_state(self, state):
        for btn in [self.btn_sync_down, self.btn_sync_up, self.btn_full_sync,
                    self.btn_pull, self.btn_push, self.btn_fetch, self.btn_status,
                    self.btn_sub_init, self.btn_sub_pull, self.btn_sub_push, self.btn_sub_commit,
                    self.btn_build, self.btn_test, self.btn_clean]:
            btn.config(state=state)

    def run_threaded(self, target):
        if self.is_running:
            self.log('Another operation is already running', 'warn')
            return
        thread = threading.Thread(target=target, daemon=True)
        thread.start()

    # ========================================================================
    # COMMAND EXECUTION
    # ========================================================================
    def run_command(self, cmd, cwd=None, shell=False):
        self.log(f'> {cmd}', 'cmd')
        try:
            proc = subprocess.Popen(
                cmd if shell else cmd.split(),
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, cwd=cwd or self.root_dir, shell=shell)
            for line in proc.stdout:
                line = line.rstrip()
                if line:
                    self.log(line, 'info')
            proc.wait()
            if proc.returncode != 0:
                self.log(f'Exit code: {proc.returncode}', 'warn')
            return proc.returncode
        except Exception as e:
            self.log(f'Command failed: {e}', 'error')
            return -1

    def run_git(self, args, cwd=None):
        return self.run_command(f'git {args}', cwd=cwd, shell=True)

    def run_git_capture(self, args, cwd=None):
        try:
            result = subprocess.run(
                f'git {args}', shell=True, capture_output=True, text=True,
                cwd=cwd or self.root_dir)
            return result.stdout.strip() if result.returncode == 0 else None
        except Exception:
            return None

    # ========================================================================
    # HELPERS
    # ========================================================================
    def get_current_branch(self):
        return self.run_git_capture('rev-parse --abbrev-ref HEAD')

    def get_submodules(self):
        submodules_dir = self.root_dir / 'submodules'
        if not submodules_dir.exists():
            return []
        return [d.name for d in submodules_dir.iterdir() if d.is_dir()]

    def get_submodule_default_branch(self, submod):
        sub_path = self.root_dir / 'submodules' / submod
        for branch in ['main', 'master']:
            result = self.run_git_capture(f'rev-parse --abbrev-ref origin/{branch}', cwd=sub_path)
            if result:
                return branch
        return 'main'

    def get_conflict_files(self):
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            capture_output=True, text=True, cwd=self.root_dir)
        if result.returncode != 0:
            return []
        conflicts = []
        conflict_markers = ('UU', 'AA', 'DD', 'AU', 'UA', 'DU', 'UD')
        for line in result.stdout.split('\n'):
            if len(line) >= 2 and line[:2] in conflict_markers:
                path = line[3:].strip()
                conflicts.append({'path': path, 'status': line[:2]})
        return conflicts

    def has_unmerged_files(self):
        return len(self.get_conflict_files()) > 0

    def is_working_tree_dirty(self):
        result = subprocess.run(
            ['git', 'status', '--porcelain'],
            capture_output=True, text=True, cwd=self.root_dir)
        if result.returncode != 0:
            return False
        for line in result.stdout.split('\n'):
            if line.strip() and not line.startswith('??'):
                return True
        return False

    def get_submodule_remote_status(self, submod):
        sub_path = self.root_dir / 'submodules' / submod
        default_branch = self.get_submodule_default_branch(submod)

        subprocess.run(['git', 'fetch', 'origin'],
            cwd=sub_path, capture_output=True)

        local = self.run_git_capture('rev-parse HEAD', cwd=sub_path)
        remote = self.run_git_capture(f'rev-parse origin/{default_branch}', cwd=sub_path)

        if not local or not remote:
            return 'error'

        if local == remote:
            return 'synced'

        merge_base = self.run_git_capture(f'merge-base {local} {remote}', cwd=sub_path)
        if merge_base == local:
            return 'behind'
        elif merge_base == remote:
            return 'ahead'
        else:
            return 'diverged'

    def get_cmake_presets(self):
        presets_file = self.root_dir / 'CMakePresets.json'
        if not presets_file.exists():
            return ['linux-release', 'linux-debug', 'windows-release', 'windows-debug']
        try:
            with open(presets_file, 'r') as f:
                data = json.load(f)
                presets = [p['name'] for p in data.get('configurePresets', [])]
                return presets if presets else ['linux-release']
        except Exception:
            return ['linux-release']

    # ========================================================================
    # CONFLICT RESOLUTION
    # ========================================================================
    def on_show_conflicts(self):
        self.progress_queue.put('start')
        self.log('=== CONFLICT FILES ===', 'conflict')
        conflicts = self.get_conflict_files()
        if not conflicts:
            self.log('No conflicts detected', 'ok')
            self.progress_queue.put('stop')
            return
        self.log(f'{len(conflicts)} file(s) with conflicts:', 'conflict')
        for c in conflicts:
            status_desc = {
                'UU': 'Both modified', 'AA': 'Both added', 'DD': 'Both deleted',
                'AU': 'Added by us, modified by them',
                'UA': 'Modified by us, added by them',
                'DU': 'Deleted by us, modified by them',
                'UD': 'Modified by us, deleted by them',
            }.get(c['status'], 'Unknown conflict')
            self.log(f'  [{c["status"]}] {c["path"]}  ({status_desc})', 'conflict')
        self.progress_queue.put('stop')

    def on_keep_ours(self):
        self.progress_queue.put('start')
        self.log('=== RESOLVE: KEEP MINE (ours) ===', 'conflict')
        conflicts = self.get_conflict_files()
        if not conflicts:
            self.log('No conflicts to resolve', 'warn')
            self.progress_queue.put('stop')
            return
        for c in conflicts:
            path = c['path']
            self.log(f'Keeping ours: {path}', 'info')
            rc = self.run_git(f'checkout --ours "{path}"')
            if rc == 0:
                rc2 = self.run_git(f'add "{path}"')
                if rc2 == 0:
                    self.log(f'  Resolved: {path}', 'ok')
        remaining = self.get_conflict_files()
        if not remaining:
            self.log('All conflicts resolved!', 'ok')
            self._has_conflicts = False
            self.set_conflict_buttons_state(tk.DISABLED)
            self.lbl_conflict.config(text='No Conflicts', fg=self.COLORS['success'])
        self.progress_queue.put('stop')

    def on_keep_theirs(self):
        self.progress_queue.put('start')
        self.log('=== RESOLVE: KEEP THEIRS (theirs) ===', 'conflict')
        conflicts = self.get_conflict_files()
        if not conflicts:
            self.log('No conflicts to resolve', 'warn')
            self.progress_queue.put('stop')
            return
        for c in conflicts:
            path = c['path']
            self.log(f'Keeping theirs: {path}', 'info')
            rc = self.run_git(f'checkout --theirs "{path}"')
            if rc == 0:
                rc2 = self.run_git(f'add "{path}"')
                if rc2 == 0:
                    self.log(f'  Resolved: {path}', 'ok')
        remaining = self.get_conflict_files()
        if not remaining:
            self.log('All conflicts resolved!', 'ok')
            self._has_conflicts = False
            self.set_conflict_buttons_state(tk.DISABLED)
            self.lbl_conflict.config(text='No Conflicts', fg=self.COLORS['success'])
        self.progress_queue.put('stop')

    def on_abort_merge(self):
        self.progress_queue.put('start')
        self.log('=== ABORT MERGE ===', 'conflict')
        rc = self.run_git('merge --abort')
        if rc == 0:
            self.log('Merge aborted. Working tree reset to HEAD.', 'ok')
            self._has_conflicts = False
            self.set_conflict_buttons_state(tk.DISABLED)
            self.lbl_conflict.config(text='No Conflicts', fg=self.COLORS['success'])
        else:
            self.log('Merge abort failed!', 'error')
        self.refresh_status()
        self.progress_queue.put('stop')

    # ========================================================================
    # GIT OPERATIONS
    # ========================================================================
    def on_pull(self):
        self.progress_queue.put('start')
        self.log('=== META-REPO PULL STARTED ===', 'info')
        branch = self.get_current_branch()
        if not branch or branch not in self.app_config['branches']:
            self.log(f'Branch "{branch}" not in watchlist. Aborting.', 'warn')
            self.progress_queue.put('stop')
            return
        if self.has_unmerged_files():
            self.log('MERGE CONFLICTS DETECTED! Resolve before pulling.', 'error')
            self.progress_queue.put('stop')
            return
        if self.is_working_tree_dirty() and self.app_config.get('stash_on_dirty', True):
            self.log('Working tree dirty. Stashing...', 'warn')
            rc = self.run_git('stash push -m "sync-stash"')
            if rc != 0:
                self.log('Stash failed! Aborting.', 'error')
                self.progress_queue.put('stop')
                return
        self.log(f'Pulling origin/{branch}...', 'info')
        rc = self.run_git(f'pull origin {branch}')
        if rc == 0:
            self.log('Pull successful', 'ok')
            self.lbl_last_sync.config(text=f'Last sync: {datetime.now().strftime("%H:%M:%S")}')
        else:
            self.log('Pull failed!', 'error')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_push(self):
        self.progress_queue.put('start')
        self.log('=== META-REPO PUSH STARTED ===', 'info')
        branch = self.get_current_branch()
        if not branch:
            self.progress_queue.put('stop')
            return
        rc = self.run_git(f'push --force-with-lease origin {branch}')
        if rc == 0:
            self.log('Push successful', 'ok')
        else:
            self.log('Push failed!', 'error')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_fetch(self):
        self.progress_queue.put('start')
        self.log('=== FETCH STARTED ===', 'info')
        self.run_git('fetch --all')
        self.log('Fetch complete', 'ok')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_status(self):
        self.progress_queue.put('start')
        self.log('=== STATUS ===', 'info')
        self.run_git('status')
        self.progress_queue.put('stop')

    # ========================================================================
    # SUBMODULE OPERATIONS (FIXED WORKFLOW)
    # ========================================================================
    def on_submodule_init(self):
        """Clone all submodules if not present."""
        self.progress_queue.put('start')
        self.log('=== SUBMODULE INIT/CLONE STARTED ===', 'info')
        if not (self.root_dir / '.gitmodules').exists():
            self.log('.gitmodules not found!', 'error')
            self.progress_queue.put('stop')
            return
        self.log('Running: git submodule update --init --recursive', 'cmd')
        rc = self.run_git('submodule update --init --recursive')
        if rc == 0:
            self.log('Submodules initialized', 'ok')
        else:
            self.log('Submodule init failed!', 'error')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_submodule_pull(self):
        """
        FIXED WORKFLOW:
        1. cd submodules/X
        2. git checkout main (or master)
        3. git pull origin main
        4. cd ../..
        5. git add submodules/X
        6. git commit
        """
        self.progress_queue.put('start')
        self.log('=== SUBMODULE PULL (FIXED WORKFLOW) ===', 'info')

        submodules = self.get_submodules()
        if not submodules:
            self.log('No submodules found in submodules/', 'warn')
            self.progress_queue.put('stop')
            return

        updated_submodules = []

        for submod in submodules:
            sub_path = self.root_dir / 'submodules' / submod
            self.log(f'--- {submod} ---', 'info')

            # Check if directory exists and has .git
            if not (sub_path / '.git').exists():
                self.log(f'{submod}: Not initialized! Run "Init All" first.', 'error')
                continue

            # Step 1: Get hash BEFORE any changes
            old_hash = self.run_git_capture('rev-parse HEAD', cwd=sub_path)
            self.log(f'{submod}: Current HEAD = {old_hash[:8] if old_hash else "unknown"}', 'info')

            # Step 2: Detect default branch
            default_branch = self.get_submodule_default_branch(submod)
            self.log(f'{submod}: Default branch = {default_branch}', 'info')

            # Step 3: Checkout the branch (exit detached HEAD)
            current = self.run_git_capture('rev-parse --abbrev-ref HEAD', cwd=sub_path)
            self.log(f'{submod}: Current ref = {current}', 'info')

            if current != default_branch:
                self.log(f'{submod}: Checking out {default_branch}...', 'warn')
                rc = self.run_git(f'checkout {default_branch}', cwd=sub_path)
                if rc != 0:
                    self.log(f'{submod}: Failed to checkout {default_branch}!', 'error')
                    continue

            # Step 4: Fetch remote
            self.log(f'{submod}: Fetching origin...', 'info')
            self.run_git('fetch origin', cwd=sub_path)

            # Step 5: Pull
            self.log(f'{submod}: Pulling origin/{default_branch}...', 'info')
            rc = self.run_git(f'pull origin {default_branch}', cwd=sub_path)
            if rc != 0:
                self.log(f'{submod}: Pull failed!', 'error')
                continue

            # Step 6: Get hash AFTER pull
            new_hash = self.run_git_capture('rev-parse HEAD', cwd=sub_path)
            self.log(f'{submod}: New HEAD = {new_hash[:8] if new_hash else "unknown"}', 'info')

            # Step 7: Compare
            if old_hash and new_hash and old_hash != new_hash:
                self.log(f'{submod}: UPDATED {old_hash[:8]} -> {new_hash[:8]}', 'ok')
                updated_submodules.append({'name': submod, 'old': old_hash[:8], 'new': new_hash[:8]})
            else:
                self.log(f'{submod}: No change', 'ok')

        # Step 8: Commit pointers in Meta-Repo for updated submodules
        if updated_submodules:
            self.log('', 'info')
            self.log('=== COMMITTING SUBMODULE POINTERS ===', 'info')

            for sub in updated_submodules:
                self.log(f'Adding submodules/{sub["name"]} ({sub["old"]} -> {sub["new"]})', 'info')

            rc = self.run_git('add submodules/')
            if rc != 0:
                self.log('git add failed!', 'error')
                self.progress_queue.put('stop')
                return

            commit_body = '\n'.join([f'- {s["name"]}: {s["old"]} -> {s["new"]}' for s in updated_submodules])
            commit_msg = (
                f'chore(submodule): update submodule pointers\n\n'
                f'{commit_body}\n\n'
                f'Time: {datetime.now().strftime("%Y-%m-%d %H:%M")}\n'
                f'Refs: auto-sync'
            )

            rc = self.run_git(f'commit -m "{commit_msg}"')
            if rc == 0:
                self.log('Submodule pointers committed', 'ok')
            else:
                self.log('Commit failed!', 'error')
        else:
            self.log('No submodule pointers to commit', 'ok')

        self.refresh_status()
        self.progress_queue.put('stop')

    def on_submodule_push(self):
        """Push submodules to their remotes."""
        self.progress_queue.put('start')
        self.log('=== SUBMODULE PUSH STARTED ===', 'info')

        submodules = self.get_submodules()
        if not submodules:
            self.log('No submodules found', 'warn')
            self.progress_queue.put('stop')
            return

        for submod in submodules:
            sub_path = self.root_dir / 'submodules' / submod
            default_branch = self.get_submodule_default_branch(submod)

            self.log(f'--- {submod} ---', 'info')

            if not (sub_path / '.git').exists():
                self.log(f'{submod}: Not initialized! Skipping.', 'error')
                continue

            # Ensure on branch
            current = self.run_git_capture('rev-parse --abbrev-ref HEAD', cwd=sub_path)
            if current == 'HEAD':
                self.log(f'{submod}: Detached HEAD -> checkout {default_branch}', 'warn')
                self.run_git(f'checkout {default_branch}', cwd=sub_path)

            status = self.get_submodule_remote_status(submod)

            if status == 'ahead':
                self.log(f'{submod}: Local ahead. Pushing...', 'info')
                rc = self.run_git(f'push origin {default_branch}', cwd=sub_path)
                if rc == 0:
                    self.log(f'{submod}: Pushed', 'ok')
                else:
                    self.log(f'{submod}: Push failed!', 'error')
            elif status == 'behind':
                self.log(f'{submod}: Remote ahead. Skipping.', 'warn')
            elif status == 'diverged':
                self.log(f'{submod}: DIVERGED! Skipping.', 'error')
            elif status == 'synced':
                self.log(f'{submod}: Up to date', 'ok')

        self.refresh_status()
        self.progress_queue.put('stop')

    def on_submodule_commit(self):
        """Manually commit current submodule pointers to meta-repo."""
        self.progress_queue.put('start')
        self.log('=== MANUAL SUBMODULE POINTER COMMIT ===', 'info')

        changes = []
        for submod in self.get_submodules():
            sub_path = self.root_dir / 'submodules' / submod
            if not (sub_path / '.git').exists():
                continue

            # Pinned in meta-repo
            pinned = self.run_git_capture(f'ls-tree HEAD submodules/{submod}')
            if not pinned:
                continue
            parts = pinned.split()
            if len(parts) < 3:
                continue
            pinned_hash = parts[2]

            # Actual HEAD
            actual = self.run_git_capture('rev-parse HEAD', cwd=sub_path)
            if not actual:
                continue

            if pinned_hash != actual:
                changes.append({
                    'name': submod,
                    'pinned': pinned_hash[:8],
                    'actual': actual[:8]
                })

        if not changes:
            self.log('No pointer changes to commit', 'warn')
            self.progress_queue.put('stop')
            return

        for change in changes:
            self.log(f'{change["name"]}: {change["pinned"]} -> {change["actual"]}', 'warn')

        rc = self.run_git('add submodules/')
        if rc != 0:
            self.log('git add failed!', 'error')
            self.progress_queue.put('stop')
            return

        commit_body = '\n'.join([f'- {c["name"]}: {c["actual"]}' for c in changes])
        commit_msg = (
            f'chore(submodule): update submodule pointers\n\n'
            f'{commit_body}\n\n'
            f'Time: {datetime.now().strftime("%Y-%m-%d %H:%M")}\n'
            f'Refs: auto-sync'
        )

        rc = self.run_git(f'commit -m "{commit_msg}"')
        if rc == 0:
            self.log('Submodule pointers committed', 'ok')
        else:
            self.log('Commit failed!', 'error')

        self.refresh_status()
        self.progress_queue.put('stop')

    # ========================================================================
    # BIDIRECTIONAL SYNC
    # ========================================================================
    def on_sync_down(self):
        self.progress_queue.put('start')
        self.log('=== SYNC DOWN (GitHub -> Local) STARTED ===', 'info')

        branch = self.get_current_branch()
        if not branch or branch not in self.app_config['branches']:
            self.log('Invalid branch for sync', 'error')
            self.progress_queue.put('stop')
            return

        if self.has_unmerged_files():
            self.log('MERGE CONFLICTS DETECTED! Fix manually first.', 'error')
            self.progress_queue.put('stop')
            return

        if self.is_working_tree_dirty() and self.app_config.get('stash_on_dirty', True):
            self.log('Stashing local changes...', 'warn')
            rc = self.run_git('stash push -m "sync-down-stash"')
            if rc != 0:
                self.log('Stash failed! Aborting.', 'error')
                self.progress_queue.put('stop')
                return

        # Pull Meta-Repo
        self.log(f'Pulling Meta-Repo (origin/{branch})...', 'info')
        rc = self.run_git(f'pull origin {branch}')
        if rc != 0:
            self.log('Meta-Repo pull failed! Aborting.', 'error')
            self.progress_queue.put('stop')
            return

        # Init submodules if needed
        self.log('Ensuring submodules are initialized...', 'info')
        self.run_git('submodule update --init --recursive')

        # Pull each submodule
        updated_submodules = []
        for submod in self.get_submodules():
            sub_path = self.root_dir / 'submodules' / submod
            if not (sub_path / '.git').exists():
                self.log(f'{submod}: Not initialized! Skipping.', 'error')
                continue

            default_branch = self.get_submodule_default_branch(submod)

            # Checkout branch
            current = self.run_git_capture('rev-parse --abbrev-ref HEAD', cwd=sub_path)
            if current == 'HEAD' or current != default_branch:
                self.log(f'{submod}: Checkout {default_branch}...', 'warn')
                self.run_git(f'checkout {default_branch}', cwd=sub_path)

            # Get old hash
            old_hash = self.run_git_capture('rev-parse HEAD', cwd=sub_path)

            # Fetch and pull
            self.run_git('fetch origin', cwd=sub_path)
            rc = self.run_git(f'pull origin {default_branch}', cwd=sub_path)

            # Get new hash
            new_hash = self.run_git_capture('rev-parse HEAD', cwd=sub_path)

            if rc == 0 and old_hash != new_hash:
                self.log(f'{submod}: Updated {old_hash[:8]} -> {new_hash[:8]}', 'ok')
                updated_submodules.append({'name': submod, 'old': old_hash[:8], 'new': new_hash[:8]})

        # Commit pointers
        if updated_submodules:
            self.log('Committing submodule pointers...', 'info')
            self.run_git('add submodules/')
            commit_body = '\n'.join([f'- {s["name"]}: {s["old"]} -> {s["new"]}' for s in updated_submodules])
            commit_msg = (
                f'chore(submodule): update submodule pointers\n\n'
                f'{commit_body}\n\n'
                f'Time: {datetime.now().strftime("%Y-%m-%d %H:%M")}\n'
                f'Refs: auto-sync'
            )
            self.run_git(f'commit -m "{commit_msg}"')
            self.log('Pointers committed', 'ok')

        self.lbl_last_sync.config(text=f'Last sync: {datetime.now().strftime("%H:%M:%S")}')
        self.log('=== SYNC DOWN COMPLETE ===', 'ok')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_sync_up(self):
        self.progress_queue.put('start')
        self.log('=== SYNC UP (Local -> GitHub) STARTED ===', 'info')

        branch = self.get_current_branch()
        if not branch:
            self.progress_queue.put('stop')
            return

        # Push submodules
        for submod in self.get_submodules():
            sub_path = self.root_dir / 'submodules' / submod
            default_branch = self.get_submodule_default_branch(submod)

            if not (sub_path / '.git').exists():
                continue

            current = self.run_git_capture('rev-parse --abbrev-ref HEAD', cwd=sub_path)
            if current == 'HEAD':
                self.run_git(f'checkout {default_branch}', cwd=sub_path)

            status = self.get_submodule_remote_status(submod)
            if status == 'ahead':
                self.log(f'{submod}: Pushing...', 'info')
                self.run_git(f'push origin {default_branch}', cwd=sub_path)
                self.log(f'{submod}: Pushed', 'ok')

        # Commit pointers
        changes = []
        for submod in self.get_submodules():
            pinned = self.run_git_capture(f'ls-tree HEAD submodules/{submod}')
            if not pinned:
                continue
            parts = pinned.split()
            if len(parts) < 3:
                continue
            pinned_hash = parts[2]
            actual = self.run_git_capture('rev-parse HEAD',
                cwd=self.root_dir / 'submodules' / submod)
            if actual and pinned_hash != actual:
                changes.append({'name': submod, 'actual': actual[:8]})

        if changes:
            self.log('Committing pointers...', 'info')
            self.run_git('add submodules/')
            commit_body = '\n'.join([f'- {c["name"]}: {c["actual"]}' for c in changes])
            commit_msg = (
                f'chore(submodule): update submodule pointers\n\n'
                f'{commit_body}\n\n'
                f'Time: {datetime.now().strftime("%Y-%m-%d %H:%M")}\n'
                f'Refs: auto-sync'
            )
            self.run_git(f'commit -m "{commit_msg}"')

        # Push Meta-Repo
        local_commits = self.run_git_capture(f'log origin/{branch}..HEAD --oneline')
        if local_commits:
            count = len(local_commits.split('\n'))
            self.log(f'Pushing Meta-Repo ({count} commits)...', 'info')
            self.run_git(f'push --force-with-lease origin {branch}')
            self.log('Meta-Repo pushed', 'ok')
        else:
            self.log('Meta-Repo: Nothing to push', 'ok')

        self.lbl_last_sync.config(text=f'Last sync: {datetime.now().strftime("%H:%M:%S")}')
        self.log('=== SYNC UP COMPLETE ===', 'ok')
        self.refresh_status()
        self.progress_queue.put('stop')

    def on_full_sync(self):
        def full():
            self.on_sync_down()
            while self.is_running:
                import time
                time.sleep(0.5)
            self.after(1000, self.on_sync_up)
        self.run_threaded(full)

    # ========================================================================
    # AUTO SYNC
    # ========================================================================
    def on_auto_sync_toggle(self):
        if self._auto_sync_id:
            self.after_cancel(self._auto_sync_id)
            self._auto_sync_id = None
            self.btn_auto_sync.config(text='Auto: OFF', bg='#4a4a4a')
            self.log('Auto-sync STOPPED', 'warn')
        else:
            interval = self.app_config.get('auto_sync_interval_minutes', 5)
            self.log(f'Auto-sync STARTED (every {interval} min)', 'ok')
            self.btn_auto_sync.config(text='Auto: ON', bg=self.COLORS['success'])
            self._schedule_auto_sync()

    def _schedule_auto_sync(self):
        if not self._auto_sync_id and self.btn_auto_sync.cget('text') == 'Auto: OFF':
            return
        interval = self.app_config.get('auto_sync_interval_minutes', 5) * 60 * 1000
        def cycle():
            self.on_sync_down()
        self.after(100, cycle)
        self._auto_sync_id = self.after(interval, self._schedule_auto_sync)

    # ========================================================================
    # BUILD & TEST
    # ========================================================================
    def on_build(self):
        self.progress_queue.put('start')
        self.log('=== BUILD STARTED ===', 'info')
        build_dir = self.root_dir / self.app_config.get('build_dir', 'build')
        preset = self.app_config.get('build_preset', 'linux-release')
        if not build_dir.exists():
            self.log(f'Build directory not found: {build_dir}', 'error')
            self.log(f'Run: cmake --preset {preset}', 'info')
            self.progress_queue.put('stop')
            return
        self.log(f'Building preset: {preset}', 'info')
        self.run_command(f'cmake --build {build_dir} --parallel', shell=True)
        self.log('Build complete', 'ok')
        self.progress_queue.put('stop')

    def on_test(self):
        self.progress_queue.put('start')
        self.log('=== TEST STARTED ===', 'info')
        build_dir = self.root_dir / self.app_config.get('build_dir', 'build')
        if not build_dir.exists():
            self.log(f'Build directory not found: {build_dir}', 'error')
            self.progress_queue.put('stop')
            return
        self.log('Running tests...', 'info')
        self.run_command(f'ctest --test-dir {build_dir} --output-on-failure', shell=True)
        self.log('Tests complete', 'ok')
        self.progress_queue.put('stop')

    def on_clean(self):
        self.progress_queue.put('start')
        self.log('=== CLEAN STARTED ===', 'info')
        build_dir = self.root_dir / self.app_config.get('build_dir', 'build')
        if build_dir.exists():
            try:
                shutil.rmtree(build_dir)
                self.log(f'Removed: {build_dir}', 'ok')
            except Exception as e:
                self.log(f'Clean failed: {e}', 'error')
        else:
            self.log('Build directory does not exist', 'warn')
        self.progress_queue.put('stop')

    # ========================================================================
    # CONFIG DIALOG
    # ========================================================================
    def open_config(self):
        ConfigDialog(self, self.app_config, self.save_config)

    # ========================================================================
    # STATUS REFRESH
    # ========================================================================
    def refresh_status(self):
        try:
            branch = self.get_current_branch()
            if branch:
                self.current_branch = branch
                self.lbl_branch.config(text=f'Branch: {branch}')
                if branch in self.app_config['branches']:
                    self.lbl_branch.config(fg=self.COLORS['success'])
                else:
                    self.lbl_branch.config(fg=self.COLORS['warning'])

            conflicts = self.get_conflict_files()
            if conflicts:
                self._has_conflicts = True
                self.lbl_conflict.config(text=f'{len(conflicts)} CONFLICTS!', fg=self.COLORS['conflict'])
                self.set_conflict_buttons_state(tk.NORMAL)
                self.status_bar.config(text=f'CONFLICT: {len(conflicts)} file(s) need resolution', fg=self.COLORS['conflict'])
            else:
                self._has_conflicts = False
                self.lbl_conflict.config(text='No Conflicts', fg=self.COLORS['success'])
                self.set_conflict_buttons_state(tk.DISABLED)
                self.status_bar.config(text='Ready', fg=self.COLORS['fg'])

            dirty = self.is_working_tree_dirty()
            self.is_dirty = dirty
            if dirty:
                self.lbl_dirty.config(text='DIRTY', fg=self.COLORS['error'])
            else:
                self.lbl_dirty.config(text='Clean', fg=self.COLORS['success'])

            submodules = self.get_submodules()
            if submodules:
                not_init = sum(1 for s in submodules if not (self.root_dir / 'submodules' / s / '.git').exists())
                behind_count = ahead_count = diverged_count = 0
                for submod in submodules:
                    if (self.root_dir / 'submodules' / submod / '.git').exists():
                        status = self.get_submodule_remote_status(submod)
                        if status == 'behind': behind_count += 1
                        elif status == 'ahead': ahead_count += 1
                        elif status == 'diverged': diverged_count += 1

                if not_init > 0:
                    self.lbl_submodule.config(text=f'Submodule: {not_init} NOT INIT!', fg=self.COLORS['error'])
                elif diverged_count > 0:
                    self.lbl_submodule.config(text=f'Submodule: {diverged_count} diverged!', fg=self.COLORS['error'])
                elif behind_count > 0:
                    self.lbl_submodule.config(text=f'Submodule: {behind_count} behind', fg=self.COLORS['warning'])
                elif ahead_count > 0:
                    self.lbl_submodule.config(text=f'Submodule: {ahead_count} ahead', fg=self.COLORS['info'])
                else:
                    self.lbl_submodule.config(text=f'Submodule: {len(submodules)} synced', fg=self.COLORS['success'])
            else:
                self.lbl_submodule.config(text='Submodule: none', fg=self.COLORS['fg'])

        except Exception as e:
            self.log(f'Status refresh failed: {e}', 'error')

        interval = self.app_config.get('auto_refresh_interval', 30) * 1000
        self.after(interval, self.refresh_status)

    def clear_log(self):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.delete('1.0', tk.END)
        self.log_text.config(state=tk.DISABLED)
        self.log('Log cleared', 'info')

    def open_log_file(self):
        log_path = self.root_dir / 'scripts' / 'auto-sync.log'
        if log_path.exists():
            import platform
            system = platform.system()
            try:
                if system == 'Windows':
                    os.startfile(str(log_path))
                elif system == 'Darwin':
                    subprocess.run(['open', str(log_path)])
                else:
                    subprocess.run(['xdg-open', str(log_path)])
            except Exception as e:
                self.log(f'Could not open log file: {e}', 'error')
        else:
            self.log('Log file not found', 'warn')


# =============================================================================
# CONFIG DIALOG
# =============================================================================
class ConfigDialog(tk.Toplevel):
    def __init__(self, parent, config, save_callback):
        super().__init__(parent)
        self.parent = parent
        self.config = config.copy()
        self.save_callback = save_callback
        self.title('TheSeed Sync - Configuration')
        self.geometry('500x750')
        self.configure(bg=parent.COLORS['bg'])
        self.transient(parent)
        self.grab_set()
        self.create_widgets()
        self.center_window()

    def create_widgets(self):
        frame = tk.Frame(self, bg=self.parent.COLORS['bg'], padx=20, pady=20)
        frame.pack(fill=tk.BOTH, expand=True)
        tk.Label(frame, text='Configuration',
            bg=self.parent.COLORS['bg'], fg=self.parent.COLORS['info'],
            font=('Consolas', 14, 'bold')).pack(pady=(0, 20))

        self._create_entry(frame, 'Watched Branches (comma-separated):',
                          'branches', ','.join(self.config.get('branches', ['main', 'develop'])))
        presets = self.parent.get_cmake_presets()
        self._create_dropdown(frame, 'CMake Preset:',
                             'build_preset', presets,
                             self.config.get('build_preset', 'linux-release'))
        self._create_entry(frame, 'Build Directory:',
                          'build_dir', self.config.get('build_dir', 'build'))
        self._create_entry(frame, 'Auto-refresh Interval (seconds):',
                          'auto_refresh_interval',
                          str(self.config.get('auto_refresh_interval', 30)))
        self._create_entry(frame, 'Auto-Sync Interval (minutes):',
                          'auto_sync_interval_minutes',
                          str(self.config.get('auto_sync_interval_minutes', 5)))
        self._create_checkbox(frame, 'Stash on Dirty',
                             'stash_on_dirty',
                             self.config.get('stash_on_dirty', True))
        self._create_checkbox(frame, 'Test After Sync',
                             'test_after_sync',
                             self.config.get('test_after_sync', False))
        self._create_entry(frame, 'Discord Webhook (optional):',
                          'discord_webhook',
                          self.config.get('discord_webhook', '') or '')

        btn_frame = tk.Frame(frame, bg=self.parent.COLORS['bg'])
        btn_frame.pack(fill=tk.X, pady=(20, 0))
        tk.Button(btn_frame, text='Save', command=self.on_save,
            bg=self.parent.COLORS['success'], fg='#000000',
            font=('Consolas', 11, 'bold'),
            width=12, relief=tk.FLAT, cursor='hand2').pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text='Cancel', command=self.destroy,
            bg=self.parent.COLORS['border'], fg=self.parent.COLORS['fg'],
            font=('Consolas', 11),
            width=12, relief=tk.FLAT, cursor='hand2').pack(side=tk.LEFT, padx=5)

    def _create_entry(self, parent, label, key, default):
        tk.Label(parent, text=label,
            bg=self.parent.COLORS['bg'], fg=self.parent.COLORS['fg'],
            font=('Consolas', 10), anchor='w').pack(fill=tk.X, pady=(10, 2))
        entry = tk.Entry(parent, font=('Consolas', 10),
            bg=self.parent.COLORS['panel'], fg=self.parent.COLORS['fg'],
            insertbackground=self.parent.COLORS['fg'],
            relief=tk.FLAT, highlightthickness=1,
            highlightbackground=self.parent.COLORS['border'])
        entry.insert(0, default)
        entry.pack(fill=tk.X, pady=(0, 5))
        setattr(self, f'entry_{key}', entry)

    def _create_dropdown(self, parent, label, key, options, default):
        tk.Label(parent, text=label,
            bg=self.parent.COLORS['bg'], fg=self.parent.COLORS['fg'],
            font=('Consolas', 10), anchor='w').pack(fill=tk.X, pady=(10, 2))
        var = tk.StringVar(value=default)
        dropdown = ttk.Combobox(parent, textvariable=var,
            values=options, font=('Consolas', 10), state='readonly')
        dropdown.pack(fill=tk.X, pady=(0, 5))
        setattr(self, f'var_{key}', var)

    def _create_checkbox(self, parent, label, key, default):
        var = tk.BooleanVar(value=default)
        cb = tk.Checkbutton(parent, text=label, variable=var,
            bg=self.parent.COLORS['bg'], fg=self.parent.COLORS['fg'],
            selectcolor=self.parent.COLORS['panel'],
            activebackground=self.parent.COLORS['bg'],
            font=('Consolas', 10))
        cb.pack(anchor='w', pady=5)
        setattr(self, f'var_{key}', var)

    def on_save(self):
        try:
            self.config['branches'] = [b.strip() for b in self.entry_branches.get().split(',') if b.strip()]
            self.config['build_preset'] = self.var_build_preset.get()
            self.config['build_dir'] = self.entry_build_dir.get()
            self.config['auto_refresh_interval'] = int(self.entry_auto_refresh_interval.get())
            self.config['auto_sync_interval_minutes'] = int(self.entry_auto_sync_interval_minutes.get())
            self.config['stash_on_dirty'] = self.var_stash_on_dirty.get()
            self.config['test_after_sync'] = self.var_test_after_sync.get()
            webhook = self.entry_discord_webhook.get().strip()
            self.config['discord_webhook'] = webhook if webhook else None
            self.save_callback()
            self.parent.app_config = self.config
            self.parent.log('Configuration saved and applied', 'ok')
            self.destroy()
        except Exception as e:
            messagebox.showerror('Error', f'Failed to save config: {e}')

    def center_window(self):
        self.update_idletasks()
        width = self.winfo_width()
        height = self.winfo_height()
        x = (self.winfo_screenwidth() // 2) - (width // 2)
        y = (self.winfo_screenheight() // 2) - (height // 2)
        self.geometry(f'{width}x{height}+{x}+{y}')


# =============================================================================
# MAIN
# =============================================================================
def main():
    app = TheSeedSyncApp()
    app.mainloop()


if __name__ == '__main__':
    main()