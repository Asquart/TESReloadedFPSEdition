// #pragma once
//
// #include "../../VulkanEffect.h"
//
// class FVulkanDebugDepthEffect : public IVulkanEffect
// {
// public:
//     virtual const char* GetName() const override
//     {
//         return "CombineDepth";
//     }
//
//     FVulkanDebugDepthEffect();
//
//     // Depth should be ready before AO etc., so PreTonemap is reasonable
//     virtual EVulkanEffectPhase GetPhase() const override
//     {
//         return EVulkanEffectPhase::PreTonemap;
//     }
//
//     virtual void SubmitRendering() override;
//     virtual void CompleteRendering(IDirect3DSurface9* SceneColor) override;
//
// protected:
//     // IVulkanEffect overrides
//     virtual void DestroyResources() override;
//     virtual void CreatePipeline() override;
//     virtual void CreateDescriptorSets() override;
//     virtual void CreateInteropTextures() override;
//
// private:
//     void CreateOutputSurfaceIfNeeded(uint32_t Width, uint32_t Height);
//
//     // Dummy push constants struct to serve as an example
//     struct FPushConstants
//     {
//     };
//
//     FVulkanInteropSurface OutputSurface;
// };
