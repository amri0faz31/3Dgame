#version 450 core

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in uvec4 inBoneIDs;
layout(location=4) in vec4 inWeights;

layout(std140, binding=0) uniform Bones {
    mat4 uBones[128];
};

uniform bool uUseSkinning;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uLightSpace;

out VS_OUT {
    vec2 uv;
    vec3 normal;
    vec3 worldPos;
    vec4 fragPosLightSpace;
} vs_out;

void main() {
    vec4 localPos = vec4(inPos, 1.0);
    vec3 localNormal = inNormal;

    if(uUseSkinning){
        mat4 skinMat = mat4(0.0);
        for(int i=0; i<4; ++i){
            uint id = inBoneIDs[i];
            float w = inWeights[i];
            if(w > 0.0){
                skinMat += uBones[id] * w;
            }
        }
        localPos = skinMat * localPos;
        localNormal = mat3(skinMat) * inNormal;
    }

    vec4 worldPos = uModel * localPos;
    gl_Position = uProj * uView * worldPos;

    vs_out.uv = inUV;
    vs_out.normal = normalize(mat3(uModel) * localNormal);
    vs_out.worldPos = worldPos.xyz;
    vs_out.fragPosLightSpace = uLightSpace * worldPos;
}
