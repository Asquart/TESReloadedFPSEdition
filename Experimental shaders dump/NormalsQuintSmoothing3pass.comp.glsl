#version 450
#extension GL_GOOGLE_include_directive : enable

#include "Includes/Depth.comp.glsl"
#include "Includes/Helpers.comp.glsl"

layout(local_size_x = 16, local_size_y = 16) in;

// Output of the current pass
layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D uNormalsOut;

// Input normals texture (used in blur passes; bound but unused in pass 0)
layout(set = 1, binding = 1) uniform sampler2D uNormalsIn;

// Push constants: which pass we are running
layout(push_constant) uniform PushConstants {
    uint Pass;      // 0 = reconstruct, 1 = blur horizontal, 2 = blur vertical
    uint SmoothNumDirs;
    uint SmoothNumSteps;
    float SmoothBaseRadius;
    float SmoothingAngleMin;   // e.g. 0.20
    float SmoothingAngleMax;   // e.g. 0.95
} pc;

// ------------------------------------------------------------------
// HLSL constants, translated
// ------------------------------------------------------------------
const float dropTreshold = 0.82;
const float blurRadius   = 0.6;
const int   KernelSize   = 24;

const float BlurNormalsWeights[KernelSize] = float[KernelSize](
    0.019956226, 0.021463016, 0.032969806, 0.044476596,
    0.055983386, 0.067490176, 0.078996966, 0.080503756,
    0.092010546, 0.105024126, 0.116530916, 0.128037706,
    0.128037706, 0.116530916, 0.105024126, 0.092010546,
    0.080503756, 0.078996966, 0.067490176, 0.055983386,
    0.044476596, 0.032969806, 0.021463016, 0.019956226
);

// OffsetMask equivalents
const vec2 OffsetMaskH = vec2(1.0, 0.0);
const vec2 OffsetMaskV = vec2(0.0, 1.0);

// ------------------------------------------------------------------
// New plane-ish reconstruction + à-trous filter tunables
// ------------------------------------------------------------------

// Plane-fit style reconstruction around center (view-space depth units)
const float PlaneDepthSigma  = 0.15;
const float PlaneDepthCutoff = 0.5;

// 1D à-trous kernel: 5-tap binomial (1,4,6,4,1) normalized
const float AtrousKernel1D[5] = float[5](
    1.0 / 16.0,
    4.0 / 16.0,
    6.0 / 16.0,
    4.0 / 16.0,
    1.0 / 16.0
);

// Bilateral terms for à-trous
const float AtrousDepthSigma  = 0.02;  // depth falloff
const float AtrousNormalSharp = 32.0;  // higher = sharper normal edges

const int   SmoothNumDirs   = 4;
const int   SmoothNumSteps  = 4;      // was 5
const float SmoothBaseRadius = 0.040; // was 0.018 (we'll use this instead)

vec2 GetAspectRatio()
{
    // TESR_ReciprocalResolution = (1/width, 1/height)
    vec2 recip = uFrame.TESR_ReciprocalResolution.xy;
    float width  = 1.0 / recip.x;
    float height = 1.0 / recip.y;
    return vec2(1.0, width / height);  // x unchanged, y scaled by aspect
}

// qUINT MXAO "SmoothNormals" MKI port.
// normal/position come from your reconstructed G-buffer.
vec3 SmoothNormals_MKI(ivec2 pix)
{
    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    // Center position & normal from your buffers
    vec3 position = reconstructPosition(uv);
    vec3 normal   = expand(texture(uNormalsIn, uv).rgb);

    // qUINT::ASPECT_RATIO replacement
    vec2 aspect = GetAspectRatio();

    // Scaled radius exactly like MXAO: 0.018 / z * ASPECT_RATIO
    vec2 scaled_radius = pc.SmoothBaseRadius / position.z * aspect;

    // 4 directional accumulators
    vec3 neighbour_normal[4];
    neighbour_normal[0] = normal;
    neighbour_normal[1] = normal;
    neighbour_normal[2] = normal;
    neighbour_normal[3] = normal;

    // Outer loop over 4 directions
    for (int i = 0; i < pc.SmoothNumDirs; ++i)
    {
        float angle = 6.28318548 * 0.25 * float(i);
        vec2 direction = vec2(cos(angle), sin(angle));

        // Inner loop: 5 steps outwards along that direction
        for (int direction_step = 1; direction_step <= pc.SmoothNumSteps; ++direction_step)
        {
            float search_radius = exp2(float(direction_step)); // 2,4,8,16,32

            vec2 tap_uv = uv + direction * search_radius * scaled_radius;

            // Optional: keep within screen; MXAO relies on sampler clamp, but this is safer
            if (tap_uv.x < 0.0 || tap_uv.x > 1.0 ||
                tap_uv.y < 0.0 || tap_uv.y > 1.0)
                continue;

            vec3 temp_normal   = expand(textureLod(uNormalsIn, tap_uv, 0.0).rgb);
            vec3 temp_position = reconstructPosition(tap_uv);

            vec3 position_delta = temp_position - position;

            float dist2          = dot(position_delta, position_delta);
            float distance_weight = saturate(1.0 - dist2 * 14.0 / search_radius);

            float normal_angle  = dot(normal, temp_normal);
            float angle_weight  = smoothstep(pc.SmoothingAngleMin, pc.SmoothingAngleMax, normal_angle);

            float total_weight  = saturate(3.0 * distance_weight * angle_weight / search_radius);

            neighbour_normal[i] = mix(neighbour_normal[i], temp_normal, total_weight);
        }
    }

    vec3 sumN = neighbour_normal[0]
              + neighbour_normal[1]
              + neighbour_normal[2]
              + neighbour_normal[3];

    return normalize(sumN);
}


// ------------------------------------------------------------------
// PASS 0: ComputeNormals (from Depth.hlsl)
// ------------------------------------------------------------------
vec4 ComputeNormalsAtPixel(ivec2 pix)
{
    ivec2 imgSize = imageSize(uNormalsOut);
    if (pix.x < 0 || pix.y < 0 || pix.x >= imgSize.x || pix.y >= imgSize.y) {
        return vec4(0.0, 0.0, 1.0, 1.0);
    }

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    // Center depth + position (view space)
    float centerDepth = readDepth(uv);
    vec3  centerPos   = reconstructPosition(uv);

    // 4 direct neighbours (right, left, down, up)
    const ivec2 OFFS[4] = ivec2[4](
        ivec2( 1,  0),
        ivec2(-1,  0),
        ivec2( 0,  1),
        ivec2( 0, -1)
    );

    vec3  neighPos[4];
    float neighDepth[4];

    for (int i = 0; i < 4; ++i) {
        ivec2 q = pix + OFFS[i];
        q.x = clamp(q.x, 0, imgSize.x - 1);
        q.y = clamp(q.y, 0, imgSize.y - 1);

        vec2 uvN = (vec2(q) + 0.5) * texel;
        neighDepth[i] = readDepth(uvN);
        neighPos[i]   = reconstructPosition(uvN);
    }

    vec3  nAccum = vec3(0.0);
    float wAccum = 0.0;

    // Helper to accumulate one triangle (center, ia, ib)
    for (int t = 0; t < 4; ++t) {
        int ia, ib;
        if      (t == 0) { ia = 0; ib = 2; } // right, down
        else if (t == 1) { ia = 2; ib = 1; } // down, left
        else if (t == 2) { ia = 1; ib = 3; } // left, up
        else             { ia = 3; ib = 0; } // up, right

        float da = abs(neighDepth[ia] - centerDepth);
        float db = abs(neighDepth[ib] - centerDepth);

        // Work in relative depth: Δz / z
        float zRef = max(centerDepth, 1.0); // avoid div by ~0 near camera

        float relA = da / zRef;
        float relB = db / zRef;

        // Tunables (put these near top of file)
        const float PlaneDepthSigma  = 0.15; // was 0.02, way too small for your scale
        const float PlaneDepthCutoff = 0.5;  // reject only really big jumps (~50% change)

        // Large *relative* jumps = edge, skip this triangle
        if (relA > PlaneDepthCutoff || relB > PlaneDepthCutoff)
            continue;

        // Simple bilateral-style weight
        float wa = exp(-relA / PlaneDepthSigma);
        float wb = exp(-relB / PlaneDepthSigma);
        float w  = min(wa, wb);


        vec3 v1 = neighPos[ia] - centerPos;
        vec3 v2 = neighPos[ib] - centerPos;
        vec3 n  = cross(v2, v1);

        if (dot(n, n) < 1e-8)
            continue;

        n = normalize(n);
        nAccum += w * n;
        wAccum += w;
    }

    vec3 finalN;
    if (wAccum > 0.0)
        finalN = normalize(nAccum / wAccum);
    else
        finalN = vec3(0.0, 0.0, 1.0); // fallback

    return vec4(finalN, 1.0);
}

// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------
void main()
{
    ivec2 size = imageSize(uNormalsOut);
    ivec2 pix  = ivec2(gl_GlobalInvocationID.xy);

    if (pix.x >= size.x || pix.y >= size.y)
        return;

    // PASS 0: reconstruct normals from depth into uNormalsOut
    if (pc.Pass == 0u)
    {
        vec4 n = ComputeNormalsAtPixel(pix);   // your existing reconstruction
        imageStore(uNormalsOut, pix, vec4(compress(n.xyz), 1.0));
        return;
    }

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;
    vec2 uv    = (vec2(pix) + 0.5) * texel;

    // PASS 1: Smooth normals (MKI) into ping-pong target
    if (pc.Pass == 1u)
    {
        vec3 smoothN = SmoothNormals_MKI(pix); // reads uNormalsIn
        imageStore(uNormalsOut, pix, vec4(compress(smoothN), 1.0));
        return;
    }

    // PASS 2: simple copy back to final target (if your pipeline expects 3 passes)
    if (pc.Pass == 2u)
    {
        vec3 n = expand(texture(uNormalsIn, uv).rgb);
        imageStore(uNormalsOut, pix, vec4(compress(n), 1.0));
        return;
    }
}


