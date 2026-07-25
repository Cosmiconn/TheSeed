# Serialization – TheSeed Engine

## Overview

Fast binary serialization with delta compression for network replication. Reflection-based automatic component serialization. Endianness: Little-Endian.

## Architecture

```
[Component Data] -> [BinaryWriter] -> [Binary Stream]
                              |
                         [Delta Compressor] -> [Network Packet]
```

## Key Components

| Component | File | Purpose |
|-----------|------|---------|
| `BinaryWriter` | `binary_writer.h/.cpp` | Write primitives/structs to byte stream |
| `BinaryReader` | `binary_reader.h/.cpp` | Read primitives/structs from byte stream |
| `Snapshot` | `snapshot.h/.cpp` | Full world state capture |
| `Delta` | `delta.h/.cpp` | Delta compression between snapshots |
| `Reflection` | `reflection.h/.cpp` | Type metadata for automatic serialization |

## API

```cpp
// Binary I/O
seed::serialize::BinaryWriter writer;
writer.writePOD(42);
writer.writePOD(3.14f);
auto data = writer.data();

seed::serialize::BinaryReader reader(data.data(), data.size());
int val; reader.readPOD(val);

// World snapshot
auto snapshot = world.snapshot();
world.restore(snapshot);
```

## Performance Targets

| Metric | Target | Test |
|--------|--------|------|
| 100k Entities serialize | <10ms | `gate_p0_serialize_speed` |
| Delta compression | 1% change → <50KB | Benchmark |
| Roundtrip correctness | 100% | Unit tests |

## Testing

```bash
./scripts/test.sh Serialize
```

## Deliverables

- `seed-core/src/core/serialize/binary_writer.h/.cpp`
- `seed-core/src/core/serialize/binary_reader.h/.cpp`
- `seed-core/src/core/serialize/snapshot.h/.cpp`
- `seed-core/src/core/serialize/delta.h/.cpp`
- `seed-core/src/core/serialize/reflection.h/.cpp`
- Tests: unit, integration, benchmarks
