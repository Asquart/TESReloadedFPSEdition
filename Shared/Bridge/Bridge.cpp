#include "Bridge.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace
{
struct SharedState
{
    std::mutex mutex;
    std::atomic<uint32_t> refCount{0};
    bool initialized{false};
    TRBridgeConfiguration configuration{};
    std::vector<TRBridgeRenderTargetDescriptor> renderTargets;
    std::vector<TRBridgeInteropSurface> interopSurfaces;
    TRBridgePluginFrameInputs pendingInputs{};
    bool hasPendingInputs{false};
    TRBridgeInteropSyncState syncState{};
    bool hasSyncState{false};
    TRBridgeLogCallback logCallback{nullptr};
    void* logUserData{nullptr};
    TRBridgeLayerCallbacks layerCallbacks{};
    uint64_t lastLayerHeartbeat{0};
};

SharedState& GetState()
{
    static SharedState state;
    return state;
}

uint64_t GetTimestampUS()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void EnsureInitializedUnlocked(SharedState& state)
{
    if (!state.initialized)
    {
        state.configuration.version = TR_BRIDGE_CONFIGURATION_VERSION;
        state.configuration.flags = 0;
        state.configuration.ambientOcclusionRadius = 1.0f;
        state.configuration.ambientOcclusionIntensity = 1.0f;
        state.configuration.ambientOcclusionPower = 1.0f;
        state.renderTargets.clear();
        state.interopSurfaces.clear();
        state.hasPendingInputs = false;
        state.pendingInputs.frameId = 0;
        state.pendingInputs.renderTargetCount = 0;
        state.hasSyncState = false;
        std::memset(&state.syncState, 0, sizeof(state.syncState));
        state.lastLayerHeartbeat = 0;
        state.initialized = true;
    }
}

void NotifyLayerFrame(SharedState& state, uint64_t frameId)
{
    if (state.layerCallbacks.onFrameReady)
    {
        state.layerCallbacks.onFrameReady(frameId, state.layerCallbacks.userData);
    }
}

} // namespace

extern "C"
{

TESR_BRIDGE_API void TRBridge_Initialize(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    state.refCount.fetch_add(1, std::memory_order_relaxed);
}

TESR_BRIDGE_API void TRBridge_Shutdown(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.refCount.load(std::memory_order_relaxed) == 0)
    {
        return;
    }

    uint32_t newCount = state.refCount.fetch_sub(1, std::memory_order_relaxed) - 1;
    if (newCount == 0)
    {
        state.initialized = false;
        state.renderTargets.clear();
        state.interopSurfaces.clear();
        state.hasPendingInputs = false;
        state.pendingInputs.frameId = 0;
        state.pendingInputs.renderTargetCount = 0;
        state.hasSyncState = false;
        std::memset(&state.syncState, 0, sizeof(state.syncState));
        state.logCallback = nullptr;
        state.logUserData = nullptr;
        state.layerCallbacks.onFrameReady = nullptr;
        state.layerCallbacks.userData = nullptr;
        state.lastLayerHeartbeat = 0;
    }
}

TESR_BRIDGE_API void TRBridge_SetConfiguration(const TRBridgeConfiguration* configuration)
{
    if (configuration == nullptr)
    {
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    state.configuration = *configuration;
}

TESR_BRIDGE_API void TRBridge_GetConfiguration(TRBridgeConfiguration* outConfiguration)
{
    if (outConfiguration == nullptr)
    {
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    *outConfiguration = state.configuration;
}

TESR_BRIDGE_API void TRBridge_SetRenderTargets(const TRBridgeRenderTargetDescriptor* descriptors, size_t count)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    state.renderTargets.clear();
    if (descriptors == nullptr || count == 0)
    {
        return;
    }

    state.renderTargets.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        TRBridgeRenderTargetDescriptor descriptor{};
        descriptor = descriptors[i];
        descriptor.name[sizeof(descriptor.name) - 1] = '\0';
        state.renderTargets.push_back(descriptor);
    }
}

TESR_BRIDGE_API size_t TRBridge_CopyRenderTargets(TRBridgeRenderTargetDescriptor* outDescriptors, size_t maxCount)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    size_t count = state.renderTargets.size();
    if (outDescriptors == nullptr || maxCount == 0)
    {
        return count;
    }

    size_t copyCount = std::min(count, maxCount);
    for (size_t i = 0; i < copyCount; ++i)
    {
        outDescriptors[i] = state.renderTargets[i];
    }
    return count;
}

TESR_BRIDGE_API void TRBridge_RegisterLogCallback(TRBridgeLogCallback callback, void* userData)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    state.logCallback = callback;
    state.logUserData = userData;
}

TESR_BRIDGE_API void TRBridge_LogMessage(const char* message)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    if (state.logCallback)
    {
        state.logCallback(message != nullptr ? message : "", state.logUserData);
    }
    else if (message != nullptr)
    {
        std::fprintf(stderr, "%s\n", message);
    }
}

TESR_BRIDGE_API void TRBridge_SignalPluginFrame(const TRBridgePluginFrameInputs* inputs)
{
    if (inputs == nullptr)
    {
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    state.pendingInputs = *inputs;
    state.hasPendingInputs = true;
    NotifyLayerFrame(state, inputs->frameId);
}

TESR_BRIDGE_API bool TRBridge_ConsumePendingFrame(TRBridgePluginFrameInputs* outInputs)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    if (!state.hasPendingInputs)
    {
        return false;
    }

    if (outInputs != nullptr)
    {
        *outInputs = state.pendingInputs;
    }

    state.hasPendingInputs = false;
    state.pendingInputs.frameId = 0;
    state.pendingInputs.renderTargetCount = 0;
    return true;
}

TESR_BRIDGE_API uint64_t TRBridge_PeekPendingFrameId(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    return state.hasPendingInputs ? state.pendingInputs.frameId : 0u;
}

TESR_BRIDGE_API void TRBridge_SetInteropSurfaces(const TRBridgeInteropSurface* surfaces, size_t count)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    state.interopSurfaces.clear();
    if (surfaces == nullptr || count == 0)
    {
        return;
    }

    state.interopSurfaces.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        TRBridgeInteropSurface surface = surfaces[i];
        surface.descriptor.name[sizeof(surface.descriptor.name) - 1] = '\0';
        if (surface.handleCount > TR_BRIDGE_MAX_INTEROP_HANDLES)
        {
            surface.handleCount = TR_BRIDGE_MAX_INTEROP_HANDLES;
        }
        state.interopSurfaces.push_back(surface);
    }
}

TESR_BRIDGE_API size_t TRBridge_CopyInteropSurfaces(TRBridgeInteropSurface* outSurfaces, size_t maxCount)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    size_t count = state.interopSurfaces.size();
    if (outSurfaces == nullptr || maxCount == 0)
    {
        return count;
    }

    size_t copyCount = std::min(count, maxCount);
    for (size_t i = 0; i < copyCount; ++i)
    {
        outSurfaces[i] = state.interopSurfaces[i];
    }
    return count;
}

TESR_BRIDGE_API size_t TRBridge_GetInteropSurfaceCount(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    return state.interopSurfaces.size();
}

TESR_BRIDGE_API bool TRBridge_GetInteropSurface(const char* name, TRBridgeInteropSurface* outSurface)
{
    if (name == nullptr || outSurface == nullptr)
    {
        return false;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    for (const auto& surface : state.interopSurfaces)
    {
        if (std::strncmp(surface.descriptor.name, name, sizeof(surface.descriptor.name)) == 0)
        {
            *outSurface = surface;
            return true;
        }
    }

    return false;
}

TESR_BRIDGE_API void TRBridge_UpdateInteropSurface(const TRBridgeInteropSurface* surface)
{
    if (surface == nullptr)
    {
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    TRBridgeInteropSurface copy = *surface;
    copy.descriptor.name[sizeof(copy.descriptor.name) - 1] = '\0';
    if (copy.handleCount > TR_BRIDGE_MAX_INTEROP_HANDLES)
    {
        copy.handleCount = TR_BRIDGE_MAX_INTEROP_HANDLES;
    }

    for (auto& existing : state.interopSurfaces)
    {
        if (std::strncmp(existing.descriptor.name, copy.descriptor.name, sizeof(copy.descriptor.name)) == 0)
        {
            existing = copy;
            return;
        }
    }

    state.interopSurfaces.push_back(copy);
}

TESR_BRIDGE_API void TRBridge_SetInteropSyncState(const TRBridgeInteropSyncState* stateParam)
{
    if (stateParam == nullptr)
    {
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    state.syncState = *stateParam;
    if (state.syncState.waitHandleCount > TR_BRIDGE_MAX_INTEROP_HANDLES)
    {
        state.syncState.waitHandleCount = TR_BRIDGE_MAX_INTEROP_HANDLES;
    }
    if (state.syncState.signalHandleCount > TR_BRIDGE_MAX_INTEROP_HANDLES)
    {
        state.syncState.signalHandleCount = TR_BRIDGE_MAX_INTEROP_HANDLES;
    }
    state.hasSyncState = true;
}

TESR_BRIDGE_API bool TRBridge_GetInteropSyncState(TRBridgeInteropSyncState* outState)
{
    if (outState == nullptr)
    {
        return false;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    if (!state.hasSyncState)
    {
        return false;
    }

    *outState = state.syncState;
    return true;
}

TESR_BRIDGE_API void TRBridge_RegisterLayerCallbacks(const TRBridgeLayerCallbacks* callbacks)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);

    if (callbacks != nullptr)
    {
        state.layerCallbacks = *callbacks;
    }
    else
    {
        state.layerCallbacks.onFrameReady = nullptr;
        state.layerCallbacks.userData = nullptr;
    }
}

TESR_BRIDGE_API void TRBridge_MarkLayerHeartbeat(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    state.lastLayerHeartbeat = GetTimestampUS();
}

TESR_BRIDGE_API uint64_t TRBridge_GetLastLayerHeartbeat(void)
{
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    EnsureInitializedUnlocked(state);
    return state.lastLayerHeartbeat;
}

} // extern "C"
