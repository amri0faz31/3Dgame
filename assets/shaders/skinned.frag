#version 450 core
// Basic lit fragment shader for the character. Swap with your terrain PBR
// shader if you want perfectly matching shading.

in VS_OUT {
    vec2 uv;
    vec3 normal;
    vec3 worldPos;
    vec4 fragPosLightSpace;
} fs_in;

out vec4 FragColor;

uniform sampler2D uAlbedo;
uniform sampler2D uShadowMap;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uCameraPos;
uniform vec3 uSkyColor;
uniform float uFogStart;
uniform float uFogRange;
uniform float uSpecularStrength;
uniform float uShininess;

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.002);
    
    // PCF for softer shadows
    // Stronger shadows at night (0.15 vs 0.3)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    float shadowDarkness = 0.3;  // Can be made uniform for night/day control
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? shadowDarkness : 1.0;
        }
    }
    return shadow / 9.0;
}

void main(){
    vec3 base = texture(uAlbedo, fs_in.uv).rgb;
    vec3 normal = normalize(fs_in.normal);
    vec3 lightDir = normalize(-uLightDir);
    vec3 viewDir = normalize(uCameraPos - fs_in.worldPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), uShininess) * uSpecularStrength;
    
    float shadow = calculateShadow(fs_in.fragPosLightSpace, normal, lightDir);

    vec3 ambient = base * uAmbientColor;
    vec3 diffuse = base * diff * uLightColor * shadow;
    vec3 specular = spec * uLightColor * shadow;
    vec3 color = ambient + diffuse + specular;

    float dist = length(uCameraPos - fs_in.worldPos);
    float fogFactor = clamp((dist - uFogStart) / uFogRange, 0.0, 1.0);
    color = mix(color, uSkyColor, fogFactor);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
