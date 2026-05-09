#version 330 core
in float vAlpha;
in float vWarmth;

out vec4 FragColor;

void main()
{
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float d = dot(p, p);
    float halo = 1.0 - smoothstep(0.08, 1.0, d);
    float core = 1.0 - smoothstep(0.00, 0.12, d);

    vec3 warm = vec3(1.0, 0.90, 0.28);
    vec3 green = vec3(0.65, 1.0, 0.34);
    vec3 color = mix(warm, green, vWarmth * 0.35);
    color += core * vec3(0.85, 0.78, 0.25);

    FragColor = vec4(color, halo * vAlpha * 0.86);
}
