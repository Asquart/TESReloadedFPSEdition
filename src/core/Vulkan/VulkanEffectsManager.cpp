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

static float HalfToFloat(uint16_t h)
{
	uint16_t h_exp  = (h & 0x7C00u) >> 10;
	uint16_t h_frac = (h & 0x03FFu);
	uint16_t h_sign = (h & 0x8000u);

	uint32_t sign = uint32_t(h_sign) << 16;

	if (h_exp == 0) {
		// Zero / subnormal
		if (h_frac == 0) {
			uint32_t bits = sign; // +/- 0.0f
			float f;
			std::memcpy(&f, &bits, sizeof(f));
			return f;
		} else {
			// Normalize subnormal
			float mant = float(h_frac) / 1024.0f; // 2^10
			float val  = std::ldexp(mant, -14);   // 2^-14
			return (sign ? -val : val);
		}
	} else if (h_exp == 0x1F) {
		// Inf / NaN
		uint32_t exp  = 0xFFu << 23;
		uint32_t frac = uint32_t(h_frac) << 13;
		uint32_t bits = sign | exp | frac;
		float f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	} else {
		// Normalized
		int32_t exp = int32_t(h_exp) - 15 + 127; // half bias 15, float bias 127
		uint32_t frac = uint32_t(h_frac) << 13;

		uint32_t bits = sign | (uint32_t(exp) << 23) | frac;
		float f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	}
}

static bool LogSurfacePixelA16B16G16R16F(
    IDirect3DDevice9* device,
    IDirect3DSurface9* srcSurface,
    UINT x, UINT y,
    const std::string& msg)
{
    if (!device || !srcSurface) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: !device || !srcSurface");
        return false;
    }

    D3DSURFACE_DESC desc;
    if (FAILED(srcSurface->GetDesc(&desc))) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: GetDesc failed");
        return false;
    }

    if (desc.Format != D3DFMT_A16B16G16R16F) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: wrong format (%u)", desc.Format);
        return false;
    }

    IDirect3DSurface9* sysmem = nullptr;
    HRESULT hr = device->CreateOffscreenPlainSurface(
        desc.Width,
        desc.Height,
        desc.Format,          // A16B16G16R16F
        D3DPOOL_SYSTEMMEM,
        &sysmem,
        nullptr);

    if (FAILED(hr) || !sysmem) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: CreateOffscreenPlainSurface failed (hr=%08X)", hr);
        return false;
    }

    hr = device->GetRenderTargetData(srcSurface, sysmem);
    if (FAILED(hr)) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: GetRenderTargetData failed (hr=%08X)", hr);
        sysmem->Release();
        return false;
    }

    if (x >= desc.Width)  x = desc.Width  - 1;
    if (y >= desc.Height) y = desc.Height - 1;

    D3DLOCKED_RECT lr;
    hr = sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        Logger::Log("LogSurfacePixelA16B16G16R16F: LockRect failed (hr=%08X)", hr);
        sysmem->Release();
        return false;
    }

    const uint8_t* row      = (const uint8_t*)lr.pBits + y * lr.Pitch;
    const uint16_t* px_half = (const uint16_t*)row + x * 4; // 4 halfs per pixel (A,B,G,R)

    // NOTE: D3DFMT_A16B16G16R16F channel order is A,B,G,R
    uint16_t hA = px_half[0];
    uint16_t hB = px_half[1];
    uint16_t hG = px_half[2];
    uint16_t hR = px_half[3];

    float r = HalfToFloat(hR);
    float g = HalfToFloat(hG);
    float b = HalfToFloat(hB);
    float a = HalfToFloat(hA);

    Logger::Log("%s (x=%u,y=%u) -> R=%f G=%f B=%f A=%f",
                msg.c_str(), x, y, r, g, b, a);

    sysmem->UnlockRect();
    sysmem->Release();
    return true;
}

void FVulkanEffectsManager::RenderPreTonemapping(IDirect3DSurface9* InSceneColor)
{
    DXVK_CheckReturn()

    GlobalResources.UpdatePerFrame();
    for (auto& EffectPair : EffectsPreTonemap)
    {
        //Logger::Log("Submitting effect %s", EffectPair.first.c_str());
        EffectPair.second->SubmitRendering();
    }
    for (auto& EffectPair : EffectsPreTonemap)
    {
        //Logger::Log("Completing effect %s", EffectPair.first.c_str());
        EffectPair.second->CompleteRendering(InSceneColor);
    }

     // if (const FVulkanInteropSurface* DepthSurface = TheVulkanEffectsManager->GetNormalsSurface())
     // {
    //     LogSurfacePixelA16B16G16R16F(TheRenderManager->device, TheShaderManager->Effects.Normals->Textures.NormalsSurface, 1258, 1427, "NVR Normal pixel value");
    //     LogSurfacePixelA16B16G16R16F(TheRenderManager->device, DepthSurface->D3DSurface, 1258, 1427, "Vulkan Normal pixel value");
         //TheRenderManager->device->StretchRect(DepthSurface->D3DSurface, NULL, InSceneColor, NULL, D3DTEXF_NONE);
     //}
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
        return nullptr;
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
        DepthSurface.D3DTexture = (IDirect3DTexture9*)container; // GetContainer already AddRef�d
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

