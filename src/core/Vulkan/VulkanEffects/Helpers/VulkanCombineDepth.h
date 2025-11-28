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

    FVulkanCombineDepthEffect();

    // Depth should be ready before AO etc., so PreTonemap is reasonable
    virtual EVulkanEffectPhase GetPhase() const override
    {
        return EVulkanEffectPhase::PreTonemap;
    }

    virtual void SubmitRendering() override;
    virtual void CompleteRendering(IDirect3DSurface9* SceneColor) override;

protected:
    // IVulkanEffect overrides
    virtual void DestroyResources() override;
    virtual void CreatePipeline() override;
    virtual void CreateDescriptorSets() override;
    virtual void CreateInteropTextures() override;

private:
    void CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height);

    // Dummy push constants struct to serve as an example
    struct FPushConstants
    {
    };

    FVulkanInteropSurface OutputSurface;
};
