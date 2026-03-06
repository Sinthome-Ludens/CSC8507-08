#pragma once

#include "Game/Components/Res_DialogueData.h"
#include <string>

namespace ECS {

/// 从 JSON 文件加载对话数据到 Res_DialogueData（纯工具函数，不依赖 Registry）
bool LoadDialogueFromJSON(const std::string& filepath, Res_DialogueData& outData);

} // namespace ECS
