/**
 * @file SaveManager.cpp
 * @brief JSON 存档读写实现（nlohmann/json + fstream）。
 */
#include "SaveManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
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

static constexpr int kSaveVersion = 1;

std::string GetSavePath() {
    return NCL::Assets::ASSETROOT + "Saves/autosave.save";
}

bool HasSaveFile() {
    return std::filesystem::exists(GetSavePath());
}

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
                    {"storeCount", inv.slots[i].storeCount}
                });
            }
            root["inventory"] = arr;
        }

        // Mission
        if (registry.has_ctx<Res_UIState>()) {
            auto& ui = registry.ctx<Res_UIState>();
            root["mission"] = {
                {"selectedMap", static_cast<int>(ui.missionSelectedMap)}
            };
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

bool LoadGame(Registry& registry) {
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
        if (version != kSaveVersion) {
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

            // If Res_ItemInventory2 exists (in-game), restore directly
            if (registry.has_ctx<Res_ItemInventory2>()) {
                auto& inv = registry.ctx<Res_ItemInventory2>();
                for (size_t i = 0; i < arr.size() && i < static_cast<size_t>(inv.kItemCount); ++i) {
                    inv.slots[i].storeCount = arr[i].value("storeCount", static_cast<uint8_t>(0));
                }
            }

            // Always update UIState cache (for menu stage fallback)
            if (registry.has_ctx<Res_UIState>()) {
                auto& ui = registry.ctx<Res_UIState>();
                for (size_t i = 0; i < arr.size() && i < 5u; ++i) {
                    ui.savedStoreCount[i] = arr[i].value("storeCount", static_cast<uint8_t>(0));
                }
                ui.hasSavedInventory = true;
            }
        }

        // Restore mission selection
        if (root.contains("mission") && registry.has_ctx<Res_UIState>()) {
            auto& ui = registry.ctx<Res_UIState>();
            ui.missionSelectedMap = static_cast<int8_t>(
                root["mission"].value("selectedMap", 0));
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
