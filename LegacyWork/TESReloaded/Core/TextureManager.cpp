#include <string>
#include <regex>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <cstdlib>

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

void AppendHandle(TRBridgeInteropSurface& surface, TRBridgeInteropHandleType type, uint64_t value)
{
if (surface.handleCount >= TR_BRIDGE_MAX_INTEROP_HANDLES || value == 0)
{
return;
}

surface.handles[surface.handleCount].type = type;
surface.handles[surface.handleCount].value = value;
surface.handles[surface.handleCount].auxValue = 0;
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

#define WordWaterHeightMapBuffer "TESR_WaterHeightMapBuffer"
#define WordWaterReflectionMapBuffer "TESR_WaterReflectionMapBuffer"

TextureRecord::TextureRecord() {

	Texture = NULL;
	SamplerStates[0] = 0; //This isn't used. Just to simplify  the matching between index and meaning
	SamplerStates[D3DSAMP_ADDRESSU] = D3DTADDRESS_WRAP;
	SamplerStates[D3DSAMP_ADDRESSV] = D3DTADDRESS_WRAP;
	SamplerStates[D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
	SamplerStates[D3DSAMP_BORDERCOLOR] = 0;
	SamplerStates[D3DSAMP_MAGFILTER] = D3DTEXF_POINT;
	SamplerStates[D3DSAMP_MINFILTER] = D3DTEXF_POINT;
	SamplerStates[D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
	SamplerStates[D3DSAMP_MIPMAPLODBIAS] = 0;
	SamplerStates[D3DSAMP_MAXMIPLEVEL] = 0;
	SamplerStates[D3DSAMP_MAXANISOTROPY] = 1;
	SamplerStates[D3DSAMP_SRGBTEXTURE] = 0;

}

bool TextureRecord::LoadTexture(TextureRecordType Type, const char* Name) {

	IDirect3DTexture9* Tex = NULL;
	IDirect3DVolumeTexture9* TexV = NULL;
	IDirect3DCubeTexture9* TexC = NULL;

	switch (Type) {
		case PlanarBuffer:
			D3DXCreateTextureFromFileA(TheRenderManager->device, Name, &Tex);
			if (Tex == NULL) return false;
			Texture = Tex;
			break;
		case VolumeBuffer:
			D3DXCreateVolumeTextureFromFileA(TheRenderManager->device, Name, &TexV);
			if (TexV == NULL) return false;
			Texture = TexV;
			break;
		case CubeBuffer:
			D3DXCreateCubeTextureFromFileA(TheRenderManager->device, Name, &TexC);
			if (TexC == NULL) return false;
			Texture = TexC;
			break;
		case SourceBuffer:
			Texture = TheTextureManager->SourceTexture;
			break;
		case RenderedBuffer:
			Texture = TheTextureManager->RenderedTexture;
			break;
		case DepthBuffer:
			Texture = TheTextureManager->DepthTexture;
			break;
		case ShadowMapBufferNear:
			Texture = TheTextureManager->ShadowMapTextureBlurred[ShadowManager::ShadowMapTypeEnum::MapNear];
			break;
		case ShadowMapBufferMiddle:
			Texture = TheTextureManager->ShadowMapTextureBlurred[ShadowManager::ShadowMapTypeEnum::MapMiddle];
			break;
		case ShadowMapBufferFar:
			Texture = TheTextureManager->ShadowMapTextureBlurred[ShadowManager::ShadowMapTypeEnum::MapFar];
			break;
		case ShadowMapBufferLod:
			Texture = TheTextureManager->ShadowMapTextureBlurred[ShadowManager::ShadowMapTypeEnum::MapLod];
			break;
		case OrthoMapBuffer:
			Texture = TheTextureManager->ShadowMapTexture[ShadowManager::ShadowMapTypeEnum::MapOrtho];
			break;
		case ShadowCubeMapBuffer0:
			Texture = TheTextureManager->ShadowCubeMapTexture[0];
			break;
		case ShadowCubeMapBuffer1:
			Texture = TheTextureManager->ShadowCubeMapTexture[1];
			break;
		case ShadowCubeMapBuffer2:
			Texture = TheTextureManager->ShadowCubeMapTexture[2];
			break;
		case ShadowCubeMapBuffer3:
			Texture = TheTextureManager->ShadowCubeMapTexture[3];
			break;
        default:
            return false; //Texture is invalid or not assigned here.
	}
	return true;

}

void TextureManager::Initialize() {

Logger::Log("Starting the textures manager...");
TheTextureManager = new TextureManager();

IDirect3DDevice9* Device = TheRenderManager->device;

TRBridge_Initialize();

g_vulkanInteropSurfaces.clear();
g_vulkanInteropAvailable = false;
g_vulkanInteropValidation = false;

if (EnvironmentFlagEnabled("TESR_ENABLE_VK_INTEROP"))
{
if (DxvkInterop_Initialize(Device) && DxvkInterop_IsAvailable())
{
g_vulkanInteropAvailable = true;
g_vulkanInteropValidation = EnvironmentFlagEnabled("TESR_VALIDATE_VK_INTEROP");
Logger::Log("[TESR][Interop] Vulkan interop enabled%s", g_vulkanInteropValidation ? " with validation" : "");
}
else
{
Logger::Log("[TESR][Interop] Vulkan interop requested but unavailable");
}
}
else
{
Logger::Log("[TESR][Interop] Vulkan interop disabled (TESR_ENABLE_VK_INTEROP not set)");
}
UInt32 Width = TheRenderManager->width;
UInt32 Height = TheRenderManager->height;
SettingsShadowStruct::ExteriorsStruct* ShadowsExteriors = &TheSettingManager->SettingsShadows.Exteriors;
SettingsShadowStruct::InteriorsStruct* ShadowsInteriors = &TheSettingManager->SettingsShadows.Interiors;
	UINT ShadowMapSize = 0;
	UINT ShadowCubeMapSize = ShadowsInteriors->ShadowCubeMapSize;
	
	TheTextureManager->SourceTexture = NULL;
	TheTextureManager->SourceSurface = NULL;
	TheTextureManager->RenderedTexture = NULL;
	TheTextureManager->RenderedSurface = NULL;
	TheTextureManager->DepthTexture = NULL;
	TheTextureManager->DepthSurface = NULL;
	Device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &TheTextureManager->SourceTexture, NULL);
Device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &TheTextureManager->RenderedTexture, NULL);
TheTextureManager->SourceTexture->GetSurfaceLevel(0, &TheTextureManager->SourceSurface);
TheTextureManager->RenderedTexture->GetSurfaceLevel(0, &TheTextureManager->RenderedSurface);
Device->CreateTexture(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL, (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &TheTextureManager->DepthTexture, NULL);

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
}

	for (int i = 0; i <= ShadowManager::ShadowMapTypeEnum::MapOrtho; i++) {
		// create one texture per Exterior ShadowMap type
		float multiple = i == ShadowManager::ShadowMapTypeEnum::MapLod ? 2.0f : 1.0f; // double the size of lod map only
		ShadowMapSize = ShadowsExteriors->ShadowMapResolution * multiple;
		
		Device->CreateTexture(ShadowMapSize, ShadowMapSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_G32R32F, D3DPOOL_DEFAULT, &TheTextureManager->ShadowMapTexture[i], NULL);
		TheTextureManager->ShadowMapTexture[i]->GetSurfaceLevel(0, &TheTextureManager->ShadowMapSurface[i]);
		Device->CreateDepthStencilSurface(ShadowMapSize, ShadowMapSize, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, true, &TheTextureManager->ShadowMapDepthSurface[i], NULL);
        if (i != TheShadowManager->ShadowMapTypeEnum::MapOrtho){ //Don't blur orthomap
            // create a texture to receive the surface contents
			Device->CreateTexture(ShadowMapSize, ShadowMapSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_G32R32F, D3DPOOL_DEFAULT, &TheTextureManager->ShadowMapTextureBlurred[i], NULL);
            // set the surface level to the texture.
			TheTextureManager->ShadowMapTextureBlurred[i]->GetSurfaceLevel(0, &TheTextureManager->ShadowMapSurfaceBlurred[i]);
        }
    }
	for (int i = 0; i < ShadowCubeMapsMax; i++) {
		Device->CreateCubeTexture(ShadowCubeMapSize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &TheTextureManager->ShadowCubeMapTexture[i], NULL);
		for (int j = 0; j < 6; j++) {
			TheTextureManager->ShadowCubeMapTexture[i]->GetCubeMapSurface((D3DCUBEMAP_FACES)j, 0, &TheTextureManager->ShadowCubeMapSurface[i][j]);
		}
	}

Device->CreateDepthStencilSurface(ShadowCubeMapSize, ShadowCubeMapSize, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, true, &TheTextureManager->ShadowCubeMapDepthSurface, NULL);

TextureManager::PublishBridgeState();

}

void TextureManager::PublishBridgeState() {

if (!TheTextureManager)
return;

std::vector<TRBridgeRenderTargetDescriptor> descriptors;
std::vector<TRBridgeInteropSurface> interops;

auto appendTexture = [&](const char* name, IDirect3DTexture9* texture, uint32_t usageFlags) {
if (!texture)
return;

D3DSURFACE_DESC desc;
if (FAILED(texture->GetLevelDesc(0, &desc)))
return;

TRBridgeRenderTargetDescriptor descriptor{};
std::strncpy(descriptor.name, name, sizeof(descriptor.name) - 1);
descriptor.name[sizeof(descriptor.name) - 1] = '\0';
descriptor.width = desc.Width;
descriptor.height = desc.Height;
descriptor.mipLevels = texture->GetLevelCount();
descriptor.arrayLayers = 1;
descriptor.format = ConvertFormat(desc.Format);
descriptor.usageFlags = usageFlags;

descriptors.push_back(descriptor);

TRBridgeInteropSurface surface{};
surface.descriptor = descriptor;
surface.handleCount = 0;
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_D3D9_TEXTURE_POINTER, reinterpret_cast<uint64_t>(texture));

IDirect3DSurface9* levelSurface = NULL;
if (SUCCEEDED(texture->GetSurfaceLevel(0, &levelSurface)) && levelSurface)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_D3D9_SURFACE_POINTER, reinterpret_cast<uint64_t>(levelSurface));
levelSurface->Release();
}

if (const VulkanInteropEntry* interopEntry = FindInteropInfo(name))
{
const DxvkInteropSurfaceInfo& info = interopEntry->info;
if (info.image)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE, info.image);
}
if (info.imageView)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW, info.imageView);
}
if (info.memory)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_DEVICE_MEMORY, info.memory);
}
if (info.externalMemory.win32Handle)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_WIN32_SHARED_HANDLE, info.externalMemory.win32Handle);
}
if (info.externalMemory.opaqueFd >= 0)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_OPAQUE_FD, static_cast<uint64_t>(static_cast<int64_t>(info.externalMemory.opaqueFd)));
}
if (info.rowPitch)
{
surface.rowPitch = info.rowPitch;
}
if (info.depthPitch)
{
surface.depthPitch = info.depthPitch;
}
}

interops.push_back(surface);
};

appendTexture("TESR_SourceTexture", TheTextureManager->SourceTexture, TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
appendTexture("TESR_RenderedTexture", TheTextureManager->RenderedTexture, TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
appendTexture("TESR_DepthTexture", TheTextureManager->DepthTexture, TR_BRIDGE_RT_USAGE_DEPTH_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);

if (TheTextureManager->DepthSurface)
{
D3DSURFACE_DESC desc;
if (SUCCEEDED(TheTextureManager->DepthSurface->GetDesc(&desc)))
{
TRBridgeRenderTargetDescriptor descriptor{};
std::strncpy(descriptor.name, "TESR_MainDepthStencil", sizeof(descriptor.name) - 1);
descriptor.name[sizeof(descriptor.name) - 1] = '\0';
descriptor.width = desc.Width;
descriptor.height = desc.Height;
descriptor.mipLevels = 1;
descriptor.arrayLayers = 1;
descriptor.format = ConvertFormat(desc.Format);
if (descriptor.format == TR_BRIDGE_FORMAT_UNDEFINED)
descriptor.format = TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT;
descriptor.usageFlags = TR_BRIDGE_RT_USAGE_DEPTH_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT;

descriptors.push_back(descriptor);

TRBridgeInteropSurface surface{};
surface.descriptor = descriptor;
surface.handleCount = 0;
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_D3D9_SURFACE_POINTER, reinterpret_cast<uint64_t>(TheTextureManager->DepthSurface));

if (const VulkanInteropEntry* interopEntry = FindInteropInfo("TESR_MainDepthStencil"))
{
const DxvkInteropSurfaceInfo& info = interopEntry->info;
if (info.image)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE, info.image);
}
if (info.imageView)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW, info.imageView);
}
if (info.memory)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_VK_DEVICE_MEMORY, info.memory);
}
if (info.externalMemory.win32Handle)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_WIN32_SHARED_HANDLE, info.externalMemory.win32Handle);
}
if (info.externalMemory.opaqueFd >= 0)
{
AppendHandle(surface, TR_BRIDGE_INTEROP_HANDLE_OPAQUE_FD, static_cast<uint64_t>(static_cast<int64_t>(info.externalMemory.opaqueFd)));
}
}

interops.push_back(surface);

if (g_vulkanInteropAvailable)
{
if (const VulkanInteropEntry* validationInfo = FindInteropInfo("TESR_MainDepthStencil"))
{
ValidateInteropSurface("TESR_MainDepthStencil", TheTextureManager->DepthSurface, validationInfo->info);
}
}
}
}

TRBridge_SetRenderTargets(descriptors.empty() ? NULL : descriptors.data(), descriptors.size());
TRBridge_SetInteropSurfaces(interops.empty() ? NULL : interops.data(), interops.size());
}

/*
* Binds texture buffers to a given register name
*/
TextureRecord* TextureManager::LoadTexture(ID3DXBuffer* ShaderSourceBuffer, D3DXPARAMETER_TYPE ConstantType, LPCSTR ConstantName, UINT ConstantIndex, bool* HasRenderedBuffer, bool* HasDepthBuffer) {
	
	TextureRecord::TextureRecordType Type = TextureRecord::TextureRecordType::None;
	std::string Source = std::string((const char*) ShaderSourceBuffer->GetBufferPointer());
	std::string TexturePath;
	Type = (ConstantType >= D3DXPT_SAMPLER && ConstantType <= D3DXPT_SAMPLER2D) ? TextureRecord::TextureRecordType::PlanarBuffer : Type;
	Type = ConstantType == D3DXPT_SAMPLER3D ? TextureRecord::TextureRecordType::VolumeBuffer : Type;
	Type = ConstantType == D3DXPT_SAMPLERCUBE ? TextureRecord::TextureRecordType::CubeBuffer : Type;
	Type = !strcmp(ConstantName, "TESR_SourceBuffer") ? TextureRecord::TextureRecordType::SourceBuffer : Type;
	Type = !strcmp(ConstantName, "TESR_RenderedBuffer") ? TextureRecord::TextureRecordType::RenderedBuffer : Type;
	Type = !strcmp(ConstantName, "TESR_DepthBuffer") ? TextureRecord::TextureRecordType::DepthBuffer : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowMapBufferNear") ? TextureRecord::TextureRecordType::ShadowMapBufferNear : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowMapBufferMiddle") ? TextureRecord::TextureRecordType::ShadowMapBufferMiddle : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowMapBufferFar") ? TextureRecord::TextureRecordType::ShadowMapBufferFar : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowMapBufferLod") ? TextureRecord::TextureRecordType::ShadowMapBufferLod : Type;
	Type = !strcmp(ConstantName, "TESR_OrthoMapBuffer") ? TextureRecord::TextureRecordType::OrthoMapBuffer : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowCubeMapBuffer0") ? TextureRecord::TextureRecordType::ShadowCubeMapBuffer0 : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowCubeMapBuffer1") ? TextureRecord::TextureRecordType::ShadowCubeMapBuffer1 : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowCubeMapBuffer2") ? TextureRecord::TextureRecordType::ShadowCubeMapBuffer2 : Type;
	Type = !strcmp(ConstantName, "TESR_ShadowCubeMapBuffer3") ? TextureRecord::TextureRecordType::ShadowCubeMapBuffer3 : Type;
	Type = !strcmp(ConstantName, WordWaterHeightMapBuffer) ? TextureRecord::TextureRecordType::WaterHeightMapBuffer : Type;
	Type = !strcmp(ConstantName, WordWaterReflectionMapBuffer) ? TextureRecord::TextureRecordType::WaterReflectionMapBuffer : Type;

	if (HasRenderedBuffer && !*HasRenderedBuffer) *HasRenderedBuffer = (Type == TextureRecord::TextureRecordType::RenderedBuffer);
	if (HasDepthBuffer && !*HasDepthBuffer) *HasDepthBuffer = (Type == TextureRecord::TextureRecordType::DepthBuffer);
	if (Type) {
		size_t SamplerPos = Source.find(("register ( s" + std::to_string(ConstantIndex) + " )"));
		if(SamplerPos == std::string::npos) {
			Logger::Log("[ERROR] %s  cannot be binded", ConstantName);
			return nullptr;
		}
		if(Type >= TextureRecord::TextureRecordType::PlanarBuffer && Type <= TextureRecord::TextureRecordType::CubeBuffer){
			//Only these samplers are bindable to an arbitrary texture
			size_t StartTexture = Source.find("<", SamplerPos +1);
			size_t EndTexture = Source.find(">", SamplerPos +1);
			if(StartTexture == std::string::npos || EndTexture == std::string::npos) {
				Logger::Log("[ERROR] %s  cannot be binded", ConstantName);
				return nullptr;
			}
			std::string TextureString = Source.substr(StartTexture +1, EndTexture - StartTexture - 1);
			TexturePath = GetFilenameForTexture(TextureString);
		}
		size_t StartStatePos = Source.find("{", SamplerPos);
		size_t EndStatePos = Source.find("}", SamplerPos);
		if(EndStatePos == std::string::npos || StartStatePos == std::string::npos) {
			Logger::Log("[ERROR] %s  cannot be binded", ConstantName);
			return nullptr;
		}
		std::string SamplerString = Source.substr(StartStatePos + 1, EndStatePos - StartStatePos - 1);
//		Logger::Log("%s \n", SamplerString.c_str());
		TextureRecord* NewTextureRecord = new TextureRecord();
        if(Type >= TextureRecord::TextureRecordType::WaterHeightMapBuffer){ /*Texture assigned after init*/
            if(Type == TextureRecord::TextureRecordType::WaterHeightMapBuffer){
                NewTextureRecord->Texture = WaterHeightMapB; 
                WaterHeightMapTextures.push_back(NewTextureRecord);
            }
            else if(Type == TextureRecord::TextureRecordType::WaterReflectionMapBuffer){
                NewTextureRecord->Texture = WaterReflectionMapB; 
                WaterHeightMapTextures.push_back(NewTextureRecord);
            }
            Logger::Log("Game Texture %s Attached", ConstantName);
        }
		else if(Type >= TextureRecord::TextureRecordType::PlanarBuffer && Type <= TextureRecord::TextureRecordType::CubeBuffer){ //Cache only non game textures
			IDirect3DBaseTexture9* cached = GetCachedTexture(TexturePath);
			if(!cached) {
				if (NewTextureRecord->LoadTexture(Type, TexturePath.c_str())) {
					if (NewTextureRecord->Texture){
						Logger::Log("Texture loaded: %s", TexturePath.c_str());
						Textures[TexturePath] = NewTextureRecord->Texture;
					}
				}
				else {
					Logger::Log("ERROR: Cannot load texture %s", TexturePath.c_str());
				}
			}
			else {
				NewTextureRecord->Texture = cached;
				Logger::Log("Texture linked: %s", TexturePath.c_str());
			}
		}
		else {
			if (NewTextureRecord->LoadTexture(Type, nullptr)) {
				Logger::Log("Game Texture %s Binded", ConstantName);
			}
			else {
                Logger::Log("ERROR: Cannot bind texture %s", ConstantName);
            }
		}

		GetSamplerStates(SamplerString, NewTextureRecord);
		return NewTextureRecord;
	}
	Logger::Log("[ERROR] Sampler %s doesn't have a valid type", ConstantName);
	return nullptr;
}

IDirect3DBaseTexture9* TextureManager::GetCachedTexture(std::string& pathS){
    TextureList::iterator t = Textures.find(pathS);
    if (t == Textures.end()) return nullptr;
    return t->second;
}

std::string ltrim(const std::string& s) {
	return std::regex_replace(s, std::regex("^\\s+"), "");
}
std::string rtrim(const std::string& s) {
	return std::regex_replace(s, std::regex("\\s+$"), "");
}

std::string trim(const std::string& s ) {
	return ltrim(rtrim(s));
}

std::string TextureManager::GetFilenameForTexture(std::string& resourceSubstring){
	std::string PathS;
	if (resourceSubstring.find("ResourceName") != std::string::npos) {
		size_t StartPath = resourceSubstring.find("\"");
		size_t EndPath = resourceSubstring.rfind("\"");
		PathS = trim(resourceSubstring.substr(StartPath + 1, EndPath - 1 - StartPath));
		PathS.insert(0, "Data\\Textures\\");
	}
	else{
		Logger::Log("[ERROR] Cannot parse bindable texture");
	}
	return PathS;
}


void TextureManager::GetSamplerStates(std::string& samplerStateSubstring, TextureRecord* textureRecord ) {
	std::string WordSamplerType[SamplerStatesMax];
	std::string WordTextureAddress[6];
	std::string WordTextureFilterType[4];
	std::string WordSRGBType[2];
	WordSamplerType[0] = "";
	WordSamplerType[D3DSAMP_ADDRESSU] = "ADDRESSU";
	WordSamplerType[D3DSAMP_ADDRESSV] = "ADDRESSV";
	WordSamplerType[D3DSAMP_ADDRESSW] = "ADDRESSW";
	WordSamplerType[D3DSAMP_BORDERCOLOR] = "BORDERCOLOR";
	WordSamplerType[D3DSAMP_MAGFILTER] = "MAGFILTER";
	WordSamplerType[D3DSAMP_MINFILTER] = "MINFILTER";
	WordSamplerType[D3DSAMP_MIPFILTER] = "MIPFILTER";
	WordSamplerType[D3DSAMP_MIPMAPLODBIAS] = "MIPMAPLODBIAS";
	WordSamplerType[D3DSAMP_MAXMIPLEVEL] = "MAXMIPLEVEL";
	WordSamplerType[D3DSAMP_MAXANISOTROPY] = "MAXANISOTROPY";
	WordSamplerType[D3DSAMP_SRGBTEXTURE] = "SRGBTEXTURE";
    WordSamplerType[D3DSAMP_ELEMENTINDEX] = "";
    WordSamplerType[D3DSAMP_DMAPOFFSET] = "";
	WordTextureAddress[0] = "";
	WordTextureAddress[D3DTADDRESS_WRAP] = "WRAP";
	WordTextureAddress[D3DTADDRESS_MIRROR] = "MIRROR";
	WordTextureAddress[D3DTADDRESS_CLAMP] = "CLAMP";
	WordTextureAddress[D3DTADDRESS_BORDER] = "BORDER";
	WordTextureAddress[D3DTADDRESS_MIRRORONCE] = "MIRRORONCE";
	WordTextureFilterType[D3DTEXF_NONE] = "NONE";
	WordTextureFilterType[D3DTEXF_POINT] = "POINT";
	WordTextureFilterType[D3DTEXF_LINEAR] = "LINEAR";
	WordTextureFilterType[D3DTEXF_ANISOTROPIC] = "ANISOTROPIC";
	WordSRGBType[0] = "FALSE";
	WordSRGBType[1] = "TRUE";

	std::stringstream samplerSettings = std::stringstream(trim(samplerStateSubstring));
	std::string setting;
	while (std::getline(samplerSettings, setting, ';')) {
		size_t newlinePos = setting.find("\n");
		if (newlinePos != std::string::npos) setting.erase(newlinePos, 1);
		std::string opt = trim(setting.substr(0, setting.find("=") - 1));
		std::string val = trim(setting.substr(setting.find("=") + 1, setting.length()));
        std::transform(opt.begin(), opt.end(),opt.begin(), ::toupper);
        std::transform(val.begin(), val.end(),val.begin(), ::toupper);
	//	Logger::Log("%s : %s", opt.c_str(), val.c_str());
		size_t optIdx = 0;
		for (size_t i = 1; i < 12; i++) {
			if (opt == WordSamplerType[i]) {
				optIdx = i;
				break;
			}
		}
		if (optIdx >= D3DSAMP_ADDRESSU && optIdx <= D3DSAMP_ADDRESSW) {
			for (size_t i = 1; i < 6; i++) {
				if (val == WordTextureAddress[i]) {
					textureRecord->SamplerStates[(D3DSAMPLERSTATETYPE)optIdx] = i;
					break;
				}
			}
		}
		else if (optIdx >= D3DSAMP_MAGFILTER && optIdx <= D3DSAMP_MIPFILTER) {
			for (size_t i = 0; i < 4; i++) {
				if (val == WordTextureFilterType[i]) {
					textureRecord->SamplerStates[(D3DSAMPLERSTATETYPE)optIdx] = i;
					break;
				}
			}
		}
		else if (optIdx == D3DSAMP_SRGBTEXTURE) {
			for (size_t i = 0; i < 2; i++) {
				if (val == WordSRGBType[i]) {
					textureRecord->SamplerStates[(D3DSAMPLERSTATETYPE)optIdx] = i;
					break;
				}
			}
		}
		else if(optIdx == D3DSAMP_BORDERCOLOR){
            float va = std::stof(val);
			textureRecord->SamplerStates[(D3DSAMPLERSTATETYPE)optIdx] = *((DWORD*)&va);
        }
		else if(optIdx == D3DSAMP_MAXANISOTROPY){
			textureRecord->SamplerStates[(D3DSAMPLERSTATETYPE)optIdx] = std::stoi(val);
        }
	}
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