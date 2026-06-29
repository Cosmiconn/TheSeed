#pragma once
// =============================================================================
// renderer_vulkan/UniformBuffer.h — UBO + Descriptor Sets (AP-04)
// Per-frame UBOs for MVP matrices, lights, materials
// =============================================================================
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>
#include <memory>

namespace render {

// =============================================================================
// Shader Uniforms (matches shader layout)
// =============================================================================
struct GlobalUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
    alignas(16) glm::vec3 sunDirection;
    alignas(16) glm::vec3 sunColor;
    alignas(4) float time;
    alignas(4) float deltaTime;
};

struct ObjectUBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 normalMatrix;
    alignas(16) glm::vec4 color;
    alignas(4) float metallic;
    alignas(4) float roughness;
    alignas(4) float ao;
};

struct MaterialUBO {
    alignas(16) glm::vec4 albedoColor;
    alignas(4) float metallic;
    alignas(4) float roughness;
    alignas(4) float ao;
    alignas(4) uint32_t useAlbedoMap;
    alignas(4) uint32_t useNormalMap;
    alignas(4) uint32_t useMetallicMap;
    alignas(4) uint32_t useRoughnessMap;
    alignas(4) uint32_t useAOMap;
};

// =============================================================================
// Uniform Buffer (GPU memory for shader uniforms)
// =============================================================================
class UniformBuffer {
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;

public:
    UniformBuffer() = default;
    ~UniformBuffer() { Destroy(); }

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

    [[nodiscard]] bool Create(VkDevice dev, VkPhysicalDevice physDev, VkDeviceSize bufferSize);
    void Destroy();

    // Update data (host-visible, coherent)
    void Update(const void* data, VkDeviceSize dataSize);
    void Update(const GlobalUBO& ubo) { Update(&ubo, sizeof(ubo)); }
    void Update(const ObjectUBO& ubo) { Update(&ubo, sizeof(ubo)); }
    void Update(const MaterialUBO& ubo) { Update(&ubo, sizeof(ubo)); }

    [[nodiscard]] VkBuffer GetBuffer() const { return buffer; }
    [[nodiscard]] VkDeviceSize GetSize() const { return size; }
};

// =============================================================================
// Descriptor Set Layout Builder
// =============================================================================
class DescriptorSetLayoutBuilder {
    VkDevice device = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

public:
    explicit DescriptorSetLayoutBuilder(VkDevice dev) : device(dev) {}

    DescriptorSetLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type,
                                            uint32_t count, VkShaderStageFlags stages,
                                            const VkSampler* immutableSamplers = nullptr);

    [[nodiscard]] VkDescriptorSetLayout Build() const;
};

// =============================================================================
// Descriptor Pool
// =============================================================================
class DescriptorPool {
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    uint32_t maxSets = 1000;

public:
    DescriptorPool() = default;
    ~DescriptorPool() { Destroy(); }

    [[nodiscard]] bool Create(VkDevice dev, uint32_t maxSets = 1000);
    void Destroy();

    [[nodiscard]] VkDescriptorSet Allocate(VkDescriptorSetLayout layout);
    void Free(VkDescriptorSet set);
    void Reset();

    [[nodiscard]] VkDescriptorPool GetPool() const { return pool; }
};

// =============================================================================
// Descriptor Writer (convenience for updating descriptor sets)
// =============================================================================
class DescriptorWriter {
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

public:
    DescriptorWriter(VkDevice dev, VkDescriptorSet descriptorSet) 
        : device(dev), set(descriptorSet) {}

    DescriptorWriter& WriteBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                   VkDescriptorType type, VkDeviceSize offset = 0);
    DescriptorWriter& WriteImage(uint32_t binding, VkImageView view, VkSampler sampler,
                                  VkDescriptorType type, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void Apply();
};

// =============================================================================
// Per-Frame Descriptor Manager
// =============================================================================
class FrameDescriptorManager {
    static constexpr size_t MAX_FRAMES = 2;

    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice = VK_NULL_HANDLE;

    std::array<std::unique_ptr<UniformBuffer>, MAX_FRAMES> globalUBOs;
    std::array<std::unique_ptr<UniformBuffer>, MAX_FRAMES> objectUBOs;
    std::array<std::unique_ptr<UniformBuffer>, MAX_FRAMES> materialUBOs;

    VkDescriptorSetLayout globalLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout objectLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialLayout = VK_NULL_HANDLE;

    std::array<VkDescriptorSet, MAX_FRAMES> globalSets{};
    std::array<VkDescriptorSet, MAX_FRAMES> objectSets{};
    std::array<VkDescriptorSet, MAX_FRAMES> materialSets{};

    std::unique_ptr<DescriptorPool> descriptorPool;

public:
    FrameDescriptorManager() = default;
    ~FrameDescriptorManager() { Destroy(); }

    [[nodiscard]] bool Init(VkDevice dev, VkPhysicalDevice physDev);
    void Destroy();

    // Update UBOs for current frame
    void UpdateGlobal(uint32_t frameIndex, const GlobalUBO& ubo);
    void UpdateObject(uint32_t frameIndex, const ObjectUBO& ubo);
    void UpdateMaterial(uint32_t frameIndex, const MaterialUBO& ubo);

    // Get descriptor sets for binding
    [[nodiscard]] VkDescriptorSet GetGlobalSet(uint32_t frameIndex) const { return globalSets[frameIndex]; }
    [[nodiscard]] VkDescriptorSet GetObjectSet(uint32_t frameIndex) const { return objectSets[frameIndex]; }
    [[nodiscard]] VkDescriptorSet GetMaterialSet(uint32_t frameIndex) const { return materialSets[frameIndex]; }

    [[nodiscard]] VkDescriptorSetLayout GetGlobalLayout() const { return globalLayout; }
    [[nodiscard]] VkDescriptorSetLayout GetObjectLayout() const { return objectLayout; }
    [[nodiscard]] VkDescriptorSetLayout GetMaterialLayout() const { return materialLayout; }

private:
    [[nodiscard]] bool CreateLayouts();
    [[nodiscard]] bool CreateUBOs();
    [[nodiscard]] bool AllocateSets();
    void BindUBOsToSets();
};

} // namespace render
