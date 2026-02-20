/**
 * @file AssetManager.cpp
 * @brief AssetManager 实现：Handle 体系、引用计数、默认资源
 */

#include "AssetManager.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/Assert.h"
#include "OGLMesh.h"
#include "OGLTexture.h"
#include "Mesh.h"

using namespace NCL;
using namespace NCL::Rendering;

namespace ECS {

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() {
    Clear();
}

void AssetManager::Init() {
    LOG_INFO("[AssetManager] Initializing...");

    // 创建默认立方体网格
    auto* defaultMesh = CreateDefaultMesh();
    m_MeshCache[1].resource.reset(defaultMesh);
    m_MeshCache[1].refCount = 1; // 永不卸载
    m_DefaultMeshHandle = 1;

    // TODO: 创建默认纹理（紫黑格）
    m_DefaultTextureHandle = INVALID_HANDLE;

    LOG_INFO("[AssetManager] Default resources loaded");
}

void AssetManager::Clear() {
    LOG_INFO("[AssetManager] Clearing all resources...");

    // 清空所有缓存（保留默认资源）
    for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ) {
        if (it->first == m_DefaultMeshHandle) {
            ++it; // 跳过默认资源
        } else {
            it = m_MeshCache.erase(it);
        }
    }

    for (auto it = m_TextureCache.begin(); it != m_TextureCache.end(); ) {
        if (it->first == m_DefaultTextureHandle) {
            ++it;
        } else {
            it = m_TextureCache.erase(it);
        }
    }

    m_PathToMeshHandle.clear();
    m_PathToTextureHandle.clear();

    LOG_INFO("[AssetManager] Cleared");
}

void AssetManager::UnloadUnused() {
    // 卸载所有引用计数为 0 的资源
    for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ) {
        if (it->second.refCount == 0 && it->first != m_DefaultMeshHandle) {
            LOG_INFO("[AssetManager] Unloading unused mesh " << it->first);
            it = m_MeshCache.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// Mesh 资源管理
// =============================================================================

MeshHandle AssetManager::LoadMesh(const std::string& path) {
    // 检查是否已加载
    auto it = m_PathToMeshHandle.find(path);
    if (it != m_PathToMeshHandle.end()) {
        MeshHandle handle = it->second;
        m_MeshCache[handle].refCount++;
        LOG_INFO("[AssetManager] Mesh already loaded: " << path
                  << " (Handle: " << handle << ", RefCount: "
                  << m_MeshCache[handle].refCount << ")");
        return handle;
    }

    // 根据文件扩展名选择加载器
    OGLMesh* mesh = nullptr;

    if (IsAssimpFormat(path)) {
        // 使用 Assimp 加载（支持 OBJ, FBX, GLTF, DAE, BLEND 等）
        LOG_INFO("[AssetManager] Loading mesh via Assimp: " << path);
        mesh = AssimpLoader::LoadMesh(path);
    } else if (path.ends_with(".msh")) {
        // 保留 NCL 原生格式支持（可选）
        LOG_INFO("[AssetManager] Loading mesh via MshLoader: " << path);
        // TODO: 调用 MshLoader::LoadMesh(path)
        // 当前暂不支持，返回默认网格
        mesh = nullptr;
    } else {
        LOG_ERROR("[AssetManager] Unsupported mesh format: " << path);
        mesh = nullptr;
    }

    // 加载失败，返回默认网格
    if (!mesh) {
        LOG_ERROR("[AssetManager] Failed to load mesh: " << path
                  << ", using default cube");
        return m_DefaultMeshHandle;
    }

    // 缓存网格
    MeshHandle newHandle = m_NextMeshHandle++;
    m_MeshCache[newHandle].resource.reset(mesh);
    m_MeshCache[newHandle].refCount = 1;
    m_PathToMeshHandle[path] = newHandle;

    LOG_INFO("[AssetManager] Successfully loaded mesh: " << path
              << " (Handle: " << newHandle << ")");

    return newHandle;
}

OGLMesh* AssetManager::GetMesh(MeshHandle handle) {
    if (handle == INVALID_HANDLE || m_MeshCache.find(handle) == m_MeshCache.end()) {
        // 返回默认网格
        return m_MeshCache[m_DefaultMeshHandle].resource.get();
    }
    return m_MeshCache[handle].resource.get();
}

void AssetManager::ReleaseMesh(MeshHandle handle) {
    auto it = m_MeshCache.find(handle);
    if (it != m_MeshCache.end() && it->second.refCount > 0) {
        it->second.refCount--;
    }
}

// =============================================================================
// Texture 资源管理
// =============================================================================

TextureHandle AssetManager::LoadTexture(const std::string& path) {
    // TODO: 实现纹理加载
    LOG_WARN("[AssetManager] Loading texture: " << path << " (not implemented)");
    return INVALID_HANDLE;
}

OGLTexture* AssetManager::GetTexture(TextureHandle handle) {
    // TODO: 实现纹理解析
    return nullptr;
}

void AssetManager::ReleaseTexture(TextureHandle handle) {
    // TODO: 实现纹理引用计数
}

// =============================================================================
// 默认资源创建
// =============================================================================
// 默认资源创建
// =============================================================================

bool AssetManager::IsAssimpFormat(const std::string& path) {
    // 支持的 Assimp 格式列表
    static const std::vector<std::string> formats = {
        ".obj",   // Wavefront OBJ
        ".fbx",   // Autodesk FBX
        ".gltf",  // GL Transmission Format
        ".glb",   // GLTF Binary
        ".dae",   // COLLADA
        ".blend", // Blender
        ".3ds",   // 3D Studio
        ".ase",   // 3D Studio ASE
        ".ifc",   // Industry Foundation Classes
        ".xgl",   // XGL
        ".zgl",   // ZGL
        ".ply",   // Stanford PLY
        ".dxf",   // AutoCAD DXF
        ".lwo",   // LightWave Object
        ".lws",   // LightWave Scene
        ".lxo",   // Modo
        ".stl",   // Stereolithography
        ".x",     // DirectX X
        ".ac",    // AC3D
        ".ms3d",  // Milkshape 3D
        ".cob",   // TrueSpace
        ".scn",   // TrueSpace
        ".bvh",   // Biovision BVH
        ".csm",   // CharacterStudio Motion
        ".xml",   // Irrlicht
        ".irrmesh", // Irrlicht Mesh
        ".irr",   // Irrlicht Scene
        ".mdl",   // Quake Model
        ".md2",   // Quake II Model
        ".md3",   // Quake III Model
        ".pk3",   // Quake III BSP
        ".mdc",   // Return to Castle Wolfenstein
        ".md5",   // Doom 3 Model
        ".smd",   // Valve SMD
        ".vta",   // Valve VTA
        ".ogex",  // Open Game Engine Exchange
        ".3d",    // Unreal
        ".b3d",   // BlitzBasic 3D
        ".q3d",   // Quick3D
        ".q3s",   // Quick3D
        ".nff",   // Neutral File Format
        ".off",   // Object File Format
        ".raw",   // Raw Triangles
        ".ter",   // Terragen Terrain
        ".hmp",   // 3D GameStudio Terrain
        ".ndo"    // Nendo
    };

    for (const auto& ext : formats) {
        if (path.ends_with(ext)) {
            return true;
        }
    }
    return false;
}

OGLMesh* AssetManager::CreateDefaultMesh() {
    // 创建单位立方体（1x1x1）
    OGLMesh* cubeMesh = new OGLMesh();
    cubeMesh->SetVertexPositions({
        NCL::Maths::Vector3(-0.5f, -0.5f, -0.5f),
        NCL::Maths::Vector3( 0.5f, -0.5f, -0.5f),
        NCL::Maths::Vector3( 0.5f,  0.5f, -0.5f),
        NCL::Maths::Vector3(-0.5f,  0.5f, -0.5f),
        NCL::Maths::Vector3(-0.5f, -0.5f,  0.5f),
        NCL::Maths::Vector3( 0.5f, -0.5f,  0.5f),
        NCL::Maths::Vector3( 0.5f,  0.5f,  0.5f),
        NCL::Maths::Vector3(-0.5f,  0.5f,  0.5f),
    });

    cubeMesh->SetVertexIndices({
        0, 1, 2,  0, 2, 3, // Front
        4, 5, 6,  4, 6, 7, // Back
        0, 4, 7,  0, 7, 3, // Left
        1, 5, 6,  1, 6, 2, // Right
        3, 2, 6,  3, 6, 7, // Top
        0, 1, 5,  0, 5, 4  // Bottom
    });

    cubeMesh->SetPrimitiveType(NCL::Rendering::GeometryPrimitive::Triangles);
    cubeMesh->UploadToGPU();

    return cubeMesh;
}

OGLTexture* AssetManager::CreateDefaultTexture() {
    // TODO: 创建紫黑格纹理（2x2 棋盘格）
    return nullptr;
}

} // namespace ECS
