#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <com_pointer.h>
#include <DxvkInterop.h>

#include "VulkanFunctionsHooks.h"

#define DXVK_CheckReturn()      \
if (!TheRenderManager->DXVK)    \
{                               \
    return;                     \
}                               \

#define DEBUG TheSettingManager->SettingsMain.Develop.DebugMode

#define VULKAN_CONTEXT TheVulkanEffectsManager->VulkanContext

#define BEGIN_DEBUG_GPU_TIME(InVulkanEffect) \
    if (DEBUG) { (InVulkanEffect)->BeginGpuTimer(); }

#define END_DEBUG_GPU_TIME(InVulkanEffect) \
    if (DEBUG) { (InVulkanEffect)->EndGpuTimer(); }

#define TRY_APPLY_FENCE(InVulkanEffect)\
    if (InVulkanEffect->EffectFence != VK_NULL_HANDLE) \
    { \
        p_vkResetFences(VULKAN_CONTEXT.Device, 1, &InVulkanEffect->EffectFence); \
        InVulkanEffect->bFenceInUse = true; \
    } \

#define TRY_DEBUG_END_FENCE(InVulkanEffect)\
    if (DEBUG) \
    { \
        if (InVulkanEffect->EffectFence != VK_NULL_HANDLE && InVulkanEffect->bFenceInUse) \
        { \
            p_vkWaitForFences(VULKAN_CONTEXT.Device, 1, &InVulkanEffect->EffectFence, VK_TRUE, UINT64_MAX); \
            InVulkanEffect->bFenceInUse = false; \
            InVulkanEffect->ResolveGpuTime(); \
        } \
    } \

#define ENSURE_END_FENCE(InVulkanEffect)\
    if ((InVulkanEffect->EffectFence) != VK_NULL_HANDLE && (InVulkanEffect->bFenceInUse)) { \
            p_vkWaitForFences(VULKAN_CONTEXT.Device, 1, &(InVulkanEffect->EffectFence), VK_TRUE, UINT64_MAX); \
            (InVulkanEffect->bFenceInUse) = false;                 \
            InVulkanEffect->ResolveGpuTime(); \
        }

struct FVulkanContext
{
    // DXVK interop
    dxvk::Com<ID3D9VkInteropDevice> InteropDevice;

    // Vulkan handles
    VkInstance       Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice         Device = VK_NULL_HANDLE;
    VkQueue          Queue = VK_NULL_HANDLE;
    uint32_t         QueueFamilyIndex = 0;
    uint32_t         QueueIndex = 0;

    // Global pools / helpers
    VkCommandPool    CmdPool = VK_NULL_HANDLE;
    VkDescriptorPool GlobalDescPool = VK_NULL_HANDLE;
    
    // Global samplers
    VkSampler SamplerLinearClamp = VK_NULL_HANDLE;
    VkSampler SamplerPointClamp = VK_NULL_HANDLE;
    VkSampler SamplerLinearRepeat = VK_NULL_HANDLE;

    // GPU timing
    VkQueryPool      QueryPool = VK_NULL_HANDLE;
    float            TimestampPeriod = 0.0f;

    bool                bComputeRecording = false;  // are we inside a begun command buffer?
    bool                bComputeBroken = false;  // permanent failure, stop using compute

    bool             bInitialized = false;

    void Initialize();
    void Shutdown();

    uint32_t FindMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags Properties) const;

    void TryStartFence(VkCommandBuffer InCommandBuffer);
    void TryEndFence(VkCommandBuffer InCommandBuffer);

private:
    void InitSamplers();
    void InitPools();
};