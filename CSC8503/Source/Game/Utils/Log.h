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

    #define LOG_INFO(x)  std::cout << "[INFO] " << x << std::endl
    #define LOG_WARN(x)  std::cout << "\033[33m[WARN] " << x << "\033[0m" << std::endl
    #define LOG_ERROR(x) std::cout << "\033[31m[ERROR] " << x << "\033[0m" << std::endl
#else
    #define LOG_INFO(x)
    #define LOG_WARN(x)
    #define LOG_ERROR(x)
#endif
