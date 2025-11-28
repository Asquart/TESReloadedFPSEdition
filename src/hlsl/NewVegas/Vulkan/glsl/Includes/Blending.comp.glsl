// Blending.glsl - alpha + overlay + soft light

#ifndef BLENDING_GLSL
#define BLENDING_GLSL

// Requires: Helpers.glsl (for luma())

vec4 alphaBlend(vec4 base, vec4 blend)
{
    // base*base.a + (1 - base.a)*blend
    return base * base.a + (1.0 - base.a) * blend;
}

vec4 Desaturate(vec4 inputColor)
{
    float grey = luma(inputColor);
    return vec4(grey, grey, grey, inputColor.a);
}

// Photoshop Overlay blend mode code (scalar)
float BlendMode_Overlay(float base, float blend)
{
    return (base <= 0.5)
        ? 2.0 * base * blend
        : 1.0 - 2.0 * (1.0 - base) * (1.0 - blend);
}

// Vector version
vec4 BlendMode_Overlay(vec4 base, vec4 blend)
{
    vec4 result;
    result.r = BlendMode_Overlay(base.r, blend.r);
    result.g = BlendMode_Overlay(base.g, blend.g);
    result.b = BlendMode_Overlay(base.b, blend.b);
    result.a = blend.a;

    return alphaBlend(result, base);
}

vec4 BlendMode_SoftLight(vec4 base, vec4 blend)
{
    vec4 lowValues  = 2.0 * base * blend * (1.0 + base * (1.0 - blend));
    vec4 a_sqrt     = sqrt(base);
    vec4 highValues = (base + blend * (a_sqrt - base)) * 2.0 - a_sqrt;

    // HLSL: (blend < 0.5f) ? lowValues : highValues;
    // Per-component: use mix + step
    vec4 result = mix(lowValues, highValues, step(vec4(0.5), blend));
    result.a = blend.a;

    return alphaBlend(result, base);
}

#endif // BLENDING_GLSL
