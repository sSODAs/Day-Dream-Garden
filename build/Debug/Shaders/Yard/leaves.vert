#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

// xyz = clustered base position in world, w = leaf size
layout(location=3) in vec4 iBaseSize;
// x = phase, y = speed, z = sway, w = color variation
layout(location=4) in vec4 iParams;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;
uniform vec3 uWindVector;
uniform int uWindMode;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vUV;
out float vColorVar;
out float vWind;
out float vAgeFade;
out float vTurnShade;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float v = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        v += noise(p) * amp;
        p = mat2(1.7, 1.1, -1.1, 1.7) * p;
        amp *= 0.52;
    }
    return v;
}

void main()
{
    vec3 base = iBaseSize.xyz;
    float size = iBaseSize.w;
    float phase = iParams.x;
    float speed = iParams.y;
    float sway = iParams.z;
    float colorVar = iParams.w;

    vec3 wind3 = normalize(uWindVector + vec3(0.0001, 0.02, 0.0001));
    vec2 windDir = normalize(wind3.xz + vec2(0.0001));
    vec2 side2 = vec2(-windDir.y, windDir.x);
    vec3 side3 = vec3(side2.x, 0.0, side2.y);

    float gust = 0.5;
    float travelTime = uTime * max(uWindSpeed, 0.15) * speed + phase;
    if (uWindMode == 1)
    {
        float along = dot(base.xz, windDir);
        float broadWave = sin(along * 1.35 - uTime * uWindSpeed * 1.55) * 0.5 + 0.5;
        float gustNoise = fbm(base.xz * 0.72 - windDir * uTime * uWindSpeed * 0.38 + side2 * 0.21);
        gust = smoothstep(0.30, 0.96, mix(broadWave, gustNoise, 0.68));
    }
    else
    {
        float along = dot(base.xz, windDir);
        float cross = dot(base.xz, side2);
        gust = sin(along * 3.0 + uTime * uWindSpeed + phase) * 0.35 +
               sin(cross * 4.7 + uTime * uWindSpeed * 1.45 + phase) * 0.15 + 0.5;
    }

    float duration = mix(4.8, 8.2, colorVar);
    float age = fract(travelTime / duration);
    float fadeIn = smoothstep(0.00, 0.12, age);
    float fadeOut = 1.0 - smoothstep(0.82, 1.0, age);
    float ageFade = fadeIn * fadeOut;

    float windPush = (0.55 + uWindStrength * 1.65) * age;
    float fall = age * mix(0.85, 1.72, colorVar);
    vec3 center = base + wind3 * windPush - vec3(0.0, fall, 0.0);
    center += side3 * sin(travelTime * 2.5 + colorVar * 6.283) * sway * (0.20 + uWindStrength * 0.14);
    center.y += sin(travelTime * 3.2 + phase) * sway * 0.08 + wind3.y * windPush * 0.28;

    float spin = phase + travelTime * (1.35 + uWindStrength * 0.62) + age * 6.283;
    float c = cos(spin);
    float s = sin(spin);
    vec2 local = vec2(c * aPos.x - s * aPos.y, s * aPos.x + c * aPos.y);
    float flutterScale = 1.0 + sin(travelTime * 5.7 + phase) * 0.12;

    vec3 worldPos = center +
        uCameraRight * local.x * size * flutterScale +
        uCameraUp * local.y * size * 0.62;
    worldPos = (uModel * vec4(worldPos, 1.0)).xyz;

    vUV = aUV;
    vColorVar = colorVar;
    vWind = clamp(gust, 0.0, 1.0);
    vAgeFade = ageFade;
    vTurnShade = 0.72 + 0.28 * sin(spin);

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
