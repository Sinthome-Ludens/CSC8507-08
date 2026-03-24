/*
Part of Newcastle University's Game Engineering source code.

Use as you see fit!

Comments and queries to: richard-gordon.davison AT ncl.ac.uk
https://research.ncl.ac.uk/game/
*/
#pragma once

#ifdef ASSET_PATH_RUNTIME_RESOLVE
#include <windows.h>
#include <string>
namespace NCL::Assets {
	inline std::string ResolveAssetRoot() {
		char buf[MAX_PATH];
		GetModuleFileNameA(nullptr, buf, MAX_PATH);
		std::string p(buf);
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
