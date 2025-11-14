# Vulkan Layer Port Summary and Optimization Ideas

## Overview
This repository now contains the full Vulkan ambient occlusion layer scaffolding that previously lived in the legacy fork. The port introduces:

- A shared runtime bridge (`Shared/Bridge`) that exposes configuration, logging, render-target metadata, and frame synchronization APIs consumed by both the Direct3D 9 plugin and the Vulkan layer.
- A DXVK interoperability helper that loads an auxiliary `dxvk_interop` library at runtime to fetch Vulkan object handles and synchronization primitives corresponding to TESReloaded render targets.
- Integration points inside the New Vegas plugin (settings, shader, and texture managers) that register TESReloaded render targets with the bridge, propagate user configuration flags, and emit per-frame synchronization events for the Vulkan layer.
- A standalone implicit Vulkan layer project with full function interception, shader compilation, command recording, and the placeholder ambient occlusion compute pipeline that writes a constant colour into the AO surface.
- MSBuild wiring so the layer links against the plugin import library, guaranteeing the bridge symbols resolve when the layer is built.

## Shared Bridge Runtime
The bridge keeps all cross-module state in a guarded singleton that tracks configuration, render-target descriptors, interop handles, frame notifications, and heartbeat timestamps. The exported C API allows:

- Reference-counted initialization/shutdown, guaranteeing the shared state is reset once both the plugin and layer release it.
- Configuration reads/writes so the plugin can push user settings (e.g., enabling Vulkan AO) and the layer can query them.
- Registration and enumeration of render targets and interop surfaces, including descriptors for Vulkan handles obtained through DXVK.
- Frame signalling (`TRBridge_SignalPluginFrame`) and consumption (`TRBridge_ConsumePendingFrame`) so the layer knows when to inject compute work.
- Synchronization state exchange that carries Vulkan semaphore and fence handles, including timeline semaphore payloads, between DXVK and the compute queue.
- Logging callbacks and heartbeat tracking to help diagnose whether the layer is active.

## DXVK Interoperability Helper
`Shared/Bridge/DXVKInterop.*` dynamically loads a helper module (DLL/SO) and resolves exported functions that provide Vulkan images, views, memory, and synchronization objects for DXVK-managed resources. The helper exposes:

- Capability queries (`DxvkInterop_IsAvailable`) after resolving function pointers.
- Surface info queries and sibling-surface creation for TESReloaded textures.
- Frame synchronization retrieval that reports acquire/release semaphores and optional fences for the current DXVK frame.
- Optional debug upload/download hooks to aid validation when environment variables request it.

## Plugin Integration Points
### Settings Manager
The settings system pulls bridge configuration on load and writes back the current values so the layer sees persisted AO parameters. This keeps the plugin and layer in sync when users toggle the Vulkan AO option through the menu.

### Shader Manager
When Vulkan ambient occlusion is enabled, the shader manager updates the bridge flag every time settings change and emits a per-frame bridge signal containing:

- The frame identifier.
- The number of available interop surfaces.
- Synchronization handles exported by DXVK (timeline or binary semaphores plus optional fences).

The plugin only performs this handshake when the Vulkan AO toggle is active, ensuring legacy behaviour when the feature is disabled.

### Texture Manager
During initialization the texture manager:

1. Boots the bridge and attempts to initialize the DXVK interop module when environment variables request it.
2. Registers TESReloaded render targets (source, rendered, depth, and derived surfaces) with the bridge, attaching any Vulkan image/view/memory handles exposed by DXVK.
3. Publishes a bridge snapshot so the layer can look up interop surfaces by name and access their handle arrays, row pitch, and depth pitch.
4. Optionally validates surfaces by copying data through the interop helper when validation flags are active.

## Vulkan Layer Architecture
The implicit layer follows the canonical dispatch-table pattern:

- It forwards all Vulkan entry points to DXVK while capturing instance/device creation to populate custom dispatch tables for later use.
- Queue submissions and presents are wrapped to detect the TESReloaded frame boundary and inject compute work between DXVK’s final submission and presentation when the bridge reports a pending frame.
- The layer lazily compiles the placeholder compute shader (`Shaders/placeholder_ao.comp`) through `shaderc`, caches the SPIR-V output alongside the GLSL source, and recreates the pipeline if the shader changes on disk.
- For each frame signalled by the plugin, the layer acquires a command buffer, records layout transitions for the registered AO surface, binds the compute pipeline, and dispatches enough workgroups to cover the target dimensions. Afterward it transitions the surface back for presentation and submits the command buffer with semaphores/fences mirroring DXVK’s expectations.
- Submission waits honour timeline semaphore payloads when provided and fall back to fence waits, ensuring DXVK retains ownership ordering guarantees.
- Logging is plumbed through the bridge so layer activity is visible even without attaching a debugger.

The placeholder compute shader simply paints the ambient occlusion target with a constant teal colour, verifying the end-to-end plumbing without relying on scene data.

## Build Integration
`VulkanLayer.vcxproj` now links against `NewVegasReloaded.lib` and depends on the plugin project, ensuring MSBuild produces the import library before building the layer. The additional library directory points at the plugin output so the layer can resolve bridge exports without manual post-build steps.

## Runtime Behaviour and Expected Performance Impact
Enabling the Vulkan AO toggle keeps the legacy D3D9 effects active while the layer also:

- Copies render-target metadata and interop handles into shared memory each frame.
- Performs an additional queue submission per frame, including pipeline barriers and descriptor updates, to dispatch the compute shader.
- Waits on a fence (or DXVK’s external fence, if provided) after each dispatch to guarantee completion before presentation.

With the placeholder shader active the GPU workload is minimal, but there is still measurable CPU overhead from extra synchronization and command recording. Expect a small increase in frame time (sub-millisecond on modern CPUs/GPUs) due to the submit + fence wait and any semaphore waits injected by DXVK. When Vulkan AO is disabled the plugin bypasses the bridge signalling path, eliminating that overhead.

## Potential Optimization Opportunities
### Base Mod
- Avoid redundant `StretchRect` copies in the post-processing chain by collapsing passes that only move data between identical render targets.
- Batch configuration reads/writes by caching frequently accessed settings in structures instead of repeated XML parsing.
- Replace per-frame dynamic allocations in managers with preallocated pools to reduce heap churn and synchronization.
- Audit shader constant updates to limit device state changes when values remain unchanged between frames.

### Vulkan Layer
- Move from fence waits to timeline semaphore waits with finite timeouts to avoid blocking CPU threads when DXVK already supplies completion signals.
- Reuse descriptor sets across frames and only rebind when the AO surface set changes, reducing `vkUpdateDescriptorSets` traffic.
- Introduce command buffer pools per queue family with reset-once-per-frame semantics to eliminate command pool resets inside the submission path.
- Adopt asynchronous shader compilation and pipeline creation so hot-reloading the compute shader does not stall rendering.
- Defer placeholder colour writes to a debug build flag so release builds skip redundant compute dispatch when AO is disabled or not needed.

