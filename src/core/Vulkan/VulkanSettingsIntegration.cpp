#include "VulkanSettingsIntegration.h"
#include "../SettingManager.h"
#include "VulkanEffect.h"

extern SettingManager* TheSettingManager;

// Helpers copied from earlier suggestion
static tomlValue* EnsureTomlTable(tomlValue& root, const char* section)
{
    char path[256] = "_";
    strcat(path, section);  // NVR settings use leading "_" in DefaultConfig/TomlConfig

    StringList keys;
    SettingManager::SplitString(path, ".", &keys);

    tomlValue* table = &root;
    for (auto& key : keys)
    {
        if (!table->contains(key))
            (*table)[key] = toml::table();

        table = &table->at(key);
    }
    return table;
}

static void SetDefaultFromDescriptor(tomlValue* dstTable, const VulkanSettingDescriptor& s)
{
    if (!dstTable)
        return;

    // Don't overwrite if already present in defaults
    if (dstTable->contains(s.id))
        return;

    switch (s.type)
    {
    case VulkanSettingType::Bool:
    {
        bool def = s.getBool ? s.getBool() : false;
        (*dstTable)[s.id] = def;
        break;
    }
    case VulkanSettingType::Int:
    {
        int def = s.getInt
            ? s.getInt()
            : static_cast<int>(s.minValue);
        (*dstTable)[s.id] = def;
        break;
    }
    case VulkanSettingType::Float:
    {
        float def = s.getFloat
            ? s.getFloat()
            : s.minValue;
        (*dstTable)[s.id] = def;
        break;
    }
    case VulkanSettingType::Enum:
    {
        int def = 0;
        if (s.getInt)
            def = s.getInt();
        else if (s.enumEntries && s.enumCount > 0)
            def = s.enumEntries[0].value;

        (*dstTable)[s.id] = def;
        break;
    }
    }
}

void RegisterVulkanEffectSettingsInToml(IVulkanEffect& InEffect)
{
    if (!TheSettingManager)
        return;

    if (!TheSettingManager->Config.configLoaded)
        TheSettingManager->Config.Init(); // make sure configs are loaded

    // 1) Status.Enabled under Shaders.<Id>.Status
    char statusSection[128];

    const char* EffectName = InEffect.GetName();
    sprintf_s(statusSection, "Shaders.%s.Status", EffectName);

    tomlValue* defStatus = EnsureTomlTable(TheSettingManager->Config.DefaultConfig, statusSection);
    tomlValue* cfgStatus = EnsureTomlTable(TheSettingManager->Config.TomlConfig, statusSection);

    if (!defStatus->contains("Enabled"))
        (*defStatus)["Enabled"] = true; // default off

    if (!cfgStatus->contains("Enabled"))
        (*cfgStatus)["Enabled"] = (*defStatus)["Enabled"];

    // 2) Effect-specific settings from VulkanSettingDescriptor
    const auto& settings = InEffect.GetSettings();

    for (const VulkanSettingDescriptor& s : settings)
    {
        const char* group = (s.group && s.group[0]) ? s.group : "Main";

        char section[256];
        sprintf_s(section, "Shaders.%s.%s", EffectName, group);  // e.g. "Shaders.Vulkan.Normals.Main"

        tomlValue* defTable = EnsureTomlTable(TheSettingManager->Config.DefaultConfig, section);
        tomlValue* cfgTable = EnsureTomlTable(TheSettingManager->Config.TomlConfig, section);

        SetDefaultFromDescriptor(defTable, s);

        if (defTable && cfgTable && !cfgTable->contains(s.id))
            (*cfgTable)[s.id] = (*defTable)[s.id];
    }
}
