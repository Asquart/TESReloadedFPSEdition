#pragma once
#include "VulkanEffect.h"
#include "VulkanGlobalResources.h"

class FVulkanEffectsManager
{
public:
    static FVulkanEffectsManager& Get()
    {
        static FVulkanEffectsManager Instance;
        return Instance;
    }

    FVulkanContext VulkanContext;
    FVulkanInteropManager InteropManager;
    FVulkanGlobalResources GlobalResources;
    IDirect3DDevice9* D3D9Device;

    void Initialize(IDirect3DDevice9* InD3D9Device);
    void Shutdown();

    void RenderPreTonemapping(IDirect3DSurface9* SceneColor);
    void RenderPostTonemapping(IDirect3DSurface9* SceneColor);

    IVulkanEffect* FindEffectById(const char* Id);
    float GetEffectGpuTimeMs(const char* Id) const;

    FVulkanInteropSurface* GetDepthSurface();
    FVulkanInteropSurface* GetNormalsSurface();
    FVulkanInteropSurface* GetBlueNoiseSurface();
    FVulkanInteropSurface* GetSceneColorSurface();

    IVulkanEffect* GetEffectByName(std::string InName, EVulkanEffectPhase InPhase);

    UINT ScreenWidth;
    UINT ScreenHeight;

    FVulkanInteropSurface NormalsSurface {};
private:

    FVulkanInteropSurface DepthSurface{};
    FVulkanInteropSurface BlueNoiseSurface {};
    FVulkanInteropSurface SceneColorSurface {};

    std::unordered_map<std::string, std::unique_ptr<IVulkanEffect>> EffectsPreTonemap;
    std::unordered_map<std::string, std::unique_ptr<IVulkanEffect>> EffectsPostTonemap;

    void RegisterEffects();

    void InitializeSurfaces();

    void InitializeDepthSurface();
    void InitializeNormalsSurface();
    void InitializeBlueNoiseSurface();
    void TryInitSceneColorSurface(IDirect3DSurface9* InSurface);
};
