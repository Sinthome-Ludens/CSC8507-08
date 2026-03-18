/**
 * @file DialogueLoader.h
 * @brief JSON 对话数据加载工具（纯函数，不依赖 Registry）
 */
#pragma once

#include "Game/Components/Res_DialogueData.h"
#include <string>

namespace ECS {

/// 从单个 JSON 文件加载一个 DialogueSequence（一个警戒等级对应一个文件）
bool LoadDialogueSequenceFromJSON(const std::string& filepath, DialogueSequence& outSeq);

} // namespace ECS
