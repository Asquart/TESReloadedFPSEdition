#include "VulkanAO.h"

//REGISTER_VULKAN_EFFECT(FVulkanAmbientOcclusionEffect,
//    EVulkanEffectPhase::PreTonemap,
//    10);

//void FVulkanAmbientOcclusionEffect::Initialize()
//{
//    UpdateSettingsFromNvr();
//    CreateResources();
//
//    bInitialized = true;
//}
//
//void FVulkanAmbientOcclusionEffect::RenderPreTonemapping(IDirect3DSurface9* SceneColor)
//{
//    DXVK_CheckReturn()
//    if (!bInitialized || !SceneColor)
//        return;
//
//    // 2) Copy scene color into InputSurface (source buffer)
//    TheVulkanEffectsManager->D3D9Device->StretchRect(SceneColor, nullptr, InputSurface->D3DSurface, nullptr, D3DTEXF_POINT);
//
//    // 3) SSAO passes
//
//    // Pass 1: OffsetMask = (1, 0) – ignore previous AO
//    {
//        FPushConstants Pc{};
//        Pc.OffsetMaskX = 1.0f;
//        Pc.OffsetMaskY = 0.0f;
//
//        RunSsaoPass(*InputSurface, *AoPrevSurface, *AoOutputSurface, Pc);
//
//        // Copy AO output into AoPrev for use in second pass
//        TheVulkanEffectsManager->D3D9Device->StretchRect(AoOutputSurface->D3DSurface, nullptr,
//            AoPrevSurface->D3DSurface, nullptr, D3DTEXF_POINT);
//    }
//
//    // Pass 2: OffsetMask = (0, 1) – uses AoPrev as previous AO
//    {
//        FPushConstants Pc{};
//        Pc.OffsetMaskX = 0.0f;
//        Pc.OffsetMaskY = 1.0f;
//
//        RunSsaoPass(*InputSurface, *AoPrevSurface, *AoOutputSurface, Pc);
//    }
//
//    // 4) Copy final AO result back onto SceneColor with simple modulation
//    // If you want exact Combine() behavior, make a second compute shader that matches Combine.
//    // For now, we do color *= AO.r here on CPU side via another compute or a simple DrawPrimitive in DX9.
//    // Easiest path for now: just overwrite SceneColor with AO visualization:
//    TheVulkanEffectsManager->D3D9Device->StretchRect(AoOutputSurface->D3DSurface, nullptr,
//        SceneColor, nullptr, D3DTEXF_POINT);
//}
//
//FVulkanAmbientOcclusionEffect::~FVulkanAmbientOcclusionEffect()
//{
//    DestroyResources();
//}
//
//void FVulkanAmbientOcclusionEffect::CreateResources()
//{
//    if (!TheVulkanEffectsManager->VulkanContext.Device)
//        return;
//
//    CreateShaderModule();
//    CreatePipeline();
//    CreateDescriptorSets();
//    CreateInteropTextures();
//}
//
//void FVulkanAmbientOcclusionEffect::DestroyResources()
//{
//    FVulkanContext* Vulkan = &TheVulkanEffectsManager->VulkanContext;
//    if (!Vulkan || !Vulkan->Device)
//        return;
//
//    if (PipelineSsao) {
//        p_vkDestroyPipeline(Vulkan->Device, PipelineSsao, nullptr);
//        PipelineSsao = VK_NULL_HANDLE;
//    }
//
//    if (PipelineLayoutSsao) {
//        p_vkDestroyPipelineLayout(Vulkan->Device, PipelineLayoutSsao, nullptr);
//        PipelineLayoutSsao = VK_NULL_HANDLE;
//    }
//
//    if (DescSetLayoutSsao) {
//        p_vkDestroyDescriptorSetLayout(Vulkan->Device, DescSetLayoutSsao, nullptr);
//        DescSetLayoutSsao = VK_NULL_HANDLE;
//    }
//
//    if (ShaderModuleSsao) {
//        p_vkDestroyShaderModule(Vulkan->Device, ShaderModuleSsao, nullptr);
//        ShaderModuleSsao = VK_NULL_HANDLE;
//    }
//
//    if (DescPool) {
//        p_vkDestroyDescriptorPool(Vulkan->Device, DescPool, nullptr);
//        DescPool = VK_NULL_HANDLE;
//    }
//}
//
//void FVulkanAmbientOcclusionEffect::RunSsaoPass(FVulkanInteropSurface& SourceColor, FVulkanInteropSurface& AoPrev, FVulkanInteropSurface& AoOutput, const FPushConstants& Push)
//{
//    //FVulkanContext* VulkanContext = &TheVulkanEffectsManager->VulkanContext;
//    //if (!VulkanContext)
//    //    return;
//
//    //// Ensure we have valid image views on the interop surfaces
//    //if (SourceColor.View == VK_NULL_HANDLE ||
//    //    AoPrev.View == VK_NULL_HANDLE ||
//    //    AoOutput.View == VK_NULL_HANDLE)
//    //{
//    //    Logger::Log("FVulkanAmbientOcclusionEffect::RunSsaoPass: missing image views");
//    //    return;
//    //}
//
//    //// Layout transitions handled by interop helper; assume images are in correct layouts.
//    //// Bind pipeline & descriptors
//    //p_vkCmdBindPipeline(VulkanContext->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineSsao);
//    //p_vkCmdBindDescriptorSets(
//    //    VulkanContext->CommandBuffer,
//    //    VK_PIPELINE_BIND_POINT_COMPUTE,
//    //    PipelineLayoutSsao,
//    //    0, 1, &DescSet,
//    //    0, nullptr);
//
//    //// Push constants
//    //p_vkCmdPushConstants(
//    //    VulkanContext->CommandBuffer,
//    //    PipelineLayoutSsao,
//    //    VK_SHADER_STAGE_COMPUTE_BIT,
//    //    0,
//    //    sizeof(FPushConstants),
//    //    &Push);
//
//    //// Dispatch
//    //const uint32_t GroupSizeX = 16;
//    //const uint32_t GroupSizeY = 16;
//
//    //uint32_t GroupsX = (TheVulkanEffectsManager->ScreenWidth + GroupSizeX - 1) / GroupSizeX;
//    //uint32_t GroupsY = (TheVulkanEffectsManager->ScreenHeight + GroupSizeY - 1) / GroupSizeY;
//
//    //p_vkCmdDispatch(VulkanContext->CommandBuffer, GroupsX, GroupsY, 1);
//
//    //p_vkFreeDescriptorSets(VulkanContext->Device, DescPool, 1, &DescSet);
//}
//
//void FVulkanAmbientOcclusionEffect::UpdateSettingsFromNvr()
//{
//    AOSettings.AOsamples = 5.0f;
//    AOSettings.AOstrength = 1.0f;
//    AOSettings.AOclamp = 0.2f;
//    AOSettings.AOrange = 150.0f;
//    AOSettings.AOangleBias = 0.2f;
//    AOSettings.AOlumThreshold = 0.2f;
//    AOSettings.BlurDrop = 0.3f;
//    AOSettings.BlurRadius = 1.5f;
//}
//
//void FVulkanAmbientOcclusionEffect::CreateShaderModule()
//{
//    // 1) Load SSAO SPIR-V
//    std::vector<uint32_t> Spirv = LoadSpirv("Data\\Shaders\\NewVegasReloaded\\Shaders\\AO_SSAO.comp.spv");
//    if (Spirv.empty()) {
//        Logger::Log("FVulkanAmbientOcclusionEffect: failed to load AO_SSAO.comp.spv");
//        return;
//    }
//
//    VkShaderModuleCreateInfo SmInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
//    SmInfo.codeSize = Spirv.size() * sizeof(uint32_t);
//    SmInfo.pCode = Spirv.data();
//    VK_CHECK(p_vkCreateShaderModule(TheVulkanEffectsManager->VulkanContext.Device, &SmInfo, nullptr, &ShaderModuleSsao),
//        "vkCreateShaderModule(AO_SSAO)");
//}
//
//void FVulkanAmbientOcclusionEffect::CreatePipeline()
//{
//    // 2) Descriptor set layout
//    // Binding layout must match AO_SSAO.comp.glsl
//    // 0: rendered buffer (combined image sampler)
//    // 1: depth buffer    (combined image sampler)
//    // 2: normals buffer  (combined image sampler)
//    // 3: blue noise      (combined image sampler)
//    // 4: AO output       (storage image)
//
//    VkDescriptorSetLayoutBinding Bindings[5] = {};
//
//    // s0: RenderedBuffer
//    Bindings[0].binding = 0;
//    Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Bindings[0].descriptorCount = 1;
//    Bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//    // s1: DepthBuffer
//    Bindings[1].binding = 1;
//    Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Bindings[1].descriptorCount = 1;
//    Bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//    // s2: NormalsBuffer
//    Bindings[2].binding = 2;
//    Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Bindings[2].descriptorCount = 1;
//    Bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//    // s3: BlueNoise
//    Bindings[3].binding = 3;
//    Bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Bindings[3].descriptorCount = 1;
//    Bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//    // s4: AO output (storage image)
//    Bindings[4].binding = 4;
//    Bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//    Bindings[4].descriptorCount = 1;
//    Bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//
//    VkDescriptorSetLayoutCreateInfo DslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
//    DslInfo.bindingCount = 5;
//    DslInfo.pBindings = Bindings;
//
//    VK_CHECK(p_vkCreateDescriptorSetLayout(TheVulkanEffectsManager->VulkanContext.Device, &DslInfo, nullptr, &DescSetLayoutSsao),
//        "vkCreateDescriptorSetLayout(SSAO)");
//
//    // 3) Pipeline layout (push constants for OffsetMask)
//    VkPushConstantRange Pc{};
//    Pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
//    Pc.offset = 0;
//    Pc.size = sizeof(FPushConstants);
//
//    VkPipelineLayoutCreateInfo PlInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
//    PlInfo.setLayoutCount = 1;
//    PlInfo.pSetLayouts = &DescSetLayoutSsao;
//    PlInfo.pushConstantRangeCount = 1;
//    PlInfo.pPushConstantRanges = &Pc;
//
//    VK_CHECK(p_vkCreatePipelineLayout(TheVulkanEffectsManager->VulkanContext.Device, &PlInfo, nullptr, &PipelineLayoutSsao),
//        "vkCreatePipelineLayout(SSAO)");
//
//    // 4) Compute pipeline
//    VkPipelineShaderStageCreateInfo Stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
//    Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//    Stage.module = ShaderModuleSsao;
//    Stage.pName = "main";
//
//    VkComputePipelineCreateInfo CpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
//    CpInfo.stage = Stage;
//    CpInfo.layout = PipelineLayoutSsao;
//
//    VK_CHECK(p_vkCreateComputePipelines(TheVulkanEffectsManager->VulkanContext.Device, VK_NULL_HANDLE, 1, &CpInfo, nullptr, &PipelineSsao),
//        "vkCreateComputePipelines(AO)");
//}
//
//void FVulkanAmbientOcclusionEffect::CreateDescriptorSets()
//{
//    FVulkanContext& VulkanContext = TheVulkanEffectsManager->VulkanContext;
//    VkDevice Device = VulkanContext.Device;
//
//    // Bail if we don't have interop surfaces yet
//    FVulkanInteropSurface* DepthSurface = TheVulkanEffectsManager->GetDepthSurface();
//    FVulkanInteropSurface* NormalsSurface = TheVulkanEffectsManager->GetNormalsSurface();
//    FVulkanInteropSurface* NoiseSurface = TheVulkanEffectsManager->GetBlueNoiseSurface();
//
//    if (!DepthSurface || !NormalsSurface || !NoiseSurface)
//    {
//        Logger::Log("FVulkanAmbientOcclusionEffect::CreateDescriptorSets: "
//            "missing depth/normal/noise surfaces");
//        return;
//    }
//
//    // 1) Create descriptor pool (room for future sets, but we only use one for now)
//    VkDescriptorPoolSize PoolSizes[2] = {};
//    PoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    PoolSizes[0].descriptorCount = 32;
//    PoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//    PoolSizes[1].descriptorCount = 16;
//
//    VkDescriptorPoolCreateInfo PoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
//    PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
//    PoolInfo.maxSets = 32;
//    PoolInfo.poolSizeCount = 2;
//    PoolInfo.pPoolSizes = PoolSizes;
//
//    VkResult Result = p_vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &DescPool);
//    if (Result != VK_SUCCESS)
//    {
//        VK_CHECK(Result, "vkCreateDescriptorPool(AO)");
//        DescPool = VK_NULL_HANDLE;
//        return;
//    }
//
//    // 2) Allocate descriptor set
//    VkDescriptorSetAllocateInfo AllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
//    AllocInfo.descriptorPool = DescPool;
//    AllocInfo.descriptorSetCount = 1;
//    AllocInfo.pSetLayouts = &DescSetLayoutSsao;
//
//    DescSet = VK_NULL_HANDLE;
//    Result = p_vkAllocateDescriptorSets(Device, &AllocInfo, &DescSet);
//    if (Result != VK_SUCCESS)
//    {
//        VK_CHECK(Result, "vkAllocateDescriptorSets(SSAO)");
//        p_vkDestroyDescriptorPool(Device, DescPool, nullptr);
//        DescPool = VK_NULL_HANDLE;
//        return;
//    }
//
//    // 3) Fill image infos
//    VkDescriptorImageInfo RenderedInfo{}; // TESR_RenderedBuffer / AoPrev
//    RenderedInfo.sampler = VulkanContext.SamplerLinearClamp;
//    RenderedInfo.imageView = AoPrevSurface->View;
//    RenderedInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    VkDescriptorImageInfo DepthInfo{};
//    DepthInfo.sampler = VulkanContext.SamplerLinearClamp;
//    DepthInfo.imageView = DepthSurface->View;
//    DepthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    VkDescriptorImageInfo NormalsInfo{};
//    NormalsInfo.sampler = VulkanContext.SamplerPointClamp;
//    NormalsInfo.imageView = NormalsSurface->View;
//    NormalsInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    VkDescriptorImageInfo NoiseInfo{};
//    NoiseInfo.sampler = VulkanContext.SamplerLinearRepeat;
//    NoiseInfo.imageView = NoiseSurface->View;
//    NoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    VkDescriptorImageInfo AoOutInfo{};
//    AoOutInfo.sampler = VK_NULL_HANDLE; // storage images don't need a sampler
//    AoOutInfo.imageView = AoOutputSurface->View;
//    AoOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
//
//    // 4) Write descriptors
//    VkWriteDescriptorSet Writes[5] = {};
//
//    // binding 0: previous AO / rendered buffer
//    Writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//    Writes[0].dstSet = DescSet;
//    Writes[0].dstBinding = 0;
//    Writes[0].descriptorCount = 1;
//    Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Writes[0].pImageInfo = &RenderedInfo;
//
//    // binding 1: depth
//    Writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//    Writes[1].dstSet = DescSet;
//    Writes[1].dstBinding = 1;
//    Writes[1].descriptorCount = 1;
//    Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Writes[1].pImageInfo = &DepthInfo;
//
//    // binding 2: normals
//    Writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//    Writes[2].dstSet = DescSet;
//    Writes[2].dstBinding = 2;
//    Writes[2].descriptorCount = 1;
//    Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Writes[2].pImageInfo = &NormalsInfo;
//
//    // binding 3: blue noise
//    Writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//    Writes[3].dstSet = DescSet;
//    Writes[3].dstBinding = 3;
//    Writes[3].descriptorCount = 1;
//    Writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    Writes[3].pImageInfo = &NoiseInfo;
//
//    // binding 4: AO output storage image
//    Writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//    Writes[4].dstSet = DescSet;
//    Writes[4].dstBinding = 4;
//    Writes[4].descriptorCount = 1;
//    Writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//    Writes[4].pImageInfo = &AoOutInfo;
//
//    p_vkUpdateDescriptorSets(Device, 5, Writes, 0, nullptr);
//
//    Logger::Log("FVulkanAmbientOcclusionEffect::CreateDescriptorSets: descriptors created");
//}
//
//void FVulkanAmbientOcclusionEffect::CreateInteropTextures()
//{
//    InputSurface = TheVulkanEffectsManager->InteropManager.GetOrCreateSurface(InputName, TheVulkanEffectsManager->ScreenWidth, TheVulkanEffectsManager->ScreenHeight, D3DFMT_A16B16G16R16F, /*bUseStorage*/ false);
//
//    AoPrevSurface = TheVulkanEffectsManager->InteropManager.GetOrCreateSurface(AoPrevName, TheVulkanEffectsManager->ScreenWidth, TheVulkanEffectsManager->ScreenHeight, D3DFMT_A16B16G16R16F, /*bUseStorage*/ true);
//
//    AoOutputSurface = TheVulkanEffectsManager->InteropManager.GetOrCreateSurface(AoOutputName, TheVulkanEffectsManager->ScreenWidth, TheVulkanEffectsManager->ScreenHeight, D3DFMT_A16B16G16R16F, /*bUseStorage*/ true);
//}
