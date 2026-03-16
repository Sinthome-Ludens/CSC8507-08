/**
 * @file AssimpLoader.cpp
 * @brief AssimpLoader 实现：Assimp 到 NCL Mesh 的数据转换
 */

#include "AssimpLoader.h"
#include "Game/Utils/Assert.h"
#include "Game/Utils/Log.h"

// Assimp 头文件（必须在 NCL 头文件之前包含）
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
#include <assimp/color4.h>

// NCL 头文件
#include "OGLMesh.h"
#include "MeshAnimation.h"
#include "Vector.h"
#include "Mesh.h"

#include <unordered_map>

using namespace NCL;
using namespace NCL::Rendering;
using namespace NCL::Maths;

namespace ECS {

// =============================================================================
// 内部辅助函数（文件作用域）
// =============================================================================

/**
 * @brief 转换 aiVector3D 到 NCL::Vector3
 */
static Vector3 ToVector3(const aiVector3D& v) {
    return Vector3(v.x, v.y, v.z);
}

/**
 * @brief 转换 aiVector3D 到 NCL::Vector2（取 x, y 分量）
 */
static Vector2 ToVector2(const aiVector3D& v) {
    return Vector2(v.x, v.y);
}

/**
 * @brief 转换 aiColor4D 到 NCL::Vector4
 */
static Vector4 ToVector4(const aiColor4D& c) {
    return Vector4(c.r, c.g, c.b, c.a);
}

/**
 * @brief 转换单个 aiMesh 到 OGLMesh
 */
static OGLMesh* ConvertMesh(const aiMesh* aiMesh);

// =============================================================================
// 公共接口
// =============================================================================

OGLMesh* AssimpLoader::LoadMesh(const std::string& path) {
    LOG_INFO("[AssimpLoader] Loading mesh: " << path);

    // 创建 Assimp 导入器
    Assimp::Importer importer;

    // 配置后处理标志
    unsigned int flags =
        aiProcess_Triangulate |           // 三角化所有面
        aiProcess_GenNormals |            // 生成法线（如果缺失）
        aiProcess_CalcTangentSpace |      // 计算切线空间
        aiProcess_FlipUVs |               // 翻转 UV（OpenGL 约定：原点在左下角）
        aiProcess_JoinIdenticalVertices | // 合并重复顶点
        aiProcess_OptimizeMeshes |        // 优化网格
        aiProcess_SortByPType;            // 按图元类型排序

    // 导入场景
    const aiScene* scene = importer.ReadFile(path, flags);

    // 错误检查
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("[AssimpLoader] Assimp Error: " << importer.GetErrorString());
        return nullptr;
    }

    // 检查是否有网格
    if (scene->mNumMeshes == 0) {
        LOG_WARN("[AssimpLoader] No meshes found in " << path);
        return nullptr;
    }

    // 转换第一个网格
    OGLMesh* mesh = ConvertMesh(scene->mMeshes[0]);

    if (mesh) {
        LOG_INFO("[AssimpLoader] Successfully loaded mesh: " << path
                  << " (Vertices: " << mesh->GetVertexCount()
                  << ", Indices: " << mesh->GetIndexCount() << ")");
    }

    return mesh;
}

std::vector<OGLMesh*> AssimpLoader::LoadScene(const std::string& path) {
    LOG_INFO("[AssimpLoader] Loading scene: " << path);

    std::vector<OGLMesh*> meshes;

    // 创建 Assimp 导入器
    Assimp::Importer importer;

    // 配置后处理标志
    unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_SortByPType;

    // 导入场景
    const aiScene* scene = importer.ReadFile(path, flags);

    // 错误检查
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("[AssimpLoader] Assimp Error: " << importer.GetErrorString());
        return meshes;
    }

    // 转换所有网格
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        OGLMesh* mesh = ConvertMesh(scene->mMeshes[i]);
        if (mesh) {
            meshes.push_back(mesh);
        }
    }

    LOG_INFO("[AssimpLoader] Successfully loaded " << meshes.size()
              << " meshes from " << path);

    return meshes;
}

// =============================================================================
// LoadCollisionGeometry — 仅提取碰撞用三角网格（不上传 GPU）
// =============================================================================
bool AssimpLoader::LoadCollisionGeometry(
    const std::string& path,
    std::vector<NCL::Maths::Vector3>& outVertices,
    std::vector<int>& outIndices)
{
    Assimp::Importer importer;

    // 碰撞数据只需三角化和合并重复顶点，不需要法线/UV/切线
    unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_WARN("[AssimpLoader] LoadCollisionGeometry failed: " << importer.GetErrorString());
        return false;
    }

    if (scene->mNumMeshes == 0) {
        LOG_WARN("[AssimpLoader] No meshes found in " << path);
        return false;
    }

    outVertices.clear();
    outIndices.clear();

    // 合并所有子网格到一个碰撞体
    int vertexOffset = 0;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* aiM = scene->mMeshes[m];

        for (unsigned int i = 0; i < aiM->mNumVertices; ++i) {
            outVertices.emplace_back(aiM->mVertices[i].x,
                                     aiM->mVertices[i].y,
                                     aiM->mVertices[i].z);
        }

        for (unsigned int i = 0; i < aiM->mNumFaces; ++i) {
            const aiFace& face = aiM->mFaces[i];
            if (face.mNumIndices == 3) {
                outIndices.push_back(static_cast<int>(face.mIndices[0]) + vertexOffset);
                outIndices.push_back(static_cast<int>(face.mIndices[1]) + vertexOffset);
                outIndices.push_back(static_cast<int>(face.mIndices[2]) + vertexOffset);
            }
        }

        vertexOffset += static_cast<int>(aiM->mNumVertices);
    }

    LOG_INFO("[AssimpLoader] LoadCollisionGeometry: " << path
             << " (" << outVertices.size() << " verts, "
             << outIndices.size() / 3 << " tris)");
    return true;
}

// =============================================================================
// 私有辅助函数实现
// =============================================================================

static OGLMesh* ConvertMesh(const aiMesh* aiMesh) {
    if (!aiMesh) {
        return nullptr;
    }

    // 创建 NCL 网格
    OGLMesh* mesh = new OGLMesh();

    // 1. 转换顶点位置（必需）
    std::vector<Vector3> positions;
    positions.reserve(aiMesh->mNumVertices);
    for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
        positions.push_back(ToVector3(aiMesh->mVertices[i]));
    }
    mesh->SetVertexPositions(positions);

    // 2. 转换法线（如果存在）
    if (aiMesh->HasNormals()) {
        std::vector<Vector3> normals;
        normals.reserve(aiMesh->mNumVertices);
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            normals.push_back(ToVector3(aiMesh->mNormals[i]));
        }
        mesh->SetVertexNormals(normals);
    }

    // 3. 转换纹理坐标（如果存在）
    if (aiMesh->HasTextureCoords(0)) {
        std::vector<Vector2> texCoords;
        texCoords.reserve(aiMesh->mNumVertices);
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            texCoords.push_back(ToVector2(aiMesh->mTextureCoords[0][i]));
        }
        mesh->SetVertexTextureCoords(texCoords);
    }

    // 4. 转换顶点颜色（如果存在）
    if (aiMesh->HasVertexColors(0)) {
        std::vector<Vector4> colours;
        colours.reserve(aiMesh->mNumVertices);
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            colours.push_back(ToVector4(aiMesh->mColors[0][i]));
        }
        mesh->SetVertexColours(colours);
    }

    // 5. 转换切线（如果存在）
    if (aiMesh->HasTangentsAndBitangents()) {
        std::vector<Vector4> tangents;
        tangents.reserve(aiMesh->mNumVertices);
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            // NCL 使用 Vector4 存储切线（xyz = 切线方向，w = 副切线符号）
            Vector3 tangent = ToVector3(aiMesh->mTangents[i]);
            Vector3 bitangent = ToVector3(aiMesh->mBitangents[i]);
            Vector3 normal = ToVector3(aiMesh->mNormals[i]);

            // 计算副切线符号（用于重建副切线）
            float handedness = Vector::Dot(Vector::Cross(normal, tangent), bitangent) > 0.0f ? 1.0f : -1.0f;

            tangents.push_back(Vector4(tangent.x, tangent.y, tangent.z, handedness));
        }
        mesh->SetVertexTangents(tangents);
    }

    // 6. 转换索引（必需）
    std::vector<unsigned int> indices;
    for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
        aiFace face = aiMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    mesh->SetVertexIndices(indices);

    // 7. 设置图元类型（Assimp 已三角化，所以总是 Triangles）
    mesh->SetPrimitiveType(GeometryPrimitive::Triangles);

    // 8. 上传到 GPU
    mesh->UploadToGPU();

    return mesh;
}

// =============================================================================
// LoadAnimation — 提取骨骼动画帧数据（第一个动画剪辑）
// =============================================================================
// 遍历 aiAnimation，将每帧关节世界矩阵（inverseBindPose × localToWorld）
// 存入 MeshAnimation::allJoints 中，帧步长 = 1/ticksPerSecond。
// 若 meshToFill != nullptr，同时填充骨骼绑定信息。
MeshAnimation* AssimpLoader::LoadAnimation(const std::string& path, OGLMesh* meshToFill) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_LimitBoneWeights);

    if (!scene || !(scene->mFlags & AI_SCENE_FLAGS_NON_VERBOSE_FORMAT == 0)) {
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            LOG_ERROR("[AssimpLoader] LoadAnimation failed: " << importer.GetErrorString());
            return nullptr;
        }
    }

    if (scene->mNumAnimations == 0) {
        LOG_WARN("[AssimpLoader] No animations found in: " << path);
        return nullptr;
    }

    const aiAnimation* anim = scene->mAnimations[0];
    const size_t jointCount = meshToFill ? meshToFill->GetBindPose().size() : 0;
    const float  ticksPerSec = (anim->mTicksPerSecond > 0.0) ? (float)anim->mTicksPerSecond : 25.0f;
    const size_t frameCount  = (size_t)anim->mDuration + 1;

    // 构建关节名→索引映射
    std::unordered_map<std::string, int> jointIndexMap;
    if (meshToFill) {
        // 需要读取 jointNames，但 Mesh 没有公开此接口；改从 Assimp mesh 构建
        for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
            const aiMesh* aiMesh = scene->mMeshes[m];
            for (unsigned int b = 0; b < aiMesh->mNumBones; b++) {
                std::string boneName(aiMesh->mBones[b]->mName.C_Str());
                if (jointIndexMap.find(boneName) == jointIndexMap.end()) {
                    int idx = (int)jointIndexMap.size();
                    jointIndexMap[boneName] = idx;
                }
            }
        }
    }

    // 若需要填充 meshToFill 的骨骼绑定信息
    if (meshToFill && scene->mNumMeshes > 0) {
        const aiMesh* aiMesh = scene->mMeshes[0];
        if (aiMesh->HasBones()) {
            std::vector<Matrix4> bindPose(aiMesh->mNumBones);
            std::vector<Matrix4> invBind(aiMesh->mNumBones);
            std::vector<std::string> jointNames(aiMesh->mNumBones);
            std::vector<int> jointParents(aiMesh->mNumBones, -1);

            std::vector<Vector4>  skinWeights(aiMesh->mNumVertices, Vector4(0.0f, 0.0f, 0.0f, 0.0f));
            std::vector<Vector4i> skinIndices(aiMesh->mNumVertices, Vector4i(0, 0, 0, 0));
            std::vector<int>      weightCount(aiMesh->mNumVertices, 0);

            for (unsigned int b = 0; b < aiMesh->mNumBones; b++) {
                const aiBone* bone = aiMesh->mBones[b];
                jointNames[b] = bone->mName.C_Str();

                // Inverse bind-pose matrix
                aiMatrix4x4 im = bone->mOffsetMatrix;
                {
                    Matrix4 m;
                    m.array[0][0] = im.a1; m.array[1][0] = im.a2; m.array[2][0] = im.a3; m.array[3][0] = im.a4;
                    m.array[0][1] = im.b1; m.array[1][1] = im.b2; m.array[2][1] = im.b3; m.array[3][1] = im.b4;
                    m.array[0][2] = im.c1; m.array[1][2] = im.c2; m.array[2][2] = im.c3; m.array[3][2] = im.c4;
                    m.array[0][3] = im.d1; m.array[1][3] = im.d2; m.array[2][3] = im.d3; m.array[3][3] = im.d4;
                    invBind[b] = m;
                }

                // Skin weights
                for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                    unsigned int vi  = bone->mWeights[w].mVertexId;
                    float        wt  = bone->mWeights[w].mWeight;
                    int          cnt = weightCount[vi];
                    if (cnt < 4) {
                        skinWeights[vi][cnt] = wt;
                        skinIndices[vi][cnt] = (int)b;
                        weightCount[vi]++;
                    }
                }
            }

            meshToFill->SetJointNames(jointNames);
            meshToFill->SetJointParents(jointParents);
            meshToFill->SetBindPose(bindPose);
            meshToFill->SetInverseBindPose(invBind);
            meshToFill->SetVertexSkinWeights(skinWeights);
            meshToFill->SetVertexSkinIndices(skinIndices);
        }
    }

    // 采样关节世界矩阵到 allJoints
    size_t numJoints = jointIndexMap.empty() ? 1 : jointIndexMap.size();
    std::vector<Matrix4> allJoints(frameCount * numJoints);

    // 构建节点→本地变换（简化版：对每帧采样每个通道）
    for (size_t frame = 0; frame < frameCount; frame++) {
        float tick = (float)frame;
        for (unsigned int c = 0; c < anim->mNumChannels; c++) {
            const aiNodeAnim* channel = anim->mChannels[c];
            std::string nodeName(channel->mNodeName.C_Str());
            auto it = jointIndexMap.find(nodeName);
            if (it == jointIndexMap.end()) continue;
            int jIdx = it->second;

            // 位置（最近帧）
            aiVector3D pos(0, 0, 0);
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                if (channel->mPositionKeys[k].mTime <= tick) pos = channel->mPositionKeys[k].mValue;
                else break;
            }
            // 旋转（最近帧）
            aiQuaternion rot(1, 0, 0, 0);
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
                if (channel->mRotationKeys[k].mTime <= tick) rot = channel->mRotationKeys[k].mValue;
                else break;
            }
            // 缩放（最近帧）
            aiVector3D scl(1, 1, 1);
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++) {
                if (channel->mScalingKeys[k].mTime <= tick) scl = channel->mScalingKeys[k].mValue;
                else break;
            }

            aiMatrix4x4 mat;
            aiMatrix4x4::Translation(pos, mat);
            aiMatrix4x4 rotMat(rot.GetMatrix());
            mat = mat * rotMat;
            aiMatrix4x4 sclMat;
            aiMatrix4x4::Scaling(scl, sclMat);
            mat = mat * sclMat;

            int frameOffset = (int)(frame * numJoints + jIdx);
            {
                Matrix4 m;
                m.array[0][0] = mat.a1; m.array[1][0] = mat.a2; m.array[2][0] = mat.a3; m.array[3][0] = mat.a4;
                m.array[0][1] = mat.b1; m.array[1][1] = mat.b2; m.array[2][1] = mat.b3; m.array[3][1] = mat.b4;
                m.array[0][2] = mat.c1; m.array[1][2] = mat.c2; m.array[2][2] = mat.c3; m.array[3][2] = mat.c4;
                m.array[0][3] = mat.d1; m.array[1][3] = mat.d2; m.array[2][3] = mat.d3; m.array[3][3] = mat.d4;
                allJoints[frameOffset] = m;
            }
        }
    }

    MeshAnimation* result = new MeshAnimation(numJoints, frameCount, ticksPerSec, allJoints);
    LOG_INFO("[AssimpLoader] LoadAnimation OK: " << path
             << " frames=" << frameCount << " joints=" << numJoints);
    return result;
}

} // namespace ECS
