#version 450

// ==== CONFIG ====
#define KERNEL_SIZE 5
#define HALFRES     0
#define VIEWAO      0

// ==== UNIFORMS / CONSTANTS ====
// These buffer layouts must match what you upload from C++

layout(set = 0, binding = 6) uniform AOParams {
    vec4 TESR_AmbientOcclusionAOData;
    vec4 TESR_AmbientOcclusionData;
    vec4 TESR_ReciprocalResolution;
    vec4 TESR_FogData;
    vec4 TESR_FogColor;
};

const float PI = 3.14159265;

// AO constants
#define AOsamples       (TESR_AmbientOcclusionAOData.x)
#define AOstrength      (TESR_AmbientOcclusionAOData.y)
#define AOclamp         (TESR_AmbientOcclusionAOData.z)
#define AOrange         (TESR_AmbientOcclusionAOData.w)
#define AOangleBias     (TESR_AmbientOcclusionData.x)
#define AOlumThreshold  (TESR_AmbientOcclusionData.y)
#define blurDrop        (TESR_AmbientOcclusionData.z)
#define blurRadius      (TESR_AmbientOcclusionData.w)

const int startFade = 2000;
const int endFade   = 8000;

// ==== BINDINGS ====

// 0: previous AO (TESR_RenderedBuffer)
layout(set = 0, binding = 0) uniform sampler2D TESR_RenderedBuffer;

// 1: depth
layout(set = 0, binding = 1) uniform sampler2D TESR_DepthBuffer;

// 2: normals
layout(set = 0, binding = 2) uniform sampler2D TESR_NormalsBuffer;

// 3: blue noise
layout(set = 0, binding = 3) uniform sampler2D TESR_BlueNoiseSampler;

// 4: AO output
layout(set = 0, binding = 4, rgba8) uniform writeonly image2D AOOutput;

// Push constants for OffsetMask
layout(push_constant) uniform PushConsts {
    vec2 OffsetMask;
} pc;

// ==== HELPERS ====
// Put your Depth.hlsl / Helpers.hlsl / Normals.hlsl ports here
// reconstructPosition, projectPosition, readDepth, GetNormal, luma, invlerp, compress, etc.

float saturate(float x) { return clamp(x, 0.0, 1.0); }

vec3 random3(vec2 seed)
{
    // HLSL: tex2D(TESR_BlueNoiseSampler, (seed/256 + 0.5) / TESR_ReciprocalResolution.xy)
    vec2 uv = (seed / 256.0 + 0.5) / TESR_ReciprocalResolution.xy;
    return texture(TESR_BlueNoiseSampler, uv).xyz;
}

// same as HLSL expand(rand.xy) from Helpers.hlsl
vec3 expand(vec2 e) {
    return vec3(e * 2.0 - 1.0, 0.0);
}

float fogCoeff(float depth) {
    // saturate(invlerp(TESR_FogData.x, TESR_FogData.y, depth));
    float t = (depth - TESR_FogData.x) / (TESR_FogData.y - TESR_FogData.x);
    return saturate(t);
}

// ==== MAIN ====
layout(local_size_x = 16, local_size_y = 16) in;

void main()
{
    ivec2 imgSize = imageSize(AOOutput);
    ivec2 coord   = ivec2(gl_GlobalInvocationID.xy);

    if (coord.x >= imgSize.x || coord.y >= imgSize.y)
        return;

    vec2 uv = (vec2(coord) + 0.5) / vec2(imgSize);

    // float4 color = tex2D(TESR_RenderedBuffer, uv);
    vec4 color = texture(TESR_RenderedBuffer, uv);
    // color = OffsetMask.y?color:float(1).xxxx;
    if (pc.OffsetMask.y == 0.0)
        color = vec4(1.0);

#if HALFRES
    // clip ((IN.UVCoord.x < 0.5 && IN.UVCoord.y < 0.5)-1);
    // uv *= 2;
    if (!(uv.x < 0.5 && uv.y < 0.5))
        return;
    uv *= 2.0;
#endif

    float uRadius = abs(AOrange);
    float bias    = saturate(AOangleBias);

    vec3 origin = reconstructPosition(uv);
    if (origin.z > float(endFade)) {
        imageStore(AOOutput, coord, vec4(1.0));
        return;
    }

    vec3 normal = GetNormal(uv);

    float angle = -random3(uv).x / 2.0 * PI;
    vec3 kernelRotation = vec3(-sin(angle), cos(angle), 0.0);
    vec3 tangent   = normalize(kernelRotation - normal * dot(kernelRotation, kernelRotation));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < KERNEL_SIZE; ++i) {

        vec3 rand = random3(uv + float(i) * TESR_ReciprocalResolution.xx);
        vec3 sampleVector = vec3(expand(rand.xy).xy, rand.z) * vec3(pc.OffsetMask, 1.0);
        sampleVector = normalize(sampleVector) * tbn;

        sampleVector *= random3(uv * (float(i)/2.0));
        float scale = 1.0 + float(i) / float(KERNEL_SIZE);
        scale = mix(bias, 1.0, scale * scale);
        sampleVector *= scale;

        sampleVector *= (dot(normal, sampleVector) < 0.0) ? -1.0 : 1.0;
        vec3 samplePoint = origin + sampleVector * uRadius;

        vec3 screenSpaceSample = projectPosition(samplePoint);
        float sampleDepth = readDepth(screenSpaceSample.xy);
        float actualDepth = samplePoint.z;

        float distance = abs(actualDepth - sampleDepth);
        float rangeCheck = distance < uRadius ? 1.0 : 0.0;
        float influence = (sampleDepth < actualDepth ? 1.0 : 0.0) * rangeCheck;

        float v0 = 1.0 - distance * distance / (uRadius * uRadius);
        float v1 = 1.0 - distance / uRadius;
        float t  = (i < (KERNEL_SIZE / 4)) ? 1.0 : 0.0;
        influence *= mix(v0, v1, t);

        occlusion += influence;
    }

    occlusion = 1.0 - occlusion / float(KERNEL_SIZE) * AOstrength;

    float fogColor = luma(TESR_FogColor.rgb);
    float darkness = clamp(mix(occlusion, fogColor, fogCoeff(origin.z)), occlusion, 1.0);

    float fade = saturate((origin.z - float(startFade)) / float(endFade - startFade));
    darkness = mix(darkness, 1.0, fade) * color.x;

    vec4 outColor = vec4(darkness, darkness, 1.0, 1.0);
    imageStore(AOOutput, coord, outColor);
}
