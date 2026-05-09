#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aParams;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;

out float vAlpha;
out float vWarmth;

void main()
{
    float phase = aParams.x;
    float size = aParams.y;
    float speed = aParams.z;
    vWarmth = aParams.w;

    float twinkle = sin(uTime * speed + phase) * 0.5 + 0.5;
    vec3 pos = aPos;
    pos.x += sin(uTime * 0.23 * speed + phase * 1.7) * 0.10;
    pos.y += sin(uTime * 0.37 * speed + phase * 2.1) * 0.07;
    pos.z += cos(uTime * 0.19 * speed + phase) * 0.06;

    vec4 clip = uProj * uView * vec4(pos, 1.0);
    gl_Position = clip;
    gl_PointSize = size * (5.6 / max(1.0, clip.w)) * mix(0.82, 1.45, twinkle);
    vAlpha = mix(0.35, 1.0, twinkle);
}
