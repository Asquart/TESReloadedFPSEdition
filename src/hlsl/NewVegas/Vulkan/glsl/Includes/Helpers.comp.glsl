// Helpers.glsl - generic math/color helpers

#ifndef HELPERS_GLSL
#define HELPERS_GLSL

// HLSL-style helpers
#define saturate(x) clamp((x), 0.0, 1.0)

#define expand(v)        (((v) - 0.5) / 0.5)    // 0..1 → -1..1
#define compress(v)      (((v) * 0.5) + 0.5)    // -1..1 → 0..1
#define shade(n,l)       max(dot((n),(l)), 0.0)
#define shades(n,l)      saturate(dot((n),(l)))
#define invlerp(a,b,t)   (((t) - (a)) / ((b) - (a)))
#define invlerps(a,b,t)  saturate(((t) - (a)) / ((b) - (a)))

// Luma (BT.709)
#define luma(color)      dot((color).rgb, vec3(0.2126, 0.7152, 0.0722))

// Do NOT override GLSL built-in mix(), use a different name:
#define mixColors(colora, colorb) ((colora) * (colorb) * 2.0)

#define weight(v)        dot((v), vec3(1.0))
#define sqr(v)           ((v) * (v))
#define blendnormals(a,b) vec3((a).xy + (b).xy, (a).z)

#define rand(s)          fract(sin(dot((s), vec2(12.9898, 78.233))) * 43758.5453)

#define pows(a,b)        (pow(abs(a), (b)) * sign(a))
#define bend(a,b)        ((a) * (1.0 + (b)) / (1.0 + (a) * (b)))
#define scaledReinhard(a,b) (((a) * (b)) / (1.0 + (a) * (b)))

const float PI = 3.1415926538;

// Basic colors
const vec4 white   = vec4(1.0, 1.0, 1.0, 1.0);
const vec4 grey    = vec4(0.5, 0.5, 0.5, 1.0);
const vec4 black   = vec4(0.0, 0.0, 0.0, 1.0);
const vec4 red     = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 green   = vec4(0.0, 1.0, 0.0, 1.0);
const vec4 blue    = vec4(0.0, 0.0, 1.0, 1.0);
const vec4 yellow  = vec4(1.0, 1.0, 0.0, 1.0);
const vec4 cyan    = vec4(0.0, 1.0, 1.0, 1.0);
const vec4 magenta = vec4(1.0, 0.0, 1.0, 1.0);

void sincos(float a, out float s, out float c)
{
    s = sin(a);
    c = cos(a);
}

// sRGB <-> linear conversions

vec3 linearize(vec3 color) {
    vec3 linearRGBLo = color / 12.92;
    vec3 linearRGBHi = pow((color + 0.055) / 1.055, vec3(2.4));
    // choose Lo when color <= 0.04045, else Hi
    return mix(linearRGBLo, linearRGBHi, step(vec3(0.04045), color));
}

vec4 linearize(vec4 color) {
    vec3 linearRGBLo = color.rgb / 12.92;
    vec3 linearRGBHi = pow((color.rgb + 0.055) / 1.055, vec3(2.4));
    vec3 linearRGB = mix(linearRGBLo, linearRGBHi, step(vec3(0.04045), color.rgb));
    return vec4(linearRGB, color.a);
}

vec3 delinearize(vec3 color) {
    vec3 sRGBLo = color * 12.92;
    vec3 sRGBHi = pow(abs(color), vec3(1.0 / 2.4)) * 1.055 - 0.055;
    return mix(sRGBLo, sRGBHi, step(vec3(0.0031308), color));
}

vec4 delinearize(vec4 color) {
    vec3 sRGBLo = color.rgb * 12.92;
    vec3 sRGBHi = pow(abs(color.rgb), vec3(1.0 / 2.4)) * 1.055 - 0.055;
    vec3 sRGB = mix(sRGBLo, sRGBHi, step(vec3(0.0031308), color.rgb));
    return vec4(sRGB, color.a);
}

// 10-way selector (vec4)
vec4 selectColor(
    float selector,
    vec4 color0, vec4 color1, vec4 color2, vec4 color3, vec4 color4,
    vec4 color5, vec4 color6, vec4 color7, vec4 color8, vec4 color9)
{
    if (selector == 0.0) return color0;
    if (selector >= 0.1 && selector < 0.2) return color1;
    if (selector >= 0.2 && selector < 0.3) return color2;
    if (selector >= 0.3 && selector < 0.4) return color3;
    if (selector >= 0.4 && selector < 0.5) return color4;
    if (selector >= 0.5 && selector < 0.6) return color5;
    if (selector >= 0.6 && selector < 0.7) return color6;
    if (selector >= 0.7 && selector < 0.8) return color7;
    if (selector >= 0.8 && selector < 0.9) return color8;
    if (selector >= 0.9 && selector < 1.0) return color9;
    return black;
}

// 10-way selector (vec3)
vec3 selectColor(
    float selector,
    vec3 color0, vec3 color1, vec3 color2, vec3 color3, vec3 color4,
    vec3 color5, vec3 color6, vec3 color7, vec3 color8, vec3 color9)
{
    if (selector == 0.0) return color0;
    if (selector >= 0.1 && selector < 0.2) return color1;
    if (selector >= 0.2 && selector < 0.3) return color2;
    if (selector >= 0.3 && selector < 0.4) return color3;
    if (selector >= 0.4 && selector < 0.5) return color4;
    if (selector >= 0.5 && selector < 0.6) return color5;
    if (selector >= 0.6 && selector < 0.7) return color6;
    if (selector >= 0.7 && selector < 0.8) return color7;
    if (selector >= 0.8 && selector < 0.9) return color8;
    if (selector >= 0.9 && selector < 1.0) return color9;
    return black.rgb;
}

ivec2 UVToPixel(vec2 uv)
{
    // reconstruct resolution from reciprocal
    vec2 resolution = vec2(
        1.0 / uFrame.TESR_ReciprocalResolution.x,
        1.0 / uFrame.TESR_ReciprocalResolution.y
    );

    ivec2 pix = ivec2(floor(uv * resolution));
    return pix;
}

// GLSL: check if this UV belongs to a specific debug pixel
bool IsDebugPixelUV(vec2 uv, ivec2 debugPixel)
{
    ivec2 pix = UVToPixel(uv);
    return all(equal(pix, debugPixel));
}

float quantize_half(float v) {
    // convert float32 → float16 → float32 by hand
    uint f32 = floatBitsToUint(v);
    uint sign = (f32 >> 16) & 0x8000u;
    int  exp  = int((f32 >> 23) & 0xFFu) - 127 + 15;
    uint mant = (f32 >> 13) & 0x03FFu;

    if (exp <= 0) {       // underflow → zero
        return (sign << 16);
    }
    if (exp >= 31) {      // overflow → inf
        return floatBitsToUint(sign | 0x7C00u);
    }
    uint f16 = sign | (uint(exp) << 10) | mant;
    // now convert back to f32
    return uintBitsToFloat(
        ((f16 & 0x8000u) << 16) |
        (((f16 >> 10) & 0x1Fu) + (127 - 15)) << 23 |
        ((f16 & 0x3FFu) << 13)
    );
}

vec3 quantize_to_half(vec3 x) {
    return vec3(
        quantize_half(x.r),
        quantize_half(x.g),
        quantize_half(x.b)
    );
}

vec2 snap_uv_dx9(vec2 uv) {
    // 10-bit per axis is common, but D3D9 behaves ~11 bits
    const float snap = 2048.0; // try 1024.0 / 2048.0 / 4096.0
    return floor(uv * snap + 0.5) / snap;
}

#endif // HELPERS_GLSL
