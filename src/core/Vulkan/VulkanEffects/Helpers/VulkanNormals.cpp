#include "VulkanNormals.h"

//Factory macro:
REGISTER_VULKAN_EFFECT(FVulkanNormals,
    EVulkanEffectPhase::PreTonemap,
    0); // order

FVulkanNormals::FVulkanNormals()
{
    SpirvPath = "Data\\Shaders\\NewVegasReloaded\\Vulkan\\Normals.comp.spv";
}

void FVulkanNormals::DestroyResources()
{
    DXVK_CheckReturn()
        IVulkanEffect::DestroyResources();
    // Interop surfaces cleanup
    TheVulkanEffectsManager->InteropManager.DestroySurface(OutputSurface);
}

void FVulkanNormals::CreatePipeline()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // --- 1) Local descriptor set layout (set = 1) ---
    // binding 0: output storage image (uNormalsOut)
    // binding 1: input  storage image (uNormalsIn, TemporarySurface)
    VkDescriptorSetLayoutBinding bindings[2]{};

    // binding 0: storage image (uNormalsOut)
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    // binding 1: combined image sampler (uNormalsIn)
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslInfo.bindingCount = 2;
    dslInfo.pBindings    = bindings;

    VK_CHECK(p_vkCreateDescriptorSetLayout(Device, &dslInfo, nullptr, &EffectDescriptorSetLayout),
             "vkCreateDescriptorSetLayout(VulkanNormals)");

    // --- 2) Pipeline layout (unchanged, just uses the new EffectDescriptorSetLayout) ---
    VkPushConstantRange PcRange{};
    PcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    PcRange.offset     = 0;
    PcRange.size       = sizeof(FPushConstants);

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
        "vkCreatePipelineLayout(VulkanNormals)");

    // --- 3) Compute pipeline (same shader module; you’ll run it twice) ---
    VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    Stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    Stage.module = EffectShaderModule;
    Stage.pName  = "main";

    VkComputePipelineCreateInfo CpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    CpInfo.stage  = Stage;
    CpInfo.layout = EffectPipelineLayout;

    VK_CHECK(p_vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &CpInfo, nullptr, &EffectPipeline),
        "vkCreateComputePipelines(VulkanNormals)");
}


void FVulkanNormals::CreateDescriptorSets()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    VkDescriptorPoolSize poolSizes[2]{};

    // storage image for uNormalsOut
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 2; // plenty for one set

    // combined sampler for uNormalsIn
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;

    VK_CHECK(p_vkCreateDescriptorPool(Device, &poolInfo, nullptr, &EffectDescriptorPool),
             "vkCreateDescriptorPool(VulkanNormals)");

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = EffectDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &EffectDescriptorSetLayout;

    VK_CHECK(p_vkAllocateDescriptorSets(Device, &allocInfo, &EffectDescriptorSet),
             "vkAllocateDescriptorSets(VulkanNormals)");
}

void FVulkanNormals::UpdateDescriptorsForPass(uint32_t InPass)
{
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    FVulkanInteropSurface* src = nullptr;
    FVulkanInteropSurface* dst = nullptr;

    switch (InPass) {
    case 0: // reconstruct: write to OutputSurface
        dst = &OutputSurface;
        src = &OutputSurface; // bound but unused
        break;
    case 1: // horizontal blur: OutputSurface -> TemporarySurface
        dst = &TemporarySurface;
        src = &OutputSurface;
        break;
    case 2: // vertical blur: TemporarySurface -> OutputSurface (final)
    default:
        dst = &OutputSurface;
        src = &TemporarySurface;
        break;
    }

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = dst->View;
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo inInfo{};
    inInfo.imageView   = src->View;
    inInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    // use a point-clamp sampler (equivalent to MAG/MIN = NONE, CLAMP)
    inInfo.sampler     = VULKAN_CONTEXT.SamplerPointClamp;
    // ^ replace with your actual sampler accessor

    VkWriteDescriptorSet writes[2]{};

    // binding 0: uNormalsOut (storage image)
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = EffectDescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo      = &outInfo;

    // binding 1: uNormalsIn (combined sampler)
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = EffectDescriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo      = &inInfo;

    p_vkUpdateDescriptorSets(Device, 2, writes, 0, nullptr);
}


void FVulkanNormals::CreateInteropTextures()
{
    DXVK_CheckReturn()

    OutputSurface = FVulkanInteropSurface{};
    CreateOutputSurfaceIfNeeded(TheRenderManager->width, TheRenderManager->height);
}

void FVulkanNormals::CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height)
{
    DXVK_CheckReturn()
    
    if (Width == 0 || Height == 0)
    {
        Logger::Log("FVulkanNormals::CreateOutputSurfaceIfNeeded - Width = %d, Height = %d", Width, Height);
        return;
    }
    if (OutputSurface.IsValid())
    {
        D3DSURFACE_DESC Desc{};
        OutputSurface.D3DSurface->GetDesc(&Desc);
        if (OutputSurface.D3DSurface) {
            // Check if the existing size matches
            if (Desc.Width == Width && Desc.Height == Height)
            {
                return; // already correct
            }

        }
    }

    TheVulkanEffectsManager->InteropManager.DestroySurface(OutputSurface);

    // Combined depth is a color-like FP target: we want storage usage.
    
    TheVulkanEffectsManager->InteropManager.CreateSurface(
        OutputSurface,
        Width,
        Height,
        D3DFMT_A16B16G16R16F,   // COLOR FP16
        /*bUseStorage=*/true    // Compute shader writes allowed
        , VK_IMAGE_LAYOUT_GENERAL
    );
}

void FVulkanNormals::CreateTemporaryPassIfNeeded(uint32_t Width, uint32_t Height)
{
    DXVK_CheckReturn()
    
if (Width == 0 || Height == 0)
{
    Logger::Log("FVulkanNormals::CreateTemporaryPassIfNeeded - Width = %d, Height = %d", Width, Height);
    return;
}
    if (TemporarySurface.IsValid())
    {
        D3DSURFACE_DESC Desc{};
        TemporarySurface.D3DSurface->GetDesc(&Desc);
        if (TemporarySurface.D3DSurface) {
            // Check if the existing size matches
            if (Desc.Width == Width && Desc.Height == Height)
            {
                return; // already correct
            }

        }
    }

    TheVulkanEffectsManager->InteropManager.DestroySurface(TemporarySurface);

    // Combined depth is a color-like FP target: we want storage usage.
    
    TheVulkanEffectsManager->InteropManager.CreateSurface(
        TemporarySurface,
        Width,
        Height,
        D3DFMT_A16B16G16R16F,   // COLOR FP16
        /*bUseStorage=*/true,    // Compute shader writes allowed
        VK_IMAGE_LAYOUT_GENERAL
    );
}

void FVulkanNormals::SubmitRendering()
{
    DXVK_CheckReturn();

    FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
    if (!VulkanDepthSurface || !VulkanDepthSurface->View) {
        Logger::Log("FVulkanNormals::SubmitRendering: VulkanDepthSurface is invalid");
        return;
    }

    // Create / validate output + temp surfaces
    CreateOutputSurfaceIfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!OutputSurface.D3DSurface || !OutputSurface.Image || !OutputSurface.View) {
        Logger::Log("FVulkanNormals::SubmitRendering: OutputSurface not valid");
        return;
    }

    CreateTemporaryPassIfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
    if (!TemporarySurface.D3DSurface || !TemporarySurface.Image || !TemporarySurface.View) {
        Logger::Log("FVulkanNormals::SubmitRendering: TemporarySurface not valid");
        return;
    }

    // Lock queue & reset command buffer
    VULKAN_CONTEXT.InteropDevice->LockSubmissionQueue();

    VkResult vr = p_vkResetCommandBuffer(EffectCommandBuffer, 0);
    if (vr != VK_SUCCESS) {
        Logger::Log("FVulkanNormals::SubmitRendering: vkResetCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vr = p_vkBeginCommandBuffer(EffectCommandBuffer, &beginInfo);
    if (vr != VK_SUCCESS) {
        Logger::Log("FVulkanNormals::SubmitRendering: vkBeginCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    // Bind pipeline + descriptor sets
    p_vkCmdBindPipeline(EffectCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, EffectPipeline);

    VkDescriptorSet sets[2];
    sets[0] = TheVulkanEffectsManager->GlobalResources.GetGlobalDescriptorSets().GlobalFrameDescriptorSet; // set 0
    sets[1] = EffectDescriptorSet;                                                                           // set 1

    p_vkCmdBindDescriptorSets(
        EffectCommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        EffectPipelineLayout,
        0,
        2,
        sets,
        0,
        nullptr);

    const uint32_t Wgx = 16, Wgy = 16;
    uint32_t groupsX = (VulkanDepthSurface->Width  + Wgx - 1) / Wgx;
    uint32_t groupsY = (VulkanDepthSurface->Height + Wgy - 1) / Wgy;

    // -------- PASS LOOP: 0 = reconstruct, 1 = blur H, 2 = blur V --------
    for (uint32_t pass = 0; pass < 3; ++pass) {
        UpdateDescriptorsForPass(pass);

        p_vkCmdBindDescriptorSets(
            EffectCommandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            EffectPipelineLayout,
            0,
            2,
            sets,
            0,
            nullptr);
        
        FPushConstants pc{};
        pc.Pass = pass;

        p_vkCmdPushConstants(
            EffectCommandBuffer,
            EffectPipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(FPushConstants),
            &pc);

        p_vkCmdDispatch(EffectCommandBuffer, groupsX, groupsY, 1);

        // Barriers between passes where the previous pass's output becomes the next pass's input
        if (pass == 0) {
            // Pass 0 wrote OutputSurface -> will be sampled as input in pass 1
            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image         = OutputSurface.Image;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            p_vkCmdPipelineBarrier(
                EffectCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        }
        else if (pass == 1) {
            // Pass 1 wrote TemporarySurface -> will be sampled as input in pass 2
            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image         = TemporarySurface.Image;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            p_vkCmdPipelineBarrier(
                EffectCommandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        }
    }
    vr = p_vkEndCommandBuffer(EffectCommandBuffer);
    if (vr != VK_SUCCESS) {
        Logger::Log("FVulkanNormals::SubmitRendering: vkEndCommandBuffer failed rv=%d", vr);
        VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
        return;
    }

    TRY_APPLY_FENCE(this);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &EffectCommandBuffer;

    vr = p_vkQueueSubmit(VULKAN_CONTEXT.Queue, 1, &submit, EffectFence);
    if (vr != VK_SUCCESS) {
        Logger::Log("FVulkanNormals::SubmitRendering: vkQueueSubmit failed rv=%d", vr);
    }

    VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
    TRY_DEBUG_END_FENCE(this);
}

void FVulkanNormals::CompleteRendering(IDirect3DSurface9* SceneColor)
{
    DXVK_CheckReturn();
    ENSURE_END_FENCE(this)

    if (TheVulkanEffectsManager->NormalsSurface.View != OutputSurface.View)
    {
        TheVulkanEffectsManager->NormalsSurface = OutputSurface;
        TheVulkanEffectsManager->GlobalResources.UpdatePerFrame();
    }
}