#include "VulkanEffect.h"

IVulkanEffect::~IVulkanEffect()
{
    DestroyResources();
}

void IVulkanEffect::Initialize()
{
    CreateResources();
}

void IVulkanEffect::OnGameBuffersUpdated()
{
    CreateDescriptorSets();
}

void IVulkanEffect::CreateResources()
{
    CreateShaderModule();
    CreateInteropTextures();
    CreatePipeline();
    CreateDescriptorSets();
    CreateCommandBuffer();
    CreateFence();
}

void IVulkanEffect::DestroyResources()
{
    FVulkanContext& Vulkan = TheVulkanEffectsManager->VulkanContext;

    if (EffectDescriptorPool != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorPool(Vulkan.Device, EffectDescriptorPool, nullptr);
        EffectDescriptorPool = VK_NULL_HANDLE;
    }

    if (EffectDescriptorSetLayout != VK_NULL_HANDLE) {
        p_vkDestroyDescriptorSetLayout(Vulkan.Device, EffectDescriptorSetLayout, nullptr);
        EffectDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (EffectPipeline != VK_NULL_HANDLE) {
        p_vkDestroyPipeline(Vulkan.Device, EffectPipeline, nullptr);
        EffectPipeline = VK_NULL_HANDLE;
    }

    if (EffectPipelineLayout != VK_NULL_HANDLE) {
        p_vkDestroyPipelineLayout(Vulkan.Device, EffectPipelineLayout, nullptr);
        EffectPipelineLayout = VK_NULL_HANDLE;
    }

    if (EffectShaderModule != VK_NULL_HANDLE) {
        p_vkDestroyShaderModule(Vulkan.Device, EffectShaderModule, nullptr);
        EffectShaderModule = VK_NULL_HANDLE;
    }

    if (EffectCommandBuffer != VK_NULL_HANDLE)
    {
        p_vkFreeCommandBuffers(Vulkan.Device, Vulkan.CmdPool, 1, &EffectCommandBuffer);
    }

    if (EffectFence != VK_NULL_HANDLE)
    {
        p_vkDestroyFence(Vulkan.Device, EffectFence, nullptr);
    }
}

void IVulkanEffect::CreateCommandBuffer()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    CommandBufferAllocateInfo.commandPool = VULKAN_CONTEXT.CmdPool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandBufferCount = 1;

    VkResult Vr = p_vkAllocateCommandBuffers(VULKAN_CONTEXT.Device, &CommandBufferAllocateInfo, &EffectCommandBuffer);
    if (Vr != VK_SUCCESS || !EffectCommandBuffer) {
        Logger::Log("DebugRunOnNvrCombinedDepth: vkAllocateCommandBuffers failed rv=%d cmd=%p", Vr, EffectCommandBuffer);
        return;
    }
}

void IVulkanEffect::CreateFence()
{
    if (EffectFence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkResult vr = p_vkCreateFence(VULKAN_CONTEXT.Device, &fi, nullptr, &EffectFence);
        if (vr != VK_SUCCESS) {
            Logger::Log("FVulkanCombineDepthEffect: vkCreateFence failed rv=%d", vr);
            EffectFence = VK_NULL_HANDLE;
        }
    }
}

void IVulkanEffect::LoadShaderModule()
{
    DXVK_CheckReturn()

    // Load SPIR-V from disk
    std::vector<uint32_t> Spirv;
    {
        std::ifstream File(SpirvPath,
            std::ios::binary | std::ios::ate);
        if (!File) {
            Logger::Log("IVulkanEffect: could not open %s", SpirvPath);
            return;
        }

        std::streamsize Size = File.tellg();
        File.seekg(0, std::ios::beg);
        Spirv.resize(Size / sizeof(uint32_t));

        if (!File.read(reinterpret_cast<char*>(Spirv.data()), Size)) {
            Logger::Log("IVulkanEffect: failed to read %s", SpirvPath);
            Spirv.clear();
            return;
        }
    }

    if (Spirv.empty()) {
        Logger::Log("IVulkanEffect: failed to read %s, SPIR-V is empty", SpirvPath);
        return;
    }

    VkShaderModuleCreateInfo Info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    Info.codeSize = Spirv.size() * sizeof(uint32_t);
    Info.pCode = Spirv.data();

    VK_CHECK(p_vkCreateShaderModule(VULKAN_CONTEXT.Device, &Info, nullptr, &EffectShaderModule),
        "vkCreateShaderModule");
}
