#pragma once
// =============================================================================
// renderer_vulkan/VulkanQueue.h — Queue submit wrapper
// =============================================================================
#include "VulkanContext.h"

namespace vulkan {

class VulkanQueue {
public:
    VulkanQueue(VkQueue queue, uint32_t familyIndex);
    
    void Submit(VkCommandBuffer cmd, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence);
    void WaitIdle();

    [[nodiscard]] VkQueue Get() const { return queue; }
    [[nodiscard]] uint32_t GetFamilyIndex() const { return familyIndex; }

private:
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t familyIndex = 0;
};

} // namespace vulkan
