// Normals.glsl - normal & world-space normal helpers
#ifndef NORMALS_GLSL
#define NORMALS_GLSL

// Get view-space or tangent-space normal from a normal buffer.
// Assumes normals are stored in [0..1] and need to be remapped to [-1..1].
vec3 GetNormal(vec2 uv, sampler2D normalsTex)
{
    return texture(normalsTex, uv).xyz * 2.0 - 1.0;
}

// Transform that normal into world space with a view matrix.
// (In your engine, this is effectively replicating HLSL mul(TESR_ViewTransform, normal))
vec3 GetWorldNormal(vec2 uv, sampler2D normalsTex, mat4 viewTransform)
{
    vec4 n = vec4(GetNormal(uv, normalsTex), 1.0);
    return (viewTransform * n).xyz;
}

#endif // NORMALS_GLSL
