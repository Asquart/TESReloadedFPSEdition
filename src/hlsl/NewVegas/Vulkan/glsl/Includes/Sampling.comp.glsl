// Sampling.glsl - downsample / upsample kernels
#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

// 13-tap downsample box blur.
// 'buffer'      : input texture
// 'uv'          : current UV
// 'texelSize'   : 1.0 / textureSize(buffer, 0)
vec4 DownsampleBox13(sampler2D buffer, vec2 uv, vec2 texelSize)
{
    float x = texelSize.x;
    float y = texelSize.y;

    vec4 a = texture(buffer, vec2(uv.x - 2.0 * x, uv.y + 2.0 * y));
    vec4 b = texture(buffer, vec2(uv.x,           uv.y + 2.0 * y));
    vec4 c = texture(buffer, vec2(uv.x + 2.0 * x, uv.y + 2.0 * y));

    vec4 d = texture(buffer, vec2(uv.x - 2.0 * x, uv.y));
    vec4 e = texture(buffer, vec2(uv.x,           uv.y));
    vec4 f = texture(buffer, vec2(uv.x + 2.0 * x, uv.y));

    vec4 g = texture(buffer, vec2(uv.x - 2.0 * x, uv.y - 2.0 * y));
    vec4 h = texture(buffer, vec2(uv.x,           uv.y - 2.0 * y));
    vec4 i = texture(buffer, vec2(uv.x + 2.0 * x, uv.y - 2.0 * y));

    vec4 j = texture(buffer, vec2(uv.x - 1.0 * x, uv.y + 1.0 * y));
    vec4 k = texture(buffer, vec2(uv.x + 1.0 * x, uv.y + 1.0 * y));
    vec4 l = texture(buffer, vec2(uv.x - 1.0 * x, uv.y - 1.0 * y));
    vec4 m = texture(buffer, vec2(uv.x + 1.0 * x, uv.y - 1.0 * y));

    vec2 weights = vec2(0.125, 0.5);

    vec4 box0 = (a + b + d + e) * 0.25;
    vec4 box1 = (b + c + e + f) * 0.25;
    vec4 box2 = (d + e + g + h) * 0.25;
    vec4 box3 = (e + f + h + i) * 0.25;
    vec4 box4 = (j + k + l + m) * 0.25;

    return box0 * weights.x +
           box1 * weights.x +
           box2 * weights.x +
           box3 * weights.x +
           box4 * weights.y;
}

// 9-tap tent upsample.
//
// 'filterRadius' is in **UV units**, typically something like:
//    vec2 filterRadius = vec2(1.0) / vec2(targetResolution);
vec4 UpsampleTent9(sampler2D buffer, vec2 uv, vec2 filterRadius)
{
    float x = filterRadius.x;
    float y = filterRadius.y;

    vec4 a = texture(buffer, vec2(uv.x - x, uv.y + y));
    vec4 b = texture(buffer, vec2(uv.x,     uv.y + y));
    vec4 c = texture(buffer, vec2(uv.x + x, uv.y + y));

    vec4 d = texture(buffer, vec2(uv.x - x, uv.y));
    vec4 e = texture(buffer, vec2(uv.x,     uv.y));
    vec4 f = texture(buffer, vec2(uv.x + x, uv.y));

    vec4 g = texture(buffer, vec2(uv.x - x, uv.y - y));
    vec4 h = texture(buffer, vec2(uv.x,     uv.y - y));
    vec4 i = texture(buffer, vec2(uv.x + x, uv.y - y));

    vec4 up = e * 4.0;
    up += (b + d + f + h) * 2.0;
    up += (a + c + g + i);
    up *= 1.0 / 16.0;

    return up;
}

#endif // SAMPLING_GLSL
