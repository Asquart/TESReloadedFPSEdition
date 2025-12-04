#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(expr, msg)                         \
    do {                                            \
        VkResult _vr = (expr);                      \
        if (_vr != VK_SUCCESS) {                    \
            Logger::Log("%s failed: %d", msg, (int)_vr); \
        }                                           \
    } while (0)

// Device-level functions you actually call
extern PFN_vkQueueSubmit          p_vkQueueSubmit;
extern PFN_vkCreateShaderModule   p_vkCreateShaderModule;
extern PFN_vkDestroyShaderModule  p_vkDestroyShaderModule;
extern PFN_vkCreateImageView      p_vkCreateImageView;
extern PFN_vkDestroyImageView     p_vkDestroyImageView;
extern PFN_vkCreateFence          p_vkCreateFence;
extern PFN_vkDestroyFence         p_vkDestroyFence;
extern PFN_vkAllocateMemory p_vkAllocateMemory;
extern PFN_vkCreateBuffer p_vkCreateBuffer;
extern PFN_vkBindBufferMemory p_vkBindBufferMemory;
extern PFN_vkDestroyBuffer        p_vkDestroyBuffer;
extern PFN_vkGetBufferMemoryRequirements p_vkGetBufferMemoryRequirements;
extern PFN_vkFreeMemory p_vkFreeMemory;
extern PFN_vkCreateDescriptorSetLayout         p_vkCreateDescriptorSetLayout;
extern PFN_vkCreatePipelineLayout  p_vkCreatePipelineLayout;
extern PFN_vkCreateComputePipelines p_vkCreateComputePipelines;
extern PFN_vkCreateDescriptorPool p_vkCreateDescriptorPool;
extern PFN_vkCreateCommandPool p_vkCreateCommandPool;
extern PFN_vkCmdPushConstants p_vkCmdPushConstants;
extern PFN_vkFreeDescriptorSets p_vkFreeDescriptorSets;
extern PFN_vkMapMemory p_vkMapMemory;
extern PFN_vkUnmapMemory p_vkUnmapMemory;

extern PFN_vkCmdClearColorImage p_vkCmdClearColorImage;

extern PFN_vkFreeCommandBuffers p_vkFreeCommandBuffers;
extern PFN_vkEndCommandBuffer p_vkEndCommandBuffer;
extern PFN_vkCmdDispatch p_vkCmdDispatch;
extern PFN_vkCmdBindDescriptorSets p_vkCmdBindDescriptorSets;
extern PFN_vkCmdBindPipeline p_vkCmdBindPipeline;
extern PFN_vkBeginCommandBuffer p_vkBeginCommandBuffer;
extern PFN_vkAllocateCommandBuffers p_vkAllocateCommandBuffers;
extern PFN_vkUpdateDescriptorSets p_vkUpdateDescriptorSets;
extern PFN_vkAllocateDescriptorSets p_vkAllocateDescriptorSets;
extern PFN_vkCreateQueryPool p_vkCreateQueryPool;
extern PFN_vkCreateSampler p_vkCreateSampler;

extern PFN_vkCmdResetQueryPool p_vkCmdResetQueryPool;
extern PFN_vkCmdWriteTimestamp p_vkCmdWriteTimestamp;
extern PFN_vkWaitForFences p_vkWaitForFences;
extern PFN_vkGetQueryPoolResults p_vkGetQueryPoolResults;
extern PFN_vkGetPhysicalDeviceProperties p_vkGetPhysicalDeviceProperties;

extern PFN_vkDestroyPipeline p_vkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout p_vkDestroyPipelineLayout;
extern PFN_vkDestroyDescriptorSetLayout p_vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDescriptorPool p_vkDestroyDescriptorPool;
extern PFN_vkDestroyQueryPool p_vkDestroyQueryPool;
extern PFN_vkResetCommandBuffer p_vkResetCommandBuffer;
extern PFN_vkResetFences p_vkResetFences;
extern PFN_vkQueueWaitIdle p_vkQueueWaitIdle;

extern PFN_vkGetPhysicalDeviceMemoryProperties p_vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkCmdPipelineBarrier p_vkCmdPipelineBarrier;

extern PFN_vkCmdBeginRenderPass p_vkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass p_vkCmdEndRenderPass;

class VulkanFunctionsHooks
{
public:
	static void InitVulkanFunctionPointers(VkInstance instance, VkDevice device);
};

