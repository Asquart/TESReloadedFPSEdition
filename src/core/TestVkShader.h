#pragma once
#include "VkDispatch.h"
#include <com_pointer.h>
#include <DxvkInterop.h>>

class TestVkShader
{
    struct ShaderComputeContext {
        dxvk::Com<ID3D9VkInteropDevice> VulkanDevice = nullptr;
        VkInstance Instance = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        VkDevice              Device = VK_NULL_HANDLE;
        VkQueue Queue = VK_NULL_HANDLE;
        uint32_t FamilyIndex = -1;
        uint32_t QueueIndex = -1;
        VkPipelineLayout      pipelineLayout = VK_NULL_HANDLE;
        VkPipeline            pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      descPool = VK_NULL_HANDLE;
        VkCommandPool         cmdPool = VK_NULL_HANDLE;
        VkShaderModule        shader = VK_NULL_HANDLE;
    };

    struct PushData {
        float color[4];
    };

public:

    static void	Initialize();


    void InitCompute(IDirect3DDevice9* InD3D9Device);
    VkImageView GetOrCreateViewForImage(VkImage image,
        VkFormat format,
        uint32_t mipLevels,
        uint32_t arrayLayers);
    void RunCompute(IDirect3DSurface9* InD3D9Surface);
    void ClearSurfaceVulkan(IDirect3DSurface9* surface);
    static std::vector<uint32_t> LoadSpirv(const char* path);

    ShaderComputeContext ComputeContext;
    std::unordered_map<VkImage, VkImageView> VkImageViews;
};

