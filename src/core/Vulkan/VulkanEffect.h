#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "VulkanContext.h"
#include "VulkanInteropManager.h"


enum class EVulkanEffectPhase : uint8_t
{
    PreTonemap,
    PostTonemap,
};

class IVulkanEffect
{
public:
    virtual ~IVulkanEffect();

    virtual const char* GetName() const = 0;
    virtual EVulkanEffectPhase GetPhase() const = 0;

    virtual void Initialize();

    // Different phases if you need them
    virtual void SubmitRendering() {}

    virtual void CompleteRendering(IDirect3DSurface9* SceneColor) {}

    virtual void OnGameBuffersUpdated();

    const char* SpirvPath = "";

protected:

    // Vulkan pipeline objects
    VkShaderModule        EffectShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout      EffectPipelineLayout = VK_NULL_HANDLE;
    VkPipeline            EffectPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout EffectDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      EffectDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       EffectDescriptorSet = VK_NULL_HANDLE;
    VkPushConstantRange   EffectPushConstantRange{};
    VkDescriptorSetLayoutCreateInfo EffectDescriptorSetLayoutCreateInfo;
    VkCommandBuffer EffectCommandBuffer = VK_NULL_HANDLE;
    VkFence EffectFence = VK_NULL_HANDLE;
    bool bFenceInUse = false;

    virtual void CreateResources();
    virtual void DestroyResources();

    virtual void UpdateSettingsFromNvr() {};
    virtual void CreateShaderModule();
    virtual void CreatePipeline() = 0;
    virtual void CreateDescriptorSets() = 0;
    virtual void CreateInteropTextures() = 0;
    virtual void CreateCommandBuffer();
    virtual void CreateFence();
    virtual void LoadShaderModule();
};

struct FVulkanEffectInfo
{
    std::string Name;
    EVulkanEffectPhase Phase;
    int Order = 0;
    std::function<std::unique_ptr<IVulkanEffect>()> Create;
};

class FVulkanEffectFactory
{
private:
    static std::vector<FVulkanEffectInfo>& GetRegistryMutable()
    {
        static std::vector<FVulkanEffectInfo> Registry;
        return Registry;
    }

public:
    static const std::vector<FVulkanEffectInfo>& GetRegistry()
    {
        return GetRegistryMutable();
    }

    static void Register(const std::string& Name,
        EVulkanEffectPhase Phase,
        int Order,
        std::function<std::unique_ptr<IVulkanEffect>()> CreateFn)
    {
        FVulkanEffectInfo Info{ Name, Phase, Order, std::move(CreateFn) };
        GetRegistryMutable().push_back(Info); // NOW VALID
    }
};



template<typename TEffect>
struct TVulkanEffectRegistrar
{
    TVulkanEffectRegistrar(const char* Name,
        EVulkanEffectPhase Phase,
        int Order)
    {
        FVulkanEffectFactory::Register(
            Name,
            Phase,
            Order,
            []() -> std::unique_ptr<IVulkanEffect> {
                return std::make_unique<TEffect>();
            });
    }
};

#define REGISTER_VULKAN_EFFECT(EffectClass, PhaseEnum, OrderValue) \
    static TVulkanEffectRegistrar<EffectClass> \
        g_##EffectClass##_Registrar(#EffectClass, PhaseEnum, OrderValue);
