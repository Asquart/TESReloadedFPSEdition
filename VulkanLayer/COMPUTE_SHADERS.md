# Vulkan Compute Shader Integration Guide

This fork exposes a Vulkan-based compute stage that can run after the DXVK graphics work has produced the "rendered" color buffer. The implementation is driven through the shared bridge in `Shared/Bridge` and the implicit layer contained in `VulkanLayer/Layer.cpp`. This document describes how to extend that system with additional compute shaders and how to access the render targets that are exported from the Direct3D 9 side of the mod.

## Enabling the compute path

* The runtime configuration is exposed through `TRBridgeConfiguration` (`Shared/Bridge/Bridge.h`). The flag `TR_BRIDGE_FLAG_ENABLE_VULKAN_AO` toggles the compute stage.
* The UI toggle in the in-game menu is handled by `ShaderManager::SwitchShaderStatus` and `SettingManager::SetMenuShaderEnabled`. Toggling **Vulkan Ambient Occlusion** updates the shared configuration, which is consumed every frame by the Vulkan layer (`TryInjectComputeWork`).
* When compute is disabled the bridge still tracks render targets, but the layer exits early without submitting work.

## Render targets that are available to compute shaders

The Direct3D 9 runtime publishes render targets and their Vulkan interop handles from `TextureManager::PublishBridgeState`.

| Name | Description | Format | Vulkan handles |
| ---- | ----------- | ------ | -------------- |
| `TESR_SourceTexture` | Copy of the original post-processing input (pre-effect color) | `A16B16G16R16F` | `VkImage`, `VkImageView` (color) |
| `TESR_RenderedTexture` | Post-processed color target used as compute output | `A16B16G16R16F` | `VkImage`, `VkImageView` (storage) |
| `TESR_DepthTexture` | Linearized INTZ depth texture used for effects | `D24_UNORM_S8_UINT` | `VkImage`, `VkImageView` (sampled) |
| `TESR_MainDepthStencil` | Primary depth-stencil surface; exposed when available | `D24_UNORM_S8_UINT` | `VkImage`, `VkImageView` (sampled) |
| `TESR_NormalsBuffer` | Screen-space normals written by the normals effect | `A16B16G16R16F` | `VkImage`, `VkImageView` (sampled) |
| `TESR_DepthBuffer` | Combined depth buffer used by post-processing | `G32R32F` | `VkImage`, `VkImageView` (sampled) |
| `TESR_ShadowAtlas` | Cascaded sun shadow atlas | `R32F` (configurable) | `VkImage`, `VkImageView` (sampled) |
| `TESR_OrthoMapBuffer` | Orthographic shadow map for world effects | `R32F` | `VkImage`, `VkImageView` (sampled) |
| `TESR_PointShadowBuffer` | Shadow accumulation buffer for point lights | `G16R16` | `VkImage`, `VkImageView` (sampled) |
| `TESR_ShadowSpotlightBuffer#` | Spotlight shadow slices (`#` = 0…n) | `R32F` | `VkImage`, `VkImageView` (sampled) |
| `TESR_SMAA_Edges` | SMAA edge detection buffer | `A8R8G8B8` | `VkImage`, `VkImageView` (sampled) |
| `TESR_SMAA_Blend` | SMAA blending weight buffer | `A8R8G8B8` | `VkImage`, `VkImageView` (sampled) |

Every `TRBridgeInteropSurface` carries a descriptor (dimensions, format, usage) and a set of handles. The Vulkan layer looks surfaces up by name (see `TryInjectComputeWork`) and extracts the `VkImage`/`VkImageView` handles for use in descriptors.

To export additional resources (normals, shadow maps, etc.) you can follow the same pattern used for the existing textures:

1. Register the Direct3D resource with `TextureManager::RegisterTexture` so it can be referenced.
2. Call `RegisterTextureInterop` to create a DXVK interop view and cache the Vulkan handles in `g_vulkanInteropSurfaces`.
3. Extend `TextureManager::PublishBridgeState` to append a new `TRBridgeInteropSurface` entry and publish it through `TRBridge_SetInteropSurfaces`.
4. Update the Vulkan layer to bind the new descriptor (see [Descriptor layout](#descriptor-layout)).

## Descriptor layout

`EnsureDeviceComputePipeline` builds the descriptor set layout that is shared by all compute shaders. The current bindings are:

| Binding | Type | Contents |
| ------- | ---- | -------- |
| 0 | `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` | `TESR_RenderedTexture` – writable output image |
| 1 | `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` | Depth texture sampler (`TESR_DepthTexture` or `TESR_MainDepthStencil`) |
| 2 | `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` | Source color buffer (`TESR_SourceTexture`) |
| 3 | `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` | Screen-space normals (`TESR_NormalsBuffer`) |

If you need extra resources, add another `VkDescriptorSetLayoutBinding` in `EnsureDeviceComputePipeline`, create an appropriate `VkSampler` if the resource will be sampled, and grow the descriptor pool sizes. `TryInjectComputeWork` is responsible for updating the descriptor set each frame with the per-surface handles.

## Image layout transitions

Before dispatching compute work the Vulkan layer transitions the bound images to layouts that are appropriate for compute access:

* Output color: `COLOR_ATTACHMENT_OPTIMAL → GENERAL`
* Source color + normals: `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`
* Depth: `DEPTH_STENCIL_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`

After the dispatch they are restored to their original layouts so the DXVK graphics pipeline can continue rendering without additional synchronization. If you add more bindings, remember to extend the barrier logic so every image moves to a layout that matches its descriptor type before dispatch and returns afterwards.

## Writing a compute shader

* Place GLSL compute shaders under `VulkanLayer/Shaders`. They are hot-loaded at runtime. Use `placeholder_ao.comp` as a template.
* Bindings must match the descriptor layout listed above (set = 0). For example:

  ```glsl
  layout(binding = 0, rgba16f) uniform writeonly image2D uOutputImage;
  layout(binding = 1) uniform sampler2D uDepthTexture;
  ```

* Dispatch size is determined automatically (`CmdDispatch` is invoked with the render target dimensions rounded up to 8×8 work groups).
* Write results with `imageStore`. The layer keeps the color target in `VK_IMAGE_LAYOUT_GENERAL` during the dispatch, so `imageStore` is valid.

## Accessing depth, normals, and shadows

* Depth: sample the depth map via binding 1 as shown in `placeholder_ao.comp`. The depth buffer is shared as a combined image sampler. Sampling returns the hardware depth value (0–1), so any linearization must be performed in-shader if needed.
* Source color: binding 2 provides the post-effect color buffer prior to compute. Use it to combine computed lighting with the scene color.
* Normals: binding 3 exposes `TESR_NormalsBuffer`, which stores screen-space normals encoded in the `[0,1]` range. Decode with `normal = normalize(tex * 2.0 - 1.0)` before use.
* Additional buffers: shadow maps (`TESR_ShadowAtlas`, `TESR_OrthoMapBuffer`, `TESR_PointShadowBuffer`, `TESR_ShadowSpotlightBuffer#`), the combined depth buffer (`TESR_DepthBuffer`), and antialiasing helpers (`TESR_SMAA_Edges`, `TESR_SMAA_Blend`) are published every frame. Author shaders can look them up by name inside `TryInjectComputeWork` or extend the descriptor layout to bind more resources as needed. The bridge supports up to four handles per surface, so Direct3D pointers and Vulkan objects can be forwarded together. On the Vulkan side make sure the declared descriptor type matches the usage flags you set when registering the texture (e.g., `TR_BRIDGE_RT_USAGE_SAMPLED_BIT` → sampled image, `TR_BRIDGE_RT_USAGE_STORAGE_BIT` → storage image).

## Adding a new compute effect

1. Export any additional render targets through the bridge as described above.
2. Extend `EnsureDeviceComputePipeline` to include bindings and samplers for the new resources.
3. Update `TryInjectComputeWork` to look up the surfaces by name, populate the descriptor image infos, and add layout transitions.
4. Implement the GLSL compute shader and place it under `VulkanLayer/Shaders`.
5. Reference the shader from the loader or replace the existing placeholder depending on your desired workflow.

With these steps the compute pipeline will automatically compile the shader through `shaderc` if no `.spv` binary is present and will hot-reload whenever the source changes.
