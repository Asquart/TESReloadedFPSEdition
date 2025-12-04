#include "VulkanEffect.h"

void IVulkanEffect::Initialize()
{
    CreateResources();
}

void IVulkanEffect::OnGameBuffersUpdated()
{
    DestroyResources();
    CreateResources();
}

bool IVulkanEffect::IsEnabled() const
{
    // Prefer NVR settings if available, fall back to local flag
    if (TheSettingManager)
    {
        // NVR already has helpers for "Shaders.<Name>.Status.Enabled"
        return TheSettingManager->GetMenuShaderEnabled(GetName());
    }
    return bEnabled;
}

void IVulkanEffect::SetEnabled(const bool InEnabled)
{
    bEnabled = InEnabled;

    // Push the change back into NVR so UI + config stay in sync
    if (TheSettingManager) {
        TheSettingManager->SetMenuShaderEnabled(GetName(), InEnabled);
    }
}

float IVulkanEffect::GetGpuTimeMs() const
{
    return GpuTimeMs;
}

void IVulkanEffect::SetGpuTimeMs(const float InMs)
{
    GpuTimeMs = InMs;
}

void IVulkanEffect::CreateResources()
{
    // Auto-build SPIR-V path from GetSpirvFileName if not set explicitly
    if (SpirvPath.empty())
    {
        const std::string& fileName = GetSpirvFileName();
        if (fileName[0] != '\0')
        {
            // Adjust base path to whatever you're actually using
            SpirvPath = std::string("Data\\Shaders\\NewVegasReloaded\\Vulkan\\") + fileName;
        }
    }

    LoadShaderModule();
    CreateInteropTextures();
    CreatePipeline();
    CreateDescriptorSets();
    CreateCommandBuffer();
    CreateFence();
    CreateTimestampQueryPool();
}


void IVulkanEffect::DestroyResources()
{
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    if (EffectDescriptorPool != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorPool(Vulkan.Device, EffectDescriptorPool, nullptr);
        EffectDescriptorPool = VK_NULL_HANDLE;
    }

    if (EffectDescriptorSetLayout != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorSetLayout(Vulkan.Device, EffectDescriptorSetLayout, nullptr);
        EffectDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (EffectPipeline != VK_NULL_HANDLE) {
        p_vkDestroyPipeline(Vulkan.Device, EffectPipeline, nullptr);
        EffectPipeline = VK_NULL_HANDLE;
    }

    if (EffectPipelineLayout != VK_NULL_HANDLE) {
        p_vkDestroyPipelineLayout(Vulkan.Device, EffectPipelineLayout, nullptr);
        EffectPipelineLayout = VK_NULL_HANDLE;
    }

    if (EffectShaderModule != VK_NULL_HANDLE) {
        p_vkDestroyShaderModule(Vulkan.Device, EffectShaderModule, nullptr);
        EffectShaderModule = VK_NULL_HANDLE;
    }

    if (EffectCommandBuffer != VK_NULL_HANDLE)
    {
        p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &EffectCommandBuffer);
    }

    if (EffectFence != VK_NULL_HANDLE)
    {
        p_vkDestroyFence(Vulkan.Device, EffectFence, nullptr);
    }

    if (EffectQueryPool != VK_NULL_HANDLE) {
        p_vkDestroyQueryPool(Vulkan.Device, EffectQueryPool, nullptr);
        EffectQueryPool = VK_NULL_HANDLE;
    }
}

void IVulkanEffect::CreateCommandBuffer()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    CommandBufferAllocateInfo.commandPool = VULKAN_CONTEXT.CmdPool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandBufferCount = 1;

    VkResult Vr = p_vkAllocateCommandBuffers(VULKAN_CONTEXT.Device, &CommandBufferAllocateInfo, &EffectCommandBuffer);
    if (Vr != VK_SUCCESS || !EffectCommandBuffer) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkAllocateCommandBuffers failed rv=%d cmd=%p", Vr, EffectCommandBuffer);
        return;
    }
}

void IVulkanEffect::CreateFence()
{
    if (EffectFence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkResult vr = p_vkCreateFence(VULKAN_CONTEXT.Device, &fi, nullptr, &EffectFence);
        if (vr != VK_SUCCESS) {
            Logger::Log("FVulkanCombineDepthEffect: vkCreateFence failed rv=%d", vr);
            EffectFence = VK_NULL_HANDLE;
        }
    }
}

void IVulkanEffect::LoadShaderModule()
{
    DXVK_CheckReturn()

    // Load SPIR-V from disk
    std::vector<uint32_t> Spirv;
    {
        std::ifstream File(SpirvPath,
            std::ios::binary | std::ios::ate);
        if (!File) {
            Logger::Log("IVulkanEffect: could not open %s", SpirvPath);
            return;
        }

        std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);
        Spirv.resize(Size / sizeof(uint32_t));

        if (!File.read(reinterpret_cast<char*>(Spirv.data()), Size)) {
            Logger::Log("IVulkanEffect: failed to read %s", SpirvPath);
            Spirv.clear();
            return;
        }
    }

    if (Spirv.empty()) {
        Logger::Log("IVulkanEffect: failed to read %s, SPIR-V is empty", SpirvPath);
        return;
    }

    VkShaderModuleCreateInfo Info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    Info.codeSize = Spirv.size() * sizeof(uint32_t);
    Info.pCode = Spirv.data();

    VK_CHECK(p_vkCreateShaderModule(VULKAN_CONTEXT.Device, &Info, nullptr, &EffectShaderModule),
        "vkCreateShaderModule");
}

void IVulkanEffect::CreateTimestampQueryPool()
{
    if (EffectQueryPool == VK_NULL_HANDLE && VULKAN_CONTEXT.TimestampPeriod > 0.0)
    {
        VkQueryPoolCreateInfo qp{};
        qp.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qp.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        qp.queryCount = 2; // start + end

        VkResult vr = p_vkCreateQueryPool(VULKAN_CONTEXT.Device, &qp, nullptr, &EffectQueryPool);
        if (vr != VK_SUCCESS)
        {
            Logger::Log("IVulkanEffect::CreateResources: vkCreateQueryPool failed rv=%d", vr);
            EffectQueryPool = VK_NULL_HANDLE;
        }
        else
        {
            StartQueryIndex = 0;
            EndQueryIndex   = 1;
        }
    }
}

void FComputeEffectBase::SubmitRendering()
{
    DXVK_CheckReturn();

    UpdateSettingsFromNvr();

    if (!IsEnabled())
        return;

    if (!PrepareResourcesForSubmit())
        return;

    VkExtent2D extent = GetDispatchExtent();
    if (extent.width == 0 || extent.height == 0)
        return;

    VkExtent2D wg = GetWorkgroupSize();
    const uint32_t groupsX = (extent.width  + wg.width  - 1) / wg.width;
    const uint32_t groupsY = (extent.height + wg.height - 1) / wg.height;

    VULKAN_CONTEXT.InteropDevice->LockSubmissionQueue();

    VkResult vr = p_vkResetCommandBuffer(EffectCommandBuffer, 0);
    if (vr != VK_SUCCESS) {
        Logger::Log("FComputeEffectBase::SubmitRendering: vkResetCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vr = p_vkBeginCommandBuffer(EffectCommandBuffer, &beginInfo);
    if (vr != VK_SUCCESS) {
        Logger::Log("FComputeEffectBase::SubmitRendering: vkBeginCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }

    // Reset timestamp queries if available
    if (DEBUG && EffectQueryPool != VK_NULL_HANDLE)
    {
        p_vkCmdResetQueryPool(EffectCommandBuffer, EffectQueryPool, 0, 2);

        p_vkCmdWriteTimestamp(
            EffectCommandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EffectQueryPool,
            StartQueryIndex);
    }

    // Bind pipeline once; derived classes can rebind if needed.
    p_vkCmdBindPipeline(EffectCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, EffectPipeline);

    const uint32_t passCount = GetPassCount();
    for (uint32_t pass = 0; pass < passCount; ++pass)
    {
        RecordPassCommands(EffectCommandBuffer, pass, groupsX, groupsY);
        OnAfterPass(EffectCommandBuffer, pass);
    }

    if (DEBUG && EffectQueryPool != VK_NULL_HANDLE)
    {
        p_vkCmdWriteTimestamp(
            EffectCommandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EffectQueryPool,
            EndQueryIndex);
    }

    vr = p_vkEndCommandBuffer(EffectCommandBuffer);
    if (vr != VK_SUCCESS) {
        Logger::Log("FComputeEffectBase::SubmitRendering: vkEndCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }

    TRY_APPLY_FENCE(this);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &EffectCommandBuffer;

    vr = p_vkQueueSubmit(VULKAN_CONTEXT.Queue, 1, &submit, EffectFence);
    if (vr != VK_SUCCESS) {
        Logger::Log("FComputeEffectBase::SubmitRendering: vkQueueSubmit failed rv=%d", vr);
        OnSubmitFailed(vr);
    }

    VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
    TRY_DEBUG_END_FENCE(this);
}


void FGraphicsEffectBase::SubmitRendering()
{
    DXVK_CheckReturn();

    if (!IsEnabled())
        return;

    VkExtent2D extent = GetRenderExtent();
    if (extent.width == 0 || extent.height == 0)
        return;

    VkRenderPass  renderPass  = GetRenderPass();
    VkFramebuffer framebuffer = GetFramebuffer();

    if (renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE)
        return;

    VULKAN_CONTEXT.InteropDevice->LockSubmissionQueue();

    VkResult vr = p_vkResetCommandBuffer(EffectCommandBuffer, 0);
    if (vr != VK_SUCCESS) {
        Logger::Log("FGraphicsEffectBase::SubmitRendering: vkResetCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vr = p_vkBeginCommandBuffer(EffectCommandBuffer, &beginInfo);
    if (vr != VK_SUCCESS) {
        Logger::Log("FGraphicsEffectBase::SubmitRendering: vkBeginCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }
    // (optional) configure clear values here if needed

    VkRenderPassBeginInfo rpBegin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpBegin.renderPass  = renderPass;
    rpBegin.framebuffer = framebuffer;
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = extent;
    rpBegin.clearValueCount = GetClearValueCount();
    rpBegin.pClearValues    = GetClearValues();

    p_vkCmdBeginRenderPass(EffectCommandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    OnBeginRenderPass(EffectCommandBuffer);

    RecordDrawCommands(EffectCommandBuffer);

    OnEndRenderPass(EffectCommandBuffer);
    p_vkCmdEndRenderPass(EffectCommandBuffer);

    vr = p_vkEndCommandBuffer(EffectCommandBuffer);
    if (vr != VK_SUCCESS) {
        Logger::Log("FGraphicsEffectBase::SubmitRendering: vkEndCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        OnSubmitFailed(vr);
        return;
    }

    TRY_APPLY_FENCE(this);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &EffectCommandBuffer;

    vr = p_vkQueueSubmit(VULKAN_CONTEXT.Queue, 1, &submit, EffectFence);
    if (vr != VK_SUCCESS) {
        Logger::Log("FGraphicsEffectBase::SubmitRendering: vkQueueSubmit failed rv=%d", vr);
        OnSubmitFailed(vr);
    }

    VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
    TRY_DEBUG_END_FENCE(this);
}

