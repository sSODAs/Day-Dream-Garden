#version 330 core
in vec2 vUV;
in float vColorVar;
in float vWind;
in float vAgeFade;
in float vTurnShade;

out vec4 FragColor;

uniform int uDebugMode;

void main()
{
    vec2 p = vUV * 2.0 - 1.0;
    float yAbs = abs(p.y);
    float leafWidth = 0.12 + 0.46 * pow(max(0.0, 1.0 - yAbs), 0.72);
    float edge = 1.0 - smoothstep(leafWidth - 0.035, leafWidth + 0.035, abs(p.x));
    float tip = smoothstep(1.03, 0.72, yAbs);
    float alpha = edge * tip * vAgeFade;

    if (alpha < 0.06)
        discard;

    float vein = 1.0 - smoothstep(0.012, 0.055, abs(p.x));
    float sideVeins = smoothstep(0.62, 0.96, sin((p.y + p.x * 1.65) * 24.0) * 0.5 + 0.5) * 0.08;

    vec3 deep = vec3(0.08, 0.22, 0.08);
    vec3 mid = mix(vec3(0.22, 0.48, 0.13), vec3(0.70, 0.57, 0.16), smoothstep(0.66, 1.0, vColorVar));
    vec3 sun = mix(vec3(0.76, 0.88, 0.28), vec3(0.96, 0.56, 0.18), smoothstep(0.78, 1.0, vColorVar));
    vec3 color = mix(deep, mid, 0.68 + vWind * 0.24);
    color = mix(color, sun, smoothstep(0.55, 0.98, vColorVar + vWind * 0.18) * 0.42);
    color += vec3(0.08, 0.10, 0.02) * vein;
    color -= vec3(0.02, 0.04, 0.01) * sideVeins;
    color *= vTurnShade;

    if (uDebugMode == 1)
        color = vec3(vWind, vAgeFade, vColorVar);

    FragColor = vec4(color, min(alpha * 0.92, 0.88));
}
