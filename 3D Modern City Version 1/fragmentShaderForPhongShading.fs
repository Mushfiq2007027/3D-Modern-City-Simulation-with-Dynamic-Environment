#version 330 core
out vec4 FragColor;

// ---- Material ----
struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};

// ---- Light types ----
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3  position;
    float k_c;
    float k_l;
    float k_q;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

struct SpotLight {
    vec3  position;
    vec3  direction;
    float cutOff;
    float outerCutOff;
    float k_c;
    float k_l;
    float k_q;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

#define NR_POINT_LIGHTS 8

in vec3 FragPos;
in vec3 Normal;

uniform vec3        viewPos;
uniform DirLight    dirLight;
uniform PointLight  pointLights[NR_POINT_LIGHTS];
uniform SpotLight   spotLight;
uniform Material    material;

uniform bool  dirLightOn;
uniform bool  pointLightsOn;
uniform bool  spotLightOn;
uniform bool  ambientOn;
uniform bool  diffuseOn;
uniform bool  specularOn;
uniform bool  isEmissive;
uniform bool  isWater;
uniform float time;
uniform int   viewportMode;  // 0=combined 1=ambient 2=diffuse 3=directional

// ---- Prototypes ----
vec3 CalcDir  (DirLight   l, vec3 N, vec3 V);
vec3 CalcPoint(PointLight l, vec3 N, vec3 P, vec3 V);
vec3 CalcSpot (SpotLight  l, vec3 N, vec3 P, vec3 V);

void main()
{
    // --- Animated water ---
    if (isWater)
    {
        float w1 = sin(FragPos.x * 1.5 + time * 2.0)  * 0.5 + 0.5;
        float w2 = sin(FragPos.z * 2.0 - time * 3.0)  * 0.5 + 0.5;
        float w3 = cos(FragPos.x * 0.8 - time * 1.2)  * 0.5 + 0.5;
        float wave = (w1 * 0.5 + w2 * 0.35 + w3 * 0.15);
        vec3 deepCol    = vec3(0.04, 0.22, 0.58);
        vec3 shallowCol = vec3(0.18, 0.52, 0.88);
        vec3 foamCol    = vec3(0.55, 0.78, 1.0);
        vec3 wCol = mix(deepCol, shallowCol, wave);
        // subtle foam crest
        float foam = smoothstep(0.82, 0.95, w1) * 0.4;
        wCol = mix(wCol, foamCol, foam);
        FragColor = vec4(wCol, 0.92);
        return;
    }

    // --- Emissive objects glow by themselves ---
    if (isEmissive)
    {
        FragColor = vec4(material.diffuse, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 result = vec3(0.0);

    if (viewportMode == 0)
    {
        // Full combined lighting
        if (dirLightOn)
            result += CalcDir(dirLight, N, V);
        if (pointLightsOn)
            for (int i = 0; i < NR_POINT_LIGHTS; i++)
                result += CalcPoint(pointLights[i], N, FragPos, V);
        if (spotLightOn)
            result += CalcSpot(spotLight, N, FragPos, V);
        // Ensure a minimum so scene is never pitch black
        if (length(result) < 0.01)
            result = material.ambient * 0.08;
    }
    else if (viewportMode == 1)
    {
        // Ambient only
        vec3 a = dirLightOn ? dirLight.ambient : vec3(0.15);
        result = material.ambient * a;
    }
    else if (viewportMode == 2)
    {
        // Diffuse only
        vec3 L = normalize(-dirLight.direction);
        result = material.diffuse * max(dot(N, L), 0.0) * dirLight.diffuse;
    }
    else
    {
        // Directional light only
        result = CalcDir(dirLight, N, V);
    }

    FragColor = vec4(result, 1.0);
}

// ---- Directional light ----
vec3 CalcDir(DirLight l, vec3 N, vec3 V)
{
    vec3 L = normalize(-l.direction);
    vec3 R = reflect(-L, N);
    vec3 amb  = ambientOn  ? material.ambient  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? material.diffuse  * max(dot(N,L), 0.0) * l.diffuse : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return amb + diff + spec;
}

// ---- Point light ----
vec3 CalcPoint(PointLight l, vec3 N, vec3 P, vec3 V)
{
    vec3  L   = normalize(l.position - P);
    vec3  R   = reflect(-L, N);
    float d   = length(l.position - P);
    float att = 1.0 / (l.k_c + l.k_l * d + l.k_q * d * d);
    vec3 amb  = ambientOn  ? material.ambient  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? material.diffuse  * max(dot(N,L), 0.0) * l.diffuse : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return (amb + diff + spec) * att;
}

// ---- Spot light ----
vec3 CalcSpot(SpotLight l, vec3 N, vec3 P, vec3 V)
{
    vec3  L       = normalize(l.position - P);
    vec3  R       = reflect(-L, N);
    float d       = length(l.position - P);
    float att     = 1.0 / (l.k_c + l.k_l * d + l.k_q * d * d);
    float theta   = dot(L, normalize(-l.direction));
    float eps     = l.cutOff - l.outerCutOff;
    float intens  = clamp((theta - l.outerCutOff) / eps, 0.0, 1.0);
    vec3 amb  = ambientOn  ? material.ambient  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? material.diffuse  * max(dot(N,L), 0.0) * l.diffuse : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return (amb + diff * intens + spec * intens) * att;
}
