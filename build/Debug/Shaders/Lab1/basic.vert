#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // NEW (not used yet)
layout(location = 2) in vec2 aUV;     // NEW (not used yet)

uniform mat4 uMVP;
uniform int uDebugMode;

// Pass to fragment shader
out vec3 vPos;
out vec3 vNormal;
out vec2 vUV;
flat out int vID;

void main()
{    
    vPos = aPos;          // original object/local position (before MVP)
    vNormal = aNormal;    // normal vector
    vUV = aUV;            // texture coordinates
    vID  = gl_VertexID;   // vertex id for debug coloring
    gl_Position = uMVP * vec4(aPos, 1.0);
}
