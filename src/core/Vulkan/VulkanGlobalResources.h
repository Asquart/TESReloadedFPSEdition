#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct alignas(16) FGlobalFrameUBO
{
    // Matrices
	float TESR_WorldTransform[16];
	float TESR_ViewTransform[16];
	float TESR_InvViewTransform[16];
	float TESR_ProjectionTransform[16];
	float TESR_InvProjectionTransform[16];
	float TESR_WorldViewProjectionTransform[16];
	float TESR_InvViewProjectionTransform[16];
	float TESR_ViewProjectionTransform[16];
	float TESR_OcclusionWorldViewProjTransform[16];

	// Light parameters
	float TESR_LightPosition[4];
	float TESR_LightColor[4];
	float TESR_SpotLightPosition[4];
	float TESR_SpotLightColor[4];
	float TESR_SpotLightDirection[4];
	float TESR_SpotLightToWorldTransform[16];
	float TESR_ViewSpaceLightDir[4];
	float TESR_ScreenSpaceLightDir[4];

	// Depth parameters
	float TESR_DepthConstants[4];
	
	// Camera parameters
	float TESR_ReciprocalResolution[4];
	float TESR_CameraForward[4];
	float TESR_CameraData[4];
	float TESR_CameraPosition[4];

	// Game time
	float TESR_GameTime[16];
	
	// Atmospheric parameters
	float TESR_SunDirection[4];
	float TESR_SunPosition[4];
	float TESR_SunTiming[4];
	float TESR_SunAmount[4];
	float TESR_FogData[4];
	float TESR_FogDistance[4];
	float TESR_FogColor[4];
	float TESR_SunColor[4];
	float TESR_SunDiskColor[4];
	float TESR_SunAmbient[4];
	float TESR_SkyColor[4];
	float TESR_SkyLowColor[4];
	float TESR_HorizonColor[4];
};

struct FVulkanGlobalSets
{
    // FRAME SET = set=0
    VkDescriptorSetLayout GlobalFrameSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       GlobalFrameDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool      GlobalFramePool = VK_NULL_HANDLE;

    VkBuffer              FrameUBO = VK_NULL_HANDLE;
    VkDeviceMemory        FrameUBOMemory = VK_NULL_HANDLE;

    FGlobalFrameUBO       CpuUBO{};   // cached CPU copy

    // Future expansions:
    // VkDescriptorSetLayout LightingSetLayout;
    // VkDescriptorSetLayout ShadowSetLayout;
    // VkDescriptorSetLayout MaterialSetLayout;
};

class FVulkanGlobalResources
{
public:
    void Initialize();
    void Shutdown();
    void UpdatePerFrame();

    FVulkanGlobalSets& GetGlobalDescriptorSets() { return GlobalSets; }
    FGlobalFrameUBO& GetUBO() { return GlobalSets.CpuUBO; }

private:

    FVulkanGlobalSets GlobalSets;
    bool bInitialized = false;
    void UpdateCpuUboParams();
    void CreateFrameSet();
    void DestroyFrameSet();
};