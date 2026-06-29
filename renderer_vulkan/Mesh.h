#pragma once
// =============================================================================
// renderer_vulkan/Mesh.h — Vertex/Index Buffer Management (AP-02)
// =============================================================================
#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <string>
#include <memory>

namespace render {

// =============================================================================
// Vertex Format (matches shader layout)
// =============================================================================
struct Vertex {
    float position[3];
    float normal[3];
    float texCoord[2];
    float color[4];

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions();
};

// =============================================================================
// GPU Buffer (Vertex/Index/Uniform)
// =============================================================================
class GpuBuffer {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    bool isStaging = false;

public:
    GpuBuffer() = default;
    ~GpuBuffer() { Destroy(); }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    GpuBuffer(GpuBuffer&& other) noexcept;
    GpuBuffer& operator=(GpuBuffer&& other) noexcept;

    [[nodiscard]] bool Create(VkDevice dev, VkPhysicalDevice physDev, 
                               VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags,
                               VkMemoryPropertyFlags memProps);
    void Destroy();

    [[nodiscard]] VkBuffer GetBuffer() const { return buffer; }
    [[nodiscard]] VkDeviceMemory GetMemory() const { return memory; }
    [[nodiscard]] VkDeviceSize GetSize() const { return size; }

    // Upload data to buffer (requires host-visible memory)
    [[nodiscard]] bool Upload(std::span<const uint8_t> data);

    // Copy from this buffer to another (requires transfer queue)
    void CopyTo(VkCommandBuffer cmd, VkBuffer dst, VkDeviceSize dstOffset = 0);
};

// =============================================================================
// Mesh — Vertex + Index Buffer pair
// =============================================================================
class Mesh {
    std::unique_ptr<GpuBuffer> vertexBuffer;
    std::unique_ptr<GpuBuffer> indexBuffer;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool indexed = false;

public:
    Mesh() = default;
    ~Mesh() = default;

    // Create from CPU vertex/index data
    [[nodiscard]] bool CreateFromData(VkDevice device, VkPhysicalDevice physDev,
                                       VkCommandBuffer transferCmd,
                                       std::span<const Vertex> vertices,
                                       std::span<const uint32_t> indices = {});

    // Create primitive shapes
    [[nodiscard]] bool CreateTriangle(VkDevice device, VkPhysicalDevice physDev, VkCommandBuffer cmd);
    [[nodiscard]] bool CreateQuad(VkDevice device, VkPhysicalDevice physDev, VkCommandBuffer cmd);
    [[nodiscard]] bool CreateCube(VkDevice device, VkPhysicalDevice physDev, VkCommandBuffer cmd);
    [[nodiscard]] bool CreatePlane(VkDevice device, VkPhysicalDevice physDev, VkCommandBuffer cmd,
                                     uint32_t subdivisions = 1);

    void Bind(VkCommandBuffer cmd);
    void Draw(VkCommandBuffer cmd);

    [[nodiscard]] uint32_t GetVertexCount() const { return vertexCount; }
    [[nodiscard]] uint32_t GetIndexCount() const { return indexCount; }
    [[nodiscard]] bool IsIndexed() const { return indexed; }

    void Destroy();
};

// =============================================================================
// Mesh Manager (asset caching)
// =============================================================================
class MeshManager {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice = VK_NULL_HANDLE;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> cache;

public:
    void Init(VkDevice dev, VkPhysicalDevice phys) { device = dev; physDevice = phys; }

    [[nodiscard]] std::shared_ptr<Mesh> Load(const std::string& name);
    [[nodiscard]] std::shared_ptr<Mesh> GetOrCreate(const std::string& name,
        std::function<void(Mesh&)> creator);

    void Clear();
};

} // namespace render
