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
#include "Vector.h"
#include "Mesh.h"

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

} // namespace ECS
