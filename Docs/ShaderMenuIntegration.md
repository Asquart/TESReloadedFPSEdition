# Shader Menu Integration Guide

This guide documents the steps required to expose a new shader toggle in the in-game **Shaders** menu and wire it to the Vulkan bridge. The `VulkanAmbientOcclusion` placeholder compute shader that ships with this branch follows the same flow.

## 1. Extend the configuration structs
Add a boolean field to the shader settings in both the C++ and Rust representations so the toggle can be persisted and reflected in the menu.

- `TESReloaded/Core/SettingStructure.h` – extend `ffi::ShadersStruct`.
- `Configurator/src/main_config.rs` – add the new field to `ShadersStruct` and its `Default` implementation.
- Seed a default entry (and optional documentation sample) in `Configurator/test.ini`.

## 2. Surface the toggle in the menu runtime
Update the menu helpers so they understand the new setting name.

- `TESReloaded/Core/SettingManager.cpp` – make `GetMenuShaderEnabled` return the field’s state and push the value to the shared bridge during `LoadSettings`.
- If the toggle needs extra presentation or ordering logic, adjust `Configurator/src/menu.rs` as needed (the current reflective walker picks up new boolean entries automatically).

## 3. React inside the shader manager
Hook the toggle into the shader manager so enabling/disabling performs the required runtime work.

- `TESReloaded/Core/ShaderManager.cpp` – call a helper that updates the bridge flag during initialization and whenever `SwitchShaderStatus("<ToggleName>")` fires.
- Guard any per-frame submission (`TRBridge_SignalPluginFrame`, extra effect creation, etc.) so it only runs when the toggle is active.

## 4. Honor the flag inside the Vulkan layer
If the shader requires cooperation from the Vulkan layer, read the bridge flag before scheduling GPU work.

- `VulkanLayer/Layer.cpp` – fetch `TRBridgeConfiguration.flags` (e.g., `TR_BRIDGE_FLAG_ENABLE_VULKAN_AO`) inside `TryInjectComputeWork` and drop any queued frame if the toggle is off.

Following these steps keeps the configuration, in-game menu, plugin runtime, and Vulkan layer in sync for each shader you add.
