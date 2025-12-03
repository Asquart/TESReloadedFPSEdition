// #version 450
#extension GL_GOOGLE_include_directive : enable

#include "Blur.comp.glsl"

#ifndef BLUR_DEPTH_GLSL
#define BLUR_DEPTH_GLSL

// Requires:
//  - Depth.glsl   (readDepth(), farZ)
//  - Blur.glsl    (cKernelSize, BlurOffsets, BlurWeights)
//
// Depth-aware 1D blur along offsetMask (e.g. vec2(1,0) for horizontal, vec2(0,1) for vertical)
//
// uv          - base texture coords
// buffer      - AO / color buffer to blur
// offsetMask  - blur direction (1,0) or (0,1)
// blurRadius  - scale for the kernel offsets
// depthDrop   - max allowed depth difference (world units-ish)
// endFade     - clip if depth is beyond this threshold

vec4 DepthBlur(
    vec2  uv,
    sampler2D srcTex,   // renamed from 'buffer' to avoid GLSL 'buffer' keyword
    vec2  offsetMask,
    float blurRadius,
    float depthDrop,
    float endFade)
{
    // Center tap weight
    float weightSum = 0.114725602;
    vec4  color1    = texture(srcTex, uv) * weightSum;

    float depth1 = readDepth(uv);

    // HLSL: clip(endFade - depth1);
    // In a fragment shader, this would discard the pixel and keep previous RT value.
    // In compute we can't 'discard', so we just "don't blur" and return the source.
    if (endFade - depth1 < 0.0)
        return vec4(texture(srcTex, uv).rgb, 1.0);

    depthDrop *= (depth1 / farZ);

    for (int i = 0; i < cKernelSize; ++i)
    {
        vec2 uvOff   = uv + (BlurOffsets[i] * offsetMask) * blurRadius;
        vec4 color2  = texture(srcTex, uvOff);
        float depth2 = readDepth(uvOff);
        float diff   = abs(depth1 - depth2);

        // HLSL: int useForBlur = (diff <= depthDrop);
        float useForBlur = diff <= depthDrop ? 1.0 : 0.0;

        color1    += BlurWeights[i] * color2 * useForBlur;
        weightSum += BlurWeights[i] * useForBlur;
    }

    color1 /= max(weightSum, 1e-6);

    return vec4(color1.rgb, 1.0);
}


#endif // BLUR_DEPTH_GLSL
