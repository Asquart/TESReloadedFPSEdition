#include "VulkanCombineDepth.h"

// If you use the factory macro:
REGISTER_VULKAN_EFFECT(FVulkanCombineDepthEffect,
    EVulkanEffectPhase::PreTonemap,
    3); // order: before AO etc.

FVulkanCombineDepthEffect::~FVulkanCombineDepthEffect()
{
    DestroyResources();
}

void FVulkanCombineDepthEffect::CreateResources()
{
    DXVK_CheckReturn()

        CreateShaderModule();
    CreatePipeline();
    CreateInteropTextures();
    CreateDescriptorSets();
}

void FVulkanCombineDepthEffect::DestroyResources()
{
    DXVK_CheckReturn()

        FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    if (DescPool != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorPool(Vulkan.Device, DescPool, nullptr);
        DescPool = VK_NULL_HANDLE;
    }

    if (DescSetLayout != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorSetLayout(Vulkan.Device, DescSetLayout, nullptr);
        DescSetLayout = VK_NULL_HANDLE;
    }

    if (Pipeline != VK_NULL_HANDLE) {
        p_vkDestroyPipeline(Vulkan.Device, Pipeline, nullptr);
        Pipeline = VK_NULL_HANDLE;
    }

    if (PipelineLayout != VK_NULL_HANDLE) {
        p_vkDestroyPipelineLayout(Vulkan.Device, PipelineLayout, nullptr);
        PipelineLayout = VK_NULL_HANDLE;
    }

    if (ShaderModule != VK_NULL_HANDLE) {
        p_vkDestroyShaderModule(Vulkan.Device, ShaderModule, nullptr);
        ShaderModule = VK_NULL_HANDLE;
    }

    // Interop surfaces cleanup
    TheVulkanEffectsManager->InteropManager.DestroySurface(WorldDepthSurface);
    TheVulkanEffectsManager->InteropManager.DestroySurface(ViewModelDepthSurface);
    TheVulkanEffectsManager->InteropManager.DestroySurface(CombinedDepthSurface);

    bHasWorldDepth = false;
    bHasViewModelDepth = false;
}

void FVulkanCombineDepthEffect::UpdateSettingsFromNvr()
{
    // If you ever expose depth debug toggles, read from TheSettingManager here.
    // For now, nothing special.
}

void FVulkanCombineDepthEffect::CreateShaderModule()
{
    DXVK_CheckReturn()

        FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // Load SPIR-V from disk
    std::vector<uint32_t> Spirv;
    {
        std::ifstream File("Data\\Shaders\\NewVegasReloaded\\Vulkan\\Includes\\CombineDepth.comp.spv",
            std::ios::binary | std::ios::ate);
        if (!File) {
            Logger::Log("FVulkanCombineDepthEffect: could not open CombineDepth.comp.spv");
            return;
        }

        std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);
        Spirv.resize(Size / sizeof(uint32_t));

        if (!File.read(reinterpret_cast<char*>(Spirv.data()), Size)) {
            Logger::Log("FVulkanCombineDepthEffect: failed to read CombineDepth.comp.spv");
            Spirv.clear();
            return;
        }
    }

    if (Spirv.empty()) {
        Logger::Log("FVulkanCombineDepthEffect: SPIR-V is empty");
        return;
    }

    VkShaderModuleCreateInfo Info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    Info.codeSize = Spirv.size() * sizeof(uint32_t);
    Info.pCode = Spirv.data();

    VK_CHECK(p_vkCreateShaderModule(Vulkan.Device, &Info, nullptr, &ShaderModule),
        "vkCreateShaderModule(CombineDepth)");
}

void FVulkanCombineDepthEffect::CreatePipeline()
{
    DXVK_CheckReturn()

        FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // --- 1) Local descriptor set layout (set = 1) ---
    // binding 0: output storage image
    VkDescriptorSetLayoutBinding Binding{};
    Binding.binding = 0;
    Binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    Binding.descriptorCount = 1;
    Binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo DslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    DslInfo.bindingCount = 1;
    DslInfo.pBindings = &Binding;

    VK_CHECK(p_vkCreateDescriptorSetLayout(Device, &DslInfo, nullptr, &DescSetLayout),
        "vkCreateDescriptorSetLayout(CombineDepth)");

    // --- 2) Pipeline layout: [ set 0 = global frame, set 1 = local (this effect) ] ---
    VkPushConstantRange PcRange{};
    PcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    PcRange.offset = 0;
    PcRange.size = sizeof(FPushConstants); // keep your existing push-constant size

    VkDescriptorSetLayout SetLayouts[2] = {
        TheVulkanEffectsManager->GlobalResources.GetSets().GlobalFrameSetLayout, // set = 0
        DescSetLayout                                                            // set = 1
    };

    VkPipelineLayoutCreateInfo PlInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    PlInfo.setLayoutCount = 2;
    PlInfo.pSetLayouts = SetLayouts;
    PlInfo.pushConstantRangeCount = 1;
    PlInfo.pPushConstantRanges = &PcRange;

    VK_CHECK(p_vkCreatePipelineLayout(Device, &PlInfo, nullptr, &PipelineLayout),
        "vkCreatePipelineLayout(CombineDepth)");

    // --- 3) Compute pipeline ---
    VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    Stage.module = ShaderModule;
    Stage.pName = "main";

    VkComputePipelineCreateInfo CpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    CpInfo.stage = Stage;
    CpInfo.layout = PipelineLayout;

    VK_CHECK(p_vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &CpInfo, nullptr, &Pipeline),
        "vkCreateComputePipelines(CombineDepth)");
}


void FVulkanCombineDepthEffect::CreateInteropTextures()
{
    DXVK_CheckReturn()

    // CombinedDepthSurface is owned by this effect.
    // WorldDepthSurface / ViewModelDepthSurface are created on-demand when we first
    // capture from D3D, so nothing to do here for those.
    CombinedDepthSurface = FVulkanInteropSurface{};
    WorldDepthSurface = FVulkanInteropSurface{};
    ViewModelDepthSurface = FVulkanInteropSurface{};
    bHasWorldDepth = false;
    bHasViewModelDepth = false;
}

void FVulkanCombineDepthEffect::CreateDescriptorSets()
{
    DXVK_CheckReturn()

        FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // Only one descriptor type: STORAGE_IMAGE (for uOutImage at set=1, binding=0)
    VkDescriptorPoolSize PoolSize{};
    PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    PoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo PoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    PoolInfo.maxSets = 1;
    PoolInfo.poolSizeCount = 1;
    PoolInfo.pPoolSizes = &PoolSize;

    VK_CHECK(p_vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &DescPool),
        "vkCreateDescriptorPool(CombineDepth)");

    VkDescriptorSetAllocateInfo AllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    AllocInfo.descriptorPool = DescPool;
    AllocInfo.descriptorSetCount = 1;
    AllocInfo.pSetLayouts = &DescSetLayout;

    VK_CHECK(p_vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet),
        "vkAllocateDescriptorSets(CombineDepth)");
}


void FVulkanCombineDepthEffect::RecreateCombinedSurfaceIfNeeded(uint32_t Width, uint32_t Height)
{
    DXVK_CheckReturn()

    if (Width == 0 || Height == 0)
    {
        Logger::Log("FVulkanCombineDepthEffect::RecreateCombinedSurfaceIfNeeded - Width = %d, Height = %d", Width, Height);
        return;
    }
    if (CombinedDepthSurface.IsValid())
    {
        D3DSURFACE_DESC Desc{};
        CombinedDepthSurface.D3DSurface->GetDesc(&Desc);
        if (CombinedDepthSurface.D3DSurface) {
            // Check if the existing size matches
            if (Desc.Width == Width && Desc.Height == Height)
            {
                Logger::Log("FVulkanCombineDepthEffect::RecreateCombinedSurfaceIfNeeded -CombinedDepthSurface is valid and of required dimensions");
                return; // already correct
            }

        }
    }

    TheVulkanEffectsManager->InteropManager.DestroySurface(CombinedDepthSurface);

    // Combined depth is a color-like FP target: we want storage usage.
    TheVulkanEffectsManager->InteropManager.CreateSurface(
        CombinedDepthSurface,
        Width,
        Height,
        D3DFMT_A32B32G32R32F,   // COLOR FP32
        /*bUseStorage=*/true    // Compute shader writes allowed
    );
}

void FVulkanCombineDepthEffect::DebugRunOnNvrCombinedDepth(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn();

    if (!SceneColor || !TheShaderManager || !TheShaderManager->Effects.CombineDepth)
        return;

    // 1) Ensure our Vulkan depth surface (NVR combined depth) exists
    FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
    if (!VulkanDepthSurface || !VulkanDepthSurface->View) {
        Logger::Log("DebugRunOnNvrCombinedDepth: VulkanDepthSurface is invalid");
        return;
    }

    // 2) Make sure output surface exists & matches size
    RecreateCombinedSurfaceIfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!CombinedDepthSurface.D3DSurface || !CombinedDepthSurface.Image || !CombinedDepthSurface.View) {
        Logger::Log("DebugRunOnNvrCombinedDepth: CombinedDepthSurface not valid");
        return;
    }

    // 3) Update global frame resources (UBO + depth sampler in set=0)
    TheVulkanEffectsManager->GlobalResources.UpdatePerFrame();

    // 4) Hook our output image into this effect's descriptor set (set=1, binding=0)
    VkDescriptorImageInfo OutInfo{};
    OutInfo.imageView = CombinedDepthSurface.View;
    OutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // compute writes

    VkWriteDescriptorSet Write{};
    Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    Write.dstSet = DescSet;                 // this effect's set=1
    Write.dstBinding = 0;                  // binding 0: uOutImage
    Write.descriptorCount = 1;
    Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    Write.pImageInfo = &OutInfo;

    p_vkUpdateDescriptorSets(TheVulkanEffectsManager->VulkanContext.Device, 1, &Write, 0, nullptr);

    // 5) Allocate transient command buffer
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    Vulkan.InteropDevice->LockSubmissionQueue();

    VkCommandBufferAllocateInfo CbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    CbAlloc.commandPool = Vulkan.CmdPool;
    CbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CbAlloc.commandBufferCount = 1;

    VkCommandBuffer Cmd = VK_NULL_HANDLE;
    VkResult Vr = p_vkAllocateCommandBuffers(Vulkan.Device, &CbAlloc, &Cmd);
    if (Vr != VK_SUCCESS || !Cmd) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkAllocateCommandBuffers failed rv=%d cmd=%p", Vr, Cmd);
        Vulkan.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    VkCommandBufferBeginInfo BeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Vr = p_vkBeginCommandBuffer(Cmd, &BeginInfo);
    if (Vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkBeginCommandBuffer failed rv=%d", Vr);
        p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &Cmd);
        Vulkan.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    // (Optional) if you want a barrier to GENERAL, you can add one here.
    // But your interop surface is created for storage usage already.

    // 6) Bind pipeline + both descriptor sets (set 0 = global, set 1 = local)
    p_vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);

    VkDescriptorSet Sets[2];
    Sets[0] = TheVulkanEffectsManager->GlobalResources.GetSets().GlobalFrameSet; // set 0
    Sets[1] = DescSet;                                                           // set 1

    p_vkCmdBindDescriptorSets(
        Cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        PipelineLayout,
        0,         // firstSet
        2,         // descriptorSetCount
        Sets,
        0, nullptr);

    // 7) Dispatch
    const uint32_t Wgx = 16, Wgy = 16;
    uint32_t GroupsX = (VulkanDepthSurface->Width + Wgx - 1) / Wgx;
    uint32_t GroupsY = (VulkanDepthSurface->Height + Wgy - 1) / Wgy;

    p_vkCmdDispatch(Cmd, GroupsX, GroupsY, 1);

    Vr = p_vkEndCommandBuffer(Cmd);
    if (Vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkEndCommandBuffer failed rv=%d", Vr);
        p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &Cmd);
        Vulkan.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    // 8) Submit + wait
    VkFence Fence = VK_NULL_HANDLE;
    VkFenceCreateInfo FenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    Vr = p_vkCreateFence(Vulkan.Device, &FenceInfo, nullptr, &Fence);
    if (Vr != VK_SUCCESS || !Fence) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkCreateFence failed rv=%d fence=%p", Vr, Fence);
        p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &Cmd);
        Vulkan.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    VkSubmitInfo Submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    Submit.commandBufferCount = 1;
    Submit.pCommandBuffers = &Cmd;

    Vr = p_vkQueueSubmit(Vulkan.Queue, 1, &Submit, Fence);
    if (Vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkQueueSubmit failed rv=%d", Vr);
    }
    else {
        Vr = p_vkWaitForFences(Vulkan.Device, 1, &Fence, VK_TRUE, UINT64_MAX);
        Logger::Log("DebugRunOnNvrCombinedDepth: vkWaitForFences rv=%d", Vr);
    }

    p_vkDestroyFence(Vulkan.Device, Fence, nullptr);
    p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &Cmd);

    Vulkan.InteropDevice->ReleaseSubmissionQueue();

    // 9) Blit to SceneColor for on-screen debug
    IDirect3DDevice9* Device9 = TheVulkanEffectsManager->D3D9Device;
    if (Device9 && CombinedDepthSurface.D3DSurface) {
        HRESULT HrBlit = Device9->StretchRect(
            CombinedDepthSurface.D3DSurface, nullptr,
            SceneColor, nullptr,
            D3DTEXF_POINT);
        Logger::Log("DebugRunOnNvrCombinedDepth: StretchRect hr=0x%08X", HrBlit);
    }
}



void FVulkanCombineDepthEffect::RenderPreTonemapping(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn()

    DebugRunOnNvrCombinedDepth(SceneColor);
}