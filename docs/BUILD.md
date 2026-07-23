# Build-Anleitung

## Voraussetzungen

- CMake 3.25+
- Ninja
- C++20 Compiler (GCC 13+, Clang 18+, MSVC 17+)
- Git

## Erst-Setup

```bash
./scripts/setup.sh
```

Dies initialisiert Submodules, vcpkg und installiert Dependencies.

## Bauen

```bash
# Linux Release (default)
./scripts/build.sh linux-release

# Linux Debug mit Sanitizers
./scripts/build.sh linux-debug

# Windows Release
./scripts/build.sh windows-release
```

Oder direkt mit CMake:

```bash
cmake --preset linux-release
cmake --build build/linux-release --parallel
```

## Testen

```bash
# Alle Tests
./scripts/test.sh

# Nur Unit-Tests
./scripts/test.sh unit

# Mit Debug + Sanitizers
./scripts/test.sh ".*" linux-debug
```

## Presets

| Preset | Plattform | Build-Type | Sanitizers |
|--------|-----------|------------|------------|
| `linux-release` | Linux | Release | Nein |
| `linux-debug` | Linux | Debug | Ja |
| `windows-release` | Windows | Release | Nein |
| `windows-debug` | Windows | Debug | Nein |
