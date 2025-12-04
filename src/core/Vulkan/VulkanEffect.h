#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "VulkanContext.h"
#include "VulkanInteropManager.h"

enum class VulkanSettingType
{
    Bool,
    Int,
    Float,
    Enum
};

struct VulkanEnumEntry
{
    int         value;
    const char* label;
};

struct VulkanSettingDescriptor
{
    const char* id;          // "Intensity", "Radius", "SampleCount"
    const char* label;       // "Intensity", "Radius", "Sample Count"
    const char* group;       // optional sub-section in UI, e.g. "Quality", "Debug"
    VulkanSettingType type;

    // Generic limits for numeric types
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step     = 0.01f;

    // For enums
    const VulkanEnumEntry* enumEntries = nullptr;
    uint32_t               enumCount   = 0;

    // Typed callbacks that actually talk to the effect's settings struct
    std::function<bool()>        getBool;
    std::function<void(bool)>    setBool;

    std::function<int()>         getInt;
    std::function<void(int)>     setInt;

    std::function<float()>       getFloat;
    std::function<void(float)>   setFloat;
};

enum class EVulkanEffectPhase : uint8_t
{
    PreTonemap,
    PostTonemap,
};

class IVulkanEffect
{
public:
    virtual ~IVulkanEffect() = default;

    virtual const char* GetName() const { return EffectName; }
    virtual EVulkanEffectPhase GetPhase() const = 0;

    // Optional: override to define SPIR-V file name, base class can auto-build path
    virtual const char* GetSpirvFileName() const { return EffectSpirvFileName; }

    virtual void Initialize();

    // Different phases if you need them
    // NOTE: base is still a no-op; subclasses override.
    virtual void SubmitRendering() {}

    virtual void CompleteRendering(IDirect3DSurface9* SceneColor) {}

    virtual void OnGameBuffersUpdated();

    // Let effects set this explicitly or auto from GetSpirvFileName()
    std::string SpirvPath;

    virtual bool IsEnabled() const;
    virtual void SetEnabled(const bool InEnabled);

    // GPU time in ms, updated every tick by your Vulkan code
    virtual float GetGpuTimeMs() const;
    virtual void SetGpuTimeMs(const float InMs);

    // Settings for NVR menu – default: no settings
    virtual const std::vector<VulkanSettingDescriptor>& GetSettings() const
    {
        static std::vector<VulkanSettingDescriptor> Empty;
        return Empty;
    }

protected:

    // Vulkan pipeline objects
    VkShaderModule        EffectShaderModule = VK_NULL_HANDLE;
    VkPipelineLayout      EffectPipelineLayout = VK_NULL_HANDLE;
    VkPipeline            EffectPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout EffectDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      EffectDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       EffectDescriptorSet = VK_NULL_HANDLE;
    VkPushConstantRange   EffectPushConstantRange{};
    VkDescriptorSetLayoutCreateInfo EffectDescriptorSetLayoutCreateInfo{};
    VkCommandBuffer       EffectCommandBuffer = VK_NULL_HANDLE;
    VkFence               EffectFence = VK_NULL_HANDLE;
    bool                  bFenceInUse = false;
    bool                  bEnabled    = true;
    float                 GpuTimeMs   = 0.0f;
    VkQueryPool EffectQueryPool = VK_NULL_HANDLE;
    uint32_t    StartQueryIndex = 0;
    uint32_t    EndQueryIndex   = 1;
    double DebugStartTimeMs = 0.0;

    const char* EffectID = "";
    const char* EffectName = "";
    const char* EffectSpirvFileName = "";

    virtual void CreateResources();
    virtual void DestroyResources();

    virtual void UpdateSettingsFromNvr() {}
    virtual void CreatePipeline() = 0;
    virtual void CreateDescriptorSets() = 0;
    virtual void CreateInteropTextures() = 0;
    virtual void CreateCommandBuffer();
    virtual void CreateFence();
    virtual void LoadShaderModule();
    virtual void CreateTimestampQueryPool();
};

// Base class for fullscreen / image-sized compute post-process effects.
// Handles command buffer recording, queue submission, and simple multi-pass loops.
class FComputeEffectBase : public IVulkanEffect
{
public:
    // Main entry point – you usually don't override this in derived classes.
    void SubmitRendering() override;

protected:
    // How many passes to run?
    virtual uint32_t GetPassCount() const { return 1; }

    // Workgroup size (default 16x16).
    virtual VkExtent2D GetWorkgroupSize() const
    {
        return VkExtent2D{ 16, 16 };
    }

    // Image resolution (width/height) this effect should run at.
    virtual VkExtent2D GetDispatchExtent() const = 0;

    // Called before recording; create/validate interop surfaces etc.
    // Return false to skip rendering this frame.
    virtual bool PrepareResourcesForSubmit() = 0;

    // Effect-specific commands for a given pass:
    //   - bind descriptor sets
    //   - push constants
    //   - vkCmdDispatch(cmd, groupsX, groupsY, 1)
    virtual void RecordPassCommands(VkCommandBuffer cmd,
                                    uint32_t passIndex,
                                    uint32_t groupsX,
                                    uint32_t groupsY) = 0;

    // Optional: extra barriers or debug markers after each pass.
    virtual void OnAfterPass(VkCommandBuffer /*cmd*/, uint32_t /*passIndex*/) {}

    // Optional: called when submit fails.
    virtual void OnSubmitFailed(VkResult /*vr*/) {}
};

// Template base for geometry-rendering effects (shadows, reflections, tessellation, etc.).
// For now it's just a skeleton you can fill out later.
class FGraphicsEffectBase : public IVulkanEffect
{
public:
    // We'll provide a default implementation in VulkanEffect.cpp later.
    void SubmitRendering() override;

protected:
    // Where to render (extent)
    virtual VkExtent2D GetRenderExtent() const = 0;

    // Which renderpass/framebuffer to use
    virtual VkRenderPass GetRenderPass() const = 0;
    virtual VkFramebuffer GetFramebuffer() const = 0;

    // Record your draw calls here (bind pipeline, descriptor sets, and draw).
    virtual void RecordDrawCommands(VkCommandBuffer cmd) = 0;

    // Optional hooks
    virtual void OnBeginRenderPass(VkCommandBuffer /*cmd*/) {}
    virtual void OnEndRenderPass(VkCommandBuffer /*cmd*/) {}
    virtual void OnSubmitFailed(VkResult /*vr*/) {}

    virtual uint32_t GetClearValueCount() const { return 0; }
    virtual const VkClearValue* GetClearValues() const { return nullptr; }

};


// Combined base: compute effect + settings + push constants helper.
// TSettings:     CPU-side settings struct (visible in menu)
// TPushConstants:struct matching GLSL push constant layout
template<typename TSettings, typename TPushConstants>
class FComputeEffectWithSettings : public FComputeEffectBase
{
public:
    using SettingsType      = TSettings;
    using PushConstantsType = TPushConstants;

    const SettingsType& GetSettingsStruct() const { return Settings; }
    SettingsType&       GetSettingsStruct()       { return Settings; }

    // Expose settings to NVR menu
    const std::vector<VulkanSettingDescriptor>& GetSettings() const override
    {
        return SettingDescs;
    }

protected:
    // Derived class must define how to fill push constants from settings per pass.
    virtual void FillPushConstants(PushConstantsType& out,
                                   const SettingsType& src,
                                   uint32_t passIndex) const = 0;

    // Helper to build push constants for a given pass.
    PushConstantsType BuildPushConstants(uint32_t passIndex) const
    {
        PushConstantsType pc{};
        FillPushConstants(pc, Settings, passIndex);
        return pc;
    }

    // Helper: push constants and dispatch once.
    // Derived class typically:
    //  - binds descriptor sets
    //  - calls this
    void PushConstantsAndDispatch(VkCommandBuffer cmd,
                                  uint32_t passIndex,
                                  uint32_t groupsX,
                                  uint32_t groupsY,
                                  VkShaderStageFlags stages = VK_SHADER_STAGE_COMPUTE_BIT) const
    {
        PushConstantsType pc = BuildPushConstants(passIndex);

        p_vkCmdPushConstants(
            cmd,
            EffectPipelineLayout,
            stages,
            0,
            static_cast<uint32_t>(sizeof(PushConstantsType)),
            &pc);

        p_vkCmdDispatch(cmd, groupsX, groupsY, 1);
    }

    // Derived class should fill this in its ctor via a BuildSettingsDescriptors() helper.
    SettingsType                         Settings{};
    std::vector<VulkanSettingDescriptor> SettingDescs;
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