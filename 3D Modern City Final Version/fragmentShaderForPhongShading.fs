#version 330 core
out vec4 FragColor;

// ---- Structs ----
struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight {
    vec3  position;
    float k_c, k_l, k_q;
    vec3  ambient, diffuse, specular;
};
struct SpotLight {
    vec3  position, direction;
    float cutOff, outerCutOff;
    float k_c, k_l, k_q;
    vec3  ambient, diffuse, specular;
};

#define NR_POINT_LIGHTS 8
#define NR_SPOT_LIGHTS  12

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 VertexColor;

uniform vec3        viewPos;
uniform DirLight    dirLight;
uniform PointLight  pointLights[NR_POINT_LIGHTS];
uniform SpotLight   spotLights[NR_SPOT_LIGHTS];
uniform int         numSpotLights;
uniform Material    material;

uniform bool  dirLightOn, pointLightsOn, spotLightOn;
uniform bool  ambientOn, diffuseOn, specularOn;
uniform bool  isEmissive, isWater;
uniform float time;
uniform int   viewportMode;   // 0=combined 1=ambient 2=diffuse 3=directional

// ---- Texture uniforms ----
uniform sampler2D texSampler;
uniform bool      useTexture;
uniform int       texMode;      // 0=off 1=simple 2=blend 3=vertex 4=frag
uniform bool      isNightSky;   // special glittering night sky

// ---- Effective material (set in main, used by Calc functions) ----
vec3 g_matDiff;
vec3 g_matAmb;

// ---- Prototypes ----
vec3 CalcDir  (DirLight   l, vec3 N, vec3 V);
vec3 CalcPoint(PointLight l, vec3 N, vec3 P, vec3 V);
vec3 CalcSpot (SpotLight  l, vec3 N, vec3 P, vec3 V);

// ==========================================================
void main()
{
    // --- Night sky glittering ---
    if (isNightSky) {
        vec4 skyC = texture(texSampler, TexCoords);
        float flicker = sin(time * 4.0 + FragPos.x * 7.3 + FragPos.z * 3.1) * 0.5 + 0.5;
        float bright  = smoothstep(0.65, 0.92, flicker) * 0.9;
        // Stars glitter on bright pixels of the night sky texture
        float starMask = smoothstep(0.55, 0.75, dot(skyC.rgb, vec3(0.333)));
        vec3  glitter  = skyC.rgb + vec3(bright * starMask);
        FragColor = vec4(clamp(glitter, 0.0, 1.0), 1.0);
        return;
    }

    // --- Animated water ---
    if (isWater) {
        if (useTexture && texMode > 0) {
            vec2 animUV = TexCoords + vec2(0.0, -time * 0.04);
            vec4 tc = texture(texSampler, animUV);
            FragColor = vec4(tc.rgb, 0.92);
            return;
        }
        float w1   = sin(FragPos.x*1.5 + time*2.0)*0.5+0.5;
        float w2   = sin(FragPos.z*2.0 - time*3.0)*0.5+0.5;
        float w3   = cos(FragPos.x*0.8 - time*1.2)*0.5+0.5;
        float wave = w1*0.5 + w2*0.35 + w3*0.15;
        vec3 deepC = vec3(0.04,0.22,0.58), shallC = vec3(0.18,0.52,0.88), foamC = vec3(0.55,0.78,1.0);
        vec3 wCol  = mix(deepC, shallC, wave);
        wCol = mix(wCol, foamC, smoothstep(0.82,0.95,w1)*0.4);
        FragColor = vec4(wCol, 0.92);
        return;
    }

    // --- Emissive (self-lit) ---
    if (isEmissive) {
        if (useTexture && texMode > 0) {
            vec4 tc = texture(texSampler, TexCoords);
            // Night: make window glass glow by boosting brightness
            FragColor = vec4(min(tc.rgb * 1.4, vec3(1.0)), 1.0);
        } else {
            FragColor = vec4(material.diffuse, 1.0);
        }
        return;
    }

    // ---- Determine effective material colours based on texMode ----
    g_matDiff = material.diffuse;
    g_matAmb  = material.ambient;

    if (texMode == 3) {
        // Color computed on vertex
        g_matDiff = VertexColor;
        g_matAmb  = VertexColor * 0.45;
    } else if (texMode == 4) {
        // Color computed on fragment (procedural)
        g_matDiff = fract(abs(FragPos) * vec3(0.014, 0.020, 0.017));
        g_matAmb  = g_matDiff * 0.45;
    } else if (useTexture && texMode == 1) {
        // Simple texture – texture replaces surface colour
        vec4 tc   = texture(texSampler, TexCoords);
        g_matDiff = tc.rgb;
        g_matAmb  = tc.rgb * 0.50;
    } else if (useTexture && texMode == 2) {
        // Blended texture + surface colour
        vec4 tc   = texture(texSampler, TexCoords);
        g_matDiff = mix(material.diffuse, tc.rgb, 0.55);
        g_matAmb  = mix(material.ambient, tc.rgb * 0.50, 0.55);
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 result = vec3(0.0);

    if (viewportMode == 0) {
        if (dirLightOn)    result += CalcDir(dirLight, N, V);
        if (pointLightsOn)
            for (int i = 0; i < NR_POINT_LIGHTS; i++)
                result += CalcPoint(pointLights[i], N, FragPos, V);
        if (spotLightOn)
            for (int i = 0; i < numSpotLights; i++)
                result += CalcSpot(spotLights[i], N, FragPos, V);
        if (length(result) < 0.01)
            result = g_matAmb * 0.08;
    } else if (viewportMode == 1) {
        vec3 a = dirLightOn ? dirLight.ambient : vec3(0.15);
        result  = g_matAmb * a;
    } else if (viewportMode == 2) {
        vec3 L  = normalize(-dirLight.direction);
        result  = g_matDiff * max(dot(N, L), 0.0) * dirLight.diffuse;
    } else {
        result = CalcDir(dirLight, N, V);
    }

    FragColor = vec4(result, 1.0);
}

// ---- Directional ----
vec3 CalcDir(DirLight l, vec3 N, vec3 V)
{
    vec3 L = normalize(-l.direction);
    vec3 R = reflect(-L, N);
    vec3 amb  = ambientOn  ? g_matAmb  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? g_matDiff * max(dot(N,L),0.0) * l.diffuse  : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return amb + diff + spec;
}
// ---- Point ----
vec3 CalcPoint(PointLight l, vec3 N, vec3 P, vec3 V)
{
    vec3  L   = normalize(l.position - P);
    vec3  R   = reflect(-L, N);
    float d   = length(l.position - P);
    float att = 1.0/(l.k_c + l.k_l*d + l.k_q*d*d);
    vec3 amb  = ambientOn  ? g_matAmb  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? g_matDiff * max(dot(N,L),0.0) * l.diffuse  : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return (amb + diff + spec) * att;
}
// ---- Spot ----
vec3 CalcSpot(SpotLight l, vec3 N, vec3 P, vec3 V)
{
    vec3  L      = normalize(l.position - P);
    vec3  R      = reflect(-L, N);
    float d      = length(l.position - P);
    float att    = 1.0/(l.k_c + l.k_l*d + l.k_q*d*d);
    float theta  = dot(L, normalize(-l.direction));
    float eps    = l.cutOff - l.outerCutOff;
    float intens = (eps > 0.0001) ? clamp((theta - l.outerCutOff)/eps, 0.0, 1.0)
                                  : (theta > l.cutOff ? 1.0 : 0.0);
    vec3 amb  = ambientOn  ? g_matAmb  * l.ambient  : vec3(0.0);
    vec3 diff = diffuseOn  ? g_matDiff * max(dot(N,L),0.0) * l.diffuse  : vec3(0.0);
    vec3 spec = specularOn ? material.specular * pow(max(dot(V,R),0.0), material.shininess) * l.specular : vec3(0.0);
    return (amb + diff*intens + spec*intens) * att;
}
