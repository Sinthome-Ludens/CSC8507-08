/**
 * @file C_D_MeshRenderer.h
 * @brief 网格渲染组件：引用模型网格和材质资源的 Handle
 *
 * @details
 * `C_D_MeshRenderer` 指定实体应渲染的 3D 网格（Mesh）和材质（Material）。
 * 通过 Handle 体系实现资源的间接引用，避免在组件中存储重型资源指针。
 *
 * ## Handle 体系设计
 *
 * **Handle** 是一个 `uint32_t` 的强类型别名，作为资源在 `AssetManager` 中的索引 ID：
 *
 * - **MeshHandle**：指向 `Mesh*` 的唯一 ID
 * - **MaterialHandle**：指向材质数据结构的唯一 ID（可包含 Texture + Shader）
 * - **TextureHandle**：指向 `Texture*` 的唯一 ID
 *
 * **优点**：
 * 1. **解耦资源生命周期**：Handle 失效时无悬空指针问题（AssetManager 返回默认资源）
 * 2. **序列化友好**：Handle 是整数，可直接写入 JSON / 网络包
 * 3. **引用计数**：AssetManager 根据 Handle 持有数量决定何时卸载资源
 *
 * ## 数据字段
 *
 * | 字段 | 类型 | 说明 |
 * |------|------|------|
 * | `meshHandle` | `uint32_t` | 网格资源 Handle（0 = 无效/默认立方体） |
 * | `materialHandle` | `uint32_t` | 材质资源 Handle（0 = 无效/默认紫黑格） |
 *
 * ## 设计约束
 *
 * 1. **POD 结构体**：仅包含整型 Handle，无指针/引用/虚函数。
 * 2. **懒惰解析**：Handle 在 `Sys_Render` 中通过 `AssetManager::GetMesh(handle)` 解析为实际资源。
 * 3. **默认值语义**：
 *    - `meshHandle = 0` → AssetManager 返回默认立方体网格
 *    - `materialHandle = 0` → AssetManager 返回默认紫黑格材质
 *
 * ## 使用流程
 *
 * 1. **加载阶段（PrefabFactory）**：
 *    ```cpp
 *    auto mesh = assetManager.LoadMesh("Assets/Models/Char_Hacker.obj");
 *    auto mat  = assetManager.LoadMaterial("Assets/Materials/Metal.mat");
 *    registry.Emplace<C_D_MeshRenderer>(entity, mesh, mat);
 *    ```
 *
 * 2. **渲染阶段（Sys_Render）**：
 *    ```cpp
 *    for (auto&& [id, tf, mr] : registry.view<C_D_Transform, C_D_MeshRenderer>()) {
 *        Mesh* mesh = assetManager.GetMesh(mr.meshHandle);
 *        // 构造 ModelMatrix，提交渲染
 *    }
 *    ```
 *
 * 3. **销毁阶段（Entity Destroy）**：
 *    - 组件被移除时，AssetManager 自动减少 Handle 引用计数
 *    - 引用计数归零时，资源进入"待卸载队列"，在场景切换时统一释放
 *
 * ## 性能考量
 *
 * - **内存占用**：8 字节 / 实体（两个 uint32_t）
 * - **缓存友好**：Handle 是整数，占用空间小，迭代速度快
 * - **批量渲染**：可按 `meshHandle` 排序实体，实现批量绘制（Instancing）
 *
 * @note 若未挂载此组件，实体将不会被 `Sys_Render` 渲染。
 *
 * @see AssetManager (Handle 到实际资源的解析器)
 * @see Sys_Render (读取此组件并提交渲染)
 */

#pragma once

#include <cstdint>

namespace ECS {

/// @brief 网格资源 Handle，指向 AssetManager 中的 Mesh*（0 = 无效）
using MeshHandle = uint32_t;

/// @brief 材质资源 Handle，指向材质数据结构（0 = 无效）
using MaterialHandle = uint32_t;

/// @brief 纹理资源 Handle，指向 Texture*（0 = 无效）
using TextureHandle = uint32_t;

/// @brief 着色器资源 Handle，指向 Shader*（0 = 无效）
using ShaderHandle = uint32_t;

/// @brief 骨骼动画剪辑 Handle，指向 AssetManager 中的 MeshAnimation*（0 = 无效）
using AnimHandle = uint32_t;

/// @brief 无效 Handle 哨兵值
static constexpr uint32_t INVALID_HANDLE = 0;

/**
 * @brief 网格渲染组件：引用实体要渲染的网格和材质（通过 Handle 间接引用）。
 *
 * @details
 * 所有需要被渲染的实体都应挂载此组件。Handle 在 `Sys_Render` 中通过 `AssetManager` 解析为实际资源。
 */
struct C_D_MeshRenderer {
    MeshHandle     meshHandle     = INVALID_HANDLE; ///< 网格资源 Handle（0 = 默认立方体）
    MaterialHandle materialHandle = INVALID_HANDLE; ///< 材质资源 Handle（0 = 默认紫黑格）
};

} // namespace ECS
