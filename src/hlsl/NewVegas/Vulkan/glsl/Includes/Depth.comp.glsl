// depth.glsl
// Requires:
//   layout(set = 0, binding = 0) uniform sampler2D gDepth;
//   layout(set = 0, binding = 1) uniform GlobalFrameUBO uFrame;

// ---- HLSL-style constants ----
#define invertedDepth (uFrame.DepthConstants.z)
#define nearZ         (uFrame.CameraData.x)
#define farZ          (uFrame.CameraData.y)
#define Q             (farZ / (farZ - nearZ))

// tex2D(TESR_DepthBuffer, coord).x * farZ;
float readDepth(vec2 coord)
{
    return texture(gDepth, coord).x * farZ;
}

// Reconstruct view-space position from screen UV
vec3 reconstructPosition(vec2 uv)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(gDepth, uv).y;

    vec4 clipSpace = vec4(x, y, z, 1.0);

    // HLSL: mul(clipSpace, TESR_InvProjectionTransform)
    // We emulate row-major mul(v, M) as v * M in GLSL
    vec4 viewSpace = clipSpace * uFrame.InvProjection;
    viewSpace /= viewSpace.w;

    return viewSpace.xyz;
}

// Project view/world position into screen-space (0..1, 0..1, depth)
vec3 projectPosition(vec3 position)
{
    vec4 projection = vec4(position, 1.0) * uFrame.Projection;
    projection.xyz /= projection.w;

    projection.x = projection.x * 0.5 + 0.5;
    projection.y = 0.5 - 0.5 * projection.y;

    return projection.xyz;
}

// Rebuild the "to world" direction like in HLSL toWorld()
vec3 toWorld(vec2 tex)
{
    // NOTE: GLSL matrices are column-major; we keep the same indexing
    // pattern as the working port you had before for consistency.
    vec3 v = vec3(
        uFrame.View[0][2],
        uFrame.View[1][2],
        uFrame.View[2][2]);

    float sx = (2.0 * tex.x - 1.0) / uFrame.Projection[0][0];
    float sy = (2.0 * tex.y - 1.0) / uFrame.Projection[1][1];

    v += sx * vec3(uFrame.View[0][0], uFrame.View[1][0], uFrame.View[2][0]);
    v += -sy * vec3(uFrame.View[0][1], uFrame.View[1][1], uFrame.View[2][1]);

    return v;
}

// Homogenous depth: length of camera→point vector in world-ish space
float getHomogenousDepth(vec2 uv)
{
    float depth = readDepth(uv);
    vec3 cameraVector = toWorld(uv) * depth;
    return length(cameraVector);
}

// Reconstruct world-space position and return view-space Z in out param
vec4 reconstructWorldPosition(vec2 uv, out float viewDepth)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(gDepth, uv).y;
    vec4 clipSpace = vec4(x, y, z, 1.0);

    vec4 viewSpace = clipSpace * uFrame.InvProjection;
    viewSpace /= viewSpace.w;

    viewDepth = viewSpace.z;

    vec4 worldSpace = viewSpace * uFrame.InvView;
    return vec4(worldSpace.xyz, 1.0);
}
