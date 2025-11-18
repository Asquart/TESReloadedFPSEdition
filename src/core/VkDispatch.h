#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_CHECK(expr, msg)                         \
    do {                                            \
        VkResult _vr = (expr);                      \
        if (_vr != VK_SUCCESS) {                    \
            Logger::Log("%s failed: %d", msg, (int)_vr); \
            return;                                 \
        }                                           \
    } while (0)

// Device-level functions you actually call
extern PFN_vkQueueSubmit          p_vkQueueSubmit;
extern PFN_vkCreateShaderModule   p_vkCreateShaderModule;
extern PFN_vkDestroyShaderModule  p_vkDestroyShaderModule;
extern PFN_vkCreateImageView      p_vkCreateImageView;
extern PFN_vkDestroyImageView     p_vkDestroyImageView;
extern PFN_vkCreateFence          p_vkCreateFence;
extern PFN_vkWaitForFences        p_vkWaitForFences;
extern PFN_vkDestroyFence         p_vkDestroyFence;
extern PFN_vkCreateDescriptorSetLayout         p_vkCreateDescriptorSetLayout;
extern PFN_vkCreatePipelineLayout  p_vkCreatePipelineLayout;
extern PFN_vkCreateComputePipelines p_vkCreateComputePipelines;
extern PFN_vkCreateDescriptorPool p_vkCreateDescriptorPool;
extern PFN_vkCreateCommandPool p_vkCreateCommandPool;
extern PFN_vkCmdPushConstants p_vkCmdPushConstants;
extern PFN_vkFreeDescriptorSets p_vkFreeDescriptorSets;

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

class VkDispatch
{
public:
	static void InitVulkanFunctionPointers(VkInstance instance, VkDevice device);
};

