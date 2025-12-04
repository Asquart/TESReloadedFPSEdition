//  #include "VulkanDebugDepth.h"
//
// // If you use the factory macro:
// // REGISTER_VULKAN_EFFECT(FVulkanDebugDepthEffect,
// //     EVulkanEffectPhase::PreTonemap,
// //     3); // order: before AO etc.
//
// FVulkanDebugDepthEffect::FVulkanDebugDepthEffect()
// {
//     SpirvPath = "Data\\Shaders\\NewVegasReloaded\\Vulkan\\CombineDepth.comp.spv";
// }
//
// void FVulkanDebugDepthEffect::DestroyResources()
// {
//     DXVK_CheckReturn()
//         IVulkanEffect::DestroyResources();
//     // Interop surfaces cleanup
//     TheVulkanEffectsManager->InteropManager.DestroySurface(OutputSurface);
// }
//
// void FVulkanDebugDepthEffect::CreatePipeline()
// {
//     DXVK_CheckReturn()
//
//         FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
//     VkDevice Device = Vulkan.Device;
//
//     // --- 1) Local descriptor set layout (set = 1) ---
//     // binding 0: local output storage image
//     VkDescriptorSetLayoutBinding Binding{};
//     Binding.binding = 0;
//     Binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//     Binding.descriptorCount = 1;
//     Binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//     VkDescriptorSetLayoutCreateInfo DslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
//     DslInfo.bindingCount = 1;
//     DslInfo.pBindings = &Binding;
//
//     VK_CHECK(p_vkCreateDescriptorSetLayout(Device, &DslInfo, nullptr, &EffectDescriptorSetLayout),
//         "vkCreateDescriptorSetLayout(CombineDepth)");
//
//     // --- 2) Pipeline layout: [ set 0 = global frame, set 1 = local (this effect) ] ---
//     VkPushConstantRange PcRange{};
//     PcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//     PcRange.offset = 0;
//     PcRange.size = sizeof(FPushConstants); // keep your existing push-constant size
//
//     VkDescriptorSetLayout SetLayouts[2] = {
//         TheVulkanEffectsManager->GlobalResources.GetGlobalDescriptorSets().GlobalFrameSetLayout, // set = 0
//         EffectDescriptorSetLayout                                                            // set = 1
//     };
//
//     VkPipelineLayoutCreateInfo PlInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
//     PlInfo.setLayoutCount = 2;
//     PlInfo.pSetLayouts = SetLayouts;
//     PlInfo.pushConstantRangeCount = 1;
//     PlInfo.pPushConstantRanges = &PcRange;
//
//     VK_CHECK(p_vkCreatePipelineLayout(Device, &PlInfo, nullptr, &EffectPipelineLayout),
//         "vkCreatePipelineLayout(CombineDepth)");
//
//     // --- 3) Compute pipeline ---
//     VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
//     Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//     Stage.module = EffectShaderModule;
//     Stage.pName = "main";
//
//     VkComputePipelineCreateInfo CpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
//     CpInfo.stage = Stage;
//     CpInfo.layout = EffectPipelineLayout;
//
//     VK_CHECK(p_vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &CpInfo, nullptr, &EffectPipeline),
//         "vkCreateComputePipelines(CombineDepth)");
// }
//
// void FVulkanDebugDepthEffect::CreateDescriptorSets()
// {
//     DXVK_CheckReturn()
//
//     FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
//     VkDevice Device = Vulkan.Device;
//
//     // Only one descriptor type: STORAGE_IMAGE (for uOutImage at set=1, binding=0)
//     VkDescriptorPoolSize PoolSize{};
//     PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//     PoolSize.descriptorCount = 1;
//
//     VkDescriptorPoolCreateInfo PoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
//     PoolInfo.maxSets = 1;
//     PoolInfo.poolSizeCount = 1;
//     PoolInfo.pPoolSizes = &PoolSize;
//
//     VK_CHECK(p_vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &EffectDescriptorPool),
//         "vkCreateDescriptorPool(CombineDepth)");
//
//     VkDescriptorSetAllocateInfo AllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
//     AllocInfo.descriptorPool = EffectDescriptorPool;
//     AllocInfo.descriptorSetCount = 1;
//     AllocInfo.pSetLayouts = &EffectDescriptorSetLayout;
//
//     VK_CHECK(p_vkAllocateDescriptorSets(Device, &AllocInfo, &EffectDescriptorSet),
//         "vkAllocateDescriptorSets(CombineDepth)");
// }
//
// void FVulkanDebugDepthEffect::CreateInteropTextures()
// {
//     DXVK_CheckReturn()
//
//     OutputSurface = FVulkanInteropSurface{};
//     CreateOutputSurfaceIfNeeded(TheRenderManager->width, TheRenderManager->height);
// }
//
// void FVulkanDebugDepthEffect::CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height)
// {
//     DXVK_CheckReturn()
//
//     if (Width == 0 || Height == 0)
//     {
//         Logger::Log("FVulkanCombineDepthEffect::RecreateCombinedSurfaceIfNeeded - Width = %d, Height = %d", Width, Height);
//         return;
//     }
//     if (OutputSurface.IsValid())
//     {
//         D3DSURFACE_DESC Desc{};
//         OutputSurface.D3DSurface->GetDesc(&Desc);
//         if (OutputSurface.D3DSurface) {
//             // Check if the existing size matches
//             if (Desc.Width == Width && Desc.Height == Height)
//             {
//                 return; // already correct
//             }
//
//         }
//     }
//
//     TheVulkanEffectsManager->InteropManager.DestroySurface(OutputSurface);
//
//     // Combined depth is a color-like FP target: we want storage usage.
//     TheVulkanEffectsManager->InteropManager.CreateSurface(
//         OutputSurface,
//         Width,
//         Height,
//         D3DFMT_A32B32G32R32F,   // COLOR FP32
//         /*bUseStorage=*/true    // Compute shader writes allowed
//     );
// }
//
// void FVulkanDebugDepthEffect::SubmitRendering()
// {
//     DXVK_CheckReturn();
//
//     // 1) Ensure our Vulkan depth surface (NVR combined depth) exists
//     FVulkanInteropSurface* VulkanDepthSurface = TheVulkanEffectsManager->GetDepthSurface();
//     if (!VulkanDepthSurface || !VulkanDepthSurface->View) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: VulkanDepthSurface is invalid");
//         return;
//     }
//
//     // 2) Make sure output surface exists & matches size
//     CreateOutputSurfaceIfNeeded(VulkanDepthSurface->Width, VulkanDepthSurface->Height);
//     if (!OutputSurface.D3DSurface || !OutputSurface.Image || !OutputSurface.View) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: CombinedDepthSurface not valid");
//         return;
//     }
//
//     // 3) Update global frame resources (UBO + depth sampler in set=0)
//     //TheVulkanEffectsManager->GlobalResources.UpdatePerFrame();
//
//     // 4) Hook our output image into this effect's descriptor set (set=1, binding=0)
//     VkDescriptorImageInfo OutInfo{};
//     OutInfo.imageView = OutputSurface.View;
//     OutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // compute writes
//
//     VkWriteDescriptorSet Write{};
//     Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     Write.dstSet = EffectDescriptorSet;                 // this effect's set=1
//     Write.dstBinding = 0;                  // binding 0: uOutImage
//     Write.descriptorCount = 1;
//     Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//     Write.pImageInfo = &OutInfo;
//
//     p_vkUpdateDescriptorSets(TheVulkanEffectsManager->VulkanContext.Device, 1, &Write, 0, nullptr);
//
//     // 5) Allocate transient command buffer
//     VULKAN_CONTEXT.InteropDevice->LockSubmissionQueue();
//
//     VkResult Vr = p_vkResetCommandBuffer(EffectCommandBuffer, 0);
//     if (Vr != VK_SUCCESS) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: vkResetCommandBuffer failed rv=%d", Vr);
//         VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
//         return;
//     }
//
//     VkCommandBufferBeginInfo BeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
//     BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//     Vr = p_vkBeginCommandBuffer(EffectCommandBuffer, &BeginInfo);
//     if (Vr != VK_SUCCESS) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: vkBeginCommandBuffer failed rv=%d", Vr);
//         VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
//         return;
//     }
//
//     // 6) Bind pipeline + both descriptor sets (set 0 = global, set 1 = local)
//     p_vkCmdBindPipeline(EffectCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, EffectPipeline);
//
//     VkDescriptorSet Sets[2];
//     Sets[0] = TheVulkanEffectsManager->GlobalResources.GetGlobalDescriptorSets().GlobalFrameDescriptorSet; // set 0
//     Sets[1] = EffectDescriptorSet;                                                           // set 1
//
//     p_vkCmdBindDescriptorSets(
//         EffectCommandBuffer,
//         VK_PIPELINE_BIND_POINT_COMPUTE,
//         EffectPipelineLayout,
//         0,         // firstSet
//         2,         // descriptorSetCount
//         Sets,
//         0, nullptr);
//
//     // 7) Dispatch
//     const uint32_t Wgx = 16, Wgy = 16;
//     uint32_t GroupsX = (VulkanDepthSurface->Width + Wgx - 1) / Wgx;
//     uint32_t GroupsY = (VulkanDepthSurface->Height + Wgy - 1) / Wgy;
//
//     p_vkCmdDispatch(EffectCommandBuffer, GroupsX, GroupsY, 1);
//
//     Vr = p_vkEndCommandBuffer(EffectCommandBuffer);
//     if (Vr != VK_SUCCESS) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: vkEndCommandBuffer failed rv=%d", Vr);
//         VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
//         return;
//     }
//
//     TRY_APPLY_FENCE(this)
//         VkSubmitInfo Submit {
//         VK_STRUCTURE_TYPE_SUBMIT_INFO
//     };
//     Submit.commandBufferCount = 1;
//     Submit.pCommandBuffers = &EffectCommandBuffer;
//
//     Vr = p_vkQueueSubmit(VULKAN_CONTEXT.Queue, 1, &Submit, EffectFence);
//     if (Vr != VK_SUCCESS) {
//         Logger::Log("DebugRunOnNvrCombinedDepth: vkQueueSubmit failed rv=%d", Vr);
//     }
//
//     VULKAN_CONTEXT.InteropDevice->ReleaseSubmissionQueue();
//
//     TRY_DEBUG_END_FENCE(this)
// }
//
// void FVulkanDebugDepthEffect::CompleteRendering(IDirect3DSurface9* SceneColor)
// {
//     DXVK_CheckReturn();
//     ENSURE_END_FENCE(this)
//
//     // 9) R to SceneColor for on-screen debug
//     IDirect3DDevice9* Device9 = TheVulkanEffectsManager->D3D9Device;
//     if (Device9 && OutputSurface.D3DSurface) {
//         HRESULT HrBlit = Device9->StretchRect(
//             OutputSurface.D3DSurface, nullptr,
//             SceneColor, nullptr,
//             D3DTEXF_POINT);
//     }
// }