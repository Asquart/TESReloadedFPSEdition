#version 450
#extension GL_GOOGLE_include_directive : enable

#include "Includes/Depth.comp.glsl"
#include "Includes/Helpers.comp.glsl"
#include "Includes/NormalsHelpers.comp.glsl"
#include "Includes/BlurDepth.comp.glsl"

// ============================================================================
// Config / defines
// ============================================================================
#define viewao  0     // 1 = show AO only in Combine
#define halfres 0     // keep for parity, currently unused unless you enable it
const int   kernelSize = 5;
const float startFade  = 2000.0;
const float endFade    = 8000.0;

// ============================================================================
// UBO aliases (from GlobalLayout / GlobalFrameUBO)
// ============================================================================
#define TESR_AmbientOcclusionAOData  (uFrame.TESR_AmbientOcclusionAOData)
#define TESR_AmbientOcclusionData    (uFrame.TESR_AmbientOcclusionData)
#define TESR_ReciprocalResolution    (uFrame.TESR_ReciprocalResolution)
#define TESR_FogData                 (uFrame.TESR_FogData)
#define TESR_FogColor                (uFrame.TESR_FogColor)

#define AOsamples       (TESR_AmbientOcclusionAOData.x)
#define AOstrength      (TESR_AmbientOcclusionAOData.y)
#define AOclamp         (TESR_AmbientOcclusionAOData.z)
#define AOrange         (TESR_AmbientOcclusionAOData.w)

#define AOangleBias     (TESR_AmbientOcclusionData.x)
#define AOlumThreshold  (TESR_AmbientOcclusionData.y)
#define AOBlurDrop      (TESR_AmbientOcclusionData.z)
#define AOBlurRadius    (TESR_AmbientOcclusionData.w)

// ============================================================================
// Resources
//  - set = 0 is reserved for GlobalLayout: depth, normals, uFrame, etc.
//  - set = 1 is AO / effect specific
// ============================================================================

// Blue noise used by SSAO core
layout(set = 1, binding = 0) uniform sampler2D gBlueNoise;

// AO ping-pong images (two textures total, both readable and writable)
layout(set = 1, binding = 1, r16f) uniform image2D gAO0Image; // also used as final AO
layout(set = 1, binding = 2, r16f) uniform image2D gAO1Image;

// One sampler for AO; C++ side binds this to gAO0 or gAO1 view depending on pass
layout(set = 1, binding = 3) uniform sampler2D gAOSampler;

// Scene color in & out
layout(set = 1, binding = 4) uniform sampler2D gSceneColorIn;          // pre-AO color
layout(set = 1, binding = 5, rgba16f) uniform image2D gSceneColorOut;  // post-AO color

// ============================================================================
// Push constants: which pass are we running?
// 0 = SSAO (fused 2-pass)
// 1 = Blur X
// 2 = Blur Y
// 3 = Combine
// ============================================================================
layout(push_constant) uniform PassParams {
    int passId;
} uPass;

// ============================================================================
// Helpers
// ============================================================================
vec3 randomBlueNoise(vec2 seed)
{
    // HLSL: (seed/256 + 0.5) / TESR_ReciprocalResolution.xy
    vec2 coord = (seed / 256.0 + 0.5) / TESR_ReciprocalResolution.xy;
    return texture(gBlueNoise, coord).xyz;
}

float fogCoeff(float depth)
{
    // HLSL: saturate(invlerp(TESR_FogData.x, TESR_FogData.y, depth));
    return saturate(invlerp(TESR_FogData.x, TESR_FogData.y, depth));
}

// ============================================================================
// SSAO core (1 pass) – direct port of HLSL SSAO() math without color.x multiply
// ============================================================================
float SSAO_Core(vec2 uv, vec2 offsetMask)
{
#if halfres
    // HLSL: clip((uv.x < 0.5 && uv.y < 0.5) - 1);
    if (!(uv.x < 0.5 && uv.y < 0.5))
        return 1.0;
    uv *= 2.0;
#endif

    float uRadius = abs(AOrange);
    float bias    = saturate(AOangleBias);

    // View-space origin from depth
    vec3 origin = reconstructPosition(uv);
    if (origin.z > endFade)
        return 1.0;

    // Reorient kernel by normal
    vec3 normal = normalize(GetNormal(uv, gNormals));

    // random angle between 0 and 90 degrees
    float angle = -randomBlueNoise(uv).x * 0.5 * PI;
    vec3  kernelRotation = vec3(-sin(angle), cos(angle), 0.0);
    vec3  tangent   = normalize(kernelRotation - normal * dot(kernelRotation, normal));
    vec3  bitangent = cross(normal, tangent);

    // mat3 columns: tangent, bitangent, normal
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < kernelSize; ++i)
    {
        // generate random samples in a unit sphere
        vec3 rand = randomBlueNoise(uv + float(i) * TESR_ReciprocalResolution.xx);

        // float3 sampleVector = float3(expand(rand.xy), rand.z) * float3(OffsetMask, 1);
        vec3 sampleVector = vec3(expand(rand.xy), rand.z) * vec3(offsetMask, 1.0);

        sampleVector = normalize(sampleVector);
        sampleVector = tbn * sampleVector;

        // randomize distance to center
        sampleVector *= randomBlueNoise(uv * (float(i) * 0.5)).z;

        float scale = 1.0 + float(i) / float(kernelSize);
        scale = mix(bias, 1.0, scale * scale);
        sampleVector *= scale;

        // flip if inside geometry
        if (dot(normal, sampleVector) < 0.0)
            sampleVector = -sampleVector;

        vec3 samplePoint = origin + sampleVector * uRadius;

        // project sample and compare depth
        vec3 screenSpaceSample = projectPosition(samplePoint);
        float sampleDepth = readDepth(screenSpaceSample.xy);
        float actualDepth = samplePoint.z;

        float distance   = abs(actualDepth - sampleDepth);
        float rangeCheck = distance < uRadius ? 1.0 : 0.0;
        float influence  = (sampleDepth < actualDepth ? 1.0 : 0.0) * rangeCheck;

        // stronger curve near viewer
        float inner  = 1.0 - distance * distance / (uRadius * uRadius + 1e-6);
        float outer  = 1.0 - distance / (uRadius + 1e-6);
        float useInner = (i < kernelSize / 4) ? 1.0 : 0.0;
        float falloff  = mix(outer, inner, useInner);

        influence *= falloff;
        occlusion += influence;
    }

    // occlusion = 1.0 - occlusion/kernelSize * AOstrength;
    occlusion = 1.0 - occlusion / float(kernelSize) * AOstrength;

    // fog mixing & depth fade
    float fogColorLuma = luma(TESR_FogColor.rgb);
    float darkness = clamp(
        mix(occlusion, fogColorLuma, fogCoeff(origin.z)),
        occlusion,
        1.0
    );

    float fade = saturate(invlerp(startFade, endFade, origin.z));
    darkness = mix(darkness, 1.0, fade);

    // In original SSAO(), this is multiplied by color.x.
    // Here we return per-pass darkness; the caller multiplies passes.
    return darkness;
}

// ============================================================================
// Blur wrapper – calls DepthBlur from BlurDepth.comp.glsl on AO sampler
// ============================================================================
vec4 BlurAO_Sample(vec2 uv, vec2 offsetMask)
{
    // DepthBlur signature assumed:
    // vec4 DepthBlur(vec2 uv, sampler2D buffer, vec2 offsetMask, float blurRadius, float depthDrop, float endFade);
    return DepthBlur(uv, gAOSampler, offsetMask, AOBlurRadius, AOBlurDrop, endFade);
}

// ============================================================================
// Combine – port of HLSL Combine()
// ============================================================================
vec4 AO_Combine(vec2 uv)
{
    // scene color
    vec3 color = texture(gSceneColorIn, uv).rgb;
    color = pows(color, 2.2);   // linearise

    // AO from final AO buffer: in C++ bind gAOSampler to the final AO image (gAO0Image)
    float aoSample = texture(gAOSampler, uv).r;
    float ao = mix(AOclamp, 1.0, aoSample);

    float luminance = luma(color);
    float lt = luminance - AOlumThreshold;
    luminance = saturate(lt * 3.0);
    ao = mix(ao, 1.0, luminance);
    color *= ao;

#if viewao
    return vec4(ao, ao, ao, 1.0);
#else
    color = pows(color, 1.0 / 2.2);  // back to gamma
    return vec4(color, 1.0);
#endif
}

// ============================================================================
// Entry point: 4 passes in one shader, selected via uniform push constant
//  (pass selection is uniform per dispatch, so no warp divergence issues).
// ============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    ivec2 pix = ivec2(gl_GlobalInvocationID.xy);

    ivec2 size;
    if (uPass.passId == 3) {
        // Combine uses scene color size
        size = imageSize(gSceneColorOut);
    } else {
        // SSAO / Blur use AO size (AO0)
        size = imageSize(gAO0Image);
    }

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    vec2 uv = (vec2(pix) + 0.5) / vec2(size);

    if (uPass.passId == 0) {
        // ---------------------------------------------------------------------
        // PASS 0: Fused SSAO – write to gAO0Image
        //   HLSL did two passes SSAO(io.xy) and SSAO(io.yx) and multiplied them.
        //   Here we compute both in one shader and multiply: AO = d1 * d2.
        // ---------------------------------------------------------------------
        float d1 = SSAO_Core(uv, vec2(1.0, 0.0));
        float d2 = SSAO_Core(uv, vec2(0.0, 1.0));
        float ao = d1 * d2;
        imageStore(gAO0Image, pix, vec4(ao, ao, ao, 1.0));

    } else if (uPass.passId == 1) {
        // ---------------------------------------------------------------------
        // PASS 1: Blur X (NormalBlurRChannel(io.xy))
        //   C++ side must bind gAOSampler to view of gAO0Image.
        //   We read from sampler (current AO) and write to gAO1Image.
        // ---------------------------------------------------------------------
        vec4 blurred = BlurAO_Sample(uv, vec2(1.0, 0.0));
        imageStore(gAO1Image, pix, blurred);

    } else if (uPass.passId == 2) {
        // ---------------------------------------------------------------------
        // PASS 2: Blur Y (NormalBlurRChannel(io.yx))
        //   C++ side must bind gAOSampler to view of gAO1Image.
        //   We read from sampler and write final AO back into gAO0Image.
        // ---------------------------------------------------------------------
        vec4 blurred = BlurAO_Sample(uv, vec2(0.0, 1.0));
        imageStore(gAO0Image, pix, blurred);

    } else if (uPass.passId == 3) {
        // ---------------------------------------------------------------------
        // PASS 3: Combine AO into scene color
        //   C++ side must bind gAOSampler to final AO (gAO0Image).
        // ---------------------------------------------------------------------
        vec4 outColor = AO_Combine(uv);
        imageStore(gSceneColorOut, pix, outColor);
    }
}
