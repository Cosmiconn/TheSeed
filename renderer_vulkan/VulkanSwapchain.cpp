#include "VulkanSwapchain.h"
#include <algorithm>
#include <iostream>

namespace vulkan {

VulkanSwapchain::VulkanSwapchain(VulkanContext& context) : ctx(context) {}

VulkanSwapchain::~VulkanSwapchain() {
    Shutdown();
}

bool VulkanSwapchain::Initialize(void* windowHandle, uint32_t width, uint32_t height) {
    if (!CreateSurface(windowHandle)) return false;
    
    auto support = QuerySwapchainSupport();
    if (support.formats.empty() || support.presentModes.empty()) {
        std::cerr << "[Vulkan] Swapchain support incomplete\n";
        return false;
    }

    imageFormat = ChooseSwapSurfaceFormat(support.formats).format;
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(support.presentModes);
    extent = ChooseSwapExtent(support.capabilities, width, height);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat;
    createInfo.imageColorSpace = ChooseSwapSurfaceFormat(support.formats).colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32_t queueFamilyIndices[] = {ctx.GetGraphicsQueueFamily(), ctx.GetPresentQueueFamily()};
    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(ctx.GetDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create swapchain\n";
        return false;
    }

    vkGetSwapchainImagesKHR(ctx.GetDevice(), swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(ctx.GetDevice(), swapchain, &imageCount, images.data());

    return CreateImageViews();
}

void VulkanSwapchain::Shutdown() {
    for (auto fb : framebuffers) {
        if (fb) vkDestroyFramebuffer(ctx.GetDevice(), fb, nullptr);
    }
    framebuffers.clear();
    
    for (auto view : imageViews) {
        if (view) vkDestroyImageView(ctx.GetDevice(), view, nullptr);
    }
    imageViews.clear();
    
    if (swapchain) {
        vkDestroySwapchainKHR(ctx.GetDevice(), swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    
    if (surface) {
        vkDestroySurfaceKHR(ctx.GetInstance(), surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::Recreate(uint32_t width, uint32_t height) {
    Shutdown();
    Initialize(nullptr, width, height);
}

uint32_t VulkanSwapchain::AcquireNextImage(VkSemaphore signalSemaphore) {
    uint32_t imageIndex;
    vkAcquireNextImageKHR(ctx.GetDevice(), swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &imageIndex);
    return imageIndex;
}

bool VulkanSwapchain::Present(uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    
    return vkQueuePresentKHR(ctx.GetPresentQueue(), &presentInfo) == VK_SUCCESS;
}

bool VulkanSwapchain::CreateSurface(void* windowHandle) {
    // Platform-specific: simplified for GLFW
    // Real implementation needs VkWin32SurfaceCreateInfoKHR or VkXlibSurfaceCreateInfoKHR
    std::cerr << "[Vulkan] CreateSurface: Platform-specific implementation needed\n";
    return false; // Placeholder
}

SwapchainSupportDetails VulkanSwapchain::QuerySwapchainSupport() {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.GetPhysicalDevice(), surface, &details.capabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.GetPhysicalDevice(), surface, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.GetPhysicalDevice(), surface, &formatCount, details.formats.data());
    }
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.GetPhysicalDevice(), surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.GetPhysicalDevice(), surface, &presentModeCount, details.presentModes.data());
    }
    
    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    
    VkExtent2D extent{w, h};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

bool VulkanSwapchain::CreateImageViews() {
    imageViews.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(ctx.GetDevice(), &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
            std::cerr << "[Vulkan] Failed to create image view " << i << "\n";
            return false;
        }
    }
    return true;
}

bool VulkanSwapchain::CreateFramebuffers(VkRenderPass renderPass) {
    framebuffers.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); ++i) {
        VkImageView attachments[] = {imageViews[i]};
        
        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = extent.width;
        createInfo.height = extent.height;
        createInfo.layers = 1;

        if (vkCreateFramebuffer(ctx.GetDevice(), &createInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

} // namespace vulkan
