#pragma once

#include "../../VulkanEffect.h"

class FVulkanNormals : public IVulkanEffect
{
public:
    FVulkanNormals();

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
    void UpdateDescriptorsForPass(uint32_t InPass);
    virtual void CreateInteropTextures() override;
    virtual void UpdateSettingsFromNvr() override;

private:
    void CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height);
    void CreateTemporaryPassIfNeeded(uint32_t Width, uint32_t Height);

    // Dummy push constants struct to serve as an example
    struct FPushConstants
    {
        UINT Pass = 0;
    };

    FVulkanInteropSurface OutputSurface;
    FVulkanInteropSurface TemporarySurface;
};
