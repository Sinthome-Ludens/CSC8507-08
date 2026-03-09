#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ECS { class Registry; }
struct Res_PostProcessConfig;

namespace ECS {

/// @brief 配置文件管理器：JSON 持久化所有 Debug 面板可调参数
/// 单例模式，启动时 LoadAll，退出时 SaveAll，运行时可手动 Save 单项
class ConfigManager {
public:
    static ConfigManager& Instance();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /// 设置配置文件根目录（默认 "Assets/Config/"）
    void SetConfigRoot(const std::string& root) { m_ConfigRoot = root; }

    /// 加载所有已注册的配置文件到 Registry context
    void LoadAll(Registry& registry);

    /// 保存所有 Registry context 中的配置到文件
    void SaveAll(Registry& registry);

    // ── 单项加载/保存 ─────────────────────────────────────────
    void LoadPostProcess(Registry& registry);
    void SavePostProcess(Registry& registry);

    void LoadPBRDefaults(Registry& registry);
    void SavePBRDefaults(Registry& registry);

    void LoadStylizedDefaults(Registry& registry);
    void SaveStylizedDefaults(Registry& registry);





private:
    ConfigManager() = default;

    std::string m_ConfigRoot = "Assets/Config/";

    /// 读取 JSON 文件，失败返回空 object
    nlohmann::json ReadJsonFile(const std::string& path);

    /// 写入 JSON 文件
    void WriteJsonFile(const std::string& path, const nlohmann::json& j);
};

} // namespace ECS
