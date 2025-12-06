#version 450
#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// -----------------------------------------------------------------------------
// Includes: global layout, depth helpers, generic helpers, normal helpers
// -----------------------------------------------------------------------------

#include "Includes/Depth.comp.glsl"          // gDepth, uFrame, nearZ/farZ, reconstructPosition(), etc.
#include "Includes/Helpers.comp.glsl"        // saturate, UVToPixel, etc.
#include "Includes/NormalsHelpers.comp.glsl" // GetNormal()

// -----------------------------------------------------------------------------
// SET 0 – global resources
//   NOTE: gDepth + gNormals + uFrame are declared in GlobalLayout via Depth.comp.glsl.
// -----------------------------------------------------------------------------

// Scene color / albedo for IL sampling.
// Make sure your global descriptor set binds this at set = 0, binding = 3.
layout(set = 0, binding = 3) uniform sampler2D gColor;

// -----------------------------------------------------------------------------
// SET 1 – MXAO-specific surfaces (ping-pong + final composite target)
// -----------------------------------------------------------------------------

layout(set = 1, binding = 0, rgba16f) readonly  uniform image2D mxaoIn;
layout(set = 1, binding = 1, rgba16f) writeonly uniform image2D mxaoOut;

// -----------------------------------------------------------------------------
// Push constants – MXAO-only parameters
//   All global stuff (resolution, matrices, near/farZ, etc.) comes from uFrame.
// -----------------------------------------------------------------------------

layout(push_constant) uniform MXAOPush
{
    int   Pass;                 // 0: AO gen, 1: blur1, 2: blur2 + shape + composite

    // Sampling / quality
    uint  GlobalSamplePreset;   // qUINT MXAO_GLOBAL_SAMPLE_QUALITY_PRESET
    uint  BaseSampleCount;      // MXAO.samples base value

    // AO kernel
    float SampleRadius;         // MXAO_SAMPLE_RADIUS
    float SampleNormalBias;     // MXAO_SAMPLE_NORMAL_BIAS

    // AO / IL strength & curve
    float SSAOAmount;           // MXAO_SSAO_AMOUNT
    float SSILAmount;           // MXAO_SSIL_AMOUNT
    float Power;                // MXAO_POWER

    // Depth fade (0..1 in view-space normalized depth)
    float FadeDepthStart;       // MXAO_FADE_DEPTH_START
    float FadeDepthEnd;         // MXAO_FADE_DEPTH_END

    // Blur configuration
    float RenderScale;          // MXAO_GLOBAL_RENDER_SCALE (for low-res AO later if you want)
    float BlurRadius1;          // SpatialFilter1 kernel radius
    float BlurRadius2;          // SpatialFilter2 kernel radius
    uint  BlurSteps1;           // blur taps 1 (1..8)
    uint  BlurSteps2;           // blur taps 2 (1..8)

    // Toggles / misc
    uint  EnableIL;             // MXAO_ENABLE_IL
    uint  TwoLayer;             // MXAO_TWO_LAYER
    uint  HighQuality;          // MXAO_HIGH_QUALITY
    uint  DebugView;            // 0 = normal, 1 = AO override

    // Two-layer AO intensity
    float AmountCoarse;         // MXAO_AMOUNT_COARSE
    float AmountFine;           // MXAO_AMOUNT_FINE
} pc;

// -----------------------------------------------------------------------------
// Small helpers using your global UBO
// -----------------------------------------------------------------------------

vec2 GetResolution()
{
    // TESR_ReciprocalResolution.xy = (1/width, 1/height)
    return vec2(
        1.0 / uFrame.TESR_ReciprocalResolution.x,
        1.0 / uFrame.TESR_ReciprocalResolution.y
    );
}

vec2 GetInvResolution()
{
    return uFrame.TESR_ReciprocalResolution.xy;
}

vec2 GetAspect(vec2 resolution)
{
    return vec2(1.0, resolution.y / resolution.x);
}

// -----------------------------------------------------------------------------
// qUINT sample_parameter_setup()
// -----------------------------------------------------------------------------

void sampleParameterSetup(float scaledDepth, float layerId, int sampleCount,
                          out float scaledRadius, out float falloffFactor)
{
    scaledRadius  = 0.25 * pc.SampleRadius / (float(sampleCount) * (scaledDepth + 2.0));
    falloffFactor = -1.0 / (pc.SampleRadius * pc.SampleRadius);

    if (pc.TwoLayer != 0u)
    {
        float secondaryRadius = pc.SampleRadius * 2.0;
        secondaryRadius += 1e-6;

        scaledRadius  *= mix(1.0, secondaryRadius, layerId);
        falloffFactor *= mix(1.0,
                             1.0 / (secondaryRadius * secondaryRadius),
                             layerId);
    }
}

// -----------------------------------------------------------------------------
// Pass 0 – AO/IL generation (PS_AmbientObscurance-like)
// -----------------------------------------------------------------------------

vec4 evaluateMXAO(ivec2 pix, vec2 invRes, vec2 aspect)
{
    vec2 uv = (vec2(pix) + 0.5) * invRes;

    // View-space position from your Depth.comp.glsl helper
    vec3 position = reconstructPosition(uv);
    vec3 normal   = GetNormal(uv, gNormals);

    // qUINT jitter pattern
    vec2 mod4     = floor(mod(vec2(pix), vec2(4.0)) + vec2(0.1));
    float jitter  = dot(mod4, vec2(0.0625, 0.25)) + 0.0625;

    float layerId = float((pix.x + pix.y) & 1);

    float viewDepth       = readDepth(uv);     // view-space depth in world units
    float depthNormalized = viewDepth / farZ;  // 0..1
    position             += normal * depthNormalized;

    int sampleCount = int(pc.BaseSampleCount);
    if (pc.GlobalSamplePreset == 7u)
    {
        sampleCount = int(2.0 + floor(0.05 * pc.SampleRadius / max(depthNormalized, 1e-4)));
    }

    float scaledRadius;
    float falloffFactor;
    sampleParameterSetup(position.z, layerId, sampleCount, scaledRadius, falloffFactor);

    vec2 sampleDir;
    float s, c;
    sincos(2.3999632 * 16.0 * jitter, s, c);
    sampleDir = vec2(s, c) * scaledRadius;

    vec4 accum = vec4(0.0);

    for (int i = 0; i < sampleCount; ++i)
    {
        float fi   = float(i);
        vec2 tapUV = uv + sampleDir * aspect * (fi + jitter);

        sincos(2.3999632 * 16.0, s, c);
        sampleDir = vec2(sampleDir.x * c - sampleDir.y * s,
                         sampleDir.x * s + sampleDir.y * c);

        if (tapUV.x < 0.0 || tapUV.y < 0.0 || tapUV.x > 1.0 || tapUV.y > 1.0)
            continue;

        float sampleMip = saturate(scaledRadius * fi * 20.0) * 3.0;

        vec3 samplePos = reconstructPosition(tapUV);
        vec3 delta     = samplePos - position;

        float v2 = max(dot(delta, delta), 1e-6);
        float vn = dot(delta, normal) * inversesqrt(v2);

        float sampleAO = saturate(1.0 + falloffFactor * v2) *
                         saturate(vn - pc.SampleNormalBias);

        if (pc.EnableIL != 0u)
        {
            if (sampleAO > 0.1)
            {
                vec3 sampleColor  = textureLod(gColor,   tapUV, sampleMip).rgb;
                vec3 sampleNormal = GetNormal(tapUV, gNormals);

                vec3 il = sampleColor * sampleAO;
                il     *= 0.5 + 0.5 * saturate(dot(sampleNormal, -delta * v2));

                accum += vec4(il, sampleAO);
            }
        }
        else
        {
            accum.w += sampleAO;
        }
    }

    float normFactor = (1.0 - pc.SampleNormalBias) * float(sampleCount);
    accum /= max(normFactor, 1e-6);
    accum *= 2.0;
    accum  = saturate(accum);
    accum  = sqrt(accum);

    float amount = (pc.TwoLayer != 0u)
                 ? mix(pc.AmountCoarse, pc.AmountFine, layerId)
                 : pc.AmountCoarse;
    accum *= amount;

    float ao = pow(accum.w, pc.Power);
    accum.w  = ao;

    return accum;   // IL in rgb, AO in a
}

// -----------------------------------------------------------------------------
// Blur support – SpatialFilter1/2-style
// -----------------------------------------------------------------------------

struct BlurData
{
    vec4 key;   // AO/IL
    vec4 mask;  // normal.xyz + depth/mask in .w (approx)
};

void blurSample(BlurData center, ivec2 tapPix, vec2 invRes,
                out BlurData tap, out float tapWeight)
{
    vec2 uv = (vec2(tapPix) + 0.5) * invRes;

    tap.key  = imageLoad(mxaoIn, tapPix);

    vec4 nTex = texture(gNormals, uv);
    tap.mask.xyz = GetNormal(uv, gNormals);  // [-1..1]
    tap.mask.w   = nTex.w;                   // whatever you packed into A (depth/mask)

    float depthTerm  = saturate(1.0 - abs(tap.mask.w - center.mask.w));
    float normalTerm = saturate(dot(tap.mask.xyz, center.mask.xyz) * 16.0 - 15.0);
    tapWeight        = depthTerm * normalTerm;
}

vec4 spatialBlur(ivec2 pix,
                 vec2 invRes,
                 vec2 bufferSize,
                 float inputScale,
                 float radius,
                 uint blurSteps)
{
    vec2 uv = (vec2(pix) + 0.5) * invRes;

    BlurData center;
    center.key  = imageLoad(mxaoIn, pix);
    vec4 nTex   = texture(gNormals, uv);
    center.mask.xyz = GetNormal(uv, gNormals);
    center.mask.w   = nTex.w;

    const vec2 offsets[8] = vec2[8](
        vec2( 1.5,  0.5), vec2(-1.5, -0.5),
        vec2(-0.5,  1.5), vec2( 0.5, -1.5),
        vec2( 1.5,  2.5), vec2(-1.5, -2.5),
        vec2(-2.5,  1.5), vec2( 2.5, -1.5)
    );

    vec2 blurOffsetScale = invRes / inputScale * radius;

    vec4 blursum     = vec4(0.0);
    vec4 blursumNoW  = vec4(0.0);
    float blurWeight = 0.0;

    for (uint i = 0u; i < blurSteps && i < 8u; ++i)
    {
        ivec2 tapPix = pix + ivec2(round(offsets[i] * blurOffsetScale * bufferSize));

        if (tapPix.x < 0 || tapPix.y < 0 ||
            tapPix.x >= int(bufferSize.x) || tapPix.y >= int(bufferSize.y))
            continue;

        BlurData tap;
        float w;
        blurSample(center, tapPix, invRes, tap, w);

        blurWeight += w;
        blursum    += tap.key * w;
        blursumNoW += tap.key;
    }

    if (blurWeight > 0.0)
        blursum /= blurWeight;

    blursumNoW /= float(1u + blurSteps);

    float t = (blurWeight < 2.0) ? 1.0 : 0.0;
    return mix(blursum, blursumNoW, t);
}

// -----------------------------------------------------------------------------
// Final shaping (SpatialFilter2-like, still separate from scene color)
// -----------------------------------------------------------------------------

vec4 shapeAOIL(ivec2 pix, vec2 invRes, vec2 bufferSize)
{
    vec4 ssil_ssao = spatialBlur(
        pix, invRes, bufferSize,
        1.0, pc.BlurRadius2, pc.BlurSteps2
    );
    ssil_ssao      = ssil_ssao * ssil_ssao;

    vec2 uv        = (vec2(pix) + 0.5) * invRes;
    float viewDepth = readDepth(uv);
    float depthNorm = viewDepth / farZ;

    vec3 il = ssil_ssao.rgb * pc.SSILAmount * 2.0;

    float ao;
    if (pc.HighQuality == 0u)
        ao = 1.0 - pow(saturate(1.0 - ssil_ssao.a), pc.SSAOAmount * 2.0);
    else
        ao = 1.0 - pow(saturate(1.0 - ssil_ssao.a), pc.SSAOAmount);

    float fade = 1.0 - smoothstep(pc.FadeDepthStart, pc.FadeDepthEnd, depthNorm);
    il  *= fade * 2.0;
    ao  *= fade;

    return vec4(il, ao);  // IL.rgb, AO.a
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

void main()
{
    ivec2 pix = ivec2(gl_GlobalInvocationID.xy);

    vec2 resolution = GetResolution();
    vec2 invRes     = GetInvResolution();
    vec2 aspect     = GetAspect(resolution);

    if (pix.x >= int(resolution.x) || pix.y >= int(resolution.y))
        return;

    if (pc.Pass == 0)
    {
        // AO/IL generation – mxaoOut is AO surface
        vec4 aoIl = evaluateMXAO(pix, invRes, aspect);
        imageStore(mxaoOut, pix, aoIl);
    }
    else if (pc.Pass == 1)
    {
        // Blur 1 – mxaoIn/mxaoOut are AO ping-pong surfaces
        vec4 blurred = spatialBlur(
            pix, invRes, resolution,
            pc.RenderScale, pc.BlurRadius1, pc.BlurSteps1
        );
        imageStore(mxaoOut, pix, blurred);
    }
    else if (pc.Pass == 2)
    {
        // Final blur + shaping, AND composite into scene color
        // For this pass:
        //   mxaoIn  = final AO/IL input (CommonTex1)
        //   mxaoOut = *scene color* STORAGE image (bound from C++)

        vec4 shaped = shapeAOIL(pix, invRes, resolution);  // IL.rgb + AO.a
        float ao    = shaped.a;
        vec3 il     = shaped.rgb;

        // Read current scene color from mxaoOut (in-place storage)
        vec4 scene = imageLoad(mxaoOut, pix);
        vec3 color;

        if (pc.DebugView == 1u)
        {
            // Debug: override scene color with AO only
            color = vec3(ao);
        }
        else
        {
            // Normal composite: AO multiplicative + IL additive
            color  = scene.rgb;
            color *= ao;
            color += il;
        }

        imageStore(mxaoOut, pix, vec4(color, scene.a));
    }
}
