// Shadows.glsl - point light & (E)VSM helpers
#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// Requires:
//  - Helpers.glsl for: saturate(), pows(), etc.
//    (in HLSL saturate is intrinsic; in GLSL you probably have
//     #define saturate(x) clamp(x, 0.0, 1.0) in Helpers.glsl)

// Slight rename to avoid polluting generic BIAS name
const float SHADOW_BIAS = 0.018;

// Safe scalar/vector lerp helpers (don't conflict with your mix macro)
float Lerp(float a, float b, float t) {
    return a * (1.0 - t) + b * t;
}

vec2 Lerp(vec2 a, vec2 b, float t) {
    return a * (1.0 - t) + b * t;
}

vec3 Lerp(vec3 a, vec3 b, float t) {
    return a * (1.0 - t) + b * t;
}

// ------------------------------------------------------------
// Point light / cube shadow map
// ------------------------------------------------------------

// GetPointLightAmountValue:
//  - shadowCubeMap: cube shadow map
//  - lightDir     : direction from light to shaded point, in light space
//  - distance     : normalized distance (0..1) from light
//  - shadowFadeZ  : your TESR_ShadowFade.z (0 = disabled)
float GetPointLightAmountValue(
    samplerCube shadowCubeMap,
    vec3        lightDir,
    float       distance,
    float       shadowFadeZ)
{
    if (shadowFadeZ == 0.0)
        return 1.0;

    float lightDepth = texture(shadowCubeMap, lightDir).r;

    // Increase bias with distance
    float shadow = (lightDepth + SHADOW_BIAS * distance > distance) ? 1.0 : 0.0;

    // Ignore sample if depth is out of valid cube range
    float valid = (lightDepth > 0.0 && lightDepth < 1.0) ? 1.0 : 0.0;

    return Lerp(1.0, shadow, valid);
}


// radius-based attenuation
// LightDir: world-space direction from light to point
// Distance: normalized distance (0..1) over light range
float GetPointLightAtten(vec3 LightDir, float Distance, vec4 normal)
{
    // radius based attenuation from:
    // https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
    float s = saturate(Distance * Distance);
    float atten = saturate(((1.0 - s) * (1.0 - s)) / (1.0 + 5.0 * s));

    LightDir = normalize(LightDir);

    // add some light bleeding at very short distance
    float t = smoothstep(0.0, 0.2, Distance);
    float diffuse = Lerp(1.0, dot(LightDir, normal.xyz), t);

    return saturate(diffuse * atten);
}


// Full point light contribution with cube shadow map.
//
//  - shadowCubeMap : samplerCube
//  - worldPos      : vec4 world position (w can be 1.0)
//  - lightPos      : vec4(lightPosition.xyz, lightRadius)
//  - normal        : vec4 normal (xyz used)
//  - shadowFadeZ   : TESR_ShadowFade.z (0 → shadows disabled)
float GetPointLightAmount(
    samplerCube shadowCubeMap,
    vec4        worldPos,
    vec4        lightPos,
    vec4        normal,
    float       shadowFadeZ)
{
    if (lightPos.w == 0.0)
        return 0.0; // w is light radius

    vec3 lightDir = lightPos.xyz - worldPos.xyz;
    vec3 lightUV  = lightDir * vec3(-1.0, -1.0, 1.0);

    // normalize distance over light range
    float distance = length(lightDir) / lightPos.w;

    float lightAmount = GetPointLightAmountValue(shadowCubeMap, lightUV, distance, shadowFadeZ) *
                        GetPointLightAtten(lightDir, distance, normal);

    return saturate(lightAmount);
}


// GetPointLightDistance: direction + normalized distance
vec4 GetPointLightDistance(vec4 worldPos, vec4 lightPos)
{
    vec3 LightDir = lightPos.xyz - worldPos.xyz;
    float Distance = length(LightDir) / lightPos.w;
    return vec4(LightDir, Distance);
}


// Point light contribution without shadows
float GetPointLightContribution(vec4 worldPos, vec4 lightPos, vec4 normal)
{
    vec4 light = GetPointLightDistance(worldPos, lightPos);
    return GetPointLightAtten(light.xyz, light.w, normal);
}

// ------------------------------------------------------------
// VSM / EVSM helpers
// ------------------------------------------------------------

float Linstep(float a, float b, float v) {
    return saturate((v - a) / (b - a));
}

// Reduces VSM light bleeding
float ReduceLightBleeding(float pMax, float amount) {
    // Remove the [0, amount] tail and linearly rescale (amount, 1].
    return Linstep(amount, 1.0, pMax);
}

float ChebyshevUpperBound(
    vec2  moments,
    float mean,
    float minVariance,
    float lightBleedingReduction)
{
    // variance = E[x^2] - (E[x])^2
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance);

    float d    = mean - moments.x;
    float pMax = variance / (variance + (d * d));

    pMax = ReduceLightBleeding(pMax, lightBleedingReduction);

    // One-tailed Chebyshev
    return (mean <= moments.x ? 1.0 : pMax);
}

float GetLightAmountValueVSM(
    vec2  moments,
    float depth,
    float bias,
    float bleedReduction)
{
    return ChebyshevUpperBound(moments, depth, bias, bleedReduction);
}


// GetEVSMExponents: choose exponents based on format (fp32 vs fp16 style)
// formatBits: 0 → fp32, otherwise fp16-like
vec2 GetEVSMExponents(float positiveExponent, float negativeExponent, float formatBits)
{
    const float maxExponent = (formatBits == 0.0) ? 5.54 : 42.0;

    vec2 lightSpaceExponents = vec2(positiveExponent, negativeExponent);

    // Clamp to maximum range of fp32/fp16 to prevent overflow/underflow
    return min(lightSpaceExponents, vec2(maxExponent));
}

// Applies exponential warp to shadow map depth, input depth in [0, 1]
vec2 WarpDepth(float depth, vec2 exponents)
{
    // Rescale depth into [-1, 1]
    depth = 2.0 * depth - 1.0;

    float pos = exp(exponents.x * depth);
    float neg = -exp(-exponents.y * depth);
    return vec2(pos, neg);
}

float GetLightAmountValueEVSM2(
    vec2  moments,
    float depth,
    float bias,
    float bleedReduction,
    float formatBits)
{
    vec2 exponents   = GetEVSMExponents(40.0, 5.0, formatBits);
    vec2 warpedDepth = WarpDepth(depth, exponents);

    // Derivative of warping at depth
    vec2 depthScale  = bias * exponents * warpedDepth;
    vec2 minVariance = depthScale * depthScale;

    return ChebyshevUpperBound(moments, warpedDepth.x, minVariance.x, bleedReduction);
}

float GetLightAmountValueEVSM4(
    vec4  moments,
    float depth,
    float bias,
    float bleedReduction,
    float formatBits)
{
    vec2 exponents   = GetEVSMExponents(40.0, 5.0, formatBits);
    vec2 warpedDepth = WarpDepth(depth, exponents);

    vec2 depthScale  = bias * exponents * warpedDepth;
    vec2 minVariance = depthScale * depthScale;

    float posContrib = ChebyshevUpperBound(moments.xz, warpedDepth.x, minVariance.x, bleedReduction);
    float negContrib = ChebyshevUpperBound(moments.yw, warpedDepth.y, minVariance.y, bleedReduction);

    return min(posContrib, negContrib);
}

#endif // SHADOWS_GLSL
