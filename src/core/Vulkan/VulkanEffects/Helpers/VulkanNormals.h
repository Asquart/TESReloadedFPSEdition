#pragma once

#include "../../VulkanEffect.h"

// Push constants (not exposed to menu; Pass stays internal for now)
struct FVulkanNormalsPushConstants
{
    UINT Pass = 0;
    UINT SmoothNumDirs   = 16;
    UINT SmoothNumSteps  = 2;
    float SmoothRadius = 1.0;
};

// Dummy settings (visible in menu, not yet used by shader)
struct FVulkanNormalsSettings
{
    UINT SmoothNumDirs   = 16;
    UINT SmoothNumSteps  = 2;
    float SmoothRadius = 1.0;
    bool bDebugView    = false;
};

class FVulkanNormals
    : public FComputeEffectWithSettings<FVulkanNormalsSettings, FVulkanNormalsPushConstants>
{
public:
    const char* GetName() const override               { return "VulkanNormals"; }
    const char* GetSpirvFileName() const override      { return "Normals.comp.spv"; }

    FVulkanNormals();

    EVulkanEffectPhase GetPhase() const override
    {
        return EVulkanEffectPhase::PreTonemap;
    }

    // Optional: convenient access from code
    const FVulkanNormalsSettings& GetSettingsStruct() const { return Settings; }
    FVulkanNormalsSettings&       GetSettingsStruct()       { return Settings; }

protected:
    // IVulkanEffect overrides
    void DestroyResources() override;
    void CreatePipeline() override;
    void CreateDescriptorSets() override;
    void CreateInteropTextures() override;
    void CompleteRendering(IDirect3DSurface9* SceneColor) override;
    void UpdateSettingsFromNvr() override;

    // FComputeEffectBase overrides/
    uint32_t   GetPassCount() const override { return 2; } // example: 3 passes
    VkExtent2D GetDispatchExtent() const override;
    bool       PrepareResourcesForSubmit() override;
    void       RecordPassCommands(VkCommandBuffer cmd,
                                  uint32_t passIndex,
                                  uint32_t groupsX,
                                  uint32_t groupsY) override;
    void       OnAfterPass(VkCommandBuffer cmd, uint32_t passIndex) override;

    // FComputeEffectWithSettings override:
    void FillPushConstants(FVulkanNormalsPushConstants& out,
                           const FVulkanNormalsSettings& src,
                           uint32_t passIndex) const override;

private:
    void UpdateDescriptorsForPass(uint32_t InPass);
    void CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height);
    void CreateTemporaryPassIfNeeded(uint32_t Width, uint32_t Height);
    void BuildSettingsDescriptors();

    FVulkanInteropSurface OutputSurface;
    FVulkanInteropSurface TemporarySurface;
};
