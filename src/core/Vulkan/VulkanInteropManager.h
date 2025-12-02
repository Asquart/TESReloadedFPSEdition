#pragma once

#include "VulkanContext.h"

struct FVulkanInteropSurface
{
    // D3D9 side
    IDirect3DTexture9* D3DTexture = nullptr;
    IDirect3DSurface9* D3DSurface = nullptr;

    // DXVK interop
    dxvk::Com<ID3D9VkInteropTexture> InteropTex;
    VkImage           Image = VK_NULL_HANDLE;
    VkImageLayout     Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageCreateInfo CreateInfo = {};

    bool bIsStorage = false;

    // Cached view
    VkImageView       View = VK_NULL_HANDLE;

    UINT Width = 0;
    UINT Height = 0;
    D3DFORMAT Format = D3DFMT_UNKNOWN;

    bool IsValid() const {
        return D3DSurface != nullptr &&
            Image != VK_NULL_HANDLE &&
            View != VK_NULL_HANDLE;
    }
};

class FVulkanInteropManager
{
public:
    void Initialize();
    void Shutdown();

    // Get or create a named RT (e.g. "AO_Input", "AO_Output") at given size
    FVulkanInteropSurface* GetOrCreateSurface(
        const std::string& Name,
        UINT Width,
        UINT Height,
        D3DFORMAT InFormat,
        bool bIsWriteable,
        VkImageLayout InLayout = VK_IMAGE_LAYOUT_UNDEFINED);

    void RegisterExternalTexture(const std::string& Name, IDirect3DTexture9* Texture);

    // Optional convenience overload
    void RegisterExternalTexture(const char* Name, IDirect3DTexture9* Texture)
    {
        RegisterExternalTexture(std::string(Name), Texture);
    }

    static inline bool IsDepthFormat(D3DFORMAT F)
    {
        return F == D3DFMT_D16 ||
            F == D3DFMT_D24X8 ||
            F == D3DFMT_D24S8 ||
            F == MAKEFOURCC('I', 'N', 'T', 'Z') ||
            F == MAKEFOURCC('R', 'A', 'W', 'Z');
    }

    static VkFormat MapD3DColorFormatToVk(D3DFORMAT fmt)
    {
        switch (fmt)
        {
        case D3DFMT_A8R8G8B8:        return VK_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_A16B16G16R16F:   return VK_FORMAT_R16G16B16A16_SFLOAT;
        case D3DFMT_A32B32G32R32F:   return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

    bool CreateSurface(FVulkanInteropSurface& Out, UINT InWidth, UINT InHeight, D3DFORMAT InFormat, bool bIsWriteable, VkImageLayout InLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    bool CreateSurfaceFromD3DTexture(
        FVulkanInteropSurface& Out,
        IDirect3DTexture9* InTexture,
        bool bIsWriteable,
        VkImageLayout InLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    bool CreateSurfaceFromD3DSurface(
        FVulkanInteropSurface& Out,
        IDirect3DSurface9* InTexture);
    void DestroySurface(FVulkanInteropSurface& InSurface);
    bool WrapExistingTexture(
        FVulkanInteropSurface& Out,
        IDirect3DTexture9* InTexture);

private:
    std::unordered_map<std::string, FVulkanInteropSurface> Surfaces;
};
