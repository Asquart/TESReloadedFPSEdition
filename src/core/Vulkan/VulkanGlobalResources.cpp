#include "VulkanGlobalResources.h"
#include "VulkanEffectsManager.h"

static uint32_t FindMemoryType(
    const VkPhysicalDeviceMemoryProperties& memProps,
    uint32_t typeBits,
    VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return ~0u;
}

void FVulkanGlobalResources::Initialize()
{
    DXVK_CheckReturn();

    CreateFrameSet();
}

void FVulkanGlobalResources::Shutdown()
{
    DXVK_CheckReturn();

    DestroyFrameSet();
}

void FVulkanGlobalResources::UpdatePerFrame()
{
    DXVK_CheckReturn();

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    // 1) Update UBO contents
    UpdateCpuUboParams();
    void* mapped = nullptr;
    p_vkMapMemory(Device, GlobalSets.FrameUBOMemory, 0, sizeof(FGlobalFrameUBO), 0, &mapped);
    memcpy(mapped, &GlobalSets.CpuUBO, sizeof(FGlobalFrameUBO));
    p_vkUnmapMemory(Device, GlobalSets.FrameUBOMemory);

    // 2) Depth + normals + UBO into GlobalFrameSet

    // UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = GlobalSets.FrameUBO;
    uboInfo.offset = 0;
    uboInfo.range  = sizeof(FGlobalFrameUBO);

    // Depth / Normals / SceneColor surfaces
    FVulkanInteropSurface* DepthSurface      = TheVulkanEffectsManager->GetDepthSurface();
    FVulkanInteropSurface* NormalsSurface    = TheVulkanEffectsManager->GetNormalsSurface();
    FVulkanInteropSurface* SceneColorSurface = TheVulkanEffectsManager->GetSceneColorSurface(); // <- you should already have something like this; if the name differs, adjust here.

    // Depth
    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = Vulkan.SamplerPointClamp;
    depthInfo.imageView   = DepthSurface ? DepthSurface->View : VK_NULL_HANDLE;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Normals
    VkDescriptorImageInfo normalsInfo{};
    normalsInfo.sampler     = Vulkan.SamplerPointClamp;
    normalsInfo.imageView   = NormalsSurface ? NormalsSurface->View : VK_NULL_HANDLE;
    normalsInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Scene color (pre-tonemap)
    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = Vulkan.SamplerPointClamp; // or Vulkan.SamplerLinearClamp if you have it and prefer filtered IL
    sceneInfo.imageView   = SceneColorSurface ? SceneColorSurface->View : VK_NULL_HANDLE;
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[4] = {};

    // binding 0: gDepth
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = GlobalSets.GlobalFrameDescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo      = &depthInfo;

    // binding 1: gNormals
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = GlobalSets.GlobalFrameDescriptorSet;
    writes[1].dstBinding      = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo      = &normalsInfo;

    // binding 2: GlobalFrameUBO
    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = GlobalSets.GlobalFrameDescriptorSet;
    writes[2].dstBinding      = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo     = &uboInfo;

    // binding 3: gSceneColor
    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = GlobalSets.GlobalFrameDescriptorSet;
    writes[3].dstBinding      = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo      = &sceneInfo;

    p_vkUpdateDescriptorSets(Device, 4, writes, 0, nullptr);
}


void FVulkanGlobalResources::UpdateCpuUboParams()
{
    memcpy(GlobalSets.CpuUBO.TESR_WorldTransform, TheRenderManager->worldMatrix, sizeof(GlobalSets.CpuUBO.TESR_WorldTransform));
    memcpy(GlobalSets.CpuUBO.TESR_ViewTransform, TheRenderManager->viewMatrix, sizeof(GlobalSets.CpuUBO.TESR_ViewTransform));
    memcpy(GlobalSets.CpuUBO.TESR_InvViewTransform, TheRenderManager->InvViewMatrix, sizeof(GlobalSets.CpuUBO.TESR_InvViewTransform));
    memcpy(GlobalSets.CpuUBO.TESR_ProjectionTransform, TheRenderManager->projMatrix, sizeof(GlobalSets.CpuUBO.TESR_ProjectionTransform));
    memcpy(GlobalSets.CpuUBO.TESR_InvProjectionTransform, TheRenderManager->InvProjMatrix, sizeof(GlobalSets.CpuUBO.TESR_InvProjectionTransform));
    memcpy(GlobalSets.CpuUBO.TESR_WorldViewProjectionTransform, TheRenderManager->WorldViewProjMatrix, sizeof(GlobalSets.CpuUBO.TESR_WorldViewProjectionTransform));
    memcpy(GlobalSets.CpuUBO.TESR_InvViewProjectionTransform, TheRenderManager->InvViewProjMatrix, sizeof(GlobalSets.CpuUBO.TESR_InvViewProjectionTransform));
    memcpy(GlobalSets.CpuUBO.TESR_ViewProjectionTransform, TheRenderManager->ViewProjMatrix, sizeof(GlobalSets.CpuUBO.TESR_ViewProjectionTransform));
    memcpy(GlobalSets.CpuUBO.TESR_OcclusionWorldViewProjTransform, TheShaderManager->ShaderConst.OcclusionMap.OcclusionWorldViewProj, sizeof(GlobalSets.CpuUBO.TESR_OcclusionWorldViewProjTransform));
    memcpy(GlobalSets.CpuUBO.TESR_LightPosition, TheShaderManager->LightPosition, sizeof(GlobalSets.CpuUBO.TESR_LightPosition));
    memcpy(GlobalSets.CpuUBO.TESR_LightColor, TheShaderManager->LightColor, sizeof(GlobalSets.CpuUBO.TESR_LightColor));
    memcpy(GlobalSets.CpuUBO.TESR_SpotLightPosition, TheShaderManager->SpotLightPosition, sizeof(GlobalSets.CpuUBO.TESR_SpotLightPosition));
    memcpy(GlobalSets.CpuUBO.TESR_SpotLightColor, TheShaderManager->SpotLightColor, sizeof(GlobalSets.CpuUBO.TESR_SpotLightColor));
    memcpy(GlobalSets.CpuUBO.TESR_SpotLightDirection, TheShaderManager->SpotLightDirection, sizeof(GlobalSets.CpuUBO.TESR_SpotLightDirection));
    memcpy(GlobalSets.CpuUBO.TESR_SpotLightToWorldTransform, TheShaderManager->SpotLightWorldToLightMatrix[0], sizeof(GlobalSets.CpuUBO.TESR_SpotLightToWorldTransform));
    memcpy(GlobalSets.CpuUBO.TESR_ViewSpaceLightDir, TheShaderManager->ShaderConst.ViewSpaceLightDir, sizeof(GlobalSets.CpuUBO.TESR_ViewSpaceLightDir));
    memcpy(GlobalSets.CpuUBO.TESR_ScreenSpaceLightDir, TheShaderManager->ShaderConst.ScreenSpaceLightDir, sizeof(GlobalSets.CpuUBO.TESR_ScreenSpaceLightDir));
    memcpy(GlobalSets.CpuUBO.TESR_ReciprocalResolution, TheShaderManager->ShaderConst.ReciprocalResolution, sizeof(GlobalSets.CpuUBO.TESR_ReciprocalResolution));
    memcpy(GlobalSets.CpuUBO.TESR_CameraForward, TheRenderManager->CameraForward, sizeof(GlobalSets.CpuUBO.TESR_CameraForward));
    memcpy(GlobalSets.CpuUBO.TESR_DepthConstants, TheRenderManager->DepthConstants, sizeof(GlobalSets.CpuUBO.TESR_DepthConstants));
    memcpy(GlobalSets.CpuUBO.TESR_CameraData, TheRenderManager->CameraData, sizeof(GlobalSets.CpuUBO.TESR_CameraData));
    memcpy(GlobalSets.CpuUBO.TESR_CameraPosition, TheRenderManager->CameraPosition, sizeof(GlobalSets.CpuUBO.TESR_CameraPosition));
    memcpy(GlobalSets.CpuUBO.TESR_SunDirection, TheShaderManager->ShaderConst.SunDir, sizeof(GlobalSets.CpuUBO.TESR_SunDirection));
    memcpy(GlobalSets.CpuUBO.TESR_SunPosition, TheShaderManager->ShaderConst.SunPosition, sizeof(GlobalSets.CpuUBO.TESR_SunPosition));
    memcpy(GlobalSets.CpuUBO.TESR_SunTiming, TheShaderManager->ShaderConst.SunTiming, sizeof(GlobalSets.CpuUBO.TESR_SunTiming));
    memcpy(GlobalSets.CpuUBO.TESR_SunAmount, TheShaderManager->ShaderConst.SunAmount, sizeof(GlobalSets.CpuUBO.TESR_SunAmount));
    memcpy(GlobalSets.CpuUBO.TESR_GameTime, TheShaderManager->ShaderConst.GameTime, sizeof(GlobalSets.CpuUBO.TESR_GameTime));
    memcpy(GlobalSets.CpuUBO.TESR_FogData, TheShaderManager->ShaderConst.fogData, sizeof(GlobalSets.CpuUBO.TESR_FogData));
    memcpy(GlobalSets.CpuUBO.TESR_FogDistance, TheShaderManager->ShaderConst.fogDistance, sizeof(GlobalSets.CpuUBO.TESR_FogDistance));
    memcpy(GlobalSets.CpuUBO.TESR_FogColor, TheShaderManager->ShaderConst.fogColor, sizeof(GlobalSets.CpuUBO.TESR_FogColor));
    memcpy(GlobalSets.CpuUBO.TESR_SunColor, TheShaderManager->ShaderConst.sunColor, sizeof(GlobalSets.CpuUBO.TESR_SunColor));
    memcpy(GlobalSets.CpuUBO.TESR_SunDiskColor, TheShaderManager->ShaderConst.sunDiskColor, sizeof(GlobalSets.CpuUBO.TESR_SunDiskColor));
    memcpy(GlobalSets.CpuUBO.TESR_SunAmbient, TheShaderManager->ShaderConst.sunAmbient, sizeof(GlobalSets.CpuUBO.TESR_SunAmbient));
    memcpy(GlobalSets.CpuUBO.TESR_SkyColor, TheShaderManager->ShaderConst.skyColor, sizeof(GlobalSets.CpuUBO.TESR_SkyColor));
    memcpy(GlobalSets.CpuUBO.TESR_SkyLowColor, TheShaderManager->ShaderConst.skyLowColor, sizeof(GlobalSets.CpuUBO.TESR_SkyLowColor));
    memcpy(GlobalSets.CpuUBO.TESR_HorizonColor, TheShaderManager->ShaderConst.horizonColor, sizeof(GlobalSets.CpuUBO.TESR_HorizonColor));
}

void FVulkanGlobalResources::CreateFrameSet()
{
    Logger::Log("FVulkanGlobalResources::CreateFrameSet: Starting");
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;
    VkDevice Device = Vulkan.Device;

    if (!Device) {
        Logger::Log("CreateFrameSet: no Vulkan device");
        return;
    }

    // 1) Descriptor set layout: binding 0 = depth sampler, 1 = normals sampler, 2 = UBO
    VkDescriptorSetLayoutBinding bindings[4] = {};

    // binding 0: depth sampler
    bindings[0].binding            = 0;
    bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount    = 1;
    bindings[0].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // binding 1: normals sampler
    bindings[1].binding            = 1;
    bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount    = 1;
    bindings[1].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // binding 2: frame UBO
    bindings[2].binding            = 2;
    bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount    = 1;
    bindings[2].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // binding 3: scene color sampler (pre-tonemap scene color)
    bindings[3].binding            = 3;
    bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount    = 1;
    bindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings    = bindings;

    VkResult vr = p_vkCreateDescriptorSetLayout(Device, &layoutInfo, nullptr, &GlobalSets.GlobalFrameSetLayout);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateDescriptorSetLayout failed (%d)", vr);
        GlobalSets.GlobalFrameSetLayout = VK_NULL_HANDLE;
        return;
    }

    // 2) UBO buffer
    VkDeviceSize uboSize = sizeof(FGlobalFrameUBO);

    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size = uboSize;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vr = p_vkCreateBuffer(Device, &bufInfo, nullptr, &GlobalSets.FrameUBO);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateBuffer failed (%d)", vr);
        GlobalSets.FrameUBO = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements memReq{};
    p_vkGetBufferMemoryRequirements(Device, GlobalSets.FrameUBO, &memReq);

    VkPhysicalDeviceMemoryProperties memProps{};
    p_vkGetPhysicalDeviceMemoryProperties(Vulkan.PhysicalDevice, &memProps);

    uint32_t memType = FindMemoryType(
        memProps,
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memType == ~0u) {
        Logger::Log("CreateFrameSet: no suitable memory type for UBO");
        return;
    }

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memType;

    vr = p_vkAllocateMemory(Device, &allocInfo, nullptr, &GlobalSets.FrameUBOMemory);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkAllocateMemory failed (%d)", vr);
        GlobalSets.FrameUBOMemory = VK_NULL_HANDLE;
        return;
    }

    p_vkBindBufferMemory(Device, GlobalSets.FrameUBO, GlobalSets.FrameUBOMemory, 0);

    // 3) Descriptor pool + set
    VkDescriptorPoolSize poolSizes[2] = {};
    // depth + normals + scene color
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 3;
    // UBO
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;

    vr = p_vkCreateDescriptorPool(Device, &poolInfo, nullptr, &GlobalSets.GlobalFramePool);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkCreateDescriptorPool failed (%d)", vr);
        GlobalSets.GlobalFramePool = VK_NULL_HANDLE;
        return;
    }

    VkDescriptorSetAllocateInfo setAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    setAlloc.descriptorPool = GlobalSets.GlobalFramePool;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts = &GlobalSets.GlobalFrameSetLayout;

    vr = p_vkAllocateDescriptorSets(Device, &setAlloc, &GlobalSets.GlobalFrameDescriptorSet);
    if (vr != VK_SUCCESS) {
        Logger::Log("CreateFrameSet: vkAllocateDescriptorSets failed (%d)", vr);
        GlobalSets.GlobalFrameDescriptorSet = VK_NULL_HANDLE;
        return;
    }

    Logger::Log("CreateFrameSet: OK (layout+UBO+set created)");
}


void FVulkanGlobalResources::DestroyFrameSet()
{
    VkDevice Device = TheVulkanEffectsManager->VulkanContext.Device;
    if (!Device)
        return;

    if (GlobalSets.FrameUBOMemory) {
        p_vkFreeMemory(Device, GlobalSets.FrameUBOMemory, nullptr);
        GlobalSets.FrameUBOMemory = VK_NULL_HANDLE;
    }

    if (GlobalSets.FrameUBO) {
        p_vkDestroyBuffer(Device, GlobalSets.FrameUBO, nullptr);
        GlobalSets.FrameUBO = VK_NULL_HANDLE;
    }

    if (GlobalSets.GlobalFramePool) {
        p_vkDestroyDescriptorPool(Device, GlobalSets.GlobalFramePool, nullptr);
        GlobalSets.GlobalFramePool = VK_NULL_HANDLE;
    }

    if (GlobalSets.GlobalFrameSetLayout) {
        p_vkDestroyDescriptorSetLayout(Device, GlobalSets.GlobalFrameSetLayout, nullptr);
        GlobalSets.GlobalFrameSetLayout = VK_NULL_HANDLE;
    }

    GlobalSets.GlobalFrameDescriptorSet = VK_NULL_HANDLE;
}
