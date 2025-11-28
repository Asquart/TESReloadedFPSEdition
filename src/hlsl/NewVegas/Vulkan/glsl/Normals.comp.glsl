#version 450
#extension GL_GOOGLE_include_directive : enable

// Uses global depth + camera data
#include "Includes/Depth.glsl"
#include "Includes/Helpers.glsl"

layout(local_size_x = 16, local_size_y = 16) in;

// ------------------------------------------------------------
// Global frame set (set = 0)
// Depth.glsl already declares:
//
//   layout(set = 0, binding = 0) uniform sampler2D gDepth;
//   layout(set = 0, binding = 1) uniform GlobalFrameUBO {
//       mat4 Projection;
//       mat4 InvProjection;
//       mat4 View;
//       mat4 InvView;
//       vec4 DepthConstants;
//       vec4 CameraData;
//       vec4 CameraPosition;
//       vec4 ReciprocalResolution;   // <-- make sure this exists in your UBO
//   } uFrame;
//
// Helpers.glsl provides expand(), compress(), saturate(), etc.
// Depth.glsl provides readDepth(), reconstructPosition(), etc.
// ------------------------------------------------------------

// Optional: if you want to implement blur in a second pass, you’ll sample
// the existing normals through this sampler (global set=0, binding=2).
// For now it’s only used by BlurNormals(), not by main().
layout(set = 0, binding = 2) uniform sampler2D gNormals;

// Shorthand matching HLSL TESR_ReciprocalResolution
#define TESR_ReciprocalResolution (uFrame.ReciprocalResolution)

// ------------------------------------------------------------
// Local set (set = 1): output normals image
// ------------------------------------------------------------

layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D OutNormals;

// ------------------------------------------------------------
// Constants (ported from HLSL)
// ------------------------------------------------------------

const float dropTreshold = 0.82;
const float blurRadius   = 0.6;
const int   KernelSize   = 24;

// Purely constant weights – safe as a const array in GLSL
const float BlurNormalsWeights[KernelSize] = float[](
    0.019956226,
    0.021463016,
    0.032969806,
    0.044476596,
    0.055983386,
    0.067490176,
    0.078996966,
    0.080503756,
    0.092010546,
    0.105024126,
    0.116530916,
    0.128037706,
    0.128037706,
    0.116530916,
    0.105024126,
    0.092010546,
    0.080503756,
    0.078996966,
    0.067490176,
    0.055983386,
    0.044476596,
    0.032969806,
    0.021463016,
    0.019956226
);

// NOTE: in HLSL, BlurNormalsOffsets[] depended on TESR_ReciprocalResolution,
// which is *not* allowed as a const initializer in GLSL. So instead of a
// baked array, we reconstruct the offset for each sample in the loop.
//
// Original mapping was indices 0..23 → steps -12..-1, +1..+12 along X/Y.

// Helper: convert kernel index to signed step (-12..-1, +1..+12)
float kernelStep(int i) {
    // i: 0..23
    // 0..11 -> -12..-1
    // 12..23 -> +1..+12
    return (i < 12) ? float(i - 12) : float(i - 11);
}

// ------------------------------------------------------------
// ComputeNormals – port of HLSL version (view-space normals)
// ------------------------------------------------------------

vec3 ComputeNormalsAtUv(vec2 uv) {
    // improved normal reconstruction algorithm from 
    // https://gist.github.com/bgolus/a07ed65602c009d5e2f753826e8078a0

    // store coordinates at 1 and 2 pixels from center in all directions
    vec4 rightUv  = vec4(uv, uv) + vec4( 1.0,  0.0,  2.0,  0.0) * TESR_ReciprocalResolution.xyxy;
    vec4 leftUv   = vec4(uv, uv) + vec4(-1.0,  0.0, -2.0,  0.0) * TESR_ReciprocalResolution.xyxy;
    vec4 bottomUv = vec4(uv, uv) + vec4( 0.0,  1.0,  0.0,  2.0) * TESR_ReciprocalResolution.xyxy;
    vec4 topUv    = vec4(uv, uv) + vec4( 0.0, -1.0,  0.0, -2.0) * TESR_ReciprocalResolution.xyxy;

    float depth = readDepth(uv);

    // get depth values at 1 & 2 pixels offsets from current along the horizontal axis
    vec4 H = vec4(
        readDepth(rightUv.xy),
        readDepth(leftUv.xy),
        readDepth(rightUv.zw),
        readDepth(leftUv.zw)
    );

    // get depth values at 1 & 2 pixels offsets from current along the vertical axis
    vec4 V = vec4(
        readDepth(topUv.xy),
        readDepth(bottomUv.xy),
        readDepth(topUv.zw),
        readDepth(bottomUv.zw)
    );

    vec2 he = abs((2.0 * H.xy - H.zw) - depth);
    vec2 ve = abs((2.0 * V.xy - V.zw) - depth);

    // pick horizontal and vertical diff with the smallest depth difference from slopes
    vec3 centerPoint = reconstructPosition(uv);
    vec3 rightPoint  = reconstructPosition(rightUv.xy);
    vec3 leftPoint   = reconstructPosition(leftUv.xy);
    vec3 topPoint    = reconstructPosition(topUv.xy);
    vec3 bottomPoint = reconstructPosition(bottomUv.xy);

    vec3 left  = centerPoint - leftPoint;
    vec3 right = rightPoint - centerPoint;
    vec3 down  = centerPoint - bottomPoint;
    vec3 up    = topPoint - centerPoint;

    vec3 hDeriv = (he.x > he.y) ? left : right;
    vec3 vDeriv = (ve.x > ve.y) ? down : up;

    // get view space normal from the cross product of the best derivatives
    vec3 viewNormal = normalize(cross(vDeriv, hDeriv));

    return viewNormal;
}

// ------------------------------------------------------------
// BlurNormals – ported as a helper for later passes
// (not used in main() yet, because you’ll likely want separate
// compute dispatches and ping-pong images).
// ------------------------------------------------------------

vec3 BlurNormalsAtUv(vec2 uv, vec2 OffsetMask) {
    float WeightSum = 0.12 * saturate(1.0 - dropTreshold);

    // TESR_NormalsBuffer → gNormals
    vec3 normal = expand(texture(gNormals, uv).rgb);
    vec3 finalNormal = normal * WeightSum;

    float depth = readDepth(uv);
    float farZ  = uFrame.CameraData.y;

    float depthBasedRadius = abs(log(depth / farZ)) * blurRadius;
    float depthDrop        = (depth / farZ) * 7000.0;

    for (int i = 0; i < KernelSize; ++i) {
        float stepIdx = kernelStep(i);
        vec2 baseOffset = stepIdx * TESR_ReciprocalResolution.xy;

        vec2 uvOff = (baseOffset * OffsetMask) * depthBasedRadius;

        vec2 sampleUv = uv + uvOff;

        vec3 newNormal = expand(texture(gNormals, sampleUv).rgb);
        float depth2   = readDepth(sampleUv);

        float useForBlur = abs(depth - depth2) <= depthDrop ? 1.0 : 0.0;

        float weight = BlurNormalsWeights[i] *
                       saturate(dot(newNormal, normal) - dropTreshold * 0.75) *
                       useForBlur;

        finalNormal += weight * newNormal;
        WeightSum   += weight;
    }

    finalNormal /= max(WeightSum, 1e-6);
    return finalNormal;
}

// ------------------------------------------------------------
// main(): for now, only ComputeNormals (NVR first pass)
// ------------------------------------------------------------

void main() {
    ivec2 size = imageSize(OutNormals);
    ivec2 pix  = ivec2(gl_GlobalInvocationID.xy);

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    vec2 uv = (vec2(pix) + 0.5) / vec2(size);

    // First-stage reconstruction: view-space normal from depth
    vec3 viewNormal = ComputeNormalsAtUv(uv);

    // Store as compressed [-1..1] → [0..1], like original HLSL
    vec3 enc = compress(viewNormal);
    imageStore(OutNormals, pix, vec4(enc, 1.0));
}
