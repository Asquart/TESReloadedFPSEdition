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

    // 1) Descriptor set layout:
    //  binding 0: world depth    - combined image sampler
    //  binding 1: viewmodel depth- combined image sampler
    //  binding 2: out image      - storage image
    VkDescriptorSetLayoutBinding Bindings[3] = {};

    // binding 0: world depth
    Bindings[0].binding = 0;
    Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    Bindings[0].descriptorCount = 1;
    Bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // binding 1: viewmodel depth
    Bindings[1].binding = 1;
    Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    Bindings[1].descriptorCount = 1;
    Bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // binding 2: output storage image
    Bindings[2].binding = 2;
    Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    Bindings[2].descriptorCount = 1;
    Bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo DL{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    DL.bindingCount = 3;
    DL.pBindings = Bindings;

    VK_CHECK(p_vkCreateDescriptorSetLayout(Vulkan.Device, &DL, nullptr, &DescSetLayout),
        "vkCreateDescriptorSetLayout(CombineDepth)");

    // 2) Pipeline layout with push constants
    VkPushConstantRange PCRange{};
    PCRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    PCRange.offset = 0;
    PCRange.size = sizeof(FPushConstants); // 96 bytes

    VkPipelineLayoutCreateInfo PL{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    PL.setLayoutCount = 1;
    PL.pSetLayouts = &DescSetLayout;
    PL.pushConstantRangeCount = 1;
    PL.pPushConstantRanges = &PCRange;

    VK_CHECK(p_vkCreatePipelineLayout(Vulkan.Device, &PL, nullptr, &PipelineLayout),
        "vkCreatePipelineLayout(CombineDepth)");

    // 3) Compute pipeline
    VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    Stage.module = ShaderModule;
    Stage.pName = "main";

    VkComputePipelineCreateInfo CP{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    CP.stage = Stage;
    CP.layout = PipelineLayout;

    VK_CHECK(p_vkCreateComputePipelines(Vulkan.Device, VK_NULL_HANDLE, 1, &CP, nullptr, &Pipeline),
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

    // Descriptor pool (small, just 1 set)
    VkDescriptorPoolSize PoolSizes[2] = {};
    PoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    PoolSizes[0].descriptorCount = 2; // world + viewmodel
    PoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    PoolSizes[1].descriptorCount = 1; // output

    VkDescriptorPoolCreateInfo DP{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    DP.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    DP.maxSets = 1;
    DP.poolSizeCount = 2;
    DP.pPoolSizes = PoolSizes;

    VK_CHECK(p_vkCreateDescriptorPool(Vulkan.Device, &DP, nullptr, &DescPool),
        "vkCreateDescriptorPool(CombineDepth)");

    VkDescriptorSetAllocateInfo DSAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    DSAlloc.descriptorPool = DescPool;
    DSAlloc.descriptorSetCount = 1;
    DSAlloc.pSetLayouts = &DescSetLayout;

    VK_CHECK(p_vkAllocateDescriptorSets(Vulkan.Device, &DSAlloc, &DescSet),
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

void FVulkanCombineDepthEffect::RunCombinePass()
{
    DXVK_CheckReturn()

        // Must have world depth for anything useful
        if (!bHasWorldDepth ||
            !WorldDepthSurface.D3DSurface ||
            WorldDepthSurface.View == VK_NULL_HANDLE ||
            !WorldDepthSurface.InteropTex.ptr())
        {
            Logger::Log("FVulkanCombineDepthEffect::RunCombinePass: missing world depth, skipping");
            return;
        }

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    auto& Interop = Vulkan.InteropDevice;

    if (!Interop.ptr() || !Vulkan.Device || !Vulkan.Queue) {
        Logger::Log("FVulkanCombineDepthEffect::RunCombinePass: Vulkan/Interop not initialized");
        return;
    }

    // Get resolution from world depth
    D3DSURFACE_DESC DepthDesc{};
    WorldDepthSurface.D3DSurface->GetDesc(&DepthDesc);

    // Make sure combined surface exists with matching size
    RecreateCombinedSurfaceIfNeeded(DepthDesc.Width, DepthDesc.Height);
    if (!CombinedDepthSurface.Image || CombinedDepthSurface.View == VK_NULL_HANDLE) {
        Logger::Log("FVulkanCombineDepthEffect::RunCombinePass: CombinedDepthSurface not valid");
        return;
    }

    // -----------------------------
    // Update descriptor set
    // -----------------------------
    VkDescriptorImageInfo WorldInfo{};
    WorldInfo.sampler = Vulkan.SamplerPointClamp;
    WorldInfo.imageView = WorldDepthSurface.View;
    WorldInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //Alterantive - VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL

    VkDescriptorImageInfo ViewModelInfo{};
    ViewModelInfo.sampler = Vulkan.SamplerPointClamp;
    ViewModelInfo.imageView = bHasViewModelDepth && ViewModelDepthSurface.View
        ? ViewModelDepthSurface.View
        : WorldDepthSurface.View; // fallback is safe
    ViewModelInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    //Alterantive - VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL

    VkDescriptorImageInfo OutInfo{};
    OutInfo.imageView = CombinedDepthSurface.View;
    OutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;  // we’ll use it as storage

    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = DescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &WorldInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = DescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &ViewModelInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = DescSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &OutInfo;

    p_vkUpdateDescriptorSets(Vulkan.Device, 3, writes, 0, nullptr);

    // -----------------------------
    // Build push constants
    // -----------------------------
    FPushConstants pc{};
    pc.DepthConstants[0] = TheRenderManager->DepthConstants.x;
    pc.DepthConstants[1] = TheRenderManager->DepthConstants.y;
    pc.DepthConstants[2] = TheRenderManager->DepthConstants.z;
    pc.DepthConstants[3] = TheRenderManager->DepthConstants.w;

    pc.CameraData[0] = TheRenderManager->CameraData.x;
    pc.CameraData[1] = TheRenderManager->CameraData.y;
    pc.CameraData[2] = TheRenderManager->CameraData.z;
    pc.CameraData[3] = TheRenderManager->CameraData.w;

    memcpy(pc.InvProjection,
        TheRenderManager->InvViewProjMatrix,
        sizeof(float) * 16);

    // -----------------------------
    // Lock DXVK submission queue
    // -----------------------------
    Interop->LockSubmissionQueue();

    // -----------------------------
    // Allocate transient command buffer
    // -----------------------------
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAlloc.commandPool = Vulkan.CmdPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VK_CHECK(p_vkAllocateCommandBuffers(Vulkan.Device, &cbAlloc, &cmd),
        "vkAllocateCommandBuffers(CombineDepth)");

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(p_vkBeginCommandBuffer(cmd, &beginInfo),
        "vkBeginCommandBuffer(CombineDepth)");

    // -----------------------------
    // Make sure combined image is in GENERAL for storage usage
    // -----------------------------
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    // We don't trust Layout from GetVulkanImageInfo; treat as "don't care"
    VkImageMemoryBarrier toGeneral{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    toGeneral.srcAccessMask = 0;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // conservative
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = CombinedDepthSurface.Image;
    toGeneral.subresourceRange = range;

    p_vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toGeneral
    );

    // Optional: debug clear so we can see something even if dispatch fails
    //VkClearColorValue clearColor = { { 0.0f, 1.0f, 0.0f, 1.0f } };
    //p_vkCmdClearColorImage(
    //    cmd,
    //    CombinedDepthSurface.Image,
    //    VK_IMAGE_LAYOUT_GENERAL,
    //    &clearColor,
    //    1,
    //    &range);

    // -----------------------------
    // Bind pipeline & descriptors
    // -----------------------------
    p_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
    p_vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        PipelineLayout,
        0,
        1,
        &DescSet,
        0,
        nullptr);

    p_vkCmdPushConstants(
        cmd,
        PipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(FPushConstants),
        &pc);

    uint32_t wgX = 16;
    uint32_t wgY = 16;
    uint32_t groupsX = (DepthDesc.Width + wgX - 1) / wgX;
    uint32_t groupsY = (DepthDesc.Height + wgY - 1) / wgY;

    p_vkCmdDispatch(cmd, groupsX, groupsY, 1);

    VK_CHECK(p_vkEndCommandBuffer(cmd),
        "vkEndCommandBuffer(CombineDepth)");

    // -----------------------------
    // Submit + wait (fence)
    // -----------------------------
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VK_CHECK(p_vkCreateFence(Vulkan.Device, &fenceInfo, nullptr, &fence),
        "vkCreateFence(CombineDepth)");

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkResult subRes = p_vkQueueSubmit(Vulkan.Queue, 1, &submit, fence);
    if (subRes != VK_SUCCESS) {
        Logger::Log("FVulkanCombineDepthEffect::RunCombinePass: vkQueueSubmit failed (%d)", subRes);
    }
    else {
        p_vkWaitForFences(Vulkan.Device, 1, &fence, VK_TRUE, UINT64_MAX);
    }

    p_vkDestroyFence(Vulkan.Device, fence, nullptr);
    p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &cmd);

    // We **don’t** try to track layout, we just know we left it in GENERAL
    CombinedDepthSurface.Layout = VK_IMAGE_LAYOUT_GENERAL;

    // -----------------------------
    // Unlock queue
    // -----------------------------
    Interop->ReleaseSubmissionQueue();

    Logger::Log("FVulkanCombineDepthEffect::RunCombinePass: dispatch finished");
}

void FVulkanCombineDepthEffect::DebugRunOnNvrCombinedDepth(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn();

    if (!SceneColor || !TheShaderManager || !TheShaderManager->Effects.CombineDepth)
        return;

    // 2) Make sure our output surface (CombinedDepthSurface) exists and matches size
    FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
    if (!VulkanDepthSurface)
    {
        Logger::Log("DebugRunOnNvrCombinedDepth: VulkanDepthSurface is invalid");
        return;
    }
    RecreateCombinedSurfaceIfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!CombinedDepthSurface.D3DSurface || !CombinedDepthSurface.Image || !CombinedDepthSurface.View) {
        Logger::Log("DebugRunOnNvrCombinedDepth: CombinedDepthSurface not valid");
        return;
    }

    // 4) Grab image info and layouts for both textures via interop
    VkImage srcImage = VK_NULL_HANDLE;
    VkImage dstImage = VK_NULL_HANDLE;
    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageCreateInfo srcInfo{};
    VkImageCreateInfo dstInfo{};

    // DEST (our CombinedDepthSurface)
    {
        dxvk::Com<ID3D9VkInteropTexture> dstInterop = CombinedDepthSurface.InteropTex;
        if (!dstInterop.ptr() && CombinedDepthSurface.D3DSurface) {
            CombinedDepthSurface.D3DSurface->QueryInterface(__uuidof(ID3D9VkInteropTexture),
                (void**)&dstInterop);
            CombinedDepthSurface.InteropTex = dstInterop;
        }

        if (dstInterop.ptr()) {
            dstInterop->GetVulkanImageInfo(&dstImage, &dstLayout, &dstInfo);
        }

        if (!dstImage) {
            Logger::Log("DebugRunOnNvrCombinedDepth: dstImage invalid");
            return;
        }
    }

    // 5) Lock submission queue (DXVK requirement)
    TheVulkanEffectsManager->VulkanContext.InteropDevice->LockSubmissionQueue();

    // 6) Transition layouts using DXVK helper (like your TestVkShader)
    VkImageSubresourceRange srcRange{};
    srcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    srcRange.baseMipLevel = 0;
    srcRange.levelCount = srcInfo.mipLevels ? srcInfo.mipLevels : 1;
    srcRange.baseArrayLayer = 0;
    srcRange.layerCount = srcInfo.arrayLayers ? srcInfo.arrayLayers : 1;

    VkImageSubresourceRange dstRange{};
    dstRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    dstRange.baseMipLevel = 0;
    dstRange.levelCount = dstInfo.mipLevels ? dstInfo.mipLevels : 1;
    dstRange.baseArrayLayer = 0;
    dstRange.layerCount = dstInfo.arrayLayers ? dstInfo.arrayLayers : 1;

    VkImageLayout srcLayoutCompute = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout dstLayoutCompute = VK_IMAGE_LAYOUT_GENERAL;

    // 7) Update descriptor set (binding 0: sampler, binding 1: storage)
    VkDescriptorImageInfo srcInfoImg{};
    srcInfoImg.sampler = TheVulkanEffectsManager->VulkanContext.SamplerPointClamp;
    srcInfoImg.imageView = VulkanDepthSurface->View;
    srcInfoImg.imageLayout = srcLayoutCompute;

    VkDescriptorImageInfo dstInfoImg{};
    dstInfoImg.imageView = CombinedDepthSurface.View;
    dstInfoImg.imageLayout = dstLayoutCompute;

    VkWriteDescriptorSet writes[2] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = DescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &srcInfoImg;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = DescSet;
    writes[1].dstBinding = 1; // storage image binding in your debug shader
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &dstInfoImg;

    p_vkUpdateDescriptorSets(TheVulkanEffectsManager->VulkanContext.Device, 2, writes, 0, nullptr);

    // 8) Allocate a transient command buffer (like TestVkShader::RunCompute)
    VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAlloc.commandPool = TheVulkanEffectsManager->VulkanContext.CmdPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkResult vr = p_vkAllocateCommandBuffers(TheVulkanEffectsManager->VulkanContext.Device, &cbAlloc, &cmd);
    if (vr != VK_SUCCESS || !cmd) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkAllocateCommandBuffers failed rv=%d cmd=%p", vr, cmd);
        TheVulkanEffectsManager->VulkanContext.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vr = p_vkBeginCommandBuffer(cmd, &beginInfo);
    if (vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkBeginCommandBuffer failed rv=%d", vr);
        p_vkFreeCommandBuffers(TheVulkanEffectsManager->VulkanContext.Device, TheVulkanEffectsManager->VulkanContext.CmdPool, 1, &cmd);
        TheVulkanEffectsManager->VulkanContext.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    // 9) Bind pipeline and descriptor set
    p_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
    p_vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        PipelineLayout,
        0, 1, &DescSet,
        0, nullptr);

    // If your debug shader has push constants, set them here.
    // (For now we assume none.)

    const uint32_t wgX = 16;
    const uint32_t wgY = 16;
    uint32_t groupsX = (VulkanDepthSurface->Width + wgX - 1) / wgX;
    uint32_t groupsY = (VulkanDepthSurface->Height + wgY - 1) / wgY;

    p_vkCmdDispatch(cmd, groupsX, groupsY, 1);

    vr = p_vkEndCommandBuffer(cmd);
    if (vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkEndCommandBuffer failed rv=%d", vr);
        p_vkFreeCommandBuffers(TheVulkanEffectsManager->VulkanContext.Device, TheVulkanEffectsManager->VulkanContext.CmdPool, 1, &cmd);
        TheVulkanEffectsManager->VulkanContext.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    // 10) Submit with a fence (like TestVkShader)
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vr = p_vkCreateFence(TheVulkanEffectsManager->VulkanContext.Device, &fenceInfo, nullptr, &fence);
    if (vr != VK_SUCCESS || !fence) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkCreateFence failed rv=%d fence=%p", vr, fence);
        p_vkFreeCommandBuffers(TheVulkanEffectsManager->VulkanContext.Device, TheVulkanEffectsManager->VulkanContext.CmdPool, 1, &cmd);
        TheVulkanEffectsManager->VulkanContext.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vr = p_vkQueueSubmit(TheVulkanEffectsManager->VulkanContext.Queue, 1, &submit, fence);
    if (vr != VK_SUCCESS) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkQueueSubmit failed rv=%d", vr);
    }
    else {
        vr = p_vkWaitForFences(TheVulkanEffectsManager->VulkanContext.Device, 1, &fence, VK_TRUE, UINT64_MAX);
        Logger::Log("DebugRunOnNvrCombinedDepth: vkWaitForFences rv=%d", vr);
    }

    p_vkDestroyFence(TheVulkanEffectsManager->VulkanContext.Device, fence, nullptr);
    p_vkFreeCommandBuffers(TheVulkanEffectsManager->VulkanContext.Device, TheVulkanEffectsManager->VulkanContext.CmdPool, 1, &cmd);

    // 12) Unlock queue
    TheVulkanEffectsManager->VulkanContext.InteropDevice->ReleaseSubmissionQueue();

    // 13) Blit result to SceneColor so we can see it
    IDirect3DDevice9* Device = TheVulkanEffectsManager->D3D9Device;
    if (Device && CombinedDepthSurface.D3DSurface) {
        HRESULT hrBlit = Device->StretchRect(
            CombinedDepthSurface.D3DSurface, nullptr,
            SceneColor, nullptr,
            D3DTEXF_POINT);
        Logger::Log("DebugRunOnNvrCombinedDepth: StretchRect hr=0x%08X", hrBlit);
    }
}



void FVulkanCombineDepthEffect::RenderPreTonemapping(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn()

    DebugRunOnNvrCombinedDepth(SceneColor);
}