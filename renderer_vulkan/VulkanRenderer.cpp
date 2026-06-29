// =============================================================================
// renderer_vulkan/VulkanRenderer.cpp — Vulkan Triangle Implementation (AP-01)
// =============================================================================
#include "VulkanRenderer.h"
#include "../core/Log.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <vector>
#include <array>

namespace render {

// =============================================================================
// SPIR-V Shaders (compiled inline for AP-01 triangle)
// Vertex shader: simple triangle with color
// Fragment shader: output interpolated color
// =============================================================================

// SPIR-V for a simple vertex shader (position + color)
// Generated from GLSL:
// layout(location = 0) in vec2 inPosition;
// layout(location = 1) in vec3 inColor;
// layout(location = 0) out vec3 fragColor;
// void main() { gl_Position = vec4(inPosition, 0.0, 1.0); fragColor = inColor; }
static const uint32_t triangleVertSpv[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000002d, 0x00000000, 0x00020011,
    0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0008000f, 0x00000004,
    0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00000011,
    0x00030003, 0x00000002, 0x000000c2, 0x00090004, 0x415f4c47, 0x735f4252,
    0x72747065, 0x5f6e695f, 0x6c61636f, 0x00004563, 0x00050004, 0x4f5f4c47,
    0x735f4252, 0x72747065, 0x696e695f, 0x00000000, 0x00040005, 0x00000004,
    0x6e69616d, 0x00000000, 0x00050005, 0x00000009, 0x6e695f67, 0x6f505f74,
    0x00000073, 0x00050005, 0x0000000d, 0x6f6c6f43, 0x6e695f72, 0x00000000,
    0x00050005, 0x00000011, 0x67617266, 0x6f6c6f43, 0x00000072, 0x00040047,
    0x00000009, 0x0000001e, 0x00000000, 0x00040047, 0x0000000d, 0x0000001e,
    0x00000000, 0x00040047, 0x00000011, 0x0000001e, 0x00000000, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
    0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009,
    0x00000003, 0x00040015, 0x0000000a, 0x00000020, 0x00000000, 0x0004002b,
    0x0000000a, 0x0000000b, 0x00000000, 0x00040020, 0x0000000c, 0x00000001,
    0x00000007, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000001, 0x00040017,
    0x0000000e, 0x00000006, 0x00000003, 0x00040020, 0x0000000f, 0x00000003,
    0x0000000e, 0x0004003b, 0x0000000f, 0x00000011, 0x00000003, 0x0004002b,
    0x00000006, 0x00000014, 0x00000000, 0x0004002b, 0x00000006, 0x00000015,
    0x3f800000, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003,
    0x000200f8, 0x00000005, 0x0004003d, 0x00000007, 0x00000010, 0x0000000d,
    0x00050051, 0x00000006, 0x00000012, 0x00000010, 0x00000000, 0x00050051,
    0x00000006, 0x00000013, 0x00000010, 0x00000001, 0x00070050, 0x00000007,
    0x00000016, 0x00000012, 0x00000013, 0x00000014, 0x00000015, 0x0003003e,
    0x00000009, 0x00000016, 0x0004003d, 0x0000000e, 0x00000017, 0x00000011,
    0x0004003d, 0x00000007, 0x00000018, 0x0000000d, 0x0008004f, 0x0000000e,
    0x00000019, 0x00000018, 0x00000018, 0x00000000, 0x00000001, 0x00000002,
    0x0003003e, 0x00000017, 0x00000019, 0x000100fd, 0x00010038
};

// SPIR-V for a simple fragment shader
// layout(location = 0) in vec3 fragColor;
// layout(location = 0) out vec4 outColor;
// void main() { outColor = vec4(fragColor, 1.0); }
static const uint32_t triangleFragSpv[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000001a, 0x00000000, 0x00020011,
    0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0007000f, 0x00000004,
    0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000d, 0x00030010,
    0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000000c2, 0x00090004,
    0x415f4c47, 0x735f4252, 0x72747065, 0x5f6e695f, 0x6c61636f, 0x00004563,
    0x00050004, 0x4f5f4c47, 0x735f4252, 0x72747065, 0x696e695f, 0x00000000,
    0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00050005, 0x00000009,
    0x6f6c6f43, 0x6e695f72, 0x00000000, 0x00050005, 0x0000000d, 0x756f7247,
    0x6f6c6f43, 0x00000072, 0x00040047, 0x00000009, 0x0000001e, 0x00000000,
    0x00040047, 0x0000000d, 0x0000001e, 0x00000000, 0x00020013, 0x00000002,
    0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008,
    0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003,
    0x00040017, 0x0000000a, 0x00000006, 0x00000003, 0x00040020, 0x0000000b,
    0x00000001, 0x0000000a, 0x0004003b, 0x0000000b, 0x0000000d, 0x00000001,
    0x0004002b, 0x00000006, 0x0000000f, 0x3f800000, 0x00050036, 0x00000002,
    0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d,
    0x0000000a, 0x0000000e, 0x0000000d, 0x00050051, 0x00000006, 0x00000010,
    0x0000000e, 0x00000000, 0x00050051, 0x00000006, 0x00000011, 0x0000000e,
    0x00000001, 0x00050051, 0x00000006, 0x00000012, 0x0000000e, 0x00000002,
    0x00070050, 0x00000007, 0x00000013, 0x00000010, 0x00000011, 0x00000012,
    0x0000000f, 0x0003003e, 0x00000009, 0x00000013, 0x000100fd, 0x00010038
};

// =============================================================================
// Vertex Data (Triangle)
// =============================================================================
struct Vertex {
    float position[2];
    float color[3];
};

static const Vertex triangleVertices[] = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // Top - Red
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},  // Right - Green
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},  // Left - Blue
};

// =============================================================================
// VulkanRenderer Implementation
// =============================================================================
bool VulkanRenderer::Init(GLFWwindow* win, uint32_t w, uint32_t h, const VulkanConfig& cfg) {
    window = win;
    width = w;
    height = h;
    config = cfg;

    AddLog("[Vulkan] Initializing renderer...");

    if (!CreateInstance()) return false;
    if (config.enableDebugMessenger && !SetupDebugMessenger()) return false;
    if (!CreateSurface()) return false;
    if (!SelectPhysicalDevice()) return false;
    if (!CreateLogicalDevice()) return false;
    if (!CreateSwapchain()) return false;
    if (!CreateImageViews()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreatePipeline()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateCommandPool()) return false;
    if (!CreateCommandBuffers()) return false;
    if (!CreateSyncObjects()) return false;

    initialized = true;
    AddLog("[Vulkan] Renderer initialized successfully");
    AddLog("[Vulkan] Swapchain: {}x{}, {} images", swapchainExtent.width, swapchainExtent.height, swapchainImages.size());
    return true;
}

void VulkanRenderer::Shutdown() {
    if (!initialized) return;

    vkDeviceWaitIdle(device);

    // Cleanup frame data
    for (auto& frame : frames) {
        if (frame.renderFence) vkDestroyFence(device, frame.renderFence, nullptr);
        if (frame.presentSemaphore) vkDestroySemaphore(device, frame.presentSemaphore, nullptr);
        if (frame.renderSemaphore) vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
        if (frame.commandPool) vkDestroyCommandPool(device, frame.commandPool, nullptr);
    }

    if (immediateFence) vkDestroyFence(device, immediateFence, nullptr);
    if (immediateSemaphore) vkDestroySemaphore(device, immediateSemaphore, nullptr);
    if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);

    CleanupSwapchain();

    if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);

    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (debugMessenger) {
        auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFunc) destroyFunc(instance, debugMessenger, nullptr);
    }
    if (instance) vkDestroyInstance(instance, nullptr);

    initialized = false;
    AddLog("[Vulkan] Renderer shutdown complete");
}

// =============================================================================
// Init Steps
// =============================================================================
bool VulkanRenderer::CreateInstance() {
    vkb::InstanceBuilder builder;
    auto instRet = builder
        .set_app_name("TheSeed")
        .set_engine_name("TheSeed Engine")
        .require_api_version(1, 3, 0)
        .request_validation_layers(config.enableValidation)
        .use_default_debug_messenger()
        .build();

    if (!instRet) {
        AddLog("[Vulkan] Failed to create instance: {}", static_cast<int>(instRet.error()));
        return false;
    }

    vkbInstance = instRet.value();
    instance = vkbInstance.instance;

    if (config.enableDebugMessenger) {
        debugMessenger = vkbInstance.debug_messenger;
    }

    AddLog("[Vulkan] Instance created (API 1.3)");
    return true;
}

bool VulkanRenderer::CreateSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to create window surface");
        return false;
    }
    AddLog("[Vulkan] Surface created");
    return true;
}

bool VulkanRenderer::SelectPhysicalDevice() {
    vkb::PhysicalDeviceSelector selector(vkbInstance);
    auto physRet = selector
        .set_surface(surface)
        .set_minimum_version(1, 3)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .require_dedicated_transfer_queue()
        .select();

    if (!physRet) {
        AddLog("[Vulkan] Failed to select physical device: {}", static_cast<int>(physRet.error()));
        return false;
    }

    physicalDevice = physRet.value().physical_device;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    AddLog("[Vulkan] Physical device: {}", props.deviceName);

    return true;
}

bool VulkanRenderer::CreateLogicalDevice() {
    vkb::DeviceBuilder builder(physicalDevice);
    auto devRet = builder.build();

    if (!devRet) {
        AddLog("[Vulkan] Failed to create logical device: {}", static_cast<int>(devRet.error()));
        return false;
    }

    vkbDevice = devRet.value();
    device = vkbDevice.device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value_or(graphicsQueue);
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    AddLog("[Vulkan] Logical device created");
    return true;
}

bool VulkanRenderer::CreateSwapchain() {
    vkb::SwapchainBuilder builder(vkbDevice);
    auto swapRet = builder
        .set_old_swapchain(VK_NULL_HANDLE)
        .set_desired_min_image_count(config.desiredImageCount)
        .set_desired_format({config.preferredFormat, config.preferredColorSpace})
        .set_present_mode(config.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
        .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();

    if (!swapRet) {
        AddLog("[Vulkan] Failed to create swapchain: {}", static_cast<int>(swapRet.error()));
        return false;
    }

    vkbSwapchain = swapRet.value();
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageFormat = vkbSwapchain.image_format;
    swapchainExtent = vkbSwapchain.extent;

    return true;
}

bool VulkanRenderer::CreateImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            AddLog("[Vulkan] Failed to create image view {}", i);
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to create render pass");
        return false;
    }

    return true;
}

bool VulkanRenderer::CreateFramebuffers() {
    framebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews[i]};

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = swapchainExtent.width;
        fbInfo.height = swapchainExtent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            AddLog("[Vulkan] Failed to create framebuffer {}", i);
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to create command pool");
        return false;
    }

    return true;
}

bool VulkanRenderer::CreateCommandBuffers() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &frames[i].commandPool) != VK_SUCCESS) {
            AddLog("[Vulkan] Failed to create frame command pool {}", i);
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = frames[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer) != VK_SUCCESS) {
            AddLog("[Vulkan] Failed to allocate command buffer {}", i);
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::CreateSyncObjects() {
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].presentSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].renderSemaphore) != VK_SUCCESS) {
            AddLog("[Vulkan] Failed to create sync objects for frame {}", i);
            return false;
        }
    }

    // Immediate submit fence
    if (vkCreateFence(device, &fenceInfo, nullptr, &immediateFence) != VK_SUCCESS) {
        return false;
    }
    if (vkCreateSemaphore(device, &semInfo, nullptr, &immediateSemaphore) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool VulkanRenderer::CreatePipeline() {
    // Create shader modules
    VkShaderModule vertModule = CreateShaderModule(std::span(triangleVertSpv));
    VkShaderModule fragModule = CreateShaderModule(std::span(triangleFragSpv));

    if (!vertModule || !fragModule) {
        AddLog("[Vulkan] Failed to create shader modules");
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStage = {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage = {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // Vertex input
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2] = {};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);

    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attrDescs;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport & Scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to create pipeline layout");
        return false;
    }

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to create graphics pipeline");
        return false;
    }

    // Cleanup shader modules
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    AddLog("[Vulkan] Graphics pipeline created (triangle)");
    return true;
}

// =============================================================================
// Frame Operations
// =============================================================================
void VulkanRenderer::BeginFrame() {
    auto& frame = frames[currentFrame];

    // Wait for previous frame
    vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &frame.renderFence);

    // Acquire next image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
        frame.presentSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        AddLog("[Vulkan] Failed to acquire swapchain image");
        return;
    }

    // Reset command pool
    vkResetCommandPool(device, frame.commandPool, 0);

    // Begin recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

    // Begin render pass
    BeginRenderPass(frame.commandBuffer, imageIndex);

    // Set viewport and scissor
    SetViewport(frame.commandBuffer, 0, 0, 
        static_cast<float>(swapchainExtent.width), 
        static_cast<float>(swapchainExtent.height));
    SetScissor(frame.commandBuffer, 0, 0, swapchainExtent.width, swapchainExtent.height);

    // Bind pipeline
    BindPipeline(frame.commandBuffer);
}

void VulkanRenderer::EndFrame() {
    auto& frame = frames[currentFrame];

    EndRenderPass(frame.commandBuffer);

    vkEndCommandBuffer(frame.commandBuffer);

    // Submit
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.presentSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.renderSemaphore;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.renderFence);

    // Present
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentFrame; // Simplified - should track imageIndex properly

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        AddLog("[Vulkan] Failed to present swapchain image");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::Present() {
    // Handled in EndFrame
}

// =============================================================================
// Rendering Commands
// =============================================================================
void VulkanRenderer::BeginRenderPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    VkClearValue clearColor = {{{0.1f, 0.1f, 0.15f, 1.0f}}}; // Dark blue-gray
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderer::EndRenderPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void VulkanRenderer::BindPipeline(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
}

void VulkanRenderer::DrawTriangle(VkCommandBuffer cmd) {
    // For AP-01, we draw a simple triangle using vertex data embedded in shader
    // In production, this would use vertex buffers
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void VulkanRenderer::DrawIndexed(VkCommandBuffer cmd, uint32_t indexCount) {
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

void VulkanRenderer::SetViewport(VkCommandBuffer cmd, float x, float y, float w, float h) {
    VkViewport viewport = {};
    viewport.x = x;
    viewport.y = y;
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
}

void VulkanRenderer::SetScissor(VkCommandBuffer cmd, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    VkRect2D scissor = {};
    scissor.offset = {x, y};
    scissor.extent = {w, h};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

// =============================================================================
// Resource Management
// =============================================================================
VkShaderModule VulkanRenderer::CreateShaderModule(std::span<const uint32_t> spirvCode) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    VkShaderModule module;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

void VulkanRenderer::DestroyShaderModule(VkShaderModule module) {
    if (module) vkDestroyShaderModule(device, module, nullptr);
}

void VulkanRenderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);
    function(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, immediateFence);
    vkWaitForFences(device, 1, &immediateFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &immediateFence);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// =============================================================================
// Resize
// =============================================================================
void VulkanRenderer::Resize(uint32_t w, uint32_t h) {
    width = w;
    height = h;
    RecreateSwapchain();
}

void VulkanRenderer::CleanupSwapchain() {
    for (auto fb : framebuffers) {
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers.clear();

    for (auto view : swapchainImageViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();

    if (swapchain) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::RecreateSwapchain() {
    vkDeviceWaitIdle(device);
    CleanupSwapchain();

    // Recreate
    vkb::SwapchainBuilder builder(vkbDevice);
    auto swapRet = builder
        .set_old_swapchain(VK_NULL_HANDLE)
        .set_desired_min_image_count(config.desiredImageCount)
        .set_desired_format({config.preferredFormat, config.preferredColorSpace})
        .set_present_mode(config.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
        .build();

    if (swapRet) {
        vkbSwapchain = swapRet.value();
        swapchain = vkbSwapchain.swapchain;
        swapchainImages = vkbSwapchain.get_images().value();
        swapchainImageFormat = vkbSwapchain.image_format;
        swapchainExtent = vkbSwapchain.extent;

        CreateImageViews();
        CreateFramebuffers();
    }
}

// =============================================================================
// Debug
// =============================================================================
bool VulkanRenderer::SetupDebugMessenger() {
    // Already handled by vk-bootstrap
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void)pUserData;
    (void)messageType;

    const char* severity = "INFO";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) severity = "WARN";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) severity = "ERROR";

    AddLog("[Vulkan {}] {}", severity, pCallbackData->pMessage);
    return VK_FALSE;
}

} // namespace render
