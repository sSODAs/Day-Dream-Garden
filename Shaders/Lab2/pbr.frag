#version 330 core

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uCamPos;

uniform vec3  uAlbedo;     // linear space
uniform float uMetallic;   // [0,1]
uniform float uRoughness;  // [0,1]
uniform float uAO;         // [0,1]

uniform int   uLightCount;
uniform vec3  uLightPos[4];
uniform vec3  uLightColor[4]; // linear intensity (HDR-ish)

const float PI = 3.14159265358979323846;

vec3 fresnelSchlick(vec3 H, vec3 V, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - max(dot(H, V), 0.0), 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 1e-6);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 1e-6);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggxV = GeometrySchlickGGX(NdotV, roughness);
    float ggxL = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

void main()
{
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCamPos - vWorldPos);

    // Avoid roughness = 0 (GGX becomes too sharp / unstable)
    float rough = clamp(uRoughness, 0.05, 1.0);

    // Base reflectance
    vec3 F0 = mix(vec3(0.04), uAlbedo, uMetallic);

    vec3 Lo = vec3(0.0);

    for(int i = 0; i < uLightCount; i++)
    {
        vec3 L = normalize(uLightPos[i] - vWorldPos);
        vec3 H = normalize(V + L);

        float distance = length(uLightPos[i] - vWorldPos);
        float attenuation = 1.0 / max(distance * distance, 1e-3);
        vec3 radiance = uLightColor[i] * attenuation;

        float D = DistributionGGX(N, H, rough);
        float G   = GeometrySmith(N, V, L, rough);
        vec3  F   = fresnelSchlick(H, V, F0);

        vec3 numerator = D * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular = numerator / max(denom, 1e-6);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - uMetallic);

        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = uAlbedo / PI;

        Lo += (kD * diffuse + specular) * radiance * NdotL;
    }

    // Ambient approximation
    vec3 kS_view = fresnelSchlick(N, V, F0);
    vec3 kD_view = (vec3(1.0) - kS_view) * (1.0 - uMetallic);

    // Diffuse ambient only for dielectrics (metals should not get this)
    vec3 ambientDiffuse = vec3(0.03) * uAlbedo * uAO * kD_view;

    // Fake specular environment (helps metals read as metals without real environment maps)
    vec3 envColor = vec3(0.6, 0.7, 0.9);     // "sky"
    float envStrength = 0.35;                // a bit stronger than 0.25
    float specStrength = (1.0 - rough) * (1.0 - rough); // smoother => much stronger
    vec3 ambientSpec = envColor * kS_view * envStrength * specStrength * uAO;

    vec3 color = ambientDiffuse + ambientSpec + Lo;

    // ✅ Tonemap (Reinhard) to fit HDR into [0,1]
    color = color / (color + vec3(1.0));

    // ✅ Gamma correct to sRGB
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
