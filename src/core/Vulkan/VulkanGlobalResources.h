#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct alignas(16) FGlobalFrameUBO
{
    // Matrices
    float Projection[16];
    float InvProjection[16];
    float View[16];
    float InvView[16];

    // Camera / Depth info
    float DepthConstants[4];
    float CameraData[4];
    float CameraPosition[4];

    // Expand later:
    // float ScreenSize[4];
    // float Jitter[4];
    // float FrameTime[4];
};

struct FVulkanGlobalSets
{
    // FRAME SET = set=0
    VkDescriptorSetLayout GlobalFrameSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       GlobalFrameSet = VK_NULL_HANDLE;
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

    FVulkanGlobalSets& GetSets() { return GlobalSets; }
    FGlobalFrameUBO& GetUBO() { return GlobalSets.CpuUBO; }

private:

    FVulkanGlobalSets GlobalSets;

    void CreateFrameSet();
    void DestroyFrameSet();
};