// VulkanSettingsIntegration.h
#pragma once

class IVulkanEffect;

// effectId: what you want to use in TOML, e.g. "Vulkan.Normals"
void RegisterVulkanEffectSettingsInToml(IVulkanEffect& InEffect);
