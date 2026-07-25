# CI/CD – TheSeed Engine

## Pipelines

### 1. CI (`ci.yml`)
**Trigger:** Push to `main`/`develop`, PRs

**Matrix:**
| OS | Compiler | Build | Sanitizer |
|----|----------|-------|-----------|
| ubuntu-24.04 | GCC 13 | Release + Debug | ASan+UBSan (Debug) |
| windows-latest | MSVC 17 | Release + Debug | /RTC1 (Debug) |

**Steps:**
1. Checkout with submodules
2. vcpkg cache restore
3. Configure (CMake presets)
4. Build
5. Unit tests (`ctest -R unit`)
6. Integration tests (`ctest -R integration`)
7. Stress tests (10× MultiThreadStress, 10× 100kEntities)
8. Static analysis (clang-tidy, cppcheck)

**Target duration:** < 10 minutes

### 2. Nightly (`nightly.yml`)
**Trigger:** Daily 02:00 UTC

**Additional checks:**
- Sanitizer tests (ASan, UBSan, TSan)
- Stress tests ×20
- Valgrind memory check
- Benchmarks with trend analysis
- Coverage report

**Target duration:** ~30 minutes

### 3. Phase Gate (`gate.yml`)
**Trigger:** Manual (end of each phase)

**Checks:**
- All gate tests (`ctest -R gate_p0`)
- Performance benchmarks
- Build time (< 3 min clean)
- Coverage (>= 80%)

**Target duration:** ~2 hours

### 4. Release (`release.yml`)
**Trigger:** Git tag `v*`

**Artifacts:**
- Linux x64 static library + headers
- Windows x64 static library + headers
- Changelog

### 5. Submodule Update (`submodule_update.yml`)
**Trigger:** Weekly (Monday 06:00 UTC)

**Action:** Auto-create PR with submodule updates

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `VCPKG_ROOT` | Path to vcpkg installation |
| `CMAKE_TOOLCHAIN_FILE` | vcpkg CMake toolchain |
| `ASAN_OPTIONS` | AddressSanitizer config |
| `UBSAN_OPTIONS` | UndefinedBehaviorSanitizer config |

## Local Development

```bash
# Quick build + test
./scripts/build.sh linux-debug

# Full test suite
./scripts/test.sh

# With sanitizers
cmake --preset linux-debug
cmake --build build/linux-debug --parallel
ctest --test-dir build/linux-debug --output-on-failure
```
