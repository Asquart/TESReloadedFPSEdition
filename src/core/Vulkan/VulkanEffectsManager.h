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


    FVulkanInteropSurface* GetDepthSurface();
    FVulkanInteropSurface* GetNormalsSurface();
    FVulkanInteropSurface* GetBlueNoiseSurface();

    IVulkanEffect* GetEffectByName(std::string InName, EVulkanEffectPhase InPhase);
    IVulkanEffect* FindEffectByName(const std::string& InName);

    UINT ScreenWidth;
    UINT ScreenHeight;

    FVulkanInteropSurface NormalsSurface {};
private:

    FVulkanInteropSurface DepthSurface{};
    FVulkanInteropSurface BlueNoiseSurface {};

    std::unordered_map<std::string, std::unique_ptr<IVulkanEffect>> EffectsPreTonemap;
    std::unordered_map<std::string, std::unique_ptr<IVulkanEffect>> EffectsPostTonemap;

    void RegisterEffects();
    void RefreshEffectSettings();

    void InitializeSurfaces();

    void InitializeDepthSurface();
    void InitializeNormalsSurface();
    void InitializeBlueNoiseSurface();
};
