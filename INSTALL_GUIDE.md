# THE SEED V13.2 — Installations-Guide (VOLLSTÄNDIG)

## Neue Ordner erstellen

```bash
mkdir -p network
mkdir -p server/auth
mkdir -p server/network
mkdir -p server/ai
```

## Dateien kopieren

### ROOT (Ersetzen)
```bash
cp vcpkg.json ./
cp CMakeLists.txt ./
cp main.cpp ./
```

### core/ (Ersetzen)
```bash
cp ECS.h core/
```

### ecs/ (Neu)
```bash
cp Components.h ecs/
```

### network/ (Neu)
```bash
cp network_UdpSocket.h network/
cp network_UdpSocket.cpp network/
cp network_ReliableUdp.h network/
cp network_ReliableUdp.cpp network/
cp network_NetworkServer.h network/
cp network_NetworkServer.cpp network/
```

### server/ (Ersetzen)
```bash
cp Server.h server/
cp Server.cpp server/
cp ThreadPool.h server/
cp ThreadPool.cpp server/
```

### server/auth/ (Neu)
```bash
cp AuthService.h server/auth/
cp AuthService.cpp server/auth/
```

### server/network/ (Neu)
```bash
cp Snapshot.h server/network/
cp Snapshot.cpp server/network/
```

### server/ai/ (Neu)
```bash
cp AIBehavior.h server/ai/
cp AIBehavior.cpp server/ai/
```

### client/ (Neu)
```bash
cp Interpolation.h client/
cp Interpolation.cpp client/
```

### renderer_vulkan/ (Neu/Ersetzen)
```bash
cp VulkanRenderer.h renderer_vulkan/
cp VulkanRenderer.cpp renderer_vulkan/
cp Mesh.h renderer_vulkan/
cp Mesh.cpp renderer_vulkan/
cp Texture.h renderer_vulkan/
cp Texture.cpp renderer_vulkan/
cp UniformBuffer.h renderer_vulkan/
cp UniformBuffer.cpp renderer_vulkan/
```

## Build

```bash
vcpkg install --triplet x64-windows
mkdir build && cd build
cmake .. -DENGINE_ENABLE_VULKAN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Implementierte Arbeitspakete

| AP | Datei | Beschreibung |
|----|-------|-------------|
| AP-17 | vcpkg.json, CMakeLists.txt | Build-System |
| AP-20 | ecs/Components.h | Game Components |
| AP-23 | core/ECS.h | Legacy-Adapter |
| AP-32 | network/* | UDP Socket + Server |
| AP-33 | network_ReliableUdp.* | Reliable UDP |
| AP-37 | server/network/Snapshot.* | World Snapshots |
| AP-38 | client/Interpolation.* | Client Interpolation |
| AP-42 | server/ThreadPool.* | Multi-Threading |
| AP-45 | server/auth/AuthService.* | JWT + Argon2id |
| AP-47-49 | server/ai/AIBehavior.* | AI Behavior Trees |
| AP-01 | renderer_vulkan/VulkanRenderer.* | Vulkan Triangle |
| AP-02 | renderer_vulkan/Mesh.* | Vertex/Index Buffers |
| AP-03 | renderer_vulkan/Texture.* | Texture Loading |
| AP-04 | renderer_vulkan/UniformBuffer.* | UBO + Descriptors |
