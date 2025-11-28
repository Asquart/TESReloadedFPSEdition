// Blur.glsl - helper functions for scaling and blurring
// HLSL original: blur.hlsl
//
// Requirements (from the including shader):
//   1) A sampler2D for the blur source:
//        #define BLUR_TEXTURE someSamplerName
//   2) A vec2 with reciprocal resolution (1/width, 1/height):
//        #define TESR_RECIPROCAL_RES someVec2
//
// If TESR_RECIPROCAL_RES is not defined, this include assumes a
// uniform vec2 TESR_ReciprocalResolution exists in the shader.

#ifndef TESR_RECIPROCAL_RES
    #define TESR_RECIPROCAL_RES TESR_ReciprocalResolution
#endif

#ifndef BLUR_TEXTURE
    #define BLUR_TEXTURE BlurBuffer
#endif

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------

const int   cKernelSize = 12;
const vec2  OffsetMaskH = vec2(1.0, 0.0);
const vec2  OffsetMaskV = vec2(0.0, 1.0);

// Same weights as in HLSL
const float BlurWeights[cKernelSize] = float[](
    0.057424882,
    0.058107773,
    0.061460144,
    0.071020611,
    0.088092873,
    0.106530916,
    0.106530916,
    0.088092873,
    0.071020611,
    0.061460144,
    0.058107773,
    0.057424882
);

// Integer offsets, scaled by TESR_RECIPROCAL_RES at runtime.
// HLSL had float2 offsets with TESR_ReciprocalResolution baked in.
const int BlurOffsetsRaw[cKernelSize] = int[](
    -6, -5, -4, -3, -2, -1,
     1,  2,  3,  4,  5,  6
);

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

// Downsample/upsample a part of the screen given by scaleFactor.
// HLSL: Scale(VSOUT IN, sampler2D buffer, float scaleFactor)
//
// Here we just take UV directly and use BLUR_TEXTURE.
// Outside [0,1] we return transparent black instead of clip/discard.
vec4 Scale(vec2 uv, float scaleFactor)
{
    vec2 suv = uv / scaleFactor;

    // HLSL clip( (uv < 1) - 1 )  → discard if outside [0,1]
    if (suv.x < 0.0 || suv.x > 1.0 ||
        suv.y < 0.0 || suv.y > 1.0)
    {
        return vec4(0.0);
    }

    return texture(BLUR_TEXTURE, suv);
}

// Simple local average without weights. Use scaleFactor to only blur
// a portion of the screen starting from the top-left corner.
// HLSL: BoxBlur(VSOUT IN, sampler2D buffer, float2 offsetMask, float scaleFactor)
vec4 BoxBlur(vec2 uv, vec2 offsetMask, float scaleFactor)
{
    // HLSL: clip((IN.UVCoord <= scaleFactor) - 1);
    // If either component is greater than scaleFactor, treat as no contribution.
    if (uv.x > scaleFactor || uv.y > scaleFactor)
        return vec4(0.0);

    vec2 maxuv = vec2(scaleFactor) - 1.5 * TESR_RECIPROCAL_RES;

    vec4 color = texture(BLUR_TEXTURE, uv);
    color += texture(BLUR_TEXTURE,
                     min(uv + offsetMask *  1.0 * TESR_RECIPROCAL_RES, maxuv));
    color += texture(BLUR_TEXTURE,
                     min(uv + offsetMask * -1.0 * TESR_RECIPROCAL_RES, maxuv));
    color += texture(BLUR_TEXTURE,
                     min(uv + offsetMask * -2.0 * TESR_RECIPROCAL_RES, maxuv));
    color += texture(BLUR_TEXTURE,
                     min(uv + offsetMask *  2.0 * TESR_RECIPROCAL_RES, maxuv));

    color.rgb /= 5.0;
    color.a = 1.0;
    return color;
}

// Gaussian blur along OffsetMask (H or V depending on caller).
//   OffsetMask: usually OffsetMaskH or OffsetMaskV
//   blurRadius: scalar multiplier for kernel radius
//   scale:      only blur pixels with uv <= scale (like original HLSL)
vec4 Blur(vec2 uv, vec2 OffsetMask, float blurRadius, float scale)
{
    // HLSL: clip ((uv <= scale) - 1);
    if (uv.x > scale || uv.y > scale)
        return vec4(0.0);

    // Center weight from HLSL
    float WeightSum = 0.114725602;
    vec4  color     = texture(BLUR_TEXTURE, uv) * WeightSum;

    for (int i = 0; i < cKernelSize; ++i)
    {
        float tap   = float(BlurOffsetsRaw[i]);
        vec2  uvOff = tap * TESR_RECIPROCAL_RES * OffsetMask * blurRadius;

        bool inside = ((uv.x + uvOff.x) < scale) &&
                      ((uv.y + uvOff.y) < scale);

        float isValid = inside ? 1.0 : 0.0;

        vec4 sampleColor = texture(BLUR_TEXTURE, uv + uvOff);
        color     += BlurWeights[i] * sampleColor * isValid;
        WeightSum += BlurWeights[i] * isValid;
    }

    color /= WeightSum;
    color.a = 1.0;
    return color;
}

