#include "CharacterImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <limits>
#include <stb_image.h>

namespace {
static constexpr int kMaxBoneInfluences = 4;
static constexpr unsigned int kImportFlags =
    aiProcess_Triangulate |
    aiProcess_GenSmoothNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_LimitBoneWeights |
    aiProcess_JoinIdenticalVertices;

GLuint uploadTexture2D(const unsigned char* pixels, int width, int height){
    if(!pixels || width <= 0 || height <= 0) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}
}

SkinnedMesh CharacterImporter::load(const std::string& path){
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, kImportFlags);
    if(!scene || !scene->mRootNode || scene->mNumMeshes == 0){
        throw std::runtime_error("[CharacterImporter] Failed to load " + path);
    }

    SkinnedMesh result;
    std::string directory;
    size_t slash = path.find_last_of("/\\");
    if(slash != std::string::npos){
        directory = path.substr(0, slash + 1);
    }
    processMesh(scene->mMeshes[0], result);
    extractSkeleton(scene, result);
    extractAnimations(scene, result);
    extractMaterial(scene, scene->mMeshes[0], directory, result);
    return result;
}

void CharacterImporter::processMesh(const aiMesh* mesh, SkinnedMesh& outMesh){
    std::vector<SkinnedVertex> vertices(mesh->mNumVertices);
    std::vector<uint32_t> indices;
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    auto zeroBoneData = [](SkinnedVertex& v){
        v.boneIds = glm::uvec4(0);
        v.boneWeights = glm::vec4(0.0f);
    };

    for(unsigned int i=0; i<mesh->mNumVertices; ++i){
        SkinnedVertex v{};
        v.position = glm::make_vec3(&mesh->mVertices[i].x);
        v.normal = glm::make_vec3(&mesh->mNormals[i].x);
        v.uv = mesh->mTextureCoords[0] ?
            glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) :
            glm::vec2(0.0f);
        zeroBoneData(v);
        minBounds.x = std::min(minBounds.x, v.position.x);
        minBounds.y = std::min(minBounds.y, v.position.y);
        minBounds.z = std::min(minBounds.z, v.position.z);
        maxBounds.x = std::max(maxBounds.x, v.position.x);
        maxBounds.y = std::max(maxBounds.y, v.position.y);
        maxBounds.z = std::max(maxBounds.z, v.position.z);
        vertices[i] = v;
    }

    outMesh.bones.resize(mesh->mNumBones);
    outMesh.boneNames.resize(mesh->mNumBones);
    outMesh.boneLookup.reserve(mesh->mNumBones);
    for(unsigned int boneIdx=0; boneIdx<mesh->mNumBones; ++boneIdx){
        aiBone* bone = mesh->mBones[boneIdx];
        outMesh.bones[boneIdx].offset = glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
        outMesh.boneLookup[bone->mName.C_Str()] = static_cast<int>(boneIdx);
        outMesh.boneNames[boneIdx] = bone->mName.C_Str();

        for(unsigned int weightIdx=0; weightIdx<bone->mNumWeights; ++weightIdx){
            const aiVertexWeight& weight = bone->mWeights[weightIdx];
            SkinnedVertex& v = vertices[weight.mVertexId];
            for(int slot=0; slot<kMaxBoneInfluences; ++slot){
                if(v.boneWeights[slot] == 0.0f){
                    v.boneIds[slot] = boneIdx;
                    v.boneWeights[slot] = weight.mWeight;
                    break;
                }
            }
        }
    }

    for(auto& v : vertices){
        float sum = v.boneWeights.x + v.boneWeights.y + v.boneWeights.z + v.boneWeights.w;
        if(sum > 0.0f){
            v.boneWeights /= sum;
        } else {
            v.boneWeights = glm::vec4(0.0f);
        }
    }

    for(unsigned int f=0; f<mesh->mNumFaces; ++f){
        const aiFace& face = mesh->mFaces[f];
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    uploadBuffers(outMesh, vertices, indices);
    outMesh.minBounds = minBounds;
    outMesh.maxBounds = maxBounds;
}

void CharacterImporter::uploadBuffers(SkinnedMesh& mesh,
                                      const std::vector<SkinnedVertex>& vertices,
                                      const std::vector<uint32_t>& indices){
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ibo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, boneIds));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (void*)offsetof(SkinnedVertex, boneWeights));
    glBindVertexArray(0);

    mesh.indexCount = static_cast<unsigned int>(indices.size());
}

void CharacterImporter::extractSkeleton(const aiScene* scene, SkinnedMesh& outMesh){
    outMesh.boneParents.resize(outMesh.bones.size(), -1);

    std::unordered_map<std::string, const aiNode*> nodeMap;
    std::function<void(const aiNode*)> buildMap = [&](const aiNode* node){
        nodeMap[node->mName.C_Str()] = node;
        for(unsigned int i=0; i<node->mNumChildren; ++i){
            buildMap(node->mChildren[i]);
        }
    };
    buildMap(scene->mRootNode);

    for(const auto& pair : outMesh.boneLookup){
        const std::string& boneName = pair.first;
        int boneIndex = pair.second;
        const aiNode* node = nodeMap[boneName];
        const aiNode* parent = node ? node->mParent : nullptr;
        while(parent && outMesh.boneLookup.find(parent->mName.C_Str()) == outMesh.boneLookup.end()){
            parent = parent->mParent;
        }
        if(parent){
            outMesh.boneParents[boneIndex] = outMesh.boneLookup[parent->mName.C_Str()];
            outMesh.bones[boneIndex].parentIndex = outMesh.boneParents[boneIndex];
        }
    }

    auto toLower = [](const std::string& s){
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return out;
    };
    for(size_t i=0; i<outMesh.boneNames.size(); ++i){
        std::string lower = toLower(outMesh.boneNames[i]);
        if(outMesh.headBone == -1 && lower.find("head") != std::string::npos){
            outMesh.headBone = static_cast<int>(i);
        }
        if(outMesh.leftFootBone == -1 && (lower.find("leftfoot") != std::string::npos || lower.find("foot_l") != std::string::npos || lower.find("l_foot") != std::string::npos)){
            outMesh.leftFootBone = static_cast<int>(i);
        }
        if(outMesh.rightFootBone == -1 && (lower.find("rightfoot") != std::string::npos || lower.find("foot_r") != std::string::npos || lower.find("r_foot") != std::string::npos)){
            outMesh.rightFootBone = static_cast<int>(i);
        }
    }
}

void CharacterImporter::extractAnimations(const aiScene* scene, SkinnedMesh& outMesh){
    outMesh.clips.reserve(scene->mNumAnimations);
    for(unsigned int a=0; a<scene->mNumAnimations; ++a){
        const aiAnimation* anim = scene->mAnimations[a];
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = anim->mDuration;
        clip.ticksPerSecond = anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 25.0;

        for(unsigned int c=0; c<anim->mNumChannels; ++c){
            const aiNodeAnim* channel = anim->mChannels[c];
            AnimationChannel ch;
            ch.boneName = channel->mNodeName.C_Str();
            for(unsigned int i=0; i<channel->mNumPositionKeys; ++i){
                ch.positionKeys.emplace_back(channel->mPositionKeys[i].mTime,
                                             glm::make_vec3(&channel->mPositionKeys[i].mValue.x));
            }
            for(unsigned int i=0; i<channel->mNumRotationKeys; ++i){
                const aiQuatKey& q = channel->mRotationKeys[i];
                ch.rotationKeys.emplace_back(q.mTime,
                                             glm::quat(q.mValue.w, q.mValue.x, q.mValue.y, q.mValue.z));
            }
            for(unsigned int i=0; i<channel->mNumScalingKeys; ++i){
                ch.scaleKeys.emplace_back(channel->mScalingKeys[i].mTime,
                                          glm::make_vec3(&channel->mScalingKeys[i].mValue.x));
            }
            clip.channels[ch.boneName] = std::move(ch);
        }
        outMesh.clips.push_back(std::move(clip));
    }
}

void CharacterImporter::extractMaterial(const aiScene* scene,
                                        const aiMesh* mesh,
                                        const std::string& sourceDir,
                                        SkinnedMesh& outMesh){
    if(!scene || !mesh || scene->mNumMaterials == 0) return;
    if(mesh->mMaterialIndex >= scene->mNumMaterials) return;
    const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    aiString texPath;
    if(material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) != AI_SUCCESS){
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
    }
    if(texPath.length == 0) return;

    auto createFromEmbedded = [&](const aiTexture* tex)->GLuint{
        if(!tex) return 0u;
        if(tex->mHeight == 0){
            int width = 0, height = 0, channels = 0;
            const stbi_uc* raw = reinterpret_cast<const stbi_uc*>(tex->pcData);
            stbi_uc* decoded = stbi_load_from_memory(raw,
                                                     static_cast<int>(tex->mWidth),
                                                     &width,
                                                     &height,
                                                     &channels,
                                                     STBI_rgb_alpha);
            if(!decoded) return 0u;
            GLuint glTex = uploadTexture2D(decoded, width, height);
            stbi_image_free(decoded);
            return glTex;
        }
        std::vector<unsigned char> pixels(tex->mWidth * tex->mHeight * 4);
        for(unsigned int i=0; i<tex->mWidth * tex->mHeight; ++i){
            pixels[i * 4 + 0] = tex->pcData[i].r;
            pixels[i * 4 + 1] = tex->pcData[i].g;
            pixels[i * 4 + 2] = tex->pcData[i].b;
            pixels[i * 4 + 3] = tex->pcData[i].a;
        }
        return uploadTexture2D(pixels.data(), tex->mWidth, tex->mHeight);
    };

    if(texPath.length > 0 && texPath.C_Str()[0] == '*'){
        const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str());
        outMesh.albedoTex = createFromEmbedded(embedded);
        return;
    }

    std::string fullPath = sourceDir + texPath.C_Str();
    int width = 0, height = 0, channels = 0;
    stbi_uc* decoded = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if(decoded){
        outMesh.albedoTex = uploadTexture2D(decoded, width, height);
        stbi_image_free(decoded);
    }
}
