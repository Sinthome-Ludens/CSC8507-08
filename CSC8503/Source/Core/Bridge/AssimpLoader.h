/**
 * @file AssimpLoader.h
 * @brief Assimp 模型加载适配器：将 Assimp 数据转换为 NCL Mesh 格式
 *
 * @details
 * `AssimpLoader` 是 Bridge 层的核心组件，负责使用 Assimp 库加载各种 3D 模型格式，
 * 并将数据转换为 NCL 的 `OGLMesh` 格式。
 *
 * ## 支持的格式
 *
 * - **OBJ** (.obj) - Wavefront Object
 * - **FBX** (.fbx) - Autodesk FBX
 * - **GLTF** (.gltf, .glb) - GL Transmission Format
 * - **Collada** (.dae) - COLLADA
 * - **Blender** (.blend) - Blender 3D
 * - **3DS** (.3ds) - 3D Studio
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
 * OGLMesh (NCL 数据结构)
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
 * OGLMesh* mesh = AssimpLoader::LoadMesh("Assets/Models/cube.obj");
 * if (mesh) {
 *     // 网格已上传到 GPU，可以直接渲染
 * }
 *
 * // 加载场景中的所有网格
 * std::vector<OGLMesh*> meshes = AssimpLoader::LoadScene("Assets/Models/scene.fbx");
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
 * @see OGLMesh (目标数据结构)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

// 包含 NCL 类型（用于返回值和参数）
#include "Vector.h"

// 前向声明 NCL Rendering 类型
namespace NCL::Rendering {
    class OGLMesh;
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
    /**
     * @brief 加载模型文件的第一个网格
     * @param path 模型文件路径（相对于 Assets/ 或绝对路径）
     * @return OGLMesh* 成功返回网格指针（已上传到 GPU），失败返回 nullptr
     *
     * @details
     * 使用 Assimp 加载模型文件，提取第一个网格并转换为 NCL 格式。
     * 适用于只包含单个网格的模型（如简单的 OBJ 文件）。
     */
    static NCL::Rendering::OGLMesh* LoadMesh(const std::string& path);

    /**
     * @brief 加载模型文件中的所有网格
     * @param path 模型文件路径
     * @return std::vector<OGLMesh*> 网格指针数组（可能为空）
     *
     * @details
     * 适用于包含多个子网格的复杂模型（如 FBX 场景）。
     * 每个网格都已上传到 GPU，可以独立渲染。
     */
    static std::vector<NCL::Rendering::OGLMesh*> LoadScene(const std::string& path);

private:
    AssimpLoader() = delete; // 禁止实例化
    ~AssimpLoader() = delete;
};

} // namespace ECS
