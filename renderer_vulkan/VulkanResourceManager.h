#pragma once
// =============================================================================
// renderer_vulkan/VulkanResourceManager.h — Descriptors, Layouts, Pools
// AP-03: Resource-Manager (Descriptor Pools, Layouts)
// Bindless-ready, Texture-Array-Support
// =============================================================================
#include "VulkanContext.h"
#include <vector>
#include <map>
#include <memory>

namespace vulkan {

struct DescriptorSetLayoutBinding {
    uint32_t binding;
    VkDescriptorType type;
    uint32_t count;
    VkShaderStageFlags stages;
};

class VulkanDescriptorLayout {
public:
    VulkanDescriptorLayout(VulkanContext& context);
    ~VulkanDescriptorLayout();

    void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages);
    [[nodiscard]] bool Build();
    [[nodiscard]] VkDescriptorSetLayout Get() const { return layout; }

private:
    VulkanContext& ctx;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};

class VulkanDescriptorPool {
public:
    VulkanDescriptorPool(VulkanContext& context);
    ~VulkanDescriptorPool();

    void AddPoolSize(VkDescriptorType type, uint32_t count);
    void SetMaxSets(uint32_t max);
    [[nodiscard]] bool Build();
    [[nodiscard]] VkDescriptorSet Allocate(VkDescriptorSetLayout layout);
    void Free(VkDescriptorSet set);
    void Reset();

    [[nodiscard]] VkDescriptorPool Get() const { return pool; }

private:
    VulkanContext& ctx;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPoolSize> poolSizes;
    uint32_t maxSets = 100;
};

class VulkanResourceManager {
public:
    explicit VulkanResourceManager(VulkanContext& context);
    
    [[nodiscard]] VulkanDescriptorLayout* CreateLayout(const std::string& name);
    [[nodiscard]] VulkanDescriptorPool* CreatePool(const std::string& name);
    
    VulkanDescriptorLayout* GetLayout(const std::string& name);
    VulkanDescriptorPool* GetPool(const std::string& name);

    void DestroyAll();

private:
    VulkanContext& ctx;
    std::map<std::string, std::unique_ptr<VulkanDescriptorLayout>> layouts;
    std::map<std::string, std::unique_ptr<VulkanDescriptorPool>> pools;
};

} // namespace vulkan
