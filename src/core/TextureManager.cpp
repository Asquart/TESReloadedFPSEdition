#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "TextureManager.h"
#include "RenderManager.h"
#include "ShaderManager.h"

#include "../../Shared/Bridge/Bridge.h"
#include "../../Shared/Bridge/DXVKInterop.h"

namespace
{
TRBridgeFormat ConvertFormat(D3DFORMAT format)
{
        switch (format)
        {
        case D3DFMT_A8R8G8B8:
                return TR_BRIDGE_FORMAT_R8G8B8A8_UNORM;
        case D3DFMT_A16B16G16R16F:
                return TR_BRIDGE_FORMAT_R16G16B16A16_FLOAT;
        case D3DFMT_G32R32F:
                return TR_BRIDGE_FORMAT_R32G32_SFLOAT;
        case D3DFMT_R32F:
                return TR_BRIDGE_FORMAT_R32_SFLOAT;
        case D3DFMT_A32B32G32R32F:
                return TR_BRIDGE_FORMAT_R32G32B32A32_FLOAT;
        case D3DFMT_D24S8:
                return TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT;
        default:
                if (format == static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z')))
                {
                        return TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT;
                }
                break;
        }
        return TR_BRIDGE_FORMAT_UNDEFINED;
}

void AppendHandle(TRBridgeInteropSurface& surface, TRBridgeInteropHandleType type, uint64_t value, uint64_t auxValue = 0)
{
        if (surface.handleCount >= TR_BRIDGE_MAX_INTEROP_HANDLES || value == 0)
        {
                return;
        }

        surface.handles[surface.handleCount].type = type;
        surface.handles[surface.handleCount].value = value;
        surface.handles[surface.handleCount].auxValue = auxValue;
        ++surface.handleCount;
}

struct VulkanInteropEntry
{
        bool valid{false};
        DxvkInteropSurfaceInfo info{};
};

std::unordered_map<std::string, VulkanInteropEntry> g_vulkanInteropSurfaces;
bool g_vulkanInteropAvailable = false;
bool g_vulkanInteropValidation = false;

struct BridgeRenderTarget
{
        IDirect3DTexture9** texturePtr{nullptr};
        IDirect3DSurface9** surfacePtr{nullptr};
        uint32_t usageFlags{0};
};

std::unordered_map<std::string, BridgeRenderTarget> g_bridgeRenderTargets;
std::mutex g_bridgeRenderTargetsMutex;

bool EnvironmentFlagEnabled(const char* name)
{
        const char* value = std::getenv(name);
        if (!value)
        {
                return false;
        }

        return value[0] != '\0' && value[0] != '0' && value[0] != 'f' && value[0] != 'F' && value[0] != 'n' && value[0] != 'N';
}

void StoreInteropInfo(const char* name, const DxvkInteropSurfaceInfo& info)
{
        if (!name || !info.valid)
        {
                return;
        }

        VulkanInteropEntry entry{};
        entry.valid = true;
        entry.info = info;
        g_vulkanInteropSurfaces[name] = entry;
}

const VulkanInteropEntry* FindInteropInfo(const char* name)
{
        if (!name)
        {
                return nullptr;
        }

        auto it = g_vulkanInteropSurfaces.find(name);
        if (it == g_vulkanInteropSurfaces.end() || !it->second.valid)
        {
                return nullptr;
        }

        return &it->second;
}

void RegisterTextureInterop(IDirect3DDevice9* device, const char* name, IDirect3DTexture9* texture, uint32_t usageFlags)
{
        if (!g_vulkanInteropAvailable || !device || !name || !texture)
        {
                return;
        }

        DxvkInteropSurfaceInfo info{};
        if (!DxvkInterop_GetSurfaceInfo(texture, &info) || !info.valid)
        {
                D3DSURFACE_DESC desc;
                if (FAILED(texture->GetLevelDesc(0, &desc)))
                {
                        return;
                }

                DxvkInteropSurfaceCreateInfo createInfo{};
                createInfo.width = desc.Width;
                createInfo.height = desc.Height;
                createInfo.mipLevels = texture->GetLevelCount();
                createInfo.arrayLayers = 1;
                createInfo.format = static_cast<uint32_t>(desc.Format);
                createInfo.usageFlags = usageFlags;
                createInfo.sampleCount = 1;

                if (!DxvkInterop_CreateSiblingSurface(device, &createInfo, &info) || !info.valid)
                {
                        return;
                }
        }

        if (info.rowPitch == 0)
        {
                D3DLOCKED_RECT lockedRect;
                if (SUCCEEDED(texture->LockRect(0, &lockedRect, NULL, D3DLOCK_READONLY)))
                {
                        info.rowPitch = static_cast<uint32_t>(lockedRect.Pitch);
                        texture->UnlockRect(0);
                }
        }

        StoreInteropInfo(name, info);
}

void ValidateInteropSurface(const char* name, IDirect3DSurface9* surface, const DxvkInteropSurfaceInfo& info)
{
        if (!g_vulkanInteropValidation || !surface || !info.valid)
        {
                return;
        }

        if (DxvkInterop_DebugDownload(surface, &info))
        {
                Logger::Log("[TESR][Interop] Validated Vulkan surface %s via CPU readback", name);
        }
        else
        {
                Logger::Log("[TESR][Interop] Failed to validate Vulkan surface %s via CPU readback", name);
        }
}
}

void TextureManager::RegisterBridgeRenderTarget(const char* Name,
        IDirect3DTexture9** Texture,
        IDirect3DSurface9** Surface,
        uint32_t usageFlags)
{
        if (!Name)
                return;

        std::lock_guard<std::mutex> lock(g_bridgeRenderTargetsMutex);
        BridgeRenderTarget entry{};
        entry.texturePtr = Texture;
        entry.surfacePtr = Surface;
        entry.usageFlags = usageFlags;
        g_bridgeRenderTargets[Name] = entry;
}

void TextureManager::Initialize() {

        Logger::Log("Starting the textures manager...");
        auto timer = TimeLogger();

        TheTextureManager = new TextureManager();

        IDirect3DDevice9* Device = TheRenderManager->device;
        UInt32 Width = TheRenderManager->width;
        UInt32 Height = TheRenderManager->height;

        TRBridge_Initialize();

        g_vulkanInteropSurfaces.clear();
        g_vulkanInteropAvailable = false;
        g_vulkanInteropValidation = false;
        {
                std::lock_guard<std::mutex> lock(g_bridgeRenderTargetsMutex);
                g_bridgeRenderTargets.clear();
        }

        if (DxvkInterop_Initialize(Device) && DxvkInterop_IsAvailable())
        {
                Logger::Log("[TESR][Interop] Vulkan interop enabled");
        }
        else
        {
                Logger::Log("[TESR][Interop] Vulkan interop requested but unavailable");
                if (!DxvkInterop_Initialize(Device))
                {
                    Logger::Log("[TESR][Interop] DxvkInterop_Initialize failed");
                }
                if (!DxvkInterop_IsAvailable())
                {
                    Logger::Log("[TESR][Interop] DxvkInterop_IsAvailable failed");
                }
        }

        // create textures used by NVR and bind them to surfaces
        TheTextureManager->InitTexture("TESR_SourceBuffer", &TheTextureManager->SourceTexture, &TheTextureManager->SourceSurface, Width, Height, D3DFMT_A16B16G16R16F);
        TheTextureManager->InitTexture("TESR_RenderedBuffer", &TheTextureManager->RenderedTexture, &TheTextureManager->RenderedSurface, Width, Height, D3DFMT_A16B16G16R16F);

        RegisterBridgeRenderTarget("TESR_SourceTexture",
                &TheTextureManager->SourceTexture,
                &TheTextureManager->SourceSurface,
                TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
        RegisterBridgeRenderTarget("TESR_RenderedTexture",
                &TheTextureManager->RenderedTexture,
                &TheTextureManager->RenderedSurface,
                TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);

        Device->CreateTexture(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL, (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &TheTextureManager->DepthTexture, NULL);
        TheTextureManager->RegisterTexture("TESR_DepthBufferWorld",(IDirect3DBaseTexture9**)&TheTextureManager->DepthTexture);
        if (TheTextureManager->DepthTexture)
                TheTextureManager->DepthTexture->GetSurfaceLevel(0, &TheTextureManager->DepthSurface);
        RegisterBridgeRenderTarget("TESR_DepthTexture",
                &TheTextureManager->DepthTexture,
                &TheTextureManager->DepthSurface,
                TR_BRIDGE_RT_USAGE_DEPTH_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
        RegisterBridgeRenderTarget("TESR_MainDepthStencil",
                nullptr,
                &TheTextureManager->DepthSurface,
                TR_BRIDGE_RT_USAGE_DEPTH_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
        Device->CreateTexture(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL, (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &TheTextureManager->DepthTextureViewModel, NULL);
        TheTextureManager->RegisterTexture("TESR_DepthBufferViewModel",(IDirect3DBaseTexture9**)&TheTextureManager->DepthTextureViewModel);

        TheTextureManager->RegisterTexture(WordWaterHeightMapBuffer, &TheTextureManager->WaterHeightMapB);
        TheTextureManager->RegisterTexture(WordWaterReflectionMapBuffer, &TheTextureManager->WaterReflectionMapB);

        if (g_vulkanInteropAvailable)
        {
                RegisterTextureInterop(Device, "TESR_SourceTexture", TheTextureManager->SourceTexture, TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
                RegisterTextureInterop(Device, "TESR_RenderedTexture", TheTextureManager->RenderedTexture, TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
                RegisterTextureInterop(Device, "TESR_DepthTexture", TheTextureManager->DepthTexture, TR_BRIDGE_RT_USAGE_DEPTH_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);

                if (const VulkanInteropEntry* depthEntry = FindInteropInfo("TESR_DepthTexture"))
                {
                        StoreInteropInfo("TESR_MainDepthStencil", depthEntry->info);
                }

                if (const VulkanInteropEntry* sourceEntry = FindInteropInfo("TESR_SourceTexture"))
                {
                        ValidateInteropSurface("TESR_SourceTexture", TheTextureManager->SourceSurface, sourceEntry->info);
                }
                if (const VulkanInteropEntry* renderedEntry = FindInteropInfo("TESR_RenderedTexture"))
                {
                        ValidateInteropSurface("TESR_RenderedTexture", TheTextureManager->RenderedSurface, renderedEntry->info);
                }
                if (TheTextureManager->DepthSurface)
                {
                        if (const VulkanInteropEntry* depthEntry = FindInteropInfo("TESR_DepthTexture"))
                        {
                                ValidateInteropSurface("TESR_DepthTexture", TheTextureManager->DepthSurface, depthEntry->info);
                        }
                }
        }

        timer.LogTime("TextureManager::Initialize");

        PublishBridgeState();
}

void TextureManager::PublishBridgeState() {

        if (!TheTextureManager)
                return;

        std::vector<TRBridgeRenderTargetDescriptor> descriptors;
        std::vector<TRBridgeInteropSurface> interops;

        IDirect3DDevice9* device = TheRenderManager ? TheRenderManager->device : nullptr;

        std::vector<std::string> names;
        names.reserve(g_bridgeRenderTargets.size());
        {
                std::lock_guard<std::mutex> lock(g_bridgeRenderTargetsMutex);
                for (const auto& entry : g_bridgeRenderTargets)
                        names.push_back(entry.first);
        }

        std::sort(names.begin(), names.end());

        for (const auto& name : names)
        {
                BridgeRenderTarget entry;
                {
                        std::lock_guard<std::mutex> lock(g_bridgeRenderTargetsMutex);
                        auto it = g_bridgeRenderTargets.find(name);
                        if (it == g_bridgeRenderTargets.end())
                                continue;
                        entry = it->second;
                }

                IDirect3DTexture9* texture = entry.texturePtr ? *entry.texturePtr : nullptr;
                IDirect3DSurface9* surface = entry.surfacePtr ? *entry.surfacePtr : nullptr;

                D3DSURFACE_DESC desc{};
                bool hasDescriptor = false;
                if (texture && SUCCEEDED(texture->GetLevelDesc(0, &desc)))
                        hasDescriptor = true;
                else if (surface && SUCCEEDED(surface->GetDesc(&desc)))
                        hasDescriptor = true;

                if (!hasDescriptor)
                        continue;

                TRBridgeRenderTargetDescriptor descriptor{};
                std::strncpy(descriptor.name, name.c_str(), sizeof(descriptor.name) - 1);
                descriptor.name[sizeof(descriptor.name) - 1] = ' ';
                descriptor.width = desc.Width;
                descriptor.height = desc.Height;
                descriptor.mipLevels = texture ? texture->GetLevelCount() : 1;
                descriptor.arrayLayers = 1;
                descriptor.format = ConvertFormat(desc.Format);
                if (descriptor.format == TR_BRIDGE_FORMAT_UNDEFINED && (entry.usageFlags & TR_BRIDGE_RT_USAGE_DEPTH_BIT))
                        descriptor.format = TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT;
                descriptor.usageFlags = entry.usageFlags;

                TRBridgeInteropSurface interopSurface{};
                interopSurface.descriptor = descriptor;
                interopSurface.handleCount = 0;

                if (texture)
                        AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_D3D9_TEXTURE_POINTER, reinterpret_cast<uint64_t>(texture));
                if (surface)
                        AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_D3D9_SURFACE_POINTER, reinterpret_cast<uint64_t>(surface));

                if (g_vulkanInteropAvailable && texture && !FindInteropInfo(name.c_str()) && device)
                        RegisterTextureInterop(device, name.c_str(), texture, entry.usageFlags);

                if (const VulkanInteropEntry* interopEntry = FindInteropInfo(name.c_str()))
                {
                        const DxvkInteropSurfaceInfo& info = interopEntry->info;
                        if (info.image)
                                AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE, info.image);
                        if (info.imageView)
                                AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW, info.imageView);
                        if (info.memory)
                                AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_VK_DEVICE_MEMORY, info.memory);
                        if (info.externalMemory.win32Handle)
                                AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_WIN32_SHARED_HANDLE, info.externalMemory.win32Handle);
                        if (info.externalMemory.opaqueFd >= 0)
                                AppendHandle(interopSurface, TR_BRIDGE_INTEROP_HANDLE_OPAQUE_FD, static_cast<uint64_t>(static_cast<int64_t>(info.externalMemory.opaqueFd)));
                        if (info.rowPitch)
                                interopSurface.rowPitch = info.rowPitch;
                        if (info.depthPitch)
                                interopSurface.depthPitch = info.depthPitch;
                }

                descriptors.push_back(descriptor);
                interops.push_back(interopSurface);
        }

        TRBridge_SetRenderTargets(descriptors.empty() ? nullptr : descriptors.data(), descriptors.size());
        TRBridge_SetInteropSurfaces(interops.empty() ? nullptr : interops.data(), interops.size());
}


/*
* Creates a texture of the given size and format and binds a surface to it, so it can be used as render target.
*/
void TextureManager::InitTexture(const char* Name, IDirect3DTexture9** Texture, IDirect3DSurface9** Surface, int Width, int Height, D3DFORMAT Format, bool mipmaps) {
	IDirect3DDevice9* Device = TheRenderManager->device;
	// create a texture to receive the surface contents
	HRESULT create;
	if (!mipmaps)
		create = Device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, Texture, NULL);
	else
		create = Device->CreateTexture(Width, Height, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, Format, D3DPOOL_DEFAULT, Texture, NULL);

	if (FAILED(create)) {
		Logger::Log("[ERROR] : Failed to init texture %s", Name);
		return;
	}

	// set the surface level to the texture.
	(*Texture)->GetSurfaceLevel(0, Surface);
	RegisterTexture(Name, (IDirect3DBaseTexture9**)Texture);
}


/*
* Adds a texture to the list of sampler names recognized in shaders
*/
void TextureManager::RegisterTexture(const char* Name, IDirect3DBaseTexture9** Texture) {
	Logger::Log("Registering Texture %s", Name);
	TextureNames[Name] = Texture;
}

/*
* Gets a texture from the cache based on texture path
*/
IDirect3DBaseTexture9* TextureManager::GetCachedTexture(std::string& pathS) {
	TextureList::iterator t = TextureCache.find(pathS);
	if (t == TextureCache.end()) return nullptr;
	return t->second;
}


/*
* Gets a game dynamic texture by the sampler name
*/
IDirect3DBaseTexture9* TextureManager::GetTextureByName(std::string& Name) {
	TexturePointersList::iterator t = TextureNames.find(Name);
	if (t == TextureNames.end()) {
		Logger::Log("[ERROR] Texture %s not found.", Name.c_str());
		return nullptr;
	}
	return *(t->second);
}


/*
* Loads the actual texture file or get it from cache based on type/Name
*/
IDirect3DBaseTexture9* TextureManager::GetFileTexture(std::string TexturePath, TextureRecord::TextureRecordType Type) {

	IDirect3DBaseTexture9* Texture = GetCachedTexture(TexturePath);
	if (Texture) return Texture;

	switch (Type) {
	case TextureRecord::TextureRecordType::PlanarBuffer:
		D3DXCreateTextureFromFileA(TheRenderManager->device, TexturePath.data(), (IDirect3DTexture9**)&Texture);
		break;
	case TextureRecord::TextureRecordType::VolumeBuffer:
		D3DXCreateVolumeTextureFromFileA(TheRenderManager->device, TexturePath.data(), (IDirect3DVolumeTexture9**)&Texture);
		break;
	case TextureRecord::TextureRecordType::CubeBuffer:
		D3DXCreateCubeTextureFromFileA(TheRenderManager->device, TexturePath.data(), (IDirect3DCubeTexture9**)&Texture);
		break;
	default:
		Logger::Log("[ERROR] : Invalid texture type %i for %s", Type, TexturePath);
	}

	if (!Texture) Logger::Log("[ERROR] : Couldn't load texture file %s", TexturePath);
	else Logger::Log("Loaded texture file %s", TexturePath);

	// add texture to cache
	TextureCache[TexturePath] = Texture;
	return Texture;
}


void TextureManager::SetWaterHeightMap(IDirect3DBaseTexture9* WaterHeightMap) {
    if (WaterHeightMapB == WaterHeightMap) return;
    WaterHeightMapB = WaterHeightMap;  //This may cause crashes on certain conditions
//    Logger::Log("Binding %0X", WaterHeightMap);
	for (WaterMapList::iterator it = TheTextureManager->WaterHeightMapTextures.begin(); it != TheTextureManager->WaterHeightMapTextures.end(); it++){
		 (*it)->Texture = WaterHeightMap;
	}
}


void TextureManager::SetWaterReflectionMap(IDirect3DBaseTexture9* WaterReflectionMap) {
    if (WaterReflectionMapB == WaterReflectionMap) return;
    WaterReflectionMapB = WaterReflectionMap;
//    Logger::Log("Binding %0X", WaterReflectionMap);
	for (WaterMapList::iterator it = TheTextureManager->WaterReflectionMapTextures.begin(); it != TheTextureManager->WaterReflectionMapTextures.end(); it++){
		 (*it)->Texture = WaterReflectionMap;
	}
}



/*
* Debug function to save the texture from the sampler into a "Test" folder
*/
void TextureManager::DumpToFile (IDirect3DTexture9* Texture, const char*  Name) {
	IDirect3DSurface9* Surface = nullptr;
	Texture->GetSurfaceLevel(0, &Surface);
	char path[32] = ".\\Test\\";
	strcat(path, Name);
	strcat(path, ".jpg");
	if (Surface) {
		D3DXSaveSurfaceToFileA(path, D3DXIFF_JPG, Surface, NULL, NULL);
		Surface->Release();
		Surface = nullptr;
	}
	else {
		Logger::Log("Surface is null for %s", Name);
	}
}