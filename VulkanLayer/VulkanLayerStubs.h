#pragma once

#include <cstddef>
#include <cstdint>

#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#else
#ifndef TESR_VULKAN_CORE_STUB
#define TESR_VULKAN_CORE_STUB

#if defined(_WIN32)
#define VKAPI_ATTR
#define VKAPI_CALL __stdcall
#define VKAPI_PTR VKAPI_CALL
#define VK_LAYER_EXPORT __declspec(dllexport)
#else
#define VKAPI_ATTR
#define VKAPI_CALL
#define VKAPI_PTR
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#endif

#ifndef VK_MAKE_API_VERSION
#define VK_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))
#endif

#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif

using VkFlags = uint32_t;
using VkBool32 = uint32_t;
using VkDeviceSize = uint64_t;
using VkSampleMask = uint32_t;
using VkStructureType = uint32_t;
using VkResult = int32_t;
using VkSystemAllocationScope = uint32_t;
using VkInternalAllocationType = uint32_t;
using VkDeviceQueueCreateFlags = uint32_t;
using VkInstanceCreateFlags = uint32_t;
using VkDeviceCreateFlags = uint32_t;
using VkPipelineStageFlags = uint32_t;

static constexpr VkResult VK_SUCCESS = 0;
static constexpr VkResult VK_INCOMPLETE = 5;
static constexpr VkResult VK_ERROR_INITIALIZATION_FAILED = -3;
static constexpr VkResult VK_ERROR_DEVICE_LOST = -4;

static constexpr VkStructureType VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
static constexpr VkStructureType VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;
static constexpr VkStructureType VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
static constexpr VkStructureType VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8;
static constexpr VkStructureType VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 = 1000211003;
static constexpr VkStructureType VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO = 1000008000;
static constexpr VkStructureType VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO = 1000008001;
static constexpr VkStructureType VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 28;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
static constexpr VkStructureType VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 41;

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_HANDLE(VkCommandBuffer)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSemaphore)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFence)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSwapchainKHR)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkCommandPool)

struct VkAllocationCallbacks;
struct VkApplicationInfo;

struct VkDeviceQueueCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceQueueCreateFlags flags;
    uint32_t queueFamilyIndex;
    uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkInstanceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkInstanceCreateFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkDeviceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceCreateFlags flags;
    uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

struct VkDeviceQueueInfo2
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceQueueCreateFlags flags;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
    float queuePriority;
};

struct VkSubmitInfo
{
    VkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    const VkPipelineStageFlags* pWaitDstStageMask;
    uint32_t commandBufferCount;
    const VkCommandBuffer* pCommandBuffers;
    uint32_t signalSemaphoreCount;
    const VkSemaphore* pSignalSemaphores;
};

struct VkPresentInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    uint32_t swapchainCount;
    const VkSwapchainKHR* pSwapchains;
    const uint32_t* pImageIndices;
    VkResult* pResults;
};

using VkCommandPoolCreateFlags = VkFlags;
constexpr VkCommandPoolCreateFlags VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002;

struct VkCommandPoolCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandPoolCreateFlags flags;
    uint32_t queueFamilyIndex;
};

enum VkCommandBufferLevel
{
    VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0,
    VK_COMMAND_BUFFER_LEVEL_SECONDARY = 1,
};

using VkCommandBufferUsageFlags = VkFlags;
constexpr VkCommandBufferUsageFlags VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001;

struct VkCommandBufferAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandPool commandPool;
    VkCommandBufferLevel level;
    uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandBufferUsageFlags flags;
    const void* pInheritanceInfo;
};

using VkFenceCreateFlags = VkFlags;

struct VkFenceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFenceCreateFlags flags;
};

constexpr uint32_t VK_MAX_EXTENSION_NAME_SIZE = 256;
constexpr uint32_t VK_MAX_DESCRIPTION_SIZE = 256;

struct VkExtensionProperties
{
    char extensionName[VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
};

struct VkLayerProperties
{
    char layerName[VK_MAX_EXTENSION_NAME_SIZE];
    uint32_t specVersion;
    uint32_t implementationVersion;
    char description[VK_MAX_DESCRIPTION_SIZE];
};

constexpr VkBool32 VK_FALSE = 0;
constexpr VkBool32 VK_TRUE = 1;

using PFN_vkVoidFunction = void (*)(void);
using PFN_vkGetInstanceProcAddr = PFN_vkVoidFunction(VKAPI_PTR*)(VkInstance instance, const char* pName);
using PFN_vkGetDeviceProcAddr = PFN_vkVoidFunction(VKAPI_PTR*)(VkDevice device, const char* pName);
using PFN_vkGetPhysicalDeviceProcAddr = PFN_vkVoidFunction(VKAPI_PTR*)(VkInstance instance, const char* pName);
using PFN_vkCreateInstance = VkResult(VKAPI_PTR*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
using PFN_vkDestroyInstance = void(VKAPI_PTR*)(VkInstance, const VkAllocationCallbacks*);
using PFN_vkCreateDevice = VkResult(VKAPI_PTR*)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
using PFN_vkDestroyDevice = void(VKAPI_PTR*)(VkDevice, const VkAllocationCallbacks*);
using PFN_vkGetDeviceQueue = void(VKAPI_PTR*)(VkDevice, uint32_t, uint32_t, VkQueue*);
using PFN_vkGetDeviceQueue2 = void(VKAPI_PTR*)(VkDevice, const VkDeviceQueueInfo2*, VkQueue*);
using PFN_vkQueueSubmit = VkResult(VKAPI_PTR*)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
using PFN_vkQueuePresentKHR = VkResult(VKAPI_PTR*)(VkQueue, const VkPresentInfoKHR*);
using PFN_vkCreateCommandPool = VkResult(VKAPI_PTR*)(VkDevice, const struct VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool*);
using PFN_vkDestroyCommandPool = void(VKAPI_PTR*)(VkDevice, VkCommandPool, const VkAllocationCallbacks*);
using PFN_vkResetCommandPool = VkResult(VKAPI_PTR*)(VkDevice, VkCommandPool, VkFlags);
using PFN_vkAllocateCommandBuffers = VkResult(VKAPI_PTR*)(VkDevice, const struct VkCommandBufferAllocateInfo*, VkCommandBuffer*);
using PFN_vkFreeCommandBuffers = void(VKAPI_PTR*)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
using PFN_vkBeginCommandBuffer = VkResult(VKAPI_PTR*)(VkCommandBuffer, const struct VkCommandBufferBeginInfo*);
using PFN_vkEndCommandBuffer = VkResult(VKAPI_PTR*)(VkCommandBuffer);
using PFN_vkCreateFence = VkResult(VKAPI_PTR*)(VkDevice, const struct VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence*);
using PFN_vkDestroyFence = void(VKAPI_PTR*)(VkDevice, VkFence, const VkAllocationCallbacks*);
using PFN_vkWaitForFences = VkResult(VKAPI_PTR*)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
using PFN_vkResetFences = VkResult(VKAPI_PTR*)(VkDevice, uint32_t, const VkFence*);

#endif // TESR_VULKAN_CORE_STUB
#endif // __has_include(<vulkan/vulkan.h>)

#if __has_include(<vulkan/vk_layer.h>)
#include <vulkan/vk_layer.h>
#else
#ifndef TESR_VK_LAYER_STUB
#define TESR_VK_LAYER_STUB

enum VkLayerFunction
{
    VK_LAYER_FUNCTION_BEGIN_RANGE = 0,
    VK_LAYER_LINK_INFO = 0,
    VK_LAYER_FUNCTION_END_RANGE = 1,
    VK_LAYER_FUNCTION_MAX_ENUM = 0x7FFFFFFF
};

struct VkLayerInstanceLink
{
    VkLayerInstanceLink* pNext;
    PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
    PFN_vkGetPhysicalDeviceProcAddr pfnNextGetPhysicalDeviceProcAddr;
};

struct VkLayerDeviceLink
{
    VkLayerDeviceLink* pNext;
    PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
};

struct VkLayerInstanceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkLayerFunction function;
    union
    {
        struct
        {
            VkLayerInstanceLink* pLayerInfo;
        } layerInfo;
        struct
        {
            void* pUserData;
        } loaderData;
    } u;
};

struct VkLayerDeviceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkLayerFunction function;
    union
    {
        struct
        {
            VkLayerDeviceLink* pLayerInfo;
        } layerInfo;
        struct
        {
            void* pUserData;
        } loaderData;
    } u;
};

struct VkNegotiateLayerInterface
{
    VkStructureType sType;
    void* pNext;
    uint32_t loaderLayerInterfaceVersion;
    PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr;
    PFN_vkGetPhysicalDeviceProcAddr pfnGetPhysicalDeviceProcAddr;
};

#endif // TESR_VK_LAYER_STUB
#endif // __has_include(<vulkan/vk_layer.h>)

