#version 450
#extension GL_GOOGLE_include_directive : enable

#include "Includes/Depth.comp.glsl"
#include "Includes/Helpers.comp.glsl"

layout(local_size_x = 16, local_size_y = 16) in;

// Output of the current pass
layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D uNormalsOut;

// Input normals texture (used in blur passes; bound but unused in pass 0)
layout(set = 1, binding = 1) uniform sampler2D uNormalsIn;

// Push constants: which pass we are running
layout(push_constant) uniform PushConstants {
    uint Pass;      // 0 = reconstruct, 1 = blur horizontal, 2 = blur vertical
} pc;


// ------------------------------------------------------------------
// HLSL constants, translated
// ------------------------------------------------------------------
const float dropTreshold = 0.82;
const float blurRadius   = 0.6;
const int   KernelSize   = 24;

const float BlurNormalsWeights[KernelSize] = float[KernelSize](
    0.019956226, 0.021463016, 0.032969806, 0.044476596,
    0.055983386, 0.067490176, 0.078996966, 0.080503756,
    0.092010546, 0.105024126, 0.116530916, 0.128037706,
    0.128037706, 0.116530916, 0.105024126, 0.092010546,
    0.080503756, 0.078996966, 0.067490176, 0.055983386,
    0.044476596, 0.032969806, 0.021463016, 0.019956226
);

// OffsetMask equivalents
const vec2 OffsetMaskH = vec2(1.0, 0.0);
const vec2 OffsetMaskV = vec2(0.0, 1.0);

// ------------------------------------------------------------------
// New plane-ish reconstruction + à-trous filter tunables
// ------------------------------------------------------------------

// Plane-fit style reconstruction around center (view-space depth units)
const float PlaneDepthSigma  = 0.15;
const float PlaneDepthCutoff = 0.5;

// 1D à-trous kernel: 5-tap binomial (1,4,6,4,1) normalized
const float AtrousKernel1D[5] = float[5](
    1.0 / 16.0,
    4.0 / 16.0,
    6.0 / 16.0,
    4.0 / 16.0,
    1.0 / 16.0
);

// Bilateral terms for à-trous
const float AtrousDepthSigma  = 0.02;  // depth falloff
const float AtrousNormalSharp = 32.0;  // higher = sharper normal edges



// ------------------------------------------------------------------
// PASS 0: ComputeNormals (from Depth.hlsl)
// ------------------------------------------------------------------
vec4 ComputeNormalsAtPixel(ivec2 pix)
{
    ivec2 imgSize = imageSize(uNormalsOut);
    if (pix.x < 0 || pix.y < 0 || pix.x >= imgSize.x || pix.y >= imgSize.y) {
        return vec4(0.0, 0.0, 1.0, 1.0);
    }

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    // Center depth + position (view space)
    float centerDepth = readDepth(uv);
    vec3  centerPos   = reconstructPosition(uv);

    // 4 direct neighbours (right, left, down, up)
    const ivec2 OFFS[4] = ivec2[4](
        ivec2( 1,  0),
        ivec2(-1,  0),
        ivec2( 0,  1),
        ivec2( 0, -1)
    );

    vec3  neighPos[4];
    float neighDepth[4];

    for (int i = 0; i < 4; ++i) {
        ivec2 q = pix + OFFS[i];
        q.x = clamp(q.x, 0, imgSize.x - 1);
        q.y = clamp(q.y, 0, imgSize.y - 1);

        vec2 uvN = (vec2(q) + 0.5) * texel;
        neighDepth[i] = readDepth(uvN);
        neighPos[i]   = reconstructPosition(uvN);
    }

    vec3  nAccum = vec3(0.0);
    float wAccum = 0.0;

    // Helper to accumulate one triangle (center, ia, ib)
    for (int t = 0; t < 4; ++t) {
        int ia, ib;
        if      (t == 0) { ia = 0; ib = 2; } // right, down
        else if (t == 1) { ia = 2; ib = 1; } // down, left
        else if (t == 2) { ia = 1; ib = 3; } // left, up
        else             { ia = 3; ib = 0; } // up, right

        float da = abs(neighDepth[ia] - centerDepth);
        float db = abs(neighDepth[ib] - centerDepth);

        // Work in relative depth: Δz / z
        float zRef = max(centerDepth, 1.0); // avoid div by ~0 near camera

        float relA = da / zRef;
        float relB = db / zRef;

        // Tunables (put these near top of file)
        const float PlaneDepthSigma  = 0.15; // was 0.02, way too small for your scale
        const float PlaneDepthCutoff = 0.5;  // reject only really big jumps (~50% change)

        // Large *relative* jumps = edge, skip this triangle
        if (relA > PlaneDepthCutoff || relB > PlaneDepthCutoff)
            continue;

        // Simple bilateral-style weight
        float wa = exp(-relA / PlaneDepthSigma);
        float wb = exp(-relB / PlaneDepthSigma);
        float w  = min(wa, wb);


        vec3 v1 = neighPos[ia] - centerPos;
        vec3 v2 = neighPos[ib] - centerPos;
        vec3 n  = cross(v2, v1);

        if (dot(n, n) < 1e-8)
            continue;

        n = normalize(n);
        nAccum += w * n;
        wAccum += w;
    }

    vec3 finalN;
    if (wAccum > 0.0)
        finalN = normalize(nAccum / wAccum);
    else
        finalN = vec3(0.0, 0.0, 1.0); // fallback

    return vec4(finalN, 1.0);
}



// ------------------------------------------------------------------
// PASS 1/2: BlurNormals (H or V), from BlurNormals(OffsetMask)
// ------------------------------------------------------------------
vec3 BlurNormalsAtPixel(ivec2 pix, vec2 OffsetMask)
{
    ivec2 imgSize = imageSize(uNormalsOut);
    if (pix.x < 0 || pix.y < 0 || pix.x >= imgSize.x || pix.y >= imgSize.y) {
        return vec3(0.0, 0.0, 1.0);
    }

    // Decide à-trous step from pass index:
    //   Pass 1 -> step = 1
    //   Pass 2 -> step = 2
    uint passIndex = pc.Pass;
    int  step      = (passIndex == 1u) ? 1 : 2;

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    vec3  centerN = expand(texture(uNormalsIn, uv).rgb);
    float centerD = readDepth(uv);

    vec3  sumN = vec3(0.0);
    float sumW = 0.0;

    for (int j = -2; j <= 2; ++j) {
        for (int i = -2; i <= 2; ++i) {
            ivec2 q = pix + ivec2(i * step, j * step);

            q.x = clamp(q.x, 0, imgSize.x - 1);
            q.y = clamp(q.y, 0, imgSize.y - 1);

            vec2 uvQ = (vec2(q) + 0.5) * texel;

            vec3  nQ = expand(texture(uNormalsIn, uvQ).rgb);
            float dQ = readDepth(uvQ);

            float wSpatial = AtrousKernel1D[i + 2] * AtrousKernel1D[j + 2];

            float dd     = abs(dQ - centerD);
            float wDepth = exp(-dd / AtrousDepthSigma);

            float ndot = max(dot(centerN, nQ), 0.0);

            // Cheap polynomial "sharpening": ndot^4 or ndot^8 instead of pow
            float wNorm = ndot * ndot;      // ndot^2
            wNorm *= wNorm;                 // ndot^4

            float w = wSpatial * wDepth * wNorm;

            sumN += w * nQ;
            sumW += w;
        }
    }

    vec3 outN;
    if (sumW > 0.0)
        outN = normalize(sumN / sumW);
    else
        outN = centerN;

    return outN;
}



// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------
void main()
{
    ivec2 size = imageSize(uNormalsOut);
    ivec2 pix  = ivec2(gl_GlobalInvocationID.xy);

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    if (pc.Pass == 0u) {
        vec4 n = ComputeNormalsAtPixel(pix);
        imageStore(uNormalsOut, pix, compress(n));
        return;
    }

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    if (pc.Pass == 1u)
    {
        // One actual atrous blur pass (step=1)
        vec3 blurred = BlurNormalsAtPixel(pix, vec2(0.0)); // OffsetMask unused
        imageStore(uNormalsOut, pix, vec4(compress(blurred), 1.0));
    }
    else // pc.Pass == 2u
    {
        // Just copy (very cheap)
        vec3 n = expand(texture(uNormalsIn, uv).rgb);
        imageStore(uNormalsOut, pix, vec4(compress(n), 1.0));
}
}

