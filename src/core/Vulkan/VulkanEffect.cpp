#include "VulkanEffect.h"

IVulkanEffect::IVulkanEffect(const std::string& inName,
    const std::string& inDescription,
    const std::vector<SettingManager::VulkanEffectSetting>& inDefaults)
    : Name(inName)
    , Description(inDescription)
    , DefaultSettings(inDefaults)
    , bEnabled(false)
    , bRegistered(false)
    , GpuTimeMs(0.0f)
{
}

IVulkanEffect::~IVulkanEffect()
{
    DestroyResources();
}

void IVulkanEffect::Initialize()
{
    CreateResources();
}

void IVulkanEffect::OnGameBuffersUpdated()
{
    CreateDescriptorSets();
}

void IVulkanEffect::CreateResources()
{
    LoadShaderModule();
    CreateInteropTextures();
    CreatePipeline();
    CreateDescriptorSets();
    CreateCommandBuffer();
    CreateFence();
    CreateTimingQueries();
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

    if (TimingQueryPool != VK_NULL_HANDLE)
    {
        p_vkDestroyQueryPool(Vulkan.Device, TimingQueryPool, nullptr);
        TimingQueryPool = VK_NULL_HANDLE;
    }
}

void IVulkanEffect::CreateCommandBuffer()
{
    if (!p_vkAllocateCommandBuffers || VULKAN_CONTEXT.CmdPool == VK_NULL_HANDLE || VULKAN_CONTEXT.Device == VK_NULL_HANDLE)
    {
        Logger::Log("IVulkanEffect: command buffer allocation skipped (fn/device/pool missing)");
        return;
    }

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

void IVulkanEffect::RegisterMenuSettings()
{
    EnsureRegistered();
    RefreshMenuSettings();
}

void IVulkanEffect::RefreshMenuSettings()
{
    if (!TheSettingManager)
    {
        return;
    }

    EnsureRegistered();

    bEnabled = TheSettingManager->GetMenuShaderEnabled(Name);

    if (bEnabled)
    {
        UpdateSettingsFromNvr();
    }
    else
    {
        GpuTimeMs = 0.0f;
    }
}

std::string IVulkanEffect::GetSettingsSection(const std::string& SubSection) const
{
    std::string Section = "Shaders.";
    Section += GetName();
    Section += ".";
    Section += SubSection;
    return Section;
}

void IVulkanEffect::EnsureRegistered()
{
    if (bRegistered || !TheSettingManager)
    {
        return;
    }

    TheSettingManager->RegisterVulkanEffectDefaults(Name, Description, DefaultSettings);
    TheSettingManager->RegisterVulkanEffectMenuEntry(Name, &bEnabled, &GpuTimeMs);
    bRegistered = true;
}

void IVulkanEffect::CreateTimingQueries()
{
    if (TimingQueryPool != VK_NULL_HANDLE || !VULKAN_CONTEXT.Device)
    {
        return;
    }

    VkQueryPoolCreateInfo QueryInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    QueryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    QueryInfo.queryCount = 2;

    VkResult Vr = p_vkCreateQueryPool(VULKAN_CONTEXT.Device, &QueryInfo, nullptr, &TimingQueryPool);
    if (Vr != VK_SUCCESS)
    {
        TimingQueryPool = VK_NULL_HANDLE;
    }
}

void IVulkanEffect::BeginGpuTimer()
{
    if (!DEBUG || TimingQueryPool == VK_NULL_HANDLE || EffectCommandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    p_vkResetQueryPool(VULKAN_CONTEXT.Device, TimingQueryPool, 0, 2);
    p_vkCmdWriteTimestamp(EffectCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimingQueryPool, 0);
    bGpuTimingActive = true;
}

void IVulkanEffect::EndGpuTimer()
{
    if (!bGpuTimingActive || TimingQueryPool == VK_NULL_HANDLE || EffectCommandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    p_vkCmdWriteTimestamp(EffectCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimingQueryPool, 1);
}

void IVulkanEffect::ResolveGpuTime()
{
    if (!DEBUG || !bGpuTimingActive || TimingQueryPool == VK_NULL_HANDLE)
    {
        return;
    }

    uint64_t Timings[2]{};
    VkResult Vr = p_vkGetQueryPoolResults(
        VULKAN_CONTEXT.Device,
        TimingQueryPool,
        0,
        2,
        sizeof(Timings),
        Timings,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    if (Vr == VK_SUCCESS && Timings[1] > Timings[0])
    {
        const double Delta = static_cast<double>(Timings[1] - Timings[0]);
        GpuTimeMs = static_cast<float>((Delta * VULKAN_CONTEXT.TimestampPeriod) / 1'000'000.0);
    }

    bGpuTimingActive = false;
}
