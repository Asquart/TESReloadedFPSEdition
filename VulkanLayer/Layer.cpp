#include "VulkanLayerStubs.h"

#include "../Shared/Bridge/Bridge.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <inttypes.h>
#include <memory>
#include <mutex>
#include <optional>
#include <system_error>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <shaderc/shaderc.h>

namespace tesreloaded::vulkan
{
namespace
{
constexpr const char* kLayerName = "VK_LAYER_TESRELOADED_implicit";

const char* HandleTypeToString(TRBridgeInteropHandleType type)
{
    switch (type)
    {
        case TR_BRIDGE_INTEROP_HANDLE_NONE:
            return "None";
        case TR_BRIDGE_INTEROP_HANDLE_D3D9_TEXTURE_POINTER:
            return "D3D9Texture";
        case TR_BRIDGE_INTEROP_HANDLE_D3D9_SURFACE_POINTER:
            return "D3D9Surface";
        case TR_BRIDGE_INTEROP_HANDLE_WIN32_SHARED_HANDLE:
            return "Win32Handle";
        case TR_BRIDGE_INTEROP_HANDLE_OPAQUE_FD:
            return "OpaqueFd";
        case TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE:
            return "VkImage";
        case TR_BRIDGE_INTEROP_HANDLE_VK_DEVICE_MEMORY:
            return "VkDeviceMemory";
        case TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW:
            return "VkImageView";
        case TR_BRIDGE_INTEROP_HANDLE_VK_SEMAPHORE:
            return "VkSemaphore";
        case TR_BRIDGE_INTEROP_HANDLE_VK_FENCE:
            return "VkFence";
        default:
            return "Unknown";
    }
}

struct InstanceDispatch
{
    PFN_vkDestroyInstance DestroyInstance = nullptr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
    PFN_vkCreateDevice CreateDevice = nullptr;
};

struct DeviceDispatch
{
    PFN_vkDestroyDevice DestroyDevice = nullptr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
    PFN_vkGetDeviceQueue GetDeviceQueue = nullptr;
    PFN_vkGetDeviceQueue2 GetDeviceQueue2 = nullptr;
    PFN_vkQueueSubmit QueueSubmit = nullptr;
    PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;
    PFN_vkCreateCommandPool CreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool DestroyCommandPool = nullptr;
    PFN_vkResetCommandPool ResetCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers FreeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer BeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer EndCommandBuffer = nullptr;
    PFN_vkCreateFence CreateFence = nullptr;
    PFN_vkDestroyFence DestroyFence = nullptr;
    PFN_vkWaitForFences WaitForFences = nullptr;
    PFN_vkResetFences ResetFences = nullptr;
    PFN_vkCreateShaderModule CreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule DestroyShaderModule = nullptr;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout = nullptr;
    PFN_vkCreatePipelineLayout CreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout = nullptr;
    PFN_vkCreateComputePipelines CreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline DestroyPipeline = nullptr;
    PFN_vkCreateDescriptorPool CreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets = nullptr;
    PFN_vkFreeDescriptorSets FreeDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets = nullptr;
    PFN_vkCreateSampler CreateSampler = nullptr;
    PFN_vkDestroySampler DestroySampler = nullptr;
    PFN_vkCmdBindPipeline CmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets = nullptr;
    PFN_vkCmdDispatch CmdDispatch = nullptr;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
    bool computeFunctionsLoggedMissing = false;

    bool HasComputeSupport() const
    {
        return CreateCommandPool && DestroyCommandPool && ResetCommandPool && AllocateCommandBuffers &&
               FreeCommandBuffers && BeginCommandBuffer && EndCommandBuffer && CreateFence && DestroyFence &&
               WaitForFences && ResetFences && CreateShaderModule && DestroyShaderModule &&
               CreateDescriptorSetLayout && DestroyDescriptorSetLayout && CreatePipelineLayout &&
               DestroyPipelineLayout && CreateComputePipelines && DestroyPipeline && CreateDescriptorPool &&
               DestroyDescriptorPool && AllocateDescriptorSets && UpdateDescriptorSets && CreateSampler &&
               DestroySampler && CmdBindPipeline &&
               CmdBindDescriptorSets && CmdDispatch && CmdPipelineBarrier;
    }
};

struct QueueRegistration
{
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
};

struct QueueComputeState
{
    std::mutex mutex;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool initialized = false;
    bool inFlight = false;
    uint64_t lastSubmittedFrame = 0;
};

struct DeviceComputeState
{
    std::mutex mutex;
    bool initialized = false;
    bool descriptorSetValid = false;
    std::filesystem::file_time_type shaderTimestamp{};
    std::vector<uint32_t> shaderCode;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

struct ShaderCompilerContext
{
    std::mutex mutex;
    bool attemptedLoad = false;
    bool available = false;
#if defined(_WIN32)
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif
    shaderc_compiler_t compiler = nullptr;
    shaderc_compile_options_t options = nullptr;

    decltype(&shaderc_compiler_initialize) compilerInitialize = nullptr;
    decltype(&shaderc_compiler_release) compilerRelease = nullptr;
    decltype(&shaderc_compile_options_initialize) optionsInitialize = nullptr;
    decltype(&shaderc_compile_options_release) optionsRelease = nullptr;
    decltype(&shaderc_compile_options_set_source_language) optionsSetSourceLanguage = nullptr;
    decltype(&shaderc_compile_options_set_target_env) optionsSetTargetEnv = nullptr;
    decltype(&shaderc_compile_options_set_target_spirv) optionsSetTargetSpirv = nullptr;
    decltype(&shaderc_compile_options_set_optimization_level) optionsSetOptimizationLevel = nullptr;
    decltype(&shaderc_compile_into_spv) compileIntoSpv = nullptr;
    decltype(&shaderc_result_get_compilation_status) resultGetCompilationStatus = nullptr;
    decltype(&shaderc_result_get_error_message) resultGetErrorMessage = nullptr;
    decltype(&shaderc_result_get_length) resultGetLength = nullptr;
    decltype(&shaderc_result_get_bytes) resultGetBytes = nullptr;
    decltype(&shaderc_result_release) resultRelease = nullptr;
};

struct PendingFrameState
{
    bool hasFrame = false;
    TRBridgePluginFrameInputs inputs{};
    std::vector<TRBridgeInteropSurface> surfaces;
    TRBridgeInteropSyncState syncState{};
    bool hasSyncState = false;
};

std::mutex g_dispatchMutex;
std::unordered_map<VkInstance, InstanceDispatch> g_instanceDispatch;
std::unordered_map<VkDevice, DeviceDispatch> g_deviceDispatch;
std::unordered_map<VkQueue, QueueRegistration> g_queueOwners;
std::unordered_map<VkQueue, std::shared_ptr<QueueComputeState>> g_queueComputeStates;
std::unordered_map<VkDevice, std::shared_ptr<DeviceComputeState>> g_deviceComputeStates;

ShaderCompilerContext& GetShaderCompiler()
{
    static ShaderCompilerContext compilerContext;
    return compilerContext;
}

std::mutex g_pendingFrameMutex;
PendingFrameState g_pendingFrame;

PFN_vkGetInstanceProcAddr g_nextGetInstanceProcAddr = nullptr;
PFN_vkGetDeviceProcAddr g_nextGetDeviceProcAddr = nullptr;

void Log(const char* fmt, ...)
{
    char buffer[1024];
    std::va_list args;
    va_start(args, fmt);
    int written = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (written < 0)
    {
        return;
    }

    buffer[sizeof(buffer) - 1] = '\0';
    TRBridge_LogMessage(buffer);
}

std::filesystem::path GetLayerModuleDirectory()
{
    static std::filesystem::path cachedPath;
    static bool cached = false;
    if (cached)
    {
        return cachedPath;
    }

#if defined(_WIN32)
    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&GetLayerModuleDirectory), &module))
    {
        char modulePath[MAX_PATH] = {};
        DWORD length = GetModuleFileNameA(module, modulePath, static_cast<DWORD>(std::size(modulePath)));
        if (length > 0)
        {
            cachedPath = std::filesystem::path(modulePath).parent_path();
        }
    }
#else
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&GetLayerModuleDirectory), &info) != 0 && info.dli_fname != nullptr)
    {
        cachedPath = std::filesystem::path(info.dli_fname).parent_path();
    }
#endif

    if (cachedPath.empty())
    {
        std::error_code ec;
        cachedPath = std::filesystem::current_path(ec);
    }

    cached = true;
    return cachedPath;
}

std::filesystem::path GetShaderDirectory()
{
    static std::filesystem::path cachedPath;
    static bool cached = false;
    if (cached)
    {
        return cachedPath;
    }

    const std::filesystem::path moduleDir = GetLayerModuleDirectory();
    const std::array<std::filesystem::path, 4> candidates = {
        moduleDir / "Shaders",
        moduleDir / "VulkanLayer" / "Shaders",
        moduleDir.parent_path() / "VulkanLayer" / "Shaders",
        moduleDir / ".." / "VulkanLayer" / "Shaders"
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec))
        {
            cachedPath = std::filesystem::weakly_canonical(candidate, ec);
            if (cachedPath.empty())
            {
                cachedPath = candidate;
            }
            break;
        }
    }

    if (cachedPath.empty())
    {
        cachedPath = moduleDir;
    }

    cached = true;
    return cachedPath;
}

std::filesystem::path ResolveShaderPath(const std::string& relativePath)
{
    return GetShaderDirectory() / relativePath;
}

bool EnsureShaderCompilerLoaded()
{
    auto& compiler = GetShaderCompiler();
    std::lock_guard<std::mutex> lock(compiler.mutex);
    if (compiler.available)
    {
        return true;
    }
    if (compiler.attemptedLoad)
    {
        return false;
    }

    compiler.attemptedLoad = true;

#if defined(_WIN32)
    compiler.module = LoadLibraryA("shaderc_shared.dll");
    if (!compiler.module)
    {
        const std::filesystem::path moduleDir = GetLayerModuleDirectory();
        const std::array<std::filesystem::path, 3> searchPaths = {
            moduleDir / "shaderc_shared.dll",
            moduleDir.parent_path() / "ThirdParty" / "VulkanSDK" / "1.4.328.1" / "Bin" / "shaderc_shared.dll",
            moduleDir / "ThirdParty" / "VulkanSDK" / "1.4.328.1" / "Bin" / "shaderc_shared.dll"
        };

        for (const auto& path : searchPaths)
        {
            if (!path.empty())
            {
                compiler.module = LoadLibraryA(path.string().c_str());
                if (compiler.module)
                {
                    break;
                }
            }
        }
    }
#else
    compiler.module = dlopen("libshaderc_shared.so", RTLD_NOW | RTLD_LOCAL);
    if (!compiler.module)
    {
        const std::filesystem::path moduleDir = GetLayerModuleDirectory();
        const std::array<std::filesystem::path, 3> searchPaths = {
            moduleDir / "libshaderc_shared.so",
            moduleDir.parent_path() / "ThirdParty" / "VulkanSDK" / "1.4.328.1" / "Bin" / "libshaderc_shared.so",
            moduleDir / "ThirdParty" / "VulkanSDK" / "1.4.328.1" / "Bin" / "libshaderc_shared.so"
        };

        for (const auto& path : searchPaths)
        {
            if (!path.empty())
            {
                compiler.module = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
                if (compiler.module)
                {
                    break;
                }
            }
        }
    }
#endif

    if (!compiler.module)
    {
        Log("[TESR][Layer] Failed to locate shaderc_shared library");
        return false;
    }

    auto loadSymbol = [&](auto& target, const char* name) -> bool {
#if defined(_WIN32)
        target = reinterpret_cast<std::remove_reference_t<decltype(target)>>(GetProcAddress(compiler.module, name));
#else
        target = reinterpret_cast<std::remove_reference_t<decltype(target)>>(dlsym(compiler.module, name));
#endif
        if (!target)
        {
            Log("[TESR][Layer] Missing shaderc symbol %s", name);
            return false;
        }
        return true;
    };

    if (!loadSymbol(compiler.compilerInitialize, "shaderc_compiler_initialize") ||
        !loadSymbol(compiler.compilerRelease, "shaderc_compiler_release") ||
        !loadSymbol(compiler.optionsInitialize, "shaderc_compile_options_initialize") ||
        !loadSymbol(compiler.optionsRelease, "shaderc_compile_options_release") ||
        !loadSymbol(compiler.optionsSetSourceLanguage, "shaderc_compile_options_set_source_language") ||
        !loadSymbol(compiler.optionsSetTargetEnv, "shaderc_compile_options_set_target_env") ||
        !loadSymbol(compiler.optionsSetTargetSpirv, "shaderc_compile_options_set_target_spirv") ||
        !loadSymbol(compiler.optionsSetOptimizationLevel, "shaderc_compile_options_set_optimization_level") ||
        !loadSymbol(compiler.compileIntoSpv, "shaderc_compile_into_spv") ||
        !loadSymbol(compiler.resultGetCompilationStatus, "shaderc_result_get_compilation_status") ||
        !loadSymbol(compiler.resultGetErrorMessage, "shaderc_result_get_error_message") ||
        !loadSymbol(compiler.resultGetLength, "shaderc_result_get_length") ||
        !loadSymbol(compiler.resultGetBytes, "shaderc_result_get_bytes") ||
        !loadSymbol(compiler.resultRelease, "shaderc_result_release"))
    {
        Log("[TESR][Layer] Failed to resolve shaderc exports");
        return false;
    }

    compiler.compiler = compiler.compilerInitialize();
    if (!compiler.compiler)
    {
        Log("[TESR][Layer] shaderc_compiler_initialize returned null");
        return false;
    }

    compiler.options = compiler.optionsInitialize();
    if (!compiler.options)
    {
        compiler.compilerRelease(compiler.compiler);
        compiler.compiler = nullptr;
        Log("[TESR][Layer] shaderc_compile_options_initialize failed");
        return false;
    }

    compiler.optionsSetSourceLanguage(compiler.options, shaderc_source_language_glsl);
    compiler.optionsSetTargetEnv(compiler.options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    compiler.optionsSetTargetSpirv(compiler.options, shaderc_spirv_version_1_5);
    compiler.optionsSetOptimizationLevel(compiler.options, shaderc_optimization_level_performance);

    compiler.available = true;
    return true;
}

std::optional<std::vector<uint32_t>> CompileShaderSource(const std::filesystem::path& sourcePath)
{
    if (!EnsureShaderCompilerLoaded())
    {
        return std::nullopt;
    }

    auto& compiler = GetShaderCompiler();
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file)
    {
        Log("[TESR][Layer] Failed to open shader source %s", sourcePath.string().c_str());
        return std::nullopt;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (source.empty())
    {
        Log("[TESR][Layer] Shader source %s is empty", sourcePath.string().c_str());
        return std::nullopt;
    }

    shaderc_compilation_result_t result = compiler.compileIntoSpv(
        compiler.compiler, source.c_str(), source.size(), shaderc_compute_shader,
        sourcePath.string().c_str(), "main", compiler.options);
    if (!result)
    {
        Log("[TESR][Layer] shaderc failed to compile %s", sourcePath.string().c_str());
        return std::nullopt;
    }

    shaderc_compilation_status status = compiler.resultGetCompilationStatus(result);
    if (status != shaderc_compilation_status_success)
    {
        const char* message = compiler.resultGetErrorMessage(result);
        Log("[TESR][Layer] Shader compilation error: %s", message ? message : "<no message>");
        compiler.resultRelease(result);
        return std::nullopt;
    }

    size_t byteLength = compiler.resultGetLength(result);
    const char* byteData = compiler.resultGetBytes(result);
    std::vector<uint32_t> spirv;
    spirv.resize((byteLength + sizeof(uint32_t) - 1) / sizeof(uint32_t));
    std::memcpy(spirv.data(), byteData, byteLength);
    compiler.resultRelease(result);
    return spirv;
}

struct ShaderBinaryCache
{
    std::mutex mutex;
    std::filesystem::file_time_type timestamp{};
    std::vector<uint32_t> code;
};

struct ShaderLoadResult
{
    std::vector<uint32_t> code;
    std::filesystem::file_time_type timestamp{};
};

ShaderBinaryCache& GetShaderBinaryCache()
{
    static ShaderBinaryCache cache;
    return cache;
}

std::optional<ShaderLoadResult> LoadPlaceholderShaderBinary()
{
    auto& cache = GetShaderBinaryCache();
    std::lock_guard<std::mutex> lock(cache.mutex);

    const std::filesystem::path sourcePath = ResolveShaderPath("placeholder_ao.comp");
    const std::filesystem::path binaryPath = ResolveShaderPath("placeholder_ao.spv");

    std::error_code ec;
    std::filesystem::file_time_type sourceTime{};
    if (std::filesystem::exists(sourcePath, ec) && !ec)
    {
        sourceTime = std::filesystem::last_write_time(sourcePath, ec);
    }

    if (!cache.code.empty() && sourceTime == cache.timestamp)
    {
        return ShaderLoadResult{cache.code, cache.timestamp};
    }

    std::vector<uint32_t> spirv;
    bool loadedFromDisk = false;

    if (std::filesystem::exists(binaryPath, ec) && !ec)
    {
        auto binaryTime = std::filesystem::last_write_time(binaryPath, ec);
        if (!ec && (!std::filesystem::exists(sourcePath, ec) || binaryTime >= sourceTime))
        {
            std::ifstream binaryFile(binaryPath, std::ios::binary);
            if (binaryFile)
            {
                std::vector<char> rawData((std::istreambuf_iterator<char>(binaryFile)), std::istreambuf_iterator<char>());
                if (!rawData.empty())
                {
                    size_t paddedSize = (rawData.size() + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1);
                    rawData.resize(paddedSize, 0);
                    spirv.resize(rawData.size() / sizeof(uint32_t));
                    std::memcpy(spirv.data(), rawData.data(), rawData.size());
                    loadedFromDisk = true;
                }
            }
        }
    }

    if (!loadedFromDisk && std::filesystem::exists(sourcePath, ec) && !ec)
    {
        auto compiled = CompileShaderSource(sourcePath);
        if (compiled)
        {
            spirv = *compiled;
            std::ofstream outFile(binaryPath, std::ios::binary | std::ios::trunc);
            if (outFile)
            {
                outFile.write(reinterpret_cast<const char*>(spirv.data()),
                              static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
            }
        }
    }

    if (spirv.empty())
    {
        Log("[TESR][Layer] Unable to load placeholder compute shader");
        return std::nullopt;
    }

    cache.code = spirv;
    cache.timestamp = sourceTime;
    return ShaderLoadResult{cache.code, cache.timestamp};
}

VkLayerInstanceCreateInfo* GetInstanceChainInfo(const VkInstanceCreateInfo* createInfo, VkLayerFunction func)
{
    auto* chainInfo = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(createInfo->pNext));
    while (chainInfo != nullptr &&
           (chainInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || chainInfo->function != func))
    {
        chainInfo = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(chainInfo->pNext));
    }
    return chainInfo;
}

VkLayerDeviceCreateInfo* GetDeviceChainInfo(const VkDeviceCreateInfo* createInfo, VkLayerFunction func)
{
    auto* chainInfo = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(createInfo->pNext));
    while (chainInfo != nullptr &&
           (chainInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || chainInfo->function != func))
    {
        chainInfo = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(chainInfo->pNext));
    }
    return chainInfo;
}

InstanceDispatch& GetInstanceDispatch(VkInstance instance)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_instanceDispatch.find(instance);
    assert(it != g_instanceDispatch.end());
    return it->second;
}

DeviceDispatch& GetDeviceDispatch(VkDevice device)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_deviceDispatch.find(device);
    assert(it != g_deviceDispatch.end());
    return it->second;
}

QueueRegistration GetQueueRegistration(VkQueue queue)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_queueOwners.find(queue);
    if (it == g_queueOwners.end())
    {
        return {};
    }
    return it->second;
}

std::shared_ptr<QueueComputeState> GetOrCreateQueueComputeState(VkQueue queue)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_queueComputeStates.find(queue);
    if (it != g_queueComputeStates.end())
    {
        return it->second;
    }

    auto state = std::make_shared<QueueComputeState>();
    g_queueComputeStates.emplace(queue, state);
    return state;
}

std::shared_ptr<QueueComputeState> ExtractQueueComputeState(VkQueue queue)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_queueComputeStates.find(queue);
    if (it == g_queueComputeStates.end())
    {
        return nullptr;
    }
    auto state = it->second;
    g_queueComputeStates.erase(it);
    return state;
}

std::shared_ptr<DeviceComputeState> GetOrCreateDeviceComputeState(VkDevice device)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_deviceComputeStates.find(device);
    if (it != g_deviceComputeStates.end())
    {
        return it->second;
    }

    auto state = std::make_shared<DeviceComputeState>();
    g_deviceComputeStates.emplace(device, state);
    return state;
}

std::shared_ptr<DeviceComputeState> ExtractDeviceComputeState(VkDevice device)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    auto it = g_deviceComputeStates.find(device);
    if (it == g_deviceComputeStates.end())
    {
        return nullptr;
    }
    auto state = it->second;
    g_deviceComputeStates.erase(it);
    return state;
}

void RemoveInstance(VkInstance instance)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    g_instanceDispatch.erase(instance);
}

void RemoveDevice(VkDevice device)
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    g_deviceDispatch.erase(device);
}

void RegisterQueue(VkDevice device, VkQueue queue, uint32_t queueFamilyIndex)
{
    if (queue == VK_NULL_HANDLE)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    g_queueOwners[queue] = QueueRegistration{device, queueFamilyIndex};
}

void DestroyQueueComputeState(VkDevice device, DeviceDispatch& dispatch, const std::shared_ptr<QueueComputeState>& state)
{
    if (!state)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->inFlight && dispatch.WaitForFences && state->fence != VK_NULL_HANDLE)
    {
        dispatch.WaitForFences(device, 1, &state->fence, VK_TRUE, UINT64_MAX);
        if (dispatch.ResetFences)
        {
            dispatch.ResetFences(device, 1, &state->fence);
        }
        state->inFlight = false;
    }

    if (dispatch.DestroyFence && state->fence != VK_NULL_HANDLE)
    {
        dispatch.DestroyFence(device, state->fence, nullptr);
        state->fence = VK_NULL_HANDLE;
    }

    if (dispatch.FreeCommandBuffers && state->commandBuffer != VK_NULL_HANDLE)
    {
        dispatch.FreeCommandBuffers(device, state->commandPool, 1, &state->commandBuffer);
        state->commandBuffer = VK_NULL_HANDLE;
    }

    if (dispatch.DestroyCommandPool && state->commandPool != VK_NULL_HANDLE)
    {
        dispatch.DestroyCommandPool(device, state->commandPool, nullptr);
        state->commandPool = VK_NULL_HANDLE;
    }

    state->initialized = false;
    state->lastSubmittedFrame = 0;
}

void DestroyDeviceComputeState(VkDevice device, DeviceDispatch& dispatch, const std::shared_ptr<DeviceComputeState>& state)
{
    if (!state)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(state->mutex);

    if (state->descriptorPool != VK_NULL_HANDLE)
    {
        if (dispatch.DestroyDescriptorPool)
        {
            dispatch.DestroyDescriptorPool(device, state->descriptorPool, nullptr);
        }
        state->descriptorPool = VK_NULL_HANDLE;
        state->descriptorSet = VK_NULL_HANDLE;
    }

    if (state->pipeline != VK_NULL_HANDLE)
    {
        if (dispatch.DestroyPipeline)
        {
            dispatch.DestroyPipeline(device, state->pipeline, nullptr);
        }
        state->pipeline = VK_NULL_HANDLE;
    }

    if (state->sampler != VK_NULL_HANDLE)
    {
        if (dispatch.DestroySampler)
        {
            dispatch.DestroySampler(device, state->sampler, nullptr);
        }
        state->sampler = VK_NULL_HANDLE;
    }

    if (state->pipelineLayout != VK_NULL_HANDLE)
    {
        if (dispatch.DestroyPipelineLayout)
        {
            dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        }
        state->pipelineLayout = VK_NULL_HANDLE;
    }

    if (state->descriptorSetLayout != VK_NULL_HANDLE)
    {
        if (dispatch.DestroyDescriptorSetLayout)
        {
            dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        }
        state->descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (state->shaderModule != VK_NULL_HANDLE)
    {
        if (dispatch.DestroyShaderModule)
        {
            dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        }
        state->shaderModule = VK_NULL_HANDLE;
    }

    state->shaderCode.clear();
    state->descriptorSetValid = false;
    state->initialized = false;
}

bool EnsureDeviceComputePipeline(VkDevice device,
                                 DeviceDispatch& dispatch,
                                 const std::shared_ptr<DeviceComputeState>& state,
                                 const ShaderLoadResult& shader)
{
    if (!state)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(state->mutex);

    bool needsInitialization = !state->initialized || state->shaderTimestamp != shader.timestamp ||
                               state->shaderCode.size() != shader.code.size();
    if (!needsInitialization)
    {
        return state->pipeline != VK_NULL_HANDLE && state->descriptorSetLayout != VK_NULL_HANDLE &&
               state->pipelineLayout != VK_NULL_HANDLE && state->descriptorPool != VK_NULL_HANDLE &&
               state->descriptorSet != VK_NULL_HANDLE;
    }

    if (state->descriptorPool != VK_NULL_HANDLE && dispatch.DestroyDescriptorPool)
    {
        dispatch.DestroyDescriptorPool(device, state->descriptorPool, nullptr);
        state->descriptorPool = VK_NULL_HANDLE;
        state->descriptorSet = VK_NULL_HANDLE;
    }
    if (state->pipeline != VK_NULL_HANDLE && dispatch.DestroyPipeline)
    {
        dispatch.DestroyPipeline(device, state->pipeline, nullptr);
        state->pipeline = VK_NULL_HANDLE;
    }
    if (state->sampler != VK_NULL_HANDLE && dispatch.DestroySampler)
    {
        dispatch.DestroySampler(device, state->sampler, nullptr);
        state->sampler = VK_NULL_HANDLE;
    }
    if (state->pipelineLayout != VK_NULL_HANDLE && dispatch.DestroyPipelineLayout)
    {
        dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        state->pipelineLayout = VK_NULL_HANDLE;
    }
    if (state->descriptorSetLayout != VK_NULL_HANDLE && dispatch.DestroyDescriptorSetLayout)
    {
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (state->shaderModule != VK_NULL_HANDLE && dispatch.DestroyShaderModule)
    {
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = shader.code.size() * sizeof(uint32_t);
    moduleInfo.pCode = shader.code.data();

    VkResult moduleResult = dispatch.CreateShaderModule(device, &moduleInfo, nullptr, &state->shaderModule);
    if (moduleResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create shader module (%d)", moduleResult);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult layoutResult = dispatch.CreateDescriptorSetLayout(device, &layoutInfo, nullptr, &state->descriptorSetLayout);
    if (layoutResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create descriptor set layout (%d)", layoutResult);
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &state->descriptorSetLayout;

    VkResult pipelineLayoutResult = dispatch.CreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &state->pipelineLayout);
    if (pipelineLayoutResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create pipeline layout (%d)", pipelineLayoutResult);
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = state->shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = state->pipelineLayout;

    VkResult pipelineResult = dispatch.CreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &state->pipeline);
    if (pipelineResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create compute pipeline (%d)", pipelineResult);
        dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        state->pipelineLayout = VK_NULL_HANDLE;
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkResult samplerResult = dispatch.CreateSampler(device, &samplerInfo, nullptr, &state->sampler);
    if (samplerResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create compute sampler (%d)", samplerResult);
        dispatch.DestroyPipeline(device, state->pipeline, nullptr);
        state->pipeline = VK_NULL_HANDLE;
        dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        state->pipelineLayout = VK_NULL_HANDLE;
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkResult poolResult = dispatch.CreateDescriptorPool(device, &poolInfo, nullptr, &state->descriptorPool);
    if (poolResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create descriptor pool (%d)", poolResult);
        dispatch.DestroySampler(device, state->sampler, nullptr);
        state->sampler = VK_NULL_HANDLE;
        dispatch.DestroyPipeline(device, state->pipeline, nullptr);
        state->pipeline = VK_NULL_HANDLE;
        dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        state->pipelineLayout = VK_NULL_HANDLE;
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = state->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &state->descriptorSetLayout;

    VkResult allocResult = dispatch.AllocateDescriptorSets(device, &allocInfo, &state->descriptorSet);
    if (allocResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to allocate descriptor set (%d)", allocResult);
        dispatch.DestroyDescriptorPool(device, state->descriptorPool, nullptr);
        state->descriptorPool = VK_NULL_HANDLE;
        dispatch.DestroySampler(device, state->sampler, nullptr);
        state->sampler = VK_NULL_HANDLE;
        dispatch.DestroyPipeline(device, state->pipeline, nullptr);
        state->pipeline = VK_NULL_HANDLE;
        dispatch.DestroyPipelineLayout(device, state->pipelineLayout, nullptr);
        state->pipelineLayout = VK_NULL_HANDLE;
        dispatch.DestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
        state->descriptorSetLayout = VK_NULL_HANDLE;
        dispatch.DestroyShaderModule(device, state->shaderModule, nullptr);
        state->shaderModule = VK_NULL_HANDLE;
        return false;
    }

    state->descriptorSetValid = false;
    state->shaderCode = shader.code;
    state->shaderTimestamp = shader.timestamp;
    state->initialized = true;
    return true;
}

void ReleaseDeviceQueues(VkDevice device, DeviceDispatch& dispatch)
{
    std::vector<VkQueue> queues;
    {
        std::lock_guard<std::mutex> lock(g_dispatchMutex);
        for (const auto& entry : g_queueOwners)
        {
            if (entry.second.device == device)
            {
                queues.push_back(entry.first);
            }
        }
    }

    for (VkQueue queue : queues)
    {
        auto state = ExtractQueueComputeState(queue);
        DestroyQueueComputeState(device, dispatch, state);
        std::lock_guard<std::mutex> lock(g_dispatchMutex);
        g_queueOwners.erase(queue);
    }
}

bool EnsureQueueComputeState(VkDevice device,
                             uint32_t queueFamilyIndex,
                             DeviceDispatch& dispatch,
                             const std::shared_ptr<QueueComputeState>& state)
{
    if (!state)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->initialized)
    {
        return true;
    }

    if (!dispatch.HasComputeSupport())
    {
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkResult poolResult = dispatch.CreateCommandPool(device, &poolInfo, nullptr, &state->commandPool);
    if (poolResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create compute command pool (%d)", poolResult);
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = state->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult allocResult = dispatch.AllocateCommandBuffers(device, &allocInfo, &state->commandBuffer);
    if (allocResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to allocate compute command buffer (%d)", allocResult);
        dispatch.DestroyCommandPool(device, state->commandPool, nullptr);
        state->commandPool = VK_NULL_HANDLE;
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;

    VkResult fenceResult = dispatch.CreateFence(device, &fenceInfo, nullptr, &state->fence);
    if (fenceResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to create compute fence (%d)", fenceResult);
        dispatch.FreeCommandBuffers(device, state->commandPool, 1, &state->commandBuffer);
        state->commandBuffer = VK_NULL_HANDLE;
        dispatch.DestroyCommandPool(device, state->commandPool, nullptr);
        state->commandPool = VK_NULL_HANDLE;
        return false;
    }

    state->initialized = true;
    state->inFlight = false;
    state->lastSubmittedFrame = 0;
    return true;
}

void PollBridgeFrameInputs()
{
    {
        std::lock_guard<std::mutex> lock(g_pendingFrameMutex);
        if (g_pendingFrame.hasFrame)
        {
            return;
        }
    }

    TRBridgePluginFrameInputs inputs{};
    if (!TRBridge_ConsumePendingFrame(&inputs))
    {
        return;
    }

    PendingFrameState newFrame{};
    newFrame.hasFrame = true;
    newFrame.inputs = inputs;

    size_t interopCount = TRBridge_CopyInteropSurfaces(nullptr, 0);
    if (interopCount > 0)
    {
        newFrame.surfaces.resize(interopCount);
        TRBridge_CopyInteropSurfaces(newFrame.surfaces.data(), newFrame.surfaces.size());
    }

    TRBridgeInteropSyncState syncState{};
    if (TRBridge_GetInteropSyncState(&syncState) && syncState.frameId == inputs.frameId)
    {
        newFrame.syncState = syncState;
        newFrame.hasSyncState = true;
    }
    else
    {
        newFrame.hasSyncState = false;
    }

    Log("[TESR][Layer] Received plugin frame %" PRIu64 " for compute (targets=%u, surfaces=%zu)",
        static_cast<std::uint64_t>(inputs.frameId), inputs.renderTargetCount,
        static_cast<std::size_t>(newFrame.surfaces.size()));

    for (const auto& surface : newFrame.surfaces)
    {
        Log("[TESR][Layer]   Staged surface %s: %ux%u fmt=%u usage=0x%08x handles=%u",
            surface.descriptor.name, surface.descriptor.width, surface.descriptor.height,
            static_cast<unsigned>(surface.descriptor.format),
            static_cast<unsigned>(surface.descriptor.usageFlags), surface.handleCount);
        for (uint32_t i = 0; i < surface.handleCount; ++i)
        {
            const auto& handle = surface.handles[i];
            Log("[TESR][Layer]     -> %s 0x%016" PRIx64 " aux=0x%016" PRIx64,
                HandleTypeToString(handle.type),
                static_cast<std::uint64_t>(handle.value),
                static_cast<std::uint64_t>(handle.auxValue));
        }
    }

    if (newFrame.hasSyncState)
    {
        Log("[TESR][Layer]   Sync waits=%u signals=%u",
            static_cast<unsigned>(newFrame.syncState.waitHandleCount),
            static_cast<unsigned>(newFrame.syncState.signalHandleCount));
        for (uint32_t i = 0; i < newFrame.syncState.waitHandleCount; ++i)
        {
            const auto& handle = newFrame.syncState.waitHandles[i];
            Log("[TESR][Layer]     wait -> %s 0x%016" PRIx64 " aux=0x%016" PRIx64,
                HandleTypeToString(handle.type),
                static_cast<std::uint64_t>(handle.value),
                static_cast<std::uint64_t>(handle.auxValue));
        }
        for (uint32_t i = 0; i < newFrame.syncState.signalHandleCount; ++i)
        {
            const auto& handle = newFrame.syncState.signalHandles[i];
            Log("[TESR][Layer]     signal -> %s 0x%016" PRIx64 " aux=0x%016" PRIx64,
                HandleTypeToString(handle.type),
                static_cast<std::uint64_t>(handle.value),
                static_cast<std::uint64_t>(handle.auxValue));
        }
    }
    else
    {
        Log("[TESR][Layer]   No synchronization handles provided for frame %" PRIu64,
            static_cast<std::uint64_t>(inputs.frameId));
    }

    {
        std::lock_guard<std::mutex> lock(g_pendingFrameMutex);
        g_pendingFrame = std::move(newFrame);
    }
}

void DropPendingFrame(uint64_t frameId)
{
    std::lock_guard<std::mutex> lock(g_pendingFrameMutex);
    if (g_pendingFrame.hasFrame && g_pendingFrame.inputs.frameId == frameId)
    {
        g_pendingFrame.hasFrame = false;
        g_pendingFrame.surfaces.clear();
        g_pendingFrame.inputs = {};
        g_pendingFrame.hasSyncState = false;
        g_pendingFrame.syncState = {};
    }
}

bool TryInjectComputeWork(VkQueue queue, const QueueRegistration& queueInfo, DeviceDispatch& dispatch)
{
    if (queueInfo.device == VK_NULL_HANDLE)
    {
        return false;
    }

    TRBridgeConfiguration configuration{};
    TRBridge_GetConfiguration(&configuration);
    bool computeEnabled = (configuration.flags & TR_BRIDGE_FLAG_ENABLE_VULKAN_AO) != 0;

    PendingFrameState frame;
    {
        std::lock_guard<std::mutex> lock(g_pendingFrameMutex);
        if (!g_pendingFrame.hasFrame)
        {
            return false;
        }

        if (!computeEnabled)
        {
            g_pendingFrame.hasFrame = false;
            g_pendingFrame.surfaces.clear();
            g_pendingFrame.inputs = {};
            g_pendingFrame.hasSyncState = false;
            g_pendingFrame.syncState = {};
            return false;
        }
        frame = g_pendingFrame;
    }

    if (frame.hasSyncState)
    {
        Log("[TESR][Layer] Frame %" PRIu64 " includes %u waits and %u signals",
            static_cast<std::uint64_t>(frame.inputs.frameId),
            static_cast<unsigned>(frame.syncState.waitHandleCount),
            static_cast<unsigned>(frame.syncState.signalHandleCount));
    }

    if (!dispatch.HasComputeSupport())
    {
        if (!dispatch.computeFunctionsLoggedMissing)
        {
            Log("[TESR][Layer] Compute injection skipped; required device functions are unavailable");
            dispatch.computeFunctionsLoggedMissing = true;
        }
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    auto state = GetOrCreateQueueComputeState(queue);
    if (!EnsureQueueComputeState(queueInfo.device, queueInfo.queueFamilyIndex, dispatch, state))
    {
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    auto shaderBinary = LoadPlaceholderShaderBinary();
    if (!shaderBinary)
    {
        Log("[TESR][Layer] Placeholder compute shader missing; skipping frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    auto deviceState = GetOrCreateDeviceComputeState(queueInfo.device);
    if (!EnsureDeviceComputePipeline(queueInfo.device, dispatch, deviceState, *shaderBinary))
    {
        Log("[TESR][Layer] Failed to prepare compute pipeline; skipping frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    const TRBridgeInteropSurface* outputSurface = nullptr;
    for (const auto& surface : frame.surfaces)
    {
        if (std::strncmp(surface.descriptor.name, "TESR_RenderedTexture", sizeof(surface.descriptor.name)) == 0)
        {
            outputSurface = &surface;
            break;
        }
    }

    if (!outputSurface)
    {
        Log("[TESR][Layer] No rendered texture surface available for frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    VkImage outputImage = VK_NULL_HANDLE;
    VkImageView outputView = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < outputSurface->handleCount; ++i)
    {
        const auto& handle = outputSurface->handles[i];
        if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE)
        {
            outputImage = reinterpret_cast<VkImage>(handle.value);
        }
        else if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW)
        {
            outputView = reinterpret_cast<VkImageView>(handle.value);
        }
    }

    if (outputImage == VK_NULL_HANDLE || outputView == VK_NULL_HANDLE)
    {
        Log("[TESR][Layer] Missing Vulkan handles for rendered texture in frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    const TRBridgeInteropSurface* depthSurface = nullptr;
    for (const auto& surface : frame.surfaces)
    {
        if (std::strncmp(surface.descriptor.name, "TESR_DepthTexture", sizeof(surface.descriptor.name)) == 0)
        {
            depthSurface = &surface;
            break;
        }
    }

    if (!depthSurface)
    {
        for (const auto& surface : frame.surfaces)
        {
            if (std::strncmp(surface.descriptor.name, "TESR_MainDepthStencil", sizeof(surface.descriptor.name)) == 0)
            {
                depthSurface = &surface;
                break;
            }
        }
    }

    if (!depthSurface)
    {
        Log("[TESR][Layer] No depth surface available for frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    VkImage depthImage = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < depthSurface->handleCount; ++i)
    {
        const auto& handle = depthSurface->handles[i];
        if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE)
        {
            depthImage = reinterpret_cast<VkImage>(handle.value);
        }
        else if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_IMAGE_VIEW)
        {
            depthView = reinterpret_cast<VkImageView>(handle.value);
        }
    }

    if (depthImage == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE)
    {
        Log("[TESR][Layer] Missing Vulkan handles for depth texture in frame %" PRIu64,
            static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    VkImageAspectFlags depthAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depthSurface->descriptor.format == TR_BRIDGE_FORMAT_D24_UNORM_S8_UINT)
    {
        depthAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    const uint32_t width = outputSurface->descriptor.width;
    const uint32_t height = outputSurface->descriptor.height;
    if (width == 0 || height == 0)
    {
        Log("[TESR][Layer] Rendered texture has invalid dimensions (%ux%u) for frame %" PRIu64,
            width, height, static_cast<std::uint64_t>(frame.inputs.frameId));
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    VkResult resetResult = VK_SUCCESS;
    VkResult beginResult = VK_SUCCESS;
    VkResult endResult = VK_SUCCESS;

    {
        std::lock_guard<std::mutex> deviceLock(deviceState->mutex);
        std::lock_guard<std::mutex> stateLock(state->mutex);

        resetResult = dispatch.ResetCommandPool(queueInfo.device, state->commandPool, 0);
        if (resetResult != VK_SUCCESS)
        {
            Log("[TESR][Layer] Failed to reset compute command pool (%d)", resetResult);
            DropPendingFrame(frame.inputs.frameId);
            return false;
        }

        if (deviceState->sampler == VK_NULL_HANDLE)
        {
            Log("[TESR][Layer] Compute sampler unavailable for frame %" PRIu64,
                static_cast<std::uint64_t>(frame.inputs.frameId));
            DropPendingFrame(frame.inputs.frameId);
            return false;
        }

        VkDescriptorImageInfo outputInfo{};
        outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputInfo.imageView = outputView;
        outputInfo.sampler = VK_NULL_HANDLE;

        VkDescriptorImageInfo depthInfo{};
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthInfo.imageView = depthView;
        depthInfo.sampler = deviceState->sampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = deviceState->descriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo = &outputInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = deviceState->descriptorSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &depthInfo;

        dispatch.UpdateDescriptorSets(queueInfo.device,
                                      static_cast<uint32_t>(writes.size()),
                                      writes.data(),
                                      0,
                                      nullptr);
        deviceState->descriptorSetValid = true;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        beginResult = dispatch.BeginCommandBuffer(state->commandBuffer, &beginInfo);
        if (beginResult != VK_SUCCESS)
        {
            Log("[TESR][Layer] Failed to begin compute command buffer (%d)", beginResult);
            DropPendingFrame(frame.inputs.frameId);
            return false;
        }

        std::array<VkImageMemoryBarrier, 2> acquireBarriers{};
        uint32_t acquireCount = 0;

        VkImageMemoryBarrier outputAcquire{};
        outputAcquire.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        outputAcquire.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        outputAcquire.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        outputAcquire.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        outputAcquire.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputAcquire.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        outputAcquire.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        outputAcquire.image = outputImage;
        outputAcquire.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        outputAcquire.subresourceRange.baseMipLevel = 0;
        outputAcquire.subresourceRange.levelCount = 1;
        outputAcquire.subresourceRange.baseArrayLayer = 0;
        outputAcquire.subresourceRange.layerCount = 1;
        acquireBarriers[acquireCount++] = outputAcquire;

        VkPipelineStageFlags acquireSrcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkImageMemoryBarrier depthAcquire{};
        if (depthImage != VK_NULL_HANDLE)
        {
            depthAcquire.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthAcquire.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthAcquire.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            depthAcquire.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAcquire.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            depthAcquire.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthAcquire.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthAcquire.image = depthImage;
            depthAcquire.subresourceRange.aspectMask = depthAspectMask;
            depthAcquire.subresourceRange.baseMipLevel = 0;
            depthAcquire.subresourceRange.levelCount = 1;
            depthAcquire.subresourceRange.baseArrayLayer = 0;
            depthAcquire.subresourceRange.layerCount = 1;
            acquireBarriers[acquireCount++] = depthAcquire;
            acquireSrcStages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }

        dispatch.CmdPipelineBarrier(state->commandBuffer,
                                    acquireSrcStages,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    0,
                                    0, nullptr,
                                    0, nullptr,
                                    acquireCount,
                                    acquireBarriers.data());

        dispatch.CmdBindPipeline(state->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, deviceState->pipeline);
        dispatch.CmdBindDescriptorSets(state->commandBuffer,
                                       VK_PIPELINE_BIND_POINT_COMPUTE,
                                       deviceState->pipelineLayout,
                                       0,
                                       1,
                                       &deviceState->descriptorSet,
                                       0,
                                       nullptr);

        const uint32_t groupSizeX = (width + 7u) / 8u;
        const uint32_t groupSizeY = (height + 7u) / 8u;
        dispatch.CmdDispatch(state->commandBuffer, groupSizeX == 0 ? 1 : groupSizeX, groupSizeY == 0 ? 1 : groupSizeY, 1);

        std::array<VkImageMemoryBarrier, 2> releaseBarriers{};
        uint32_t releaseCount = 0;

        VkImageMemoryBarrier outputRelease{};
        outputRelease.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        outputRelease.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        outputRelease.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        outputRelease.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputRelease.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        outputRelease.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        outputRelease.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        outputRelease.image = outputImage;
        outputRelease.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        outputRelease.subresourceRange.baseMipLevel = 0;
        outputRelease.subresourceRange.levelCount = 1;
        outputRelease.subresourceRange.baseArrayLayer = 0;
        outputRelease.subresourceRange.layerCount = 1;
        releaseBarriers[releaseCount++] = outputRelease;

        VkPipelineStageFlags releaseDstStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkImageMemoryBarrier depthRelease{};
        if (depthImage != VK_NULL_HANDLE)
        {
            depthRelease.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthRelease.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            depthRelease.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthRelease.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            depthRelease.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthRelease.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthRelease.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthRelease.image = depthImage;
            depthRelease.subresourceRange.aspectMask = depthAspectMask;
            depthRelease.subresourceRange.baseMipLevel = 0;
            depthRelease.subresourceRange.levelCount = 1;
            depthRelease.subresourceRange.baseArrayLayer = 0;
            depthRelease.subresourceRange.layerCount = 1;
            releaseBarriers[releaseCount++] = depthRelease;
            releaseDstStages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }

        dispatch.CmdPipelineBarrier(state->commandBuffer,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    releaseDstStages,
                                    0,
                                    0, nullptr,
                                    0, nullptr,
                                    releaseCount,
                                    releaseBarriers.data());

        endResult = dispatch.EndCommandBuffer(state->commandBuffer);
        if (endResult != VK_SUCCESS)
        {
            Log("[TESR][Layer] Failed to end compute command buffer (%d)", endResult);
            DropPendingFrame(frame.inputs.frameId);
            return false;
        }
    }

    std::vector<VkSemaphore> waitSemaphores;
    std::vector<uint64_t> waitValues;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> signalSemaphores;
    std::vector<uint64_t> signalValues;
    bool hasTimelineWait = false;
    bool hasTimelineSignal = false;
    VkFence externalFence = VK_NULL_HANDLE;

    auto decodeTimelineValue = [](uint64_t auxValue, bool& hasTimeline) -> uint64_t {
        if (auxValue & TR_BRIDGE_HANDLE_AUX_TIMELINE_BIT)
        {
            hasTimeline = true;
            return auxValue & TR_BRIDGE_HANDLE_AUX_VALUE_MASK;
        }
        if (auxValue != 0)
        {
            return auxValue;
        }
        return 0;
    };

    if (frame.hasSyncState)
    {
        for (uint32_t i = 0; i < frame.syncState.waitHandleCount; ++i)
        {
            const auto& handle = frame.syncState.waitHandles[i];
            if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_SEMAPHORE && handle.value != 0)
            {
                waitSemaphores.push_back(reinterpret_cast<VkSemaphore>(handle.value));
                waitValues.push_back(decodeTimelineValue(handle.auxValue, hasTimelineWait));
                waitStages.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            }
        }

        for (uint32_t i = 0; i < frame.syncState.signalHandleCount; ++i)
        {
            const auto& handle = frame.syncState.signalHandles[i];
            if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_SEMAPHORE && handle.value != 0)
            {
                signalSemaphores.push_back(reinterpret_cast<VkSemaphore>(handle.value));
                signalValues.push_back(decodeTimelineValue(handle.auxValue, hasTimelineSignal));
            }
            else if (handle.type == TR_BRIDGE_INTEROP_HANDLE_VK_FENCE && handle.value != 0)
            {
                externalFence = reinterpret_cast<VkFence>(handle.value);
            }
        }
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffer;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.empty() ? nullptr : waitStages.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.empty() ? nullptr : signalSemaphores.data();

    VkTimelineSemaphoreSubmitInfo timelineInfo{};
    if (hasTimelineWait || hasTimelineSignal)
    {
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.waitSemaphoreValueCount = static_cast<uint32_t>(waitValues.size());
        timelineInfo.pWaitSemaphoreValues = waitValues.empty() ? nullptr : waitValues.data();
        timelineInfo.signalSemaphoreValueCount = static_cast<uint32_t>(signalValues.size());
        timelineInfo.pSignalSemaphoreValues = signalValues.empty() ? nullptr : signalValues.data();
        submitInfo.pNext = &timelineInfo;
    }

    VkFence submissionFence = externalFence != VK_NULL_HANDLE ? externalFence : state->fence;

    VkResult submitResult = dispatch.QueueSubmit(queue, 1, &submitInfo, submissionFence);
    if (submitResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Failed to submit compute workload (%d)", submitResult);
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    VkResult waitResult = dispatch.WaitForFences(queueInfo.device, 1, &submissionFence, VK_TRUE, UINT64_MAX);
    if (waitResult != VK_SUCCESS)
    {
        Log("[TESR][Layer] Compute fence wait failed (%d)", waitResult);
        DropPendingFrame(frame.inputs.frameId);
        return false;
    }

    if (externalFence == VK_NULL_HANDLE)
    {
        dispatch.ResetFences(queueInfo.device, 1, &submissionFence);
    }

    state->inFlight = false;
    state->lastSubmittedFrame = frame.inputs.frameId;

    DropPendingFrame(frame.inputs.frameId);

    Log("[TESR][Layer] Enqueued placeholder compute workload for frame %" PRIu64,
        static_cast<std::uint64_t>(frame.inputs.frameId));
    return true;
}

void InitInstanceDispatch(VkInstance instance)
{
    InstanceDispatch dispatch{};
    dispatch.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(g_nextGetInstanceProcAddr(instance, "vkDestroyInstance"));
    dispatch.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(g_nextGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    dispatch.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(g_nextGetInstanceProcAddr(instance, "vkCreateDevice"));

    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    g_instanceDispatch.emplace(instance, dispatch);
}

void InitDeviceDispatch(VkDevice device)
{
    DeviceDispatch dispatch{};
    dispatch.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(g_nextGetDeviceProcAddr(device, "vkDestroyDevice"));
    dispatch.GetDeviceProcAddr = g_nextGetDeviceProcAddr;
    dispatch.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(g_nextGetDeviceProcAddr(device, "vkGetDeviceQueue"));
    dispatch.GetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(g_nextGetDeviceProcAddr(device, "vkGetDeviceQueue2"));
    dispatch.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(g_nextGetDeviceProcAddr(device, "vkQueueSubmit"));
    dispatch.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(g_nextGetDeviceProcAddr(device, "vkQueuePresentKHR"));
    dispatch.CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(g_nextGetDeviceProcAddr(device, "vkCreateCommandPool"));
    dispatch.DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(g_nextGetDeviceProcAddr(device, "vkDestroyCommandPool"));
    dispatch.ResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(g_nextGetDeviceProcAddr(device, "vkResetCommandPool"));
    dispatch.AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(g_nextGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    dispatch.FreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(g_nextGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    dispatch.BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(g_nextGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    dispatch.EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(g_nextGetDeviceProcAddr(device, "vkEndCommandBuffer"));
    dispatch.CreateFence = reinterpret_cast<PFN_vkCreateFence>(g_nextGetDeviceProcAddr(device, "vkCreateFence"));
    dispatch.DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(g_nextGetDeviceProcAddr(device, "vkDestroyFence"));
    dispatch.WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(g_nextGetDeviceProcAddr(device, "vkWaitForFences"));
    dispatch.ResetFences = reinterpret_cast<PFN_vkResetFences>(g_nextGetDeviceProcAddr(device, "vkResetFences"));
    dispatch.CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(g_nextGetDeviceProcAddr(device, "vkCreateShaderModule"));
    dispatch.DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(g_nextGetDeviceProcAddr(device, "vkDestroyShaderModule"));
    dispatch.CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(g_nextGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout"));
    dispatch.DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(g_nextGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout"));
    dispatch.CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(g_nextGetDeviceProcAddr(device, "vkCreatePipelineLayout"));
    dispatch.DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(g_nextGetDeviceProcAddr(device, "vkDestroyPipelineLayout"));
    dispatch.CreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(g_nextGetDeviceProcAddr(device, "vkCreateComputePipelines"));
    dispatch.DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(g_nextGetDeviceProcAddr(device, "vkDestroyPipeline"));
    dispatch.CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(g_nextGetDeviceProcAddr(device, "vkCreateDescriptorPool"));
    dispatch.DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(g_nextGetDeviceProcAddr(device, "vkDestroyDescriptorPool"));
    dispatch.AllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(g_nextGetDeviceProcAddr(device, "vkAllocateDescriptorSets"));
    dispatch.FreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(g_nextGetDeviceProcAddr(device, "vkFreeDescriptorSets"));
    dispatch.UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(g_nextGetDeviceProcAddr(device, "vkUpdateDescriptorSets"));
    dispatch.CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(g_nextGetDeviceProcAddr(device, "vkCreateSampler"));
    dispatch.DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(g_nextGetDeviceProcAddr(device, "vkDestroySampler"));
    dispatch.CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(g_nextGetDeviceProcAddr(device, "vkCmdBindPipeline"));
    dispatch.CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(g_nextGetDeviceProcAddr(device, "vkCmdBindDescriptorSets"));
    dispatch.CmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(g_nextGetDeviceProcAddr(device, "vkCmdDispatch"));
    dispatch.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(g_nextGetDeviceProcAddr(device, "vkCmdPipelineBarrier"));

    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    g_deviceDispatch.emplace(device, dispatch);
}

VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                      const VkAllocationCallbacks* pAllocator,
                                                      VkInstance* pInstance)
{
    Log("[TESR][Layer] vkCreateInstance intercept");

    TRBridge_Initialize();

    auto* chainInfo = GetInstanceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);
    assert(chainInfo != nullptr);

    auto* layerInfo = chainInfo->u.layerInfo.pLayerInfo;
    assert(layerInfo != nullptr);
    g_nextGetInstanceProcAddr = layerInfo->pfnNextGetInstanceProcAddr;
    chainInfo->u.layerInfo.pLayerInfo = layerInfo->pNext;

    auto fpCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(g_nextGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    VkResult result = fpCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result == VK_SUCCESS)
    {
        InitInstanceDispatch(*pInstance);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    Log("[TESR][Layer] vkDestroyInstance intercept");
    auto& table = GetInstanceDispatch(instance);
    table.DestroyInstance(instance, pAllocator);
    RemoveInstance(instance);
    TRBridge_Shutdown();
}

VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateDevice(VkPhysicalDevice physicalDevice,
                                                    const VkDeviceCreateInfo* pCreateInfo,
                                                    const VkAllocationCallbacks* pAllocator,
                                                    VkDevice* pDevice)
{
    Log("[TESR][Layer] vkCreateDevice intercept");

    TRBridge_MarkLayerHeartbeat();

    auto* chainInfo = GetDeviceChainInfo(pCreateInfo, VK_LAYER_LINK_INFO);
    assert(chainInfo != nullptr);

    auto* layerInfo = chainInfo->u.layerInfo.pLayerInfo;
    assert(layerInfo != nullptr);
    g_nextGetInstanceProcAddr = layerInfo->pfnNextGetInstanceProcAddr;
    g_nextGetDeviceProcAddr = layerInfo->pfnNextGetDeviceProcAddr;
    chainInfo->u.layerInfo.pLayerInfo = layerInfo->pNext;

    auto fpCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(g_nextGetInstanceProcAddr(nullptr, "vkCreateDevice"));

    VkResult result = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS)
    {
        InitDeviceDispatch(*pDevice);
    }
    return result;
}

VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    Log("[TESR][Layer] vkDestroyDevice intercept");
    auto& table = GetDeviceDispatch(device);
    ReleaseDeviceQueues(device, table);
    auto computeState = ExtractDeviceComputeState(device);
    DestroyDeviceComputeState(device, table, computeState);
    table.DestroyDevice(device, pAllocator);
    RemoveDevice(device);
}

VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue(VkDevice device,
                                                  uint32_t queueFamilyIndex,
                                                  uint32_t queueIndex,
                                                  VkQueue* pQueue)
{
    auto& table = GetDeviceDispatch(device);
    table.GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    RegisterQueue(device, *pQueue, queueFamilyIndex);
}

VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    auto& table = GetDeviceDispatch(device);
    if (table.GetDeviceQueue2)
    {
        table.GetDeviceQueue2(device, pQueueInfo, pQueue);
        RegisterQueue(device, *pQueue, pQueueInfo->queueFamilyIndex);
    }
    else
    {
        table.GetDeviceQueue(device, pQueueInfo->queueFamilyIndex, pQueueInfo->queueIndex, pQueue);
        RegisterQueue(device, *pQueue, pQueueInfo->queueFamilyIndex);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL Layer_vkQueueSubmit(VkQueue queue,
                                                   uint32_t submitCount,
                                                   const VkSubmitInfo* pSubmits,
                                                   VkFence fence)
{
    Log("[TESR][Layer] vkQueueSubmit intercept (queue=%p, submits=%u)", static_cast<void*>(queue), submitCount);
    TRBridge_MarkLayerHeartbeat();

    PollBridgeFrameInputs();

    QueueRegistration queueInfo = GetQueueRegistration(queue);
    VkDevice device = queueInfo.device;
    if (device == VK_NULL_HANDLE)
    {
        Log("[TESR][Layer] queue submit missing owner; forwarding");
        auto fpQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
            g_nextGetDeviceProcAddr != nullptr ? g_nextGetDeviceProcAddr(nullptr, "vkQueueSubmit") : nullptr);
        if (fpQueueSubmit == nullptr)
        {
            return VK_ERROR_DEVICE_LOST;
        }
        VkResult fallback = fpQueueSubmit(queue, submitCount, pSubmits, fence);
        return fallback;
    }

    auto& table = GetDeviceDispatch(device);
    VkResult result = table.QueueSubmit(queue, submitCount, pSubmits, fence);
    if (result == VK_SUCCESS)
    {
        TryInjectComputeWork(queue, queueInfo, table);
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL Layer_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    Log("[TESR][Layer] vkQueuePresentKHR intercept (queue=%p)", static_cast<void*>(queue));
    TRBridge_MarkLayerHeartbeat();

    PollBridgeFrameInputs();

    QueueRegistration queueInfo = GetQueueRegistration(queue);
    VkDevice device = queueInfo.device;
    if (device == VK_NULL_HANDLE)
    {
        Log("[TESR][Layer] queue present missing owner; forwarding");
        auto fpQueuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(
            g_nextGetDeviceProcAddr != nullptr ? g_nextGetDeviceProcAddr(nullptr, "vkQueuePresentKHR") : nullptr);
        if (fpQueuePresent == nullptr)
        {
            return VK_ERROR_DEVICE_LOST;
        }
        return fpQueuePresent(queue, pPresentInfo);
    }

    auto& table = GetDeviceDispatch(device);
    TryInjectComputeWork(queue, queueInfo, table);
    return table.QueuePresentKHR(queue, pPresentInfo);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Layer_vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    if (std::strcmp(pName, "vkDestroyDevice") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroyDevice);
    }
    if (std::strcmp(pName, "vkGetDeviceQueue") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue);
    }
    if (std::strcmp(pName, "vkGetDeviceQueue2") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue2);
    }
    if (std::strcmp(pName, "vkQueueSubmit") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkQueueSubmit);
    }
    if (std::strcmp(pName, "vkQueuePresentKHR") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkQueuePresentKHR);
    }

    if (g_nextGetDeviceProcAddr)
    {
        return g_nextGetDeviceProcAddr(device, pName);
    }
    return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Layer_vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    if (std::strcmp(pName, "vkGetInstanceProcAddr") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetInstanceProcAddr);
    }
    if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceProcAddr);
    }
    if (std::strcmp(pName, "vkCreateInstance") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkCreateInstance);
    }
    if (std::strcmp(pName, "vkDestroyInstance") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroyInstance);
    }
    if (std::strcmp(pName, "vkCreateDevice") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkCreateDevice);
    }

    if (g_nextGetInstanceProcAddr)
    {
        return g_nextGetInstanceProcAddr(instance, pName);
    }
    return nullptr;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                                                  VkLayerProperties* pProperties)
{
    if (pProperties == nullptr)
    {
        *pPropertyCount = 1;
        return VK_SUCCESS;
    }

    if (*pPropertyCount < 1)
    {
        return VK_INCOMPLETE;
    }

    VkLayerProperties props{};
    std::strncpy(props.layerName, kLayerName, sizeof(props.layerName) - 1);
    props.specVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);
    props.implementationVersion = 1;
    std::strncpy(props.description, "TESReloaded implicit Vulkan layer", sizeof(props.description) - 1);

    pProperties[0] = props;
    *pPropertyCount = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice /*physicalDevice*/,
                                                                uint32_t* pPropertyCount,
                                                                VkLayerProperties* pProperties)
{
    return vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char* /*layerName*/,
                                                                      uint32_t* pPropertyCount,
                                                                      VkExtensionProperties* pProperties)
{
    if (pProperties == nullptr)
    {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }

    *pPropertyCount = 0;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice /*physicalDevice*/,
                                                                    const char* /*layerName*/,
                                                                    uint32_t* pPropertyCount,
                                                                    VkExtensionProperties* pProperties)
{
    return vkEnumerateInstanceExtensionProperties(nullptr, pPropertyCount, pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct)
{
    if (pVersionStruct == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (pVersionStruct->loaderLayerInterfaceVersion < 2)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pVersionStruct->loaderLayerInterfaceVersion = 2;
    pVersionStruct->pfnGetInstanceProcAddr = &Layer_vkGetInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = &Layer_vkGetDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;

    return VK_SUCCESS;
}

} // namespace
} // namespace tesreloaded::vulkan

extern "C"
{
VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    return tesreloaded::vulkan::Layer_vkGetInstanceProcAddr(instance, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    return tesreloaded::vulkan::Layer_vkGetDeviceProcAddr(device, pName);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                     const VkAllocationCallbacks* pAllocator,
                                                     VkInstance* pInstance)
{
    return tesreloaded::vulkan::Layer_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

VK_LAYER_EXPORT void VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    tesreloaded::vulkan::Layer_vkDestroyInstance(instance, pAllocator);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice,
                                                   const VkDeviceCreateInfo* pCreateInfo,
                                                   const VkAllocationCallbacks* pAllocator,
                                                   VkDevice* pDevice)
{
    return tesreloaded::vulkan::Layer_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

VK_LAYER_EXPORT void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    tesreloaded::vulkan::Layer_vkDestroyDevice(device, pAllocator);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                                                       VkLayerProperties* pProperties)
{
    return tesreloaded::vulkan::vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                                                     uint32_t* pPropertyCount,
                                                                     VkLayerProperties* pProperties)
{
    return tesreloaded::vulkan::vkEnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char* layerName,
                                                                           uint32_t* pPropertyCount,
                                                                           VkExtensionProperties* pProperties)
{
    return tesreloaded::vulkan::vkEnumerateInstanceExtensionProperties(layerName, pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                         const char* layerName,
                                                                         uint32_t* pPropertyCount,
                                                                         VkExtensionProperties* pProperties)
{
    return tesreloaded::vulkan::vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct)
{
    return tesreloaded::vulkan::vkNegotiateLoaderLayerInterfaceVersion(pVersionStruct);
}
}

