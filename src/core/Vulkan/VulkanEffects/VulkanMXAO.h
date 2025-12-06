#pragma once

#include "../VulkanEffect.h"

// ============================================================================
// Push constants for MXAO
// - Only per-shader parameters live here (Pass index + MXAO settings).
// - Global stuff like depth/normal/scene descriptors and camera globals
//   stay in the GlobalFrame UBO / descriptor set 0.
// ============================================================================

struct FVulkanMXAOPushConstants
{
    // Pass index: 0 = AO gen, 1 = blur1, 2 = blur2 + composite
    UINT Pass = 0;

    // Sampling / quality
    UINT GlobalSamplePreset = 4;
    UINT BaseSampleCount    = 32;

    // AO kernel
    float SampleRadius      = 1.0f;
    float SampleNormalBias  = 0.2f;

    // AO / IL strength & curve
    float SSAOAmount        = 1.0f;
    float SSILAmount        = 1.0f;
    float Power             = 1.0f;

    // Depth fade (0..1 in normalized view depth)
    float FadeDepthStart    = 0.0f;
    float FadeDepthEnd      = 1.0f;

    // Blur configuration
    float RenderScale       = 1.0f;
    float BlurRadius1       = 0.75f;
    float BlurRadius2       = 1.0f;
    UINT  BlurSteps1        = 4;
    UINT  BlurSteps2        = 8;

    // Toggles / misc
    UINT  EnableIL          = 1;
    UINT  TwoLayer          = 0;
    UINT  HighQuality       = 1;
    UINT  DebugView         = 0;

    // Two-layer AO intensity
    float AmountCoarse      = 1.0f;
    float AmountFine        = 1.0f;
};

// ============================================================================
// User-facing settings (NVR menu / config)
// - These map 1:1 into the push constants in FillPushConstants.
// ============================================================================

struct FVulkanMXAOSettings
{
    // Sampling / quality
    UINT  GlobalSamplePreset = 4;   // matches MXAO_GLOBAL_SAMPLE_QUALITY_PRESET
    UINT  BaseSampleCount    = 32;

    // AO kernel
    float SampleRadius       = 1.0f;
    float SampleNormalBias   = 0.2f;

    // AO / IL strength & curve
    float SSAOAmount         = 1.0f;
    float SSILAmount         = 1.0f;
    float Power              = 1.0f;

    // Depth fade (UI could be 0..100 and remapped to 0..1 in FillPushConstants)
    float FadeDepthStart     = 0.0f;
    float FadeDepthEnd       = 1.0f;

    // Blur configuration
    float RenderScale        = 1.0f;
    float BlurRadius1        = 0.75f;
    float BlurRadius2        = 1.0f;
    UINT  BlurSteps1         = 4;
    UINT  BlurSteps2         = 8;

    // Toggles
    bool  bEnableIL          = true;
    bool  bTwoLayer          = false;
    bool  bHighQuality       = true;
    bool  bDebugView         = false;

    // Two-layer AO intensity
    float AmountCoarse       = 1.0f;
    float AmountFine         = 1.0f;
};

// ============================================================================
// MXAO compute effect
// - 3 passes: AO gen, blur1, blur2+shape+composite
// - Set 0: global resources (depth, normals, scene color, etc.)
// - Set 1: MXAO-specific ping-pong storage images (mxaoIn / mxaoOut).
// ============================================================================

class FVulkanMXAO
    : public FComputeEffectWithSettings<FVulkanMXAOSettings, FVulkanMXAOPushConstants>
{
public:
    const char* GetName() const override          { return "VulkanMXAO"; }
    const char* GetSpirvFileName() const override { return "MXAO.comp.spv"; }

    FVulkanMXAO();

    EVulkanEffectPhase GetPhase() const override
    {
        // AO is applied before tonemapping, same as your normals effect.
        return EVulkanEffectPhase::PreTonemap;
    }

    // Optional: convenient access from code
    const FVulkanMXAOSettings& GetSettingsStruct() const { return Settings; }
    FVulkanMXAOSettings&       GetSettingsStruct()       { return Settings; }

protected:
    // IVulkanEffect overrides
    void DestroyResources() override;
    void CreatePipeline() override;
    void CreateDescriptorSets() override;
    void CreateInteropTextures() override;
    void CompleteRendering(IDirect3DSurface9* SceneColor) override;
    void UpdateSettingsFromNvr() override;

    // FComputeEffectBase overrides
    uint32_t   GetPassCount() const override { return 3; } // 0:AO, 1:blur, 2:blur+composite
    VkExtent2D GetDispatchExtent() const override;
    bool       PrepareResourcesForSubmit() override;
    void       RecordPassCommands(VkCommandBuffer cmd,
                                  uint32_t passIndex,
                                  uint32_t groupsX,
                                  uint32_t groupsY) override;
    void       OnAfterPass(VkCommandBuffer cmd, uint32_t passIndex) override;

    // FComputeEffectWithSettings override:
    void FillPushConstants(FVulkanMXAOPushConstants& out,
                           const FVulkanMXAOSettings& src,
                           uint32_t passIndex) const override;

private:
    // Update local descriptor set 1 bindings (mxaoIn / mxaoOut) per pass
    void UpdateDescriptorsForPass(uint32_t InPass);

    // Create / resize AO ping-pong surfaces as needed
    void CreateAOSurface0IfNeeded(uint32_t Width, uint32_t Height);
    void CreateAOSurface1IfNeeded(uint32_t Width, uint32_t Height);

    // Build NVR menu descriptors for MXAO settings
    void BuildSettingsDescriptors();

    // Two AO ping-pong surfaces (CommonTex0 / CommonTex1 analogue)
    FVulkanInteropSurface AOSurface0;
    FVulkanInteropSurface AOSurface1;
};
