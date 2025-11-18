#include "VkDispatch.h"
#include <windows.h>

// Here are the **definitions**
PFN_vkQueueSubmit          p_vkQueueSubmit = nullptr;
PFN_vkCreateFence          p_vkCreateFence = nullptr;
PFN_vkDestroyFence         p_vkDestroyFence = nullptr;
PFN_vkWaitForFences        p_vkWaitForFences = nullptr;
PFN_vkCreateImageView      p_vkCreateImageView = nullptr;
PFN_vkDestroyImageView     p_vkDestroyImageView = nullptr;
PFN_vkAllocateDescriptorSets p_vkAllocateDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets   p_vkUpdateDescriptorSets = nullptr;
PFN_vkAllocateCommandBuffers p_vkAllocateCommandBuffers = nullptr;
PFN_vkFreeCommandBuffers     p_vkFreeCommandBuffers = nullptr;
PFN_vkBeginCommandBuffer     p_vkBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer       p_vkEndCommandBuffer = nullptr;
PFN_vkCmdBindPipeline        p_vkCmdBindPipeline = nullptr;
PFN_vkCmdBindDescriptorSets  p_vkCmdBindDescriptorSets = nullptr;
PFN_vkCmdDispatch            p_vkCmdDispatch = nullptr;
PFN_vkCreateDescriptorPool p_vkCreateDescriptorPool = nullptr;
PFN_vkCreateShaderModule p_vkCreateShaderModule = nullptr;
PFN_vkCreateDescriptorSetLayout p_vkCreateDescriptorSetLayout = nullptr;
PFN_vkCreatePipelineLayout p_vkCreatePipelineLayout = nullptr;
PFN_vkCreateComputePipelines p_vkCreateComputePipelines = nullptr;
PFN_vkCreateCommandPool p_vkCreateCommandPool = nullptr;
PFN_vkCmdPushConstants p_vkCmdPushConstants = nullptr;
PFN_vkFreeDescriptorSets p_vkFreeDescriptorSets = nullptr;
PFN_vkCmdClearColorImage p_vkCmdClearColorImage = nullptr;

void VkDispatch::InitVulkanFunctionPointers(VkInstance instance, VkDevice device) {
    HMODULE mod = LoadLibraryA("vulkan-1.dll");
    if (!mod)
    {
        Logger::Log("Could not load vulkan-1.dll library");
        return;
    }

    auto LoadDev = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
            GetProcAddress(mod, name));
    };

    LoadDev(p_vkQueueSubmit, "vkQueueSubmit");
    if (!p_vkQueueSubmit)
    {
        Logger::Log("p_vkQueueSubmit is null");
    }
    LoadDev(p_vkCreateFence, "vkCreateFence");
    if (!p_vkCreateFence)
    {
        Logger::Log("p_vkCreateFence is null");
    }
    LoadDev(p_vkDestroyFence, "vkDestroyFence");
    if (!p_vkDestroyFence)
    {
        Logger::Log("p_vkDestroyFence is null");
    }
    LoadDev(p_vkWaitForFences, "vkWaitForFences");
    if (!p_vkWaitForFences)
    {
        Logger::Log("p_vkWaitForFences is null");
    }
    LoadDev(p_vkCreateImageView, "vkCreateImageView");
    if (!p_vkCreateImageView)
    {
        Logger::Log("p_vkCreateImageView is null");
    }
    LoadDev(p_vkDestroyImageView, "vkDestroyImageView");
    if (!p_vkDestroyImageView)
    {
        Logger::Log("p_vkDestroyImageView is null");
    }
    LoadDev(p_vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    if (!p_vkAllocateDescriptorSets)
    {
        Logger::Log("p_vkAllocateDescriptorSets is null");
    }
    LoadDev(p_vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    if (!p_vkUpdateDescriptorSets)
    {
        Logger::Log("p_vkUpdateDescriptorSets is null");
    }
    LoadDev(p_vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    if (!p_vkAllocateCommandBuffers)
    {
        Logger::Log("p_vkAllocateCommandBuffers is null");
    }
    LoadDev(p_vkBeginCommandBuffer, "vkBeginCommandBuffer");
    if (!p_vkBeginCommandBuffer)
    {
        Logger::Log("p_vkBeginCommandBuffer is null");
    }
    LoadDev(p_vkEndCommandBuffer, "vkEndCommandBuffer");
    if (!p_vkEndCommandBuffer)
    {
        Logger::Log("p_vkEndCommandBuffer is null");
    }
    LoadDev(p_vkCmdBindPipeline, "vkCmdBindPipeline");
    if (!p_vkCmdBindPipeline)
    {
        Logger::Log("p_vkCmdBindPipeline is null");
    }
    LoadDev(p_vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    if (!p_vkCmdBindDescriptorSets)
    {
        Logger::Log("p_vkCmdBindDescriptorSets is null");
    }
    LoadDev(p_vkCmdDispatch, "vkCmdDispatch");
    if (!p_vkCmdDispatch)
    {
        Logger::Log("p_vkCmdDispatch is null");
    }
    LoadDev(p_vkFreeCommandBuffers, "vkFreeCommandBuffers");
    if (!p_vkFreeCommandBuffers)
    {
        Logger::Log("p_vkFreeCommandBuffers is null");
    }
    LoadDev(p_vkCreateDescriptorPool, "vkCreateDescriptorPool");
    if (!p_vkCreateDescriptorPool)
    {
        Logger::Log("p_vkCreateDescriptorPool is null");
    }
    LoadDev(p_vkCreateShaderModule, "vkCreateShaderModule");
    if (!p_vkCreateShaderModule)
    {
        Logger::Log("p_vkCreateShaderModule is null");
    }
    LoadDev(p_vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    if (!p_vkCreateDescriptorSetLayout)
    {
        Logger::Log("p_vkCreateDescriptorSetLayout is null");
    }
    LoadDev(p_vkCreatePipelineLayout, "vkCreatePipelineLayout");
    if (!p_vkCreatePipelineLayout)
    {
        Logger::Log("p_vkCreatePipelineLayout is null");
    }
    LoadDev(p_vkCreateComputePipelines, "vkCreateComputePipelines");
    if (!p_vkCreateComputePipelines)
    {
        Logger::Log("p_vkCreateComputePipelines is null");
    }
    LoadDev(p_vkCreateCommandPool, "vkCreateCommandPool");
    if (!p_vkCreateCommandPool)
    {
        Logger::Log("p_vkCreateCommandPool is null");
    }
    LoadDev(p_vkCmdPushConstants, "vkCmdPushConstants");
    if (!p_vkCmdPushConstants)
    {
        Logger::Log("p_vkCmdPushConstants is null");
    }
    LoadDev(p_vkFreeDescriptorSets, "vkFreeDescriptorSets");
    if (!p_vkFreeDescriptorSets)
    {
        Logger::Log("p_vkFreeDescriptorSets is null");
    }
    LoadDev(p_vkCmdClearColorImage, "vkCmdClearColorImage");

    Logger::Log("Vulkan function pointers initialized");
}
