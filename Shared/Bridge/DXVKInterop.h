#pragma once

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DBaseTexture9;
struct IDirect3DSurface9;

struct DxvkInteropSurfaceCreateInfo
{
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t format;
    uint32_t usageFlags;
    uint32_t sampleCount;
};

struct DxvkInteropExternalMemory
{
    uint64_t win32Handle;
    int32_t opaqueFd;
};

struct DxvkInteropSurfaceInfo
{
    bool valid;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    uint32_t format;
    uint32_t usageFlags;
    uint32_t sampleCount;
    uint32_t rowPitch;
    uint32_t depthPitch;
    uint64_t image;
    uint64_t imageView;
    uint64_t memory;
    uint64_t allocationSize;
    DxvkInteropExternalMemory externalMemory;
};

struct DxvkInteropFrameSync
{
    bool valid;
    uint64_t acquireSemaphore;
    uint64_t acquireValue;
    bool acquireIsTimeline;
    uint64_t releaseSemaphore;
    uint64_t releaseValue;
    bool releaseIsTimeline;
    uint64_t completionFence;
};

bool DxvkInterop_Initialize(IDirect3DDevice9* device);
bool DxvkInterop_IsAvailable();
bool DxvkInterop_GetSurfaceInfo(IDirect3DBaseTexture9* texture, DxvkInteropSurfaceInfo* outInfo);
bool DxvkInterop_CreateSiblingSurface(IDirect3DDevice9* device, const DxvkInteropSurfaceCreateInfo* createInfo, DxvkInteropSurfaceInfo* outInfo);
bool DxvkInterop_GetFrameSync(IDirect3DDevice9* device, DxvkInteropFrameSync* outSync);
bool DxvkInterop_DebugUpload(IDirect3DSurface9* sourceSurface, const DxvkInteropSurfaceInfo* destinationInfo);
bool DxvkInterop_DebugDownload(IDirect3DSurface9* destinationSurface, const DxvkInteropSurfaceInfo* sourceInfo);

