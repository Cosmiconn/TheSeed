#pragma once
// =============================================================================
// renderer_vulkan/VulkanContext.h — Vulkan Instance, Device, Queues
// AP-01: Vulkan-Renderer-Init
// =============================================================================
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace vulkan {

struct QueueFamilyIndices {
    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t presentFamily = UINT32_MAX;
    bool graphicsFound = false;
    bool presentFound = false;
    
    [[nodiscard]] bool IsComplete() const { return graphicsFound && presentFound; }
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    [[nodiscard]] bool Initialize(const std::string& appName, uint32_t appVersion, void* windowHandle);
    void Shutdown();

    [[nodiscard]] VkInstance GetInstance() const { return instance; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
    [[nodiscard]] VkDevice GetDevice() const { return device; }
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return graphicsQueue; }
    [[nodiscard]] VkQueue GetPresentQueue() const { return presentQueue; }
    [[nodiscard]] QueueFamilyIndices GetQueueFamilies() const { return queueFamilies; }
    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const { return queueFamilies.graphicsFamily; }
    [[nodiscard]] uint32_t GetPresentQueueFamily() const { return queueFamilies.presentFamily; }

    [[nodiscard]] bool IsValidationEnabled() const { return validationEnabled; }

private:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilies;
    
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool validationEnabled = false;

    std::vector<const char*> instanceExtensions;
    std::vector<const char*> deviceExtensions;
    std::vector<const char*> validationLayers;

    [[nodiscard]] bool CreateInstance(const std::string& appName, uint32_t appVersion);
    [[nodiscard]] bool SetupDebugMessenger();
    [[nodiscard]] bool PickPhysicalDevice();
    [[nodiscard]] bool CreateLogicalDevice();
    [[nodiscard]] QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    
    [[nodiscard]] bool CheckValidationLayerSupport();
    [[nodiscard]] std::vector<const char*> GetRequiredExtensions(void* windowHandle);
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};

} // namespace vulkan
