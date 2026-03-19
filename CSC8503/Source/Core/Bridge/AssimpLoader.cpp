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
#include "MeshAnimation.h"
#include "Vector.h"
#include "Mesh.h"

#include <unordered_map>
#include <queue>

using namespace NCL;
using namespace NCL::Rendering;
using namespace NCL::Maths;

namespace ECS {

AssimpLoader::MeshFactory AssimpLoader::s_MeshFactory = nullptr;

void AssimpLoader::SetMeshFactory(MeshFactory factory) {
    s_MeshFactory = std::move(factory);
}

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
 * @brief 转换单个 aiMesh 到 Mesh
 */
static Mesh* ConvertMesh(const aiMesh* aiMesh);

/**
 * @brief 判断 aiMesh 是否为数据标记网格（不应渲染）
 *
 * Unity 导出的 OBJ 可能包含 PatrolPoints、EnemySpawnPoints 等仅提供坐标的
 * 占位几何体。这些网格名称包含特定前缀，应在加载时过滤掉。
 */
static bool IsMarkerMesh(const aiMesh* m) {
    if (!m || m->mName.length == 0) return false;
    std::string name(m->mName.C_Str());
    return name.find("PatrolPoint") != std::string::npos
        || name.find("EnemySpawn")  != std::string::npos
        || name.find("SpawnPoint")  != std::string::npos;
}

/**
 * @brief 合并多个 aiMesh 到一个 Mesh（跳过标记网格）
 */
static Mesh* MergeMeshes(const aiScene* scene);

// =============================================================================
// 公共接口
// =============================================================================

Mesh* AssimpLoader::LoadMesh(const std::string& path) {
    GAME_ASSERT(s_MeshFactory, "[AssimpLoader] MeshFactory not set! Call SetMeshFactory() first.");
    LOG_INFO("[AssimpLoader] Loading mesh: " << path);

    // 创建 Assimp 导入器
    Assimp::Importer importer;

    // 配置后处理标志
    // 注意：不使用 aiProcess_OptimizeMeshes，以保留各 group 的名称信息，
    // 从而在合并时过滤掉 PatrolPoints 等数据标记网格。
    unsigned int flags =
        aiProcess_Triangulate |           // 三角化所有面
        aiProcess_GenNormals |            // 生成法线（如果缺失）
        aiProcess_CalcTangentSpace |      // 计算切线空间
        aiProcess_FlipUVs |               // 翻转 UV（OpenGL 约定：原点在左下角）
        aiProcess_JoinIdenticalVertices | // 合并重复顶点
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

    // 合并所有非标记网格
    Mesh* mesh = MergeMeshes(scene);

    if (mesh) {
        LOG_INFO("[AssimpLoader] Successfully loaded mesh: " << path
                  << " (Vertices: " << mesh->GetVertexCount()
                  << ", Indices: " << mesh->GetIndexCount() << ")");
    }

    return mesh;
}

std::vector<Mesh*> AssimpLoader::LoadScene(const std::string& path) {
    GAME_ASSERT(s_MeshFactory, "[AssimpLoader] LoadScene: MeshFactory not set. Call SetMeshFactory() first.");
    LOG_INFO("[AssimpLoader] Loading scene: " << path);

    std::vector<Mesh*> meshes;

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
        Mesh* mesh = ConvertMesh(scene->mMeshes[i]);
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
    // 不使用 aiProcess_OptimizeMeshes 以保留名称信息用于标记过滤
    unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices;

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

    // 合并所有子网格到一个碰撞体（跳过标记网格）
    int vertexOffset = 0;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* aiM = scene->mMeshes[m];
        if (IsMarkerMesh(aiM)) continue;

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
// MergeMeshes — 合并所有非标记 aiMesh 到单个 OGLMesh
// =============================================================================

static Mesh* MergeMeshes(const aiScene* scene) {
    std::vector<Vector3>       positions;
    std::vector<Vector3>       normals;
    std::vector<Vector2>       texCoords;
    std::vector<Vector4>       colours;
    std::vector<Vector4>       tangents;
    std::vector<unsigned int>  indices;

    bool hasNormals   = false;
    bool hasTexCoords = false;
    bool hasColours   = false;
    bool hasTangents  = false;

    unsigned int vertexOffset = 0;
    int skipped = 0;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* aiM = scene->mMeshes[m];
        if (IsMarkerMesh(aiM)) {
            LOG_INFO("[AssimpLoader] Skipping marker mesh: " << aiM->mName.C_Str());
            ++skipped;
            continue;
        }

        // Positions
        for (unsigned int i = 0; i < aiM->mNumVertices; ++i) {
            positions.push_back(ToVector3(aiM->mVertices[i]));
        }

        // Normals (pad with zero if sub-mesh lacks them)
        if (aiM->HasNormals()) {
            hasNormals = true;
            for (unsigned int i = 0; i < aiM->mNumVertices; ++i)
                normals.push_back(ToVector3(aiM->mNormals[i]));
        } else if (hasNormals) {
            normals.resize(positions.size(), Vector3(0, 1, 0));
        }

        // Texture coords (pad with zero)
        if (aiM->HasTextureCoords(0)) {
            hasTexCoords = true;
            for (unsigned int i = 0; i < aiM->mNumVertices; ++i)
                texCoords.push_back(ToVector2(aiM->mTextureCoords[0][i]));
        } else if (hasTexCoords) {
            texCoords.resize(positions.size(), Vector2(0, 0));
        }

        // Vertex colours (pad with white)
        if (aiM->HasVertexColors(0)) {
            hasColours = true;
            for (unsigned int i = 0; i < aiM->mNumVertices; ++i)
                colours.push_back(ToVector4(aiM->mColors[0][i]));
        } else if (hasColours) {
            colours.resize(positions.size(), Vector4(1, 1, 1, 1));
        }

        // Tangents (pad with zero)
        if (aiM->HasTangentsAndBitangents()) {
            hasTangents = true;
            for (unsigned int i = 0; i < aiM->mNumVertices; ++i) {
                Vector3 t = ToVector3(aiM->mTangents[i]);
                Vector3 b = ToVector3(aiM->mBitangents[i]);
                Vector3 n = ToVector3(aiM->mNormals[i]);
                float h = Vector::Dot(Vector::Cross(n, t), b) > 0.0f ? 1.0f : -1.0f;
                tangents.push_back(Vector4(t.x, t.y, t.z, h));
            }
        } else if (hasTangents) {
            tangents.resize(positions.size(), Vector4(0, 0, 0, 1));
        }

        // Indices (offset by accumulated vertex count)
        for (unsigned int i = 0; i < aiM->mNumFaces; ++i) {
            const aiFace& face = aiM->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j] + vertexOffset);
            }
        }

        vertexOffset += aiM->mNumVertices;
    }

    if (positions.empty()) return nullptr;
    if (skipped > 0) {
        LOG_INFO("[AssimpLoader] Filtered " << skipped << " marker mesh(es)");
    }

    Mesh* mesh = AssimpLoader::s_MeshFactory();
    mesh->SetVertexPositions(positions);
    if (hasNormals)   mesh->SetVertexNormals(normals);
    if (hasTexCoords) mesh->SetVertexTextureCoords(texCoords);
    if (hasColours)   mesh->SetVertexColours(colours);
    if (hasTangents)  mesh->SetVertexTangents(tangents);
    mesh->SetVertexIndices(indices);
    mesh->SetPrimitiveType(GeometryPrimitive::Triangles);
    mesh->UploadToGPU();
    return mesh;
}

// =============================================================================
// 私有辅助函数实现
// =============================================================================

static Mesh* ConvertMesh(const aiMesh* aiMesh) {
    if (!aiMesh) {
        return nullptr;
    }

    // 创建 NCL 网格
    Mesh* mesh = AssimpLoader::s_MeshFactory();

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
MeshAnimation* AssimpLoader::LoadAnimation(const std::string& path, Mesh* meshToFill) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_LimitBoneWeights);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("[AssimpLoader] LoadAnimation failed: " << importer.GetErrorString());
        return nullptr;
    }

    if (scene->mNumAnimations == 0) {
        LOG_WARN("[AssimpLoader] No animations found in: " << path);
        return nullptr;
    }

    const aiAnimation* anim = scene->mAnimations[0];
    const size_t jointCount = meshToFill ? meshToFill->GetBindPose().size() : 0;
    const float  ticksPerSec = (anim->mTicksPerSecond > 0.0) ? (float)anim->mTicksPerSecond : 25.0f;
    const size_t frameCount  = (size_t)anim->mDuration + 1;

    // 构建关节名→索引映射（始终从动画通道构建，确保 meshToFill==nullptr 时也能正确匹配骨骼）
    std::unordered_map<std::string, int> jointIndexMap;
    for (unsigned int ch = 0; ch < anim->mNumChannels; ch++) {
        std::string chanName(anim->mChannels[ch]->mNodeName.C_Str());
        if (jointIndexMap.find(chanName) == jointIndexMap.end()) {
            jointIndexMap[chanName] = (int)jointIndexMap.size();
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

            // 从 aiScene 节点树构建 jointParents
            {
                std::unordered_map<std::string, int> boneNameToIdx;
                for (unsigned int b = 0; b < aiMesh->mNumBones; b++) {
                    boneNameToIdx[jointNames[b]] = (int)b;
                }
                // BFS 遍历节点树
                std::queue<const aiNode*> bfsQueue;
                bfsQueue.push(scene->mRootNode);
                while (!bfsQueue.empty()) {
                    const aiNode* node = bfsQueue.front();
                    bfsQueue.pop();
                    std::string nodeName(node->mName.C_Str());
                    auto parentIt = boneNameToIdx.find(nodeName);
                    for (unsigned int c = 0; c < node->mNumChildren; c++) {
                        const aiNode* child = node->mChildren[c];
                        std::string childName(child->mName.C_Str());
                        auto childIt = boneNameToIdx.find(childName);
                        if (childIt != boneNameToIdx.end() && parentIt != boneNameToIdx.end()) {
                            jointParents[childIt->second] = parentIt->second;
                        }
                        bfsQueue.push(child);
                    }
                }
            }

            meshToFill->SetJointNames(jointNames);
            meshToFill->SetJointParents(jointParents);
            meshToFill->SetBindPose(bindPose);
            meshToFill->SetInverseBindPose(invBind);
            meshToFill->SetVertexSkinWeights(skinWeights);
            meshToFill->SetVertexSkinIndices(skinIndices);
            meshToFill->UploadSkinBuffers();
        }
    }

    // 采样关节世界矩阵到 allJoints
    size_t numJoints = jointIndexMap.empty() ? 1 : jointIndexMap.size();
    std::vector<Matrix4> allJoints(frameCount * numJoints);

    // 构建节点树的默认本地变换 + 父子关系（用于层级累乘）
    struct NodeInfo {
        std::string name;
        int         parentIdx = -1;   // index into nodeList
        Matrix4     defaultLocal;     // 节点默认本地变换
        int         jointIdx  = -1;   // jointIndexMap 中的索引，-1 表示非骨骼中间节点
    };
    std::vector<NodeInfo> nodeList;
    std::unordered_map<std::string, int> nodeNameToListIdx;

    // BFS 构建扁平化节点列表（保证 parent 在 child 之前 = 拓扑序）
    {
        std::queue<std::pair<const aiNode*, int>> bfs;
        bfs.push({scene->mRootNode, -1});
        while (!bfs.empty()) {
            auto [node, parentListIdx] = bfs.front();
            bfs.pop();
            int curIdx = (int)nodeList.size();
            NodeInfo info;
            info.name = node->mName.C_Str();
            info.parentIdx = parentListIdx;
            // 默认本地变换
            aiMatrix4x4 t = node->mTransformation;
            info.defaultLocal.array[0][0] = t.a1; info.defaultLocal.array[1][0] = t.a2; info.defaultLocal.array[2][0] = t.a3; info.defaultLocal.array[3][0] = t.a4;
            info.defaultLocal.array[0][1] = t.b1; info.defaultLocal.array[1][1] = t.b2; info.defaultLocal.array[2][1] = t.b3; info.defaultLocal.array[3][1] = t.b4;
            info.defaultLocal.array[0][2] = t.c1; info.defaultLocal.array[1][2] = t.c2; info.defaultLocal.array[2][2] = t.c3; info.defaultLocal.array[3][2] = t.c4;
            info.defaultLocal.array[0][3] = t.d1; info.defaultLocal.array[1][3] = t.d2; info.defaultLocal.array[2][3] = t.d3; info.defaultLocal.array[3][3] = t.d4;
            auto jit = jointIndexMap.find(info.name);
            info.jointIdx = (jit != jointIndexMap.end()) ? jit->second : -1;
            nodeList.push_back(info);
            nodeNameToListIdx[info.name] = curIdx;
            for (unsigned int c = 0; c < node->mNumChildren; c++) {
                bfs.push({node->mChildren[c], curIdx});
            }
        }
    }

    // 每帧：先采样动画通道到 localTransforms，再按拓扑序累乘
    std::vector<Matrix4> localTransforms(nodeList.size());
    std::vector<Matrix4> globalTransforms(nodeList.size());

    for (size_t frame = 0; frame < frameCount; frame++) {
        float tick = (float)frame;

        // 初始化所有节点为默认本地变换
        for (size_t n = 0; n < nodeList.size(); n++) {
            localTransforms[n] = nodeList[n].defaultLocal;
        }

        // 用动画通道覆盖有动画的节点
        for (unsigned int c = 0; c < anim->mNumChannels; c++) {
            const aiNodeAnim* channel = anim->mChannels[c];
            std::string nodeName(channel->mNodeName.C_Str());
            auto nit = nodeNameToListIdx.find(nodeName);
            if (nit == nodeNameToListIdx.end()) continue;

            aiVector3D pos(0, 0, 0);
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++) {
                if (channel->mPositionKeys[k].mTime <= tick) pos = channel->mPositionKeys[k].mValue;
                else break;
            }
            aiQuaternion rot(1, 0, 0, 0);
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++) {
                if (channel->mRotationKeys[k].mTime <= tick) rot = channel->mRotationKeys[k].mValue;
                else break;
            }
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

            Matrix4 m;
            m.array[0][0] = mat.a1; m.array[1][0] = mat.a2; m.array[2][0] = mat.a3; m.array[3][0] = mat.a4;
            m.array[0][1] = mat.b1; m.array[1][1] = mat.b2; m.array[2][1] = mat.b3; m.array[3][1] = mat.b4;
            m.array[0][2] = mat.c1; m.array[1][2] = mat.c2; m.array[2][2] = mat.c3; m.array[3][2] = mat.c4;
            m.array[0][3] = mat.d1; m.array[1][3] = mat.d2; m.array[2][3] = mat.d3; m.array[3][3] = mat.d4;
            localTransforms[nit->second] = m;
        }

        // 拓扑序累乘：globalTransform[n] = globalTransform[parent] * localTransform[n]
        for (size_t n = 0; n < nodeList.size(); n++) {
            if (nodeList[n].parentIdx >= 0) {
                globalTransforms[n] = globalTransforms[nodeList[n].parentIdx] * localTransforms[n];
            } else {
                globalTransforms[n] = localTransforms[n];
            }
            // 写入 allJoints（仅骨骼节点）
            if (nodeList[n].jointIdx >= 0) {
                allJoints[frame * numJoints + nodeList[n].jointIdx] = globalTransforms[n];
            }
        }
    }

    MeshAnimation* result = new MeshAnimation(numJoints, frameCount, ticksPerSec, allJoints);
    LOG_INFO("[AssimpLoader] LoadAnimation OK: " << path
             << " frames=" << frameCount << " joints=" << numJoints);
    return result;
}

} // namespace ECS
