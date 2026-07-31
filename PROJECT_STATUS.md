# TheSeed – Project Status Snapshot
**Version:** 0.1.0 (Phase 0 Complete)  
**Date:** 2026-07-31  
**GitHub User:** Cosmiconn  

---

## What This Project Is

TheSeed is a C++20 game engine built as a meta-repo with 8 submodules:
- `seed-core` (P0: Memory, ECS, Jobs, Serialize, Math, Utils) ✅ COMPLETE
- `seed-network` (P1: Monat 7–12) ⏳ NOT STARTED
- `seed-renderer` (P2: Monat 13–20) ⏳ NOT STARTED
- `seed-editor` (P3: Monat 21–28) ⏳ NOT STARTED
- `seed-game` (P4: Monat 29–42) ⏳ NOT STARTED
- `seed-platform` (P5: Monat 43–54) ⏳ NOT STARTED
- `seed-cloud` (P6: Monat 55–66) ⏳ NOT STARTED
- `seed-launch` (P7: Monat 67–78) ⏳ NOT STARTED

Only `seed-core` physically exists. The other 7 are defined in `.gitmodules` with `__GITHUB_USER__` placeholder URLs.

---

## Critical Setup Requirements

### 1. GitHub Repository Secrets
The following secrets must be configured in the GitHub repo:

| Secret | Purpose |
|--------|---------|
| `SUBMODULE_TOKEN` | Personal Access Token for private submodule access |
| `CODECOV_TOKEN` | For Codecov coverage upload |
| `VOPKG_BINARY_SOURCES` | For vcpkg binary caching |

### 2. Local Sync Profile
Create this file before using `theseed_sync.py`:

```bash
mkdir -p ~/.config/theseed
cat > ~/.config/theseed/sync_profile.json << 'EOF'
{
  "github_user": "Cosmiconn",
  "auth_method": "token"
}
EOF
```

Also set the environment variable:
```bash
export SUBMODULE_TOKEN=ghp_xxxxxxxxxxxxxxxxxxxx
```

### 3. .gitmodules URLs
The `.gitmodules` file uses `__GITHUB_USER__` placeholder. The sync tool resolves this at runtime. Do NOT hardcode the username in `.gitmodules`.

---

## Architecture Decisions (ADRs)

| ADR | Topic | Status |
|-----|-------|--------|
| ADR-001 | Meta-repo vs. Monorepo | Accepted |
| ADR-002 | Test Strategy (doctest) | Accepted |
| ADR-003 | Memory Management | Accepted |
| ADR-004 | ECS Architecture | Accepted |
| ADR-005 | Roadmap Deviations | Accepted |
| ADR-006 | Performance Budgets (aspirational) | Accepted |

**Key deviation:** `barrier.h/.cpp` is integrated into `job_system.h/.cpp` instead of being a separate module. Documented in ADR-005.

---

## Test Strategy

| Test Type | Location | Block CI? | Framework |
|-----------|----------|-----------|-----------|
| Unit Tests | `submodules/seed-core/tests/unit/` | ✅ Yes | doctest |
| Property Tests | `submodules/seed-core/tests/property/` | ✅ Yes | doctest + rapidcheck |
| Fuzz Tests | `submodules/seed-core/tests/fuzz/` | ✅ Yes | libFuzzer |
| Benchmarks | `submodules/seed-core/tests/benchmarks/` | ❌ No | nanobench |
| Integration | `tests/integration/` | ✅ Yes | doctest |
| E2E | `tests/e2e/` | ✅ Yes | doctest |
| Gate – Functional | `tests/gate/test_gate_p0.cpp` | ✅ Yes | doctest |
| Gate – Performance | `tests/gate/test_gate_p0_perf.cpp` | ❌ No | doctest (CHECK) |

**Important:** Performance gates use `CHECK` (not `REQUIRE`) so they don't block CI. They run as informational tests.

---

## CI/CD Workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | Push/PR | Build + Test Linux & Windows, Coverage, clang-tidy, cppcheck |
| `gate.yml` | Manual | P0 Functional Gate (blocking) + Performance Gate (informational) |
| `nightly.yml` | Cron 2am | Stress tests x20, Valgrind, Static Analysis |
| `release.yml` | Manual | Build Release artifacts for Linux & Windows |
| `submodule_update.yml` | Manual | Update all submodules to latest main |

**All workflows use `token: ${{ secrets.SUBMODULE_TOKEN }}` for checkout.**

---

## Known Issues / TODOs

1. **Benchmarks:** Consolidated from two directories into `tests/benchmarks/` with nanobench. Verified against actual API.
2. **Performance Budgets:** Defined as "aspirational" (ADR-006). CI runners cannot meet them. Run benchmarks on dedicated hardware.
3. **Hardware Baseline:** Documented in `docs/BUILD.md` and ADR-006.
4. **Math Public API:** `math.h` includes `math_utils.h` (clamp, lerp, smoothstep, PI constants).
5. **Submodule Placeholder:** Only `seed-core` exists physically. Others are placeholders.

---

## Build Commands

```bash
# Linux Release
./scripts/build.sh linux-release

# Linux Debug with Sanitizers
./scripts/build.sh linux-debug

# Windows Release
scripts\build.bat windows-release

# Run tests
./scripts/test.sh

# Sync submodules
python scripts/sync/theseed_sync.py --github-to-local
```

---

## Next Phase: Phase 1 (Network Stack)

When ready to start Phase 1:
1. Create `seed-network` submodule repository on GitHub
2. Add it to `.gitmodules` (or let sync tool handle it)
3. Implement network layer per roadmap (Monat 7–12)
4. The meta-repo structure is ready to accept new submodules

---

## File Count
- Total: ~180 files
- Source (cpp/h): ~90 files
- Tests: ~50 files
- Docs/Scripts/CI: ~40 files
