# Math & Utilities – TheSeed Engine

## Overview

Mathematical building blocks: Vec3/Quaternion/Matrix, deterministic PCG32 random, fast hash functions (FNV1a), UUID v4, JSON config with hot-reload. All SIMD-ready with scalar fallback.

## Math (`seed::math`)

| Type | File | Operations |
|------|------|------------|
| `Vec3` | `math/vec3.h` | +, -, *, /, dot, cross, length, normalize |
| `Quat` | `math/quat.h` | fromAxisAngle |
| `Mat4` | `math/mat4.h` | *, perspective, lookAt, toMatrix |

```cpp
seed::math::Vec3 a(1,2,3), b(4,5,6);
auto c = a + b;
float d = seed::math::dot(a, b);
auto m = seed::math::perspective(fov, aspect, near, far);
```

## Utilities (`seed::util`)

| Component | File | Purpose |
|-----------|------|---------|
| `Pcg32Random` | `util/random.h/.cpp` | Deterministic, seedable random |
| `fnv1a32/64` | `util/hash.h/.cpp` | Fast hash functions |
| `UUID` | `util/uuid.h/.cpp` | UUID v4 generation |
| `Config` | `util/config.h/.cpp` | JSON config with hot-reload |

```cpp
seed::util::Pcg32Random rng(42);
auto val = rng.nextInt(0, 100);

auto uuid = seed::util::UUID::generate();

seed::util::Config cfg("config.json");
cfg.load();
auto fps = cfg.getInt("target_fps", 60);
```

## Performance Targets

| Metric | Target | Test |
|--------|--------|------|
| Mat4 multiply | <10ns | Benchmark |
| PCG32 | Deterministic, period > 2^32 | `Property_Random_*` |
| UUID | 1M/sec, no collisions | `Property_UUID_Unique` |
| Config reload | <100ms | Manual |

## Testing

```bash
./scripts/test.sh Math
./scripts/test.sh Random
./scripts/test.sh UUID
```

## Deliverables

- `seed-core/src/core/math/vec3.h`
- `seed-core/src/core/math/quat.h`
- `seed-core/src/core/math/mat4.h`
- `seed-core/src/core/util/random.h/.cpp`
- `seed-core/src/core/util/hash.h/.cpp`
- `seed-core/src/core/util/uuid.h/.cpp`
- `seed-core/src/core/util/config.h/.cpp`
- Tests: unit, property, benchmarks
