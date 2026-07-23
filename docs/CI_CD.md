# CI/CD

## Pipelines

### ci.yml
- **Trigger:** Push/PR auf `develop`/`main`
- **Matrix:** Linux (GCC 13, Clang 18) × (Release, Debug) + Windows (Release, Debug)
- **Dauer-Ziel:** < 10 Minuten

### nightly.yml
- **Trigger:** Täglich 02:00 UTC
- **Checks:** Sanitizer (ASan+UBSan), Coverage, Benchmarks

## Branching

```
main      ← Produktion, getaggte Releases
  |
develop   ← Integration, CI läuft hier
  |
feature/* ← Feature-Branches
```

## Commit-Konventionen

```
<type>(<scope>): <subject>

<body>

Refs: #<issue>
```

| Type | Bedeutung |
|------|-----------|
| `feat` | Neue Funktion |
| `fix` | Bugfix |
| `test` | Tests |
| `perf` | Performance |
| `refactor` | Refactoring |
| `docs` | Dokumentation |
| `chore` | Build/CI |
