#include "VulkanDebug.h"
#include <stdexcept>

namespace vulkan {

std::string VkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "SUCCESS";
        case VK_NOT_READY: return "NOT_READY";
        case VK_TIMEOUT: return "TIMEOUT";
        case VK_ERROR_DEVICE_LOST: return "DEVICE_LOST";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "EXTENSION_NOT_PRESENT";
        default: return "UNKNOWN(" + std::to_string(result) + ")";
    }
}

void CheckVkResult(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("[Vulkan] ") + operation + " failed: " + VkResultToString(result));
    }
}

} // namespace vulkan
