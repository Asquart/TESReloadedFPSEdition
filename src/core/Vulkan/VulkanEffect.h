#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "VulkanContext.h"
#include "VulkanInteropManager.h"
#include "../SettingManager.h"


enum class EVulkanEffectPhase : uint8_t
{
    PreTonemap,
    PostTonemap,
};

class IVulkanEffect
{
public:
    IVulkanEffect(const std::string& inName,
                  const std::string& inDescription,
                  const std::vector<SettingManager::VulkanEffectSetting>& inDefaults);

    virtual ~IVulkanEffect();

    const std::string& GetName() const { return Name; }
    const std::string& GetDescription() const { return Description; }
    virtual EVulkanEffectPhase GetPhase() const = 0;

    virtual void Initialize();

    // Different phases if you need them
    virtual void SubmitRendering() {}

    virtual void CompleteRendering(IDirect3DSurface9* SceneColor) {}

    virtual void OnGameBuffersUpdated();

    const char* SpirvPath = "";

    void RegisterMenuSettings();
    void RefreshMenuSettings();
    bool IsEnabled() const { return bEnabled; }
    float GetGpuTimeMs() const { return GpuTimeMs; }
    std::string GetSettingsSection(const std::string& SubSection) const;

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
    VkQueryPool TimingQueryPool = VK_NULL_HANDLE;
    bool bGpuTimingActive = false;
    bool bEnabled = false;
    bool bRegistered = false;

    virtual void CreateResources();
    virtual void DestroyResources();

    virtual void UpdateSettingsFromNvr() = 0;
    virtual void CreatePipeline() = 0;
    virtual void CreateDescriptorSets() = 0;
    virtual void CreateInteropTextures() = 0;
    virtual void CreateCommandBuffer();
    virtual void CreateFence();
    virtual void LoadShaderModule();
    virtual void CreateTimingQueries();
    void BeginGpuTimer();
    void EndGpuTimer();
    void ResolveGpuTime();

    void EnsureRegistered();

    std::string Name;
    std::string Description;
    std::vector<SettingManager::VulkanEffectSetting> DefaultSettings;

public:
    float GpuTimeMs;
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
    TVulkanEffectRegistrar(const std::string& Name,
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
        g_##EffectClass##_Registrar(EffectClass().GetName(), PhaseEnum, OrderValue);
