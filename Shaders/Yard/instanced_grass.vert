#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

// x = world x, y = world z, z = height, w = width
layout(location=3) in vec4 iOffsetHeightWidth;
// x = yaw, y = phase, z = stiffness, w = color variation
layout(location=4) in vec4 iParams;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;
uniform vec3 uWindVector;
uniform int uWindMode;
uniform vec2 uYardCenter;
uniform vec2 uYardSize;

out vec2 vUV;
out float vHeight;
out float vColorVar;
out vec3 vWorldPos;
out float vWind;
out vec2 vGroundUV;

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

float terrainHeight(vec2 worldXZ)
{
    vec2 localXZ = worldXZ - vec2(0.0, 0.45);
    return -0.030 + fbm(localXZ * 1.2) * 0.10 + fbm(localXZ * 3.5 + 9.0) * 0.035;
}

void main()
{
    vec2 offset = iOffsetHeightWidth.xy;
    float bladeHeight = iOffsetHeightWidth.z;
    float bladeWidth = iOffsetHeightWidth.w;
    float yaw = iParams.x;
    float phase = iParams.y;
    float stiffness = iParams.z;

    vec3 local = aPos;
    local.x *= bladeWidth;
    local.z *= bladeWidth;
    local.y *= bladeHeight;

    float tip = aUV.y;
    vec3 windVector = normalize(uWindVector + vec3(0.0001, 0.0, 0.0001));
    vec2 windDir = normalize(windVector.xz + vec2(0.0001));
    vec2 side = vec2(-windDir.y, windDir.x);
    float windWave = 0.0;
    vec3 worldBend = vec3(0.0);

    if (uWindMode == 1)
    {
        float along = dot(offset, windDir);
        float cross = dot(offset, side);
        float travel = uTime * uWindSpeed;
        float broadWave = sin(along * 1.35 - travel * 1.55) * 0.5 + 0.5;
        float gustNoise = fbm(offset * 0.72 - windDir * travel * 0.38 + side * 0.21);
        float smallBreakup = fbm(offset * 2.15 - windDir * travel * 0.85 + vec2(cross * 0.05, along * 0.03));
        float gustField = smoothstep(0.34, 0.96, mix(broadWave, gustNoise, 0.68));
        gustField = mix(gustField, smallBreakup, 0.18);
        float push = (0.32 + gustField * 0.82) * uWindStrength;
        worldBend = windVector * push * 0.20 * pow(tip, 1.25) * stiffness;
        windWave = gustField;
    }
    else
    {
        windWave = sin(uTime * uWindSpeed * 2.0 + offset.x * 3.0 + offset.y * 2.0 + phase);
        float crossWave = cos(uTime * uWindSpeed + offset.x + phase);
        vec2 bend2 = windDir * windWave * 0.24 * uWindStrength;
        bend2 += side * crossWave * 0.09 * uWindStrength;
        worldBend.xz = bend2 * tip * stiffness;
    }

    float c = cos(yaw);
    float s = sin(yaw);
    vec2 rotated = vec2(c * local.x - s * local.z, s * local.x + c * local.z);

    vec3 worldPos;
    worldPos.xz = offset + rotated + worldBend.xz;
    worldPos.y = terrainHeight(offset) + 0.018 + local.y + worldBend.y * 0.45;
    worldPos = (uModel * vec4(worldPos, 1.0)).xyz;

    vUV = aUV;
    vHeight = tip;
    vColorVar = iParams.w;
    vWorldPos = worldPos;
    vWind = uWindMode == 1 ? windWave : windWave * 0.5 + 0.5;
    vGroundUV = (offset - uYardCenter) / uYardSize + vec2(0.5);

    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
