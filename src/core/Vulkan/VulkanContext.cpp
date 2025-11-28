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
    InitTiming();

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

void FVulkanContext::BeginComputeWithTimestamp()
{
    DXVK_CheckReturn();

    if (!bInitialized || bComputeBroken)
        return;

    bComputeRecording = false;

    VkResult res = p_vkResetCommandBuffer(CommandBuffer, 0);
    if (res != VK_SUCCESS) {
        Logger::Log("BeginComputeWithTimestamp: vkResetCommandBuffer failed (%d)", res);
        bComputeBroken = true;
        return;
    }

    VkCommandBufferBeginInfo BeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    res = p_vkBeginCommandBuffer(CommandBuffer, &BeginInfo);
    if (res != VK_SUCCESS) {
        Logger::Log("BeginComputeWithTimestamp: vkBeginCommandBuffer failed (%d)", res);
        bComputeBroken = true;
        return;
    }

    bComputeRecording = true;

    if (TheSettingManager->SettingsMain.Develop.DebugMode) {
        p_vkCmdResetQueryPool(CommandBuffer, QueryPool, 0, 2);
        p_vkCmdWriteTimestamp(
            CommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            QueryPool,
            0);
    }
}


void FVulkanContext::EndComputeAndSubmit()
{
    DXVK_CheckReturn();

    if (!bInitialized || bComputeBroken)
        return;

    if (!bComputeRecording)
        return; // nothing recorded, nothing to submit

    const bool bDebug = TheSettingManager->SettingsMain.Develop.DebugMode;

    if (bDebug) {
        p_vkCmdWriteTimestamp(
            CommandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            QueryPool,
            1);
    }

    VkResult res = p_vkEndCommandBuffer(CommandBuffer);
    if (res != VK_SUCCESS) {
        Logger::Log("EndComputeAndSubmit: vkEndCommandBuffer failed (%d)", res);
        bComputeBroken = true;
        bComputeRecording = false;
        return;
    }

    VkSubmitInfo Submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    Submit.commandBufferCount = 1;
    Submit.pCommandBuffers = &CommandBuffer;

    if (bDebug) {
        res = p_vkResetFences(Device, 1, &SynchronizationFence);
        if (res != VK_SUCCESS) {
            Logger::Log("EndComputeAndSubmit: vkResetFences failed (%d)", res);
            bComputeBroken = true;
            bComputeRecording = false;
            return;
        }
    }

    res = p_vkQueueSubmit(
        Queue,
        1,
        &Submit,
        bDebug ? SynchronizationFence : VK_NULL_HANDLE);

    if (res == VK_ERROR_DEVICE_LOST) {
        Logger::Log("EndComputeAndSubmit: vkQueueSubmit -> VK_ERROR_DEVICE_LOST, disabling compute");
        bComputeBroken = true;
        bComputeRecording = false;
        return;
    }
    else if (res != VK_SUCCESS) {
        Logger::Log("EndComputeAndSubmit: vkQueueSubmit failed (%d)", res);
        bComputeBroken = true;
        bComputeRecording = false;
        return;
    }

    if (bDebug) {
        res = p_vkWaitForFences(Device, 1, &SynchronizationFence, VK_TRUE, UINT64_MAX);
        if (res != VK_SUCCESS) {
            Logger::Log("EndComputeAndSubmit: vkWaitForFences failed (%d)", res);
        }
        else {
            uint64_t ts[2] = {};
            res = p_vkGetQueryPoolResults(
                Device,
                QueryPool,
                0,
                2,
                sizeof(ts),
                ts,
                sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT);
            if (res == VK_SUCCESS) {
                VkPhysicalDeviceProperties props{};
                p_vkGetPhysicalDeviceProperties(PhysicalDevice, &props);
                uint64_t delta = ts[1] - ts[0];
                float ns = float(delta) * props.limits.timestampPeriod;
                TimestampPeriod = ns / 1.0e6f; // ms
            }
        }
    }
    InteropDevice->ReleaseSubmissionQueue();

    bComputeRecording = false;
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

void FVulkanContext::InitTiming()
{
    // --- Command buffer (reused every frame) ---
    if (CommandBuffer == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo CbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        CbAlloc.commandPool = CmdPool;
        CbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CbAlloc.commandBufferCount = 1;

        VK_CHECK(p_vkAllocateCommandBuffers(Device, &CbAlloc, &CommandBuffer),
            "vkAllocateCommandBuffers(ComputeCmd)");
    }

    // --- Fence (reused) ---
    if (SynchronizationFence == VK_NULL_HANDLE) {
        VkFenceCreateInfo FenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        FenceInfo.flags = 0;
        VK_CHECK(p_vkCreateFence(Device, &FenceInfo, nullptr, &SynchronizationFence),
            "vkCreateFence(ComputeFence)");
    }

    // --- Timestamp query pool (2 queries: start + end) ---
    if (QueryPool == VK_NULL_HANDLE) {
        VkQueryPoolCreateInfo QInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        QInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        QInfo.queryCount = 2;

        VK_CHECK(p_vkCreateQueryPool(Device, &QInfo, nullptr, &QueryPool),
            "vkCreateQueryPool(TimestampQueryPool)");
    }

    TimestampPeriod = 0.0;
}

void FVulkanContext::ShutdownTiming()
{
    if (QueryPool != VK_NULL_HANDLE) {
        p_vkDestroyQueryPool(Device, QueryPool, nullptr);
        QueryPool = VK_NULL_HANDLE;
    }

    if (SynchronizationFence != VK_NULL_HANDLE) {
        p_vkDestroyFence(Device, SynchronizationFence, nullptr);
        SynchronizationFence = VK_NULL_HANDLE;
    }

    if (CommandBuffer != VK_NULL_HANDLE) {
        p_vkFreeCommandBuffers(Device, CmdPool, 1, &CommandBuffer);
        CommandBuffer = VK_NULL_HANDLE;
    }
}
