// PBR.glsl - GGX / Cook-Torrance helpers
#ifndef PBR_GLSL
#define PBR_GLSL

// Requires Helpers.glsl for:
//   - PI
//   - sqr(x)
//   - shade(n,l)
//   - shades(n,l)
//   - saturate / etc.

float GGX(float alpha, float NdotH)
{
    float num = alpha * alpha;
    float denom = PI * sqr(NdotH * NdotH * (num - 1.0) + 1.0);
    return num / max(denom, 0.0001);
}

// Schlick-Beckmann G1 term
float ShlickBeckmann(float NdotX, float alpha)
{
    // float k = alpha / 2;     // original commented version
    float k = alpha * alpha * 0.797884560802865; // HLSL version you used
    return max(NdotX, 0.0001) /
           max(NdotX * (1.0 - k) + k, 0.0001);
}

// Smith shadowing-masking term G
float GeometryShadowing(float alpha, float NdotV, float NdotL)
{
    return ShlickBeckmann(NdotV, alpha) * ShlickBeckmann(NdotL, alpha);
}

// Schlick Fresnel
vec3 FresnelShlick(vec3 reflectance, vec3 halfway, vec3 eyeDir)
{
    // shades(halfway, eyeDir) → saturate(dot(halfway, eyeDir))
    float cosTheta = shades(halfway, eyeDir);
    return reflectance + (vec3(1.0) - reflectance) * pow(1.0 - cosTheta, 5.0);
}

vec3 CookTorrance(
    float alpha,
    vec3  fresnel,
    float NdotV,
    float NdotL,
    float NdotH)
{
    vec3 num = GGX(alpha, NdotH) *
               GeometryShadowing(alpha, NdotV, NdotL) *
               fresnel;

    float denom = max(4.0 * NdotV * NdotL, 1e-7);
    return num / denom;
}

vec3 BRDF(
    float roughness,
    float NdotL,
    float NdotV,
    float NdotH,
    vec3  fresnel)
{
    return fresnel * CookTorrance(roughness, fresnel, NdotV, NdotL, NdotH);
}

// Variant with NdotL baked in (for water etc.)
vec3 modifiedBRDF(
    float roughness,
    float NdotL,
    float NdotV,
    float NdotH,
    vec3  fresnel)
{
    vec3 brdf = fresnel * CookTorrance(roughness, fresnel, NdotV, NdotL, NdotH);
    return brdf * NdotL;
}

// Full PBR lighting from a single directional light
vec3 PBR(
    float metallicness,
    float roughness,
    vec3  albedo,
    vec3  normal,
    vec3  eyeDir,
    vec3  lightDir,
    vec3  lightColor)
{
    // Reflectance at normal incidence
    vec3 reflectance = mix(vec3(0.04), albedo, metallicness);

    vec3 halfway = normalize(eyeDir + lightDir);

    float NdotL = max(shade(normal, lightDir), 1e-7);
    float NdotV = max(shade(normal, eyeDir),   1e-7);
    float NdotH = shades(normal, halfway);

    vec3 Ks = FresnelShlick(reflectance, halfway, eyeDir);

    // Lambert diffuse (energy-conserving)
    vec3 lambertDiffuse = (1.0 - metallicness) * (vec3(1.0) - Ks) * albedo / PI;

    // Specular
    float a = roughness * roughness;
    vec3 spec = BRDF(a, NdotL, NdotV, NdotH, Ks);

    return (lambertDiffuse + spec) * NdotL * lightColor;
    // Or use the commented version from HLSL if you want color-tinted lights:
    // return (lambertDiffuse + spec) * NdotL * mix(lightColor, albedo, metallicness);
}

#endif // PBR_GLSL
