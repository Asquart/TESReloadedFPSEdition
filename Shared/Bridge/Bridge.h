#pragma once

#include "BridgeExports.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TR_BRIDGE_CONFIGURATION_VERSION 1u

typedef enum TRBridgeRenderTargetUsageFlags
{
    TR_BRIDGE_RT_USAGE_COLOR_BIT = 1u << 0,
    TR_BRIDGE_RT_USAGE_DEPTH_BIT = 1u << 1,
    TR_BRIDGE_RT_USAGE_SAMPLED_BIT = 1u << 2,
    TR_BRIDGE_RT_USAGE_STORAGE_BIT = 1u << 3
} TRBridgeRenderTargetUsageFlags;

typedef enum TRBridgeFormat
{
    TR_BRIDGE_FORMAT_UNDEFINED = 0,
    TR_BRIDGE_FORMAT_R8G8B8A8_UNORM = 37,
    TR_BRIDGE_FORMAT_R16G16B16A16_FLOAT = 97,
    TR_BRIDGE_FORMAT_R32_SFLOAT = 100,
    TR_BRIDGE_FORMAT_R32G32_SFLOAT = 103,
    TR_BRIDGE_FORMAT_R32G32B32A32_FLOAT = 109,
    TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT = 129
} TRBridgeFormat;

typedef enum TRBridgeInteropHandleType
{
    TR_BRIDGE_INTEROP_HANDLE_NONE = 0,
    TR_BRIDGE_INTEROP_HANDLE_D3D9_TEXTURE_POINTER = 1,
    TR_BRIDGE_INTEROP_HANDLE_D3D9_SURFACE_POINTER = 2,
    TR_BRIDGE_INTEROP_HANDLE_WIN32_SHARED_HANDLE = 3,
    TR_BRIDGE_INTEROP_HANDLE_OPAQUE_FD = 4,
    TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE = 5,
    TR_BRIDGE_INTEROP_HANDLE_VK_DEVICE_MEMORY = 6,
    TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW = 7,
    TR_BRIDGE_INTEROP_HANDLE_VK_SEMAPHORE = 8,
    TR_BRIDGE_INTEROP_HANDLE_VK_FENCE = 9
} TRBridgeInteropHandleType;

typedef struct TRBridgeInteropHandle
{
    TRBridgeInteropHandleType type;
    uint64_t value;
    uint64_t auxValue;
} TRBridgeInteropHandle;

#define TR_BRIDGE_HANDLE_AUX_TIMELINE_BIT (1ull << 63)
#define TR_BRIDGE_HANDLE_AUX_VALUE_MASK (~TR_BRIDGE_HANDLE_AUX_TIMELINE_BIT)

#define TR_BRIDGE_MAX_INTEROP_HANDLES 4

typedef struct TRBridgeConfiguration
{
    uint32_t version;
    uint32_t flags;
    float ambientOcclusionRadius;
    float ambientOcclusionIntensity;
    float ambientOcclusionPower;
} TRBridgeConfiguration;

#define TR_BRIDGE_FLAG_ENABLE_VULKAN_AO (1u << 0)

typedef struct TRBridgeRenderTargetDescriptor
{
    char name[64];
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    TRBridgeFormat format;
    uint32_t usageFlags;
} TRBridgeRenderTargetDescriptor;

typedef struct TRBridgePluginFrameInputs
{
    uint64_t frameId;
    uint32_t renderTargetCount;
} TRBridgePluginFrameInputs;

typedef void (*TRBridgeLogCallback)(const char* message, void* userData);

typedef void (*TRBridgeFrameNotification)(uint64_t frameId, void* userData);

typedef struct TRBridgeLayerCallbacks
{
    TRBridgeFrameNotification onFrameReady;
    void* userData;
} TRBridgeLayerCallbacks;

typedef struct TRBridgeInteropSurface
{
    TRBridgeRenderTargetDescriptor descriptor;
    TRBridgeInteropHandle handles[TR_BRIDGE_MAX_INTEROP_HANDLES];
    uint32_t handleCount;
    uint32_t rowPitch;
    uint32_t depthPitch;
} TRBridgeInteropSurface;

typedef struct TRBridgeInteropSyncState
{
    uint64_t frameId;
    TRBridgeInteropHandle waitHandles[TR_BRIDGE_MAX_INTEROP_HANDLES];
    uint32_t waitHandleCount;
    TRBridgeInteropHandle signalHandles[TR_BRIDGE_MAX_INTEROP_HANDLES];
    uint32_t signalHandleCount;
} TRBridgeInteropSyncState;

TESR_BRIDGE_API void TRBridge_Initialize(void);
TESR_BRIDGE_API void TRBridge_Shutdown(void);

TESR_BRIDGE_API void TRBridge_SetConfiguration(const TRBridgeConfiguration* configuration);
TESR_BRIDGE_API void TRBridge_GetConfiguration(TRBridgeConfiguration* outConfiguration);

TESR_BRIDGE_API void TRBridge_SetRenderTargets(const TRBridgeRenderTargetDescriptor* descriptors, size_t count);
TESR_BRIDGE_API size_t TRBridge_CopyRenderTargets(TRBridgeRenderTargetDescriptor* outDescriptors, size_t maxCount);

TESR_BRIDGE_API void TRBridge_RegisterLogCallback(TRBridgeLogCallback callback, void* userData);
TESR_BRIDGE_API void TRBridge_LogMessage(const char* message);

TESR_BRIDGE_API void TRBridge_SignalPluginFrame(const TRBridgePluginFrameInputs* inputs);
TESR_BRIDGE_API bool TRBridge_ConsumePendingFrame(TRBridgePluginFrameInputs* outInputs);
TESR_BRIDGE_API uint64_t TRBridge_PeekPendingFrameId(void);

TESR_BRIDGE_API void TRBridge_SetInteropSurfaces(const TRBridgeInteropSurface* surfaces, size_t count);
TESR_BRIDGE_API size_t TRBridge_CopyInteropSurfaces(TRBridgeInteropSurface* outSurfaces, size_t maxCount);
TESR_BRIDGE_API size_t TRBridge_GetInteropSurfaceCount(void);
TESR_BRIDGE_API bool TRBridge_GetInteropSurface(const char* name, TRBridgeInteropSurface* outSurface);
TESR_BRIDGE_API void TRBridge_UpdateInteropSurface(const TRBridgeInteropSurface* surface);

TESR_BRIDGE_API void TRBridge_SetInteropSyncState(const TRBridgeInteropSyncState* state);
TESR_BRIDGE_API bool TRBridge_GetInteropSyncState(TRBridgeInteropSyncState* outState);

TESR_BRIDGE_API void TRBridge_RegisterLayerCallbacks(const TRBridgeLayerCallbacks* callbacks);
TESR_BRIDGE_API void TRBridge_MarkLayerHeartbeat(void);
TESR_BRIDGE_API uint64_t TRBridge_GetLastLayerHeartbeat(void);

#ifdef __cplusplus
} // extern "C"
#endif

