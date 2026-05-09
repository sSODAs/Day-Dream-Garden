#version 330 core
// Stylized color pass for the kinematic fish mesh.
in vec3 vColor;
in float vBody;
in float vMask;
in float vFlash;
in vec2 vUV;

out vec4 FragColor;

uniform sampler2D uFishTexture;

void main()
{
    vec4 texel = texture(uFishTexture, vUV);
    vec3 color = texel.rgb * vColor;
    color *= mix(0.94, 1.08, vFlash);
    float glowStrength = texel.a * (0.24 + 0.22 * vFlash) * smoothstep(0.05, 0.95, vBody);
    color += vec3(1.0, 0.45, 0.15) * glowStrength;
    FragColor = vec4(color, texel.a);
}
