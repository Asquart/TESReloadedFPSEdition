// Sky.glsl - simple sun & sky color helpers
#ifndef SKY_GLSL
#define SKY_GLSL

// Requires Helpers.glsl for:
//   - linearize()
//   - pows()
//   - saturate()
//   - (optionally) PI, etc.

vec3 GetSunColor(
    float sunHeight,
    float atmosphere,
    float dayTime,
    vec3  sunColor,
    vec3  sunsetColor)
{
    sunColor    = linearize(sunColor);
    sunsetColor = linearize(sunsetColor);

    // Increase sun color with height
    vec3 color = (1.0 + sunHeight) * sunColor;

    float sunSet = saturate(pows(1.0 - sunHeight, 8.0)) * dayTime;
    color += sunsetColor * sunSet * atmosphere;

    return color;
}

// 'sunAmount' is your TESR_SunAmount.x (0..1) passed in explicitly.
vec3 GetSkyColor(
    float verticality,
    float atmosphere,
    float sunHeight,
    float sunInfluence,
    float sunStrength,
    float sunAmount,
    vec3  skyColor,
    vec3  skyLowColor,
    vec3  horizonColor,
    vec3  sunColor)
{
    skyColor     = linearize(skyColor);
    skyLowColor  = linearize(skyLowColor);
    horizonColor = linearize(horizonColor);

    float isDayTime = smoothstep(0.0, 0.5, sunAmount);

    // Fade from low sky to high sky with height
    vec3 color = mix(skyLowColor, skyColor, verticality);

    // Fade to horizon color depending on atmosphere & sun side
    color = mix(
        color,
        horizonColor,
        saturate(atmosphere * (0.5 + sunInfluence * 0.5))
    );

    // Add sun glow near horizon
    vec3 sunTerm = sunColor *
                   sunInfluence *
                   (1.0 - sunHeight) *
                   (((1.0 - verticality) + atmosphere) * 0.5) *
                   sunStrength;

    color += mix(vec3(0.0), sunTerm, isDayTime);

    return color;
}

#endif // SKY_GLSL
