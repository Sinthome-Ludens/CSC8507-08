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
#include "OGLTexture.h"
#include "MeshAnimation.h"
#include "Mesh.h"
#include "../GLTFLoader/GLTFLoader.h"
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
using namespace NCL;
using namespace NCL::Rendering;
using namespace NCL::Maths;

namespace ECS {

struct GLTFLoadSelector {
    std::string basePath;
    int nodeIndex = -1;
    bool recenter = false;
};

static GLTFLoadSelector ParseGLTFSelector(const std::string& path) {
    GLTFLoadSelector sel{};
    size_t hash = path.find('#');
    sel.basePath = (hash == std::string::npos) ? path : path.substr(0, hash);
    if (hash == std::string::npos) {
        return sel;
    }

    std::string fragment = path.substr(hash + 1);
    size_t start = 0;
    while (start < fragment.size()) {
        size_t end = fragment.find('&', start);
        std::string token = fragment.substr(start, end == std::string::npos ? std::string::npos : end - start);

        const std::string nodePrefix = "node=";
        const std::string recenterPrefix = "recenter=";
        if (token.rfind(nodePrefix, 0) == 0) {
            try {
                sel.nodeIndex = std::stoi(token.substr(nodePrefix.size()));
            } catch (...) {
                sel.nodeIndex = -1;
            }
        } else if (token.rfind(recenterPrefix, 0) == 0) {
            std::string value = token.substr(recenterPrefix.size());
            sel.recenter = (value == "1" || value == "true" || value == "yes");
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }
    return sel;
}

static ImportedAlphaMode ToImportedAlphaMode(NCL::Rendering::GLTFAlphaMode mode) {
    switch (mode) {
        case NCL::Rendering::GLTFAlphaMode::Mask:   return ImportedAlphaMode::Mask;
        case NCL::Rendering::GLTFAlphaMode::Cutoff: return ImportedAlphaMode::Blend;
        case NCL::Rendering::GLTFAlphaMode::Opaque:
        default:                                    return ImportedAlphaMode::Opaque;
    }
}

static int FindPreferredMaterialLayer(const GLTFScene& scene) {
    if (scene.meshMaterials.empty() || scene.materials.empty()) {
        return -1;
    }

    int firstLayer = -1;
    for (const auto& meshMats : scene.meshMaterials) {
        for (int layer : meshMats.layers) {
            if (layer < 0 || layer >= static_cast<int>(scene.materials.size())) {
                continue;
            }
            if (firstLayer < 0) {
                firstLayer = layer;
            }
            if (scene.materials[layer].albedo) {
                return layer;
            }
        }
    }
    return firstLayer;
}

static int FindPreferredMaterialLayerForSelector(const GLTFScene& scene, const GLTFLoadSelector& selector) {
    if (selector.nodeIndex >= 0 && selector.nodeIndex < static_cast<int>(scene.sceneNodes.size())) {
        const auto& node = scene.sceneNodes[selector.nodeIndex];
        if (node.mesh) {
            for (size_t meshIndex = 0; meshIndex < scene.meshes.size() && meshIndex < scene.meshMaterials.size(); ++meshIndex) {
                if (scene.meshes[meshIndex] != node.mesh) {
                    continue;
                }

                int firstLayer = -1;
                for (int layer : scene.meshMaterials[meshIndex].layers) {
                    if (layer < 0 || layer >= static_cast<int>(scene.materials.size())) {
                        continue;
                    }
                    if (firstLayer < 0) {
                        firstLayer = layer;
                    }
                    if (scene.materials[layer].albedo) {
                        return layer;
                    }
                }
                if (firstLayer >= 0) {
                    return firstLayer;
                }
            }
        }
    }

    return FindPreferredMaterialLayer(scene);
}

static TextureHandle LoadGLTFTextureHandle(AssetManager& am, const json& root, int textureIndex, const std::string& fullPath) {
    if (!root.contains("textures") || !root["textures"].is_array()) {
        return INVALID_HANDLE;
    }
    const auto& textures = root["textures"];
    if (textureIndex < 0 || textureIndex >= static_cast<int>(textures.size())) {
        return INVALID_HANDLE;
    }
    int imageIndex = textures[textureIndex].value("source", -1);
    if (imageIndex < 0 || !root.contains("images") || !root["images"].is_array()) {
        return INVALID_HANDLE;
    }
    const auto& images = root["images"];
    if (imageIndex >= static_cast<int>(images.size())) return INVALID_HANDLE;
    std::string uri = images[imageIndex].value("uri", "");
    if (uri.empty()) return INVALID_HANDLE;
    std::filesystem::path texPath = std::filesystem::path(fullPath).parent_path() / uri;
    return am.LoadTexture(texPath.string());
}

static ImportedMaterialDefaults BuildImportedMaterialDefaultsForLayer(const GLTFScene& scene, int materialLayer) {
    ImportedMaterialDefaults defaults{};
    if (materialLayer < 0 || materialLayer >= static_cast<int>(scene.materials.size())) {
        return defaults;
    }

    const auto& src = scene.materials[materialLayer];
    defaults.valid       = true;
    defaults.baseColour  = src.albedoColour;
    defaults.metallic    = src.metallicFactor;
    defaults.roughness   = src.roughnessFactor;
    defaults.ao          = 1.0f;
    defaults.alphaCutoff = src.alphaCutoff;
    defaults.doubleSided = src.doubleSided;
    defaults.alphaMode   = ToImportedAlphaMode(src.alphaMode);
    return defaults;
}

static ImportedMaterialDefaults BuildImportedMaterialDefaults(const GLTFScene& scene, const std::string& fullPath) {
    ImportedMaterialDefaults defaults{};

    if (scene.meshMaterials.empty() || scene.materials.empty()) {
        return defaults;
    }

    int materialLayer = FindPreferredMaterialLayer(scene);

    if (materialLayer < 0 || materialLayer >= static_cast<int>(scene.materials.size())) {
        return defaults;
    }

    defaults = BuildImportedMaterialDefaultsForLayer(scene, materialLayer);
    const auto& src = scene.materials[materialLayer];

    if (scene.materials.size() > 1) {
        LOG_WARN("[AssetManager] GLTF material merge fallback: '" << fullPath
                 << "' has " << scene.materials.size()
                 << " materials; using first resolved layer '" << src.name << "'");
    }

    return defaults;
}

static ImportedMaterialDefaults LoadImportedMaterialDefaultsFromGLTF(const std::string& fullPath) {
    GLTFLoadSelector selector = ParseGLTFSelector(fullPath);
    const std::string& sourcePath = selector.basePath;

    GLTFScene scene;
    if (!GLTFLoader::LoadFromPath(sourcePath, scene)) {
        return ImportedMaterialDefaults{};
    }

    ImportedMaterialDefaults defaults = BuildImportedMaterialDefaults(scene, sourcePath);
    if (!defaults.valid) {
        return defaults;
    }

    if (sourcePath.ends_with(".glb")) {
        return defaults;
    }

    std::ifstream in(sourcePath);
    if (!in.good()) {
        return defaults;
    }

    json root;
    try {
        in >> root;
    } catch (...) {
        return defaults;
    }
    if (!root.contains("materials") || !root["materials"].is_array()) {
        return defaults;
    }

    int materialIndex = FindPreferredMaterialLayerForSelector(scene, selector);
    const auto& materials = root["materials"];
    if (materialIndex < 0 || materialIndex >= static_cast<int>(materials.size())) {
        return defaults;
    }

    auto& am = AssetManager::Instance();
    const auto& mat = materials[materialIndex];
    int albedoTex = -1;
    int normalTex = -1;
    int ormTex = -1;
    int occTex = -1;
    int emissiveTex = -1;

    if (mat.contains("pbrMetallicRoughness")) {
        const auto& pbr = mat["pbrMetallicRoughness"];
        if (pbr.contains("baseColorTexture")) albedoTex = pbr["baseColorTexture"].value("index", -1);
        if (pbr.contains("metallicRoughnessTexture")) ormTex = pbr["metallicRoughnessTexture"].value("index", -1);
    }
    if (mat.contains("normalTexture")) normalTex = mat["normalTexture"].value("index", -1);
    if (mat.contains("occlusionTexture")) occTex = mat["occlusionTexture"].value("index", -1);
    if (mat.contains("emissiveTexture")) emissiveTex = mat["emissiveTexture"].value("index", -1);

    defaults.albedoHandle = LoadGLTFTextureHandle(am, root, albedoTex, sourcePath);
    defaults.normalHandle = LoadGLTFTextureHandle(am, root, normalTex, sourcePath);
    defaults.ormHandle = LoadGLTFTextureHandle(am, root, ormTex, sourcePath);
    if (defaults.ormHandle == INVALID_HANDLE) {
        defaults.ormHandle = LoadGLTFTextureHandle(am, root, occTex, sourcePath);
    }
    defaults.emissiveHandle = LoadGLTFTextureHandle(am, root, emissiveTex, sourcePath);

    return defaults;
}

static Vector3 TransformPosition(const Matrix4& mat, const Vector3& pos) {
    Vector4 transformed = mat * Vector4(pos.x, pos.y, pos.z, 1.0f);
    if (std::abs(transformed.w) > 1e-6f) {
        return Vector3(transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w);
    }
    return Vector3(transformed.x, transformed.y, transformed.z);
}

static Vector3 TransformDirection(const Matrix4& mat, const Vector3& dir) {
    Vector4 transformed = mat * Vector4(dir.x, dir.y, dir.z, 0.0f);
    Vector3 out(transformed.x, transformed.y, transformed.z);
    float lenSq = Vector::LengthSquared(out);
    return lenSq > 1e-8f ? Vector::Normalise(out) : dir;
}

static Vector4 TransformTangent(const Matrix4& mat, const Vector4& tangent) {
    Vector3 xyz = TransformDirection(mat, Vector3(tangent.x, tangent.y, tangent.z));
    return Vector4(xyz.x, xyz.y, xyz.z, tangent.w);
}

// ── Auto-generate tangents (mirrors Assimp's aiProcess_CalcTangentSpace) ──────
// Uses triangle UV derivatives to compute per-vertex tangent + handedness (w).
// Needed because GLTF files may omit TANGENT attributes, but the PBR shader's
// TBN matrix construction requires them (normalize(vec3(0)) → NaN in GLSL).
static void ComputeTangents(
    const std::vector<Vector3>& positions,
    const std::vector<Vector3>& normals,
    const std::vector<Vector2>& texCoords,
    const std::vector<unsigned int>& indices,
    std::vector<Vector4>& outTangents)
{
    size_t vertCount = positions.size();
    std::vector<Vector3> tan1(vertCount, Vector3(0, 0, 0));
    std::vector<Vector3> tan2(vertCount, Vector3(0, 0, 0));

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        unsigned int i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];

        Vector3 edge1 = positions[i1] - positions[i0];
        Vector3 edge2 = positions[i2] - positions[i0];

        float du1 = texCoords[i1].x - texCoords[i0].x;
        float dv1 = texCoords[i1].y - texCoords[i0].y;
        float du2 = texCoords[i2].x - texCoords[i0].x;
        float dv2 = texCoords[i2].y - texCoords[i0].y;

        float det = du1 * dv2 - du2 * dv1;
        if (std::abs(det) < 1e-8f) continue; // degenerate UV triangle
        float r = 1.0f / det;

        Vector3 sdir = (edge1 * dv2 - edge2 * dv1) * r;
        Vector3 tdir = (edge2 * du1 - edge1 * du2) * r;

        tan1[i0] = tan1[i0] + sdir; tan1[i1] = tan1[i1] + sdir; tan1[i2] = tan1[i2] + sdir;
        tan2[i0] = tan2[i0] + tdir; tan2[i1] = tan2[i1] + tdir; tan2[i2] = tan2[i2] + tdir;
    }

    outTangents.resize(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        const Vector3& n = normals[i];
        const Vector3& t = tan1[i];

        // Gram-Schmidt orthogonalize: T' = normalize(t - n * dot(n, t))
        Vector3 tangent = t - n * Vector::Dot(n, t);
        float len = Vector::Length(tangent);
        if (len > 1e-6f) {
            tangent = tangent * (1.0f / len);
        } else {
            // Fallback: arbitrary perpendicular to normal
            tangent = (std::abs(n.y) < 0.999f)
                ? Vector::Normalise(Vector::Cross(n, Vector3(0, 1, 0)))
                : Vector::Normalise(Vector::Cross(n, Vector3(1, 0, 0)));
        }

        // Handedness: sign = dot(cross(n, tangent), tan2) < 0 ? -1 : 1
        float w = Vector::Dot(Vector::Cross(n, tangent), tan2[i]) < 0.0f ? -1.0f : 1.0f;
        outTangents[i] = Vector4(tangent.x, tangent.y, tangent.z, w);
    }
}

bool AssetManager::s_GLTFInitialized = false;

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() {
    Clear();
}

void AssetManager::SetMeshFactory(MeshFactory factory) {
    m_MeshFactory = std::move(factory);
    AssimpLoader::SetMeshFactory(m_MeshFactory);
    LOG_INFO("[AssetManager] MeshFactory injected");
}

void AssetManager::Init() {
    if (m_DefaultMeshHandle != INVALID_HANDLE) return; // 幂等守卫

    GAME_ASSERT(m_MeshFactory, "[AssetManager] MeshFactory not set! Call SetMeshFactory() before Init().");
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

    // Default ORM: 1×1 (255, 128, 0) → occlusion=1.0, roughness=0.5, metallic=0.0
    auto* orm = CreateSolidColorTexture(255, 128, 0, 255);
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
    m_ImportedMaterialDefaultsCache.clear();

    LOG_INFO("[AssetManager] Cleared");
}

void AssetManager::UnloadUnused() {
    for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ) {
        if (it->second.refCount == 0 && it->first != m_DefaultMeshHandle) {
            LOG_INFO("[AssetManager] Unloading unused mesh " << it->first);
            m_ImportedMaterialDefaultsCache.erase(it->first);
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
    GLTFLoadSelector selector = ParseGLTFSelector(path);
    const std::string& sourcePath = selector.basePath;

    auto it = m_PathToMeshHandle.find(path);
    if (it != m_PathToMeshHandle.end()) {
        MeshHandle handle = it->second;
        m_MeshCache[handle].refCount++;
        return handle;
    }

    Mesh* mesh = nullptr;

    if (IsGltfFormat(path)) {
        LOG_INFO("[AssetManager] LoadMesh pipeline: GLTF branch → GLTFLoader::LoadFromPath for '" << path << "'");
        mesh = LoadMeshViaGLTF(path);
    } else if (IsAssimpFormat(sourcePath)) {
        LOG_INFO("[AssetManager] LoadMesh pipeline: Assimp branch → AssimpLoader::LoadMesh for '" << sourcePath << "'");
        mesh = AssimpLoader::LoadMesh(sourcePath);
    } else if (sourcePath.ends_with(".msh")) {
        LOG_WARN("[AssetManager] LoadMesh pipeline: .msh format not yet supported, file='" << sourcePath << "'");
        mesh = nullptr;
    } else {
        LOG_ERROR("[AssetManager] LoadMesh pipeline: unsupported format, file='" << sourcePath << "'");
        mesh = nullptr;
    }

    if (!mesh) {
        LOG_ERROR("[AssetManager] LoadMesh FAILED: no mesh loaded for '" << path << "', returning default cube handle");
        return m_DefaultMeshHandle;
    }

    MeshHandle newHandle = m_NextMeshHandle++;
    m_MeshCache[newHandle].resource.reset(mesh);
    m_MeshCache[newHandle].refCount = 1;
    m_PathToMeshHandle[path] = newHandle;

    if (IsGltfFormat(path)) {
        ImportedMaterialDefaults defaults = LoadImportedMaterialDefaultsFromGLTF(path);
        if (defaults.valid) {
            m_ImportedMaterialDefaultsCache[newHandle] = defaults;
        }
    }

    LOG_INFO("[AssetManager] LoadMesh SUCCESS: '" << path << "' → Handle=" << newHandle);
    return newHandle;
}

Mesh* AssetManager::GetMesh(MeshHandle handle) {
    if (handle == INVALID_HANDLE || m_MeshCache.find(handle) == m_MeshCache.end()) {
        return m_MeshCache[m_DefaultMeshHandle].resource.get();
    }
    return m_MeshCache[handle].resource.get();
}

bool AssetManager::GetImportedMaterialDefaults(MeshHandle handle, ImportedMaterialDefaults& outDefaults) const {
    auto it = m_ImportedMaterialDefaultsCache.find(handle);
    if (it == m_ImportedMaterialDefaultsCache.end() || !it->second.valid) {
        return false;
    }
    outDefaults = it->second;
    return true;
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
    // NOTE: .gltf/.glb removed — GLTF files now go through GLTFLoader, not Assimp
    static const std::vector<std::string> formats = {
        ".obj", ".fbx", ".dae", ".blend", ".3ds",
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

bool AssetManager::IsGltfFormat(const std::string& path) {
    std::string basePath = ParseGLTFSelector(path).basePath;
    return basePath.ends_with(".gltf") || basePath.ends_with(".glb");
}

void AssetManager::EnsureGLTFInitialized() {
    if (s_GLTFInitialized) return;
    LOG_INFO("[AssetManager] Initializing GLTFLoader: injecting MeshConstructionFunction + TextureConstructionFunction");
    auto factory = m_MeshFactory; // capture by value for lambda
    GLTFLoader::SetMeshConstructionFunction([factory]() -> SharedMesh {
        return SharedMesh(factory());
    });
    GLTFLoader::SetTextureConstructionFunction([](std::string& path) -> SharedTexture {
        return SharedTexture(OGLTexture::TextureFromFile(path));
    });
    s_GLTFInitialized = true;
    LOG_INFO("[AssetManager] GLTFLoader initialized OK");
}

Mesh* AssetManager::LoadMeshViaGLTF(const std::string& fullPath) {
    EnsureGLTFInitialized();

    GLTFLoadSelector selector = ParseGLTFSelector(fullPath);
    const std::string& sourcePath = selector.basePath;

    LOG_INFO("[AssetManager] LoadMeshViaGLTF: calling GLTFLoader::LoadFromPath('" << sourcePath << "')");

    GLTFScene scene;
    if (!GLTFLoader::LoadFromPath(sourcePath, scene)) {
        LOG_ERROR("[AssetManager] LoadMeshViaGLTF FAILED: GLTFLoader::LoadFromPath returned false for '" << sourcePath << "'");
        return nullptr;
    }

    if (scene.meshes.empty()) {
        LOG_WARN("[AssetManager] LoadMeshViaGLTF: GLTFScene has 0 meshes in '" << sourcePath << "'");
        return nullptr;
    }

    LOG_INFO("[AssetManager] LoadMeshViaGLTF: GLTFScene contains " << scene.meshes.size() << " mesh(es) from '" << sourcePath << "'");

    Mesh* result = m_MeshFactory();

    // Merge all sub-meshes into one OGLMesh (with attribute padding like Assimp's MergeMeshes)
    std::vector<Vector3>       mergedPositions;
    std::vector<unsigned int>  mergedIndices;
    std::vector<Vector3>       mergedNormals;
    std::vector<Vector4>       mergedTangents;
    std::vector<Vector2>       mergedTexCoords;
    bool hasNormals   = false;
    bool hasTangents  = false;
    bool hasTexCoords = false;

    bool mergedAnyNodeMesh = false;
    for (size_t ni = 0; ni < scene.sceneNodes.size(); ++ni) {
        if (selector.nodeIndex >= 0 && static_cast<int>(ni) != selector.nodeIndex) {
            continue;
        }
        const auto& node = scene.sceneNodes[ni];
        if (!node.mesh) {
            continue;
        }

        mergedAnyNodeMesh = true;
        auto& mesh = node.mesh;
        unsigned int baseVertex = static_cast<unsigned int>(mergedPositions.size());
        Matrix4 world = node.worldMatrix;
        Matrix4 normalMatrix = Matrix::Transpose(Matrix::Inverse(world));

        const auto& pos = mesh->GetPositionData();
        const auto& idx = mesh->GetIndexData();
        const auto& nrm = mesh->GetNormalData();
        const auto& tan = mesh->GetTangentData();
        const auto& uv  = mesh->GetTextureCoordData();

        for (const auto& v : pos) mergedPositions.push_back(TransformPosition(world, v));
        for (const auto& i : idx) mergedIndices.push_back(i + baseVertex);

        // Normals: pad front if first appearance, then append
        if (!nrm.empty()) {
            if (!hasNormals) {
                hasNormals = true;
                mergedNormals.resize(baseVertex, Vector3(0, 1, 0));
            }
            for (const auto& n : nrm) mergedNormals.push_back(TransformDirection(normalMatrix, n));
        } else if (hasNormals) {
            mergedNormals.resize(mergedPositions.size(), Vector3(0, 1, 0));
        }

        // Tangents: pad front if first appearance, then append
        if (!tan.empty()) {
            if (!hasTangents) {
                hasTangents = true;
                mergedTangents.resize(baseVertex, Vector4(1, 0, 0, 1));
            }
            for (const auto& t : tan) mergedTangents.push_back(TransformTangent(normalMatrix, t));
        } else if (hasTangents) {
            mergedTangents.resize(mergedPositions.size(), Vector4(1, 0, 0, 1));
        }

        // UVs: pad front if first appearance, then append
        if (!uv.empty()) {
            if (!hasTexCoords) {
                hasTexCoords = true;
                mergedTexCoords.resize(baseVertex, Vector2(0, 0));
            }
            for (const auto& u : uv) mergedTexCoords.push_back(u);
        } else if (hasTexCoords) {
            mergedTexCoords.resize(mergedPositions.size(), Vector2(0, 0));
        }

        LOG_INFO("[AssetManager] LoadMeshViaGLTF: merged node[" << ni << "] '" << node.name << "' — "
                 << pos.size() << " verts, " << idx.size() / 3 << " tris");
    }

    if (!mergedAnyNodeMesh) {
        LOG_WARN("[AssetManager] LoadMeshViaGLTF: scene nodes in '" << sourcePath << "' reference no meshes matching selector");
        delete result;
        return nullptr;
    }

    // Final alignment: if the first mesh(es) lacked an attribute but later ones had it,
    // the front portion needs padding too
    if (hasNormals)   mergedNormals.resize(mergedPositions.size(), Vector3(0, 1, 0));
    if (hasTangents)  mergedTangents.resize(mergedPositions.size(), Vector4(1, 0, 0, 1));
    if (hasTexCoords) mergedTexCoords.resize(mergedPositions.size(), Vector2(0, 0));

    if (selector.recenter && !mergedPositions.empty()) {
        Vector3 minPos = mergedPositions.front();
        Vector3 maxPos = mergedPositions.front();
        for (const auto& p : mergedPositions) {
            minPos.x = std::min(minPos.x, p.x); minPos.y = std::min(minPos.y, p.y); minPos.z = std::min(minPos.z, p.z);
            maxPos.x = std::max(maxPos.x, p.x); maxPos.y = std::max(maxPos.y, p.y); maxPos.z = std::max(maxPos.z, p.z);
        }
        Vector3 center = (minPos + maxPos) * 0.5f;
        for (auto& p : mergedPositions) {
            p -= center;
        }
        LOG_INFO("[AssetManager] LoadMeshViaGLTF: recentered mesh around local center ("
                 << center.x << "," << center.y << "," << center.z << ") for '" << fullPath << "'");
    }

    // ── Auto-generate missing tangents (mirrors aiProcess_CalcTangentSpace) ──
    // Without tangents, the vertex shader's normalize(vec3(0)) → NaN, corrupting
    // the TBN matrix and breaking all lighting.
    if (!hasTangents && hasNormals && hasTexCoords) {
        ComputeTangents(mergedPositions, mergedNormals, mergedTexCoords,
                        mergedIndices, mergedTangents);
        LOG_INFO("[AssetManager] LoadMeshViaGLTF: auto-generated tangents for '"
                 << fullPath << "' (" << mergedTangents.size() << " verts)");
    }

    result->SetVertexPositions(mergedPositions);
    result->SetVertexIndices(mergedIndices);
    if (!mergedNormals.empty())   result->SetVertexNormals(mergedNormals);
    if (!mergedTangents.empty())  result->SetVertexTangents(mergedTangents);
    if (!mergedTexCoords.empty()) result->SetVertexTextureCoords(mergedTexCoords);

    result->SetPrimitiveType(GeometryPrimitive::Triangles);
    result->UploadToGPU();

    LOG_INFO("[AssetManager] LoadMeshViaGLTF SUCCESS: '" << fullPath << "' → "
             << mergedPositions.size() << " total verts, "
             << mergedIndices.size() / 3 << " total tris, uploaded to GPU");

    return result;
}

bool AssetManager::LoadCollisionGeometry(const std::string& path,
                                          std::vector<NCL::Maths::Vector3>& outVerts,
                                          std::vector<int>& outIndices) {
    if (IsGltfFormat(path)) {
        LOG_INFO("[AssetManager] LoadCollisionGeometry: GLTF pipeline for '" << path << "'");
        return LoadCollisionFromGLTF(path, outVerts, outIndices);
    }
    if (IsAssimpFormat(path)) {
        LOG_INFO("[AssetManager] LoadCollisionGeometry: Assimp pipeline for '" << path << "'");
        return AssimpLoader::LoadCollisionGeometry(path, outVerts, outIndices);
    }
    LOG_ERROR("[AssetManager] LoadCollisionGeometry: unsupported format '" << path << "'");
    return false;
}

bool AssetManager::LoadCollisionFromGLTF(const std::string& fullPath,
                                          std::vector<NCL::Maths::Vector3>& outVerts,
                                          std::vector<int>& outIndices) {
    EnsureGLTFInitialized();

    LOG_INFO("[AssetManager] LoadCollisionFromGLTF: calling GLTFLoader::LoadFromPath('" << fullPath << "')");

    GLTFScene scene;
    if (!GLTFLoader::LoadFromPath(fullPath, scene)) {
        LOG_ERROR("[AssetManager] LoadCollisionFromGLTF FAILED: GLTFLoader::LoadFromPath returned false for '" << fullPath << "'");
        return false;
    }

    if (scene.meshes.empty()) {
        LOG_WARN("[AssetManager] LoadCollisionFromGLTF: GLTFScene has 0 meshes in '" << fullPath << "'");
        return false;
    }

    for (size_t mi = 0; mi < scene.meshes.size(); ++mi) {
        auto& mesh = scene.meshes[mi];
        int baseVertex = static_cast<int>(outVerts.size());

        const auto& pos = mesh->GetPositionData();
        const auto& idx = mesh->GetIndexData();

        for (const auto& v : pos) outVerts.push_back(v);
        for (const auto& i : idx) outIndices.push_back(static_cast<int>(i) + baseVertex);
    }

    LOG_INFO("[AssetManager] LoadCollisionFromGLTF SUCCESS: '" << fullPath << "' → "
             << outVerts.size() << " verts, " << outIndices.size() / 3 << " tris (no GPU upload)");

    return !outVerts.empty();
}

Mesh* AssetManager::CreateDefaultMesh() {
    Mesh* cubeMesh = m_MeshFactory();
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
                                        NCL::Rendering::Mesh* meshToFill) {
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
