#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;

out vec3 vWorldPos;
out vec2 vUV;

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

void main()
{
    vec3 pos = aPos;
    float h = fbm(pos.xz * 1.2) * 0.10 + fbm(pos.xz * 3.5 + 9.0) * 0.035;
    pos.y += h;

    vec4 world = uModel * vec4(pos, 1.0);
    vWorldPos = world.xyz;
    vUV = aUV;
    gl_Position = uProj * uView * world;
}
