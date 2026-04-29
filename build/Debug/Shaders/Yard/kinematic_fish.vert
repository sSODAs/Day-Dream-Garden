#version 330 core
// Kinematic path-following fish with body trail deformation.
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=4) in vec2 aUV;

// xyz = local school offset, w = phase
layout(location=2) in vec4 iOffsetPhase;
// x = scale, y = speed, z = color variation, w = lane
layout(location=3) in vec4 iParams;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;
uniform float uYardZ;

out vec3 vColor;
out float vBody;
out float vMask;
out float vFlash;
out vec2 vUV;

const float PATH_X_RADIUS = 3.75;
const float PATH_Z_RADIUS = 1.85;
const float PATH_Y = 1.18;
const float PATH_Z_OFFSET = -0.72;
const float FISH_LOCAL_MIN_X = -0.82;
const float FISH_LOCAL_MAX_X = 0.60;
const float FISH_X_SCALE = 1.85;

mat2 rot2(float a)
{
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

vec3 safeNormalize(vec3 v, vec3 fallback)
{
    float m = length(v);
    if (m > 0.00001)
    {
        return v / m;
    }
    return fallback;
}

vec3 fishPathPoint(float phase)
{
    float s = sin(phase);
    float c = cos(phase);
    return vec3(s * PATH_X_RADIUS,
                PATH_Y,
                uYardZ + PATH_Z_OFFSET + s * c * PATH_Z_RADIUS);
}

vec3 fishPathVelocity(float phase)
{
    return vec3(cos(phase) * PATH_X_RADIUS,
                0.0,
                cos(phase * 2.0) * PATH_Z_RADIUS);
}

void main()
{
    float fishLocalLength = FISH_LOCAL_MAX_X - FISH_LOCAL_MIN_X;
    float body = clamp((aPos.x - FISH_LOCAL_MIN_X) / fishLocalLength, 0.0, 1.0);
    float tailMask = smoothstep(0.18, 0.88, 1.0 - body);

    float swimTime = uTime * iParams.y * 5.6 + iOffsetPhase.w;
    float pathDelay = iParams.w * 3.9 + iOffsetPhase.w * 0.015;
    float pathPhase = uTime * 0.34 - pathDelay;

    float fishWorldLength = fishLocalLength * iParams.x * FISH_X_SCALE;
    float pathSpeed = max(length(fishPathVelocity(pathPhase)), 0.65);
    float bodyTrail = (1.0 - body) * fishWorldLength / pathSpeed;
    float segmentPhase = pathPhase - bodyTrail;
    vec3 center = fishPathPoint(segmentPhase);
    vec3 tangent = safeNormalize(fishPathVelocity(segmentPhase), vec3(1.0, 0.0, 0.0));
    vec3 right = safeNormalize(vec3(tangent.z, 0.0, -tangent.x), vec3(0.0, 0.0, 1.0));
    vec3 up = vec3(0.0, 1.0, 0.0);

    float headLeadWave = sin(swimTime - (1.0 - body) * 2.7);
    float tailFlutter = sin(swimTime * 1.55 - (1.0 - body) * 5.3);
    float bend = headLeadWave * tailMask * 0.135 + tailFlutter * tailMask * tailMask * 0.035;

    float rollAngle = headLeadWave * tailMask * 0.22;
    mat2 roll = rot2(rollAngle);
    vec2 section = roll * vec2(aPos.y * iParams.x * 1.22,
                               aPos.z * iParams.x * 1.35);

    vec3 fishOffset =
        tangent * iOffsetPhase.x +
        up * (iOffsetPhase.y + sin(uTime * 0.95 + iOffsetPhase.w) * 0.035) +
        right * iOffsetPhase.z;

    vec3 worldPos = center + fishOffset + right * (bend + section.y) + up * section.x;

    vBody = body;
    vMask = tailMask;
    vFlash = sin(swimTime + body * 4.0) * 0.5 + 0.5;
    vUV = aUV;
    vColor = aColor * mix(0.98, 1.02, iParams.z);

    gl_Position = uProj * uView * uModel * vec4(worldPos, 1.0);
}
