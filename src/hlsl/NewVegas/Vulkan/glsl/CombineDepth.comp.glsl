#version 450
#extension GL_GOOGLE_include_directive : enable

#include "Includes/GlobalLayout.comp.glsl"

layout(local_size_x = 16, local_size_y = 16) in;

// ---------- Local set (set = 1) ----------
//
// binding 0: output storage image

layout(set = 1, binding = 0, rgba32f) uniform writeonly image2D uOutImage;

void main()
{
    ivec2 size = imageSize(uOutImage);
    ivec2 pix  = ivec2(gl_GlobalInvocationID.xy);

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    vec2 uv = (vec2(pix) + 0.5) / vec2(size);

    // gDepth is the NVR combined depth surface coming from FVulkanGlobalResources
    vec2 rg = texture(gDepth, uv).rg;

    // Just visualize red channel for now
    float d = rg.r;

    // Naive remap + clamp for visibility
    float t = clamp(d, 0.0, 1.0);

    imageStore(uOutImage, pix, vec4(t, t, t, 1.0));
}
