#pragma once
// =============================================================================
// renderer_vulkan/VulkanRenderer.h — Vulkan Renderer Backend (AP-01)
// Triangle rendering with vk-bootstrap, replacing OpenGL legacy
// =============================================================================
#include <vulkan/vulkan.h>
#include <vk-bootstrap/VkBootstrap.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

struct GLFWwindow;

namespace render {

// =============================================================================
// Vulkan Renderer Configuration
// =============================================================================
struct VulkanConfig {
    bool enableValidation = true;
    bool enableDebugMessenger = true;
    bool vsync = true;
    uint32_t desiredImageCount = 3;
    VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
};

// =============================================================================
// Vulkan Renderer
// =============================================================================
class VulkanRenderer {
public:
    struct FrameData {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence renderFence = VK_NULL_HANDLE;
        VkSemaphore presentSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    };

private:
    GLFWwindow* window = nullptr;
    uint32_t width = 0, height = 0;

    // vk-bootstrap instances
    vkb::Instance vkbInstance;
    vkb::Device vkbDevice;
    vkb::Swapchain vkbSwapchain;

    // Core Vulkan handles
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily = 0;

    // Swapchain images
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    // Render pass & pipeline
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers;

    // Command pool & buffers
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // Synchronization
    VkFence immediateFence = VK_NULL_HANDLE;
    VkSemaphore immediateSemaphore = VK_NULL_HANDLE;

    // Frame data (double/triple buffering)
    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frames;
    uint32_t currentFrame = 0;

    // Debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // Config
    VulkanConfig config;
    bool initialized = false;

public:
    VulkanRenderer() = default;
    ~VulkanRenderer() { Shutdown(); }

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // ===================================================================
    // Lifecycle
    // ===================================================================
    [[nodiscard]] bool Init(GLFWwindow* win, uint32_t w, uint32_t h, const VulkanConfig& cfg = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return initialized; }

    // ===================================================================
    // Frame Operations
    // ===================================================================
    void BeginFrame();
    void EndFrame();
    void Present();

    // ===================================================================
    // Rendering Commands
    // ===================================================================
    void BeginRenderPass(VkCommandBuffer cmd, uint32_t imageIndex);
    void EndRenderPass(VkCommandBuffer cmd);
    void BindPipeline(VkCommandBuffer cmd);
    void DrawTriangle(VkCommandBuffer cmd);  // AP-01: Basic triangle
    void DrawIndexed(VkCommandBuffer cmd, uint32_t indexCount);
    void SetViewport(VkCommandBuffer cmd, float x, float y, float w, float h);
    void SetScissor(VkCommandBuffer cmd, int32_t x, int32_t y, uint32_t w, uint32_t h);

    // ===================================================================
    // Resource Management
    // ===================================================================
    [[nodiscard]] VkShaderModule CreateShaderModule(std::span<const uint32_t> spirvCode);
    void DestroyShaderModule(VkShaderModule module);

    // Immediate submit (for resource uploads)
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    // ===================================================================
    // Resize
    // ===================================================================
    void Resize(uint32_t w, uint32_t h);

    // ===================================================================
    // Getters
    // ===================================================================
    [[nodiscard]] VkDevice GetDevice() const { return device; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
    [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const { return frames[currentFrame].commandBuffer; }
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return currentFrame; }
    [[nodiscard]] VkRenderPass GetRenderPass() const { return renderPass; }
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const { return swapchainExtent; }

private:
    // Init steps
    [[nodiscard]] bool CreateInstance();
    [[nodiscard]] bool CreateSurface();
    [[nodiscard]] bool SelectPhysicalDevice();
    [[nodiscard]] bool CreateLogicalDevice();
    [[nodiscard]] bool CreateSwapchain();
    [[nodiscard]] bool CreateImageViews();
    [[nodiscard]] bool CreateRenderPass();
    [[nodiscard]] bool CreateFramebuffers();
    [[nodiscard]] bool CreateCommandPool();
    [[nodiscard]] bool CreateCommandBuffers();
    [[nodiscard]] bool CreateSyncObjects();
    [[nodiscard]] bool CreatePipeline();  // AP-01: Basic triangle pipeline

    // Cleanup helpers
    void CleanupSwapchain();
    void RecreateSwapchain();

    // Debug
    [[nodiscard]] bool SetupDebugMessenger();
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};

} // namespace render
