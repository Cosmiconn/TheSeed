// =============================================================================
// renderer_vulkan/ShadowMap.cpp — Cascaded Shadow Maps Implementation (AP-10)
// =============================================================================
#include "ShadowMap.h"
#include "../core/Log.h"
#include <algorithm>
#include <cmath>

namespace render {

bool ShadowMapRenderer::Init(VkDevice dev, VkPhysicalDevice physDev,
                               VkDescriptorSetLayout globalLayout) {
    device = dev;
    physDevice = physDev;

    if (!CreateShadowRenderPass()) return false;
    if (!CreateShadowImage()) return false;
    if (!CreateShadowPipeline(globalLayout)) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateSampler()) return false;

    initialized = true;
    AddLog("[ShadowMap] Initialized with {} cascades @ {}x{}",
           ShadowConfig::CASCADE_COUNT, ShadowConfig::SHADOW_MAP_SIZE, ShadowConfig::SHADOW_MAP_SIZE);
    return true;
}

void ShadowMapRenderer::Destroy() {
    if (shadowSampler) vkDestroySampler(device, shadowSampler, nullptr);
    if (shadowImageView) vkDestroyImageView(device, shadowImageView, nullptr);
    if (shadowImage) vkDestroyImage(device, shadowImage, nullptr);
    if (shadowMemory) vkFreeMemory(device, shadowMemory, nullptr);

    for (auto& cascade : cascades) {
        if (cascade.framebuffer) vkDestroyFramebuffer(device, cascade.framebuffer, nullptr);
        if (cascade.imageView) vkDestroyImageView(device, cascade.imageView, nullptr);
    }

    if (shadowPipeline) vkDestroyPipeline(device, shadowPipeline, nullptr);
    if (shadowPipelineLayout) vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
    if (shadowRenderPass) vkDestroyRenderPass(device, shadowRenderPass, nullptr);

    initialized = false;
}

// =============================================================================
// Shadow Render Pass
// =============================================================================
bool ShadowMapRenderer::CreateShadowRenderPass() {
    VkAttachmentDescription attachment = {};
    attachment.format = VK_FORMAT_D32_SFLOAT;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &info, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to create shadow render pass");
        return false;
    }
    return true;
}

// =============================================================================
// Shadow Image (Depth Array)
// =============================================================================
bool ShadowMapRenderer::CreateShadowImage() {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = ShadowConfig::SHADOW_MAP_SIZE;
    imageInfo.extent.height = ShadowConfig::SHADOW_MAP_SIZE;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = ShadowConfig::CASCADE_COUNT;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &shadowImage) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to create shadow image");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, shadowImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &shadowMemory) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to allocate shadow memory");
        return false;
    }

    vkBindImageMemory(device, shadowImage, shadowMemory, 0);

    // Create image view for the whole array
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = shadowImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = ShadowConfig::CASCADE_COUNT;

    if (vkCreateImageView(device, &viewInfo, nullptr, &shadowImageView) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to create shadow image view");
        return false;
    }

    return true;
}

// =============================================================================
// Shadow Pipeline
// =============================================================================
bool ShadowMapRenderer::CreateShadowPipeline(VkDescriptorSetLayout globalLayout) {
    // Shadow pipeline: vertex only (no fragment shader needed for depth-only)
    // Simplified: would need actual vertex shader with MVP

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &globalLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to create shadow pipeline layout");
        return false;
    }

    // Pipeline creation would go here (vertex shader with light MVP)
    // For brevity, omitted - would be similar to VulkanRenderer::CreatePipeline

    return true;
}

// =============================================================================
// Framebuffers (one per cascade)
// =============================================================================
bool ShadowMapRenderer::CreateFramebuffers() {
    for (uint32_t i = 0; i < ShadowConfig::CASCADE_COUNT; ++i) {
        // Per-cascade image view
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = shadowImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &cascades[i].imageView) != VK_SUCCESS) {
            AddLog("[ShadowMap] Failed to create cascade {} image view", i);
            return false;
        }

        // Framebuffer
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = shadowRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &cascades[i].imageView;
        fbInfo.width = ShadowConfig::SHADOW_MAP_SIZE;
        fbInfo.height = ShadowConfig::SHADOW_MAP_SIZE;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &cascades[i].framebuffer) != VK_SUCCESS) {
            AddLog("[ShadowMap] Failed to create cascade {} framebuffer", i);
            return false;
        }
    }
    return true;
}

// =============================================================================
// Sampler
// =============================================================================
bool ShadowMapRenderer::CreateSampler() {
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
        AddLog("[ShadowMap] Failed to create shadow sampler");
        return false;
    }
    return true;
}

// =============================================================================
// Cascade Update
// =============================================================================
void ShadowMapRenderer::UpdateCascades(const glm::mat4& cameraView, const glm::mat4& cameraProj,
                                        const glm::vec3& lightDir,
                                        float cameraNear, float cameraFar) {
    for (uint32_t i = 0; i < ShadowConfig::CASCADE_COUNT; ++i) {
        float prevSplit = (i == 0) ? cameraNear : config.cascadeDistances[i - 1];
        float currSplit = config.cascadeDistances[i];

        CalculateCascadeMatrices(cameraView, cameraProj, lightDir,
                                  prevSplit, currSplit, cascades[i]);

        cascades[i].splitDepth = config.cascadeSplits[i];
        cascades[i].farPlane = currSplit;
    }
}

void ShadowMapRenderer::CalculateCascadeMatrices(const glm::mat4& cameraView, 
                                                   const glm::mat4& cameraProj,
                                                   const glm::vec3& lightDir,
                                                   float nearPlane, float farPlane,
                                                   Cascade& cascade) {
    // Extract frustum corners for this cascade split
    glm::mat4 invViewProj = glm::inverse(cameraProj * cameraView);

    std::array<glm::vec4, 8> frustumCorners = {
        glm::vec4(-1, -1, -1, 1), glm::vec4(1, -1, -1, 1),
        glm::vec4(-1, 1, -1, 1),  glm::vec4(1, 1, -1, 1),
        glm::vec4(-1, -1, 1, 1),  glm::vec4(1, -1, 1, 1),
        glm::vec4(-1, 1, 1, 1),   glm::vec4(1, 1, 1, 1),
    };

    glm::vec3 center(0.0f);
    for (auto& corner : frustumCorners) {
        corner = invViewProj * corner;
        corner /= corner.w;
        center += glm::vec3(corner);
    }
    center /= 8.0f;

    // Light view matrix
    glm::vec3 lightPos = center - glm::normalize(lightDir) * 50.0f;
    cascade.view = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

    // Calculate orthographic projection bounds
    float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::lowest();

    for (const auto& corner : frustumCorners) {
        glm::vec4 lightSpace = cascade.view * corner;
        minX = std::min(minX, lightSpace.x);
        maxX = std::max(maxX, lightSpace.x);
        minY = std::min(minY, lightSpace.y);
        maxY = std::max(maxY, lightSpace.y);
        minZ = std::min(minZ, lightSpace.z);
        maxZ = std::max(maxZ, lightSpace.z);
    }

    // Add some padding
    float padding = 5.0f;
    minX -= padding; maxX += padding;
    minY -= padding; maxY += padding;
    minZ -= padding; maxZ += padding;

    cascade.proj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    cascade.viewProj = cascade.proj * cascade.view;
}

// =============================================================================
// Render Pass
// =============================================================================
void ShadowMapRenderer::BeginCascadePass(VkCommandBuffer cmd, uint32_t cascadeIndex) {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = shadowRenderPass;
    info.framebuffer = cascades[cascadeIndex].framebuffer;
    info.renderArea.offset = {0, 0};
    info.renderArea.extent = {ShadowConfig::SHADOW_MAP_SIZE, ShadowConfig::SHADOW_MAP_SIZE};

    VkClearValue clearValue = {};
    clearValue.depthStencil = {1.0f, 0};
    info.clearValueCount = 1;
    info.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width = static_cast<float>(ShadowConfig::SHADOW_MAP_SIZE);
    viewport.height = static_cast<float>(ShadowConfig::SHADOW_MAP_SIZE);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = {ShadowConfig::SHADOW_MAP_SIZE, ShadowConfig::SHADOW_MAP_SIZE};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void ShadowMapRenderer::EndCascadePass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void ShadowMapRenderer::BindShadowPipeline(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
}

// =============================================================================
// Helpers
// =============================================================================
ShadowMapRenderer::ShadowUBO ShadowMapRenderer::GetShadowUBO() const {
    ShadowUBO ubo = {};
    for (uint32_t i = 0; i < ShadowConfig::CASCADE_COUNT; ++i) {
        ubo.viewProjMatrices[i] = cascades[i].viewProj;
    }
    ubo.cascadeSplits = glm::vec4(
        cascades[0].splitDepth,
        cascades[1].splitDepth,
        cascades[2].splitDepth,
        cascades[3].splitDepth
    );
    ubo.bias = config.bias;
    ubo.normalBias = config.normalBias;
    ubo.usePCF = config.usePCF ? 1 : 0;
    ubo.pcfSamples = config.pcfSamples;
    return ubo;
}

uint32_t ShadowMapRenderer::GetCascadeIndex(float viewSpaceDepth) const {
    for (uint32_t i = 0; i < ShadowConfig::CASCADE_COUNT; ++i) {
        if (viewSpaceDepth < cascades[i].farPlane) {
            return i;
        }
    }
    return ShadowConfig::CASCADE_COUNT - 1;
}

uint32_t ShadowMapRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return 0;
}

} // namespace render
