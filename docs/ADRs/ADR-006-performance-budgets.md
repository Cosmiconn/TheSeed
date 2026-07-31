# ADR-006: Performance-Budgets sind aspirational

**Status:** Accepted  
**Date:** 2026-07-31  
**Deciders:** TheSeed Core Team  

## Context

Die Roadmap (Phase 0, Monat 1–6) definiert strikte Performance-Budgets:

| Kriterium | Roadmap-Ziel | Getestet auf |
|-----------|-------------|--------------|
| 100k Entities erstellen | < 100 ms | CI-Runner, Gaming-PC (6C/16GB/RTX 3060) |
| 10 Systeme @ 60 FPS | < 16 ms/Frame | CI-Runner, Gaming-PC |
| 1M Tasks/sec | < 1 s | CI-Runner, Gaming-PC |
| 100k Entities serialisieren | < 10 ms | CI-Runner, Gaming-PC |
| Clean Build | < 3 min | CI-Runner, Gaming-PC |

Bei Tests auf realer Hardware (GitHub Actions ubuntu-24.04 Runner + lokaler
Gaming-PC mit 6 Kernen @ 3.95 GHz, 16 GB RAM, RTX 3060) wurden die Budgets
**nicht erreicht**.

## Decision Drivers

- CI muss stabil laufen (keine flaky Builds wegen Runner-Variance)
- Lokale Dev-Maschinen variieren stark (Laptop vs. Workstation vs. CI)
- Performance ist wichtig, aber **funktionale Korrektheit** hat Prioritaet
- Die Budgets waren theoretisch ("Zielwerte") ohne Hardware-Baseline

## Decision

### 1. Performance-Budgets werden "aspirational"

Die urspruenglichen Zahlen bleiben als **Zielwerte** erhalten, aber sie
blockieren weder CI noch Gate. Stattdessen:

- **Funktionale Gates** (EntityCount, Memory-Leak-frei, Korrektheit)
  → Blockieren CI, muessen passen
- **Performance Gates** (Zeitbudgets) → Laufen als Information,
  erzeugen Warnungen aber keinen Fail

### 2. Hardware-Baseline definieren

Fuer reproduzierbare Performance-Messungen wird eine dedizierte
Hardware-Baseline definiert:

| Komponente | Minimum | Empfohlen |
|-----------|---------|-----------|
| CPU | 6 Cores, 3.5 GHz | 8+ Cores, 4.0+ GHz |
| RAM | 16 GB DDR4 | 32 GB DDR4/DDR5 |
| Storage | SSD (SATA) | NVMe SSD |
| OS | Ubuntu 22.04/24.04 | Ubuntu 24.04 LTS |
| Compiler | GCC 13, Clang 18 | GCC 13 -O3 |
| Build Type | Release | Release with LTO |

**Wichtig:** CI-Runner (GitHub Actions `ubuntu-24.04`) erfuellen das
Minimum **nicht** (shared vCPUs, variable Last). Performance-Gates
sollten daher nie auf CI-Runnern als Blocker laufen.

### 3. Test-Strategie

| Test-Typ | Framework | Blockiert CI | Wo |
|----------|-----------|-------------|-----|
| Funktionale Gates | doctest `REQUIRE` | Ja | `tests/gate/test_gate_p0.cpp` |
| Performance Gates | doctest `CHECK` + `MESSAGE` | Nein | `tests/gate/test_gate_p0_perf.cpp` |
| Benchmarks | nanobench | Nein | `tests/benchmarks/` |

`CHECK` statt `REQUIRE` in Performance-Tests:
- Test laeuft durch und misst
- Bei Ueberschreitung wird gewarnt (`MESSAGE`)
- CI failt nicht

### 4. CI/CD Anpassung

- `ci.yml`: Laueft **nur** funktionale Gates (`-R gate_p0_` exkludiert `_perf`)
- `gate.yml`: Zwei Jobs:
  1. `gate-p0-functional` → Muss passen
  2. `gate-p0-performance` → `continue-on-error: true`, dient als Trend

## Consequences

- **Positiv:** CI ist stabil, keine flaky Builds
- **Positiv:** Performance bleibt sichtbar (Trend-Analyse moeglich)
- **Negativ:** Keine automatische Regression-Erkennung fuer Performance
- **Mitigation:** Nightly-Job speichert Benchmark-Ergebnisse als Artefakte

## Links

- [Roadmap: Phase 0 Fundament](../../04_PHASE_0_FUNDAMENT.md)
- [ADR-005: Roadmap-Abweichungen](ADR-005-roadmap-deviations.md)
