#version 330 core
in vec3 vColor;
in float vBody;
in float vMask;
in float vFlash;

out vec4 FragColor;

void main()
{
    vec3 color = vColor;
    color *= mix(0.78, 1.16, vFlash);
    color = mix(color, vec3(1.0, 0.88, 0.36), smoothstep(0.62, 1.0, vBody) * 0.20);
    color = mix(color, vec3(0.78, 0.20, 0.12), vMask * 0.22);
    FragColor = vec4(color, 1.0);
}
