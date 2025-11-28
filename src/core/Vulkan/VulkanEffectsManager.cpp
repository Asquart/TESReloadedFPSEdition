#include "VulkanEffectsManager.h"

//Effects
#include "VulkanEffects/VulkanAO.h"

namespace VulkanEffectsManagerLocal
{
    static const std::string NormalsSurfaceName = "NormalsSurface";
    static const std::string DepthSurfaceName = "DepthSurface";
    static const std::string BlueNoiseTextureName = "BlueNoiseTexture";
}

void FVulkanEffectsManager::Initialize(IDirect3DDevice9* InD3D9Device)
{
    D3D9Device = InD3D9Device;

    D3DVIEWPORT9 Viewport{};
    InD3D9Device->GetViewport(&Viewport);

    ScreenWidth = Viewport.Width;
    ScreenHeight = Viewport.Height;

    VulkanContext.Initialize();
    InteropManager.Initialize();

    InitializeDepthSurface();
    GlobalResources.Initialize();

    RegisterEffects();

    // Call Initialize on all effects
    for (auto& EffectPair : EffectsPreTonemap)
        EffectPair.second->Initialize();

    for (auto& EffectPair : EffectsPostTonemap)
        EffectPair.second->Initialize();
}

void FVulkanEffectsManager::Shutdown()
{
    VulkanContext.Shutdown();
    InteropManager.Shutdown();
    GlobalResources.Shutdown();
}

void FVulkanEffectsManager::RenderPreTonemapping(IDirect3DSurface9* InSceneColor)
{
    DXVK_CheckReturn()

    GlobalResources.UpdatePerFrame();

    for (auto& EffectPair : EffectsPreTonemap)
    {
        Logger::Log("Submitting effect %s", EffectPair.first);
        EffectPair.second->SubmitRendering();
    }
    for (auto& EffectPair : EffectsPreTonemap)
    {
        Logger::Log("Completing effect %s", EffectPair.first);
        EffectPair.second->CompleteRendering(InSceneColor);
    }
}

void FVulkanEffectsManager::RenderPostTonemapping(IDirect3DSurface9* InSceneColor)
{
    DXVK_CheckReturn()
    for (auto& EffectPair : EffectsPostTonemap)
        EffectPair.second->CompleteRendering(InSceneColor);
}

FVulkanInteropSurface* FVulkanEffectsManager::GetDepthSurface()
{
    if (!DepthSurface.IsValid())
    {
        InitializeDepthSurface();
    }
    return &DepthSurface;
}

FVulkanInteropSurface* FVulkanEffectsManager::GetNormalsSurface()
{
    if (!NormalsSurface.IsValid())
    {
        InitializeNormalsSurface();
    }
    return &NormalsSurface;
}

FVulkanInteropSurface* FVulkanEffectsManager::GetBlueNoiseSurface()
{
    if (!BlueNoiseSurface.IsValid())
    {
        InitializeBlueNoiseSurface();
    }
    return &BlueNoiseSurface;
}

IVulkanEffect* FVulkanEffectsManager::GetEffectByName(std::string InName, EVulkanEffectPhase InPhase)
{
    if (InPhase == EVulkanEffectPhase::PreTonemap)
    {
        if (EffectsPreTonemap.contains(InName))
        {
            return EffectsPreTonemap[InName].get();
        }
        return nullptr;
    }
    if (EffectsPostTonemap.contains(InName))
    {
        return EffectsPostTonemap[InName].get();
    }
    return nullptr;
}

void FVulkanEffectsManager::InitializeSurfaces()
{
    InitializeDepthSurface();
    InitializeNormalsSurface();
    InitializeBlueNoiseSurface();
}

void FVulkanEffectsManager::InitializeDepthSurface()
{
    if (!TheShaderManager || !TheShaderManager->Effects.CombineDepth)
    {
        Logger::Log("FVulkanEffectsManager::InitializeDepthSurface: TheShaderManager or CombineDepth is invalid");
        return;
    }

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // Cleanup previous
    if (DepthSurface.View) {
        p_vkDestroyImageView(Vulkan.Device, DepthSurface.View, nullptr);
        DepthSurface.View = VK_NULL_HANDLE;
    }
    if (DepthSurface.D3DSurface) {
        DepthSurface.D3DSurface->Release();
        DepthSurface.D3DSurface = nullptr;
    }
    if (DepthSurface.D3DTexture) {
        DepthSurface.D3DTexture->Release();
        DepthSurface.D3DTexture = nullptr;
    }
    DepthSurface.InteropTex = nullptr;
    DepthSurface.Image = VK_NULL_HANDLE;
    DepthSurface.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    DepthSurface.CreateInfo = {};

    // Keep the surface
    DepthSurface.D3DSurface = TheShaderManager->Effects.CombineDepth->Textures.CombinedDepthSurface;
    DepthSurface.D3DSurface->AddRef();

    // Try to grab container texture (optional but nice)
    IUnknown* container = nullptr;
    if (SUCCEEDED(TheShaderManager->Effects.CombineDepth->Textures.CombinedDepthSurface->GetContainer(IID_IDirect3DTexture9, (void**)&container)) && container) {
        DepthSurface.D3DTexture = (IDirect3DTexture9*)container; // GetContainer already AddRef’d
    }

    // Describe to get size/format
    D3DSURFACE_DESC Desc{};
    TheShaderManager->Effects.CombineDepth->Textures.CombinedDepthSurface->GetDesc(&Desc);

    DepthSurface.Width = Desc.Width;
    DepthSurface.Height = Desc.Height;
    DepthSurface.Format = Desc.Format; // should be D3DFMT_G32R32F for NVR combined

    // Get interop
    HRESULT hr = DepthSurface.D3DSurface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        (void**)&DepthSurface.InteropTex);

    if (FAILED(hr) || !DepthSurface.InteropTex.ptr()) {
        Logger::Log("VulkanEffectsManager::InitializeDepthSurface: QI(ID3D9VkInteropTexture) failed hr=0x%08X", hr);
        return;
    }

    VkImage        img = VK_NULL_HANDLE;
    VkImageLayout  dummyLayout = {};
    VkImageCreateInfo dummyCi = {};

    hr = DepthSurface.InteropTex->GetVulkanImageInfo(&img, &dummyLayout, &dummyCi);
    Logger::Log("VulkanEffectsManager::InitializeDepthSurface: GetVulkanImageInfo hr=0x%08X image=%p fmt=%d",
        hr, img, dummyCi.format);

    if (!img) {
        Logger::Log("WrapNvrCombinedSurface: null VkImage");
        return;
    }

    DepthSurface.Image = img;
    DepthSurface.Layout = VK_IMAGE_LAYOUT_UNDEFINED; // we won't trust or touch it
    DepthSurface.CreateInfo = {};
    DepthSurface.CreateInfo.format = VK_FORMAT_R32G32_SFLOAT; // hardcoded format from DXVK for NVR CombinedDepthTexture specifically

    VkImageViewCreateInfo iv{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    iv.image = DepthSurface.Image;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format = DepthSurface.CreateInfo.format;
    iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    iv.subresourceRange.baseMipLevel = 0;
    iv.subresourceRange.levelCount = 1;
    iv.subresourceRange.baseArrayLayer = 0;
    iv.subresourceRange.layerCount = 1;

    VkResult rv = p_vkCreateImageView(Vulkan.Device, &iv, nullptr, &DepthSurface.View);
    if (rv != VK_SUCCESS || DepthSurface.View == VK_NULL_HANDLE) {
        Logger::Log("VulkanEffectsManager::InitializeDepthSurface: vkCreateImageView FAILED rv=%d fmt=%d",
            rv, DepthSurface.CreateInfo.format);
        DepthSurface.View = VK_NULL_HANDLE;
        DepthSurface.InteropTex = nullptr;
        return;
    }

    Logger::Log("VulkanEffectsManager::InitializeDepthSurface: OK image=%d view=%d fmt=%d", DepthSurface.Image, DepthSurface.View, DepthSurface.CreateInfo.format);

}


void FVulkanEffectsManager::InitializeNormalsSurface()
{
    InteropManager.GetOrCreateSurface(VulkanEffectsManagerLocal::NormalsSurfaceName, ScreenWidth, ScreenHeight, D3DFMT_A16B16G16R16F, /*bStorage*/ false);
}

void FVulkanEffectsManager::InitializeBlueNoiseSurface()
{
    IDirect3DTexture9* NoiseTex = nullptr;
    // Load the noise texture (you must include a proper .dds/.png in your mod)
    if (FAILED(D3DXCreateTextureFromFileA(D3D9Device,
        "Data\\Textures\\Effects\\bluenoise256.dds",
        &NoiseTex)))
    {
        Logger::Log("Could not load blue noise texture!");
        return;
    }

    // Store in interop system
    InteropManager.RegisterExternalTexture(VulkanEffectsManagerLocal::BlueNoiseTextureName, NoiseTex);

    NoiseTex->Release();
}

void FVulkanEffectsManager::RegisterEffects()
{
    for (const auto& Info : FVulkanEffectFactory::GetRegistry())
    {
        auto Effect = Info.Create();
        Effect->Initialize();

        if (Info.Phase == EVulkanEffectPhase::PreTonemap)
            EffectsPreTonemap[Info.Name] = std::move(Effect);
        else
            EffectsPostTonemap[Info.Name] = std::move(Effect);
    }
}

