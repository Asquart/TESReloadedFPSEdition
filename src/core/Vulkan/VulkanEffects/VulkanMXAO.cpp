#include "VulkanMXAO.h"

// Factory macro:
// REGISTER_VULKAN_EFFECT(FVulkanMXAO,
//     EVulkanEffectPhase::PreTonemap,
//     1); // order

FVulkanMXAO::FVulkanMXAO()
{
    BuildSettingsDescriptors();
}

void FVulkanMXAO::FillPushConstants(FVulkanMXAOPushConstants& out,
                                    const FVulkanMXAOSettings& /*src*/,
                                    uint32_t passIndex) const
{
    out.Pass = passIndex;

    // Sampling / quality
    out.GlobalSamplePreset = Settings.GlobalSamplePreset;
    out.BaseSampleCount    = Settings.BaseSampleCount;

    // AO kernel
    out.SampleRadius      = Settings.SampleRadius;
    out.SampleNormalBias  = Settings.SampleNormalBias;

    // AO / IL strength & curve
    out.SSAOAmount        = Settings.SSAOAmount;
    out.SSILAmount        = Settings.SSILAmount;
    out.Power             = Settings.Power;

    // Depth fade – assume UI is 0..1 already; if you prefer 0..100, divide here.
    out.FadeDepthStart    = Settings.FadeDepthStart;
    out.FadeDepthEnd      = Settings.FadeDepthEnd;

    // Blur config
    out.RenderScale       = Settings.RenderScale;
    out.BlurRadius1       = Settings.BlurRadius1;
    out.BlurRadius2       = Settings.BlurRadius2;
    out.BlurSteps1        = Settings.BlurSteps1;
    out.BlurSteps2        = Settings.BlurSteps2;

    // Toggles
    out.EnableIL          = Settings.bEnableIL ? 1u : 0u;
    out.TwoLayer          = Settings.bTwoLayer ? 1u : 0u;
    out.HighQuality       = Settings.bHighQuality ? 1u : 0u;
    out.DebugView         = Settings.bDebugView ? 1u : 0u;

    // Two-layer intensities
    out.AmountCoarse      = Settings.AmountCoarse;
    out.AmountFine        = Settings.AmountFine;
}

void FVulkanMXAO::DestroyResources()
{
    DXVK_CheckReturn();

    // First clean up our own surfaces
    TheVulkanEffectsManager->InteropManager.DestroySurface(AOSurface0);
    TheVulkanEffectsManager->InteropManager.DestroySurface(AOSurface1);

    // Then clean up generic Vulkan stuff (pipeline, descriptor pool, etc.)
    IVulkanEffect::DestroyResources();
}

VkExtent2D FVulkanMXAO::GetDispatchExtent() const
{
    FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
    if (!VulkanDepthSurface)
        return VkExtent2D{ 0, 0 };

    return VkExtent2D{ VulkanDepthSurface->Width, VulkanDepthSurface->Height };
}

bool FVulkanMXAO::PrepareResourcesForSubmit()
{
    FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
    if (!VulkanDepthSurface || !VulkanDepthSurface->View) {
        Logger::Log("FVulkanMXAO::PrepareResourcesForSubmit: depth surface invalid");
        return false;
    }

    CreateAOSurface0IfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!AOSurface0.D3DSurface || !AOSurface0.Image || !AOSurface0.View) {
        Logger::Log("FVulkanMXAO::PrepareResourcesForSubmit: AO surface 0 invalid");
        return false;
    }

    CreateAOSurface1IfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!AOSurface1.D3DSurface || !AOSurface1.Image || !AOSurface1.View) {
        Logger::Log("FVulkanMXAO::PrepareResourcesForSubmit: AO surface 1 invalid");
        return false;
    }

    return true;
}

void FVulkanMXAO::RecordPassCommands(VkCommandBuffer cmd,
                                     uint32_t passIndex,
                                     uint32_t groupsX,
                                     uint32_t groupsY)
{
    UpdateDescriptorsForPass(passIndex);

    // Bind global + effect descriptor sets
    VkDescriptorSet sets[2];
    sets[0] = TheVulkanEffectsManager->GlobalResources
                  .GetGlobalDescriptorSets().GlobalFrameDescriptorSet; // set 0
    sets[1] = EffectDescriptorSet;                                     // set 1

    p_vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        EffectPipelineLayout,
        0,
        2,
        sets,
        0,
        nullptr);

    // Push constants + dispatch via helper
    PushConstantsAndDispatch(cmd, passIndex, groupsX, groupsY);
}

void FVulkanMXAO::OnAfterPass(VkCommandBuffer cmd, uint32_t passIndex)
{
    // We only need barriers between passes that ping-pong AO surfaces.
    if (passIndex == 0) {
        // Pass 0 wrote AOSurface0, read in pass 1
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image         = AOSurface0.Image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        p_vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }
    else if (passIndex == 1) {
        // Pass 1 wrote AOSurface1, read in pass 2
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image         = AOSurface1.Image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        p_vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    // No explicit barrier needed after pass 2 here – we just StretchRect the AO result in CompleteRendering.
}

void FVulkanMXAO::CreatePipeline()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // --- 1) Local descriptor set layout (set = 1) ---
    // binding 0: mxaoIn  (AO/IL input, storage image)
    // binding 1: mxaoOut (AO/IL output, storage image OR final color target if you later choose)
    VkDescriptorSetLayoutBinding bindings[2]{};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslInfo.bindingCount = 2;
    dslInfo.pBindings    = bindings;

    VK_CHECK(p_vkCreateDescriptorSetLayout(Device, &dslInfo, nullptr, &EffectDescriptorSetLayout),
             "vkCreateDescriptorSetLayout(VulkanMXAO)");

    // --- 2) Pipeline layout ---
    VkPushConstantRange PcRange{};
    PcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    PcRange.offset     = 0;
    PcRange.size       = sizeof(FVulkanMXAOPushConstants);

    VkDescriptorSetLayout SetLayouts[2] = {
        TheVulkanEffectsManager->GlobalResources.GetGlobalDescriptorSets().GlobalFrameSetLayout, // set = 0
        EffectDescriptorSetLayout                                                                 // set = 1
    };

    VkPipelineLayoutCreateInfo PlInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    PlInfo.setLayoutCount         = 2;
    PlInfo.pSetLayouts            = SetLayouts;
    PlInfo.pushConstantRangeCount = 1;
    PlInfo.pPushConstantRanges    = &PcRange;

    VK_CHECK(p_vkCreatePipelineLayout(Device, &PlInfo, nullptr, &EffectPipelineLayout),
        "vkCreatePipelineLayout(VulkanMXAO)");

    // --- 3) Compute pipeline ---
    VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    Stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    Stage.module = EffectShaderModule;
    Stage.pName  = "main";

    VkComputePipelineCreateInfo CpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    CpInfo.stage  = Stage;
    CpInfo.layout = EffectPipelineLayout;

    VK_CHECK(p_vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &CpInfo, nullptr, &EffectPipeline),
        "vkCreateComputePipelines(VulkanMXAO)");
}

void FVulkanMXAO::CreateDescriptorSets()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // We only have storage images in set 1
    VkDescriptorPoolSize poolSizes[1]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 2; // mxaoIn + mxaoOut

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = poolSizes;

    VK_CHECK(p_vkCreateDescriptorPool(Device, &poolInfo, nullptr, &EffectDescriptorPool),
             "vkCreateDescriptorPool(VulkanMXAO)");

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = EffectDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &EffectDescriptorSetLayout;

    VK_CHECK(p_vkAllocateDescriptorSets(Device, &allocInfo, &EffectDescriptorSet),
             "vkAllocateDescriptorSets(VulkanMXAO)");
}

void FVulkanMXAO::UpdateDescriptorsForPass(uint32_t InPass)
{
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    FVulkanInteropSurface* src = nullptr;
    FVulkanInteropSurface* dst = nullptr;

    switch (InPass) {
    case 0: // AO gen: write AO/IL to AOSurface0, src unused
        dst = &AOSurface0;
        src = &AOSurface0; // just bind something valid; shader doesn't read mxaoIn in pass 0
        break;
    case 1: // blur1: AOSurface0 -> AOSurface1
        dst = &AOSurface1;
        src = &AOSurface0;
        break;
    case 2: // blur2 + shape + composite into final AO result (AOSurface0)
    default:
        dst = &AOSurface0;
        src = &AOSurface1;
        break;
    }

    VkDescriptorImageInfo inInfo{};
    inInfo.imageView   = src->View;
    inInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    inInfo.sampler     = VK_NULL_HANDLE; // unused for storage images

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = dst->View;
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outInfo.sampler     = VK_NULL_HANDLE;

    VkWriteDescriptorSet writes[2]{};

    // binding 0: mxaoIn (storage image, read-only in shader)
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = EffectDescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo      = &inInfo;

    // binding 1: mxaoOut (storage image, write-only in shader)
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = EffectDescriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo      = &outInfo;

    p_vkUpdateDescriptorSets(Device, 2, writes, 0, nullptr);
}

void FVulkanMXAO::CreateInteropTextures()
{
    DXVK_CheckReturn()

    AOSurface0 = FVulkanInteropSurface{};
    AOSurface1 = FVulkanInteropSurface{};

    CreateAOSurface0IfNeeded(TheRenderManager->width, TheRenderManager->height);
    CreateAOSurface1IfNeeded(TheRenderManager->width, TheRenderManager->height);
}

void FVulkanMXAO::CreateAOSurface0IfNeeded(uint32_t Width, uint32_t Height)
{
    DXVK_CheckReturn()

    if (Width == 0 || Height == 0)
    {
        Logger::Log("FVulkanMXAO::CreateAOSurface0IfNeeded - Width = %d, Height = %d", Width, Height);
        return;
    }

    if (AOSurface0.IsValid())
    {
        D3DSURFACE_DESC Desc{};
        AOSurface0.D3DSurface->GetDesc(&Desc);
        if (AOSurface0.D3DSurface) {
            if (Desc.Width == Width && Desc.Height == Height)
                return; // size matches, nothing to do
        }
    }

    TheVulkanEffectsManager->InteropManager.DestroySurface(AOSurface0);

    // FP16 color surface with storage usage for compute
    TheVulkanEffectsManager->InteropManager.CreateSurface(
        AOSurface0,
        Width,
        Height,
        D3DFMT_A16B16G16R16F,   // COLOR FP16
        /*bUseStorage=*/true,   // compute shader writes allowed
        VK_IMAGE_LAYOUT_GENERAL
    );
}

void FVulkanMXAO::CreateAOSurface1IfNeeded(uint32_t Width, uint32_t Height)
{
    DXVK_CheckReturn()

    if (Width == 0 || Height == 0)
    {
        Logger::Log("FVulkanMXAO::CreateAOSurface1IfNeeded - Width = %d, Height = %d", Width, Height);
        return;
    }

    if (AOSurface1.IsValid())
    {
        D3DSURFACE_DESC Desc{};
        AOSurface1.D3DSurface->GetDesc(&Desc);
        if (AOSurface1.D3DSurface) {
            if (Desc.Width == Width && Desc.Height == Height)
                return; // size matches
        }
    }

    TheVulkanEffectsManager->InteropManager.DestroySurface(AOSurface1);

    TheVulkanEffectsManager->InteropManager.CreateSurface(
        AOSurface1,
        Width,
        Height,
        D3DFMT_A16B16G16R16F,   // COLOR FP16
        /*bUseStorage=*/true,   // compute shader writes allowed
        VK_IMAGE_LAYOUT_GENERAL
    );
}

void FVulkanMXAO::CompleteRendering(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn();
    ENSURE_END_FENCE(this)

    // For MXAO we actually want to render onto scene color.
    // At this point AOSurface0 holds the final composited color (scene * AO + IL),
    // so we just blit it over.
    if (AOSurface0.D3DSurface && SceneColor)
    {
        TheRenderManager->device->StretchRect(
            AOSurface0.D3DSurface,
            nullptr,
            SceneColor,
            nullptr,
            D3DTEXF_NONE);
    }
}

void FVulkanMXAO::UpdateSettingsFromNvr()
{
    if (!TheSettingManager)
        return;

    const char* shaderId = GetName();   // e.g. "VulkanMXAO"

    char section[128];

    // Main group
    std::snprintf(section, sizeof(section), "Shaders.%s.Main", shaderId);
    Settings.GlobalSamplePreset = TheSettingManager->GetSettingI(section, "GlobalSamplePreset");
    Settings.BaseSampleCount    = TheSettingManager->GetSettingI(section, "BaseSampleCount");
    Settings.SampleRadius       = TheSettingManager->GetSettingF(section, "SampleRadius");
    Settings.SampleNormalBias   = TheSettingManager->GetSettingF(section, "SampleNormalBias");
    Settings.SSAOAmount         = TheSettingManager->GetSettingF(section, "SSAOAmount");
    Settings.SSILAmount         = TheSettingManager->GetSettingF(section, "SSILAmount");
    Settings.Power              = TheSettingManager->GetSettingF(section, "Power");
    Settings.FadeDepthStart     = TheSettingManager->GetSettingF(section, "FadeDepthStart");
    Settings.FadeDepthEnd       = TheSettingManager->GetSettingF(section, "FadeDepthEnd");
    Settings.RenderScale        = TheSettingManager->GetSettingF(section, "RenderScale");
    Settings.BlurRadius1        = TheSettingManager->GetSettingF(section, "BlurRadius1");
    Settings.BlurRadius2        = TheSettingManager->GetSettingF(section, "BlurRadius2");
    Settings.BlurSteps1         = TheSettingManager->GetSettingI(section, "BlurSteps1");
    Settings.BlurSteps2         = TheSettingManager->GetSettingI(section, "BlurSteps2");
    Settings.AmountCoarse       = TheSettingManager->GetSettingF(section, "AmountCoarse");
    Settings.AmountFine         = TheSettingManager->GetSettingF(section, "AmountFine");
    Settings.bEnableIL          = (TheSettingManager->GetSettingI(section, "EnableIL")     != 0);
    Settings.bTwoLayer          = (TheSettingManager->GetSettingI(section, "TwoLayer")     != 0);
    Settings.bHighQuality       = (TheSettingManager->GetSettingI(section, "HighQuality")  != 0);

    // Debug group
    std::snprintf(section, sizeof(section), "Shaders.%s.Debug", shaderId);
    Settings.bDebugView = (TheSettingManager->GetSettingI(section, "DebugView") != 0);
}

void FVulkanMXAO::BuildSettingsDescriptors()
{
    SettingDescs.clear();

    // --- Main group ---

    {
        VulkanSettingDescriptor d{};
        d.id       = "GlobalSamplePreset";
        d.label    = "Sample Quality Preset";
        d.group    = "Main";
        d.type     = VulkanSettingType::Int;
        d.minValue = 0;
        d.maxValue = 7;
        d.step     = 1.0f;

        d.getInt = [this]() { return Settings.GlobalSamplePreset; };
        d.setInt = [this](int v) { Settings.GlobalSamplePreset = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "BaseSampleCount";
        d.label    = "Base Sample Count";
        d.group    = "Main";
        d.type     = VulkanSettingType::Int;
        d.minValue = 4;
        d.maxValue = 128;
        d.step     = 1.0f;

        d.getInt = [this]() { return Settings.BaseSampleCount; };
        d.setInt = [this](int v) { Settings.BaseSampleCount = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "SampleRadius";
        d.label    = "Sample Radius";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.1f;
        d.maxValue = 10.0f;
        d.step     = 0.1f;

        d.getFloat = [this]() { return Settings.SampleRadius; };
        d.setFloat = [this](float v) { Settings.SampleRadius = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "SampleNormalBias";
        d.label    = "Normal Bias";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.step     = 0.01f;

        d.getFloat = [this]() { return Settings.SampleNormalBias; };
        d.setFloat = [this](float v) { Settings.SampleNormalBias = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "SSAOAmount";
        d.label    = "SSAO Amount";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 4.0f;
        d.step     = 0.05f;

        d.getFloat = [this]() { return Settings.SSAOAmount; };
        d.setFloat = [this](float v) { Settings.SSAOAmount = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "SSILAmount";
        d.label    = "SSIL Amount";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 4.0f;
        d.step     = 0.05f;

        d.getFloat = [this]() { return Settings.SSILAmount; };
        d.setFloat = [this](float v) { Settings.SSILAmount = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "Power";
        d.label    = "AO Power";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.1f;
        d.maxValue = 4.0f;
        d.step     = 0.05f;

        d.getFloat = [this]() { return Settings.Power; };
        d.setFloat = [this](float v) { Settings.Power = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "FadeDepthStart";
        d.label    = "Fade Depth Start";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.step     = 0.01f;

        d.getFloat = [this]() { return Settings.FadeDepthStart; };
        d.setFloat = [this](float v) { Settings.FadeDepthStart = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "FadeDepthEnd";
        d.label    = "Fade Depth End";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.step     = 0.01f;

        d.getFloat = [this]() { return Settings.FadeDepthEnd; };
        d.setFloat = [this](float v) { Settings.FadeDepthEnd = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "BlurRadius1";
        d.label    = "Blur Radius 1";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.1f;
        d.maxValue = 4.0f;
        d.step     = 0.1f;

        d.getFloat = [this]() { return Settings.BlurRadius1; };
        d.setFloat = [this](float v) { Settings.BlurRadius1 = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "BlurRadius2";
        d.label    = "Blur Radius 2";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.1f;
        d.maxValue = 4.0f;
        d.step     = 0.1f;

        d.getFloat = [this]() { return Settings.BlurRadius2; };
        d.setFloat = [this](float v) { Settings.BlurRadius2 = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "BlurSteps1";
        d.label    = "Blur Steps 1";
        d.group    = "Main";
        d.type     = VulkanSettingType::Int;
        d.minValue = 1;
        d.maxValue = 8;
        d.step     = 1.0f;

        d.getInt = [this]() { return Settings.BlurSteps1; };
        d.setInt = [this](int v) { Settings.BlurSteps1 = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "BlurSteps2";
        d.label    = "Blur Steps 2";
        d.group    = "Main";
        d.type     = VulkanSettingType::Int;
        d.minValue = 1;
        d.maxValue = 8;
        d.step     = 1.0f;

        d.getInt = [this]() { return Settings.BlurSteps2; };
        d.setInt = [this](int v) { Settings.BlurSteps2 = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "AmountCoarse";
        d.label    = "Amount Coarse";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 4.0f;
        d.step     = 0.05f;

        d.getFloat = [this]() { return Settings.AmountCoarse; };
        d.setFloat = [this](float v) { Settings.AmountCoarse = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "AmountFine";
        d.label    = "Amount Fine";
        d.group    = "Main";
        d.type     = VulkanSettingType::Float;
        d.minValue = 0.0f;
        d.maxValue = 4.0f;
        d.step     = 0.05f;

        d.getFloat = [this]() { return Settings.AmountFine; };
        d.setFloat = [this](float v) { Settings.AmountFine = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "EnableIL";
        d.label    = "Enable IL";
        d.group    = "Main";
        d.type     = VulkanSettingType::Bool;

        d.getBool = [this]() { return Settings.bEnableIL; };
        d.setBool = [this](bool v) { Settings.bEnableIL = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "TwoLayer";
        d.label    = "Two Layer AO";
        d.group    = "Main";
        d.type     = VulkanSettingType::Bool;

        d.getBool = [this]() { return Settings.bTwoLayer; };
        d.setBool = [this](bool v) { Settings.bTwoLayer = v; };

        SettingDescs.push_back(std::move(d));
    }

    {
        VulkanSettingDescriptor d{};
        d.id       = "HighQuality";
        d.label    = "High Quality AO";
        d.group    = "Main";
        d.type     = VulkanSettingType::Bool;

        d.getBool = [this]() { return Settings.bHighQuality; };
        d.setBool = [this](bool v) { Settings.bHighQuality = v; };

        SettingDescs.push_back(std::move(d));
    }

    // --- Debug group ---

    {
        VulkanSettingDescriptor d{};
        d.id       = "DebugView";
        d.label    = "Debug View (AO override)";
        d.group    = "Debug";
        d.type     = VulkanSettingType::Bool;

        d.getBool = [this]() { return Settings.bDebugView; };
        d.setBool = [this](bool v) { Settings.bDebugView = v; };

        SettingDescs.push_back(std::move(d));
    }
}
