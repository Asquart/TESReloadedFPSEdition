# Vulkan Layer Integration Plan for TESReloaded

## Goals
- Inject a custom implicit Vulkan layer that cooperates with DXVK to run compute-heavy post-process work (starting with ambient occlusion) on modern hardware.
- Maintain compatibility with the existing Direct3D 9-based plugin while enabling a high-performance compute path.
- Provide an extensible foundation for other post-processing features to migrate to Vulkan compute shaders over time.

## Overview
The plan hinges on installing a Vulkan layer that intercepts DXVK's Vulkan objects, synchronizes with TESReloaded's Direct3D 9 pipeline, and executes compute workloads using the same swapchain images. The implementation proceeds in five phases:

1. **Layer Scaffolding** – Build the implicit layer DLL/so, install its JSON manifest, and ensure it receives callbacks for `vkCreateInstance`, `vkCreateDevice`, queue submissions, and presentation.
2. **Shared Runtime Bridge** – Establish a shared module that both the Direct3D 9 plugin and the Vulkan layer can call into for coordination, resource lookup, and lifetime management.
3. **Resource Interop** – Mirror TESReloaded render targets with Vulkan images or secure access to DXVK's internal images using exported handles.
4. **Compute Injection** – Record compute dispatches between DXVK's render passes and `vkQueuePresentKHR`, with robust synchronization (fences/semaphores) to guarantee safe ownership transfers.
5. **Feature Rollout** – Port SSAO to the compute path, then onboard other expensive effects once the framework proves stable.

## Phase Details

### 1. Layer Scaffolding
- Author a new module (e.g., `VulkanLayer/Layer.cpp`) implementing the standard `PFN_vkGetInstanceProcAddr`/`PFN_vkGetDeviceProcAddr` forwarding pattern.
- Register the layer as implicit via a JSON manifest placed under the game's `reshade-shaders` equivalent or system layer directory. Use environment variables for development override.
- In `vkCreateInstance`/`vkCreateDevice`, wrap DXVK's returned handles with tracking structures storing dispatch tables and queue lists.
- Hook `vkQueueSubmit`, `vkQueuePresentKHR`, and (if necessary) command buffer creation to inject post-render workloads.

### 2. Shared Runtime Bridge
- Introduce a lightweight shared library (`NVReloadedBridge.dll`) loaded by both the D3D9 plugin and the Vulkan layer.
- Provide thread-safe structures for:
  - Exchanging configuration (enabled features, AO parameters).
  - Advertising D3D9 render-target descriptors (dimensions, formats, usage).
  - Signaling when the plugin has produced new inputs requiring compute processing.
- Implement logging and diagnostics accessible from both sides to simplify debugging.

### 3. Resource Interop
- Extend `TextureManager::Initialize` to optionally create sibling Vulkan images matching each key D3D9 texture. Depending on platform:
  - **Preferred path:** Modify DXVK locally to expose `VkImage` handles and associated `VkDeviceMemory` via a lightweight API (e.g., exported C functions). This keeps copies GPU-resident and avoids duplication.
  - **Fallback path:** Use shared memory exports (e.g., `VK_KHR_external_memory_fd` / Win32 handles) if DXVK can be configured to allocate textures with exportable memory. This still requires DXVK cooperation.
- Establish per-frame synchronization primitives (timeline semaphores or fences) that the layer waits on before reading from these images and signals once compute work completes.
- Implement upload/download utilities for development builds to validate correctness via CPU copies before enabling full GPU path.

### 4. Compute Injection
- During `vkQueueSubmit`, detect when DXVK submits its final post-process/compose command buffer. Insert an additional submission on the same queue that:
  - Acquires the shared images using pipeline barriers to transition layouts for compute reads/writes.
  - Binds the AO compute pipeline, descriptor sets, and dispatch parameters.
  - Releases the images back to color-attachment layout for the subsequent presentation submission.
- Provide fallbacks to skip compute when the layer or bridge reports missing inputs or device loss.
- Manage shader compilation by generating SPIR-V from GLSL/HLSL via `glslangValidator` or DXC during build time; hot-reload support aids iteration.

### 5. Feature Rollout
- **Stage 1:** Port the optimized SSAO to compute: implement kernels that mirror the precomputed sample set, leverage shared normal/depth textures, and write results into the same AO surface TESReloaded expects.
- **Stage 2:** Evaluate temporal accumulation by persisting AO history images inside the layer and blending them with the compute output before handing control back to DXVK.
- **Stage 3:** Gradually migrate other heavy shaders (god rays, water, depth of field) by exposing their inputs to the bridge and authoring compute equivalents.
- **Stage 4:** Provide configuration toggles within TESReloaded to choose between the legacy D3D9 path and the Vulkan compute layer for each effect.

## Risk Mitigation
- Maintain a runtime flag to disable the Vulkan layer dynamically, falling back to the legacy shaders to avoid breaking the game during development.
- Implement extensive validation (Vulkan validation layers, optional CPU readback comparisons) to detect synchronization or format mismatches early.
- Document the DXVK modifications clearly; upstream contribution or a patch set may ease long-term maintenance.

## Dependencies & Tooling
- Vulkan SDK for layer development, validation, and SPIR-V compilation.
- Build integration within the existing solution (e.g., new CMake/MSBuild projects) to compile the layer alongside the plugin.
- Custom DXVK build or patches enabling resource export if upstream does not expose the necessary handles.

## Next Steps
1. Prototype the implicit layer with logging to confirm intercepts on `vkCreateInstance`/`vkCreateDevice` when running under DXVK.
2. Define the shared bridge API and integrate it into the TESReloaded initialization sequence.
3. Decide on the resource sharing strategy (DXVK patch vs. mirrored allocations) and implement a proof-of-concept that copies a color buffer through the layer.
4. Author the compute SSAO shader and validation harness before replacing the in-game effect.

