#pragma once

#include "../../VulkanEffect.h"

struct FSimpleInteropSurface
{
    dxvk::Com<IDirect3DSurface9> Surface;
    dxvk::Com<ID3D9VkInteropTexture> Interop;

    VkImage     Image = VK_NULL_HANDLE;
    VkImageView View = VK_NULL_HANDLE;
};

// Combines world + viewmodel depth into a single buffer using CombineDepth.comp.spv
// and (optionally) can debug-blit it to the scene color.
class FVulkanCombineDepthEffect : public IVulkanEffect
{
public:
    virtual const char* GetName() const override
    {
        return "CombineDepth";
    }

    // Depth should be ready before AO etc., so PreTonemap is reasonable
    virtual EVulkanEffectPhase GetPhase() const override
    {
        return EVulkanEffectPhase::PreTonemap;
    }

    // Main render hook – for now we’ll *optionally* blit combined depth to SceneColor for debug.
    virtual void RenderPreTonemapping(IDirect3DSurface9* SceneColor) override;

    // No post-tonemap path for this effect
    virtual void RenderPostTonemapping(IDirect3DSurface9* /*SceneColor*/) override
    {
        // Intentionally empty
    }

    virtual ~FVulkanCombineDepthEffect();

    void DebugRunOnNvrCombinedDepth(IDirect3DSurface9* SceneColor);

    // If other effects need the combined depth, they can grab this:
    FVulkanInteropSurface* GetCombinedDepthSurface() { return &CombinedDepthSurface; }

protected:
    // IVulkanEffect overrides
    virtual void CreateResources() override;
    virtual void DestroyResources() override;

    virtual void UpdateSettingsFromNvr() override;
    virtual void CreateShaderModule() override;
    virtual void CreatePipeline() override;
    virtual void CreateDescriptorSets() override;
    virtual void CreateInteropTextures() override;

private:
    void RecreateCombinedSurfaceIfNeeded(uint32_t Width, uint32_t Height);

    // Push constants layout matching CombineDepth.comp.glsl
    struct FPushConstants
    {
        float DepthConstants[4];   // TESR_DepthConstants: x=viewModelNearZ, z=invertedDepth
        float CameraData[4];       // TESR_CameraData: x=nearZ, y=farZ
        float InvProjection[16];   // TESR_InvProjectionTransform (column-major)
    };

    // Vulkan pipeline objects
    VkShaderModule        ShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout      PipelineLayout = VK_NULL_HANDLE;
    VkPipeline            Pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      DescPool = VK_NULL_HANDLE;
    VkDescriptorSet       DescSet = VK_NULL_HANDLE;

    // Interop depth surfaces:
    //  - World + ViewModel: updated each frame by Capture*DepthFromD3D.
    //  - CombinedDepthSurface: storage image written by compute pass.
    FVulkanInteropSurface WorldDepthSurface;
    FVulkanInteropSurface ViewModelDepthSurface;
    FVulkanInteropSurface CombinedDepthSurface;

    //FSimpleInteropSurface NvrCombined;

    // Flags tracking whether we have valid input for the combine pass
    bool bHasWorldDepth = false;
    bool bHasViewModelDepth = false;
};
