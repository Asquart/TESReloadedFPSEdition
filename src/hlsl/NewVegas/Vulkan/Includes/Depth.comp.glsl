#version 450
#extension GL_GOOGLE_include_directive : enable

// One workgroup = 16x16 pixels
layout(local_size_x = 16, local_size_y = 16) in;

// Binding layout:
// set=0, binding=0: storage image we write debug output into
// set=0, binding=1: depth buffer sampler (DXVK interop)
// set=0, binding=2: uniform buffer with TESR_* matrices/constants

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D OutImage;
layout(set = 0, binding = 1) uniform sampler2D TESR_DepthBuffer;

layout(set = 0, binding = 2) uniform DepthUniforms {
    mat4 TESR_ProjectionTransform;
    mat4 TESR_InvProjectionTransform;
    mat4 TESR_ViewTransform;
    mat4 TESR_InvViewTransform;
    vec4 TESR_DepthConstants;
    vec4 TESR_CameraData;
    vec4 TESR_CameraPosition;
};

// Match HLSL "static const" semantics via macros
#define invertedDepth (TESR_DepthConstants.z)
#define nearZ         (TESR_CameraData.x)
#define farZ          (TESR_CameraData.y)
#define Q             (farZ / (farZ - nearZ))

// --- Helpers: direct port of Depth.hlsl ---

float readDepth(vec2 coord)
{
    // HLSL: tex2D(TESR_DepthBuffer, coord).x * farZ;
    return texture(TESR_DepthBuffer, coord).x * farZ;
}

vec3 reconstructPosition(vec2 uv)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(TESR_DepthBuffer, uv).y;
    vec4 clipSpace = vec4(x, y, z, 1.0);

    // HLSL: mul(clipSpace, TESR_InvProjectionTransform)
    // HLSL uses row-major v*M, so we keep clipSpace * InvProj
    vec4 viewSpace = clipSpace * TESR_InvProjectionTransform;
    viewSpace /= viewSpace.w;

    return viewSpace.xyz;
}

vec3 projectPosition(vec3 position)
{
    vec4 projection = vec4(position, 1.0) * TESR_ProjectionTransform;
    projection.xyz /= projection.w;

    projection.x = projection.x * 0.5 + 0.5;
    projection.y = 0.5 - 0.5 * projection.y;

    return projection.xyz;
}

vec3 toWorld(vec2 tex)
{
    vec3 v = vec3(
        TESR_ViewTransform[0][2],
        TESR_ViewTransform[1][2],
        TESR_ViewTransform[2][2]);

    v += (1.0 / TESR_ProjectionTransform[0][0] * (2.0 * tex.x - 1.0)).xxx *
         vec3(TESR_ViewTransform[0][0], TESR_ViewTransform[1][0], TESR_ViewTransform[2][0]);

    v += (-1.0 / TESR_ProjectionTransform[1][1] * (2.0 * tex.y - 1.0)).xxx *
         vec3(TESR_ViewTransform[0][1], TESR_ViewTransform[1][1], TESR_ViewTransform[2][1]);

    return v;
}

float getHomogenousDepth(vec2 uv)
{
    float depth = readDepth(uv);
    vec3 cameraVector = toWorld(uv) * depth;
    return length(cameraVector);
}

vec4 reconstructWorldPosition(vec2 uv, out float viewDepth)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(TESR_DepthBuffer, uv).y;
    vec4 clipSpace = vec4(x, y, z, 1.0);

    vec4 viewSpace = clipSpace * TESR_InvProjectionTransform;
    viewSpace /= viewSpace.w;

    viewDepth = viewSpace.z;

    vec4 worldSpace = viewSpace * TESR_InvViewTransform;
    return vec4(worldSpace.xyz, 1.0);
}

// --- Main compute ---

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);

    ivec2 size = imageSize(OutImage);
    if (pixel.x >= size.x || pixel.y >= size.y)
        return;

    // UV in [0,1]
    vec2 uv = (vec2(pixel) + 0.5) / vec2(size);

    float viewDepth;
    reconstructWorldPosition(uv, viewDepth);

    // Debug: visualize depth as grayscale based on viewDepth
    // NOTE: viewDepth is typically negative in view space for forward camera.
    float d = -viewDepth;        // make positive
    float normalized = clamp(d / farZ, 0.0, 1.0);

    vec4 color = vec4(normalized, normalized, normalized, 1.0);
    imageStore(OutImage, pixel, color);
}
