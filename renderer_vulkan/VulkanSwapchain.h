#pragma once
// =============================================================================
// renderer_vulkan/VulkanSwapchain.h — Swapchain, Surface, Present
// =============================================================================
#include "VulkanContext.h"
#include <vector>

namespace vulkan {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanContext& context);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    [[nodiscard]] bool Initialize(void* windowHandle, uint32_t width, uint32_t height);
    void Shutdown();
    void Recreate(uint32_t width, uint32_t height);

    [[nodiscard]] VkSwapchainKHR GetSwapchain() const { return swapchain; }
    [[nodiscard]] VkFormat GetImageFormat() const { return imageFormat; }
    [[nodiscard]] VkExtent2D GetExtent() const { return extent; }
    [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(images.size()); }
    [[nodiscard]] VkImageView GetImageView(uint32_t index) const { return imageViews[index]; }
    [[nodiscard]] VkFramebuffer GetFramebuffer(uint32_t index) const { return framebuffers[index]; }

    [[nodiscard]] uint32_t AcquireNextImage(VkSemaphore signalSemaphore);
    [[nodiscard]] bool Present(uint32_t imageIndex, VkSemaphore waitSemaphore);

private:
    VulkanContext& ctx;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    [[nodiscard]] bool CreateSurface(void* windowHandle);
    [[nodiscard]] SwapchainSupportDetails QuerySwapchainSupport();
    [[nodiscard]] VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    [[nodiscard]] VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes);
    [[nodiscard]] VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h);
    [[nodiscard]] bool CreateImageViews();
    [[nodiscard]] bool CreateFramebuffers(VkRenderPass renderPass);
};

} // namespace vulkan
