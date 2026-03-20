/**
 * @file Res_DialogueConfig.h
 * @brief 对话系统配置资源：对话 JSON 文件路径（消除 Sys_Chat 中的硬编码文件名）。
 */
#pragma once

namespace ECS {

struct Res_DialogueConfig {
    /// 对话文件路径（相对于 Assets/，含子目录）
    char proactiveFile[64] = "Dialogue/Dialogue_Normal_EN.json";
    char mixedFile[64]     = "Dialogue/Dialogue_Alert_EN.json";
    char passiveFile[64]   = "Dialogue/Dialogue_Exposed_EN.json";
};

} // namespace ECS
