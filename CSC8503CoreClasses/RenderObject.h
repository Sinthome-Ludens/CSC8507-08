#pragma once
#include <vector>

namespace NCL {
	namespace Rendering {
		class Texture;
		class Shader;
		class Mesh;
	}
	using namespace NCL::Rendering;

	namespace CSC8503 {
		class Transform;
		using namespace Maths;

		enum class MaterialType{
			Opaque,
			Transparent,
			Effect
		};

		/// @brief 着色模型（与 ECS::ShadingModel 对应）
		enum class ShadingModel : int {
			BlinnPhong = 0,
			PBR        = 1,
			Stylized   = 2
		};

		/// @brief Alpha 混合模式（与 ECS::AlphaMode 对应）
		enum class AlphaMode : uint8_t {
			Opaque = 0, ///< 完全不透明
			Mask   = 1, ///< Alpha 测试
			Blend  = 2  ///< 半透明混合
		};

		struct GameTechMaterial
		{
			MaterialType	type		= MaterialType::Opaque;
			Texture*		diffuseTex	= nullptr; ///< Albedo / diffuse 纹理（unit 0）
			Texture*		bumpTex		= nullptr; ///< 切线空间法线贴图（unit 1）
			Texture*		ormTex		= nullptr; ///< R=occlusion G=roughness B=metallic（unit 2）
			Texture*		emissiveTex	= nullptr; ///< 自发光纹理（unit 3）

			// ── 着色模型选择 ────────────────────────────────
			ShadingModel	shadingModel = ShadingModel::BlinnPhong;

			// ── Alpha 模式 ──────────────────────────────────
			AlphaMode	alphaMode   = AlphaMode::Opaque;
			float		alphaCutoff = 0.5f;
			bool		doubleSided = false;

			// ── PBR 参数 ───────────────────────────────────
			float metallic  = 0.0f;
			float roughness = 0.5f;
			float ao        = 1.0f;

			// ── Stylized 参数 ──────────────────────────────
			Vector3 emissiveColor    = Vector3(0, 0, 0);
			float   emissiveStrength = 0.0f;
			Vector3 rimColour        = Vector3(1, 1, 1);
			float   rimPower         = 3.0f;
			float   rimStrength      = 0.5f;
			bool    flatShading      = false;
		};

		class RenderObject
		{
		public:
			RenderObject(Transform& parentTransform, Mesh* mesh, const GameTechMaterial& material);
			~RenderObject() = default;

			Mesh*	GetMesh() const
			{
				return mesh;
			}

			Transform&		GetTransform() const
			{
				return transform;
			}

			void SetColour(const Vector4& c)
			{
				colour = c;
			}

			Vector4 GetColour() const
			{
				return colour;
			}

			GameTechMaterial& GetMaterial()
			{
				return material;
			}

			const GameTechMaterial& GetMaterial() const
			{
				return material;
			}

			// ── 骨骼蒙皮（由 Sys_Render 每帧写入，GameTechRenderer 上传到 shader）──
			bool                 useSkinning     = false;
			std::vector<Matrix4> skinBoneMatrices; ///< 仅在 useSkinning 时填充

		protected:
			Transform&	transform;
			GameTechMaterial material;

			Mesh*		mesh;
			Vector4		colour;
		};
	}
}
