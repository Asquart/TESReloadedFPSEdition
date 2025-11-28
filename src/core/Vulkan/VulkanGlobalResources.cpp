#include "VulkanGlobalResources.h"
#include "VulkanEffectsManager.h"

static uint32_t FindMemoryType(
    const VkPhysicalDeviceMemoryProperties& memProps,
    uint32_t typeBits,
    VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return ~0u;
}

void FVulkanGlobalResources::Initialize()
{
    DXVK_CheckReturn();

    CreateFrameSet();
}

void FVulkanGlobalResources::Shutdown()
{
    DXVK_CheckReturn();

    DestroyFrameSet();
}

void FVulkanGlobalResources::UpdatePerFrame()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // 1) Update UBO contents
    void* mapped = nullptr;
    p_vkMapMemory(Device, GlobalSets.FrameUBOMemory, 0, sizeof(FGlobalFrameUBO), 0, &mapped);
    memcpy(mapped, &GlobalSets.CpuUBO, sizeof(FGlobalFrameUBO));
    p_vkUnmapMemory(Device, GlobalSets.FrameUBOMemory);

    // 2) Write depth sampler + UBO into GlobalFrameSet
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = GlobalSets.FrameUBO;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(FGlobalFrameUBO);

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler = Vulkan.SamplerPointClamp;
    depthInfo.imageView = TheVulkanEffectsManager->GetDepthSurface()->View;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[2] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = GlobalSets.GlobalFrameDescriptorSet;
    writes[0].dstBinding = 0; // gDepth
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &depthInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = GlobalSets.GlobalFrameDescriptorSet;
    writes[1].dstBinding = 1; // GlobalFrameUBO
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &uboInfo;

    p_vkUpdateDescriptorSets(Device, 2, writes, 0, nullptr);
}

void FVulkanGlobalResources::CreateFrameSet()
{
    Logger::Log("FVulkanGlobalResources::CreateFrameSet: Starting");
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    if (!Device) {
        Logger::Log("CreateFrameSet: no Vulkan device");
        return;
    }

    // 1) Descriptor set layout: binding 0 = depth sampler, 1 = UBO
    VkDescriptorSetLayoutBinding bindings[2] = {};

    // binding 0: depth sampler
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // binding 1: frame UBO
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    VkResult vr = p_vkCreateDescriptorSetLayout(Device, &layoutInfo, nullptr, &GlobalSets.GlobalFrameSetLayout);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateDescriptorSetLayout failed (%d)", vr);
        GlobalSets.GlobalFrameSetLayout = VK_NULL_HANDLE;
        return;
    }

    // 2) UBO buffer
    VkDeviceSize uboSize = sizeof(FGlobalFrameUBO);

    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size = uboSize;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vr = p_vkCreateBuffer(Device, &bufInfo, nullptr, &GlobalSets.FrameUBO);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateBuffer failed (%d)", vr);
        GlobalSets.FrameUBO = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memReq{};
    p_vkGetBufferMemoryRequirements(Device, GlobalSets.FrameUBO, &memReq);

    VkPhysicalDeviceMemoryProperties memProps{};
    p_vkGetPhysicalDeviceMemoryProperties(Vulkan.PhysicalDevice, &memProps);

    uint32_t memType = FindMemoryType(
        memProps,
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memType == ~0u) {
        Logger::Log("CreateFrameSet: no suitable memory type for UBO");
        return;
    }

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;

    vr = p_vkAllocateMemory(Device, &allocInfo, nullptr, &GlobalSets.FrameUBOMemory);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkAllocateMemory failed (%d)", vr);
        GlobalSets.FrameUBOMemory = VK_NULL_HANDLE;
        return;
    }

    p_vkBindBufferMemory(Device, GlobalSets.FrameUBO, GlobalSets.FrameUBOMemory, 0);

    // 3) Descriptor pool + set
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    vr = p_vkCreateDescriptorPool(Device, &poolInfo, nullptr, &GlobalSets.GlobalFramePool);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateDescriptorPool failed (%d)", vr);
        GlobalSets.GlobalFramePool = VK_NULL_HANDLE;
        return;
    }

    VkDescriptorSetAllocateInfo setAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    setAlloc.descriptorPool = GlobalSets.GlobalFramePool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts = &GlobalSets.GlobalFrameSetLayout;

    vr = p_vkAllocateDescriptorSets(Device, &setAlloc, &GlobalSets.GlobalFrameDescriptorSet);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkAllocateDescriptorSets failed (%d)", vr);
        GlobalSets.GlobalFrameDescriptorSet = VK_NULL_HANDLE;
        return;
    }

    Logger::Log("CreateFrameSet: OK (layout+UBO+set created)");
}


void FVulkanGlobalResources::DestroyFrameSet()
{
    VkDevice Device = TheVulkanEffectsManager->VulkanContext.Device;
    if (!Device)
        return;

    if (GlobalSets.FrameUBOMemory) {
        p_vkFreeMemory(Device, GlobalSets.FrameUBOMemory, nullptr);
        GlobalSets.FrameUBOMemory = VK_NULL_HANDLE;
    }

    if (GlobalSets.FrameUBO) {
        p_vkDestroyBuffer(Device, GlobalSets.FrameUBO, nullptr);
        GlobalSets.FrameUBO = VK_NULL_HANDLE;
    }

    if (GlobalSets.GlobalFramePool) {
        p_vkDestroyDescriptorPool(Device, GlobalSets.GlobalFramePool, nullptr);
        GlobalSets.GlobalFramePool = VK_NULL_HANDLE;
    }

    if (GlobalSets.GlobalFrameSetLayout) {
        p_vkDestroyDescriptorSetLayout(Device, GlobalSets.GlobalFrameSetLayout, nullptr);
        GlobalSets.GlobalFrameSetLayout = VK_NULL_HANDLE;
    }

    GlobalSets.GlobalFrameDescriptorSet = VK_NULL_HANDLE;
}
