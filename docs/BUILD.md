# TheSeed Build Instructions
## Prerequisites
- CMake 3.25+, Ninja, vcpkg
- C++20: MSVC 2022+ (Windows) / GCC 13+ (Linux)

## System Requirements

### Minimum (Build + Run)
| Component | Requirement |
|-----------|-------------|
| CPU | 4 Cores, 2.5 GHz |
| RAM | 8 GB |
| Storage | 10 GB free (SSD recommended) |
| OS | Ubuntu 22.04 / Windows 10 / macOS 13 |

### Recommended (Development)
| Component | Requirement |
|-----------|-------------|
| CPU | 6+ Cores, 3.5+ GHz |
| RAM | 16+ GB |
| Storage | NVMe SSD |
| OS | Ubuntu 24.04 LTS / Windows 11 |

### Performance Measurement Baseline
For reproducible benchmark results (see ADR-006):

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| CPU | 6 Cores, 3.5 GHz | 8+ Cores, 4.0+ GHz |
| RAM | 16 GB DDR4 | 32 GB DDR4/DDR5 |
| Storage | SSD (SATA) | NVMe SSD |
| Compiler | GCC 13, Clang 18 | GCC 13 -O3 |
| Build Type | Release | Release with LTO |

**Note:** CI runners (GitHub Actions) do NOT meet the performance baseline.
Performance gates are informational only on CI. Run benchmarks locally on
dedicated hardware for accurate measurements.

## Quick Start
Linux: `./scripts/build.sh linux-release`
Windows: `scripts\\build.bat windows-release`
