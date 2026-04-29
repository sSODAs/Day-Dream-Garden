#version 330 core

uniform vec3 uColor;
uniform int  uDebugMode;

in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;
flat in int vID;

out vec4 FragColor;

// Deterministic pseudo-random color from ID
vec3 idColor(int id)
{
    float r = fract(sin(float(id) * 12.9898) * 43758.5453);
    float g = fract(sin(float(id) * 78.233 ) * 43758.5453);
    float b = fract(sin(float(id) * 39.425 ) * 43758.5453);
    return vec3(r, g, b);
}

// Simple procedural checkerboard from UV (no textures needed)
float checker(vec2 uv, float scale)
{
    vec2 p = floor(uv * scale);
    return mod(p.x + p.y, 2.0);
}

void main()
{
    // Mode 0: normal solid color
    if (uDebugMode == 0)
    {
        FragColor = vec4(uColor, 1.0);
        return;
    }

    // Mode 1: Vertex-ID color
    if (uDebugMode == 1)
    {
        FragColor = vec4(idColor(vID), 1.0);
        return;
    }

    // Mode 2: Position-as-color (local position mapped to [0,1])
    if (uDebugMode == 2)
    {
        vec2 rg = vPos.xy * 0.5 + 0.5;  // [-1,1] -> [0,1]
        FragColor = vec4(rg, 0.0, 1.0);
        return;
    }

    // Mode 3: UV-as-color (R=U, G=V)
    if (uDebugMode == 3)
    {
        FragColor = vec4(clamp(vUV, 0.0, 1.0), 0.0, 1.0);
        return;
    }

    // Mode 4: UV checkerboard (procedural texture)
    if (uDebugMode == 4)
    {
        float c = checker(vUV, 12.0);
        // white/black checkers
        FragColor = vec4(vec3(c), 1.0);
        return;
    }

    // Fallback: magenta
    FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
