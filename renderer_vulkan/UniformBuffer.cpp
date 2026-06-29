// =============================================================================
// renderer_vulkan/UniformBuffer.cpp — UBO + Descriptor Sets Implementation (AP-04)
// =============================================================================
#include "UniformBuffer.h"
#include "../core/Log.h"
#include <cstring>

namespace render {

// =============================================================================
// UniformBuffer
// =============================================================================
bool UniformBuffer::Create(VkDevice dev, VkPhysicalDevice physDev, VkDeviceSize bufferSize) {
    Destroy();
    device = dev;
    size = bufferSize;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        AddLog("[UniformBuffer] Failed to create buffer");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer, &memReq);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & 
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memTypeIndex = i;
            break;
        }
    }

    if (memTypeIndex == UINT32_MAX) {
        AddLog("[UniformBuffer] Failed to find suitable memory type");
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        AddLog("[UniformBuffer] Failed to allocate memory");
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    vkMapMemory(device, memory, 0, size, 0, &mapped);

    return true;
}

void UniformBuffer::Destroy() {
    if (mapped) {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
    if (memory) vkFreeMemory(device, memory, nullptr);
    if (buffer) vkDestroyBuffer(device, buffer, nullptr);
    memory = VK_NULL_HANDLE;
    buffer = VK_NULL_HANDLE;
    size = 0;
}

void UniformBuffer::Update(const void* data, VkDeviceSize dataSize) {
    if (mapped && dataSize <= size) {
        std::memcpy(mapped, data, static_cast<size_t>(dataSize));
    }
}

// =============================================================================
// DescriptorSetLayoutBuilder
// =============================================================================
DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding(
    uint32_t binding, VkDescriptorType type, uint32_t count,
    VkShaderStageFlags stages, const VkSampler* immutableSamplers) {

    VkDescriptorSetLayoutBinding b = {};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = count;
    b.stageFlags = stages;
    b.pImmutableSamplers = immutableSamplers;
    bindings.push_back(b);
    return *this;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build() const {
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
        AddLog("[DescriptorSetLayout] Failed to create layout");
        return VK_NULL_HANDLE;
    }
    return layout;
}

// =============================================================================
// DescriptorPool
// =============================================================================
bool DescriptorPool::Create(VkDevice dev, uint32_t max) {
    Destroy();
    device = dev;
    maxSets = max;

    std::array<VkDescriptorPoolSize, 4> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxSets * 10;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[1].descriptorCount = maxSets * 5;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = maxSets * 10;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = maxSets * 2;

    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes = poolSizes.data();
    info.maxSets = maxSets;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS) {
        AddLog("[DescriptorPool] Failed to create pool");
        return false;
    }
    return true;
}

void DescriptorPool::Destroy() {
    if (pool) vkDestroyDescriptorPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
}

VkDescriptorSet DescriptorPool::Allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(device, &info, &set);
    return set;
}

void DescriptorPool::Free(VkDescriptorSet set) {
    vkFreeDescriptorSets(device, pool, 1, &set);
}

void DescriptorPool::Reset() {
    vkResetDescriptorPool(device, pool, 0);
}

// =============================================================================
// DescriptorWriter
// =============================================================================
DescriptorWriter& DescriptorWriter::WriteBuffer(uint32_t binding, VkBuffer buffer,
                                                 VkDeviceSize size, VkDescriptorType type,
                                                 VkDeviceSize offset) {
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back();
    info.buffer = buffer;
    info.offset = offset;
    info.range = size;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &info;
    writes.push_back(write);
    return *this;
}

DescriptorWriter& DescriptorWriter::WriteImage(uint32_t binding, VkImageView view,
                                                VkSampler sampler, VkDescriptorType type,
                                                VkImageLayout layout) {
    VkDescriptorImageInfo& info = imageInfos.emplace_back();
    info.imageView = view;
    info.sampler = sampler;
    info.imageLayout = layout;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pImageInfo = &info;
    writes.push_back(write);
    return *this;
}

void DescriptorWriter::Apply() {
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// =============================================================================
// FrameDescriptorManager
// =============================================================================
bool FrameDescriptorManager::Init(VkDevice dev, VkPhysicalDevice physDev) {
    device = dev;
    physDevice = physDev;

    descriptorPool = std::make_unique<DescriptorPool>();
    if (!descriptorPool->Create(device, 1000)) return false;

    if (!CreateLayouts()) return false;
    if (!CreateUBOs()) return false;
    if (!AllocateSets()) return false;
    BindUBOsToSets();

    AddLog("[FrameDescriptorManager] Initialized with {} frames", MAX_FRAMES);
    return true;
}

void FrameDescriptorManager::Destroy() {
    for (auto& ubo : globalUBOs) if (ubo) ubo->Destroy();
    for (auto& ubo : objectUBOs) if (ubo) ubo->Destroy();
    for (auto& ubo : materialUBOs) if (ubo) ubo->Destroy();

    if (globalLayout) vkDestroyDescriptorSetLayout(device, globalLayout, nullptr);
    if (objectLayout) vkDestroyDescriptorSetLayout(device, objectLayout, nullptr);
    if (materialLayout) vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);

    descriptorPool.reset();
}

bool FrameDescriptorManager::CreateLayouts() {
    // Global layout: Camera matrices + lights
    DescriptorSetLayoutBuilder globalBuilder(device);
    globalBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    globalLayout = globalBuilder.Build();
    if (!globalLayout) return false;

    // Object layout: Per-object transform
    DescriptorSetLayoutBuilder objectBuilder(device);
    objectBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
    objectLayout = objectBuilder.Build();
    if (!objectLayout) return false;

    // Material layout: Material properties + textures
    DescriptorSetLayoutBuilder materialBuilder(device);
    materialBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialBuilder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    materialLayout = materialBuilder.Build();
    if (!materialLayout) return false;

    return true;
}

bool FrameDescriptorManager::CreateUBOs() {
    for (size_t i = 0; i < MAX_FRAMES; ++i) {
        globalUBOs[i] = std::make_unique<UniformBuffer>();
        if (!globalUBOs[i]->Create(device, physDevice, sizeof(GlobalUBO))) return false;

        objectUBOs[i] = std::make_unique<UniformBuffer>();
        if (!objectUBOs[i]->Create(device, physDevice, sizeof(ObjectUBO))) return false;

        materialUBOs[i] = std::make_unique<UniformBuffer>();
        if (!materialUBOs[i]->Create(device, physDevice, sizeof(MaterialUBO))) return false;
    }
    return true;
}

bool FrameDescriptorManager::AllocateSets() {
    for (size_t i = 0; i < MAX_FRAMES; ++i) {
        globalSets[i] = descriptorPool->Allocate(globalLayout);
        objectSets[i] = descriptorPool->Allocate(objectLayout);
        materialSets[i] = descriptorPool->Allocate(materialLayout);

        if (!globalSets[i] || !objectSets[i] || !materialSets[i]) {
            AddLog("[FrameDescriptorManager] Failed to allocate descriptor sets");
            return false;
        }
    }
    return true;
}

void FrameDescriptorManager::BindUBOsToSets() {
    for (size_t i = 0; i < MAX_FRAMES; ++i) {
        // Global set
        DescriptorWriter globalWriter(device, globalSets[i]);
        globalWriter.WriteBuffer(0, globalUBOs[i]->GetBuffer(), sizeof(GlobalUBO), 
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        globalWriter.Apply();

        // Object set
        DescriptorWriter objectWriter(device, objectSets[i]);
        objectWriter.WriteBuffer(0, objectUBOs[i]->GetBuffer(), sizeof(ObjectUBO),
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        objectWriter.Apply();

        // Material set
        DescriptorWriter materialWriter(device, materialSets[i]);
        materialWriter.WriteBuffer(0, materialUBOs[i]->GetBuffer(), sizeof(MaterialUBO),
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        materialWriter.Apply();
    }
}

void FrameDescriptorManager::UpdateGlobal(uint32_t frameIndex, const GlobalUBO& ubo) {
    if (frameIndex < MAX_FRAMES && globalUBOs[frameIndex]) {
        globalUBOs[frameIndex]->Update(ubo);
    }
}

void FrameDescriptorManager::UpdateObject(uint32_t frameIndex, const ObjectUBO& ubo) {
    if (frameIndex < MAX_FRAMES && objectUBOs[frameIndex]) {
        objectUBOs[frameIndex]->Update(ubo);
    }
}

void FrameDescriptorManager::UpdateMaterial(uint32_t frameIndex, const MaterialUBO& ubo) {
    if (frameIndex < MAX_FRAMES && materialUBOs[frameIndex]) {
        materialUBOs[frameIndex]->Update(ubo);
    }
}

} // namespace render
