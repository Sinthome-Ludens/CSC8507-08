#pragma once

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

		struct GameTechMaterial
		{
			MaterialType	type		= MaterialType::Opaque;
			Texture*		diffuseTex	= nullptr;
			Texture*		bumpTex		= nullptr;

			// ── 着色模型选择 ────────────────────────────────
			ShadingModel	shadingModel = ShadingModel::BlinnPhong;

			// ── PBR 参数 ───────────────────────────────────
			float metallic  = 0.0f;
			float roughness = 0.5f;
			float ao        = 1.0f;

			// ── Stylized 参数 ──────────────────────────────
			Vector3 emissiveColor    = Vector3(0, 0, 0);
			float   emissiveStrength = 0.0f;
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

		protected:
			Transform&	transform;
			GameTechMaterial material;

			Mesh*		mesh;
			Vector4		colour;
		};
	}
}
