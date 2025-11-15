#include "DXVKInterop.h"

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
struct DxvkInteropDispatch
{
    bool initialized{false};
    bool available{false};

    typedef bool (*PFN_dxvk_interop_initialize)(IDirect3DDevice9* device);
    typedef bool (*PFN_dxvk_interop_is_available)();
    typedef bool (*PFN_dxvk_interop_get_surface_info)(IDirect3DBaseTexture9* texture, DxvkInteropSurfaceInfo* outInfo);
    typedef bool (*PFN_dxvk_interop_create_sibling_surface)(IDirect3DDevice9* device, const DxvkInteropSurfaceCreateInfo* createInfo, DxvkInteropSurfaceInfo* outInfo);
    typedef bool (*PFN_dxvk_interop_get_frame_sync)(IDirect3DDevice9* device, DxvkInteropFrameSync* outSync);
    typedef bool (*PFN_dxvk_interop_debug_upload)(IDirect3DSurface9* sourceSurface, const DxvkInteropSurfaceInfo* destinationInfo);
    typedef bool (*PFN_dxvk_interop_debug_download)(IDirect3DSurface9* destinationSurface, const DxvkInteropSurfaceInfo* sourceInfo);

    PFN_dxvk_interop_initialize initialize{nullptr};
    PFN_dxvk_interop_is_available isAvailable{nullptr};
    PFN_dxvk_interop_get_surface_info getSurfaceInfo{nullptr};
    PFN_dxvk_interop_create_sibling_surface createSiblingSurface{nullptr};
    PFN_dxvk_interop_get_frame_sync getFrameSync{nullptr};
    PFN_dxvk_interop_debug_upload debugUpload{nullptr};
    PFN_dxvk_interop_debug_download debugDownload{nullptr};

#ifdef _WIN32
    HMODULE module{nullptr};
#else
    void* module{nullptr};
#endif
};

DxvkInteropDispatch& GetDispatch()
{
    static DxvkInteropDispatch dispatch;
    return dispatch;
}

#ifdef _WIN32
HMODULE LoadInteropModule()
{
    const wchar_t* moduleNames[] = {
        L"dxvk_interop.dll",
        L"TESReloaded_dxvk_interop.dll",
        nullptr,
    };

    for (const wchar_t* name : moduleNames)
    {
        if (!name)
        {
            break;
        }

        HMODULE module = LoadLibraryW(name);
        if (module)
        {
            return module;
        }
    }

    return nullptr;
}
#else
void* LoadInteropModule()
{
    const char* moduleNames[] = {
        "libdxvk_interop.so",
        "libtesreloaded_dxvk_interop.so",
        nullptr,
    };

    for (const char* name : moduleNames)
    {
        if (!name)
        {
            break;
        }

        void* module = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
        if (module)
        {
            return module;
        }
    }

    return nullptr;
}
#endif

void ResolveDispatch(DxvkInteropDispatch& dispatch)
{
    if (dispatch.initialized)
    {
        return;
    }
    dispatch.initialized = true;

#ifdef _WIN32
    dispatch.module = LoadInteropModule();
    if (!dispatch.module)
    {
        return;
    }

    dispatch.initialize = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_initialize>(GetProcAddress(dispatch.module, "dxvk_interop_initialize"));
    dispatch.isAvailable = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_is_available>(GetProcAddress(dispatch.module, "dxvk_interop_is_available"));
    dispatch.getSurfaceInfo = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_get_surface_info>(GetProcAddress(dispatch.module, "dxvk_interop_get_surface_info"));
    dispatch.createSiblingSurface = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_create_sibling_surface>(GetProcAddress(dispatch.module, "dxvk_interop_create_sibling_surface"));
    dispatch.getFrameSync = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_get_frame_sync>(GetProcAddress(dispatch.module, "dxvk_interop_get_frame_sync"));
    dispatch.debugUpload = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_debug_upload>(GetProcAddress(dispatch.module, "dxvk_interop_debug_upload"));
    dispatch.debugDownload = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_debug_download>(GetProcAddress(dispatch.module, "dxvk_interop_debug_download"));
#else
    dispatch.module = LoadInteropModule();
    if (!dispatch.module)
    {
        return;
    }

    dispatch.initialize = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_initialize>(dlsym(dispatch.module, "dxvk_interop_initialize"));
    dispatch.isAvailable = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_is_available>(dlsym(dispatch.module, "dxvk_interop_is_available"));
    dispatch.getSurfaceInfo = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_get_surface_info>(dlsym(dispatch.module, "dxvk_interop_get_surface_info"));
    dispatch.createSiblingSurface = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_create_sibling_surface>(dlsym(dispatch.module, "dxvk_interop_create_sibling_surface"));
    dispatch.getFrameSync = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_get_frame_sync>(dlsym(dispatch.module, "dxvk_interop_get_frame_sync"));
    dispatch.debugUpload = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_debug_upload>(dlsym(dispatch.module, "dxvk_interop_debug_upload"));
    dispatch.debugDownload = reinterpret_cast<DxvkInteropDispatch::PFN_dxvk_interop_debug_download>(dlsym(dispatch.module, "dxvk_interop_debug_download"));
#endif

    if (!dispatch.isAvailable || !dispatch.getSurfaceInfo)
    {
        return;
    }

    dispatch.available = true;
}

} // namespace

bool DxvkInterop_Initialize(IDirect3DDevice9* device)
{
    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available)
    {
        return false;
    }

    if (dispatch.initialize)
    {
        return dispatch.initialize(device);
    }

    return true;
}

bool DxvkInterop_IsAvailable()
{
    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available)
    {
        return false;
    }

    if (dispatch.isAvailable)
    {
        return dispatch.isAvailable();
    }

    return true;
}

bool DxvkInterop_GetSurfaceInfo(IDirect3DBaseTexture9* texture, DxvkInteropSurfaceInfo* outInfo)
{
    if (outInfo)
    {
        std::memset(outInfo, 0, sizeof(*outInfo));
        outInfo->externalMemory.opaqueFd = -1;
    }

    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available || !dispatch.getSurfaceInfo || !texture)
    {
        return false;
    }

    return dispatch.getSurfaceInfo(texture, outInfo);
}

bool DxvkInterop_CreateSiblingSurface(IDirect3DDevice9* device, const DxvkInteropSurfaceCreateInfo* createInfo, DxvkInteropSurfaceInfo* outInfo)
{
    if (outInfo)
    {
        std::memset(outInfo, 0, sizeof(*outInfo));
        outInfo->externalMemory.opaqueFd = -1;
    }

    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available || !dispatch.createSiblingSurface || !device || !createInfo)
    {
        return false;
    }

    return dispatch.createSiblingSurface(device, createInfo, outInfo);
}

bool DxvkInterop_GetFrameSync(IDirect3DDevice9* device, DxvkInteropFrameSync* outSync)
{
    if (outSync)
    {
        std::memset(outSync, 0, sizeof(*outSync));
    }

    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available || !dispatch.getFrameSync || !device)
    {
        return false;
    }

    return dispatch.getFrameSync(device, outSync);
}

bool DxvkInterop_DebugUpload(IDirect3DSurface9* sourceSurface, const DxvkInteropSurfaceInfo* destinationInfo)
{
    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available || !dispatch.debugUpload || !sourceSurface || !destinationInfo)
    {
        return false;
    }

    return dispatch.debugUpload(sourceSurface, destinationInfo);
}

bool DxvkInterop_DebugDownload(IDirect3DSurface9* destinationSurface, const DxvkInteropSurfaceInfo* sourceInfo)
{
    auto& dispatch = GetDispatch();
    ResolveDispatch(dispatch);
    if (!dispatch.available || !dispatch.debugDownload || !destinationSurface || !sourceInfo)
    {
        return false;
    }

    return dispatch.debugDownload(destinationSurface, sourceInfo);
}

