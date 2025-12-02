#include "VulkanInteropManager.h"

void FVulkanInteropManager::Initialize()
{
}

void FVulkanInteropManager::Shutdown()
{
}

FVulkanInteropSurface* FVulkanInteropManager::GetOrCreateSurface(const std::string& Name, UINT Width, UINT Height, D3DFORMAT InFormat, bool bIsWriteable, VkImageLayout InLayout)
{
    if (Surfaces.contains(Name) && Surfaces[Name].IsValid())
    {
        return &Surfaces[Name];
    }
    FVulkanInteropSurface NewSurface;
    if (!CreateSurface(NewSurface, Width, Height, InFormat, bIsWriteable, InLayout))
    {
        Logger::Log("GetOrCreateSurface could not create new surface, returning nullptr");
        return nullptr;
    }
    Surfaces[Name] = NewSurface;

    Logger::Log("GetOrCreateSurface created new surface, returning ");
	return &NewSurface;
}

void FVulkanInteropManager::RegisterExternalTexture(const std::string& Name, IDirect3DTexture9* Texture)
{
    if (!TheVulkanEffectsManager->VulkanContext.Device || !TheVulkanEffectsManager->VulkanContext.InteropDevice.ptr()) {
        Logger::Log("RegisterExternalTexture('%s'): missing Context/Device/D3D9Device", Name.c_str());
        return;
    }

    if (!Texture) {
        Logger::Log("RegisterExternalTexture('%s'): Texture is null", Name.c_str());
        return;
    }

    // Get or create the slot
    FVulkanInteropSurface& Out = Surfaces[Name];

    // 0) Destroy any previous contents for this slot
    if (Out.View != VK_NULL_HANDLE) {
        p_vkDestroyImageView(TheVulkanEffectsManager->VulkanContext.Device, Out.View, nullptr);
        Out.View = VK_NULL_HANDLE;
    }

    if (Out.InteropTex.ptr()) {
        Out.InteropTex = nullptr;
    }

    if (Out.D3DSurface) {
        Out.D3DSurface->Release();
        Out.D3DSurface = nullptr;
    }

    if (Out.D3DTexture) {
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
    }

    Out.Image = VK_NULL_HANDLE;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Out.CreateInfo = VkImageCreateInfo{};

    // 1) Hold on to the D3D texture
    Out.D3DTexture = Texture;
    Out.D3DTexture->AddRef();

    // 2) Get level 0 surface
    HRESULT hr = Out.D3DTexture->GetSurfaceLevel(0, &Out.D3DSurface);
    if (FAILED(hr) || !Out.D3DSurface) {
        Logger::Log("RegisterExternalTexture('%s'): GetSurfaceLevel(0) failed hr=0x%08X", Name.c_str(), hr);
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
        return;
    }

    // 3) Query DXVK interop texture interface
    hr = Out.D3DSurface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        reinterpret_cast<void**>(&Out.InteropTex));

    if (FAILED(hr) || !Out.InteropTex.ptr()) {
        Logger::Log("RegisterExternalTexture('%s'): QI(ID3D9VkInteropTexture) failed hr=0x%08X",
            Name.c_str(), hr);

        Out.D3DSurface->Release();
        Out.D3DSurface = nullptr;
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
        return;
    }

    // 4) Get Vulkan image + layout + create info
    hr = Out.InteropTex->GetVulkanImageInfo(&Out.Image, &Out.Layout, &Out.CreateInfo);
    if (FAILED(hr) || Out.Image == VK_NULL_HANDLE) {
        Logger::Log("RegisterExternalTexture('%s'): GetVulkanImageInfo failed hr=0x%08X",
            Name.c_str(), hr);

        Out.InteropTex = nullptr;
        Out.D3DSurface->Release();
        Out.D3DSurface = nullptr;
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
        Out.Image = VK_NULL_HANDLE;
        return;
    }

    // 5) Create a VkImageView
    VkImageViewCreateInfo IvInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    IvInfo.image = Out.Image;
    IvInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    IvInfo.format = Out.CreateInfo.format; // should reflect the D3D texture's format via DXVK

    IvInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    IvInfo.subresourceRange.baseMipLevel = 0;
    IvInfo.subresourceRange.levelCount = Out.CreateInfo.mipLevels ? Out.CreateInfo.mipLevels : 1;
    IvInfo.subresourceRange.baseArrayLayer = 0;
    IvInfo.subresourceRange.layerCount = Out.CreateInfo.arrayLayers ? Out.CreateInfo.arrayLayers : 1;

    VkResult res = p_vkCreateImageView(TheVulkanEffectsManager->VulkanContext.Device, &IvInfo, nullptr, &Out.View);
    if (res != VK_SUCCESS) {
        Logger::Log("RegisterExternalTexture('%s'): vkCreateImageView failed res=%d", Name.c_str(), res);
        Out.View = VK_NULL_HANDLE;
        return;
    }

    Logger::Log("RegisterExternalTexture('%s'): registered external tex=%p image=%p view=%p",
        Name.c_str(), Texture, Out.Image, Out.View);
    return;
}

bool FVulkanInteropManager::CreateSurface(
    FVulkanInteropSurface& Out,
    UINT InWidth,
    UINT InHeight,
    D3DFORMAT InFormat,
    bool bIsWriteable,
    VkImageLayout InLayout)
{
    auto& VulkanContext = TheVulkanEffectsManager->VulkanContext;
    IDirect3DDevice9* D3D9 = TheVulkanEffectsManager->D3D9Device;

    if (!D3D9 || !VulkanContext.InteropDevice.ptr()) {
        Logger::Log("CreateSurface: missing D3D9 or InteropDevice");
        return false;
    }

    if (InWidth == 0 || InHeight == 0) {
        Logger::Log("CreateSurface: invalid size %ux%u", InWidth, InHeight);
        return false;
    }

    // --- Cleanup old resources ---
    if (Out.View != VK_NULL_HANDLE) {
        p_vkDestroyImageView(VulkanContext.Device, Out.View, nullptr);
        Out.View = VK_NULL_HANDLE;
    }

    if (Out.D3DSurface) { Out.D3DSurface->Release(); Out.D3DSurface = nullptr; }
    if (Out.D3DTexture) { Out.D3DTexture->Release(); Out.D3DTexture = nullptr; }

    Out.InteropTex = nullptr;
    Out.Image = VK_NULL_HANDLE;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Out.CreateInfo = VkImageCreateInfo{};
    Out.Width = InWidth;
    Out.Height = InHeight;
    Out.Format = InFormat;
    Out.bIsStorage = bIsWriteable; // if you have this field

    // Map D3D color formats to VkFormat (color-only helper)
    auto MapD3DColorFormatToVk = [](D3DFORMAT fmt) -> VkFormat
    {
        switch (fmt)
        {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            return VK_FORMAT_B8G8R8A8_UNORM;

        case D3DFMT_A16B16G16R16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;

        case D3DFMT_A32B32G32R32F:
            return VK_FORMAT_R32G32B32A32_SFLOAT;

            // Add more color formats here if you need them
        default:
            return VK_FORMAT_UNDEFINED;
        }
    };

    // --- 1) Ask DXVK to create an image for us ---
    D3D9VkExtImageDesc Desc{};
    Desc.Type = D3DRTYPE_TEXTURE;
    Desc.Width = InWidth;
    Desc.Height = InHeight;
    Desc.Depth = 1;
    Desc.MipLevels = 1;
    Desc.Usage = D3DUSAGE_RENDERTARGET;
    Desc.Format = InFormat;
    Desc.Pool = D3DPOOL_DEFAULT;
    Desc.MultiSample = D3DMULTISAMPLE_NONE;
    Desc.MultiSampleQuality = 0;
    Desc.Discard = false;
    Desc.IsAttachmentOnly = false;
    Desc.IsLockable = false;

    Desc.ImageUsage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (bIsWriteable) {
        Desc.ImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    Logger::Log("CreateSurface: CreateImage WxH=%ux%u fmt=%d usage=0x%08X storage=%d",
        InWidth, InHeight, InFormat, Desc.ImageUsage, bIsWriteable ? 1 : 0);

    dxvk::Com<IDirect3DResource9> Resource;
    HRESULT hr = VulkanContext.InteropDevice->CreateImage(&Desc, &Resource);
    if (FAILED(hr) || !Resource.ptr()) {
        Logger::Log("CreateSurface: InteropDevice->CreateImage failed hr=0x%08X", hr);
        return false;
    }

    // --- 2) Convert to D3D9 texture + get level-0 surface ---
    hr = Resource->QueryInterface(__uuidof(IDirect3DTexture9),
        (void**)&Out.D3DTexture);
    if (FAILED(hr) || !Out.D3DTexture) {
        Logger::Log("CreateSurface: QI(IDirect3DTexture9) failed hr=0x%08X", hr);
        return false;
    }

    hr = Out.D3DTexture->GetSurfaceLevel(0, &Out.D3DSurface);
    if (FAILED(hr) || !Out.D3DSurface) {
        Logger::Log("CreateSurface: GetSurfaceLevel(0) failed hr=0x%08X", hr);
        return false;
    }

    // --- 3) Get DXVK interop + raw VkImage handle ---
    hr = Out.D3DSurface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        (void**)&Out.InteropTex);

    if (FAILED(hr) || !Out.InteropTex.ptr()) {
        Logger::Log("CreateSurface: QI(ID3D9VkInteropTexture) failed hr=0x%08X", hr);
        return false;
    }

    VkImage          Img = VK_NULL_HANDLE;
    VkImageLayout    DummyLayout{};
    VkImageCreateInfo DummyCi{};

    hr = Out.InteropTex->GetVulkanImageInfo(&Img, &DummyLayout, &DummyCi);
    Logger::Log("CreateSurface: GetVulkanImageInfo hr=0x%08X image=%p dummyFmt=%d dummyUsage=0x%08X",
        hr, Img, DummyCi.format, DummyCi.usage);

    if (Img == VK_NULL_HANDLE) {
        Logger::Log("CreateSurface: GetVulkanImageInfo returned invalid image hr=0x%08X image=%p", hr, Img);
        return false;
    }

    Out.Image = Img;
    Out.Layout = InLayout; // we'll manage layout explicitly

    // --- 4) Create image view with a *known* VkFormat ---
    VkFormat VkFmt = MapD3DColorFormatToVk(InFormat);
    if (VkFmt == VK_FORMAT_UNDEFINED) {
        Logger::Log("CreateSurface: unsupported D3D format %d -> VkFormat_UNDEFINED", InFormat);
        return false;
    }

    Out.CreateInfo = {};
    Out.CreateInfo.format = VkFmt; // store something sane for debugging / future use

    VkImageViewCreateInfo IvInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    IvInfo.image = Out.Image;
    IvInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    IvInfo.format = VkFmt;
    IvInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    IvInfo.subresourceRange.baseMipLevel = 0;
    IvInfo.subresourceRange.levelCount = 1;
    IvInfo.subresourceRange.baseArrayLayer = 0;
    IvInfo.subresourceRange.layerCount = 1;

    VkResult rv = p_vkCreateImageView(VulkanContext.Device, &IvInfo, nullptr, &Out.View);
    if (rv != VK_SUCCESS || Out.View == VK_NULL_HANDLE) {
        Logger::Log("CreateSurface: vkCreateImageView failed (%d), view=%p", rv, Out.View);
        Out.View = VK_NULL_HANDLE;
        return false;
    }

    Logger::Log("CreateSurface: OK, image=%p view=%p storage=%d", Out.Image, Out.View, bIsWriteable ? 1 : 0);
    return true;
}


void FVulkanInteropManager::DestroySurface(FVulkanInteropSurface& InSurface)
{
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // --- Destroy Vulkan ImageView ---
    if (InSurface.View != VK_NULL_HANDLE)
    {
        if (Vulkan.Device)
            p_vkDestroyImageView(Vulkan.Device, InSurface.View, nullptr);

        InSurface.View = VK_NULL_HANDLE;
    }

    // --- Release DXVK interop texture ---
    if (InSurface.InteropTex.ptr())
    {
        InSurface.InteropTex = nullptr;   // dxvk::Com handles Release()
    }

    // --- Release D3D9 surface ---
    if (InSurface.D3DSurface)
    {
        InSurface.D3DSurface->Release();
        InSurface.D3DSurface = nullptr;
    }

    // --- Release D3D9 texture ---
    if (InSurface.D3DTexture)
    {
        InSurface.D3DTexture->Release();
        InSurface.D3DTexture = nullptr;
    }

    // --- Clear Vulkan image metadata ---
    InSurface.Image = VK_NULL_HANDLE;
    InSurface.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    InSurface.CreateInfo = {};

    // Optional: good for debugging
    // Logger::Log("FVulkanInteropManager::DestroySurface: Surface destroyed");
}

bool FVulkanInteropManager::WrapExistingTexture(
    FVulkanInteropSurface& Out,
    IDirect3DTexture9* InTexture)
{
    if (!InTexture ||
        !TheVulkanEffectsManager ||
        !TheVulkanEffectsManager->VulkanContext.Device) {
        Logger::Log("WrapColorTexture: invalid args");
        return false;
    }

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // --- Cleanup previous ---
    if (Out.View) {
        p_vkDestroyImageView(Vulkan.Device, Out.View, nullptr);
        Out.View = VK_NULL_HANDLE;
    }

    if (Out.D3DSurface) {
        Out.D3DSurface->Release();
        Out.D3DSurface = nullptr;
    }

    if (Out.D3DTexture) {
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
    }

    Out.InteropTex = nullptr;
    Out.Image = VK_NULL_HANDLE;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Out.CreateInfo = {};

    // --- Hold reference to the source texture ---
    Out.D3DTexture = InTexture;
    Out.D3DTexture->AddRef();

    // Level 0 surface
    HRESULT hr = Out.D3DTexture->GetSurfaceLevel(0, &Out.D3DSurface);
    if (FAILED(hr) || !Out.D3DSurface) {
        Logger::Log("WrapColorTexture: GetSurfaceLevel(0) failed hr=0x%08X", hr);
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
        return false;
    }

    // Just for logging / size info
    D3DSURFACE_DESC desc{};
    Out.D3DSurface->GetDesc(&desc);
    Out.Width = desc.Width;
    Out.Height = desc.Height;
    Out.Format = desc.Format;

    Logger::Log(
        "WrapColorTexture: D3D fmt=0x%08X (%d) size=%ux%u",
        desc.Format, desc.Format, desc.Width, desc.Height);

    // --- Get DXVK interop ---
    hr = Out.D3DSurface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        (void**)&Out.InteropTex);

    if (FAILED(hr) || !Out.InteropTex.ptr()) {
        Logger::Log("WrapColorTexture: QI(ID3D9VkInteropTexture) failed hr=0x%08X", hr);
        return false;
    }

    // Here we DO trust DXVK for color RTs: we only need VkImage + format
    VkImage          rawImage = VK_NULL_HANDLE;
    VkImageLayout    dummyLayout{};
    VkImageCreateInfo dummyCi{};

    hr = Out.InteropTex->GetVulkanImageInfo(&rawImage, &dummyLayout, &dummyCi);
    Logger::Log(
        "WrapColorTexture: GetVulkanImageInfo hr=0x%08X image=%p vkFmt=%d usage=0x%08X",
        hr, rawImage, dummyCi.format, dummyCi.usage);

    if (rawImage == VK_NULL_HANDLE) {
        Logger::Log("WrapColorTexture: null VkImage");
        return false;
    }

    auto MapD3DDepthToVk = [](D3DFORMAT fmt) -> VkFormat {
        switch (fmt) {
        case D3DFMT_D16:              return VK_FORMAT_D16_UNORM;
        case D3DFMT_D32:              return VK_FORMAT_D32_SFLOAT;
        case D3DFMT_D15S1:            return VK_FORMAT_D16_UNORM;
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'):
            // matches DXVK mapping for INTZ
            return VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    };

    auto MapD3DColorToVk = [](D3DFORMAT fmt) -> VkFormat {
        switch (fmt) {
        case D3DFMT_A8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_X8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_A16B16G16R16F:    return VK_FORMAT_R16G16B16A16_SFLOAT;
        case D3DFMT_A32B32G32R32F:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case D3DFMT_G32R32F:          return VK_FORMAT_R32G32_SFLOAT;
        default:                      return VK_FORMAT_UNDEFINED;
        }
    };

    Out.Image = rawImage;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED; // we'll transition as needed
    Out.CreateInfo = {};
    Out.CreateInfo.format = MapD3DColorToVk(desc.Format); // <- use DXVK's real format for this texture

    // For combined depth (G32R32F), this should be something like VK_FORMAT_R32G32_SFLOAT

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageViewCreateInfo iv{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    iv.image = Out.Image;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format = Out.CreateInfo.format;
    iv.subresourceRange.aspectMask = aspect;
    iv.subresourceRange.baseMipLevel = 0;
    iv.subresourceRange.levelCount = 1;
    iv.subresourceRange.baseArrayLayer = 0;
    iv.subresourceRange.layerCount = 1;

     p_vkCreateImageView(Vulkan.Device, &iv, nullptr, &Out.View);

    Logger::Log("WrapColorTexture: OK tex=%p surf=%p image=%p view=%p vkFmt=%d",
        InTexture, Out.D3DSurface, Out.Image, Out.View, Out.CreateInfo.format);

    return true;
}


bool FVulkanInteropManager::CreateSurfaceFromD3DTexture(
    FVulkanInteropSurface& Out,
    IDirect3DTexture9* InTexture,
    bool bIsWriteable, VkImageLayout InLayout)
{
    if (!InTexture ||
        !TheVulkanEffectsManager ||
        !TheVulkanEffectsManager->D3D9Device ||
        !TheVulkanEffectsManager->VulkanContext.InteropDevice.ptr())
    {
        Logger::Log("CreateSurfaceFromD3DTexture: invalid args");
        return false;
    }

    IDirect3DDevice9* D3D9Device = TheVulkanEffectsManager->D3D9Device;
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // --- Cleanup previous state ---
    if (Out.View) {
        p_vkDestroyImageView(Vulkan.Device, Out.View, nullptr);
        Out.View = VK_NULL_HANDLE;
    }

    if (Out.D3DSurface) { Out.D3DSurface->Release(); Out.D3DSurface = nullptr; }
    if (Out.D3DTexture) { Out.D3DTexture->Release(); Out.D3DTexture = nullptr; }

    Out.InteropTex = nullptr;
    Out.Image = VK_NULL_HANDLE;
    Out.Layout = InLayout;
    Out.CreateInfo = {};
    Out.Width = 0;
    Out.Height = 0;
    Out.Format = D3DFMT_UNKNOWN;

    // --- Describe source texture ---
    D3DSURFACE_DESC Desc{};
    HRESULT hr = InTexture->GetLevelDesc(0, &Desc);
    if (FAILED(hr)) {
        Logger::Log("CreateSurfaceFromD3DTexture: GetLevelDesc failed hr=0x%08X", hr);
        return false;
    }

    Out.Width = Desc.Width;
    Out.Height = Desc.Height;
    Out.Format = Desc.Format;

    // Depth or color?
    bool bIsDepthFormat = false;
    switch (Desc.Format)
    {
    case D3DFMT_D16:
    case D3DFMT_D24X8:
    case D3DFMT_D24S8:
    case D3DFMT_D32:
    case D3DFMT_D15S1:
    case D3DFMT_D24X4S4:
        bIsDepthFormat = true;
        break;
    default:
        if (Desc.Format == (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'))
            bIsDepthFormat = true;
        break;
    }

    // Map D3D formats to VkFormat (our own mapping, not DXVK's)
    auto MapD3DDepthToVk = [](D3DFORMAT fmt) -> VkFormat
    {
        switch (fmt)
        {
        case D3DFMT_D16:              return VK_FORMAT_D16_UNORM;
        case D3DFMT_D24X8:
        case D3DFMT_D24S8:
        case D3DFMT_D24X4S4:
        case (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'):
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case D3DFMT_D32:              return VK_FORMAT_D32_SFLOAT;
        case D3DFMT_D15S1:            return VK_FORMAT_D16_UNORM;
        default:                      return VK_FORMAT_UNDEFINED;
        }
    };

    auto MapD3DColorToVk = [](D3DFORMAT fmt) -> VkFormat
    {
        switch (fmt)
        {
        case D3DFMT_A8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_X8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_A16B16G16R16F:    return VK_FORMAT_R16G16B16A16_SFLOAT;
        case D3DFMT_A32B32G32R32F:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:                      return VK_FORMAT_UNDEFINED;
        }
    };

    VkFormat VkFmt = bIsDepthFormat
        ? MapD3DDepthToVk(Desc.Format)
        : MapD3DColorToVk(Desc.Format);

    // --- 1) Create a DXVK-owned image that we can interop with ---
    D3D9VkExtImageDesc VkDesc{};
    VkDesc.Type = D3DRTYPE_TEXTURE;
    VkDesc.Width = Desc.Width;
    VkDesc.Height = Desc.Height;
    VkDesc.Depth = 1;
    VkDesc.MipLevels = 1;

    VkDesc.Usage = bIsDepthFormat
        ? D3DUSAGE_DEPTHSTENCIL
        : D3DUSAGE_RENDERTARGET;

    VkDesc.Format = Desc.Format;
    VkDesc.Pool = D3DPOOL_DEFAULT;
    VkDesc.MultiSample = D3DMULTISAMPLE_NONE;
    VkDesc.MultiSampleQuality = 0;
    VkDesc.Discard = bIsDepthFormat;
    VkDesc.IsAttachmentOnly = false;
    VkDesc.IsLockable = false;

    VkDesc.ImageUsage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Only allow STORAGE on color render targets
    if (bIsWriteable && !bIsDepthFormat) {
        VkDesc.ImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    dxvk::Com<IDirect3DResource9> Resource;
    hr = Vulkan.InteropDevice->CreateImage(&VkDesc, &Resource);
    if (FAILED(hr) || !Resource.ptr())
    {
        Logger::Log("CreateSurfaceFromD3DTexture: CreateImage failed hr=0x%08X", hr);
        return false;
    }

    // --- 2) Convert to IDirect3DTexture9 + get its surface ---
    hr = Resource->QueryInterface(__uuidof(IDirect3DTexture9),
        (void**)&Out.D3DTexture);
    if (FAILED(hr) || !Out.D3DTexture) {
        Logger::Log("CreateSurfaceFromD3DTexture: QI(IDirect3DTexture9) failed hr=0x%08X", hr);
        return false;
    }

    hr = Out.D3DTexture->GetSurfaceLevel(0, &Out.D3DSurface);
    if (FAILED(hr) || !Out.D3DSurface) {
        Logger::Log("CreateSurfaceFromD3DTexture: GetSurfaceLevel(0) failed hr=0x%08X", hr);
        return false;
    }

    // --- 3) Copy the original D3D9 texture contents into the DXVK texture ---
    //{
    //    dxvk::Com<IDirect3DSurface9> SrcSurface;
    //    hr = InTexture->GetSurfaceLevel(0, &SrcSurface);
    //    if (FAILED(hr) || !SrcSurface.ptr()) {
    //        Logger::Log("CreateSurfaceFromD3DTexture: InTexture->GetSurfaceLevel failed hr=0x%08X", hr);
    //        return false;
    //    }

    //    hr = D3D9Device->StretchRect(
    //        SrcSurface.ptr(), nullptr,
    //        Out.D3DSurface, nullptr,
    //        D3DTEXF_POINT);
    //    if (FAILED(hr)) {
    //        Logger::Log("CreateSurfaceFromD3DTexture: StretchRect failed hr=0x%08X", hr);
    //        return false;
    //    }
    //}

    // --- 4) Get Vulkan interop handle (image only) ---
    hr = Out.D3DSurface->QueryInterface(__uuidof(ID3D9VkInteropTexture),
        (void**)&Out.InteropTex);
    if (FAILED(hr) || !Out.InteropTex.ptr()) {
        Logger::Log("CreateSurfaceFromD3DTexture: QI(ID3D9VkInteropTexture) failed hr=0x%08X", hr);
        return false;
    }

    VkImage DummyImage = VK_NULL_HANDLE;
    VkImageLayout DummyLayout{};
    VkImageCreateInfo DummyInfo{};

    hr = Out.InteropTex->GetVulkanImageInfo(&DummyImage, &DummyLayout, &DummyInfo);
    Logger::Log("CreateSurfaceFromD3DTexture: GetVulkanImageInfo hr=0x%08X image=%p", hr, DummyImage);

    Out.Image = DummyImage;                 // ONLY trusted field
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;  // we will treat it as unknown
    Out.CreateInfo = {};
    Out.CreateInfo.format = VkFmt;              // store our own view format

    if (Out.Image == VK_NULL_HANDLE) {
        Logger::Log("CreateSurfaceFromD3DTexture: null VkImage");
        return false;
    }

    // --- 5) Create Vulkan image view with proper aspect ---
    VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (bIsDepthFormat) {
        bool HasStencil =
            (VkFmt == VK_FORMAT_D24_UNORM_S8_UINT) ||
            (VkFmt == VK_FORMAT_D32_SFLOAT_S8_UINT);

        Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (HasStencil)
            Aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo Iv{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    Iv.image = Out.Image;
    Iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    Iv.format = VkFmt;
    Iv.subresourceRange.aspectMask = Aspect;
    Iv.subresourceRange.baseMipLevel = 0;
    Iv.subresourceRange.levelCount = 1;
    Iv.subresourceRange.baseArrayLayer = 0;
    Iv.subresourceRange.layerCount = 1;

    VkResult rv = p_vkCreateImageView(Vulkan.Device, &Iv, nullptr, &Out.View);
    if (rv != VK_SUCCESS || Out.View == VK_NULL_HANDLE) {
        Logger::Log("CreateSurfaceFromD3DTexture: vkCreateImageView failed (%d) image=%p fmt=%d",
            rv, Out.Image, VkFmt);
        Out.View = VK_NULL_HANDLE;
        return false;
    }

    if (!Out.IsValid()) {
        Logger::Log("CreateSurfaceFromD3DTexture: Out surface invalid after creation (image/view mismatch)");
        return false;
    }

    Logger::Log("CreateSurfaceFromD3DTexture: finished successfully tex=%p image=%p view=%p depth=%d vkFmt=%d",
        InTexture, Out.Image, Out.View, bIsDepthFormat ? 1 : 0, VkFmt);

    return true;
}


bool FVulkanInteropManager::CreateSurfaceFromD3DSurface(
    FVulkanInteropSurface& Out,
    IDirect3DSurface9* InSurface)
{
    if (!InSurface ||
        !TheVulkanEffectsManager ||
        !TheVulkanEffectsManager->VulkanContext.Device)
    {
        Logger::Log("CreateSurfaceFromD3DSurface: invalid args");
        return false;
    }

    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    // --- Cleanup previous ---
    if (Out.View) {
        p_vkDestroyImageView(Vulkan.Device, Out.View, nullptr);
        Out.View = VK_NULL_HANDLE;
    }

    if (Out.D3DSurface) {
        Out.D3DSurface->Release();
        Out.D3DSurface = nullptr;
    }

    if (Out.D3DTexture) {
        Out.D3DTexture->Release();
        Out.D3DTexture = nullptr;
    }

    Out.InteropTex = nullptr;
    Out.Image = VK_NULL_HANDLE;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Out.CreateInfo = {};

    // --- Hold reference to the source surface ---
    Out.D3DSurface = InSurface;
    Out.D3DSurface->AddRef();

    // Optional: grab container texture
    {
        IUnknown* container = nullptr;
        if (SUCCEEDED(InSurface->GetContainer(IID_IDirect3DTexture9, (void**)&container)) && container) {
            Out.D3DTexture = static_cast<IDirect3DTexture9*>(container);
            // GetContainer already AddRef'd
        }
    }

    // Describe source to detect depth vs color and choose VkFormat
    D3DSURFACE_DESC desc{};
    InSurface->GetDesc(&desc);

    Out.Width = desc.Width;
    Out.Height = desc.Height;
    Out.Format = desc.Format;

    const bool isDepthFormat =
        desc.Format == D3DFMT_D16 ||
        desc.Format == D3DFMT_D32 ||
        desc.Format == D3DFMT_D15S1 ||
        desc.Format == D3DFMT_D24S8 ||
        desc.Format == D3DFMT_D24X8 ||
        desc.Format == D3DFMT_D24X4S4 ||
        desc.Format == (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z');  // INTZ

    auto MapD3DDepthToVk = [](D3DFORMAT fmt) -> VkFormat {
        switch (fmt) {
        case D3DFMT_D16:              return VK_FORMAT_D16_UNORM;
        case D3DFMT_D32:              return VK_FORMAT_D32_SFLOAT;
        case D3DFMT_D15S1:            return VK_FORMAT_D16_UNORM;
        case D3DFMT_D24S8:
        case D3DFMT_D24X8:
        case D3DFMT_D24X4S4:
        case (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'):
            // matches DXVK mapping for INTZ
            return VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    };

    auto MapD3DColorToVk = [](D3DFORMAT fmt) -> VkFormat {
        switch (fmt) {
        case D3DFMT_A8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_X8R8G8B8:         return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_A16B16G16R16F:    return VK_FORMAT_R16G16B16A16_SFLOAT;
        case D3DFMT_A32B32G32R32F:    return VK_FORMAT_R32G32B32A32_SFLOAT;
        case D3DFMT_G32R32F:          return VK_FORMAT_R32G32_SFLOAT;
        default:                      return VK_FORMAT_UNDEFINED;
        }
    };

    VkFormat vkFmt = isDepthFormat
        ? MapD3DDepthToVk(desc.Format)
        : MapD3DColorToVk(desc.Format);

    Logger::Log(
        "CreateSurfaceFromD3DSurface: D3D fmt=0x%08X (%d) depth=%d -> VkFmt=%d",
        desc.Format, desc.Format, isDepthFormat ? 1 : 0, vkFmt);

    if (vkFmt == VK_FORMAT_UNDEFINED) {
        Logger::Log("CreateSurfaceFromD3DSurface: unsupported D3D format 0x%08X", desc.Format);
        return false;
    }

    Logger::Log(
        "CreateSurfaceFromD3DSurface: BEFORE QueryInterface vkFmt=%d addr=%p",
        vkFmt, &vkFmt);
    // --- Get DXVK interop + raw VkImage handle ---
    HRESULT hr = Out.D3DSurface->QueryInterface(
        __uuidof(ID3D9VkInteropTexture),
        (void**)&Out.InteropTex);

    if (FAILED(hr) || !Out.InteropTex.ptr()) {
        Logger::Log("CreateSurfaceFromD3DSurface: QI(ID3D9VkInteropTexture) failed hr=0x%08X", hr);
        return false;
    }

    VkImage        rawImage = VK_NULL_HANDLE;
    VkImageLayout  dummyLayout{};
    VkImageCreateInfo dummyCi{};

    Logger::Log(
        "CreateSurfaceFromD3DSurface: BEFORE GetVulkanImageInfo vkFmt=%d addr=%p",
        vkFmt, &vkFmt);
    hr = Out.InteropTex->GetVulkanImageInfo(&rawImage, &dummyLayout, &dummyCi);
    Logger::Log("CreateSurfaceFromD3DSurface: GetVulkanImageInfo hr=0x%08X image=%p",
        hr, rawImage);

    if (rawImage == VK_NULL_HANDLE) {
        Logger::Log("CreateSurfaceFromD3DSurface: null VkImage");
        return false;
    }

    Out.Image = rawImage;
    Out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Out.CreateInfo = {};
    Out.CreateInfo.format = vkFmt;

    // --- Aspect mask ---
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (isDepthFormat) {
        bool hasStencil =
            vkFmt == VK_FORMAT_D24_UNORM_S8_UINT ||
            vkFmt == VK_FORMAT_D32_SFLOAT_S8_UINT;

        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil)
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo iv{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    iv.image = Out.Image;
    iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv.format = vkFmt;
    iv.subresourceRange.aspectMask = aspect;
    iv.subresourceRange.baseMipLevel = 0;
    iv.subresourceRange.levelCount = 1;
    iv.subresourceRange.baseArrayLayer = 0;
    iv.subresourceRange.layerCount = 1;
    p_vkCreateImageView(Vulkan.Device, &iv, nullptr, &Out.View);
    //Logger::Log(
    //    "CreateSurfaceFromD3DSurface: BEFORE p_vkCreateImageView vkFmt=%d addr=%p device=%d, image=%d",
    //    vkFmt, &vkFmt, Vulkan.Device, iv.image);
    //VkResult rv = p_vkCreateImageView(Vulkan.Device, &iv, nullptr, &Out.View);
    //if (rv != VK_SUCCESS || Out.View == VK_NULL_HANDLE) {
    //    Logger::Log(
    //        "CreateSurfaceFromD3DSurface: vkCreateImageView FAILED rv=%d image=%p VkFmt=%d aspect=0x%X",
    //        rv, Out.Image, vkFmt, aspect);
    //    Out.View = VK_NULL_HANDLE;
    //    Out.InteropTex = nullptr;
    //    return false;
    //}
    //Logger::Log(
    //    "CreateSurfaceFromD3DSurface: AFTER  GetVulkanImageInfo surface=%p image=%p view=%p depth=%b VkFmt=%d aspect=0x%X",
    //    InSurface,
    //    Out.Image,
    //    Out.View,
    //    isDepthFormat,
    //    vkFmt,
    //    aspect);

    //Logger::Log(
    //    "CreateSurfaceFromD3DSurface: OK surface=%p image=%p view=%p depth=%b VkFmt=%d aspect=0x%X",
    //    InSurface,
    //    Out.Image,
    //    Out.View,
    //    isDepthFormat,
    //    vkFmt,
    //    aspect);

    return true;
}
