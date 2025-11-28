#include "VulkanContext.h"

void FVulkanContext::Initialize()
{
    if (bInitialized)
    {
        return;
    }
    DXVK_CheckReturn()

    HRESULT Hr = TheVulkanEffectsManager->D3D9Device->QueryInterface(__uuidof(ID3D9VkInteropDevice), (void**)&InteropDevice);
    if (FAILED(Hr) || !InteropDevice.ptr()) {
        Logger::Log("FVulkanContext: Query of ID3D9VkInteropDevice failed hr=0x%08X", Hr);
        return;
    }

    InteropDevice->GetVulkanHandles(&Instance, &PhysicalDevice, &Device);
    InteropDevice->GetSubmissionQueue(&Queue, &QueueIndex, &QueueFamilyIndex);

    VulkanFunctionsHooks::InitVulkanFunctionPointers(Instance, Device);

    InitSamplers();
    InitPools();

    bInitialized = true;
}

void FVulkanContext::Shutdown()
{

}

uint32_t FVulkanContext::FindMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags Properties) const
{
    VkPhysicalDeviceMemoryProperties MemProps{};
    p_vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProps);

    for (uint32_t i = 0; i < MemProps.memoryTypeCount; ++i)
    {
        const bool bTypeSupported = (TypeBits & (1u << i)) != 0;
        const bool bPropsMatch =
            (MemProps.memoryTypes[i].propertyFlags & Properties) == Properties;

        if (bTypeSupported && bPropsMatch)
            return i;
    }

    // Fallback / debug – in release you might assert or log and return 0
    Logger::Log("FVulkanContext::FindMemoryType: failed to find suitable memory type (typeBits=0x%08X, props=0x%08X)",
        TypeBits, Properties);
    return 0;
}

void FVulkanContext::TryStartFence(VkCommandBuffer InCommandBuffer)
{
}

void FVulkanContext::TryEndFence(VkCommandBuffer InCommandBuffer)
{
}

void FVulkanContext::InitSamplers()
{
    VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;

    // Linear clamp
    info.minFilter = VK_FILTER_LINEAR;
    info.magFilter = VK_FILTER_LINEAR;
    VK_CHECK(p_vkCreateSampler(Device, &info, nullptr, &SamplerLinearClamp), "vkCreateSampler");

    // Point clamp
    info.minFilter = VK_FILTER_NEAREST;
    info.magFilter = VK_FILTER_NEAREST;
    p_vkCreateSampler(Device, &info, nullptr, &SamplerPointClamp);

    // Linear repeat (optional)
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minFilter = VK_FILTER_LINEAR;
    info.magFilter = VK_FILTER_LINEAR;
    p_vkCreateSampler(Device, &info, nullptr, &SamplerLinearRepeat);
}

void FVulkanContext::InitPools()
{
    VkDescriptorPoolSize PoolSize{};
    PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    PoolSize.descriptorCount = 64; // enough for multiple frames

    VkDescriptorPoolCreateInfo DescriptorPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    DescriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    DescriptorPoolInfo.maxSets = 64;
    DescriptorPoolInfo.poolSizeCount = 1;
    DescriptorPoolInfo.pPoolSizes = &PoolSize;
    VK_CHECK(p_vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &GlobalDescPool),
        "vkCreateDescriptorPool");

    VkCommandPoolCreateInfo CmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CmdPoolInfo.queueFamilyIndex = QueueFamilyIndex;
    VK_CHECK(p_vkCreateCommandPool(Device, &CmdPoolInfo, nullptr, &CmdPool), "p_vkCreateCommandPool");

    VkQueryPoolCreateInfo QueryPoolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    QueryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    QueryPoolInfo.queryCount = 2;

    VK_CHECK(p_vkCreateQueryPool(Device, &QueryPoolInfo, nullptr, &QueryPool),
        "vkCreateQueryPool");
}
