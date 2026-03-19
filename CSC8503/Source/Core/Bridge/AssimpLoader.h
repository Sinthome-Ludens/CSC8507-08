/**
 * @file AssimpLoader.h
 * @brief Assimp 模型加载适配器：将 Assimp 数据转换为 NCL Mesh 格式
 *
 * @details
 * `AssimpLoader` 是 Bridge 层的核心组件，负责使用 Assimp 库加载各种 3D 模型格式，
 * 并将数据转换为 NCL 的 `Mesh` 格式（具体子类由注入的 MeshFactory 决定）。
 *
 * ## 支持的格式（Assimp 路径）
 *
 * - **OBJ** (.obj) - Wavefront Object
 * - **FBX** (.fbx) - Autodesk FBX
 * - **Collada** (.dae) - COLLADA
 * - **Blender** (.blend) - Blender 3D
 *
 * GLTF (.gltf/.glb) 由 AssetManager 路由到 GLTFLoader，不经过 AssimpLoader。
 *
 * ## 数据流
 *
 * ```
 * 模型文件 (.obj/.fbx/...)
 *   ↓
 * Assimp::aiImportFile()
 *   ↓
 * aiScene (Assimp 数据结构)
 *   ↓
 * AssimpLoader::ConvertMesh()
 *   ↓
 * Mesh* (由 MeshFactory 创建的具体子类)
 *   ↓
 * UploadToGPU()
 * ```
 *
 * ## 顶点属性映射
 *
 * | Assimp 属性 | NCL 属性 | Shader Location |
 * |-------------|----------|-----------------|
 * | aiMesh->mVertices | Positions | location 0 |
 * | aiMesh->mNormals | Normals | location 3 |
 * | aiMesh->mTextureCoords[0] | TextureCoords | location 2 |
 * | aiMesh->mColors[0] | Colours | location 1 |
 * | aiMesh->mTangents | Tangents | location 4 |
 *
 * ## 后处理标志
 *
 * 使用以下 Assimp 后处理标志优化导入：
 * - `aiProcess_Triangulate` - 三角化所有面
 * - `aiProcess_GenNormals` - 生成法线（如果缺失）
 * - `aiProcess_CalcTangentSpace` - 计算切线空间
 * - `aiProcess_FlipUVs` - 翻转 UV（OpenGL 约定）
 * - `aiProcess_JoinIdenticalVertices` - 合并重复顶点
 * - `aiProcess_OptimizeMeshes` - 优化网格
 *
 * ## 使用示例
 *
 * @code
 * // 加载单个网格
 * Mesh* mesh = AssimpLoader::LoadMesh("Assets/Models/cube.obj");
 * if (mesh) {
 *     // 网格已上传到 GPU，可以直接渲染
 * }
 *
 * // 加载场景中的所有网格
 * std::vector<Mesh*> meshes = AssimpLoader::LoadScene("Assets/Models/scene.fbx");
 * @endcode
 *
 * ## 错误处理
 *
 * - 文件不存在 → 返回 `nullptr`，输出错误日志
 * - Assimp 导入失败 → 返回 `nullptr`，输出 `aiGetErrorString()`
 * - 空场景（无网格）→ 返回 `nullptr`，输出警告
 *
 * @note 当前实现仅支持静态网格，骨骼动画支持将在后续版本添加。
 *
 * @see AssetManager (调用 LoadMesh)
 * @see Mesh (目标数据结构基类)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

// 包含 NCL 类型（用于返回值和参数）
#include "Vector.h"

namespace NCL::Rendering {
    class Mesh;
    class MeshAnimation;
}

namespace ECS {

/**
 * @brief Assimp 模型加载适配器（静态工具类）
 *
 * @details
 * 无状态设计，所有方法均为静态函数。负责将 Assimp 的数据结构转换为 NCL 的 Mesh 格式。
 */
class AssimpLoader {
public:
    /// @brief Mesh 工厂函数类型（由 AssetManager 注入）
    using MeshFactory = std::function<NCL::Rendering::Mesh*()>;

    /**
     * @brief 注入 Mesh 工厂函数，决定运行时创建哪种 Mesh 子类。
     * @details 由 AssetManager::SetMeshFactory() 自动调用。
     */
    static void SetMeshFactory(MeshFactory factory);

    /**
     * @brief 加载模型文件的第一个网格
     * @param path 模型文件路径（相对于 Assets/ 或绝对路径）
     * @return Mesh* 成功返回网格指针（已上传到 GPU），失败返回 nullptr
     */
    static NCL::Rendering::Mesh* LoadMesh(const std::string& path);

    /**
     * @brief 加载模型文件中的所有网格
     * @param path 模型文件路径
     * @return std::vector<Mesh*> 网格指针数组（可能为空）
     */
    static std::vector<NCL::Rendering::Mesh*> LoadScene(const std::string& path);

    /**
     * @brief 从模型文件提取碰撞用三角网格数据（不上传 GPU）
     *
     * 加载 OBJ/FBX 等文件，仅提取顶点位置和三角形索引，用于创建
     * 独立于渲染 mesh 的碰撞体（碰撞箱与 mesh 不相等）。
     *
     * @param path           模型文件路径
     * @param outVertices    输出顶点坐标列表
     * @param outIndices     输出三角形索引列表（int 类型，每 3 个为一个三角形）
     * @return true 成功，false 文件不存在或格式错误
     */
    static bool LoadCollisionGeometry(
        const std::string& path,
        std::vector<NCL::Maths::Vector3>& outVertices,
        std::vector<int>& outIndices
    );

    /**
     * @brief 从 FBX/GLTF 等文件加载骨骼动画剪辑，同时填充网格骨骼绑定姿态
     *
     * @param path       模型文件路径（含骨骼动画数据）
     * @param meshToFill 需要填充 inverseBindPose / jointNames / jointParents 的目标网格，可为 nullptr
     * @return MeshAnimation* 成功返回动画对象（堆分配，调用者负责释放），失败返回 nullptr
     *
     * @details
     * 提取第一个动画剪辑，将所有帧的关节世界矩阵存储在 MeshAnimation::allJoints 中。
     * 若 meshToFill 不为 nullptr，同时填充骨骼绑定信息（bindPose、inverseBindPose 等）。
     */
    static NCL::Rendering::MeshAnimation* LoadAnimation(const std::string& path,
                                                         NCL::Rendering::Mesh* meshToFill = nullptr);

    static MeshFactory s_MeshFactory; ///< 由 SetMeshFactory 注入

private:
    AssimpLoader() = delete; // 禁止实例化
    ~AssimpLoader() = delete;
};

} // namespace ECS
