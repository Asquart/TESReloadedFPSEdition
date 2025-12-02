// Depth.comp.glsl
// Include-only file. Assumes GlobalLayout.comp.glsl has already declared:
//   layout(set = 0, binding = 0) uniform sampler2D gDepth;
//   uniform GlobalFrameUBO { ... } uFrame;

// --- Handy access macros (HLSL-style "static const" on UBO fields) ---
#include "GlobalLayout.comp.glsl"


#define invertedDepth (uFrame.TESR_DepthConstants.z)
#define nearZ         (uFrame.TESR_CameraData.x)
#define farZ          (uFrame.TESR_CameraData.y)
#define Q             (farZ / (farZ - nearZ))

// Build a GLSL mat4 from four row vec4s (matching D3D row-major layout)
mat4 TESR_MakeMat4FromRows(vec4 r0, vec4 r1, vec4 r2, vec4 r3)
{
    // GLSL mat4 columns are the *columns* of the matrix.
    // Our inputs r0..r3 are *rows*, so we transpose here.
    return mat4(
        vec4(r0.x, r1.x, r2.x, r3.x),  // column 0
        vec4(r0.y, r1.y, r2.y, r3.y),  // column 1
        vec4(r0.z, r1.z, r2.z, r3.z),  // column 2
        vec4(r0.w, r1.w, r2.w, r3.w)   // column 3
    );
}

mat4 TESR_GetProjection()
{
    vec4 r0 = uFrame.TESR_ProjectionTransform[0];
    vec4 r1 = uFrame.TESR_ProjectionTransform[1];
    vec4 r2 = uFrame.TESR_ProjectionTransform[2];
    vec4 r3 = uFrame.TESR_ProjectionTransform[3];
    return TESR_MakeMat4FromRows(r0, r1, r2, r3);
}

mat4 TESR_GetInvProjection()
{
    vec4 r0 = uFrame.TESR_InvProjectionTransform[0];
    vec4 r1 = uFrame.TESR_InvProjectionTransform[1];
    vec4 r2 = uFrame.TESR_InvProjectionTransform[2];
    vec4 r3 = uFrame.TESR_InvProjectionTransform[3];
    return TESR_MakeMat4FromRows(r0, r1, r2, r3);
}

mat4 TESR_GetView()
{
    vec4 r0 = uFrame.TESR_ViewTransform[0];
    vec4 r1 = uFrame.TESR_ViewTransform[1];
    vec4 r2 = uFrame.TESR_ViewTransform[2];
    vec4 r3 = uFrame.TESR_ViewTransform[3];
    return TESR_MakeMat4FromRows(r0, r1, r2, r3);
}

mat4 TESR_GetInvView()
{
    vec4 r0 = uFrame.TESR_InvViewTransform[0];
    vec4 r1 = uFrame.TESR_InvViewTransform[1];
    vec4 r2 = uFrame.TESR_InvViewTransform[2];
    vec4 r3 = uFrame.TESR_InvViewTransform[3];
    return TESR_MakeMat4FromRows(r0, r1, r2, r3);
}


// --- Depth read ----

float readDepth(vec2 coord)
{
    // exactly like HLSL: tex2D(TESR_DepthBuffer, coord).x * farZ;
    float z = texture(gDepth, coord).x;
    return z * farZ;   // view-space depth
}

// --- Position reconstruction in view space ---

vec3 reconstructPosition(vec2 uv)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(gDepth, uv).y;   // matching HLSL .y

    vec4 clipSpace = vec4(x, y, z, 1.0);

    mat4 invProj = TESR_GetInvProjection();

    // row-vector style multiply, matching HLSL mul(clipSpace, InvProj)
    vec4 viewSpace = clipSpace * invProj;

    viewSpace /= viewSpace.w;
    return viewSpace.xyz;
}


// --- Project a view-space position back to screen space (0..1) ---

vec3 projectPosition(vec3 position)
{
    vec4 pos = vec4(position, 1.0);

    vec4 row0 = uFrame.TESR_ProjectionTransform[0];
    vec4 row1 = uFrame.TESR_ProjectionTransform[1];
    vec4 row2 = uFrame.TESR_ProjectionTransform[2];
    vec4 row3 = uFrame.TESR_ProjectionTransform[3];

    vec4 projection;
    projection.x = dot(pos, row0);
    projection.y = dot(pos, row1);
    projection.z = dot(pos, row2);
    projection.w = dot(pos, row3);

    projection.xyz /= projection.w;

    // back to 0..1 screen space, same convention as original
    projection.x = projection.x * 0.5 + 0.5;
    projection.y = projection.y * 0.5 + 0.5;
    projection.z = projection.z * 0.5 + 0.5;

    return projection.xyz;
}


// --- Helper that reconstructs a "camera ray" direction from UV ---

vec3 toWorld(vec2 tex)
{
    // We treat TESR_ViewTransform as 4 rows (row-major, like HLSL),
    // and manually read its columns.

    // Column 2 (z) of the first 3 rows
    vec3 v = vec3(
        uFrame.TESR_ViewTransform[0].z,
        uFrame.TESR_ViewTransform[1].z,
        uFrame.TESR_ViewTransform[2].z
    );

    // Column 0 (x) for basis X
    vec3 col0 = vec3(
        uFrame.TESR_ViewTransform[0].x,
        uFrame.TESR_ViewTransform[1].x,
        uFrame.TESR_ViewTransform[2].x
    );

    // Column 1 (y) for basis Y
    vec3 col1 = vec3(
        uFrame.TESR_ViewTransform[0].y,
        uFrame.TESR_ViewTransform[1].y,
        uFrame.TESR_ViewTransform[2].y
    );

    float proj00 = uFrame.TESR_ProjectionTransform[0].x;
    float proj11 = uFrame.TESR_ProjectionTransform[1].y;

    v += (1.0 / proj00 * (2.0 * tex.x - 1.0)).xxx * col0;
    v += (-1.0 / proj11 * (2.0 * tex.y - 1.0)).xxx * col1;

    return v;
}

float getHomogenousDepth(vec2 uv)
{
    float depth = readDepth(uv);
    vec3 cameraVector = toWorld(uv) * depth;
    return length(cameraVector);
}

// --- Reconstruct world position + return view-space depth via out param ---

vec4 reconstructWorldPosition(vec2 uv, out float viewDepth)
{
    float x = uv.x * 2.0 - 1.0;
    float y = (1.0 - uv.y) * 2.0 - 1.0;
    float z = texture(gDepth, uv).y;
    vec4 clipSpace = vec4(x, y, z, 1.0);

    mat4 invProj = TESR_GetInvProjection();
    mat4 invView = TESR_GetInvView();

    // Column-vector convention: mat4 * vec4
    vec4 viewSpace = invProj * clipSpace;
    viewSpace /= viewSpace.w;
    viewDepth = viewSpace.z;

    vec4 worldSpace = invView * viewSpace;
    return vec4(worldSpace.xyz, 1.0);
}
