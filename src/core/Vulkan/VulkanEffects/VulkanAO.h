#pragma once
#include "../VulkanEffect.h"

#include <string>

class FVulkanAmbientOcclusionEffect : public IVulkanEffect
{
public:
    virtual const char* GetName() const override
    {
        return "AmbientOcclusion";
    }

    virtual EVulkanEffectPhase GetPhase() const override
    {
        return EVulkanEffectPhase::PreTonemap;
    }

    virtual void Initialize() override;

    virtual void RenderPreTonemapping(IDirect3DSurface9* SceneColor) override;

    virtual ~FVulkanAmbientOcclusionEffect();

private:
    bool bInitialized = false;

    // Pipeline stuff
    VkShaderModule      ShaderModuleSsao = VK_NULL_HANDLE;
    VkPipelineLayout    PipelineLayoutSsao = VK_NULL_HANDLE;
    VkPipeline          PipelineSsao = VK_NULL_HANDLE;
    VkDescriptorSetLayout DescSetLayoutSsao = VK_NULL_HANDLE;

    VkDescriptorPool    DescPool = VK_NULL_HANDLE;
    VkDescriptorSet DescSet = VK_NULL_HANDLE;

    // Interop RT names (keys for FVulkanInteropManager)
    std::string InputName = "AO_SourceColor";
    std::string AoPrevName = "AO_Prev";
    std::string AoOutputName = "AO_Output";

    FVulkanInteropSurface* DepthSurface;
    FVulkanInteropSurface* NormalsSurface;
    FVulkanInteropSurface* NoiseSurface;

    //Render surfaces
    FVulkanInteropSurface* InputSurface = nullptr;
    FVulkanInteropSurface* AoPrevSurface = nullptr;
    FVulkanInteropSurface* AoOutputSurface = nullptr;

    // AO params – mirror HLSL constants
    struct FAmbientOcclusionSettings
    {
        float AOsamples;
        float AOstrength;
        float AOclamp;
        float AOrange;
        float AOangleBias;
        float AOlumThreshold;
        float BlurDrop;
        float BlurRadius;
    } AOSettings{};

    // Push constants (matches shader)
    struct FPushConstants
    {
        float OffsetMaskX;
        float OffsetMaskY;
    };

private:
    virtual void CreateResources() override;
    virtual void DestroyResources() override;

    void RunSsaoPass(
        FVulkanInteropSurface& SourceColor,
        FVulkanInteropSurface& AoPrev,
        FVulkanInteropSurface& AoOutput,
        const FPushConstants& Push);

    virtual void UpdateSettingsFromNvr() override; // read AO data from existing NVR settings
    virtual void CreateShaderModule() override;
    virtual void CreatePipeline() override;
    virtual void CreateDescriptorSets() override;
    virtual void CreateInteropTextures() override;
};
