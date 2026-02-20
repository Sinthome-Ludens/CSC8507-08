/**
 * @file Assert.h
 * @brief 游戏断言宏定义
 *
 * 提供运行时断言检查功能，用于验证程序假设和捕获逻辑错误。
 *
 * 实现原理：
 * - Debug 模式下检查条件，失败时输出错误信息并触发断点
 * - Release 模式下断言被完全移除，不产生任何运行时开销
 * - 使用 __debugbreak() 在 Windows 上触发调试器断点
 *
 * 注意事项：
 * - 断言应仅用于检查程序内部逻辑错误，不应用于用户输入验证
 * - 断言条件不应有副作用（如函数调用），因为 Release 下会被移除
 *
 * 使用示例：
 * @code
 * GAME_ASSERT(ptr != nullptr, "Pointer should not be null");
 * GAME_ASSERT(index < size, "Index out of bounds: " << index << " >= " << size);
 * @endcode
 */

#pragma once

#ifdef _DEBUG
    #include <iostream>
    #include <cstdlib>

    #define GAME_ASSERT(condition, message)                                     \
        do {                                                                     \
            if (!(condition)) {                                                  \
                std::cerr << "\033[31m[ASSERT FAILED] " << message               \
                          << "\nFile: " << __FILE__                              \
                          << "\nLine: " << __LINE__                              \
                          << "\033[0m" << std::endl;                             \
                __debugbreak();                                                  \
            }                                                                    \
        } while (0)
#else
    #define GAME_ASSERT(condition, message)
#endif
