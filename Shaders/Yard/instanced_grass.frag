#version 330 core
in vec2 vUV;
in float vHeight;
in float vColorVar;
in vec3 vWorldPos;
in float vWind;
in vec2 vGroundUV;

out vec4 FragColor;

uniform int uDebugMode;
uniform sampler2D uGrassTexture;

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

void main()
{
    vec3 groundColor = texture(uGrassTexture, clamp(vGroundUV, vec2(0.0), vec2(1.0))).rgb;
    vec3 bottom = mix(groundColor * 0.52, vec3(0.06, 0.26, 0.12), 0.28);
    vec3 top = mix(groundColor * 1.26, vec3(0.82, 0.92, 0.24), 0.22);
    vec3 blue = vec3(0.04, 0.34, 0.31);
    vec3 yellow = vec3(0.95, 0.78, 0.15);

    vec3 color = mix(bottom, top, pow(vHeight, 0.68));
    float patchValue = noise(vWorldPos.xz * 2.7);
    color = mix(color, groundColor, patchValue * 0.48);
    color = mix(color, blue, smoothstep(0.60, 0.95, noise(vWorldPos.xz * 4.0 + 5.0)) * 0.22);
    color = mix(color, yellow, smoothstep(0.82, 0.98, vColorVar + patchValue * 0.18) * 0.26);
    color *= mix(0.90, 1.16, vColorVar);
    color *= mix(0.96, 1.24, vWind);

    if (uDebugMode == 1)
        color = vec3(vWind, vHeight, vColorVar);

    FragColor = vec4(color, 1.0);
}
