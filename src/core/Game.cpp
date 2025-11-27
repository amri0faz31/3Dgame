// core/Game.cpp
// Implements lifecycle functions. Currently minimal, acting as a seam
// for future subsystems (Renderer, Scene, LightingController, etc.).

#include "Game.h"
#include <iostream>
#include "render/Renderer.h"
#include "render/Shader.h"
#include "render/Camera.h"
#include "scene/Terrain.h"
#include "scene/TerrainSampler.h"
#include "scene/Sky.h"
#include "scene/Water.h"
#include "platform/Window.h"
#include "core/Time.h"
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// ImGui (optional) - only available when built with BUILD_IMGUI
#ifdef BUILD_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#endif
#include <random>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <exception>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

extern Window* g_windowPtr; // declared in main for access

// Explicit forward declarations so gameplay helpers always compile even if
// TerrainSampler.h is not included through other translation units.
void setActiveTerrain(Terrain* terrain);
float getTerrainHeightAt(float worldX, float worldZ);

namespace {
constexpr size_t kMaxBones = 128;
constexpr int kMaxFireParticles = 96;
constexpr int kFireQuadVertexCount = 6;
constexpr int kMaxPointLights = 2;
constexpr std::array<float, kFireQuadVertexCount * 4> kFireQuadVertices = {
    // corner.x, corner.y, u, v
    -0.5f, -0.5f, 0.0f, 1.0f,
     0.5f, -0.5f, 1.0f, 1.0f,
     0.5f,  0.5f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.0f, 1.0f,
     0.5f,  0.5f, 1.0f, 0.0f,
    -0.5f,  0.5f, 0.0f, 0.0f
};

std::string loadTextFile(const std::vector<std::string>& candidates){
    for(const auto& path : candidates){
        std::ifstream file(path, std::ios::binary);
        if(!file) continue;
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    return {};
}

std::string resolveExistingPath(const std::vector<std::string>& candidates){
    for(const auto& path : candidates){
        std::ifstream file(path, std::ios::binary);
        if(file) return path;
    }
    return {};
}

GLuint createBeaconDiscTexture(int resolution){
    const int size = std::max(resolution, 16);
    std::vector<unsigned char> pixels(size * size * 4, 0u);
    for(int y = 0; y < size; ++y){
        for(int x = 0; x < size; ++x){
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
            float nx = u * 2.0f - 1.0f;
            float ny = v * 2.0f - 1.0f;
            float r = std::sqrt(nx * nx + ny * ny);
            float mask = std::clamp(1.05f - r, 0.0f, 1.0f);
            float core = std::pow(mask, 1.2f);
            unsigned char alpha = static_cast<unsigned char>(std::clamp(core * 255.0f, 0.0f, 255.0f));
            unsigned char glow = static_cast<unsigned char>(std::clamp(220.0f + 35.0f * mask, 0.0f, 255.0f));
            size_t idx = (y * size + x) * 4;
            pixels[idx + 0] = glow;
            pixels[idx + 1] = glow;
            pixels[idx + 2] = glow;
            pixels[idx + 3] = alpha;
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if(tex == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

glm::vec3 sampleTerrainNormal(float worldX, float worldZ){
    const float eps = 0.25f;
    float hL = getTerrainHeightAt(worldX - eps, worldZ);
    float hR = getTerrainHeightAt(worldX + eps, worldZ);
    float hB = getTerrainHeightAt(worldX, worldZ - eps);
    float hF = getTerrainHeightAt(worldX, worldZ + eps);
    glm::vec3 tangentX(2.0f * eps, hR - hL, 0.0f);
    glm::vec3 tangentZ(0.0f, hF - hB, 2.0f * eps);
    glm::vec3 normal = glm::normalize(glm::cross(tangentZ, tangentX));
    if(glm::length2(normal) < 1e-4f){
        normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return normal;
}

glm::vec3 extractBindPosition(const BoneInfo& bone){
    glm::mat4 bind = glm::inverse(bone.offset);
    return glm::vec3(bind[3]);
}

glm::vec3 projectOntoPlane(const glm::vec3& v, const glm::vec3& normal){
    glm::vec3 projected = v - normal * glm::dot(v, normal);
    float len2 = glm::length2(projected);
    if(len2 < 1e-6f){
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
    return projected / std::sqrt(len2);
}

glm::mat4 composeTransform(const glm::vec3& position,
                           const glm::quat& rotation,
                           float uniformScale){
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model *= glm::mat4_cast(rotation);
    model = glm::scale(model, glm::vec3(uniformScale));
    return model;
}
}

static const char* kVertex = R"GLSL(
#version 450 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent; // unused
uniform mat4 uModel; uniform mat4 uView; uniform mat4 uProj;
uniform mat4 uLightSpace;
uniform sampler2D uHeightMap;
uniform float uHeightScale; // vertical scale (world units)
uniform vec2 uTexelSize;    // 1/textureSize
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec4 vFragPosLightSpace;
void main(){
    // Sample heightmap for displacement but use CPU-computed vertex normal (attribute)
    float h = texture(uHeightMap, aUV).r;
    vec3 displaced = vec3(aPos.x, h * uHeightScale, aPos.z);
    vec4 wp = uModel * vec4(displaced,1.0);
    // Transform the precomputed normal to world space
    vNormal = normalize(mat3(uModel) * aNormal);
    vWorldPos = wp.xyz; vUV = aUV;
    // Position in light space for shadow mapping
    vFragPosLightSpace = uLightSpace * wp;
    gl_Position = uProj * uView * wp;
}
)GLSL";

static const char* kFragment = R"GLSL(
#version 450 core
in vec3 vNormal; in vec3 vWorldPos; in vec2 vUV; in vec4 vFragPosLightSpace;
out vec4 FragColor;
uniform vec3 uLightDir; uniform vec3 uLightColor; uniform float uAmbient; uniform bool uIsNight;
uniform float uSpecularStrength; uniform float uShininess; uniform vec3 uCameraPos;
uniform float uHeightScale;
uniform sampler2D uGrassTex; // tiled grass detail
uniform float uGrassScale;
uniform sampler2D uHeightMap;
uniform sampler2D uTexFungus;
uniform sampler2D uTexSandgrass;
uniform sampler2D uTexRocks;
uniform sampler2D uShadowMap;
uniform mat4 uLightSpace;
uniform vec3 uSkyColor; uniform float uFogStart; uniform float uFogRange;
const int MAX_POINT_LIGHTS = 2;
uniform int uPointLightCount;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightIntensity[MAX_POINT_LIGHTS];
uniform float uPointLightRadius[MAX_POINT_LIGHTS];
uniform bool uSpotLightEnabled;
uniform vec3 uSpotLightPos;
uniform vec3 uSpotLightDir;
uniform vec3 uSpotLightColor;
uniform float uSpotLightIntensity;
uniform float uSpotLightRange;
uniform float uSpotLightInnerCutoff;
uniform float uSpotLightOuterCutoff;

// Base terrain palette: light green, dark green, and light brown
const vec3 lightGreen  = vec3(0.35, 0.80, 0.30);
const vec3 darkGreen   = vec3(0.06, 0.40, 0.12);
const vec3 lightBrown  = vec3(0.48, 0.37, 0.28);

void main(){
    vec3 n = normalize(vNormal);
    // Subtle procedural detail to avoid perfectly smooth hills; keeps lighting lively without fake glow
    vec3 detailNormal = vec3(
        sin(vWorldPos.x * 0.35),
        0.0,
        cos(vWorldPos.z * 0.35)
    ) * 0.03;
    n = normalize(n + detailNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Compute steepness (0 = flat, 1 = vertical)
    float steepness = clamp(1.0 - n.y, 0.0, 1.0);
    float slopeBlend = smoothstep(0.12, 0.60, steepness);

    // Blend light/dark greens by slope (flat -> lightGreen, steep -> darkGreen)
    vec3 base = mix(lightGreen, darkGreen, slopeBlend);

    // Sample grass texture (tiled) and blend into base color for more realism
    vec3 grassSample = texture(uGrassTex, vUV * uGrassScale).rgb;
    base = mix(base, grassSample, 0.5);

    // Sample biome textures
    vec3 fungus = texture(uTexFungus, vUV * uGrassScale).rgb;
    vec3 sandgrass = texture(uTexSandgrass, vUV * uGrassScale).rgb;
    vec3 rocks = texture(uTexRocks, vUV * uGrassScale).rgb;

    // Height-based mapping using heightmap value (0..1)
    float hVal = texture(uHeightMap, vUV).r;
    vec3 mappedColor;
    if (hVal <= 0.15) {
        mappedColor = fungus;
    } else if (hVal <= 0.30) {
        float f = smoothstep(0.15, 0.30, hVal);
        mappedColor = mix(fungus, sandgrass, f);
    } else if (hVal <= 0.60) {
        mappedColor = sandgrass;
    } else if (hVal <= 0.85) {
        float f = smoothstep(0.60, 0.85, hVal);
        mappedColor = mix(sandgrass, rocks, f);
    } else {
        mappedColor = rocks;
    }

    // Blend texture mapping into base color (texture influence weight)
    base = mix(base, mappedColor, 0.8);

    // Simple shadow lookup using shadow map
    // Transform fragment pos into light space and compute shadow factor
    vec4 projCoords = vFragPosLightSpace;
    projCoords /= projCoords.w;
    // Transform to [0,1]
    vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z * 0.5 + 0.5;
    float shadow = 0.0;
    // If outside shadow map, consider lit
    if(shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
        // PCF 3x3
        float bias = max(0.005 * (1.0 - dot(normalize(vNormal), normalize(-uLightDir))), 0.001);
        float samples = 0.0;
        float count = 0.0;
        for(int x=-1; x<=1; ++x){
            for(int y=-1; y<=1; ++y){
                vec2 off = vec2(float(x), float(y)) * (1.0 / 1024.0); // sample radius (cheap)
                float depthSample = texture(uShadowMap, shadowUV + off).r;
                if(currentDepth - bias > depthSample) samples += 1.0;
                count += 1.0;
            }
        }
        shadow = samples / count;
    }
    // Convert to direct-light multiplier (0 = fully shadowed, 1 = fully lit)
    float shadowFactor = clamp(1.0 - shadow, 0.05, 1.0);

    // Height-based tint: higher areas get a light-brown tint (e.g., sparse vegetation / plateau)
    float heightFactor = clamp(vWorldPos.y / uHeightScale, 0.0, 1.0);
    float heightBlend = smoothstep(0.6, 0.95, heightFactor);
    base = mix(base, lightBrown, heightBlend);

    // Simple ambient occlusion-like darkening for very steep areas
    float ao = 1.0 - smoothstep(0.45, 0.9, steepness) * 0.28;

    // Physically-based specular reflection (Blinn-Phong with proper Fresnel)
    float NdotH = max(dot(n, H), 0.0);
    float NdotV = max(dot(n, V), 0.0);
    float NdotL = max(dot(n, L), 0.0);
    
    // Schlick's Fresnel approximation (F0 = 0.04 for non-metallic terrain)
    float F0 = 0.04;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    
    // Specular reflection - only on surfaces facing light
    float specPower = pow(NdotH, uShininess);
    float spec = specPower * uSpecularStrength * fresnel * NdotL;
    
    // Very subtle rim lighting for depth perception
    float rim = pow(1.0 - NdotV, 3.0) * 0.04 * NdotL;

    // Night-specific suppression so terrain reflections nearly disappear
    float nightSpecScale = uIsNight ? 0.04 : 1.0;
    spec *= nightSpecScale;
    rim *= nightSpecScale;

    // Realistic ambient lighting (much weaker, modulated by AO and shadows)
    // Ambient should be minimal - most light comes from diffuse calculation
    // Night uses 0.15x multiplier (85% reduction), day uses 0.4x (60% reduction)
    float ambientMult = uIsNight ? 0.15 : 0.4;
    vec3 ambient = base * uAmbient * ao * ambientMult;
    
    // Diffuse is the main light source - Lambert's cosine law (dot product with normal)
    // This properly calculates light based on surface angle to light source
    vec3 diffuse = base * NdotL * uLightColor * shadowFactor;
    
    vec3 specular = spec * uLightColor * shadowFactor;
    vec3 rimLight = rim * uLightColor * shadowFactor;
    
    vec3 color = ambient + diffuse + specular + rimLight;

    for(int i = 0; i < uPointLightCount; ++i){
        vec3 toLight = uPointLightPos[i] - vWorldPos;
        float distPoint = length(toLight);
        if(distPoint < uPointLightRadius[i]){
            vec3 pointDir = normalize(toLight);
            float attenuation = 1.0 - distPoint / uPointLightRadius[i];
            attenuation = attenuation * attenuation;
            float pointDiffuseN = max(dot(n, pointDir), 0.0);
            if(pointDiffuseN > 0.0){
                vec3 pointColor = uPointLightColor[i] * uPointLightIntensity[i];
                vec3 pointDiffuse = base * pointDiffuseN * pointColor;
                float pointSpecPow = pow(max(dot(n, normalize(pointDir + V)), 0.0), uShininess);
                float pointSpecStrength = pointSpecPow * uSpecularStrength * nightSpecScale;
                vec3 pointSpec = pointSpecStrength * pointColor;
                color += (pointDiffuse + pointSpec) * attenuation;
            }
        }
    }

    if(uSpotLightEnabled){
        vec3 toFrag = vWorldPos - uSpotLightPos;
        float distSpot = length(toFrag);
        if(distSpot < uSpotLightRange){
            vec3 dirFromLight = normalize(toFrag);
            float theta = dot(normalize(uSpotLightDir), dirFromLight);
            if(theta > uSpotLightOuterCutoff){
                float epsilon = max(uSpotLightInnerCutoff - uSpotLightOuterCutoff, 0.0001);
                float coneFactor = clamp((theta - uSpotLightOuterCutoff) / epsilon, 0.0, 1.0);
                float distanceFactor = clamp(1.0 - distSpot / uSpotLightRange, 0.0, 1.0);
                float attenuation = coneFactor * coneFactor * distanceFactor * distanceFactor;
                vec3 lightDir = normalize(uSpotLightPos - vWorldPos);
                float spotDiffuse = max(dot(n, lightDir), 0.0);
                if(spotDiffuse > 0.0){
                    vec3 spotColor = uSpotLightColor * uSpotLightIntensity;
                    vec3 spotDiffuseCol = base * spotDiffuse * spotColor;
                    float spotSpecPow = pow(max(dot(n, normalize(lightDir + V)), 0.0), uShininess);
                    float spotSpecStrength = spotSpecPow * uSpecularStrength * nightSpecScale;
                    vec3 spotSpec = spotSpecStrength * spotColor;
                    color += (spotDiffuseCol + spotSpec) * attenuation;
                }
            }
        }
    }
    
    // Linear fog based on distance to camera (start/range) for controlled blending
    // Fog disabled; keep terrain color untouched by distance-based blending
    
    // Simple gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}
)GLSL";

static const char* kWaterVertex = R"GLSL(
#version 450 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightSpace;
uniform float uTime;
uniform sampler2D uWaveHeight0;
uniform sampler2D uWaveHeight1;
uniform vec2 uLayer0Speed;
uniform vec2 uLayer1Speed;
uniform float uLayer0Strength;
uniform float uLayer1Strength;
uniform float uBlendSharpness;

out VS_OUT {
    vec3 worldPos;
    vec2 uv0;
    vec2 uv1;
    vec4 fragPosLightSpace;
    float layerBlend;
} vs_out;

void main(){
    vec2 scroll0 = aUV * 0.25 + uLayer0Speed * uTime;
    vec2 scroll1 = aUV * 0.45 + uLayer1Speed * uTime;

    float h0 = texture(uWaveHeight0, scroll0).r * uLayer0Strength;
    float h1 = texture(uWaveHeight1, scroll1).r * uLayer1Strength;
    float blend = smoothstep(0.0, 1.0, (h0 - h1) * uBlendSharpness * 0.5 + 0.5);
    float height = mix(h0, h1, blend) * 1.05;

    vec3 pos = aPos;
    pos.y += height;

    vec4 world = uModel * vec4(pos, 1.0);
    vs_out.worldPos = world.xyz;
    vs_out.uv0 = scroll0;
    vs_out.uv1 = scroll1;
    vs_out.fragPosLightSpace = uLightSpace * world;
    vs_out.layerBlend = blend;
    gl_Position = uProj * uView * world;
}
)GLSL";

static const char* kWaterFragment = R"GLSL(
#version 450 core
in VS_OUT {
    vec3 worldPos;
    vec2 uv0;
    vec2 uv1;
    vec4 fragPosLightSpace;
    float layerBlend;
} fs_in;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uSkyColor;
const int MAX_POINT_LIGHTS = 2;
uniform int uPointLightCount;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightIntensity[MAX_POINT_LIGHTS];
uniform float uPointLightRadius[MAX_POINT_LIGHTS];
uniform bool uSpotLightEnabled;
uniform vec3 uSpotLightPos;
uniform vec3 uSpotLightDir;
uniform vec3 uSpotLightColor;
uniform float uSpotLightIntensity;
uniform float uSpotLightRange;
uniform float uSpotLightInnerCutoff;
uniform float uSpotLightOuterCutoff;
uniform float uFogStart;
uniform float uFogRange;
uniform sampler2D uWaveNormal0;
uniform sampler2D uWaveNormal1;
uniform samplerCube uEnvMap;
uniform sampler2D uTerrainHeightMap;
uniform sampler2D uShadowMap;
uniform float uWorldSize;
uniform float uHeightScale;
uniform float uFoamThreshold;
uniform float uFoamIntensity;
uniform float uRefractStrength;
uniform float uReflectStrength;

float shadowFactor(vec4 lightSpacePos);

void main(){
    vec3 V = normalize(uCameraPos - fs_in.worldPos);
    vec3 L = normalize(-uLightDir);

    vec3 n0 = texture(uWaveNormal0, fs_in.uv0).xyz * 2.0 - 1.0;
    vec3 n1 = texture(uWaveNormal1, fs_in.uv1).xyz * 2.0 - 1.0;
    vec3 n = normalize(mix(n0, n1, fs_in.layerBlend));

    vec2 terrainUV = (fs_in.worldPos.xz / uWorldSize) + vec2(0.5);
    float terrainH = texture(uTerrainHeightMap, terrainUV).r * uHeightScale;
    float depth = clamp((fs_in.worldPos.y - terrainH) / 20.0, 0.0, 1.0);

    // Rich blue ocean colors
    vec3 shallow = vec3(0.12, 0.45, 0.75);  // Bright blue shallow
    vec3 deep    = vec3(0.02, 0.15, 0.35);  // Deep navy blue
    vec3 baseColor = mix(shallow, deep, depth * depth);  // Non-linear depth

    // Physically-based water reflection using Fresnel (Schlick approximation)
    float NdotV = max(dot(n, V), 0.0);
    float F0_water = 0.02;  // Water has ~2% reflectance at normal incidence
    float fresnel = F0_water + (1.0 - F0_water) * pow(1.0 - NdotV, 5.0);
    
    // Sky/environment reflection
    vec3 reflectDir = reflect(-V, n);
    vec3 reflectColor = texture(uEnvMap, reflectDir).rgb;
    
    // Refraction (view into water)
    vec3 refractDir = refract(-V, n, 0.75);  // Air to water IOR ratio
    vec3 refractColor = (refractDir != vec3(0.0)) ? texture(uEnvMap, refractDir).rgb : baseColor;
    refractColor = mix(baseColor, refractColor, 0.3);  // Mostly use base water color
    
    // Mix reflection and refraction based on Fresnel
    vec3 waterColor = mix(refractColor, reflectColor, fresnel);
    
    // Add water color absorption (deeper = more color)
    waterColor = mix(waterColor, baseColor, depth * 0.7);

    // Foam on wave crests
    float curvature = clamp(length(vec2(dFdx(n.y), dFdy(n.y))) * 25.0, 0.0, 1.0);
    float foamMask = smoothstep(uFoamThreshold, 1.2, curvature + (1.0 - depth));
    vec3 foam = vec3(0.9) * foamMask * uFoamIntensity;
    waterColor = mix(waterColor, foam, foamMask);

    // Diffuse lighting (water surface)
    float NdotL = max(dot(n, L), 0.0);
    float diff = NdotL * 0.3;  // Water doesn't diffuse much light
    
    // Specular highlight (sun glint) - Blinn-Phong
    vec3 H = normalize(L + V);
    float NdotH = max(dot(n, H), 0.0);
    float specPower = pow(NdotH, 256.0);  // Very sharp highlight for water
    float spec = specPower * fresnel * 2.0;  // Modulated by Fresnel
    
    float shadow = shadowFactor(fs_in.fragPosLightSpace);

    // Combine lighting: base water color + diffuse + specular highlight
    vec3 color = waterColor * (0.85 + diff * 0.15) * shadow + spec * uLightColor * shadow;

    for(int i = 0; i < uPointLightCount; ++i){
        vec3 toLight = uPointLightPos[i] - fs_in.worldPos;
        float distPoint = length(toLight);
        if(distPoint < uPointLightRadius[i]){
            vec3 pointDir = normalize(toLight);
            float attenuation = 1.0 - distPoint / uPointLightRadius[i];
            attenuation *= attenuation;
            float nDotPoint = max(dot(n, pointDir), 0.0);
            if(nDotPoint > 0.0){
                vec3 pointColor = uPointLightColor[i] * uPointLightIntensity[i];
                vec3 pointDiffuse = waterColor * nDotPoint * pointColor;
                vec3 HPoint = normalize(pointDir + V);
                float pointSpec = pow(max(dot(n, HPoint), 0.0), 192.0);
                vec3 pointSpecular = pointSpec * pointColor * 0.5;
                color += (pointDiffuse + pointSpecular) * attenuation;
            }
        }
    }

    if(uSpotLightEnabled){
        vec3 toFrag = fs_in.worldPos - uSpotLightPos;
        float distSpot = length(toFrag);
        if(distSpot < uSpotLightRange){
            vec3 dirFromLight = normalize(toFrag);
            float theta = dot(normalize(uSpotLightDir), dirFromLight);
            if(theta > uSpotLightOuterCutoff){
                float epsilon = max(uSpotLightInnerCutoff - uSpotLightOuterCutoff, 0.0001);
                float coneFactor = clamp((theta - uSpotLightOuterCutoff) / epsilon, 0.0, 1.0);
                float distanceFactor = clamp(1.0 - distSpot / uSpotLightRange, 0.0, 1.0);
                float attenuation = coneFactor * coneFactor * distanceFactor * distanceFactor;
                vec3 lightDir = normalize(uSpotLightPos - fs_in.worldPos);
                float spotDiffuse = max(dot(n, lightDir), 0.0);
                if(spotDiffuse > 0.0){
                    vec3 spotColor = uSpotLightColor * uSpotLightIntensity;
                    vec3 spotDiffuseCol = waterColor * spotDiffuse * spotColor;
                    float spotSpec = pow(max(dot(n, normalize(lightDir + V)), 0.0), 160.0);
                    vec3 spotSpecular = spotSpec * spotColor * 0.4;
                    color += (spotDiffuseCol + spotSpecular) * attenuation;
                }
            }
        }
    }

    // Fog disabled; water retains full color at all distances

    float depthAlpha = mix(0.98, 0.78, depth);
    float alpha = mix(depthAlpha, 1.0, foamMask * 0.15);
    FragColor = vec4(color, alpha);
}

float shadowFactor(vec4 lightSpacePos){
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    vec2 uv = proj.xy * 0.5 + 0.5;
    float current = proj.z * 0.5 + 0.5;
    if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;
    
    // Higher quality PCF for water
    vec2 texelSize = 1.0 / vec2(4096.0);
    float bias = 0.001;
    float sum = 0.0;
    int samples = 0;
    for(int x=-2; x<=2; ++x){
        for(int y=-2; y<=2; ++y){
            vec2 offset = vec2(x,y) * texelSize;
            float sampleDepth = texture(uShadowMap, uv + offset).r;
            sum += current - bias > sampleDepth ? 0.6 : 1.0;
            samples++;
        }
    }
    return sum / float(samples);
}

)GLSL";

static const char* kGrassVertex = R"GLSL(
#version 450 core
layout(location=0) in vec3 aPatchPos;
layout(location=1) in float aSeed;
out VS_OUT {
    vec3 patchPos;
    float seed;
} vs_out;
void main(){
    vs_out.patchPos = aPatchPos;
    vs_out.seed = aSeed;
}
)GLSL";

static const char* kGrassGeometry = R"GLSL(
#version 450 core
layout(points) in;
layout(triangle_strip, max_vertices = 24) out;
in VS_OUT {
    vec3 patchPos;
    float seed;
} gs_in[];
out GS_OUT {
    vec2 uv;
    vec3 worldPos;
    vec4 lightSpacePos;
} gs_out;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightSpace;
uniform float uTime;
uniform vec2 uWindDir;
uniform float uBladeWidth;
uniform vec2 uAtlasTileScale;
uniform float uWindStrength;

const vec3 kBaseDirs[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.70710678, 0.0, 0.70710678),
    vec3(0.70710678, 0.0, -0.70710678)
);

float hash11(float n){
    return fract(sin(n) * 43758.5453);
}

float hash31(vec3 p){
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void emitQuad(vec3 root, vec3 dir, float height, float width, vec2 tileOrigin, vec2 tileScale, float swayPhase){
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, dir));
    float halfWidth = width * 0.5;
    float sway = sin(swayPhase) * uWindStrength;
    
    // Realistic curved blade - narrower at tip, curved with wind
    vec3 mid = root + up * (height * 0.5) + dir * sway * height * 0.3;
    vec3 tip = root + up * height + dir * sway * height;
    float tipWidth = width * 0.3;  // Tapered tip

    vec3 bottomLeft = root - right * halfWidth;
    vec3 bottomRight = root + right * halfWidth;
    vec3 midLeft = mid - right * halfWidth * 0.7;  // Slight taper
    vec3 midRight = mid + right * halfWidth * 0.7;
    vec3 topLeft = tip - right * tipWidth * 0.5;
    vec3 topRight = tip + right * tipWidth * 0.5;

    vec4 clipBL = uProj * uView * vec4(bottomLeft, 1.0);
    vec4 clipBR = uProj * uView * vec4(bottomRight, 1.0);
    vec4 clipML = uProj * uView * vec4(midLeft, 1.0);
    vec4 clipMR = uProj * uView * vec4(midRight, 1.0);
    vec4 clipTL = uProj * uView * vec4(topLeft, 1.0);
    vec4 clipTR = uProj * uView * vec4(topRight, 1.0);

    vec2 uvBL = tileOrigin + vec2(0.0, 1.0) * tileScale;
    vec2 uvBR = tileOrigin + vec2(1.0, 1.0) * tileScale;
    vec2 uvML = tileOrigin + vec2(0.0, 0.5) * tileScale;
    vec2 uvMR = tileOrigin + vec2(1.0, 0.5) * tileScale;
    vec2 uvTL = tileOrigin + vec2(0.0, 0.0) * tileScale;
    vec2 uvTR = tileOrigin + vec2(1.0, 0.0) * tileScale;

    // Bottom segment
    gl_Position = clipBL; gs_out.uv = uvBL; gs_out.worldPos = bottomLeft; gs_out.lightSpacePos = uLightSpace * vec4(bottomLeft, 1.0); EmitVertex();
    gl_Position = clipBR; gs_out.uv = uvBR; gs_out.worldPos = bottomRight; gs_out.lightSpacePos = uLightSpace * vec4(bottomRight, 1.0); EmitVertex();
    gl_Position = clipML; gs_out.uv = uvML; gs_out.worldPos = midLeft; gs_out.lightSpacePos = uLightSpace * vec4(midLeft, 1.0); EmitVertex();
    gl_Position = clipMR; gs_out.uv = uvMR; gs_out.worldPos = midRight; gs_out.lightSpacePos = uLightSpace * vec4(midRight, 1.0); EmitVertex();
    EndPrimitive();
    
    // Top segment
    gl_Position = clipML; gs_out.uv = uvML; gs_out.worldPos = midLeft; gs_out.lightSpacePos = uLightSpace * vec4(midLeft, 1.0); EmitVertex();
    gl_Position = clipMR; gs_out.uv = uvMR; gs_out.worldPos = midRight; gs_out.lightSpacePos = uLightSpace * vec4(midRight, 1.0); EmitVertex();
    gl_Position = clipTL; gs_out.uv = uvTL; gs_out.worldPos = topLeft; gs_out.lightSpacePos = uLightSpace * vec4(topLeft, 1.0); EmitVertex();
    gl_Position = clipTR; gs_out.uv = uvTR; gs_out.worldPos = topRight; gs_out.lightSpacePos = uLightSpace * vec4(topRight, 1.0); EmitVertex();
    EndPrimitive();
}

void main(){
    vec3 root = gs_in[0].patchPos;
    float baseSeed = gs_in[0].seed;
    for(int i = 0; i < 3; ++i){
        float seed = hash11(baseSeed + float(i) * 13.37);
        float height = mix(0.9, 1.7, hash11(seed * 45.3));
        float width = uBladeWidth * mix(0.7, 1.3, hash11(seed * 11.7));
        float phase = dot(root.xz, uWindDir * 0.22) + uTime * (0.8 + hash11(seed * 7.1));
        int tileIdx = int(floor(hash11(seed * 5.1) * 4.0));
        vec2 tileOrigin = vec2(float(tileIdx % 2), float(tileIdx / 2)) * uAtlasTileScale;
        emitQuad(root, normalize(kBaseDirs[i]), height, width, tileOrigin, uAtlasTileScale, phase);
    }
}
)GLSL";

static const char* kGrassFragment = R"GLSL(
#version 450 core
in GS_OUT {
    vec2 uv;
    vec3 worldPos;
    vec4 lightSpacePos;
} fs_in;
out vec4 FragColor;
uniform sampler2D uGrassAtlas;
uniform sampler2D uShadowMap;
uniform float uAlphaCutoff;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
const int MAX_POINT_LIGHTS = 2;
uniform int uPointLightCount;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightIntensity[MAX_POINT_LIGHTS];
uniform float uPointLightRadius[MAX_POINT_LIGHTS];

float getShadow() {
    vec3 proj = fs_in.lightSpacePos.xyz / fs_in.lightSpacePos.w;
    vec2 uv = proj.xy * 0.5 + 0.5;
    float currentDepth = proj.z * 0.5 + 0.5;
    if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;

    float bias = 0.0025;
    float occlusion = 0.0;
    vec2 texelSize = vec2(1.0 / 1024.0);
    for(int x = -1; x <= 1; ++x){
        for(int y = -1; y <= 1; ++y){
            vec2 offset = vec2(x, y) * texelSize;
            float sampleDepth = texture(uShadowMap, uv + offset).r;
            if(currentDepth - bias > sampleDepth){
                occlusion += 1.0;
            }
        }
    }
    occlusion /= 9.0;
    return clamp(1.0 - occlusion, 0.0, 1.0);
}

void main(){
    vec4 tex = texture(uGrassAtlas, fs_in.uv);
    if(tex.a < uAlphaCutoff) discard;

    vec3 normal = normalize(vec3(0.0, 1.0, 0.25));
    vec3 lightDir = normalize(-uLightDir);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float shadowFactor = getShadow();

    vec3 ambient = tex.rgb * uAmbientColor;
    vec3 diffuse = tex.rgb * NdotL * uLightColor * shadowFactor;

    for(int i = 0; i < uPointLightCount; ++i){
        vec3 toLight = uPointLightPos[i] - fs_in.worldPos;
        float dist = length(toLight);
        if(dist < uPointLightRadius[i]){
            vec3 pointDir = normalize(toLight);
            float pointDiffuse = max(dot(normal, pointDir), 0.0);
            if(pointDiffuse > 0.0){
                float attenuation = 1.0 - dist / uPointLightRadius[i];
                attenuation *= attenuation;
                vec3 pointColor = uPointLightColor[i] * uPointLightIntensity[i];
                diffuse += tex.rgb * pointDiffuse * pointColor * attenuation;
            }
        }
    }

    vec3 lit = ambient + diffuse;
    lit = mix(lit, vec3(0.92, 1.0, 0.88), 0.12);

    FragColor = vec4(lit, tex.a);
}
)GLSL";

static const char* kFireParticleVert = R"GLSL(
#version 450 core
layout(location=0) in vec2 aCorner;    // quad corner (-0.5..0.5)
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aPosSize;   // xyz world position, w size
layout(location=3) in vec2 aLifeSeed;  // x life ratio, y seed
uniform mat4 uViewProj;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
out vec2 vUV;
out float vLife;
void main(){
    vec3 offset = (uCameraRight * aCorner.x + uCameraUp * aCorner.y) * aPosSize.w;
    vec3 worldPos = aPosSize.xyz + offset;
    gl_Position = uViewProj * vec4(worldPos, 1.0);
    vUV = aUV;
    vLife = clamp(aLifeSeed.x, 0.0, 1.0);
}
)GLSL";

static const char* kFireParticleFrag = R"GLSL(
#version 450 core
in vec2 vUV;
in float vLife;
out vec4 FragColor;
uniform sampler2D uFireTex;
void main(){
    vec4 texSample = texture(uFireTex, vUV);
    if(texSample.a < 0.05) discard;
    float life = clamp(vLife, 0.0, 1.0);
    vec3 startCol = vec3(1.0, 0.98, 0.9);
    vec3 midCol = vec3(1.0, 0.82, 0.35);
    vec3 endCol = vec3(1.0, 0.38, 0.05);
    vec3 color = mix(startCol, midCol, life);
    color = mix(color, endCol, life * life);
    float alpha = texSample.a * (1.0 - life);
    FragColor = vec4(color * texSample.rgb, alpha);
}
)GLSL";

static const char* kStickFlameVert = R"GLSL(
#version 450 core
layout(location=0) in vec2 aCorner;
layout(location=1) in vec2 aUV;
uniform mat4 uViewProj;
uniform vec3 uWorldPos;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uSize;
out vec2 vUV;
void main(){
    vec3 offset = (uCameraRight * aCorner.x + uCameraUp * aCorner.y) * uSize;
    vec3 worldPos = uWorldPos + offset;
    gl_Position = uViewProj * vec4(worldPos, 1.0);
    vUV = aUV;
}
)GLSL";

static const char* kStickFlameFrag = R"GLSL(
#version 450 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uFlameTex;
uniform float uGlow;
uniform vec3 uTint;
uniform float uOpacity;
void main(){
    vec4 texSample = texture(uFlameTex, vUV);
    if(texSample.a < 0.05) discard;
    float glowFactor = mix(0.65, 1.35, clamp(uGlow, 0.0, 1.0));
    vec3 color = texSample.rgb * (uTint * glowFactor);
    FragColor = vec4(color, texSample.a * uOpacity);
}
)GLSL";

// Simple depth shader (renders scene depth from light's POV)
static const char* kDepthVertex = R"GLSL(
#version 450 core
layout(location=0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uLightSpace;
void main(){
    gl_Position = uLightSpace * uModel * vec4(aPos,1.0);
}
)GLSL";

static const char* kDepthFragment = R"GLSL(
#version 450 core
void main(){
    // Empty: depth is automatically written
}
)GLSL";

// Skinned depth shader for character shadows
static const char* kSkinnedDepthVertex = R"GLSL(
#version 450 core
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in uvec4 inBoneIDs;
layout(location=4) in vec4 inWeights;

layout(std140, binding=0) uniform Bones {
    mat4 uBones[128];
};

uniform mat4 uModel;
uniform mat4 uLightSpace;

void main(){
    mat4 skinMat = mat4(0.0);
    for(int i=0; i<4; ++i){
        uint id = inBoneIDs[i];
        float w = inWeights[i];
        if(w > 0.0){
            skinMat += uBones[id] * w;
        }
    }
    vec4 skinnedPos = skinMat * vec4(inPos, 1.0);
    gl_Position = uLightSpace * uModel * skinnedPos;
}
)GLSL";

static const char* kSkinnedDepthFragment = R"GLSL(
#version 450 core
void main(){
    // Empty: depth is automatically written
}
)GLSL";

bool Game::init(){
    std::cout << "[Game] Init" << std::endl;
    m_bonePalette.fill(glm::mat4(1.0f));
    m_renderer = new Renderer();
    if(!m_renderer->init()) return false;
    m_shader = new Shader();
    if(!m_shader->compile(kVertex, kFragment)) return false;
    m_camera = new Camera();
    m_camera->setViewport(g_windowPtr->width(), g_windowPtr->height());
    m_freeCamera = new Camera();
    m_freeCamera->setViewport(g_windowPtr->width(), g_windowPtr->height());
    m_sky = new Sky();
    if(!m_sky->init()) return false;
    m_terrain = new Terrain();
    // Rectangular terrain: widthQuads, widthWorldScale (length=2*width)
        m_terrain->generate(256, 384.0f);  // Full size
    setActiveTerrain(m_terrain);
    float startH = m_terrain->getHeight(0.0f, 0.0f);
        // Start camera near lighthouse position for easy viewing
        glm::vec3 camStart(50.0f, startH + 40.0f, -50.0f);  // Closer to lighthouse at (50, Y, -100)
        m_camera->setPosition(camStart);
        m_freeCamera->setPosition(camStart);
    m_camera->setPitch(-25.0f); // Look slightly down to see lighthouse
    m_freeCamera->setPitch(-25.0f);
    const float lighthouseViewYaw = 180.0f; // Face toward negative Z (north beach)
    m_camera->setYaw(lighthouseViewYaw);
    m_freeCamera->setYaw(lighthouseViewYaw);
    // Force initial downward pitch
    // (Access private pitch via a small lambda hack if needed or adjust Camera to expose setter; omitted here for brevity)

    m_waterShader = new Shader();
    if(!m_waterShader->compile(kWaterVertex, kWaterFragment)) return false;
    // Grass uses a geometry stage to spawn quads from a single point, so compile with the extra step
    m_grassShader = new Shader();
    if(!m_grassShader->compileWithGeometry(kGrassVertex, kGrassGeometry, kGrassFragment)) return false;
    m_water = new Water();
    m_waterLevel = 10.0f;
    m_water->generate(384, 384.0f, m_waterLevel); // Higher resolution for better waves
    
    // Initialize terrain region definitions
    initTerrainRegions();
    
    // Create a simple procedural grass texture (tiles) so the terrain has detailed look
    // 512x512 RGB texture with subtle noise
    const int gw = 512, gh = 512;
    std::vector<unsigned char> grassData(gw * gh * 3);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> noise(-28, 28);
    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            int i = (y * gw + x) * 3;
            // darker base green for less glowing, more natural look
            int g = 110 + noise(rng);
            int r = 40 + noise(rng) / 4;
            int b = 30 + noise(rng) / 4;
            if (((x * 31 + y * 17) & 31) == 0) {
                g = std::max(0, g - 36);
                r = std::max(0, r - 12);
            }
            grassData[i + 0] = (unsigned char)std::clamp(r, 0, 255);
            grassData[i + 1] = (unsigned char)std::clamp(g, 0, 255);
            grassData[i + 2] = (unsigned char)std::clamp(b, 0, 255);
        }
    }
    glGenTextures(1, &m_grassTexture);
    glBindTexture(GL_TEXTURE_2D, m_grassTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gw, gh, 0, GL_RGB, GL_UNSIGNED_BYTE, grassData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Load biome textures (fungus, sandgrass, rocks). If files are missing, create 1x1 fallback color textures.
    auto createSolidTexture = [](unsigned char r, unsigned char g, unsigned char b)->GLuint{
        GLuint t=0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
        unsigned char p[3] = { r, g, b };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, p);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return t;
    };

#if defined(STB_IMAGE_IMPLEMENTATION)
    auto tryLoad = [&](const char* relPath, int desiredChannels, GLenum format)->GLuint{
        std::vector<std::string> candidates;
        candidates.emplace_back(relPath);
        candidates.emplace_back(std::string("../") + relPath);
        candidates.emplace_back(std::string("../../") + relPath);
        for(const std::string& candidate : candidates){
            const char* path = candidate.c_str();
            int w=0,h=0,channels=0;
            unsigned char* data = stbi_load(path, &w, &h, &channels, desiredChannels);
            if(!data){
                std::cout << "[Game] tryLoad failed for: " << path << std::endl;
                continue;
            }
            std::cout << "[Game] Loaded texture: " << path << " (" << w << "x" << h << ")" << std::endl;
            GLuint tex=0; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
            GLint internalFormat = (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            return tex;
        }
        return 0u;
    };
#else
    auto tryLoad = [&](const char* path, int, GLenum)->GLuint{ return 0u; };
#endif

    m_texFungus = tryLoad("assets/textures/fungus.png", 3, GL_RGB);
    if(m_texFungus == 0) {
        std::cout << "[Game] Using fallback fungus color texture" << std::endl;
        m_texFungus = createSolidTexture(80, 120, 60);
    }
    m_texSandgrass = tryLoad("assets/textures/sandgrass.png", 3, GL_RGB);
    if(m_texSandgrass == 0) {
        std::cout << "[Game] Using fallback sandgrass color texture" << std::endl;
        m_texSandgrass = createSolidTexture(180, 150, 90);
    }
    m_texRocks = tryLoad("assets/textures/rocks.png", 3, GL_RGB);
    if(m_texRocks == 0) {
        std::cout << "[Game] Using fallback rocks color texture" << std::endl;
        m_texRocks = createSolidTexture(120, 120, 120);
    }

    auto createGrassAtlasFallback = []()->GLuint{
        const int tileSize = 256;
        const int tilesPerAxis = 2;
        const int size = tileSize * tilesPerAxis;
        std::array<glm::vec3, 4> palette = {
            glm::vec3(0.25f, 0.5f, 0.18f),
            glm::vec3(0.3f, 0.55f, 0.2f),
            glm::vec3(0.22f, 0.45f, 0.16f),
            glm::vec3(0.28f, 0.52f, 0.19f)
        };
        std::vector<unsigned char> pixels(size * size * 4);
        for(int ty = 0; ty < tilesPerAxis; ++ty){
            for(int tx = 0; tx < tilesPerAxis; ++tx){
                glm::vec3 base = palette[ty * tilesPerAxis + tx];
                for(int y = 0; y < tileSize; ++y){
                    float v = static_cast<float>(y) / static_cast<float>(tileSize - 1);
                    float width = glm::mix(0.32f, 0.05f, v);
                    for(int x = 0; x < tileSize; ++x){
                        float u = static_cast<float>(x) / static_cast<float>(tileSize - 1);
                        float dist = std::abs(u - 0.5f);
                        float blade = 1.0f - glm::smoothstep(width, width + 0.06f, dist);
                        float tip = glm::smoothstep(0.0f, 1.0f, 1.0f - v);
                        float alpha = glm::clamp(blade * tip, 0.0f, 1.0f);
                        glm::vec3 color = glm::mix(base * 0.75f, base * 1.2f, tip);
                        int globalX = tx * tileSize + x;
                        int globalY = ty * tileSize + y;
                        int idx = (globalY * size + globalX) * 4;
                        pixels[idx + 0] = static_cast<unsigned char>(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
                        pixels[idx + 1] = static_cast<unsigned char>(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
                        pixels[idx + 2] = static_cast<unsigned char>(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);
                        pixels[idx + 3] = static_cast<unsigned char>(alpha * 255.0f);
                    }
                }
            }
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    m_grassBillboardTex = tryLoad("assets/textures/grass_atlas.png", 4, GL_RGBA);
    if(m_grassBillboardTex == 0){
        std::cout << "[Game] Using procedural fallback grass atlas" << std::endl;
        m_grassBillboardTex = createGrassAtlasFallback();
    }

    auto waveNoise = [](float u, float v, float freq) -> float {
        float a = std::sin((u + v) * freq * 6.28318f);
        float b = std::cos((u * 0.73f - v * 0.45f) * freq * 6.28318f);
        float c = std::sin((u * 1.37f + v * 1.11f) * freq * 3.14159f);
        return (a + b * 0.6f + c * 0.4f);
    };

    auto createWaveHeightTex = [&](int size, float freq, float amplitude) -> GLuint {
        std::vector<float> data(size * size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(size);
                float v = static_cast<float>(y) / static_cast<float>(size);
                float n = waveNoise(u, v, freq);
                float val = 0.5f + 0.5f * glm::clamp(n * 0.5f, -1.0f, 1.0f);
                data[y * size + x] = glm::clamp(val * amplitude, 0.0f, 1.0f);
            }
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, size, size, 0, GL_RED, GL_FLOAT, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    auto createWaveNormalTex = [&](int size, float freq, float slopeScale) -> GLuint {
        std::vector<float> data(size * size * 3);
        float eps = 1.0f / static_cast<float>(size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(size);
                float v = static_cast<float>(y) / static_cast<float>(size);
                float hL = waveNoise(u - eps, v, freq);
                float hR = waveNoise(u + eps, v, freq);
                float hD = waveNoise(u, v - eps, freq);
                float hU = waveNoise(u, v + eps, freq);
                float dx = (hR - hL) * slopeScale;
                float dz = (hU - hD) * slopeScale;
                glm::vec3 n = glm::normalize(glm::vec3(-dx, 1.0f, -dz));
                int idx = (y * size + x) * 3;
                data[idx + 0] = n.x * 0.5f + 0.5f;
                data[idx + 1] = n.y * 0.5f + 0.5f;
                data[idx + 2] = n.z * 0.5f + 0.5f;
            }
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    auto createEnvCubemap = [&]() -> GLuint {
        const int size = 16;
        std::array<glm::vec3, 6> faceColors = {
            glm::vec3(0.45f, 0.65f, 0.88f),
            glm::vec3(0.40f, 0.60f, 0.84f),
            glm::vec3(0.52f, 0.74f, 0.92f),
            glm::vec3(0.35f, 0.55f, 0.78f),
            glm::vec3(0.50f, 0.70f, 0.90f),
            glm::vec3(0.38f, 0.58f, 0.82f)
        };
        std::vector<float> data(size * size * 3);
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < size; ++y) {
                float v = static_cast<float>(y) / static_cast<float>(size - 1);
                glm::vec3 rowColor = glm::mix(faceColors[face], glm::vec3(0.9f), v * 0.4f);
                for (int x = 0; x < size; ++x) {
                    int idx = (y * size + x) * 3;
                    data[idx + 0] = rowColor.r;
                    data[idx + 1] = rowColor.g;
                    data[idx + 2] = rowColor.b;
                }
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, data.data());
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return tex;
    };

    m_waveHeightTex[0] = createWaveHeightTex(256, 3.5f, 0.9f);
    m_waveHeightTex[1] = createWaveHeightTex(256, 6.5f, 0.7f);
    m_waveNormalTex[0] = createWaveNormalTex(256, 3.5f, 0.8f);
    m_waveNormalTex[1] = createWaveNormalTex(256, 6.5f, 1.2f);
    m_envCubemap = createEnvCubemap();

    const glm::vec2 campfireClearingCenter(140.0f, 83.0f);
    const float campfireClearingRadius = 14.0f;
    const float campfireClearingRadiusSq = campfireClearingRadius * campfireClearingRadius;
    const glm::vec2 forestHutClearingCenter(126.0f, 100.0f);
    const float forestHutClearingRadius = 12.0f;
    const float forestHutClearingRadiusSq = forestHutClearingRadius * forestHutClearingRadius;

    // Walk the terrain grid and emit grass patches wherever we are far enough above water
    auto generateGrassInstances = [&](){
        std::vector<glm::vec4> instances;
        if(!m_terrain) return instances;
        float half = m_terrain->worldSize() * 0.5f;
        float heightScale = m_terrain->recommendedHeightScale();
        std::mt19937 rng(94731);
        // Much tighter spacing for denser grass coverage
        std::uniform_real_distribution<float> spacing(0.4f, 0.8f);  // 2x more patches
        std::uniform_real_distribution<float> jitter(-0.2f, 0.2f);
        std::uniform_real_distribution<float> seedDist(0.0f, 2048.0f);
        float x = -half;
        while(x < half){
            float z = half;
            while(z > -half){
                float worldX = glm::clamp(x + jitter(rng), -half + 0.001f, half - 0.001f);
                float worldZ = glm::clamp(z + jitter(rng), -half + 0.001f, half - 0.001f);
                float worldY = m_terrain->getHeight(worldX, worldZ);
                // Skip grass if terrain is still within the shoreline buffer (prevents soggy fringe)
                if(worldY < m_waterLevel + m_grassWaterGap){
                    z -= spacing(rng);
                    continue;
                }
                glm::vec2 horizontal(worldX, worldZ);
                if(glm::length2(horizontal - campfireClearingCenter) < campfireClearingRadiusSq){
                    z -= spacing(rng);
                    continue;
                }
                if(glm::length2(horizontal - forestHutClearingCenter) < forestHutClearingRadiusSq){
                    z -= spacing(rng);
                    continue;
                }
                glm::vec4 entry(worldX, worldY + 0.08f, worldZ, seedDist(rng));
                instances.push_back(entry);
                z -= spacing(rng);
            }
            x += spacing(rng);
        }
        return instances;
    };

    std::vector<glm::vec4> grassInstances = generateGrassInstances();
    if(!grassInstances.empty()){
        glGenVertexArrays(1, &m_grassVAO);
        glGenBuffers(1, &m_grassVBO);
        glBindVertexArray(m_grassVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_grassVBO);
        glBufferData(GL_ARRAY_BUFFER, grassInstances.size() * sizeof(glm::vec4), grassInstances.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), reinterpret_cast<void*>(sizeof(float) * 3));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_grassPatchCount = static_cast<int>(grassInstances.size());
        std::cout << "[Game] Generated " << m_grassPatchCount << " grass patches" << std::endl;
    } else {
        std::cout << "[Game] Skipped grass generation (no valid terrain above waterline)" << std::endl;
    }

    // Character pipeline: shader, mesh, animator, camera hookup
    std::string skinnedVS = loadTextFile({
        "assets/shaders/skinned.vert",
        "../assets/shaders/skinned.vert",
        "../../assets/shaders/skinned.vert"
    });
    std::string skinnedFS = loadTextFile({
        "assets/shaders/skinned.frag",
        "../assets/shaders/skinned.frag",
        "../../assets/shaders/skinned.frag"
    });
    if(skinnedVS.empty() || skinnedFS.empty()){
        std::cerr << "[Game] Failed to read skinned shader sources" << std::endl;
        return false;
    }
    m_characterShader = new Shader();
    if(!m_characterShader->compile(skinnedVS, skinnedFS)){
        std::cerr << "[Game] Failed to compile skinned shader" << std::endl;
        return false;
    }

    if(m_boneUBO == 0){
        glGenBuffers(1, &m_boneUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, m_boneUBO);
        glBufferData(GL_UNIFORM_BUFFER, m_bonePalette.size() * sizeof(glm::mat4), m_bonePalette.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_boneUBO);
    }

    std::string glbPath = resolveExistingPath({
        "assets/models/sponge.glb",
        "../assets/models/sponge.glb",
        "../../assets/models/sponge.glb"
    });
    if(glbPath.empty()){
        std::cerr << "[Game] Could not locate assets/models/sponge.glb" << std::endl;
    } else {
        try {
            CharacterImporter importer;
            m_characterMesh = importer.load(glbPath);
            m_animator = std::make_unique<Animator>(m_characterMesh);
            m_characterReady = (m_characterMesh.vao != 0);
        } catch(const std::exception& e){
            std::cerr << e.what() << std::endl;
            m_characterReady = false;
        }
    }

    if(m_characterReady){
        m_characterScale = 0.025f;  // Reduced by half (was 0.05f)
        glm::vec2 nearCampfireXZ(136.0f, 78.0f);
        glm::vec3 spawn(nearCampfireXZ.x,
                getTerrainHeightAt(nearCampfireXZ.x, nearCampfireXZ.y),
                nearCampfireXZ.y);
        m_characterController.position = spawn;
        glm::vec2 toCampfire = campfireClearingCenter - nearCampfireXZ;
        if(glm::length2(toCampfire) > 1e-4f){
            m_characterController.yaw = std::atan2(toCampfire.x, toCampfire.y);
        }
        m_characterHeight = glm::max(0.001f, m_characterMesh.maxBounds.y - m_characterMesh.minBounds.y);
        m_characterFeetOffset = -m_characterMesh.minBounds.y;
        m_characterController.position.y = spawn.y - m_characterFeetOffset * m_characterScale;
        // Camera positioned well above and behind character's head
        float pivotHeight = m_characterHeight * m_characterScale * 1.5f;  // Higher pivot
        m_characterAimPoint = m_characterController.position + glm::vec3(0.0f, pivotHeight, 0.0f);
        float verticalOffset = m_characterHeight * m_characterScale * 1.4f;  // Much higher
        float followDistance = 13.0f;  // Further back
        m_thirdPersonCamera.setTarget(&m_characterAimPoint);
        m_thirdPersonCamera.setFollowConfig(pivotHeight, verticalOffset, followDistance);
        m_thirdPersonCamera.update(0.0, 0.0f, 0.0f, getTerrainHeightAt);
        
        // Setup character collision (capsule)
        CollisionBody characterBody;
        characterBody.shape = CollisionShape::Capsule;
        characterBody.position = m_characterController.position;
        characterBody.radius = m_characterScale * m_characterHeight * 0.3f;  // Roughly body width
        characterBody.height = m_characterHeight * m_characterScale;
        characterBody.isStatic = false;
        characterBody.userData = this;
        m_characterCollisionBody = m_collisionSystem.addBody(characterBody);
        
        if(m_characterMesh.albedoTex != 0){
            m_characterAlbedoTex = m_characterMesh.albedoTex;
            m_characterMesh.albedoTex = 0;
        } else {
            m_characterAlbedoTex = tryLoad("assets/textures/character_albedo.png", 4, GL_RGBA);
            if(m_characterAlbedoTex == 0){
                m_characterAlbedoTex = createSolidTexture(180, 150, 135);
            }
        }

    } else {
        m_useThirdPersonCamera = false;
    }

    // Load lighthouse model
    std::string lighthousePath = resolveExistingPath({
        "assets/models/light.glb",
        "../assets/models/light.glb",
        "../../assets/models/light.glb"
    });
    if(!lighthousePath.empty()){
        m_lighthouseReady = loadStaticModel(lighthousePath, m_lighthouseMesh);
        if(m_lighthouseReady){
            // Position at beach/grassland border (north side)
            float beachX = 50.0f;
            float beachZ = -100.0f;
            float terrainY = m_terrain->getHeight(beachX, beachZ);
            
            m_lighthouseScale = 10.0f;  // Scale up significantly (lighthouse is tall structure)
            
            // Calculate feet offset from actual bounding box (like character placement)
            // The model's local origin may not be at its base
            float feetOffset = -m_lighthouseMesh.minBounds.y;  // Distance from origin to lowest point
            float modelHeight = m_lighthouseMesh.maxBounds.y - m_lighthouseMesh.minBounds.y;
            
            // Position so base sits on terrain (subtract feet offset to place lowest point at terrain level)
            m_lighthousePosition = glm::vec3(
                beachX, 
                terrainY + feetOffset * m_lighthouseScale,  // Lift by feet offset
                beachZ
            );
            m_lighthouseBeaconLocal = m_lighthouseMesh.maxBounds;
            m_beaconLight.color = glm::vec3(1.0f);
            m_beaconLight.range = 220.0f;
            m_beaconLight.intensity = 4.0f;
            float inner = glm::radians(7.0f);
            float outer = glm::radians(10.5f);
            m_beaconLight.innerCutoffCos = std::cos(inner);
            m_beaconLight.outerCutoffCos = std::cos(outer);
            m_beaconLight.enabled = false;
            
            std::cout << "[Game] Lighthouse loaded and placed at (" 
                      << beachX << ", " << terrainY << " (terrain)" << ", " << beachZ << ")" << std::endl;
            std::cout << "[Game] Lighthouse bounds: Y [" << m_lighthouseMesh.minBounds.y 
                      << " to " << m_lighthouseMesh.maxBounds.y << "], feet offset: " << feetOffset << std::endl;
            std::cout << "[Game] Lighthouse scale: " << m_lighthouseScale 
                      << ", scaled height: " << (modelHeight * m_lighthouseScale) << " units"
                      << ", vertices: " << m_lighthouseMesh.totalVertexCount << std::endl;
        }
    } else {
        std::cerr << "[Game] Could not locate assets/models/light.glb" << std::endl;
    }

    // Load tree model
    std::string treePath = resolveExistingPath({
        "assets/models/tree1.glb",
        "../assets/models/tree1.glb",
        "../../assets/models/tree1.glb"
    });
    if(!treePath.empty()){
        m_treeReady = loadStaticModel(treePath, m_treeMesh);
        if(m_treeReady){
            // Replace missing material with explicit diffuse texture so bark/leaves render correctly
            if(GLuint treeTex = tryLoad("assets/textures/tree1_diffuse.png", 4, GL_RGBA)){
                if(!m_treeMesh.parts.empty()){
                    auto& part = m_treeMesh.parts.front();
                    if(part.albedoTex){
                        glDeleteTextures(1, &part.albedoTex);
                        part.albedoTex = 0;
                    }
                    part.albedoTex = treeTex;
                }
            }

            const float baseScale = 5.0f;
            const std::array<float, 3> scaleMultipliers = {1.10f, 1.14f, 1.19f};
            const float feetOffset = -m_treeMesh.minBounds.y;
            const float modelHeight = m_treeMesh.maxBounds.y - m_treeMesh.minBounds.y;

            auto computeTreePosition = [&](float x, float z, float scale){
                float terrainY = m_terrain->getHeight(x, z);
                return glm::vec3(x, terrainY + feetOffset * scale, z);
            };

            auto findRegion = [&](const std::string& name)->const TerrainRegion*{
                auto it = std::find_if(m_terrainRegions.begin(), m_terrainRegions.end(), [&](const TerrainRegion& region){
                    return region.name == name;
                });
                return it != m_terrainRegions.end() ? &(*it) : nullptr;
            };

            auto emitTreesInRegion = [&](const std::string& regionName, int desiredCount, std::mt19937& rng){
                const TerrainRegion* region = findRegion(regionName);
                if(!region || desiredCount <= 0) return;
                std::uniform_real_distribution<float> distX(region->minXZ.x, region->maxXZ.x);
                std::uniform_real_distribution<float> distZ(region->minXZ.y, region->maxXZ.y);
                std::uniform_int_distribution<int> pickScale(0, static_cast<int>(scaleMultipliers.size()) - 1);
                int attempts = 0;
                const int maxAttempts = desiredCount * 32;
                while(desiredCount > 0 && attempts < maxAttempts){
                    float x = distX(rng);
                    float z = distZ(rng);
                    float terrainY = m_terrain->getHeight(x, z);
                    if(terrainY < region->minY || terrainY > region->maxY){
                        ++attempts;
                        continue;
                    }
                    if(terrainY < m_waterLevel + m_grassWaterGap){
                        ++attempts;
                        continue;
                    }
                    float scale = baseScale * scaleMultipliers[pickScale(rng)];
                    m_treeInstances.push_back({computeTreePosition(x, z, scale), scale});
                    --desiredCount;
                    ++attempts;
                }
                if(desiredCount > 0){
                    std::cout << "[Game] Tree placement skipped " << desiredCount
                              << " slots in region " << regionName << " (terrain constraints)" << std::endl;
                }
            };

            m_treeInstances.clear();
            m_treeInstances.reserve(80);

            // Anchor tree in the southern center of the grass valley for easy visual reference
            float anchorX = 0.0f;
            float anchorZ = 128.0f;  // Midpoint of grassland_south strip (64..192)
            glm::vec3 anchorPosition = computeTreePosition(anchorX, anchorZ, baseScale);
            m_treeInstances.push_back({anchorPosition, baseScale});
            float anchorTerrainY = m_terrain->getHeight(anchorX, anchorZ);

            std::mt19937 treeRng(860321);
            emitTreesInRegion("grassland_south", 35, treeRng);
            emitTreesInRegion("grassland_center", 28, treeRng);
            emitTreesInRegion("grassland_north", 12, treeRng);

            if(m_treeInstances.empty()){
                std::cerr << "[Game] No valid placement found for tree instances" << std::endl;
                m_treeReady = false;
            } else {
                std::cout << "[Game] Tree bounds: Y [" << m_treeMesh.minBounds.y
                          << " to " << m_treeMesh.maxBounds.y << "], feet offset: " << feetOffset << std::endl;
                std::cout << "[Game] Tree base scale: " << baseScale
                          << ", scaled height: " << (modelHeight * baseScale) << " units"
                          << ", vertices: " << m_treeMesh.totalVertexCount << std::endl;
                std::cout << "[Game] Anchor tree placed at (" << anchorX << ", " << anchorTerrainY
                          << " (terrain), " << anchorZ << ")" << std::endl;
                std::cout << "[Game] Spawned " << (m_treeInstances.size() - 1)
                          << " additional tree instances across grassland regions" << std::endl;
            }
        }
    } else {
        std::cerr << "[Game] Could not locate assets/models/tree1.glb" << std::endl;
    }

    // Load campfire model
    std::string campfirePath = resolveExistingPath({
        "assets/models/campfire.glb",
        "../assets/models/campfire.glb",
        "../../assets/models/campfire.glb"
    });
    if(!campfirePath.empty()){
        m_campfireReady = loadStaticModel(campfirePath, m_campfireMesh);
        if(m_campfireReady){
            m_campfireScale = 5.0f;
            float campfireX = 140.0f;
            float campfireZ = 83.0f;
            float terrainY = m_terrain->getHeight(campfireX, campfireZ);
            float feetOffset = -m_campfireMesh.minBounds.y;
            float modelHeight = m_campfireMesh.maxBounds.y - m_campfireMesh.minBounds.y;
            m_campfirePosition = glm::vec3(
                campfireX,
                terrainY + feetOffset * m_campfireScale,
                campfireZ
            );
            m_campfireEmitterPos = m_campfirePosition + glm::vec3(0.0f, 0.5f * m_campfireScale, 0.0f);
            setupCampfireLight();
            if(m_fireTexture == 0){
                m_fireTexture = tryLoad("assets/textures/fire1.png", 4, GL_RGBA);
                if(m_fireTexture == 0){
                    std::cout << "[Game] Using fallback fire texture" << std::endl;
                    m_fireTexture = createSolidTexture(255, 170, 80);
                }
            }
            initCampfireFireFX();
            std::cout << "[Game] Campfire loaded and placed at ("
                      << campfireX << ", " << terrainY << " (terrain), " << campfireZ << ")" << std::endl;
            std::cout << "[Game] Campfire bounds: Y [" << m_campfireMesh.minBounds.y
                      << " to " << m_campfireMesh.maxBounds.y << "], feet offset: " << feetOffset << std::endl;
            std::cout << "[Game] Campfire scale: " << m_campfireScale
                      << ", scaled height: " << (modelHeight * m_campfireScale) << " units"
                      << ", vertices: " << m_campfireMesh.totalVertexCount << std::endl;
        }
    } else {
        std::cerr << "[Game] Could not locate assets/models/campfire.glb" << std::endl;
        m_campfireLight.enabled = false;
    }

    if(m_beaconDiscTexture == 0){
        m_beaconDiscTexture = createBeaconDiscTexture(192);
        if(m_beaconDiscTexture == 0){
            std::cerr << "[Game] Failed to create beacon disc texture" << std::endl;
        }
    }

    // Load stick model for world interaction
    std::string stickPath = resolveExistingPath({
        "assets/models/stick.glb",
        "../assets/models/stick.glb",
        "../../assets/models/stick.glb"
    });
    if(!stickPath.empty()){
        m_stickReady = loadStaticModel(stickPath, m_stickMesh);
        if(m_stickReady){
            m_stickItem.scale = 0.08f;  // Double previous size for better visibility
            m_stickItem.colliderRadius = 1.0f;
            m_stickBaseHeight = m_stickMesh.minBounds.y;
            m_stickTipLength = m_stickMesh.maxBounds.y - m_stickBaseHeight;
            m_stickGroundRotation = glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_stickItem.rotation = m_stickGroundRotation;
            glm::vec3 dropSpot = m_campfireReady
                ? (m_campfirePosition + glm::vec3(3.5f, 0.0f, -2.8f))
                : glm::vec3(2.0f, 0.0f, 2.0f);
            float baseOffset = m_stickBaseHeight * m_stickItem.scale;
            dropSpot.y = getTerrainHeightAt(dropSpot.x, dropSpot.z) + m_stickHoverOffset - baseOffset;
            m_stickItem.position = dropSpot;
            m_stickItem.isHeld = false;
            m_stickItem.collisionEnabled = true;
            m_stickLight.enabled = false;
            m_stickLight.baseIntensity = 1.35f;
            m_stickLight.intensity = 0.0f;
            m_stickLight.radius = 18.0f;
            m_stickLight.color = glm::vec3(1.0f, 0.58f, 0.2f);
            m_stickLight.flickerTimer = 0.0f;
            refreshStickWorldMatrix();
            std::cout << "[Game] Stick loaded from " << stickPath
                      << " and placed near (" << dropSpot.x << ", " << dropSpot.y
                      << ", " << dropSpot.z << ")" << std::endl;
        }
    } else {
        std::cerr << "[Game] Could not locate assets/models/stick.glb" << std::endl;
    }

    // Load forest hut model
    std::string forestHutPath = resolveExistingPath({
        "assets/models/forest_hut.glb",
        "../assets/models/forest_hut.glb",
        "../../assets/models/forest_hut.glb"
    });
    if(!forestHutPath.empty()){
        m_forestHutReady = loadStaticModel(forestHutPath, m_forestHutMesh);
        if(m_forestHutReady){
            m_forestHutScale = 3.4f;
            m_forestHutPitchDegrees = -90.0f;  // Convert Blender's Z-up export to engine's Y-up
            float hutX = forestHutClearingCenter.x;
            float hutZ = forestHutClearingCenter.y;
            float terrainY = m_terrain->getHeight(hutX, hutZ);
            float rotatedFeetOffset = 0.0f;
            {
                glm::vec3 minB = m_forestHutMesh.minBounds;
                glm::vec3 maxB = m_forestHutMesh.maxBounds;
                float minY = FLT_MAX;
                glm::mat4 pitchMat = glm::rotate(glm::mat4(1.0f), glm::radians(m_forestHutPitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
                for(int ix = 0; ix < 2; ++ix){
                    float x = ix == 0 ? minB.x : maxB.x;
                    for(int iy = 0; iy < 2; ++iy){
                        float y = iy == 0 ? minB.y : maxB.y;
                        for(int iz = 0; iz < 2; ++iz){
                            float z = iz == 0 ? minB.z : maxB.z;
                            glm::vec4 corner = pitchMat * glm::vec4(x, y, z, 1.0f);
                            minY = std::min(minY, corner.y);
                        }
                    }
                }
                rotatedFeetOffset = -minY;
            }
            float modelHeight = m_forestHutMesh.maxBounds.y - m_forestHutMesh.minBounds.y;
            m_forestHutPosition = glm::vec3(
                hutX,
                terrainY + rotatedFeetOffset * m_forestHutScale,
                hutZ
            );

            glm::vec2 toCampfire(0.0f);
            if(m_campfireReady){
                toCampfire = glm::vec2(m_campfirePosition.x - hutX, m_campfirePosition.z - hutZ);
            }
            if(glm::length(toCampfire) > 0.0001f){
                float yawRadians = std::atan2(toCampfire.x, toCampfire.y);
                m_forestHutYawDegrees = glm::degrees(yawRadians);
            } else {
                m_forestHutYawDegrees = 0.0f;
            }

            std::cout << "[Game] Forest hut loaded from " << forestHutPath << std::endl;
            std::cout << "[Game] Forest hut bounds: Y [" << m_forestHutMesh.minBounds.y
                      << " to " << m_forestHutMesh.maxBounds.y << "], feet offset (rotated): " << rotatedFeetOffset << std::endl;
            std::cout << "[Game] Forest hut scale: " << m_forestHutScale
                      << ", scaled height: " << (modelHeight * m_forestHutScale) << " units"
                      << ", vertices: " << m_forestHutMesh.totalVertexCount << std::endl;
            std::cout << "[Game] Forest hut placed at (" << hutX << ", " << terrainY
                      << " (terrain), " << hutZ << ") yaw " << m_forestHutYawDegrees
                      << " degrees to face the campfire" << std::endl;
            std::cout << "[Game] Forest hut pitch correction: " << m_forestHutPitchDegrees
                      << " degrees (Z-up -> Y-up)" << std::endl;
        }
    } else {
        std::cerr << "[Game] Could not locate assets/models/forest_hut.glb" << std::endl;
    }

    // Enhanced sun lighting for professional outdoor look
    // Sun angle: 45° elevation for natural midday lighting
    m_light.direction = glm::normalize(glm::vec3(0.5f, -0.7f, -0.3f));
    m_light.color = glm::vec3(1.0f, 0.98f, 0.92f);  // Warm sunlight
    m_light.ambient = 0.2625f;  // 25% darker ambient for outdoor scene (was 0.35)
    m_light.specularStrength = 0.15f;  // Visible but not overwhelming
    m_light.shininess = 32.0f;  // Natural specular highlights

#ifdef BUILD_IMGUI
    // Setup ImGui context and backends
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_windowPtr->nativeHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 450 core");
#endif

    // Setup shadow map FBO + texture
    m_depthShader = new Shader();
    if(!m_depthShader->compile(kDepthVertex, kDepthFragment)){
        std::cerr << "[Game] Failed to compile depth shader" << std::endl;
        return false;
    }
    m_skinnedDepthShader = new Shader();
    if(!m_skinnedDepthShader->compile(kSkinnedDepthVertex, kSkinnedDepthFragment)){
        std::cerr << "[Game] Failed to compile skinned depth shader" << std::endl;
        return false;
    }
    glGenFramebuffers(1, &m_shadowFBO);
    glGenTextures(1, &m_shadowTex);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_shadowMapSize, m_shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    // Linear filtering for softer shadow edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[4] = {1.0f,1.0f,1.0f,1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        std::cerr << "[Game] Shadow FBO not complete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Game::update(){
    double dt = Time::delta();
    GLFWwindow* window = g_windowPtr ? g_windowPtr->nativeHandle() : nullptr;

    auto uploadBones = [&](){
        if(!m_animator) return;
        std::fill(m_bonePalette.begin(), m_bonePalette.end(), glm::mat4(1.0f));
        const auto& matrices = m_animator->boneMatrices();
        size_t count = std::min(matrices.size(), m_bonePalette.size());
        for(size_t i=0; i<count; ++i){
            m_bonePalette[i] = matrices[i];
        }
        if(m_boneUBO != 0){
            glBindBuffer(GL_UNIFORM_BUFFER, m_boneUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, m_bonePalette.size() * sizeof(glm::mat4), m_bonePalette.data());
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
    };

    auto updateCharacterPlacement = [&](){
        if(!m_characterReady) return;
        float terrainY = getTerrainHeightAt(m_characterController.position.x, m_characterController.position.z);
        m_characterController.position.y = terrainY - m_characterFeetOffset * m_characterScale;
        float pivotHeight = m_characterHeight * m_characterScale * 1.5f;
        float verticalOffset = m_characterHeight * m_characterScale * 1.4f;
        float followDistance = 13.0f;
        m_thirdPersonCamera.setFollowConfig(pivotHeight, verticalOffset, followDistance);
        m_characterAimPoint = m_characterController.position + glm::vec3(0.0f, pivotHeight, 0.0f);
        glm::quat orientation = glm::angleAxis(m_characterController.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(m_characterScale));
        m_characterModelMatrix = glm::translate(glm::mat4(1.0f), m_characterController.position) *
                                 glm::mat4_cast(orientation) * scale;
    };

    if(window){
        int state = glfwGetKey(window, GLFW_KEY_C);
        if(state == GLFW_PRESS && !m_cameraToggleHeld){
            m_useThirdPersonCamera = !m_useThirdPersonCamera && m_characterReady;
            m_cameraToggleHeld = true;
        } else if(state == GLFW_RELEASE){
            m_cameraToggleHeld = false;
        }
        
        // Toggle cursor capture with Escape key for UI interaction
        static bool escapeHeld = false;
        int escState = glfwGetKey(window, GLFW_KEY_ESCAPE);
        if(escState == GLFW_PRESS && !escapeHeld){
            bool currentCapture = g_windowPtr->isMouseCaptured();
            g_windowPtr->setMouseCaptured(!currentCapture);
            std::cout << "[Game] Cursor: " << (!currentCapture ? "CAPTURED (camera control)" : "FREE (UI interaction)") << std::endl;
            escapeHeld = true;
        } else if(escState == GLFW_RELEASE){
            escapeHeld = false;
        }
        
        // Toggle region display with R key
        int rState = glfwGetKey(window, GLFW_KEY_R);
        if(rState == GLFW_PRESS && !m_regionToggleHeld){
            m_showRegions = !m_showRegions;
            std::cout << "[Game] Region display: " << (m_showRegions ? "ON" : "OFF") << std::endl;
            m_regionToggleHeld = true;
        } else if(rState == GLFW_RELEASE){
            m_regionToggleHeld = false;
        }
        
        // Toggle day/night with T key
        int tState = glfwGetKey(window, GLFW_KEY_T);
        if(tState == GLFW_PRESS && !m_nightToggleHeld){
            m_isNightMode = !m_isNightMode;
            // Update lighting for night/day
            if(m_isNightMode){
                // Night: Moon lighting (soft blue-white, dimmer) - 62.5% darker
                m_light.direction = glm::normalize(glm::vec3(-0.3f, -0.8f, 0.5f));  // From moon position
                m_light.color = glm::vec3(0.05f, 0.06f, 0.075f);  // Softer moonlight intensity
                m_light.ambient = 0.0225f;  // 25% darker ambient (was 0.03)
                m_light.specularStrength = 0.015f;  // Strongly reduced reflections at night
            } else {
                // Day: Sun lighting (warm, bright)
                m_light.direction = glm::normalize(glm::vec3(0.5f, -0.7f, -0.3f));
                m_light.color = glm::vec3(1.0f, 0.98f, 0.92f);  // Warm sunlight
                m_light.ambient = 0.2625f;  // 25% darker ambient (was 0.35)
                m_light.specularStrength = 0.15f;
            }
            m_nightToggleHeld = true;
        } else if(tState == GLFW_RELEASE){
            m_nightToggleHeld = false;
        }
    }

    bool placementUpdated = false;
    if(m_characterReady && window){
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if(m_firstMouseSample){
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
            m_firstMouseSample = false;
        }
        float mouseDX = static_cast<float>(mouseX - m_lastMouseX);
        float mouseDY = static_cast<float>(m_lastMouseY - mouseY);
        m_lastMouseX = mouseX;
        m_lastMouseY = mouseY;

        // Only process camera movement if cursor is captured
        bool cursorCaptured = g_windowPtr->isMouseCaptured();
        if(m_useThirdPersonCamera && cursorCaptured){
            bool moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            
            // Get camera forward direction for character movement
            glm::vec3 cameraForward = m_thirdPersonCamera.predictForward(mouseDX, mouseDY);
            glm::vec2 planar(cameraForward.x, cameraForward.z);
            
            // Update character orientation to face camera direction when moving
            if(moveForward && glm::length2(planar) > 1e-4f){
                m_characterController.yaw = std::atan2(planar.x, planar.y);
            }
            
            // Compute movement direction (flatten camera forward to horizontal plane)
            glm::vec3 moveDir(0.0f);
            if(glm::length2(planar) > 1e-4f){
                glm::vec2 normalized = glm::normalize(planar);
                moveDir = glm::vec3(normalized.x, 0.0f, normalized.y);
            }
            
            // Store old position for collision resolution
            glm::vec3 oldPos = m_characterController.position;
            CharacterState desired = m_characterController.update(dt, moveForward, moveDir);
            
            // Apply collision resolution
            if(moveForward){
                glm::vec3 resolvedPos = m_collisionSystem.resolveMovement(
                    m_characterCollisionBody, oldPos, m_characterController.position);
                m_characterController.position = resolvedPos;
            }
            
            // Update collision body position
            m_collisionSystem.updateBodyPosition(m_characterCollisionBody, m_characterController.position);
            
            if(m_animator){
                m_animator->play(desired);
                // Scale animation speed to match movement speed
                // Run animation is designed for ~6 units/s
                if(desired == CharacterState::Run){
                    m_animator->setPlaybackSpeed(m_characterController.moveSpeed / 6.0f);
                } else {
                    m_animator->setPlaybackSpeed(1.0f);
                }
                m_animator->update(dt);
                uploadBones();
            }
            updateCharacterPlacement();
            placementUpdated = true;
            m_thirdPersonCamera.update(dt, mouseDX, mouseDY, [](float x, float z){
                return getTerrainHeightAt(x, z);
            });
            if(m_camera){
                glm::vec3 eye = m_thirdPersonCamera.position();
                glm::vec3 forward = m_thirdPersonCamera.forward();
                if(glm::length2(forward) < 1e-6f){
                    forward = glm::vec3(0.0f, 0.0f, -1.0f);
                }
                float pitch = glm::degrees(std::asin(glm::clamp(forward.y, -1.0f, 1.0f)));
                float yaw = glm::degrees(std::atan2(forward.z, forward.x));
                m_camera->setPosition(eye);
                m_camera->setPitch(pitch);
                m_camera->setYaw(yaw);
            }
        } else if(m_animator) {
            m_animator->play(CharacterState::Idle);
            m_animator->update(dt);
            uploadBones();
        }
    } else if(m_characterReady && m_animator){
        m_animator->play(CharacterState::Idle);
        m_animator->update(dt);
        uploadBones();
    }

    if(m_characterReady && !placementUpdated){
        updateCharacterPlacement();
    }

    if(!m_useThirdPersonCamera && m_freeCamera && window){
        m_freeCamera->update(static_cast<float>(dt), window);
        if(m_camera){
            m_camera->setPosition(m_freeCamera->position());
            m_camera->setPitch(m_freeCamera->pitch());
            m_camera->setYaw(m_freeCamera->yaw());
        }
    }

    updateFireParticles(static_cast<float>(dt));
    updateCampfireLight(static_cast<float>(dt));
    updateBeaconLight(static_cast<float>(dt));
    updateStickInteraction();

    if(m_camera){
        m_camera->setViewport(g_windowPtr->width(), g_windowPtr->height());
    }
    // (Debug modes removed in simplified pipeline.)
}

void Game::render(){
    // 1) Render depth map from light's perspective
    // Compute light projection/view
    float near_plane = 1.0f, far_plane = 1500.0f;
    float orthoSize = m_terrain->worldSize();
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane, far_plane);
    glm::vec3 lightPos = -m_light.direction * (m_terrain->worldSize());
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f,1.0f,0.0f));
    glm::mat4 lightSpace = lightProj * lightView;

    glm::mat4 characterModel = m_characterReady ? m_characterModelMatrix : glm::mat4(1.0f);
    
    // Sky color and fog based on time of day
    glm::vec3 skyColor = m_isNightMode ? glm::vec3(0.0075f, 0.01125f, 0.03f) : glm::vec3(0.53f, 0.81f, 0.92f);
    const float fogStart = 200.0f;
    const float fogRange = 1200.0f;

    auto uploadPointLights = [&](Shader* shader){
        if(!shader) return;
        std::array<const PointLight*, kMaxPointLights> active{};
        int count = 0;
        auto pushLight = [&](const PointLight& light){
            if(count >= kMaxPointLights) return;
            if(!light.enabled) return;
            active[count++] = &light;
        };
        if(m_campfireReady){
            pushLight(m_campfireLight);
        }
        if(m_stickLight.enabled && m_stickLit){
            pushLight(m_stickLight);
        }
        shader->setInt("uPointLightCount", count);
        for(int i = 0; i < count; ++i){
            const PointLight* light = active[i];
            std::string index = "[" + std::to_string(i) + "]";
            shader->setVec3(std::string("uPointLightPos") + index, light->position);
            shader->setVec3(std::string("uPointLightColor") + index, light->color);
            shader->setFloat(std::string("uPointLightIntensity") + index, light->intensity);
            shader->setFloat(std::string("uPointLightRadius") + index, light->radius);
        }
    };
    auto uploadSpotLight = [&](Shader* shader){
        if(!shader) return;
        shader->setBool("uSpotLightEnabled", m_beaconLight.enabled);
        if(m_beaconLight.enabled){
            shader->setVec3("uSpotLightPos", m_beaconLight.position);
            shader->setVec3("uSpotLightDir", m_beaconLight.direction);
            shader->setVec3("uSpotLightColor", m_beaconLight.color);
            shader->setFloat("uSpotLightIntensity", m_beaconLight.intensity);
            shader->setFloat("uSpotLightRange", m_beaconLight.range);
            shader->setFloat("uSpotLightInnerCutoff", m_beaconLight.innerCutoffCos);
            shader->setFloat("uSpotLightOuterCutoff", m_beaconLight.outerCutoffCos);
        }
    };

    glViewport(0,0,m_shadowMapSize,m_shadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Draw character to shadow map using skinned depth shader
    if(m_characterReady){
        m_skinnedDepthShader->bind();
        m_skinnedDepthShader->setMat4("uModel", characterModel);
        m_skinnedDepthShader->setMat4("uLightSpace", lightSpace);
        glBindVertexArray(m_characterMesh.vao);
        glDrawElements(GL_TRIANGLES, m_characterMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
    // Draw lighthouse to shadow map
    if(m_lighthouseReady && !m_lighthouseMesh.parts.empty()){
        glm::mat4 lighthouseModel = glm::mat4(1.0f);
        lighthouseModel = glm::translate(lighthouseModel, m_lighthousePosition);
        lighthouseModel = glm::scale(lighthouseModel, glm::vec3(m_lighthouseScale));

        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        for(const auto& part : m_lighthouseMesh.parts){
            m_depthShader->setMat4("uModel", lighthouseModel);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    // Draw trees to shadow map
    if(m_treeReady && !m_treeInstances.empty() && !m_treeMesh.parts.empty()){
        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        for(const TreeInstance& tree : m_treeInstances){
            glm::mat4 treeModel = glm::mat4(1.0f);
            treeModel = glm::translate(treeModel, tree.position);
            treeModel = glm::scale(treeModel, glm::vec3(tree.scale));
            m_depthShader->setMat4("uModel", treeModel);
            for(const auto& part : m_treeMesh.parts){
                glBindVertexArray(part.vao);
                glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        glBindVertexArray(0);
    }

    // Draw campfire to shadow map
    if(m_campfireReady && !m_campfireMesh.parts.empty()){
        glm::mat4 campfireModel = glm::mat4(1.0f);
        campfireModel = glm::translate(campfireModel, m_campfirePosition);
        campfireModel = glm::scale(campfireModel, glm::vec3(m_campfireScale));

        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        m_depthShader->setMat4("uModel", campfireModel);
        for(const auto& part : m_campfireMesh.parts){
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    // Draw stick (world item or held) to shadow map
    if(m_stickReady && !m_stickMesh.parts.empty()){
        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        m_depthShader->setMat4("uModel", m_stickItem.worldMatrix);
        for(const auto& part : m_stickMesh.parts){
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    // Draw forest hut to shadow map
    if(m_forestHutReady && !m_forestHutMesh.parts.empty()){
        glm::mat4 hutModel = glm::mat4(1.0f);
        hutModel = glm::translate(hutModel, m_forestHutPosition);
        hutModel = glm::rotate(hutModel, glm::radians(m_forestHutYawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        hutModel = glm::rotate(hutModel, glm::radians(m_forestHutPitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
        hutModel = glm::scale(hutModel, glm::vec3(m_forestHutScale));

        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        m_depthShader->setMat4("uModel", hutModel);
        for(const auto& part : m_forestHutMesh.parts){
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    // Draw stick to shadow map
    if(m_stickReady && !m_stickMesh.parts.empty()){
        m_depthShader->bind();
        m_depthShader->setMat4("uLightSpace", lightSpace);
        m_depthShader->setMat4("uModel", m_stickItem.worldMatrix);
        for(const auto& part : m_stickMesh.parts){
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }
    
    // Draw terrain to shadow map
    m_depthShader->bind();
    m_depthShader->setMat4("uLightSpace", lightSpace);
    glm::mat4 lightModel(1.0f);
    m_depthShader->setMat4("uModel", lightModel);
    m_renderer->drawMesh(*m_terrain->mesh(), *m_depthShader, *m_camera, lightModel);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // restore viewport
    m_renderer->beginFrame(g_windowPtr->width(), g_windowPtr->height());
    // Render sky first
    m_sky->render(*m_camera, -m_light.direction, m_isNightMode); // sunDir opposite light direction
    glDisable(GL_CULL_FACE); // Disable culling to show all faces of terrain
    glm::mat4 model(1.0f);
    m_shader->bind();
    m_shader->setVec3("uLightDir", m_light.direction);
    m_shader->setVec3("uLightColor", m_light.color);
    m_shader->setFloat("uAmbient", m_light.ambient);
    m_shader->setBool("uIsNight", m_isNightMode);
    m_shader->setFloat("uSpecularStrength", m_light.specularStrength);
    m_shader->setFloat("uShininess", m_light.shininess);
    m_shader->setVec3("uCameraPos", m_camera->position());
    uploadPointLights(m_shader);
    uploadSpotLight(m_shader);
    // Terrain displacement + texture fetch uniforms
    float heightScale = m_terrain->recommendedHeightScale();
    m_shader->setFloat("uHeightScale", heightScale);
    // Pass light space matrix and shadow map to terrain shader
    m_shader->setMat4("uLightSpace", lightSpace);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    m_shader->setInt("uShadowMap", 3);
    // Bind the procedural grass texture
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_grassTexture);
    m_shader->setInt("uGrassTex", 2);
    m_shader->setFloat("uGrassScale", 30.0f);
    // Bind biome textures
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_texFungus);
    m_shader->setInt("uTexFungus", 4);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_texSandgrass);
    m_shader->setInt("uTexSandgrass", 5);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_texRocks);
    m_shader->setInt("uTexRocks", 6);
    glm::vec2 texelSize(1.0f / (float)m_terrain->widthResolution(), 1.0f / (float)m_terrain->lengthResolution());
    m_shader->setVec2("uTexelSize", texelSize);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_terrain->heightTexture());
    m_shader->setInt("uHeightMap", 0);
        // Fog parameters to give depth cue and break the infinite look
        m_shader->setVec3("uSkyColor", skyColor);
        // Start fog after 200 world units, reach full fog after additional 1200 units
        m_shader->setFloat("uFogStart", fogStart);
        m_shader->setFloat("uFogRange", fogRange);
    m_renderer->drawMesh(*m_terrain->mesh(), *m_shader, *m_camera, model);

    if(m_characterReady && m_characterShader){
        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", true);
        m_characterShader->setMat4("uModel", characterModel);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        // Night: 0.25x ambient (stronger shadows), Day: 0.5x ambient
        float charAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 ambientColor = glm::vec3(m_light.ambient) * m_light.color * charAmbientMult;
        m_characterShader->setVec3("uAmbientColor", ambientColor);
        uploadPointLights(m_characterShader);
        uploadSpotLight(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        // Add shadow receiving
        m_characterShader->setMat4("uLightSpace", lightSpace);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_characterAlbedoTex);
        m_characterShader->setInt("uAlbedo", 8);
        glBindVertexArray(m_characterMesh.vao);
        glDrawElements(GL_TRIANGLES, m_characterMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Render lighthouse (static model)
    if(m_lighthouseReady && m_characterShader && !m_lighthouseMesh.parts.empty()){
        glm::mat4 lighthouseModel = glm::mat4(1.0f);
        lighthouseModel = glm::translate(lighthouseModel, m_lighthousePosition);
        lighthouseModel = glm::scale(lighthouseModel, glm::vec3(m_lighthouseScale));
        
        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", false);
        m_characterShader->setMat4("uModel", lighthouseModel);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        // Use same ambient as character
        float lighthouseAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 ambientColor = glm::vec3(m_light.ambient) * m_light.color * lighthouseAmbientMult;
        m_characterShader->setVec3("uAmbientColor", ambientColor);
        uploadPointLights(m_characterShader);
        uploadSpotLight(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        m_characterShader->setMat4("uLightSpace", lightSpace);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        m_characterShader->setInt("uAlbedo", 8);
        for(const auto& part : m_lighthouseMesh.parts){
            glBindTexture(GL_TEXTURE_2D, part.albedoTex);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Render trees (static model instances)
    if(m_treeReady && m_characterShader && !m_treeInstances.empty() && !m_treeMesh.parts.empty()){
        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", false);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        float treeAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 ambientColor = glm::vec3(m_light.ambient) * m_light.color * treeAmbientMult;
        m_characterShader->setVec3("uAmbientColor", ambientColor);
        uploadPointLights(m_characterShader);
        uploadSpotLight(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        m_characterShader->setMat4("uLightSpace", lightSpace);

        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        m_characterShader->setInt("uAlbedo", 8);

        for(const TreeInstance& tree : m_treeInstances){
            glm::mat4 treeModel = glm::mat4(1.0f);
            treeModel = glm::translate(treeModel, tree.position);
            treeModel = glm::scale(treeModel, glm::vec3(tree.scale));
            m_characterShader->setMat4("uModel", treeModel);
            for(const auto& part : m_treeMesh.parts){
                glBindTexture(GL_TEXTURE_2D, part.albedoTex);
                glBindVertexArray(part.vao);
                glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Render campfire (static model)
    if(m_campfireReady && m_characterShader && !m_campfireMesh.parts.empty()){
        glm::mat4 campfireModel = glm::mat4(1.0f);
        campfireModel = glm::translate(campfireModel, m_campfirePosition);
        campfireModel = glm::scale(campfireModel, glm::vec3(m_campfireScale));

        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", false);
        m_characterShader->setMat4("uModel", campfireModel);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        float campfireAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 ambientColor = glm::vec3(m_light.ambient) * m_light.color * campfireAmbientMult;
        m_characterShader->setVec3("uAmbientColor", ambientColor);
        uploadPointLights(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        m_characterShader->setMat4("uLightSpace", lightSpace);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        m_characterShader->setInt("uAlbedo", 8);
        for(const auto& part : m_campfireMesh.parts){
            glBindTexture(GL_TEXTURE_2D, part.albedoTex);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Render stick (world item or attached to character)
    if(m_stickReady && m_characterShader && !m_stickMesh.parts.empty()){
        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", false);
        m_characterShader->setMat4("uModel", m_stickItem.worldMatrix);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        float stickAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 stickAmbient = glm::vec3(m_light.ambient) * m_light.color * stickAmbientMult;
        m_characterShader->setVec3("uAmbientColor", stickAmbient);
        uploadPointLights(m_characterShader);
        uploadSpotLight(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        m_characterShader->setMat4("uLightSpace", lightSpace);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        m_characterShader->setInt("uAlbedo", 8);
        for(const auto& part : m_stickMesh.parts){
            glBindTexture(GL_TEXTURE_2D, part.albedoTex);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Render forest hut (static model)
    if(m_forestHutReady && m_characterShader && !m_forestHutMesh.parts.empty()){
        glm::mat4 hutModel = glm::mat4(1.0f);
        hutModel = glm::translate(hutModel, m_forestHutPosition);
        hutModel = glm::rotate(hutModel, glm::radians(m_forestHutYawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
        hutModel = glm::rotate(hutModel, glm::radians(m_forestHutPitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
        hutModel = glm::scale(hutModel, glm::vec3(m_forestHutScale));

        m_characterShader->bind();
        m_characterShader->setBool("uUseSkinning", false);
        m_characterShader->setMat4("uModel", hutModel);
        m_characterShader->setMat4("uView", m_camera->viewMatrix());
        m_characterShader->setMat4("uProj", m_camera->projectionMatrix());
        m_characterShader->setVec3("uLightDir", m_light.direction);
        m_characterShader->setVec3("uLightColor", m_light.color);
        float hutAmbientMult = m_isNightMode ? 0.25f : 0.5f;
        glm::vec3 hutAmbient = glm::vec3(m_light.ambient) * m_light.color * hutAmbientMult;
        m_characterShader->setVec3("uAmbientColor", hutAmbient);
        uploadPointLights(m_characterShader);
        uploadSpotLight(m_characterShader);
        m_characterShader->setVec3("uCameraPos", m_camera->position());
        m_characterShader->setVec3("uSkyColor", skyColor);
        m_characterShader->setFloat("uFogStart", fogStart);
        m_characterShader->setFloat("uFogRange", fogRange);
        m_characterShader->setFloat("uSpecularStrength", m_light.specularStrength * 0.8f);
        m_characterShader->setFloat("uShininess", m_light.shininess);
        m_characterShader->setMat4("uLightSpace", lightSpace);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_characterShader->setInt("uShadowMap", 9);
        glActiveTexture(GL_TEXTURE8);
        m_characterShader->setInt("uAlbedo", 8);
        for(const auto& part : m_forestHutMesh.parts){
            glBindTexture(GL_TEXTURE_2D, part.albedoTex);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // Alpha-tested grass billboards render after terrain so depth clipping still works
    if(m_grassPatchCount > 0 && m_grassVAO != 0 && m_grassShader){
        m_grassShader->bind();
        m_grassShader->setMat4("uView", m_camera->viewMatrix());
        m_grassShader->setMat4("uProj", m_camera->projectionMatrix());
        m_grassShader->setMat4("uLightSpace", lightSpace);  // For shadows
        m_grassShader->setFloat("uTime", Time::elapsed());
        m_grassShader->setVec2("uWindDir", glm::vec2(0.65f, 0.2f));
        m_grassShader->setFloat("uBladeWidth", 0.28f);  // Slightly thinner for realism
        m_grassShader->setVec2("uAtlasTileScale", glm::vec2(0.5f, 0.5f));
        m_grassShader->setFloat("uWindStrength", 0.28f);
        m_grassShader->setFloat("uAlphaCutoff", 0.35f);
        m_grassShader->setVec3("uLightDir", m_light.direction);
        // Match sun lighting with slight green tint for grass
        m_grassShader->setVec3("uLightColor", m_light.color * glm::vec3(0.9f, 1.0f, 0.85f));
        // Night: 0.35x ambient (stronger shadows), Day: 0.7x ambient
        float grassAmbientMult = m_isNightMode ? 0.35f : 0.7f;
        glm::vec3 grassAmbient = m_light.color * m_light.ambient * grassAmbientMult;
        m_grassShader->setVec3("uAmbientColor", grassAmbient);
        uploadPointLights(m_grassShader);
        uploadSpotLight(m_grassShader);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_grassBillboardTex);
        m_grassShader->setInt("uGrassAtlas", 7);
        // Bind shadow map for grass shadows
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex);
        m_grassShader->setInt("uShadowMap", 8);
        glBindVertexArray(m_grassVAO);
        glDrawArrays(GL_POINTS, 0, m_grassPatchCount);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }
    
    if(m_fireFXReady){
        glm::mat4 view = m_camera->viewMatrix();
        glm::mat4 viewProj = m_camera->projectionMatrix() * view;
        renderFireParticles(viewProj, view);
        renderStickFlame(viewProj, view);
        renderBeaconGlow(viewProj, view);
    }

    // Render water
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth for transparency
    m_waterShader->bind();
    m_waterShader->setVec3("uCameraPos", m_camera->position());
    m_waterShader->setFloat("uTime", Time::elapsed());
    m_waterShader->setVec3("uSkyColor", skyColor);
    m_waterShader->setFloat("uFogStart", fogStart);
    m_waterShader->setFloat("uFogRange", fogRange);
    m_waterShader->setVec3("uLightDir", m_light.direction);
    m_waterShader->setVec3("uLightColor", m_light.color);
    uploadPointLights(m_waterShader);
    uploadSpotLight(m_waterShader);
    // Provide light space and shadow map so water can receive scene shadows
    m_waterShader->setMat4("uLightSpace", lightSpace);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_waveHeightTex[0]);
    m_waterShader->setInt("uWaveHeight0", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_waveHeightTex[1]);
    m_waterShader->setInt("uWaveHeight1", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_waveNormalTex[0]);
    m_waterShader->setInt("uWaveNormal0", 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_waveNormalTex[1]);
    m_waterShader->setInt("uWaveNormal1", 3);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_terrain->heightTexture());
    m_waterShader->setInt("uTerrainHeightMap", 4);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_shadowTex);
    m_waterShader->setInt("uShadowMap", 5);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envCubemap);
    m_waterShader->setInt("uEnvMap", 6);
    // Enhanced ocean-like water parameters with sharp ridges
    m_waterShader->setVec2("uLayer0Speed", glm::vec2(0.025f, 0.018f));  // Moderate speed
    m_waterShader->setVec2("uLayer1Speed", glm::vec2(-0.015f, 0.028f));  // Cross-wave pattern
    m_waterShader->setFloat("uLayer0Strength", 2.8f);  // Higher waves
    m_waterShader->setFloat("uLayer1Strength", 2.2f);  // Stronger secondary waves
    m_waterShader->setFloat("uBlendSharpness", 4.5f);  // Much sharper ridges
    m_waterShader->setFloat("uFoamThreshold", 0.18f);  // More foam coverage
    m_waterShader->setFloat("uFoamIntensity", 1.5f);  // Brighter foam
    m_waterShader->setFloat("uRefractStrength", 0.30f);  // Less refraction
    m_waterShader->setFloat("uReflectStrength", 0.95f);  // Strong sun reflection
    m_waterShader->setFloat("uHeightScale", heightScale);
    m_waterShader->setFloat("uWorldSize", m_terrain->worldSize());
    // Restore default active texture for other passes
    glActiveTexture(GL_TEXTURE0);
    m_renderer->drawMesh(*m_water->mesh(), *m_waterShader, *m_camera, model);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    
    m_renderer->endFrame();

#ifdef BUILD_IMGUI
    // Single ImGui frame for all UI elements
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Show region overlay if enabled (R key toggle)
    if(m_showRegions){
        // Region info panel
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("Terrain Regions", nullptr, 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "REGION DISPLAY ACTIVE");
        ImGui::Separator();
        
        // Current position
        glm::vec3 camPos = m_camera->position();
        float terrainHeight = m_terrain ? m_terrain->getHeight(camPos.x, camPos.z) : 0.0f;
        std::string currentRegion = getRegionAtPosition(camPos);
        
        ImGui::Text("Camera Position:");
        ImGui::Text("  X: %.1f  Y: %.1f  Z: %.1f", camPos.x, camPos.y, camPos.z);
        ImGui::Text("  Terrain Height: %.1f", terrainHeight);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "  Region: %s", currentRegion.c_str());
        
        ImGui::Separator();
        ImGui::Text("Available Regions (%zu):", m_terrainRegions.size());
        ImGui::BeginChild("RegionList", ImVec2(400, 300), true);
        
        for(const auto& region : m_terrainRegions){
            bool isCurrentRegion = (region.name == currentRegion);
            if(isCurrentRegion){
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
            }
            
            ImGui::Text("• %s", region.name.c_str());
            ImGui::Text("    %s", region.description.c_str());
            ImGui::Text("    XZ: [%.0f,%.0f] to [%.0f,%.0f]", 
                       region.minXZ.x, region.minXZ.y, 
                       region.maxXZ.x, region.maxXZ.y);
            ImGui::Text("    Y: [%.0f - %.0f]", region.minY, region.maxY);
            
            if(isCurrentRegion){
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        }
        
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press R to hide");
        
        ImGui::End();
    } else {
        // Build a small debug panel to tweak terrain parameters
        bool opened = true;
        ImGui::Begin("Debug - Terrain", &opened, ImGuiWindowFlags_AlwaysAutoResize);
        float hs = m_terrain->heightScaleMultiplier();
        if (ImGui::SliderFloat("Height Scale Mul", &hs, 0.01f, 1.0f)){
            m_terrain->setHeightScaleMultiplier(hs);
            m_terrain->recomputeNormals();
        }
        float ma = m_terrain->microAmplitude();
        if (ImGui::SliderFloat("Micro Amplitude", &ma, 0.0f, 0.08f)){
            m_terrain->setMicroAmplitude(ma);
            m_terrain->recomputeNormals();
        }
        float mf = m_terrain->microFrequency();
        if (ImGui::SliderFloat("Micro Frequency", &mf, 0.01f, 2.0f)){
            m_terrain->setMicroFrequency(mf);
            m_terrain->recomputeNormals();
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
    // Fallback without ImGui: print region to console
    if(m_showRegions){
        static float lastPrintTime = 0.0f;
        float currentTime = Time::elapsed();
        if(currentTime - lastPrintTime > 2.0f){
            glm::vec3 camPos = m_camera->position();
            std::string region = getRegionAtPosition(camPos);
            std::cout << "[Region] Current: " << region 
                      << " at (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")" << std::endl;
            lastPrintTime = currentTime;
        }
    }
#endif
}

void Game::setupCampfireLight(){
    if(!m_campfireReady){
        m_campfireLight.enabled = false;
        return;
    }
    m_campfireLight.enabled = true;
    m_campfireLight.position = m_campfireEmitterPos;
    m_campfireLight.color = glm::vec3(1.0f, 0.65f, 0.25f);
    m_campfireLight.baseIntensity = 2.2f;
    m_campfireLight.intensity = m_campfireLight.baseIntensity;
    m_campfireLight.radius = 30.0f;
    m_campfireLight.flickerTimer = 0.0f;
}

void Game::initCampfireFireFX(){
    if(!m_campfireReady || m_fireTexture == 0)
        return;
    if(m_fireShader == nullptr){
        m_fireShader = new Shader();
        if(!m_fireShader->compile(kFireParticleVert, kFireParticleFrag)){
            delete m_fireShader;
            m_fireShader = nullptr;
            return;
        }
    }
    if(m_fireVAO == 0){
        glGenVertexArrays(1, &m_fireVAO);
        glBindVertexArray(m_fireVAO);

        glGenBuffers(1, &m_fireQuadVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_fireQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kFireQuadVertices.size(), kFireQuadVertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));

        glGenBuffers(1, &m_fireInstanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_fireInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, kMaxFireParticles * sizeof(float) * 6, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<void*>(0));
        glVertexAttribDivisor(2, 1);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<void*>(sizeof(float) * 4));
        glVertexAttribDivisor(3, 1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_fireParticles.resize(kMaxFireParticles);
    for(auto& particle : m_fireParticles){
        respawnFireParticle(particle);
    }
    uploadFireParticlesToGPU();
    m_fireFXReady = true;
    initStickFlameBillboard();
}

void Game::initStickFlameBillboard(){
    if(m_stickFlameReady || m_fireTexture == 0)
        return;
    if(m_stickFlameShader == nullptr){
        m_stickFlameShader = new Shader();
        if(!m_stickFlameShader->compile(kStickFlameVert, kStickFlameFrag)){
            delete m_stickFlameShader;
            m_stickFlameShader = nullptr;
            return;
        }
    }
    if(m_stickFlameVAO == 0){
        glGenVertexArrays(1, &m_stickFlameVAO);
        glBindVertexArray(m_stickFlameVAO);
        glGenBuffers(1, &m_stickFlameVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_stickFlameVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kFireQuadVertices.size(), kFireQuadVertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_stickFlameReady = true;
}

void Game::respawnFireParticle(FireParticle& particle){
    std::uniform_real_distribution<float> spread(-0.35f, 0.35f);
    std::uniform_real_distribution<float> rise(1.6f, 2.6f);
    std::uniform_real_distribution<float> life(0.7f, 1.2f);
    std::uniform_real_distribution<float> size(0.9f, 1.5f);
    std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
    particle.position = m_campfireEmitterPos + glm::vec3(spread(m_fireRng), 0.0f, spread(m_fireRng));
    particle.velocity = glm::vec3(spread(m_fireRng) * 0.5f, rise(m_fireRng), spread(m_fireRng) * 0.5f);
    particle.life = 0.0f;
    particle.maxLife = life(m_fireRng);
    particle.size = size(m_fireRng);
    particle.seed = seedDist(m_fireRng);
}

void Game::updateFireParticles(float dt){
    if(!m_fireFXReady)
        return;
    for(auto& particle : m_fireParticles){
        particle.life += dt;
        if(particle.life >= particle.maxLife){
            respawnFireParticle(particle);
            continue;
        }
        particle.position += particle.velocity * dt;
        particle.velocity += glm::vec3(0.0f, 1.5f, 0.0f) * dt;
        particle.velocity.x *= 0.98f;
        particle.velocity.z *= 0.98f;
    }
    uploadFireParticlesToGPU();
}

void Game::uploadFireParticlesToGPU(){
    if(!m_fireFXReady || m_fireInstanceVBO == 0)
        return;
    std::vector<float> buffer(m_fireParticles.size() * 6);
    size_t idx = 0;
    for(const auto& particle : m_fireParticles){
        float lifeNorm = particle.maxLife > 0.0f ? particle.life / particle.maxLife : 0.0f;
        buffer[idx++] = particle.position.x;
        buffer[idx++] = particle.position.y;
        buffer[idx++] = particle.position.z;
        buffer[idx++] = particle.size;
        buffer[idx++] = glm::clamp(lifeNorm, 0.0f, 1.0f);
        buffer[idx++] = particle.seed;
    }
    glBindBuffer(GL_ARRAY_BUFFER, m_fireInstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer.size() * sizeof(float), buffer.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Game::renderFireParticles(const glm::mat4& viewProj, const glm::mat4& view){
    if(!m_fireFXReady || !m_fireShader || m_fireVAO == 0)
        return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    m_fireShader->bind();
    m_fireShader->setMat4("uViewProj", viewProj);
    glm::vec3 rawRight(view[0][0], view[1][0], view[2][0]);
    glm::vec3 rawUp(view[0][1], view[1][1], view[2][1]);
    if(glm::length2(rawRight) < 1e-6f) rawRight = glm::vec3(1.0f, 0.0f, 0.0f);
    if(glm::length2(rawUp) < 1e-6f) rawUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 camRight = glm::normalize(rawRight);
    glm::vec3 camUp = glm::normalize(rawUp);
    m_fireShader->setVec3("uCameraRight", camRight);
    m_fireShader->setVec3("uCameraUp", camUp);
    glActiveTexture(GL_TEXTURE15);
    glBindTexture(GL_TEXTURE_2D, m_fireTexture);
    m_fireShader->setInt("uFireTex", 15);
    glBindVertexArray(m_fireVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, kFireQuadVertexCount, static_cast<GLsizei>(m_fireParticles.size()));
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void Game::renderStickFlame(const glm::mat4& viewProj, const glm::mat4& view){
    if(!m_stickFlameReady || !m_stickFlameVisible || !m_stickFlameShader)
        return;
    glm::vec3 rawRight(view[0][0], view[1][0], view[2][0]);
    glm::vec3 rawUp(view[0][1], view[1][1], view[2][1]);
    if(glm::length2(rawRight) < 1e-6f) rawRight = glm::vec3(1.0f, 0.0f, 0.0f);
    if(glm::length2(rawUp) < 1e-6f) rawUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 camRight = glm::normalize(rawRight);
    glm::vec3 camUp = glm::normalize(rawUp);
    float baseSize = 0.65f * m_stickItem.scale * 20.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    m_stickFlameShader->bind();
    m_stickFlameShader->setMat4("uViewProj", viewProj);
    m_stickFlameShader->setVec3("uWorldPos", m_stickFlamePos + glm::vec3(0.0f, 0.15f, 0.0f));
    m_stickFlameShader->setVec3("uCameraRight", camRight);
    m_stickFlameShader->setVec3("uCameraUp", camUp);
    m_stickFlameShader->setFloat("uSize", baseSize);
    m_stickFlameShader->setFloat("uGlow", 0.8f);
    m_stickFlameShader->setVec3("uTint", glm::vec3(1.0f, 0.72f, 0.32f));
    m_stickFlameShader->setFloat("uOpacity", 0.92f);
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, m_fireTexture);
    m_stickFlameShader->setInt("uFlameTex", 16);
    glBindVertexArray(m_stickFlameVAO);
    glDrawArrays(GL_TRIANGLES, 0, kFireQuadVertexCount);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void Game::renderBeaconGlow(const glm::mat4& viewProj, const glm::mat4&){
    if(!m_stickFlameReady || !m_stickFlameShader || !m_beaconGlowVisible || !m_beaconLight.enabled)
        return;
    if(m_beaconDiscTexture == 0)
        return;

    glm::vec3 beaconPos = m_beaconLight.position;
    glm::vec3 planarDir(m_beaconLight.direction.x, 0.0f, m_beaconLight.direction.z);
    float planarLen2 = glm::length2(planarDir);
    if(planarLen2 < 1e-6f){
        planarDir = glm::vec3(std::cos(m_beaconRotationAngle), 0.0f, std::sin(m_beaconRotationAngle));
        planarLen2 = glm::length2(planarDir);
    }
    if(planarLen2 < 1e-6f){
        planarDir = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        planarDir /= std::sqrt(planarLen2);
    }
    glm::vec3 tangentDir(-planarDir.z, 0.0f, planarDir.x);
    if(glm::length2(tangentDir) < 1e-6f){
        tangentDir = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        tangentDir = glm::normalize(tangentDir);
    }

    float orbitRadius = 0.65f * m_lighthouseScale;
    glm::vec3 orbitPos = beaconPos + planarDir * orbitRadius;
    float discHeightOffset = 0.05f * m_lighthouseScale;
    glm::vec3 discWorldPos = orbitPos + glm::vec3(0.0f, discHeightOffset, 0.0f);
    float discSize = 0.65f * m_lighthouseScale;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    m_stickFlameShader->bind();
    m_stickFlameShader->setMat4("uViewProj", viewProj);
    m_stickFlameShader->setVec3("uWorldPos", discWorldPos);
    m_stickFlameShader->setVec3("uCameraRight", tangentDir);
    m_stickFlameShader->setVec3("uCameraUp", planarDir);
    m_stickFlameShader->setFloat("uSize", discSize);
    m_stickFlameShader->setFloat("uGlow", 0.85f);
    m_stickFlameShader->setVec3("uTint", glm::vec3(1.35f, 1.32f, 1.2f));
    m_stickFlameShader->setFloat("uOpacity", 1.0f);
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, m_beaconDiscTexture);
    m_stickFlameShader->setInt("uFlameTex", 16);
    glBindVertexArray(m_stickFlameVAO);
    glDrawArrays(GL_TRIANGLES, 0, kFireQuadVertexCount);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void Game::updateCampfireLight(float dt){
    if(!m_campfireLight.enabled)
        return;
    m_campfireLight.flickerTimer += dt;
    float flicker = 0.6f + 0.4f * std::sin(m_campfireLight.flickerTimer * 6.2f + std::sin(m_campfireLight.flickerTimer * 2.3f));
    m_campfireLight.intensity = m_campfireLight.baseIntensity * glm::clamp(flicker, 0.6f, 1.3f);
    float colorShift = 0.04f * std::sin(m_campfireLight.flickerTimer * 4.1f);
    m_campfireLight.color = glm::vec3(1.0f, 0.6f + colorShift, 0.2f);
    m_campfireLight.position = m_campfireEmitterPos;
}

glm::vec3 Game::getLighthouseBeaconWorldPosition() const{
    if(!m_lighthouseReady) return glm::vec3(0.0f);
    return m_lighthousePosition + m_lighthouseBeaconLocal * m_lighthouseScale;
}

void Game::updateBeaconLight(float dt){
    if(!m_lighthouseReady){
        m_beaconLight.enabled = false;
        m_beaconGlowVisible = false;
        return;
    }
    if(!m_isNightMode){
        m_beaconLight.enabled = false;
        m_beaconGlowVisible = false;
        return;
    }
    m_beaconRotationAngle = std::fmod(m_beaconRotationAngle + m_beaconRotationSpeed * dt, glm::two_pi<float>());
    glm::vec3 beaconPos = getLighthouseBeaconWorldPosition();
    glm::vec3 sweepDir(std::cos(m_beaconRotationAngle), -0.2f, std::sin(m_beaconRotationAngle));
    if(glm::length2(sweepDir) < 1e-4f){
        sweepDir = glm::vec3(0.0f, -1.0f, 0.0f);
    } else {
        sweepDir = glm::normalize(sweepDir);
    }
    m_beaconLight.position = beaconPos;
    m_beaconLight.direction = sweepDir;
    m_beaconLight.enabled = true;
    m_beaconGlowVisible = true;
}

void Game::shutdown(){
    std::cout << "[Game] Shutdown" << std::endl;
#ifdef BUILD_IMGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    if(m_shadowTex) { glDeleteTextures(1, &m_shadowTex); m_shadowTex = 0; }
    if(m_shadowFBO) { glDeleteFramebuffers(1, &m_shadowFBO); m_shadowFBO = 0; }
    delete m_depthShader; m_depthShader = nullptr;
    delete m_skinnedDepthShader; m_skinnedDepthShader = nullptr;
    if(m_fireTexture) { glDeleteTextures(1, &m_fireTexture); m_fireTexture = 0; }
    if(m_beaconDiscTexture) { glDeleteTextures(1, &m_beaconDiscTexture); m_beaconDiscTexture = 0; }
    if(m_fireInstanceVBO) { glDeleteBuffers(1, &m_fireInstanceVBO); m_fireInstanceVBO = 0; }
    if(m_fireQuadVBO) { glDeleteBuffers(1, &m_fireQuadVBO); m_fireQuadVBO = 0; }
    if(m_fireVAO) { glDeleteVertexArrays(1, &m_fireVAO); m_fireVAO = 0; }
    delete m_fireShader; m_fireShader = nullptr;
    if(m_stickFlameVBO) { glDeleteBuffers(1, &m_stickFlameVBO); m_stickFlameVBO = 0; }
    if(m_stickFlameVAO) { glDeleteVertexArrays(1, &m_stickFlameVAO); m_stickFlameVAO = 0; }
    delete m_stickFlameShader; m_stickFlameShader = nullptr;
    m_stickFlameReady = false;
    m_fireFXReady = false;
    if(m_grassTexture) { glDeleteTextures(1, &m_grassTexture); m_grassTexture = 0; }
    if(m_texFungus) { glDeleteTextures(1, &m_texFungus); m_texFungus = 0; }
    if(m_texSandgrass) { glDeleteTextures(1, &m_texSandgrass); m_texSandgrass = 0; }
    if(m_texRocks) { glDeleteTextures(1, &m_texRocks); m_texRocks = 0; }
    if(m_grassBillboardTex) { glDeleteTextures(1, &m_grassBillboardTex); m_grassBillboardTex = 0; }
    for(unsigned int& tex : m_waveHeightTex){ if(tex){ glDeleteTextures(1, &tex); tex = 0; } }
    for(unsigned int& tex : m_waveNormalTex){ if(tex){ glDeleteTextures(1, &tex); tex = 0; } }
    if(m_envCubemap) { glDeleteTextures(1, &m_envCubemap); m_envCubemap = 0; }
    if(m_grassVAO) { glDeleteVertexArrays(1, &m_grassVAO); m_grassVAO = 0; }
    if(m_grassVBO) { glDeleteBuffers(1, &m_grassVBO); m_grassVBO = 0; }
    setActiveTerrain(nullptr);
    if(m_characterMesh.vao){ glDeleteVertexArrays(1, &m_characterMesh.vao); m_characterMesh.vao = 0; }
    if(m_characterMesh.vbo){ glDeleteBuffers(1, &m_characterMesh.vbo); m_characterMesh.vbo = 0; }
    if(m_characterMesh.ibo){ glDeleteBuffers(1, &m_characterMesh.ibo); m_characterMesh.ibo = 0; }
    if(m_characterMesh.albedoTex){ glDeleteTextures(1, &m_characterMesh.albedoTex); m_characterMesh.albedoTex = 0; }
    if(m_characterAlbedoTex){ glDeleteTextures(1, &m_characterAlbedoTex); m_characterAlbedoTex = 0; }
    if(m_boneUBO){ glDeleteBuffers(1, &m_boneUBO); m_boneUBO = 0; }
    delete m_characterShader; m_characterShader = nullptr;
    m_animator.reset();
    auto releaseStaticMesh = [](StaticMesh& mesh){
        for(auto& part : mesh.parts){
            if(part.vao){ glDeleteVertexArrays(1, &part.vao); part.vao = 0; }
            if(part.vbo){ glDeleteBuffers(1, &part.vbo); part.vbo = 0; }
            if(part.ibo){ glDeleteBuffers(1, &part.ibo); part.ibo = 0; }
            if(part.albedoTex){ glDeleteTextures(1, &part.albedoTex); part.albedoTex = 0; }
        }
        mesh.parts.clear();
        mesh.totalVertexCount = 0;
        mesh.totalIndexCount = 0;
        mesh.minBounds = glm::vec3(0.0f);
        mesh.maxBounds = glm::vec3(0.0f);
    };
    releaseStaticMesh(m_lighthouseMesh);
    releaseStaticMesh(m_treeMesh);
    releaseStaticMesh(m_campfireMesh);
    releaseStaticMesh(m_forestHutMesh);
    delete m_renderer; delete m_shader; delete m_waterShader; delete m_grassShader; delete m_camera; delete m_terrain; delete m_water; delete m_sky;
    m_grassShader = nullptr;
}

void Game::initTerrainRegions(){
    // Terrain: 384.0f world size (centered at origin, so -192 to +192)
    // Water level: 10.0f
    // Layout: Underwater -> Beach -> Grassland -> Plateau
    
    float worldSize = 384.0f;
    float halfSize = worldSize / 2.0f;  // 192.0f
    float waterY = m_waterLevel;  // 10.0f
    
    // Calculate strip widths based on terrain layout
    // Beach: First strip above water (narrow, ~30-40 units from water edge)
    // Grassland: Second strip (medium, ~60-80 units wide)
    // Plateau: Rest of terrain (highest elevations)
    
    // Estimate height zones from terrain generation
    // Beach: water level to ~15 units
    // Grassland: ~15 to ~30 units
    // Plateau: ~30+ units
    
    m_terrainRegions.clear();
    
    // === UNDERWATER REGIONS ===
    m_terrainRegions.push_back({
        "underwater_north",
        glm::vec2(-halfSize, -halfSize),
        glm::vec2(halfSize, -halfSize/3.0f),
        0.0f, waterY,
        "Underwater area - northern section"
    });
    
    m_terrainRegions.push_back({
        "underwater_center",
        glm::vec2(-halfSize, -halfSize/3.0f),
        glm::vec2(halfSize, halfSize/3.0f),
        0.0f, waterY,
        "Underwater area - central section"
    });
    
    m_terrainRegions.push_back({
        "underwater_south",
        glm::vec2(-halfSize, halfSize/3.0f),
        glm::vec2(halfSize, halfSize),
        0.0f, waterY,
        "Underwater area - southern section"
    });
    
    // === BEACH REGIONS (First strip above water) ===
    // Height: water level (10) to ~18 units
    m_terrainRegions.push_back({
        "beach_north",
        glm::vec2(-halfSize, -halfSize),
        glm::vec2(halfSize, -halfSize/3.0f),
        waterY, 18.0f,
        "Beach - northern coastline"
    });
    
    m_terrainRegions.push_back({
        "beach_center",
        glm::vec2(-halfSize, -halfSize/3.0f),
        glm::vec2(halfSize, halfSize/3.0f),
        waterY, 18.0f,
        "Beach - central coastline"
    });
    
    m_terrainRegions.push_back({
        "beach_south",
        glm::vec2(-halfSize, halfSize/3.0f),
        glm::vec2(halfSize, halfSize),
        waterY, 18.0f,
        "Beach - southern coastline"
    });
    
    // === GRASSLAND REGIONS (Second strip) ===
    // Height: ~18 to ~35 units
    m_terrainRegions.push_back({
        "grassland_north",
        glm::vec2(-halfSize, -halfSize),
        glm::vec2(halfSize, -halfSize/3.0f),
        18.0f, 35.0f,
        "Grassland - northern area"
    });
    
    m_terrainRegions.push_back({
        "grassland_center",
        glm::vec2(-halfSize, -halfSize/3.0f),
        glm::vec2(halfSize, halfSize/3.0f),
        18.0f, 35.0f,
        "Grassland - central plains"
    });
    
    m_terrainRegions.push_back({
        "grassland_south",
        glm::vec2(-halfSize, halfSize/3.0f),
        glm::vec2(halfSize, halfSize),
        18.0f, 35.0f,
        "Grassland - southern area"
    });
    
    // === PLATEAU REGIONS (High elevation) ===
    // Height: 35+ units
    m_terrainRegions.push_back({
        "plateau_north",
        glm::vec2(-halfSize, -halfSize),
        glm::vec2(halfSize, -halfSize/3.0f),
        35.0f, 200.0f,
        "Plateau - northern highlands"
    });
    
    m_terrainRegions.push_back({
        "plateau_center",
        glm::vec2(-halfSize, -halfSize/3.0f),
        glm::vec2(halfSize, halfSize/3.0f),
        35.0f, 200.0f,
        "Plateau - central highlands"
    });
    
    m_terrainRegions.push_back({
        "plateau_south",
        glm::vec2(-halfSize, halfSize/3.0f),
        glm::vec2(halfSize, halfSize),
        35.0f, 200.0f,
        "Plateau - southern highlands"
    });
    
    // === SPECIAL REGIONS ===
    m_terrainRegions.push_back({
        "spawn_area",
        glm::vec2(-20.0f, -20.0f),
        glm::vec2(20.0f, 20.0f),
        0.0f, 200.0f,
        "Starting area near origin"
    });
    
    std::cout << "[Game] Initialized " << m_terrainRegions.size() << " terrain regions" << std::endl;
    std::cout << "\n=== CONTROLS ===" << std::endl;
    std::cout << "ESC  - Toggle cursor (FREE for UI / CAPTURED for camera)" << std::endl;
    std::cout << "R    - Toggle region display overlay" << std::endl;
    std::cout << "T    - Toggle day/night cycle" << std::endl;
    std::cout << "C    - Toggle camera mode (free/third-person)" << std::endl;
    std::cout << "W    - Move forward (when cursor captured)" << std::endl;
    std::cout << "Mouse- Look around (when cursor captured)" << std::endl;
    std::cout << "================\n" << std::endl;
}

std::string Game::getRegionAtPosition(const glm::vec3& pos) const {
    // Check special regions first (smaller, higher priority)
    for(int i = m_terrainRegions.size() - 1; i >= 0; --i){
        const auto& region = m_terrainRegions[i];
        if(pos.x >= region.minXZ.x && pos.x <= region.maxXZ.x &&
           pos.z >= region.minXZ.y && pos.z <= region.maxXZ.y &&
           pos.y >= region.minY && pos.y <= region.maxY){
            return region.name;
        }
    }
    return "unknown";
}

void Game::renderRegionOverlay(){
    // This function is now integrated into the main render() function
    // Kept for API compatibility but does nothing
}

void Game::updateStickInteraction(){
    if(!m_stickReady){
        m_stickFlameVisible = false;
        return;
    }

    // Ensure dropped stick hovers slightly above sampled terrain height
    if(!m_stickItem.isHeld){
        float terrainY = getTerrainHeightAt(m_stickItem.position.x, m_stickItem.position.z);
        m_stickItem.position.y = terrainY + m_stickHoverOffset;
    }

    refreshStickWorldMatrix();

    glm::vec3 stickTipWorld = getStickTipWorldPosition();
    if(m_stickLit){
        updateStickLight(stickTipWorld);
        m_stickFlamePos = stickTipWorld;
        m_stickFlameVisible = true;
    } else {
        m_stickLight.enabled = false;
        m_stickFlameVisible = false;
    }

    if(!m_characterReady){
        m_canIgniteStick = false;
        return;
    }

    glm::vec2 stickXZ(m_stickItem.position.x, m_stickItem.position.z);
    glm::vec2 playerXZ(m_characterController.position.x, m_characterController.position.z);
    float planarDistance = glm::length(stickXZ - playerXZ);
    m_canPickupStick = (!m_stickItem.isHeld && planarDistance <= m_stickItem.colliderRadius);

    bool nearCampfire = false;
    float tipDistance = -1.0f;
    if(m_stickItem.isHeld && m_campfireReady && m_campfireLight.enabled){
        tipDistance = glm::length(stickTipWorld - m_campfireEmitterPos);
        nearCampfire = tipDistance <= m_stickIgniteRadius;
    }
    if(nearCampfire != m_wasStickNearCampfire){
        if(nearCampfire){
            std::cout << "[StickTorch] Tip within ignite radius: dist=" << tipDistance
                      << ", radius=" << m_stickIgniteRadius << std::endl;
        } else if(m_wasStickNearCampfire){
            std::cout << "[StickTorch] Tip left ignite radius" << std::endl;
        }
        m_wasStickNearCampfire = nearCampfire;
    }
    m_canIgniteStick = nearCampfire && !m_stickLit;
    if(m_canIgniteStick && !m_prevCanIgniteStick){
        std::cout << "[StickTorch] Press E to ignite the torch" << std::endl;
    } else if(!m_canIgniteStick && m_prevCanIgniteStick){
        std::cout << "[StickTorch] Ignite prompt cleared" << std::endl;
    }
    m_prevCanIgniteStick = m_canIgniteStick;

    GLFWwindow* window = g_windowPtr ? g_windowPtr->nativeHandle() : nullptr;
    if(!window) return;

    int keyState = glfwGetKey(window, GLFW_KEY_F);
    if(keyState == GLFW_PRESS && !m_stickActionHeld){
        if(m_canPickupStick){
            attachStickToHand();
        } else if(m_stickItem.isHeld){
            dropStickToTerrain();
        }
        m_stickActionHeld = true;
    } else if(keyState == GLFW_RELEASE){
        m_stickActionHeld = false;
    }

    int igniteState = glfwGetKey(window, GLFW_KEY_E);
    if(igniteState == GLFW_PRESS && !m_stickIgniteHeld){
        if(m_canIgniteStick){
            igniteStickTorch();
        } else if(m_stickItem.isHeld){
            if(tipDistance >= 0.0f){
                std::cout << "[StickTorch] E pressed but tip distance " << tipDistance
                          << " exceeds ignite radius " << m_stickIgniteRadius << std::endl;
            } else {
                std::cout << "[StickTorch] E pressed but campfire not available" << std::endl;
            }
        }
        m_stickIgniteHeld = true;
    } else if(igniteState == GLFW_RELEASE){
        m_stickIgniteHeld = false;
    }
}

void Game::refreshStickWorldMatrix(){
    if(!m_stickReady) return;
    if(m_stickItem.isHeld){
        m_stickItem.worldMatrix = buildHeldStickMatrix();
        m_stickItem.position = glm::vec3(m_stickItem.worldMatrix[3]);
        return;
    }
    m_stickItem.worldMatrix = composeTransform(m_stickItem.position, m_stickItem.rotation, m_stickItem.scale);
}

void Game::attachStickToHand(){
    if(!m_stickReady || !m_characterReady) return;
    m_stickItem.isHeld = true;
    m_stickItem.collisionEnabled = false;
    refreshStickWorldMatrix();
    std::cout << "[Game] Stick attached to right hand" << std::endl;
}

void Game::dropStickToTerrain(){
    if(!m_stickReady) return;
    glm::vec3 forward(std::sin(m_characterController.yaw), 0.0f, std::cos(m_characterController.yaw));
    if(glm::length2(forward) < 1e-4f){
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        forward = glm::normalize(forward);
    }
    glm::vec3 dropPosition = m_characterController.position + forward * m_stickDropDistance;
    float terrainY = getTerrainHeightAt(dropPosition.x, dropPosition.z); // Heightmap raycast
    float baseOffset = m_stickBaseHeight * m_stickItem.scale;
    dropPosition.y = terrainY + m_stickHoverOffset - baseOffset;
    m_stickItem.position = dropPosition;
    m_stickItem.rotation = m_stickGroundRotation;
    m_stickItem.isHeld = false;
    m_stickItem.collisionEnabled = true;
    refreshStickWorldMatrix();
    std::cout << "[Game] Stick dropped at (" << dropPosition.x << ", " << dropPosition.y
              << ", " << dropPosition.z << ")" << std::endl;
}

glm::mat4 Game::buildHeldStickMatrix() const{
    if(!m_stickReady) return glm::mat4(1.0f);
    glm::quat characterOrientation = glm::angleAxis(m_characterController.yaw, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 anchor = m_characterController.position;
    anchor.y += m_characterFeetOffset * m_characterScale; // lift from controller root to actual ground contact

    glm::mat4 model(1.0f);
    model = glm::translate(model, anchor);
    model *= glm::mat4_cast(characterOrientation);
    model = glm::translate(model, m_stickLocalOffset);

    glm::mat4 holdRotation = glm::mat4(1.0f);
    holdRotation = glm::rotate(holdRotation, glm::radians(m_stickHoldEuler.y), glm::vec3(0.0f, 1.0f, 0.0f));
    holdRotation = glm::rotate(holdRotation, glm::radians(m_stickHoldEuler.x), glm::vec3(1.0f, 0.0f, 0.0f));
    holdRotation = glm::rotate(holdRotation, glm::radians(m_stickHoldEuler.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 baseAlign = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -m_stickBaseHeight, 0.0f));
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_stickItem.scale));

    return model * holdRotation * scaleMatrix * baseAlign;
}

glm::vec3 Game::getStickTipWorldPosition() const{
    if(!m_stickReady) return glm::vec3(0.0f);
    float tipLength = glm::max(m_stickTipLength, 0.0f);
    glm::vec4 localBase(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 localTip(0.0f, tipLength, 0.0f, 1.0f);
    glm::vec3 worldBase = glm::vec3(m_stickItem.worldMatrix * localBase);
    glm::vec3 worldTip = glm::vec3(m_stickItem.worldMatrix * localTip);
    glm::vec3 stickDir = worldTip - worldBase;
    float dirLen = glm::length(stickDir);
    if(dirLen < 1e-4f){
        stickDir = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        stickDir /= dirLen;
    }

    glm::vec3 adjustedTip = worldTip;
    const float upShift = 20.2f;
    const float forwardShift = -30.0f;
    adjustedTip += glm::vec3(0.0f, upShift * m_stickItem.scale, 0.0f);
    glm::vec3 horizontalDir = glm::vec3(stickDir.x, 0.0f, stickDir.z);
    if(glm::length2(horizontalDir) > 1e-4f){
        horizontalDir = glm::normalize(horizontalDir);
        adjustedTip += horizontalDir * (forwardShift * m_stickItem.scale);
    } else {
        adjustedTip += stickDir * (forwardShift * m_stickItem.scale);
    }
    return adjustedTip;
}

void Game::igniteStickTorch(){
    if(!m_stickReady || m_stickLit)
        return;
    initStickFlameBillboard();
    m_stickLit = true;
    m_stickLight.enabled = true;
    if(m_stickLight.baseIntensity <= 0.0f){
        m_stickLight.baseIntensity = 1.35f;
    }
    if(m_stickLight.radius <= 0.0f){
        m_stickLight.radius = 18.0f;
    }
    m_stickLight.intensity = m_stickLight.baseIntensity;
    m_stickLight.flickerTimer = 0.0f;
    updateStickLight(getStickTipWorldPosition());
    std::cout << "[Game] Stick ignited and is now a torch" << std::endl;
}

void Game::updateStickLight(const glm::vec3& tipWorldPos){
    if(!m_stickLit){
        m_stickLight.enabled = false;
        return;
    }
    m_stickLight.enabled = true;
    m_stickLight.position = tipWorldPos + glm::vec3(0.0f, 0.12f, 0.0f);
    float dt = Time::delta();
    m_stickLight.flickerTimer += dt * 4.5f;
    float flicker = 0.85f + 0.15f * std::sin(m_stickLight.flickerTimer * 5.2f + std::sin(m_stickLight.flickerTimer * 1.7f));
    m_stickLight.intensity = m_stickLight.baseIntensity * glm::clamp(flicker, 0.7f, 1.2f);
}

bool Game::loadStaticModel(const std::string& path, StaticMesh& outMesh){
    auto releaseParts = [](StaticMesh& mesh){
        for(auto& part : mesh.parts){
            if(part.vao){ glDeleteVertexArrays(1, &part.vao); part.vao = 0; }
            if(part.vbo){ glDeleteBuffers(1, &part.vbo); part.vbo = 0; }
            if(part.ibo){ glDeleteBuffers(1, &part.ibo); part.ibo = 0; }
            if(part.albedoTex){ glDeleteTextures(1, &part.albedoTex); part.albedoTex = 0; }
        }
        mesh.parts.clear();
        mesh.totalVertexCount = 0;
        mesh.totalIndexCount = 0;
        mesh.minBounds = glm::vec3(0.0f);
        mesh.maxBounds = glm::vec3(0.0f);
    };

    releaseParts(outMesh);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode){
        std::cerr << "[Game] Failed to load static model: " << path << " - " << importer.GetErrorString() << std::endl;
        return false;
    }

    if(scene->mNumMeshes == 0){
        std::cerr << "[Game] No meshes found in " << path << std::endl;
        return false;
    }

    auto uploadTexture = [](const unsigned char* pixels, int w, int h)->GLuint{
        if(!pixels || w <= 0 || h <= 0) return 0u;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    auto createFallbackTexture = []()->GLuint{
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        unsigned char white[3] = {220, 220, 220};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    };

    auto loadEmbeddedTexture = [&](const aiTexture* embedded)->GLuint{
        if(!embedded) return 0u;
        if(embedded->mHeight == 0){
            int dataSize = embedded->mWidth;
            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(embedded->pcData);
            int w=0,h=0,channels=0;
            unsigned char* decoded = stbi_load_from_memory(bytes, dataSize, &w, &h, &channels, 4);
            if(decoded){
                GLuint tex = uploadTexture(decoded, w, h);
                stbi_image_free(decoded);
                return tex;
            }
            return 0u;
        }
        int w = embedded->mWidth;
        int h = embedded->mHeight;
        std::vector<unsigned char> pixels(w * h * 4);
        for(int i = 0; i < w * h; ++i){
            const aiTexel& texel = embedded->pcData[i];
            pixels[i * 4 + 0] = texel.r;
            pixels[i * 4 + 1] = texel.g;
            pixels[i * 4 + 2] = texel.b;
            pixels[i * 4 + 3] = texel.a;
        }
        return uploadTexture(pixels.data(), w, h);
    };

    std::string baseDir;
    std::size_t slashPos = path.find_last_of("/\\");
    if(slashPos != std::string::npos){
        baseDir = path.substr(0, slashPos + 1);
    }

    for(unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx){
        aiMesh* mesh = scene->mMeshes[meshIdx];
        StaticMesh::Part part;

        std::vector<float> vertices;
        vertices.reserve(mesh->mNumVertices * 8);
        std::vector<unsigned int> indices;
        indices.reserve(mesh->mNumFaces * 3);

        glm::vec3 partMinBounds(FLT_MAX);
        glm::vec3 partMaxBounds(-FLT_MAX);

        for(unsigned int i = 0; i < mesh->mNumVertices; ++i){
            float px = mesh->mVertices[i].x;
            float py = mesh->mVertices[i].y;
            float pz = mesh->mVertices[i].z;

            vertices.push_back(px);
            vertices.push_back(py);
            vertices.push_back(pz);

            partMinBounds.x = std::min(partMinBounds.x, px);
            partMinBounds.y = std::min(partMinBounds.y, py);
            partMinBounds.z = std::min(partMinBounds.z, pz);
            partMaxBounds.x = std::max(partMaxBounds.x, px);
            partMaxBounds.y = std::max(partMaxBounds.y, py);
            partMaxBounds.z = std::max(partMaxBounds.z, pz);

            if(mesh->HasNormals()){
                vertices.push_back(mesh->mNormals[i].x);
                vertices.push_back(mesh->mNormals[i].y);
                vertices.push_back(mesh->mNormals[i].z);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
            }

            if(mesh->HasTextureCoords(0)){
                vertices.push_back(mesh->mTextureCoords[0][i].x);
                vertices.push_back(mesh->mTextureCoords[0][i].y);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
            }
        }

        for(unsigned int i = 0; i < mesh->mNumFaces; ++i){
            const aiFace& face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; ++j){
                indices.push_back(face.mIndices[j]);
            }
        }

        glGenVertexArrays(1, &part.vao);
        glGenBuffers(1, &part.vbo);
        glGenBuffers(1, &part.ibo);

        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glBindVertexArray(0);

        part.vertexCount = mesh->mNumVertices;
        part.indexCount = static_cast<unsigned int>(indices.size());
        part.minBounds = partMinBounds;
        part.maxBounds = partMaxBounds;

        GLuint textureHandle = 0;
        if(mesh->mMaterialIndex >= 0){
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString texPath;
            if(material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS){
                std::string texName = texPath.C_Str();
                if(!texName.empty() && texName[0] == '*'){
                    int embeddedIdx = -1;
                    try {
                        embeddedIdx = std::stoi(texName.substr(1));
                    } catch(const std::exception&){
                        embeddedIdx = -1;
                    }
                    if(embeddedIdx >= 0 && embeddedIdx < static_cast<int>(scene->mNumTextures)){
                        textureHandle = loadEmbeddedTexture(scene->mTextures[embeddedIdx]);
                        if(textureHandle != 0){
                            std::cout << "[Game] Loaded embedded static texture index " << embeddedIdx
                                      << " for mesh part " << meshIdx << std::endl;
                        }
                    }
                } else if(!texName.empty()){
                    std::vector<std::string> candidates;
                    if(!baseDir.empty()) candidates.emplace_back(baseDir + texName);
                    candidates.emplace_back(texName);
                    if(!baseDir.empty()){
                        candidates.emplace_back(baseDir + "../" + texName);
                        candidates.emplace_back(baseDir + "../../" + texName);
                    }
                    int w=0, h=0, channels=0;
                    for(const std::string& candidate : candidates){
                        unsigned char* data = stbi_load(candidate.c_str(), &w, &h, &channels, 4);
                        if(!data) continue;
                        textureHandle = uploadTexture(data, w, h);
                        stbi_image_free(data);
                        if(textureHandle != 0){
                            std::cout << "[Game] Loaded static model texture: " << candidate
                                      << " for mesh part " << meshIdx << std::endl;
                            break;
                        }
                    }
                }
            }
        }

        if(textureHandle == 0){
            textureHandle = createFallbackTexture();
        }

        part.albedoTex = textureHandle;

        outMesh.totalVertexCount += part.vertexCount;
        outMesh.totalIndexCount += part.indexCount;
        if(outMesh.parts.empty()){
            outMesh.minBounds = part.minBounds;
            outMesh.maxBounds = part.maxBounds;
        } else {
            outMesh.minBounds = glm::min(outMesh.minBounds, part.minBounds);
            outMesh.maxBounds = glm::max(outMesh.maxBounds, part.maxBounds);
        }

        outMesh.parts.push_back(std::move(part));
    }

    std::cout << "[Game] Loaded static model: " << path << " ("
              << outMesh.totalVertexCount << " vertices across "
              << outMesh.parts.size() << " mesh parts)" << std::endl;

    return true;
}
