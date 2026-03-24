/**
 * @file Assets.h
 * @brief 资产根目录及子目录路径定义。
 *
 * 开发构建使用 CMake 编译时注入的绝对路径 ASSETROOTLOCATION；
 * Shipping 构建（ASSET_PATH_RUNTIME_RESOLVE）在运行时通过
 * GetModuleFileNameA 解析 exe 所在目录，拼接 Assets/ 作为根路径，
 * 使独立打包的 exe 可从任意位置运行。
 */
#pragma once

#ifdef ASSET_PATH_RUNTIME_RESOLVE
#include <windows.h>
#include <string>
namespace NCL::Assets {
	/**
	 * @brief 运行时解析资产根目录（Shipping 构建专用）。
	 * @return exe 所在目录 + "Assets/"；失败时回退到相对路径 "Assets/"。
	 */
	inline std::string ResolveAssetRoot() {
		char buf[MAX_PATH];
		DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
		if (len == 0 || len >= MAX_PATH) {
			return std::string("Assets/");
		}
		std::string p(buf, static_cast<std::size_t>(len));
		auto pos = p.find_last_of("\\/");
		if (pos != std::string::npos) p = p.substr(0, pos + 1);
		return p + "Assets/";
	}
	const std::string ASSETROOT = ResolveAssetRoot();
#else
namespace NCL::Assets {
	const std::string ASSETROOT(ASSETROOTLOCATION);
#endif
	const std::string SHADERDIR(ASSETROOT + "Shaders/");
	const std::string MESHDIR(ASSETROOT + "Meshes/");
	const std::string TEXTUREDIR(ASSETROOT + "Textures/");
	const std::string SOUNDSDIR(ASSETROOT + "Sounds/");
	const std::string FONTSSDIR(ASSETROOT + "Fonts/");
	const std::string DATADIR(ASSETROOT + "Data/");

	extern bool ReadTextFile(const std::string& filepath, std::string& result);
	extern bool ReadBinaryFile(const std::string& filepath, char** into, size_t& size);
}
