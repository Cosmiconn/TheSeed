#pragma once
// =============================================================================
// renderer_vulkan/Texture.h — Vulkan Texture Loading (AP-03)
// Supports: PNG, JPG, TGA, BMP via stb_image (header-only)
// =============================================================================
#include <vulkan/vulkan.h>
#include <string>
#include <span>
#include <memory>
#include <unordered_map>

namespace render {

// =============================================================================
// Vulkan Image (GPU texture)
// =============================================================================
class VulkanImage {
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    uint32_t width = 0, height = 0, mipLevels = 1;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

public:
    VulkanImage() = default;
    ~VulkanImage() { Destroy(); }

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    // Load from file (stb_image)
    [[nodiscard]] bool LoadFromFile(VkDevice dev, VkPhysicalDevice physDev,
                                     VkCommandBuffer transferCmd,
                                     const std::string& filepath,
                                     bool generateMips = true);

    // Load from raw RGBA data
    [[nodiscard]] bool LoadFromMemory(VkDevice dev, VkPhysicalDevice physDev,
                                       VkCommandBuffer transferCmd,
                                       std::span<const uint8_t> rgbaData,
                                       uint32_t w, uint32_t h,
                                       bool generateMips = true);

    // Create 1x1 solid color texture (for missing textures)
    [[nodiscard]] bool CreateSolidColor(VkDevice dev, VkPhysicalDevice physDev,
                                         VkCommandBuffer transferCmd,
                                         uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    void Destroy();

    [[nodiscard]] VkImageView GetView() const { return view; }
    [[nodiscard]] VkSampler GetSampler() const { return sampler; }
    [[nodiscard]] VkImage GetImage() const { return image; }
    [[nodiscard]] uint32_t GetWidth() const { return width; }
    [[nodiscard]] uint32_t GetHeight() const { return height; }
    [[nodiscard]] uint32_t GetMipLevels() const { return mipLevels; }

    // Transition image layout (for render passes)
    void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

private:
    [[nodiscard]] bool CreateImage(VkDeviceSize size, VkImageUsageFlags usage);
    [[nodiscard]] bool CreateImageView();
    [[nodiscard]] bool CreateSampler();
    void GenerateMipmaps(VkCommandBuffer cmd);

    [[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
};

// =============================================================================
// Texture Manager (asset caching)
// =============================================================================
class TextureManager {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice = VK_NULL_HANDLE;
    std::unordered_map<std::string, std::shared_ptr<VulkanImage>> cache;
    std::shared_ptr<VulkanImage> defaultTexture;

public:
    void Init(VkDevice dev, VkPhysicalDevice phys);

    [[nodiscard]] std::shared_ptr<VulkanImage> Load(VkCommandBuffer cmd, const std::string& filepath);
    [[nodiscard]] std::shared_ptr<VulkanImage> GetDefault();

    void Clear();
};

} // namespace render
