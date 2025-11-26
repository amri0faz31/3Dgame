// Fullscreen triangle sky rendering (Option 2)
#include "Sky.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

// Vertex shader generates a single fullscreen triangle, no VBO needed.
static const char* kSkyVert = R"GLSL(
#version 450 core
out vec2 vUV;
void main(){
    // Fullscreen triangle positions (cover entire screen)
    const vec2 pos[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    gl_Position = vec4(pos[gl_VertexID], 1.0, 1.0);
    vUV = pos[gl_VertexID];
}
)GLSL";

// Fragment reconstructs world direction from inverse view-projection and produces gradient + sun/moon + stars.
static const char* kSkyFrag = R"GLSL(
#version 450 core
in vec2 vUV;
out vec4 FragColor;
uniform mat4 uInvViewProj;
uniform vec3 uSunDir;
uniform vec3 uTopColor;
uniform vec3 uHorizonColor;
uniform vec3 uSunColor;
uniform bool uIsNight;

// Simple hash function for star generation
float hash(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yxz + 19.19);
    return fract((p.x + p.y) * p.z);
}

void main(){
    // Clip-space position at far plane (z=1)
    vec4 clip = vec4(vUV, 1.0, 1.0);
    vec4 world = uInvViewProj * clip;
    world /= world.w;
    vec3 dir = normalize(world.xyz);
    
    // Use upward component for gradient
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    
    vec3 base;
    vec3 celestialColor;
    float celestialGlow;
    float celestialHalo;
    
    if(uIsNight) {
        // Night sky gradient (dark blue to black) - 62.5% darker
        vec3 nightTop = vec3(0.00188, 0.00188, 0.0075);   // Nearly pure black
        vec3 nightHorizon = vec3(0.0075, 0.01125, 0.03);  // Extremely dark blue
        base = mix(nightHorizon, nightTop, pow(t, 0.8));
        
        // Moon (fixed high position in sky, not tied to sun)
        vec3 moonDir = normalize(vec3(0.3, 0.8, -0.5));  // Always above horizon
        float moonDot = max(dot(dir, moonDir), 0.0);
        
        // Moon disc with soft edge
        float moonDisc = smoothstep(0.9985, 0.9995, moonDot);
        vec3 moonColor = vec3(0.9, 0.92, 0.95) * 1.2;  // Pale blue-white
        float moonGlow = exp((moonDot - 1.0) * 80.0) * 0.8;
        float moonHalo = exp((moonDot - 1.0) * 12.0) * 0.3;
        
        celestialColor = moonColor;
        celestialGlow = moonDisc + moonGlow;
        celestialHalo = moonHalo;
        
        // Generate stars (only in upper hemisphere)
        if(dir.y > 0.0) {
            // Scale direction for star field density
            vec3 starCoord = dir * 100.0;
            float starValue = hash(floor(starCoord));
            
            // Create sparse star field
            float starThreshold = 0.985;  // Controls star density
            if(starValue > starThreshold) {
                // Star brightness varies
                float brightness = (starValue - starThreshold) / (1.0 - starThreshold);
                brightness = pow(brightness, 2.0) * 0.8;
                
                // Star twinkle based on position
                float twinkle = hash(starCoord * 0.1) * 0.3 + 0.7;
                
                // Add colored stars (mostly white, some blue/yellow)
                vec3 starColor = mix(vec3(1.0), vec3(0.8, 0.9, 1.0), hash(starCoord * 1.3));
                starColor = mix(starColor, vec3(1.0, 0.95, 0.8), hash(starCoord * 1.7));
                
                base += starColor * brightness * twinkle;
            }
        }
    } else {
        // Day sky gradient
        base = mix(uHorizonColor, uTopColor, pow(t, 1.2));
        
        // Sun
        float sunDot = max(dot(dir, normalize(uSunDir)), 0.0);
        celestialGlow = exp((sunDot - 1.0) * 18.0);
        celestialHalo = exp((sunDot - 1.0) * 4.0);
        celestialColor = uSunColor;
    }
    
    vec3 color = base + celestialColor * (celestialGlow * 1.4 + celestialHalo * 0.25);
    FragColor = vec4(color, 1.0);
}
)GLSL";

bool Sky::init(){
    return m_shader.compile(kSkyVert, kSkyFrag);
}

void Sky::render(const Camera& cam, const glm::vec3& sunDir, bool isNight){
    // Compute inverse view-projection
    glm::mat4 vp = cam.projectionMatrix() * cam.viewMatrix();
    glm::mat4 invVP = glm::inverse(vp);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    m_shader.bind();
    m_shader.setMat4("uInvViewProj", invVP);
    m_shader.setVec3("uSunDir", sunDir);
    m_shader.setBool("uIsNight", isNight);
    m_shader.setVec3("uTopColor", glm::vec3(0.08f,0.20f,0.45f));
    m_shader.setVec3("uHorizonColor", glm::vec3(0.55f,0.70f,0.85f));
    m_shader.setVec3("uSunColor", glm::vec3(1.0f,0.95f,0.80f));
    // No VAO/VBO needed if core profile allows; create minimal state
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
