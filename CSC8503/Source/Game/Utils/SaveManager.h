/**
 * @file SaveManager.h
 * @brief JSON 存档读写工具：自动保存/加载玩家进度、库存、任务状态。
 *
 * @details
 * 存档路径：Assets/Saves/autosave.save（JSON 格式）。
 * 菜单阶段仅 Res_UIState 存在时，LoadGame 只填充 savedStoreCount 缓存。
 *
 * @see Res_GameState.h
 * @see Res_ItemInventory2.h
 * @see Res_UIState.h
 */
#pragma once

#include <string>

namespace ECS {
    class Registry;

    /**
     * @brief 将当前游戏状态序列化到 Assets/Saves/autosave.save。
     * @param registry ECS 注册表（读取 Res_GameState / Res_ItemInventory2 / Res_UIState）
     * @return true 保存成功，false 文件写入失败或异常
     */
    bool SaveGame(Registry& registry);

    /**
     * @brief 从 Assets/Saves/autosave.save 反序列化游戏状态到 ctx 资源。
     * @param registry ECS 注册表（写入存在的 ctx 资源；始终更新 Res_UIState 缓存）
     * @param restoreMission true 时恢复存档任务状态（菜单阶段），false 跳过（游戏阶段）
     * @return true 加载成功，false 文件不存在/版本不匹配/解析失败
     */
    bool LoadGame(Registry& registry, bool restoreMission = true);

    /**
     * @brief 检查 autosave.save 存档文件是否存在。
     * @return true 存档文件存在
     */
    bool HasSaveFile();

    /**
     * @brief 返回存档文件的完整路径。
     * @return Assets/Saves/autosave.save 的绝对路径字符串
     */
    std::string GetSavePath();
}
