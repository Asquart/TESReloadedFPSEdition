#version 450

layout(local_size_x = 16, local_size_y = 16) in;

// ---------- Global frame set (set = 0) ----------
//
// binding 0: depth texture (NVR combined depth, via FVulkanGlobalResources)
// binding 1: frame UBO (projection/view etc., not actually used yet here)

layout(set = 0, binding = 0) uniform sampler2D gDepth;

layout(set = 0, binding = 1, std140) uniform GlobalFrameUBO {
    mat4 Projection;
    mat4 InvProjection;
    mat4 View;
    mat4 InvView;

    vec4 DepthConstants;   // x = viewNearZ, z = invertedDepth etc.
    vec4 CameraData;       // x = nearZ, y = farZ
    vec4 CameraPosition;
} uFrame;

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
