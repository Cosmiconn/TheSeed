#include "VulkanResourceManager.h"
#include <iostream>

namespace vulkan {

// === VulkanDescriptorLayout ===

VulkanDescriptorLayout::VulkanDescriptorLayout(VulkanContext& context) : ctx(context) {}

VulkanDescriptorLayout::~VulkanDescriptorLayout() {
    if (layout) vkDestroyDescriptorSetLayout(ctx.GetDevice(), layout, nullptr);
}

void VulkanDescriptorLayout::AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = count;
    b.stageFlags = stages;
    bindings.push_back(b);
}

bool VulkanDescriptorLayout::Build() {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    // Bindless flag for future
    // createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    return vkCreateDescriptorSetLayout(ctx.GetDevice(), &createInfo, nullptr, &layout) == VK_SUCCESS;
}

// === VulkanDescriptorPool ===

VulkanDescriptorPool::VulkanDescriptorPool(VulkanContext& context) : ctx(context) {}

VulkanDescriptorPool::~VulkanDescriptorPool() {
    if (pool) vkDestroyDescriptorPool(ctx.GetDevice(), pool, nullptr);
}

void VulkanDescriptorPool::AddPoolSize(VkDescriptorType type, uint32_t count) {
    VkDescriptorPoolSize size{};
    size.type = type;
    size.descriptorCount = count;
    poolSizes.push_back(size);
}

void VulkanDescriptorPool::SetMaxSets(uint32_t max) {
    maxSets = max;
}

bool VulkanDescriptorPool::Build() {
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = maxSets;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    return vkCreateDescriptorPool(ctx.GetDevice(), &createInfo, nullptr, &pool) == VK_SUCCESS;
}

VkDescriptorSet VulkanDescriptorPool::Allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(ctx.GetDevice(), &allocInfo, &set) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return set;
}

void VulkanDescriptorPool::Free(VkDescriptorSet set) {
    vkFreeDescriptorSets(ctx.GetDevice(), pool, 1, &set);
}

void VulkanDescriptorPool::Reset() {
    vkResetDescriptorPool(ctx.GetDevice(), pool, 0);
}

// === VulkanResourceManager ===

VulkanResourceManager::VulkanResourceManager(VulkanContext& context) : ctx(context) {}

VulkanDescriptorLayout* VulkanResourceManager::CreateLayout(const std::string& name) {
    auto layout = std::make_unique<VulkanDescriptorLayout>(ctx);
    auto* ptr = layout.get();
    layouts[name] = std::move(layout);
    return ptr;
}

VulkanDescriptorPool* VulkanResourceManager::CreatePool(const std::string& name) {
    auto pool = std::make_unique<VulkanDescriptorPool>(ctx);
    auto* ptr = pool.get();
    pools[name] = std::move(pool);
    return ptr;
}

VulkanDescriptorLayout* VulkanResourceManager::GetLayout(const std::string& name) {
    auto it = layouts.find(name);
    return it != layouts.end() ? it->second.get() : nullptr;
}

VulkanDescriptorPool* VulkanResourceManager::GetPool(const std::string& name) {
    auto it = pools.find(name);
    return it != pools.end() ? it->second.get() : nullptr;
}

void VulkanResourceManager::DestroyAll() {
    layouts.clear();
    pools.clear();
}

} // namespace vulkan
