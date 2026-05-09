#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uAlpha;
uniform float uAlphaCutoff;
uniform vec3 uTint;
uniform float uBrightness;

void main()
{
    vec4 texel = texture(uTexture, vUV);
    float alpha = texel.a * uAlpha;
    if (alpha <= uAlphaCutoff)
        discard;
    vec3 color = clamp(texel.rgb * uTint * uBrightness, 0.0, 1.0);
    FragColor = vec4(color, alpha);
}
