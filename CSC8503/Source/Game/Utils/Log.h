/**
 * @file Log.h
 * @brief 游戏日志宏定义
 *
 * 提供分级日志输出功能，用于开发调试和错误追踪。
 *
 * 日志级别：
 * - LOG_INFO: 普通信息输出，白色
 * - LOG_WARN: 警告信息输出，黄色
 * - LOG_ERROR: 错误信息输出，红色
 *
 * 实现原理：
 * - 使用 std::cout 输出到控制台
 * - 通过 ANSI 转义码实现颜色区分（Windows 需启用虚拟终端）
 * - Release 模式下所有日志宏会被完全移除
 *
 * 使用示例：
 * @code
 * LOG_INFO("Player spawned at position: " << x << ", " << y);
 * LOG_WARN("Low health: " << health);
 * LOG_ERROR("Failed to load asset: " << filename);
 * @endcode
 */

#pragma once

#ifdef _DEBUG
    #include <iostream>
    #include <fstream>
    #include <sstream>
    #include <filesystem>
    #include <mutex>
    #include <chrono>
    #include <iomanip>

    namespace ECS::DebugLog {
        inline std::mutex& MultiplayerLogMutex() {
            static std::mutex m;
            return m;
        }

        inline std::filesystem::path MultiplayerLogPath() {
            static const std::filesystem::path p =
                std::filesystem::current_path() / "multiplayer_debug.log";
            return p;
        }

        inline void AppendMultiplayerLogLine(const std::string& line) {
            std::lock_guard<std::mutex> lock(MultiplayerLogMutex());

            const auto path = MultiplayerLogPath();
            std::error_code ec;
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path(), ec);
            }

            std::ofstream out(path, std::ios::app);
            if (!out.is_open()) {
                std::cout << "[MPDBG][LOGGER_ERROR] Failed to open " << path.string() << std::endl;
                return;
            }

            const auto now = std::chrono::system_clock::now();
            const auto tt = std::chrono::system_clock::to_time_t(now);
            std::tm tmBuf{};
#ifdef _WIN32
            localtime_s(&tmBuf, &tt);
#else
            tmBuf = *std::localtime(&tt);
#endif

            out << "[" << std::put_time(&tmBuf, "%H:%M:%S") << "] " << line << std::endl;
        }
    }

    #define LOG_INFO(x)  std::cout << "[INFO] " << x << std::endl
    #define LOG_WARN(x)  std::cout << "\033[33m[WARN] " << x << "\033[0m" << std::endl
    #define LOG_ERROR(x) std::cout << "\033[31m[ERROR] " << x << "\033[0m" << std::endl
    #define LOG_MPDBG(x) do { std::ostringstream _mpdbg_ss; _mpdbg_ss << x; ECS::DebugLog::AppendMultiplayerLogLine(_mpdbg_ss.str()); } while(0)
#else
    #define LOG_INFO(x)
    #define LOG_WARN(x)
    #define LOG_ERROR(x)
    #define LOG_MPDBG(x)
#endif
