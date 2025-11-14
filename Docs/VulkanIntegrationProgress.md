# Vulkan Integration Progress

This document captures the current Vulkan integration state after completing the first three steps of the layer plan and outlines what remains for compute injection.

## Step 1 – Implicit Layer Bootstrap
- Implemented an implicit Vulkan layer (`VulkanLayer/Layer.cpp`) that intercepts instance/device creation and preserves loader dispatch chains.
- Built per-instance and per-device dispatch tables, queue ownership tracking, and logging utilities exposed through the shared bridge (`TRBridge_LogMessage`).
- Layer manifest (`VulkanLayer/tesreloaded_vulkan_layer.json`) declares environment toggles so development builds can opt in/out of the layer.

## Step 2 – Plugin/Layer Bridge
- Added the shared bridge (`Shared/Bridge/Bridge.*`) exporting configuration, logging, frame notifications, and render target metadata APIs consumable from both the layer and TESReloaded.
- The plugin publishes render target descriptors and signals frame boundaries via `TRBridge_SignalPluginFrame`, while the layer consumes them inside `vkQueueSubmit`/`vkQueuePresentKHR` intercepts.
- Heartbeat callbacks let the plugin verify that the layer is responsive.

## Step 3 – Resource Interop
- Introduced DXVK interop support (`Shared/Bridge/DXVKInterop.*`) that loads exported helper functions and captures Vulkan image/device-memory handles for TESReloaded render targets.
- `TextureManager` and `ShaderManager` publish `TRBridgeInteropSurface` and `TRBridgeInteropSyncState` records so the layer knows which Vulkan images to consume and which semaphores/fences guard them.
- The layer caches the surfaces, logs their handle inventory, and stores per-queue compute state (command pool, command buffer, fence) ready for injected workloads.

## Step 4 – Compute Injection (Complete)
- The layer now consumes the plugin’s interop surfaces and injects a compute dispatch after DXVK’s final post-process submission, performing the required layout transitions before and after the compute workload.
- Synchronization honors the bridge-provided wait and signal semaphores (including timeline values) and falls back cleanly when essential handles or surfaces are missing.
- A runtime shader compiler powered by the vendored Vulkan SDK (shaderc) generates SPIR-V for a placeholder compute shader (`VulkanLayer/Shaders/placeholder_ao.comp`) that currently fills the rendered surface with a solid debug color, establishing the path for a future AO kernel.
- The new `VulkanAmbientOcclusion` toggle in the in-game Shaders menu updates the shared bridge so the layer only schedules the placeholder compute dispatch when the shader is explicitly enabled.
