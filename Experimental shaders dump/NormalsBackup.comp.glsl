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
// PASS 0: ComputeNormals (from Depth.hlsl)
// ------------------------------------------------------------------
vec4 ComputeNormalsAtPixel(ivec2 pix)
{
    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv = (vec2(pix) + 0.5) * texel;

    vec4 rightUv  = uv.xyxy + vec4( 1.0, 0.0,  2.0, 0.0) * texel.xyxy;
    vec4 leftUv   = uv.xyxy + vec4(-1.0, 0.0, -2.0, 0.0) * texel.xyxy;
    vec4 bottomUv = uv.xyxy + vec4( 0.0, 1.0,  0.0, 2.0) * texel.xyxy;
    vec4 topUv    = uv.xyxy + vec4( 0.0,-1.0,  0.0,-2.0) * texel.xyxy;

    vec2 centerDepthRaw = readDepthRaw(uv);
    float depth = readDepthFromRaw(centerDepthRaw);

    vec2 rawDepthRightXY = readDepthRaw(rightUv.xy);
    vec2 rawDepthLeftXY = readDepthRaw(leftUv.xy);
    vec2 rawDepthRightZW = readDepthRaw(rightUv.zw);
    vec2 rawDepthLeftZW = readDepthRaw(leftUv.zw);
    vec2 rawDepthTopXY = readDepthRaw(topUv.xy);
    vec2 rawDepthBottomXY = readDepthRaw(bottomUv.xy);
    vec2 rawDepthTopZW = readDepthRaw(topUv.zw);
    vec2 rawDepthBottomZW = readDepthRaw(bottomUv.zw);

    vec4 H = vec4(
        readDepthFromRaw(rawDepthRightXY),
        readDepthFromRaw(rawDepthLeftXY),
        readDepthFromRaw(rawDepthRightZW),
        readDepthFromRaw(rawDepthLeftZW)
    );

    vec4 V = vec4(
        readDepthFromRaw(rawDepthTopXY),
        readDepthFromRaw(rawDepthBottomXY),
        readDepthFromRaw(rawDepthTopZW),
        readDepthFromRaw(rawDepthBottomZW)
    );

    vec2 he = abs((2.0 * H.xy - H.zw) - depth);
    vec2 ve = abs((2.0 * V.xy - V.zw) - depth);

    vec3 centerPoint = reconstructPositionFromRawDepth(uv, centerDepthRaw);
    vec3 rightPoint  = reconstructPositionFromRawDepth(rightUv.xy, rawDepthRightXY);
    vec3 leftPoint   = reconstructPositionFromRawDepth(leftUv.xy, rawDepthLeftXY);
    vec3 topPoint    = reconstructPositionFromRawDepth(topUv.xy, rawDepthTopXY);
    vec3 bottomPoint = reconstructPositionFromRawDepth(bottomUv.xy, rawDepthBottomXY);

    vec3 left  = centerPoint - leftPoint;
    vec3 right = rightPoint - centerPoint;
    vec3 down  = centerPoint - bottomPoint;
    vec3 up    = topPoint - centerPoint;

    vec3 hDeriv = (he.x > he.y) ? left : right;
    vec3 vDeriv = (ve.x > ve.y) ? down : up;

    vec3 viewNormal = normalize(cross(vDeriv, hDeriv));
    return vec4(viewNormal, 1.0);
}


// ------------------------------------------------------------------
// PASS 1/2: BlurNormals (H or V), from BlurNormals(OffsetMask)
// ------------------------------------------------------------------
vec3 BlurNormalsAtPixel(ivec2 pix, vec2 OffsetMask)
{
    vec2 recip = uFrame.TESR_ReciprocalResolution.xy;

    // same convention as ComputeNormalsAtPixel / HLSL
    vec2 uv = (vec2(pix) + 0.5) * recip;

    float WeightSum = 0.12 * saturate(1.0 - dropTreshold);

    vec3 normal = expand(texture(uNormalsIn, uv).rgb);
    vec3 finalNormal = normal * WeightSum;

    float depth = readDepth(uv);
    float depthBasedRadius = abs(log(depth / farZ)) * blurRadius;
    float depthDrop = (depth / farZ) * 7000.0;

    for (int i = 0; i < KernelSize; ++i) {
        float k = float(i - 12); // -12..+11

        vec2 baseOffset = vec2(k) * recip;                // matches BlurNormalsOffsets[i]
        vec2 uvOff      = uv + (baseOffset * OffsetMask) * depthBasedRadius;

        vec3 newNormal   = expand(texture(uNormalsIn, uvOff).rgb);
        float depth2     = readDepth(uvOff);
        float useForBlur = (abs(depth - depth2) <= depthDrop) ? 1.0 : 0.0;

        float weight = BlurNormalsWeights[i] *
                       saturate(dot(newNormal, normal) - dropTreshold * 0.75) *
                       useForBlur;

        finalNormal += weight * newNormal;
        WeightSum   += weight;
    }

    finalNormal /= WeightSum;
    return finalNormal;
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

    vec2 offsetMask = (pc.Pass == 1u) ? OffsetMaskH : OffsetMaskV;
    vec3 blurred = BlurNormalsAtPixel(pix, offsetMask);
    imageStore(uNormalsOut, pix, vec4(compress(blurred), 1.0));
}

