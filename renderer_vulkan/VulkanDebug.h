#pragma once
// =============================================================================
// renderer_vulkan/VulkanDebug.h — Validation utilities
// =============================================================================
#include <vulkan/vulkan.h>
#include <string>

namespace vulkan {

[[nodiscard]] std::string VkResultToString(VkResult result);
void CheckVkResult(VkResult result, const char* operation);

#ifdef NDEBUG
    #define VK_CHECK(result, op) ((void)0)
#else
    #define VK_CHECK(result, op) vulkan::CheckVkResult(result, op)
#endif

} // namespace vulkan
