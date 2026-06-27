// =============================================================================
// client/ClientTick.cpp — Client Tick Implementation (STUB)
// =============================================================================
#include "ClientTick.h"
#include "../core/Log.h"
#include "../core/World.h"
#include "../core/ECS.h"
#include "../core/ByteBuffer.h"
#include "../core/Types.h"
#include "Connection.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>

void RendererInit(GLuint& grassTex, GLuint& rockTex) {
    // Minimal OpenGL init - will be replaced by AP-04
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create simple colored textures
    GLuint textures[2];
    glGenTextures(2, textures);
    grassTex = textures[0];
    rockTex = textures[1];

    // Grass texture (green)
    uint8_t grassData[4] = {34, 139, 34, 255};
    glBindTexture(GL_TEXTURE_2D, grassTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, grassData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Rock texture (gray)
    uint8_t rockData[4] = {128, 128, 128, 255};
    glBindTexture(GL_TEXTURE_2D, rockTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rockData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    AddLog("[Renderer] OpenGL legacy initialized.");
}

void RendererShutdown(GLuint& grassTex, GLuint& rockTex) {
    GLuint textures[2] = {grassTex, rockTex};
    glDeleteTextures(2, textures);
    grassTex = 0;
    rockTex = 0;
}

void ProcessClientTick() {
    // Process incoming server packets
    if (clientSocket == INVALID_SOCKET) return;

    static std::vector<uint8_t> tcpBuffer;
    char chunk[2048];
    int rc = recv(clientSocket, chunk, sizeof(chunk), 0);
    if (rc > 0) {
        tcpBuffer.insert(tcpBuffer.end(), chunk, chunk + rc);
    }

    while (tcpBuffer.size() >= 2) {
        uint16_t pLen = (static_cast<uint16_t>(tcpBuffer[0]) << 8) | static_cast<uint16_t>(tcpBuffer[1]);
        if (tcpBuffer.size() < static_cast<size_t>(2 + pLen)) break;

        std::vector<uint8_t> payload(tcpBuffer.begin() + 2, tcpBuffer.begin() + 2 + pLen);
        tcpBuffer.erase(tcpBuffer.begin(), tcpBuffer.begin() + 2 + pLen);

        // Process packet - minimal implementation
        ByteBuffer buf{std::span<const uint8_t>(payload)};
        try {
            uint8_t op = buf.ReadUInt8();
            switch (op) {
                case std::to_underlying(PacketType::MSG_ENTITY_SPAWN): {
                    uint32_t id = buf.ReadUInt32();
                    std::string name = buf.ReadString();
                    uint8_t isMonster = buf.ReadUInt8();
                    float x = buf.ReadFloat();
                    float z = buf.ReadFloat();
                    std::string mat = buf.ReadString();
                    float scale = buf.ReadFloat();
                    std::string mesh = buf.ReadString();

                    // Add to client registry
                    Entity e;
                    e.id = id;
                    e.name = name;
                    e.isMonster = isMonster != 0;
                    e.transform.x = e.transform.lerpX = x;
                    e.transform.z = e.transform.lerpZ = z;
                    e.render.materialId = mat;
                    e.render.scaleY = scale;
                    e.render.meshId = mesh;
                    e.transform.y = GetHeightFromGrid(x, z) + 0.5f;

                    auto it = std::ranges::find_if(clientRegistry,
                        [id](const Entity& ent){ return ent.id == id; });
                    if (it != clientRegistry.end()) *it = e;
                    else clientRegistry.push_back(e);
                    break;
                }
                case std::to_underlying(PacketType::MSG_ENTITY_DESPAWN): {
                    uint32_t id = buf.ReadUInt32();
                    clientRegistry.erase(
                        std::ranges::remove_if(clientRegistry,
                            [id](const Entity& e){ return e.id == id; }).begin(),
                        clientRegistry.end());
                    break;
                }
                case std::to_underlying(PacketType::MSG_MOVE_NOTIFY): {
                    uint32_t id = buf.ReadUInt32();
                    uint32_t seq = buf.ReadUInt32();
                    (void)seq;
                    float tx = buf.ReadFloat();
                    float tz = buf.ReadFloat();

                    auto it = std::ranges::find_if(clientRegistry,
                        [id](const Entity& e){ return e.id == id; });
                    if (it != clientRegistry.end()) {
                        it->transform.targetX = tx;
                        it->transform.targetZ = tz;
                    }
                    break;
                }
                case std::to_underlying(PacketType::MSG_COMBAT_NOTIFY): {
                    uint32_t id = buf.ReadUInt32();
                    uint32_t hp = buf.ReadUInt32();
                    auto it = std::ranges::find_if(clientRegistry,
                        [id](const Entity& e){ return e.id == id; });
                    if (it != clientRegistry.end()) it->currentHP = static_cast<int>(hp);
                    break;
                }
                case std::to_underlying(PacketType::MSG_SECTOR_SWITCH): {
                    uint32_t sx = buf.ReadUInt32();
                    uint32_t sz = buf.ReadUInt32();
                    float ex = buf.ReadFloat();
                    float ez = buf.ReadFloat();
                    currentSectorX = static_cast<int>(sx);
                    currentSectorZ = static_cast<int>(sz);
                    LoadHDTBinary(GetSectorName(currentSectorX, currentSectorZ) + ".hdt");
                    AddLog("[Client] Sector switch to ({}, {})", currentSectorX, currentSectorZ);
                    break;
                }
            }
        } catch (...) {
            // Invalid packet, ignore
        }
    }

    // Interpolate entity positions
    for (auto& ent : clientRegistry) {
        float dx = ent.transform.targetX - ent.transform.x;
        float dz = ent.transform.targetZ - ent.transform.z;
        float dist = std::sqrt(dx*dx + dz*dz);
        if (dist > 0.01f) {
            float speed = (ent.id == serverRegistry.empty() ? 0 : serverRegistry[0].id) 
                          ? LERP_SPEED_SELF : LERP_SPEED_REMOTE;
            float t = std::min(1.0f, speed * 0.016f);
            ent.transform.x += dx * t;
            ent.transform.z += dz * t;
        }
        ent.transform.y = GetHeightFromGrid(ent.transform.x, ent.transform.z) + 0.5f;
    }
}
