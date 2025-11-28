#version 450

layout(local_size_x = 16, local_size_y = 16) in;

// Binding 0: NVR combined depth as a sampled texture
layout(binding = 0) uniform sampler2D uCombinedDepth;

// Binding 1: output as a storage image (what you later StretchRect to SceneColor)
layout(binding = 1, rgba32f) uniform writeonly image2D uOutImage;

void main()
{
    ivec2 size = imageSize(uOutImage);
    ivec2 pix  = ivec2(gl_GlobalInvocationID.xy);

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    // Normalized UV
    vec2 uv = (vec2(pix) + 0.5) / vec2(size);

    // NVR CombinedDepthTexture is D3DFMT_G32R32F -> VK_FORMAT_R32G32_SFLOAT,
    // so we get two float channels. Let's peek at them.
    vec2 rg = texture(uCombinedDepth, uv).rg;

    // For now, just use the *red* channel as "depth-ish" value
    float d = rg.r;

    // Simple remap to [0,1] for visibility
    float t = clamp(d, 0.0, 1.0);

    // Show as grayscale
    imageStore(uOutImage, pix, vec4(t, t, t, 1.0));
}
