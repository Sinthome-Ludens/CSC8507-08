/**
 * @file AssetManager.h
 * @brief 资源管理器：Handle 体系、引用计数、默认资源回退
 *
 * @details
 * `AssetManager` 是游戏引擎的资源中心，负责加载、缓存和释放所有外部资产（网格、纹理、着色器）。
 *
 * ## Handle 体系设计
 *
 * **Handle** 是资源的唯一标识符（`uint32_t`），组件只存储 Handle 而非裸指针：
 *
 * - **MeshHandle**：指向 `OGLMesh*`
 * - **TextureHandle**：指向 `OGLTexture*`
 * - **ShaderHandle**：指向 `OGLShader*`
 *
 * **优点**：
 * 1. **解耦生命周期**：资源卸载不会导致组件持有悬空指针
 * 2. **序列化友好**：Handle 是整数，可直接写入 JSON / 网络包
 * 3. **引用计数**：自动追踪资源使用情况，按需卸载
 *
 * ## 引用计数机制
 *
 * - **LoadMesh(path)**：引用计数 +1，返回 Handle
 * - **Component 销毁**：引用计数 -1
 * - **引用计数归零**：资源进入"待回收队列"，在场景切换时统一释放
 *
 * ## 默认资源回退
 *
 * 当资源加载失败或 Handle 无效时，AssetManager 返回内置的默认资源：
 *
 * | 资源类型 | 默认资源 |
 * |----------|----------|
 * | **Mesh** | 单位立方体（1x1x1） |
 * | **Texture** | 紫黑格纹理（2x2 棋盘格） |
 * | **Shader** | 基础光照着色器 |
 *
 * **目的**：确保游戏不会崩溃，且能明显提示资源缺失。
 *
 * ## 资源加载流程
 *
 * ```
 * 1. System 调用 LoadMesh("Assets/Models/Orc.obj")
 * 2. AssetManager 检查缓存（m_MeshCache）
 * 3. 若命中：返回已有 Handle，引用计数 +1
 * 4. 若未命中：
 *    a. 调用 NCL::Msh::LoadMesh() 从磁盘加载
 *    b. 若加载成功：生成新 Handle，存入缓存
 *    c. 若加载失败：返回默认网格的 Handle
 * 5. 返回 Handle 给调用者
 * ```
 *
 * ## API 摘要
 *
 * | 方法 | 作用 |
 * |------|------|
 * | `LoadMesh(path)` | 加载网格，返回 Handle |
 * | `GetMesh(handle)` | 解析 Handle 为 OGLMesh* |
 * | `ReleaseMesh(handle)` | 减少引用计数 |
 * | `UnloadUnused()` | 卸载所有引用计数为 0 的资源 |
 * | `Clear()` | 清空所有缓存（场景切换时调用） |
 *
 * ## 使用示例
 *
 * @code
 * // 在 PrefabFactory 中加载资源
 * AssetManager& assetMgr = AssetManager::Instance();
 * MeshHandle orcMesh = assetMgr.LoadMesh("Assets/Models/Orc.obj");
 * registry.Emplace<C_D_MeshRenderer>(entity, orcMesh, 0);
 *
 * // 在 Sys_Render 中解析 Handle
 * OGLMesh* mesh = assetMgr.GetMesh(orcMesh); // 快速 O(1) 查找
 * renderer->DrawMesh(mesh, transform);
 * @endcode
 *
 * ## 性能考量
 *
 * - **缓存命中**：O(1) 哈希表查找
 * - **磁盘 I/O**：仅在首次加载时触发
 * - **内存占用**：按需加载，场景切换时统一释放
 *
 * ## 线程安全
 *
 * AssetManager **不是线程安全的**，所有操作应在主线程进行。
 *
 * @note 默认资源在 `Init()` 时预加载，永不卸载。
 *
 * @see C_D_MeshRenderer (存储 Handle 的组件)
 * @see Sys_Render (使用 AssetManager 解析 Handle)
 */

#pragma once

#include "Game/Components/C_D_MeshRenderer.h" // Handle 类型定义
#include "AssimpLoader.h" // Assimp 加载器
#include <unordered_map>
#include <string>
#include <memory>

// 前向声明 NCL 类型（避免头文件循环依赖）
namespace NCL {
    class MeshGeometry;
    namespace Rendering {
        class OGLMesh;
        class OGLTexture;
        class OGLShader;
    }
}

namespace ECS {

/**
 * @brief 资源管理器：Handle 体系、引用计数、默认资源回退。
 *
 * @details
 * 单例模式，全局唯一实例。在游戏启动时调用 `Init()` 初始化，场景切换时调用 `Clear()`。
 */
class AssetManager {
public:
    /**
     * @brief 获取单例实例。
     * @return AssetManager 的全局引用。
     */
    static AssetManager& Instance();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /**
     * @brief 初始化资源管理器，加载默认资源。
     * @details 应在游戏启动时调用一次。
     */
    void Init();

    /**
     * @brief 清空所有缓存资源（除默认资源外）。
     * @details 场景切换时调用，释放旧场景的资源。
     */
    void Clear();

    /**
     * @brief 卸载所有引用计数为 0 的资源。
     * @details 可选的优化操作，通常在场景切换时调用。
     */
    void UnloadUnused();

    // -------------------------------------------------------------------------
    // Mesh 资源管理
    // -------------------------------------------------------------------------

    /**
     * @brief 加载网格资源，返回 Handle。
     * @param path 相对于 Assets/ 的路径（如 "Models/Orc.obj"）
     * @return MeshHandle，若加载失败则返回默认立方体的 Handle
     */
    MeshHandle LoadMesh(const std::string& path);

    /**
     * @brief 解析 MeshHandle 为实际的 OGLMesh 指针。
     * @param handle 目标 Handle，若为 0 则返回默认立方体
     * @return OGLMesh 指针（非空，失败时返回默认资源）
     */
    NCL::Rendering::OGLMesh* GetMesh(MeshHandle handle);

    /**
     * @brief 减少网格资源的引用计数。
     * @param handle 目标 Handle
     * @details 当组件被销毁时自动调用（可选功能，暂未实现）
     */
    void ReleaseMesh(MeshHandle handle);

    // -------------------------------------------------------------------------
    // Texture 资源管理
    // -------------------------------------------------------------------------

    /**
     * @brief 加载纹理资源，返回 Handle。
     * @param path 相对于 Assets/ 的路径（如 "Textures/Orc_Diffuse.png"）
     * @return TextureHandle，若加载失败则返回默认紫黑格纹理的 Handle
     */
    TextureHandle LoadTexture(const std::string& path);

    /**
     * @brief 解析 TextureHandle 为实际的 OGLTexture 指针。
     * @param handle 目标 Handle，若为 0 则返回默认紫黑格纹理
     * @return OGLTexture 指针（非空）
     */
    NCL::Rendering::OGLTexture* GetTexture(TextureHandle handle);

    /**
     * @brief 减少纹理资源的引用计数。
     * @param handle 目标 Handle
     */
    void ReleaseTexture(TextureHandle handle);

private:
    AssetManager() = default;
    ~AssetManager();

    /**
     * @brief 检查文件扩展名是否为 Assimp 支持的格式
     * @param path 文件路径
     * @return true 如果是 Assimp 格式（.obj, .fbx, .gltf, .dae, .blend 等）
     */
    static bool IsAssimpFormat(const std::string& path);

    /// @brief 资源条目，包含资源指针和引用计数
    template<typename T>
    struct ResourceEntry {
        std::unique_ptr<T> resource;
        uint32_t refCount = 0;
    };

    std::unordered_map<MeshHandle, ResourceEntry<NCL::Rendering::OGLMesh>>    m_MeshCache;
    std::unordered_map<TextureHandle, ResourceEntry<NCL::Rendering::OGLTexture>> m_TextureCache;
    std::unordered_map<std::string, MeshHandle>    m_PathToMeshHandle;    // 路径 -> Handle 映射
    std::unordered_map<std::string, TextureHandle> m_PathToTextureHandle; // 路径 -> Handle 映射

    MeshHandle    m_NextMeshHandle    = 1; // Handle 生成器（0 保留为无效值）
    TextureHandle m_NextTextureHandle = 1;

    MeshHandle    m_DefaultMeshHandle    = INVALID_HANDLE; // 默认立方体 Handle
    TextureHandle m_DefaultTextureHandle = INVALID_HANDLE; // 默认紫黑格 Handle

    /**
     * @brief 创建默认立方体网格。
     * @return OGLMesh 指针（1x1x1 单位立方体）
     */
    NCL::Rendering::OGLMesh* CreateDefaultMesh();

    /**
     * @brief 创建默认紫黑格纹理。
     * @return OGLTexture 指针（2x2 棋盘格）
     */
    NCL::Rendering::OGLTexture* CreateDefaultTexture();
};

} // namespace ECS
