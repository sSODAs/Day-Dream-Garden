#version 330 core
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uEmissive;
uniform float uAlpha;
uniform float uRoughness;
uniform float uSpecularStrength;
uniform float uTintStrength;
uniform float uVerticalTint;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uKeyLightColor;
uniform vec3 uPointLightPos[2];
uniform vec3 uPointLightColor[2];

// 0 = full shaded, 1 = lighting only, 2 = emissive only
uniform int uDebugMode;

vec3 evalPointLight(vec3 N, vec3 V, vec3 lightPos, vec3 lightColor, float roughness, float specularStrength)
{
    vec3 toLight = lightPos - vWorldPos;
    float dist = length(toLight);
    vec3 L = toLight / max(dist, 0.0001);
    vec3 H = normalize(L + V);

    float atten = 1.0 / (0.45 + dist * dist * 0.38);
    float ndl = max(dot(N, L), 0.0);

    float shininess = mix(96.0, 16.0, clamp(roughness, 0.0, 1.0));
    float spec = pow(max(dot(N, H), 0.0), shininess) * specularStrength;

    return lightColor * atten * (ndl + spec);
}

void main()
{
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCamPos - vWorldPos);

    vec3 Ld = normalize(uLightDir);
    vec3 H = normalize(Ld + V);

    float rough = clamp(uRoughness, 0.02, 1.0);
    float ndl = max(dot(N, Ld), 0.0);
    float shininess = mix(128.0, 18.0, rough);
    float dirSpec = pow(max(dot(N, H), 0.0), shininess) * uSpecularStrength;

    vec3 ambient = vec3(0.08, 0.10, 0.13);
    vec3 lighting = ambient + uKeyLightColor * (ndl * 0.85 + dirSpec * 0.45);
    lighting += evalPointLight(N, V, uPointLightPos[0], uPointLightColor[0], rough, uSpecularStrength);
    lighting += evalPointLight(N, V, uPointLightPos[1], uPointLightColor[1], rough, uSpecularStrength);

    float randomTint = mix(0.78, 1.22, clamp(vUV.x, 0.0, 1.0));
    float heightTint = 1.0 + uVerticalTint * (clamp(vUV.y, 0.0, 1.0) - 0.35);
    vec3 baseColor = uColor * mix(1.0, randomTint * heightTint, clamp(uTintStrength, 0.0, 1.0));

    vec3 color;
    if (uDebugMode == 1)
    {
        color = lighting * 0.45;
    }
    else if (uDebugMode == 2)
    {
        color = uEmissive;
    }
    else
    {
        color = baseColor * lighting + uEmissive;
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, uAlpha);
}
