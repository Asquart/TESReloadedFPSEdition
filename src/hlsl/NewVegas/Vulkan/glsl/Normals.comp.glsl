#version 450
#extension GL_GOOGLE_include_directive : enable

#include "Includes/Depth.comp.glsl"
#include "Includes/Helpers.comp.glsl"

layout(local_size_x = 16, local_size_y = 16) in;

// Output for each pass
layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D uNormalsOut;

// Input for pass 1 (pass 0 writes, pass 1 reads)
layout(set = 1, binding = 1) uniform sampler2D uNormalsIn;

// -----------------------------------------------------------------------------
// Push constants
// -----------------------------------------------------------------------------
layout(push_constant) uniform PushConstants {
    uint  Pass;               // 0 = reconstruct, 1 = smooth
    uint  SmoothNumDirs;      // 4+ recommended
    uint  SmoothNumSteps;     // 2-4 recommended
    float SmoothRadius;
    float MinSmoothingAngle;
    float MaxSmoothingAngle;
    float CreaseThreshold;
} pc;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

vec3 safeNorm(vec3 v)
{
    float l2 = dot(v, v);
    return (l2 > 1e-12) ? v * inversesqrt(l2) : vec3(0.0, 0.0, 1.0);
}

vec2 GetAspectRatio()
{
    // TESR_ReciprocalResolution = (1/width, 1/height)
    vec2 recip = uFrame.TESR_ReciprocalResolution.xy;
    float width  = 1.0 / recip.x;
    float height = 1.0 / recip.y;
    return vec2(1.0, width / height);  // x unchanged, y scaled by aspect
}

// -----------------------------------------------------------------------------
// PASS 0 — EXACT GEOMETRY-FROM-DEPTH NORMAL RECONSTRUCTION
// -----------------------------------------------------------------------------

vec3 ReconstructNormal(ivec2 pix, ivec2 imgSize, vec2 texel)
{
    // Clamp coords
    ivec2 p0 = ivec2(clamp(pix.x, 0, imgSize.x-1),
                     clamp(pix.y, 0, imgSize.y-1));

    vec2 uv0 = (vec2(p0) + 0.5) * texel;
    float centerDepth = readDepth(uv0);
    vec3  centerPos   = reconstructPosition(uv0);

    const ivec2 OFFS[4] = ivec2[4](
        ivec2( 1,  0),
        ivec2(-1,  0),
        ivec2( 0,  1),
        ivec2( 0, -1)
    );

    vec3 nAccum = vec3(0.0);
    float wAccum = 0.0;

    for (int t = 0; t < 4; ++t)
    {
        int ia, ib;
        if      (t == 0) { ia = 0; ib = 2; }
        else if (t == 1) { ia = 2; ib = 1; }
        else if (t == 2) { ia = 1; ib = 3; }
        else             { ia = 3; ib = 0; }

        ivec2 qa = p0 + OFFS[ia];
        ivec2 qb = p0 + OFFS[ib];

        qa = ivec2(clamp(qa.x, 0, imgSize.x-1),
                   clamp(qa.y, 0, imgSize.y-1));
        qb = ivec2(clamp(qb.x, 0, imgSize.x-1),
                   clamp(qb.y, 0, imgSize.y-1));

        vec2 uva = (vec2(qa)+0.5)*texel;
        vec2 uvb = (vec2(qb)+0.5)*texel;

        float da = abs(readDepth(uva) - centerDepth);
        float db = abs(readDepth(uvb) - centerDepth);

        float zRef = max(centerDepth, 1.0);
        float relA = da / zRef;
        float relB = db / zRef;

        // Avoid mixing across edges
        if (relA > 0.5 || relB > 0.5)
            continue;

        // Bilateral weights (exact as before)
        float wa = exp(-relA / 0.15);
        float wb = exp(-relB / 0.15);
        float w  = min(wa, wb);

        vec3 pa = reconstructPosition(uva) - centerPos;
        vec3 pb = reconstructPosition(uvb) - centerPos;

        vec3 n = cross(pb, pa);
        if (dot(n,n) < 1e-10) continue;

        nAccum += normalize(n) * w;
        wAccum += w;
    }

    return (wAccum > 0.0 ? safeNorm(nAccum / wAccum)
                         : vec3(0,0,1));
}

// -----------------------------------------------------------------------------
// PASS 1 — EXACT MK1 SMOOTHING YOU PROVIDED (CLEANED)
// -----------------------------------------------------------------------------

vec3 SmoothNormal_MK1(ivec2 pix, ivec2 imgSize, vec2 texel)
{
    vec2 uv = (vec2(pix) + 0.5) * texel;
    vec3 centerN = expand(texture(uNormalsIn, uv).rgb);
    vec3 centerP = reconstructPosition(uv);

    vec2 aspect = GetAspectRatio();

    float smoothRadiusPixels = pc.SmoothRadius;                // radius in pixels
    vec2 texelSize          = texel;              // 1 / resolution
    vec2 scaled_radius      = smoothRadiusPixels * texelSize * aspect;

    uint dirs  = max(1u, pc.SmoothNumDirs);
    uint steps = max(1u, pc.SmoothNumSteps);

    // Precomputed directions: 0°, 90°, 180°, 270°
    const vec2 BASE_DIRS[4] = vec2[4](
        vec2( 1.0,  0.0),
        vec2( 0.0,  1.0),
        vec2(-1.0,  0.0),
        vec2( 0.0, -1.0)
    );

    // Precomputed radii for steps 1..5: 2,4,8,16,32
    const float RADIUS_LUT[5] = float[5](
        2.0, 4.0, 8.0, 16.0, 32.0
    );

    vec3 accum[4];
    accum[0] = centerN;
    accum[1] = centerN;
    accum[2] = centerN;
    accum[3] = centerN;

    // Clamp dirs/steps to our LUT sizes
    uint useDirs  = min(dirs,  4u);
    uint useSteps = min(steps, 5u);

    for (uint i = 0u; i < useDirs; ++i)
    {
        vec2 dir = BASE_DIRS[i];

        // Pre-scale direction once
        vec2 dirScaled = dir * scaled_radius;

        for (uint s = 1u; s <= useSteps; ++s)
        {
            float searchR = RADIUS_LUT[s - 1u]; // 2,4,8,...

            vec2 tap_uv = uv + dirScaled * searchR;

            if (tap_uv.x < 0.0 || tap_uv.x > 1.0 ||
                tap_uv.y < 0.0 || tap_uv.y > 1.0)
                continue;

            // --- 1) Sample normal first ---
            vec3 tapN = expand(textureLod(uNormalsIn, tap_uv, 0.0).rgb);

            // Crease / hard edge gate
            float creaseDot = dot(centerN, tapN);
            if (creaseDot < pc.CreaseThreshold)     // e.g. 0.86–0.90
                continue;

            // --- 2) Angle gate (user-controlled) ---
            float ndot    = creaseDot;
            float angle_w = smoothstep(pc.MinSmoothingAngle, pc.MaxSmoothingAngle, ndot);
            if (angle_w <= 0.0)
                continue;

            // --- 3) Reconstruct position ONLY if normal tests passed ---
            vec3 tapP = reconstructPosition(tap_uv);

            // --- 4) Distance logic with depth-aware mixing ---
            vec3 dp        = tapP - centerP;
            float dist2_raw = dot(dp, dp);

            float zCenter = max(centerP.z, 1.0);
            float dzRel   = abs(tapP.z - centerP.z) / zCenter;

            float dist2_norm = dist2_raw / (zCenter * zCenter);

            const float depthSameThreshold = 0.08;   // same depth layer
            const float z0 = 500.0;                  // start "far" (~7m)
            const float z1 = 2000.0;                 // fully far (~28m)
            float farT = saturate((zCenter - z0) / (z1 - z0));

            float dist2_mix;
            if (dzRel < depthSameThreshold)
            {
                // same layer → relax distance with depth
                dist2_mix = mix(dist2_raw, dist2_norm, farT);
            }
            else
            {
                // different layer → keep strict distance
                dist2_mix = dist2_raw;
            }

            float dist_w = saturate(1.0 - dist2_mix / searchR);

            // --- 5) Coherence test vs accumulator ---
            float nd_acc = dot(accum[i], tapN);
            if (nd_acc < 0.90)
                continue;

            // --- 6) "Same surface" floor (your 0.3) only for good taps ---
            bool sameSurface =
                (dzRel < depthSameThreshold) &&
                (nd_acc >= 0.90) &&
                (angle_w > 0.0);

            float minFloor = 0.3;
            if (sameSurface)
            {
                dist_w = max(dist_w, minFloor);
            }

            // --- 7) Final weight and accumulate ---
            float w = saturate((3.0 * dist_w * angle_w) / searchR);
            if (w <= 0.0)
                continue;

            accum[i] = mix(accum[i], tapN, w);
        }

    }

    return normalize(accum[0] + accum[1] + accum[2] + accum[3]);
}


// -----------------------------------------------------------------------------
// Entry
// -----------------------------------------------------------------------------

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(uNormalsOut);

    if (gid.x >= imgSize.x || gid.y >= imgSize.y)
        return;

    vec2 texel = uFrame.TESR_ReciprocalResolution.xy;

    if (pc.Pass == 0u)
    {
        vec3 n = ReconstructNormal(gid, imgSize, texel);
        vec3 enc = n * 0.5 + 0.5;
        imageStore(uNormalsOut, gid, vec4(enc, 1));
        return;
    }

    // PASS 1 — MK1 smoothing
    vec3 sN = SmoothNormal_MK1(gid, imgSize, texel);
    imageStore(uNormalsOut, gid, vec4(sN * 0.5 + 0.5, 1));
}
