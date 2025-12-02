// Global frame set (set = 0)

// Depth (combined depth buffer)
layout(set = 0, binding = 0) uniform sampler2D gDepth;

// Vulkan normals buffer (our blurred normals surface)
layout(set = 0, binding = 1) uniform sampler2D gNormals;

// Frame UBO (all TESR_* constants)
layout(set = 0, binding = 2, std140) uniform GlobalFrameUBO
{
    // --- Matrices (each is float[16] in C++ so we must use vec4[4] in GLSL) ---
    vec4 TESR_WorldTransform[4];
    vec4 TESR_ViewTransform[4];
    vec4 TESR_InvViewTransform[4];
    vec4 TESR_ProjectionTransform[4];
    vec4 TESR_InvProjectionTransform[4];
    vec4 TESR_WorldViewProjectionTransform[4];
    vec4 TESR_InvViewProjectionTransform[4];
    vec4 TESR_ViewProjectionTransform[4];
    vec4 TESR_OcclusionWorldViewProjTransform[4];

    // --- Light parameters ---
    vec4 TESR_LightPosition;
    vec4 TESR_LightColor;
    vec4 TESR_SpotLightPosition;
    vec4 TESR_SpotLightColor;
    vec4 TESR_SpotLightDirection;
    vec4 TESR_SpotLightToWorldTransform[4];
    vec4 TESR_ViewSpaceLightDir;
    vec4 TESR_ScreenSpaceLightDir;

    // --- Depth parameters ---
    vec4 TESR_DepthConstants;

    // --- Camera parameters ---
    vec4 TESR_ReciprocalResolution;
    vec4 TESR_CameraForward;
    vec4 TESR_CameraData;
    vec4 TESR_CameraPosition;

    // --- Game time ---
    vec4 TESR_GameTime[4];      // 16 floats = vec4[4]

    // --- Atmospheric parameters ---
    vec4 TESR_SunDirection;
    vec4 TESR_SunPosition;
    vec4 TESR_SunTiming;
    vec4 TESR_SunAmount;
    vec4 TESR_FogData;
    vec4 TESR_FogDistance;
    vec4 TESR_FogColor;
    vec4 TESR_SunColor;
    vec4 TESR_SunDiskColor;
    vec4 TESR_SunAmbient;
    vec4 TESR_SkyColor;
    vec4 TESR_SkyLowColor;
    vec4 TESR_HorizonColor;

} uFrame;
