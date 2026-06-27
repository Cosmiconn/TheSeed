#include "VulkanQueue.h"

namespace vulkan {

VulkanQueue::VulkanQueue(VkQueue q, uint32_t family) : queue(q), familyIndex(family) {}

void VulkanQueue::Submit(VkCommandBuffer cmd, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence) {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
    submitInfo.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = signalSemaphore ? 1u : 0u;
    submitInfo.pSignalSemaphores = signalSemaphore ? &signalSemaphore : nullptr;
    
    vkQueueSubmit(queue, 1, &submitInfo, fence);
}

void VulkanQueue::WaitIdle() {
    vkQueueWaitIdle(queue);
}

} // namespace vulkan
