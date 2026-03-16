/**
 * @file AssetManager.cpp
 * @brief AssetManager 实现：Handle 体系、引用计数、默认资源（含四类默认纹理）
 *
 * @details
 * - LoadTexture：使用 OGLTexture::TextureFromFile 加载，路径→Handle LRU 缓存
 * - GetTexture：O(1) Handle→OGLTexture* 解析，Handle 无效时返回默认 Albedo
 * - CreateSolidColorTexture：通过 OGLTexture::TextureFromData 创建 1×1 单色纹理
 * - 默认纹理在 Init() 中预建，不参与 Clear() 的批量卸载
 */

#include "AssetManager.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/Assert.h"
#include "OGLMesh.h"
#include "OGLTexture.h"
#include "MeshAnimation.h"
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
    if (m_DefaultMeshHandle != INVALID_HANDLE) return; // 幂等守卫

    LOG_INFO("[AssetManager] Initializing...");

    // ── 默认立方体网格 ────────────────────────────────────
    auto* defaultMesh = CreateDefaultMesh();
    m_MeshCache[1].resource.reset(defaultMesh);
    m_MeshCache[1].refCount = 1;
    m_DefaultMeshHandle = 1;
    m_NextMeshHandle = 2;

    // ── 四类默认纹理（永不卸载，refCount 固定为 1）────────
    // Default albedo: 1×1 白色 RGBA8
    auto* albedo = CreateSolidColorTexture(255, 255, 255, 255);
    m_TextureCache[1].resource.reset(albedo);
    m_TextureCache[1].refCount = 1;
    m_DefaultAlbedoHandle = 1;

    // Default normal: 1×1 切线空间中性蓝 (0x80, 0x80, 0xFF, 0xFF)
    auto* normal = CreateSolidColorTexture(128, 128, 255, 255);
    m_TextureCache[2].resource.reset(normal);
    m_TextureCache[2].refCount = 1;
    m_DefaultNormalHandle = 2;

    // Default ORM: 1×1 (0, 128, 255) → occlusion=0, roughness=0.5, metallic=0
    auto* orm = CreateSolidColorTexture(0, 128, 255, 255);
    m_TextureCache[3].resource.reset(orm);
    m_TextureCache[3].refCount = 1;
    m_DefaultOrmHandle = 3;

    // Default emissive: 1×1 黑色
    auto* emissive = CreateSolidColorTexture(0, 0, 0, 0);
    m_TextureCache[4].resource.reset(emissive);
    m_TextureCache[4].refCount = 1;
    m_DefaultEmissiveHandle = 4;

    // 兼容旧字段
    m_DefaultTextureHandle = m_DefaultAlbedoHandle;

    m_NextTextureHandle = 5;

    LOG_INFO("[AssetManager] Default resources loaded");
}

void AssetManager::Clear() {
    LOG_INFO("[AssetManager] Clearing all resources...");

    for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ) {
        if (it->first == m_DefaultMeshHandle) {
            ++it;
        } else {
            it = m_MeshCache.erase(it);
        }
    }

    // 保留所有默认纹理（handle 1-4）
    for (auto it = m_TextureCache.begin(); it != m_TextureCache.end(); ) {
        if (it->first == m_DefaultAlbedoHandle   ||
            it->first == m_DefaultNormalHandle   ||
            it->first == m_DefaultOrmHandle      ||
            it->first == m_DefaultEmissiveHandle) {
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
    auto it = m_PathToMeshHandle.find(path);
    if (it != m_PathToMeshHandle.end()) {
        MeshHandle handle = it->second;
        m_MeshCache[handle].refCount++;
        return handle;
    }

    OGLMesh* mesh = nullptr;

    if (IsAssimpFormat(path)) {
        mesh = AssimpLoader::LoadMesh(path);
    } else if (path.ends_with(".msh")) {
        LOG_WARN("[AssetManager] .msh format not yet supported: " << path);
        mesh = nullptr;
    } else {
        LOG_ERROR("[AssetManager] Unsupported mesh format: " << path);
        mesh = nullptr;
    }

    if (!mesh) {
        LOG_ERROR("[AssetManager] Failed to load mesh: " << path);
        return m_DefaultMeshHandle;
    }

    MeshHandle newHandle = m_NextMeshHandle++;
    m_MeshCache[newHandle].resource.reset(mesh);
    m_MeshCache[newHandle].refCount = 1;
    m_PathToMeshHandle[path] = newHandle;

    LOG_INFO("[AssetManager] Loaded mesh: " << path << " (Handle: " << newHandle << ")");
    return newHandle;
}

OGLMesh* AssetManager::GetMesh(MeshHandle handle) {
    if (handle == INVALID_HANDLE || m_MeshCache.find(handle) == m_MeshCache.end()) {
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
    // 路径缓存命中
    auto it = m_PathToTextureHandle.find(path);
    if (it != m_PathToTextureHandle.end()) {
        TextureHandle handle = it->second;
        m_TextureCache[handle].refCount++;
        return handle;
    }

    // 磁盘加载
    auto tex = OGLTexture::TextureFromFile(path);
    if (!tex) {
        LOG_ERROR("[AssetManager] Failed to load texture: " << path);
        return m_DefaultAlbedoHandle;
    }

    TextureHandle newHandle = m_NextTextureHandle++;
    m_TextureCache[newHandle].resource = std::move(tex);
    m_TextureCache[newHandle].refCount = 1;
    m_PathToTextureHandle[path] = newHandle;

    LOG_INFO("[AssetManager] Loaded texture: " << path << " (Handle: " << newHandle << ")");
    return newHandle;
}

Texture* AssetManager::GetTexture(TextureHandle handle) {
    if (handle == INVALID_HANDLE || m_TextureCache.find(handle) == m_TextureCache.end()) {
        return m_TextureCache[m_DefaultAlbedoHandle].resource.get();
    }
    return m_TextureCache[handle].resource.get();
}

void AssetManager::ReleaseTexture(TextureHandle handle) {
    auto it = m_TextureCache.find(handle);
    if (it != m_TextureCache.end() && it->second.refCount > 0) {
        it->second.refCount--;
    }
}

// =============================================================================
// 默认资源创建
// =============================================================================

bool AssetManager::IsAssimpFormat(const std::string& path) {
    static const std::vector<std::string> formats = {
        ".obj", ".fbx", ".gltf", ".glb", ".dae", ".blend", ".3ds",
        ".ase", ".ifc", ".xgl", ".zgl", ".ply", ".dxf", ".lwo",
        ".lws", ".lxo", ".stl", ".x", ".ac", ".ms3d", ".cob",
        ".scn", ".bvh", ".csm", ".xml", ".irrmesh", ".irr",
        ".mdl", ".md2", ".md3", ".pk3", ".mdc", ".md5",
        ".smd", ".vta", ".ogex", ".3d", ".b3d", ".q3d",
        ".q3s", ".nff", ".off", ".raw", ".ter", ".hmp", ".ndo"
    };
    for (const auto& ext : formats) {
        if (path.ends_with(ext)) return true;
    }
    return false;
}

OGLMesh* AssetManager::CreateDefaultMesh() {
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
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        0, 4, 7, 0, 7, 3,
        1, 5, 6, 1, 6, 2,
        3, 2, 6, 3, 6, 7,
        0, 1, 5, 0, 5, 4
    });
    cubeMesh->SetPrimitiveType(NCL::Rendering::GeometryPrimitive::Triangles);
    cubeMesh->UploadToGPU();
    return cubeMesh;
}

OGLTexture* AssetManager::CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // 4-byte RGBA8 pixel
    char data[4] = {static_cast<char>(r), static_cast<char>(g),
                    static_cast<char>(b), static_cast<char>(a)};
    auto tex = OGLTexture::TextureFromData(data, 1, 1, 4);
    return tex.release();
}

// =============================================================================
// Animation 资源管理
// =============================================================================

AnimHandle AssetManager::LoadAnimation(const std::string& path,
                                        NCL::Rendering::OGLMesh* meshToFill) {
    // 路径缓存命中
    auto it = m_PathToAnimHandle.find(path);
    if (it != m_PathToAnimHandle.end()) return it->second;

    MeshAnimation* anim = AssimpLoader::LoadAnimation(path, meshToFill);
    if (!anim) {
        LOG_WARN("[AssetManager] LoadAnimation failed, returning invalid handle: " << path);
        return INVALID_HANDLE;
    }

    AnimHandle handle = m_NextAnimHandle++;
    m_AnimCache[handle] = std::unique_ptr<MeshAnimation>(anim);
    m_PathToAnimHandle[path] = handle;
    LOG_INFO("[AssetManager] Animation loaded handle=" << handle << " path=" << path);
    return handle;
}

NCL::Rendering::MeshAnimation* AssetManager::GetAnimation(AnimHandle handle) {
    auto it = m_AnimCache.find(handle);
    return (it != m_AnimCache.end()) ? it->second.get() : nullptr;
}

} // namespace ECS
