#pragma once
// =============================================================================
// renderer_vulkan/ShadowMap.h — Cascaded Shadow Maps (AP-10)
// =============================================================================
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>
#include <memory>

namespace render {

// =============================================================================
// Shadow Map Configuration
// =============================================================================
struct ShadowConfig {
    static constexpr uint32_t CASCADE_COUNT = 4;
    static constexpr uint32_t SHADOW_MAP_SIZE = 2048;

    float cascadeSplits[CASCADE_COUNT] = {0.1f, 0.25f, 0.5f, 1.0f};
    float cascadeDistances[CASCADE_COUNT] = {10.0f, 25.0f, 50.0f, 100.0f};
    float bias = 0.005f;
    float normalBias = 0.02f;
    bool usePCF = true;
    uint32_t pcfSamples = 16;
};

// =============================================================================
// Cascade Data
// =============================================================================
struct Cascade {
    glm::mat4 viewProj;
    glm::mat4 view;
    glm::mat4 proj;
    float splitDepth;
    float farPlane;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};

// =============================================================================
// Shadow Map Renderer
// =============================================================================
class ShadowMapRenderer {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice = VK_NULL_HANDLE;

    // Shadow pass resources
    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkPipeline shadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;

    // Shadow map image (array for cascades)
    VkImage shadowImage = VK_NULL_HANDLE;
    VkDeviceMemory shadowMemory = VK_NULL_HANDLE;
    VkImageView shadowImageView = VK_NULL_HANDLE;
    VkSampler shadowSampler = VK_NULL_HANDLE;

    // Per-cascade data
    std::array<Cascade, ShadowConfig::CASCADE_COUNT> cascades;

    // UBO for shadow matrices
    struct ShadowUBO {
        alignas(16) glm::mat4 viewProjMatrices[ShadowConfig::CASCADE_COUNT];
        alignas(16) glm::vec4 cascadeSplits;
        alignas(4) float bias;
        alignas(4) float normalBias;
        alignas(4) int usePCF;
        alignas(4) int pcfSamples;
    };

    ShadowConfig config;
    bool initialized = false;

public:
    ShadowMapRenderer() = default;
    ~ShadowMapRenderer() { Destroy(); }

    ShadowMapRenderer(const ShadowMapRenderer&) = delete;
    ShadowMapRenderer& operator=(const ShadowMapRenderer&) = delete;

    [[nodiscard]] bool Init(VkDevice dev, VkPhysicalDevice physDev,
                            VkDescriptorSetLayout globalLayout);
    void Destroy();
    [[nodiscard]] bool IsInitialized() const { return initialized; }

    // Update cascade matrices based on camera and light direction
    void UpdateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj,
                         const glm::vec3& lightDir,
                         float cameraNear, float cameraFar);

    // Begin shadow pass for cascade
    void BeginCascadePass(VkCommandBuffer cmd, uint32_t cascadeIndex);
    void EndCascadePass(VkCommandBuffer cmd);

    // Bind shadow pipeline
    void BindShadowPipeline(VkCommandBuffer cmd);

    // Get shadow data for lighting pass
    [[nodiscard]] VkImageView GetShadowImageView() const { return shadowImageView; }
    [[nodiscard]] VkSampler GetShadowSampler() const { return shadowSampler; }
    [[nodiscard]] const Cascade& GetCascade(uint32_t index) const { return cascades[index]; }
    [[nodiscard]] uint32_t GetCascadeCount() const { return ShadowConfig::CASCADE_COUNT; }
    [[nodiscard]] const ShadowUBO GetShadowUBO() const;

    // Get cascade index for a given view-space depth
    [[nodiscard]] uint32_t GetCascadeIndex(float viewSpaceDepth) const;

private:
    [[nodiscard]] bool CreateShadowRenderPass();
    [[nodiscard]] bool CreateShadowImage();
    [[nodiscard]] bool CreateShadowPipeline(VkDescriptorSetLayout globalLayout);
    [[nodiscard]] bool CreateFramebuffers();
    [[nodiscard]] bool CreateSampler();

    void CalculateCascadeMatrices(const glm::mat4& cameraView, const glm::mat4& cameraProj,
                                   const glm::vec3& lightDir,
                                   float nearPlane, float farPlane,
                                   Cascade& cascade);

    [[nodiscard]] glm::mat4 CreateLightViewMatrix(const glm::vec3& lightDir, 
                                                     const glm::vec3& center, float radius);

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
};

} // namespace render
