#include "VulkanFunctionsHooks.h"
#include <windows.h>

// Here are the **definitions**
PFN_vkQueueSubmit          p_vkQueueSubmit = nullptr;
PFN_vkCreateFence          p_vkCreateFence = nullptr;
PFN_vkDestroyFence         p_vkDestroyFence = nullptr;
PFN_vkWaitForFences        p_vkWaitForFences = nullptr;
PFN_vkCreateImageView      p_vkCreateImageView = nullptr;
PFN_vkDestroyImageView     p_vkDestroyImageView = nullptr;
PFN_vkGetBufferMemoryRequirements p_vkGetBufferMemoryRequirements = nullptr;
PFN_vkDestroyBuffer p_vkDestroyBuffer = nullptr;
PFN_vkAllocateMemory p_vkAllocateMemory = nullptr;
PFN_vkMapMemory p_vkMapMemory = nullptr;
PFN_vkUnmapMemory p_vkUnmapMemory = nullptr;
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
PFN_vkCreateSampler p_vkCreateSampler = nullptr;
PFN_vkCreateQueryPool p_vkCreateQueryPool = nullptr;
PFN_vkCmdResetQueryPool p_vkCmdResetQueryPool = nullptr;
PFN_vkCmdWriteTimestamp p_vkCmdWriteTimestamp = nullptr;
PFN_vkGetQueryPoolResults p_vkGetQueryPoolResults = nullptr;
PFN_vkGetPhysicalDeviceProperties p_vkGetPhysicalDeviceProperties = nullptr;
PFN_vkDestroyPipeline p_vkDestroyPipeline = nullptr;
PFN_vkDestroyPipelineLayout p_vkDestroyPipelineLayout = nullptr;
PFN_vkDestroyDescriptorSetLayout p_vkDestroyDescriptorSetLayout = nullptr;
PFN_vkDestroyShaderModule p_vkDestroyShaderModule = nullptr;
PFN_vkDestroyDescriptorPool p_vkDestroyDescriptorPool = nullptr;
PFN_vkDestroyQueryPool p_vkDestroyQueryPool = nullptr;
PFN_vkResetCommandBuffer p_vkResetCommandBuffer = nullptr;
PFN_vkResetFences p_vkResetFences = nullptr;
PFN_vkFreeMemory p_vkFreeMemory = nullptr;
PFN_vkCreateBuffer p_vkCreateBuffer = nullptr;
PFN_vkBindBufferMemory p_vkBindBufferMemory = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties p_vkGetPhysicalDeviceMemoryProperties = nullptr;
PFN_vkCmdPipelineBarrier p_vkCmdPipelineBarrier = nullptr;
PFN_vkQueueWaitIdle p_vkQueueWaitIdle = nullptr;
PFN_vkResetQueryPool p_vkResetQueryPool = nullptr;

void VulkanFunctionsHooks::InitVulkanFunctionPointers(VkInstance instance, VkDevice device) {
    HMODULE mod = LoadLibraryA("vulkan-1.dll");
    if (!mod)
    {
        Logger::Log("Could not load vulkan-1.dll library");
        return;
    }

    auto LoadDev = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
            GetProcAddress(mod, name));
        if (!fn)
        {
            Logger::Log("p_%s is null", name);
        }
    };

    LoadDev(p_vkQueueSubmit, "vkQueueSubmit");
    LoadDev(p_vkCreateFence, "vkCreateFence");
    LoadDev(p_vkDestroyFence, "vkDestroyFence");
    LoadDev(p_vkCreateImageView, "vkCreateImageView");
    LoadDev(p_vkDestroyImageView, "vkDestroyImageView");
    LoadDev(p_vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    LoadDev(p_vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    LoadDev(p_vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    LoadDev(p_vkBeginCommandBuffer, "vkBeginCommandBuffer");
    LoadDev(p_vkEndCommandBuffer, "vkEndCommandBuffer");
    LoadDev(p_vkCmdBindPipeline, "vkCmdBindPipeline");
    LoadDev(p_vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    LoadDev(p_vkCmdDispatch, "vkCmdDispatch");
    LoadDev(p_vkFreeCommandBuffers, "vkFreeCommandBuffers");
    LoadDev(p_vkCreateDescriptorPool, "vkCreateDescriptorPool");
    LoadDev(p_vkCreateShaderModule, "vkCreateShaderModule");
    LoadDev(p_vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    LoadDev(p_vkCreatePipelineLayout, "vkCreatePipelineLayout");
    LoadDev(p_vkCreateComputePipelines, "vkCreateComputePipelines");
    LoadDev(p_vkCreateCommandPool, "vkCreateCommandPool");
    LoadDev(p_vkCmdPushConstants, "vkCmdPushConstants");
    LoadDev(p_vkFreeDescriptorSets, "vkFreeDescriptorSets");
    LoadDev(p_vkCmdClearColorImage, "vkCmdClearColorImage");
    LoadDev(p_vkCreateSampler, "vkCreateSampler");
    LoadDev(p_vkCreateQueryPool, "vkCreateQueryPool");
    LoadDev(p_vkCmdResetQueryPool, "vkCmdResetQueryPool");
    LoadDev(p_vkCmdWriteTimestamp, "vkCmdWriteTimestamp");
    LoadDev(p_vkWaitForFences, "vkWaitForFences");
    LoadDev(p_vkGetQueryPoolResults, "vkGetQueryPoolResults");
    LoadDev(p_vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
    LoadDev(p_vkDestroyPipeline, "vkDestroyPipeline");
    LoadDev(p_vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
    LoadDev(p_vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    LoadDev(p_vkDestroyShaderModule, "vkDestroyShaderModule");
    LoadDev(p_vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
    LoadDev(p_vkDestroyQueryPool, "vkDestroyQueryPool");
    LoadDev(p_vkResetCommandBuffer, "vkResetCommandBuffer");
    LoadDev(p_vkResetFences, "vkResetFences");
    LoadDev(p_vkDestroyBuffer, "vkDestroyBuffer");
    LoadDev(p_vkFreeMemory, "vkFreeMemory");
    LoadDev(p_vkCreateBuffer, "vkCreateBuffer");
    LoadDev(p_vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
    LoadDev(p_vkAllocateMemory, "vkAllocateMemory");
    LoadDev(p_vkBindBufferMemory, "vkBindBufferMemory");
    LoadDev(p_vkMapMemory, "vkMapMemory");
    LoadDev(p_vkUnmapMemory, "vkUnmapMemory");
    LoadDev(p_vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
    LoadDev(p_vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
    LoadDev(p_vkResetQueryPool, "vkResetQueryPool");
    LoadDev(p_vkQueueWaitIdle, "vkQueueWaitIdle");

    Logger::Log("Vulkan function pointers initialized");
}
