#version 330 core
in vec3 vWorldPos;
in vec2 vUV;
out vec4 FragColor;

uniform float uTime;
uniform float uWindStrength;
uniform float uWindSpeed;
uniform vec2 uWindDir;
uniform sampler2D uGrassTexture;
uniform int uDebugMode;
uniform int uWindMode;

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
    for (int i = 0; i < 5; ++i)
    {
        v += noise(p) * amp;
        p = mat2(1.6, 1.2, -1.2, 1.6) * p;
        amp *= 0.52;
    }
    return v;
}

void main()
{
    vec2 p = vWorldPos.xz;
    vec2 wind = normalize(uWindDir + vec2(0.0001));
    vec2 side = vec2(-wind.y, wind.x);
    float t = uTime * uWindSpeed;

    float longWave = sin(dot(p, wind) * 4.5 + t) * 0.5 + 0.5;
    float sideWave = sin(dot(p, side) * 7.0 + t * 1.35) * 0.5 + 0.5;
    float gust = fbm(p * 1.15 + wind * t * 0.12);
    float movingShadow = fbm(p * 2.3 + wind * t * 0.22 + side * sideWave);

    if (uWindMode == 1)
    {
        float along = dot(p, wind);
        float broadWave = sin(along * 1.25 - t * 1.35) * 0.5 + 0.5;
        float advectedNoise = fbm(p * 0.72 - wind * t * 0.34 + side * 0.18);
        longWave = smoothstep(0.28, 0.94, mix(broadWave, advectedNoise, 0.72));
        gust = fbm(p * 0.85 - wind * t * 0.26);
        movingShadow = fbm(p * 1.25 - wind * t * 0.42 + side * gust * 0.15);
    }

    float bladeMask = smoothstep(0.30, 0.92, fbm(vec2(p.x * 11.0 + t * 0.55, p.y * 2.0)));
    float patchValue = fbm(p * 3.2 + wind * t * 0.08);

    vec3 tex = texture(uGrassTexture, vUV * 1.15 + wind * t * 0.012).rgb;

    vec3 dark = vec3(0.06, 0.20, 0.15);
    vec3 blueGreen = vec3(0.08, 0.38, 0.34);
    vec3 mid = vec3(0.30, 0.62, 0.25);
    vec3 light = vec3(0.68, 0.82, 0.30);
    vec3 yellow = vec3(0.90, 0.76, 0.18);

    vec3 color = mix(dark, mid, patchValue);
    color = mix(color, tex * 0.95, 0.70);
    color = mix(color, blueGreen, smoothstep(0.55, 0.85, movingShadow) * 0.55);
    color = mix(color, light, smoothstep(0.58, 0.92, longWave * gust) * 0.70);
    color = mix(color, yellow, smoothstep(0.88, 0.98, patchValue + longWave * 0.22) * 0.45);

    float strokeShade = mix(0.72, 1.22, bladeMask);
    color *= strokeShade;

    // Warm sunlight from the upper-right like the reference image.
    float sunWash = smoothstep(-0.20, 1.0, p.x * 0.22 - p.y * 0.18);
    color += vec3(0.10, 0.08, 0.01) * sunWash;

    float lightPatch = sin(vWorldPos.x * 2.5 + uTime * 0.20) *
                       sin(vWorldPos.z * 2.0 + uTime * 0.15);
    lightPatch = smoothstep(0.20, 1.0, lightPatch);
    color += lightPatch * vec3(0.25, 0.25, 0.05);

    if (uDebugMode == 1)
        color = vec3(longWave, gust, movingShadow);

    FragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
