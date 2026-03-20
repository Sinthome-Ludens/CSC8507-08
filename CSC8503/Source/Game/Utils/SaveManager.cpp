/**
 * @file SaveManager.cpp
 * @brief JSON 存档读写实现（nlohmann/json + fstream）。
 */
#include "SaveManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Utils/Log.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // localtime deprecation
#endif

namespace ECS {

static_assert(std::size(Res_UIState{}.savedStoreCount) >= Res_ItemInventory2::kItemCount,
              "savedStoreCount array must cover all ItemIDs");
static_assert(std::size(Res_UIState{}.savedUnlocked) >= Res_ItemInventory2::kItemCount,
              "savedUnlocked array must cover all ItemIDs");

static constexpr int kSaveVersion = 3;

/**
 * @brief v2 → v3 道具 ID 重映射表（删除 PhotonRadar，后续 ID 前移）。
 * 旧 0→新 0, 旧 1→跳过(-1), 旧 2→新 1, 旧 3→新 2, 旧 4→新 3, 旧 5→新 4
 */
static constexpr int kV2IdRemap[] = { 0, -1, 1, 2, 3, 4 };
static constexpr int kV2IdRemapSize = 6;

/**
 * @brief 返回存档文件的完整路径。
 * @return Assets/Saves/autosave.save 的绝对路径字符串
 */
std::string GetSavePath() {
    return NCL::Assets::ASSETROOT + "Saves/autosave.save";
}

/**
 * @brief 检查 autosave.save 存档文件是否存在。
 * @return true 存档文件存在
 */
bool HasSaveFile() {
    return std::filesystem::exists(GetSavePath());
}

/**
 * @brief 将当前游戏状态序列化到 Assets/Saves/autosave.save。
 * @details 序列化 Res_GameState（player 段）、Res_ItemInventory2（inventory 段，含 itemId+storeCount）、
 *          Res_UIState（mission 段）。自动创建 Saves/ 目录。
 * @param registry ECS 注册表（读取 Res_GameState / Res_ItemInventory2 / Res_UIState）
 * @return true 保存成功，false 文件写入失败或异常
 */
bool SaveGame(Registry& registry) {
    try {
        nlohmann::json root;
        root["version"] = kSaveVersion;

        // Timestamp
        {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm* tm = std::localtime(&t);
            std::ostringstream oss;
            oss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
            root["timestamp"] = oss.str();
        }

        // Player state
        if (registry.has_ctx<Res_GameState>()) {
            auto& gs = registry.ctx<Res_GameState>();
            root["player"] = {
                {"score",    gs.score},
                {"level",    gs.currentLevel},
                {"lives",    gs.playerLives},
                {"playTime", gs.playTime}
            };
        }

        // Inventory
        if (registry.has_ctx<Res_ItemInventory2>()) {
            auto& inv = registry.ctx<Res_ItemInventory2>();
            auto arr = nlohmann::json::array();
            for (int i = 0; i < inv.kItemCount; ++i) {
                arr.push_back({
                    {"itemId",     static_cast<int>(inv.slots[i].itemId)},
                    {"storeCount", inv.slots[i].storeCount},
                    {"unlocked",   inv.slots[i].unlocked}
                });
            }
            root["inventory"] = arr;
        }

        // Ensure directory exists
        std::string path = GetSavePath();
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path());

        // Write
        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("[SaveManager] Cannot open save file for writing: " << path);
            return false;
        }
        file << root.dump(2);
        file.close();

        LOG_INFO("[SaveManager] Game saved to " << path);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("[SaveManager] Save failed: " << e.what());
        return false;
    }
}

/**
 * @brief 从 Assets/Saves/autosave.save 反序列化游戏状态到 ctx 资源。
 * @details 按 itemId 匹配恢复 storeCount（而非数组下标），防止 ItemID 顺序变化导致错位。
 *          始终更新 Res_UIState.savedStoreCount 缓存（供菜单阶段 MissionSelect 使用）。
 * @param registry ECS 注册表（写入存在的 ctx 资源）
 * @param restoreMission true 时恢复存档任务状态（菜单阶段），false 跳过（游戏阶段保留用户选择）
 * @return true 加载成功，false 文件不存在/版本不匹配/解析失败
 */
bool LoadGame(Registry& registry, bool restoreMission) {
    std::string path = GetSavePath();
    if (!std::filesystem::exists(path)) {
        LOG_INFO("[SaveManager] No save file found at " << path);
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_WARN("[SaveManager] Cannot open save file: " << path);
            return false;
        }

        nlohmann::json root = nlohmann::json::parse(file);

        int version = root.value("version", 0);
        if (version != kSaveVersion && version != 2 && version != 1) {
            LOG_WARN("[SaveManager] Unsupported save version: " << version);
            return false;
        }

        // Restore player state
        if (root.contains("player") && registry.has_ctx<Res_GameState>()) {
            auto& gs = registry.ctx<Res_GameState>();
            auto& p  = root["player"];
            gs.score        = p.value("score",    0u);
            gs.currentLevel = p.value("level",    1u);
            gs.playerLives  = p.value("lives",    3u);
            gs.playTime     = p.value("playTime", 0.0f);
        }

        // Restore inventory storeCount
        if (root.contains("inventory")) {
            auto& arr = root["inventory"];
            const bool needsRemap = (version <= 2); // v1/v2 存档需要 ID 重映射

            // If Res_ItemInventory2 exists (in-game), restore by itemId
            if (registry.has_ctx<Res_ItemInventory2>()) {
                auto& inv = registry.ctx<Res_ItemInventory2>();
                for (auto& entry : arr) {
                    int id = entry.value("itemId", -1);
                    if (needsRemap) {
                        if (id < 0 || id >= kV2IdRemapSize) continue;
                        id = kV2IdRemap[id];
                        if (id < 0) continue; // PhotonRadar → skip
                    }
                    if (id < 0 || id >= inv.kItemCount) continue;
                    inv.slots[id].storeCount = entry.value("storeCount", static_cast<uint8_t>(0));
                    inv.slots[id].unlocked   = entry.value("unlocked", false);
                }
            }

            // Always update UIState cache by itemId (for menu stage fallback)
            if (registry.has_ctx<Res_UIState>()) {
                auto& ui = registry.ctx<Res_UIState>();
                for (auto& entry : arr) {
                    int id = entry.value("itemId", -1);
                    if (needsRemap) {
                        if (id < 0 || id >= kV2IdRemapSize) continue;
                        id = kV2IdRemap[id];
                        if (id < 0) continue; // PhotonRadar → skip
                    }
                    if (id < 0 || id >= static_cast<int>(std::size(ui.savedStoreCount))) continue;
                    ui.savedStoreCount[id] = entry.value("storeCount", static_cast<uint8_t>(0));
                    ui.savedUnlocked[id]   = entry.value("unlocked", false);
                }
                ui.hasSavedInventory = true;
            }
        }

        LOG_INFO("[SaveManager] Game loaded from " << path);
        return true;

    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("[SaveManager] JSON parse error: " << e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("[SaveManager] Load failed: " << e.what());
        return false;
    }
}

} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
