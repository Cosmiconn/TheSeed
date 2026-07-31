# TheSeed Sync, Build & Test Tools

Complete toolchain for syncing, building, and testing TheSeed.

## Python Tools (Primary)

| Tool | Purpose |
|------|---------|
| `theseed_sync_gui.py` | GUI for sync, build, and test operations |
| `theseed_sync.py` | CLI for all operations |

---

## GUI

```bash
python3 scripts/sync/theseed_sync_gui.py
```

### Features

**Sync:**
- **Fix .gitmodules** - Ensures `branch = main` for all submodules
- **Init Submodules** - Initializes all submodules and checks out `main`
- **GitHub -> Local** - Pulls all changes from GitHub
- **Local -> GitHub** - Commits and pushes all local changes
- **Full Sync** - Complete bidirectional sync + pointer update
- **Auto-Sync** - Runs Full Sync automatically every N minutes

**Build & Test:**
- **Auto-Install** - Missing dependencies (vcpkg, Ninja) are installed automatically
- **Preset selector** - OS-aware: Windows shows `windows-release/debug`, Linux shows `linux-release/debug`
- **Test filters** - `all`, `unit`, `integration`, `property`, `fuzz`, `benchmark`, `gate_p0`..`gate_p7`

---

## CLI

### Sync Commands

```bash
# Fix .gitmodules
python3 scripts/sync/theseed_sync.py --fix-gitmodules

# Initialize submodules
python3 scripts/sync/theseed_sync.py --init

# Pull from GitHub
python3 scripts/sync/theseed_sync.py --github-to-local

# Push to GitHub
python3 scripts/sync/theseed_sync.py --local-to-github

# Full bidirectional sync
python3 scripts/sync/theseed_sync.py --full

# Auto-sync every 5 minutes
python3 scripts/sync/theseed_sync.py --auto-sync --interval 5
```

### Build Commands

```bash
# Build with auto-install of missing dependencies
python3 scripts/sync/theseed_sync.py --build

# Build with specific preset
python3 scripts/sync/theseed_sync.py --build --preset windows-debug
```

**Available Presets** (OS-aware):
| Preset | Platform | Type | Sanitizer |
|--------|----------|------|-----------|
| `linux-release` | Linux | Release | None |
| `linux-debug` | Linux | Debug | ASan+UBSan |
| `windows-release` | Windows | Release | None |
| `windows-debug` | Windows | Debug | ASan+UBSan |

### Test Commands

```bash
# Run all tests (default preset)
python3 scripts/sync/theseed_sync.py --test

# Run unit tests only
python3 scripts/sync/theseed_sync.py --test --test-filter unit

# Run gate tests for Phase 0
python3 scripts/sync/theseed_sync.py --test --test-filter gate_p0
```

**Available Test Filters**:
| Filter | Description |
|--------|-------------|
| `all` | All tests |
| `unit` | Unit tests (doctest) |
| `integration` | Integration tests |
| `property` | Property-based tests (rapidcheck) |
| `fuzz` | Fuzz tests (libFuzzer) |
| `benchmark` | Performance benchmarks |
| `gate_p0` | Phase 0 Gate (Fundament) |
| `gate_p1` | Phase 1 Gate (Network) |
| `gate_p2` | Phase 2 Gate (Rendering) |
| `gate_p3` | Phase 3 Gate (Editor) |
| `gate_p4` | Phase 4 Gate (Vertical Slice) |
| `gate_p5` | Phase 5 Gate (Platform) |
| `gate_p6` | Phase 6 Gate (Cloud) |
| `gate_p7` | Phase 7 Gate (Launch) |

---

## Auto-Install Behavior

When you click **Build** or run `--build`, the tool automatically:

1. **Checks vcpkg** - If missing, clones from GitHub and bootstraps
2. **Checks Ninja** - If missing, installs via `pip install ninja`
3. **Checks Compiler** - MSVC (Windows) or GCC/Clang (Linux). Cannot auto-install MSVC - shows download link
4. **Checks CMake** - Must be installed manually if missing
5. **Installs vcpkg dependencies** - Runs `vcpkg install` for the target triplet

If any step fails, the build is aborted with clear error messages.

---

## Bash Wrappers (Legacy)

The bash scripts delegate to the Python tools:

```bash
# Auto-sync daemon
./scripts/sync/auto-sync.sh [interval_minutes]

# One-directional sync
./scripts/sync/auto-sync-github-to-local.sh
./scripts/sync/auto-sync-local-to-github.sh
```

---

## Safety

| Feature | Behavior |
|---------|----------|
| Detached HEAD fix | Automatically checks out `main` branch |
| Pointer update | Commits submodule pointer changes in meta-repo |
| Error visibility | All commands logged with color-coded output |
| Timeout protection | Build: 10min, Test: 5min, Sync: 2min, vcpkg: 10min |
| Cross-platform | Python + tkinter runs on Windows, Linux, macOS |
| No CMake changes | Tool never modifies CMakeLists.txt or CMakePresets.json |
