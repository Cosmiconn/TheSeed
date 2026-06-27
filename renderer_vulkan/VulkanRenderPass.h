#pragma once
// =============================================================================
// renderer_vulkan/VulkanRenderPass.h — Generic RenderPass Builder
// AP-02: RenderPass & Framebuffer-Abstraktion
// =============================================================================
#include "VulkanContext.h"
#include <vector>

namespace vulkan {

enum class AttachmentType { Color, Depth, Resolve };

struct AttachmentDesc {
    AttachmentType type;
    VkFormat format;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
};

class VulkanRenderPass {
public:
    VulkanRenderPass(VulkanContext& context);
    ~VulkanRenderPass();

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    void AddAttachment(const AttachmentDesc& desc);
    void AddSubpass(const std::vector<uint32_t>& colorAttachments, 
                    uint32_t depthAttachment = UINT32_MAX);
    void AddDependency(uint32_t srcSubpass, uint32_t dstSubpass,
                       VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                       VkAccessFlags srcAccess, VkAccessFlags dstAccess);

    [[nodiscard]] bool Build();
    void Destroy();

    [[nodiscard]] VkRenderPass Get() const { return renderPass; }
    [[nodiscard]] const std::vector<AttachmentDesc>& GetAttachments() const { return attachments; }

private:
    VulkanContext& ctx;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    
    std::vector<AttachmentDesc> attachments;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> dependencies;
    std::vector<VkAttachmentReference> attachmentRefs;
    std::vector<VkAttachmentDescription> vkAttachments;
};

} // namespace vulkan
