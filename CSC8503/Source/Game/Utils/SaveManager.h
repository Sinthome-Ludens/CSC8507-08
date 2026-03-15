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

    bool SaveGame(Registry& registry);
    bool LoadGame(Registry& registry);
    bool HasSaveFile();
    std::string GetSavePath();
}
