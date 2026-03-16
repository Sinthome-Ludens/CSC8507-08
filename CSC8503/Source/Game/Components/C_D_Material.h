/**
 * @file C_D_Material.h
 * @brief 统一材质组件（PBR/Stylized/BlinnPhong），含纹理句柄与 Alpha 模式。
 */
#pragma once

#include "Vector.h"
#include "Game/Components/C_D_MeshRenderer.h" // TextureHandle

namespace ECS {

enum class ShadingModel : int {
    BlinnPhong = 0,
    PBR        = 1,
    Stylized   = 2
};

/// @brief Alpha 混合模式（与 GameTechMaterial::AlphaMode 对应）
enum class AlphaMode : uint8_t {
    Opaque = 0, ///< 完全不透明，写深度
    Mask   = 1, ///< Alpha 测试：低于 alphaCutoff 的片元被丢弃
    Blend  = 2  ///< 半透明混合，不写深度，需 back-to-front 排序
};

/// @brief 统一材质组件：PBR + Stylized + BlinnPhong 三种着色模型共存
struct C_D_Material {
    ShadingModel shadingModel = ShadingModel::BlinnPhong;

    // ── 基础颜色（所有着色模型通用）────────────────────────
    NCL::Maths::Vector4 baseColour = {1.0f, 1.0f, 1.0f, 1.0f};

    // ── 纹理句柄（由 AssetManager 解析为 OGLTexture*）──────
    TextureHandle albedoHandle   = INVALID_HANDLE; ///< RGBA 基础色纹理
    TextureHandle normalHandle   = INVALID_HANDLE; ///< 切线空间法线贴图
    TextureHandle ormHandle      = INVALID_HANDLE; ///< R=occlusion G=roughness B=metallic
    TextureHandle emissiveHandle = INVALID_HANDLE; ///< RGB 自发光纹理

    // ── Alpha 模式 ───────────────────────────────────────
    AlphaMode     alphaMode  = AlphaMode::Opaque;
    float         alphaCutoff = 0.5f;
    bool          doubleSided = false;

    // ── PBR 参数 (shadingModel == PBR) ──────────────────────
    float metallic  = 0.0f;
    float roughness = 0.5f;
    float ao        = 1.0f;

    // ── Stylized 参数 (shadingModel == Stylized / BlinnPhong) ─
    NCL::Maths::Vector3 emissiveColor    = {0, 0, 0};
    float               emissiveStrength = 0.0f;
    float               rimPower         = 3.0f;
    float               rimStrength      = 0.5f;
    bool                flatShading      = false;
};

} // namespace ECS
