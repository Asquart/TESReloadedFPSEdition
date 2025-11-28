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

#define DEBUG \
TheSettingManager->SettingsMain.Develop.DebugMode

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
    VkCommandBuffer     CommandBuffer = VK_NULL_HANDLE;
    VkFence             SynchronizationFence = VK_NULL_HANDLE;
    VkQueryPool      QueryPool = VK_NULL_HANDLE;
    float            TimestampPeriod = 0.0f;

    bool                bComputeRecording = false;  // are we inside a begun command buffer?
    bool                bComputeBroken = false;  // permanent failure, stop using compute

    bool             bInitialized = false;

    void Initialize();
    void Shutdown();

    uint32_t FindMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags Properties) const;

private:
    void InitSamplers();
    void InitPools();
    void InitTiming();
    void ShutdownTiming();

    void BeginComputeWithTimestamp();
    void EndComputeAndSubmit();
    friend struct FScopedComputePass;
};

struct FScopedComputePass
{
    FVulkanContext* Ctx = nullptr;
    bool              Active = false;

    FScopedComputePass(FVulkanContext* InCtx)
        : Ctx(InCtx)
    {
        if (Ctx) {
            Ctx->BeginComputeWithTimestamp();
            Active = true;
        }
    }

    ~FScopedComputePass()
    {
        if (Active && Ctx) {
            Ctx->EndComputeAndSubmit();
        }
    }

    // non-copyable
    FScopedComputePass(const FScopedComputePass&) = delete;
    FScopedComputePass& operator=(const FScopedComputePass&) = delete;
};