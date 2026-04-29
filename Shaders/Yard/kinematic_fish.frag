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
    FragColor = vec4(color, texel.a);
}
