#pragma once
#include <string>
#include <vector>

namespace NCL::Rendering {
	class Mesh;
	class Texture;
	class Shader;
}

namespace NCL::CSC8503 {
	class GameObject;

	class GameTechRendererInterface
	{
	public:
		virtual NCL::Rendering::Mesh*		LoadMesh(const std::string& name)		= 0;
		virtual NCL::Rendering::Texture*	LoadTexture(const std::string& name)	= 0;
		virtual void SetWireframeMode(bool /*enabled*/) {}

		/// @brief 设置数据海洋柱子代理列表指针（由 Sys_Render 每帧调用）
		virtual void SetOceanPillarProxies(const std::vector<GameObject*>* /*proxies*/) {}
		/// @brief 设置数据海洋 GPU 噪波时间（由 Sys_Render 每帧调用）
		virtual void SetOceanTime(float /*time*/) {}
		/// @brief 设置数据海洋噪波参数（由 Sys_Render 在 OnAwake 或配置变更时调用）
		virtual void SetOceanNoiseParams(float /*scale*/, float /*speed*/, float /*amplitude*/) {}
	};
}

