#include "VulkanRenderPass.h"
#include <iostream>

namespace vulkan {

VulkanRenderPass::VulkanRenderPass(VulkanContext& context) : ctx(context) {}

VulkanRenderPass::~VulkanRenderPass() {
    Destroy();
}

void VulkanRenderPass::AddAttachment(const AttachmentDesc& desc) {
    attachments.push_back(desc);
}

void VulkanRenderPass::AddSubpass(const std::vector<uint32_t>& colorAttachments, uint32_t depthAttachment) {
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    
    for (auto idx : colorAttachments) {
        VkAttachmentReference ref{};
        ref.attachment = idx;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentRefs.push_back(ref);
    }
    
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    subpass.pColorAttachments = attachmentRefs.data() + attachmentRefs.size() - colorAttachments.size();
    
    if (depthAttachment != UINT32_MAX) {
        VkAttachmentReference depthRef{};
        depthRef.attachment = depthAttachment;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentRefs.push_back(depthRef);
        subpass.pDepthStencilAttachment = &attachmentRefs.back();
    }
    
    subpasses.push_back(subpass);
}

void VulkanRenderPass::AddDependency(uint32_t srcSubpass, uint32_t dstSubpass,
                                     VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                     VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkSubpassDependency dep{};
    dep.srcSubpass = srcSubpass;
    dep.dstSubpass = dstSubpass;
    dep.srcStageMask = srcStage;
    dep.dstStageMask = dstStage;
    dep.srcAccessMask = srcAccess;
    dep.dstAccessMask = dstAccess;
    dependencies.push_back(dep);
}

bool VulkanRenderPass::Build() {
    vkAttachments.clear();
    for (const auto& att : attachments) {
        VkAttachmentDescription desc{};
        desc.format = att.format;
        desc.samples = att.samples;
        desc.loadOp = att.loadOp;
        desc.storeOp = att.storeOp;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = att.initialLayout;
        desc.finalLayout = att.finalLayout;
        vkAttachments.push_back(desc);
    }

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
    createInfo.pAttachments = vkAttachments.data();
    createInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    createInfo.pSubpasses = subpasses.data();
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(ctx.GetDevice(), &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Failed to create render pass\n";
        return false;
    }
    return true;
}

void VulkanRenderPass::Destroy() {
    if (renderPass) {
        vkDestroyRenderPass(ctx.GetDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

} // namespace vulkan
