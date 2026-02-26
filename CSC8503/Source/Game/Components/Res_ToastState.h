#pragma once

#include <cstdint>
#include <cstdio>

namespace ECS {

/**
 * @brief Toast通知系统全局状态资源
 *
 * 环形缓冲区存储最多8条活跃通知。
 * 由任意系统通过 PushToast() 写入，由 UI_Toast 读取渲染。
 *
 * @note 作为 Registry Context 资源（非 per-entity Component），
 *       不受 64 字节限制，但仍保持 POD 特性（无动态分配）。
 */

enum class ToastType : uint8_t {
    Info    = 0,   // 青色 — 一般信息
    Warning = 1,   // 琥珀色 — 警告
    Danger  = 2,   // 红色 — 危险
    Success = 3,   // 绿色 — 成功
};

struct ToastEntry {
    char      text[80] = {};
    ToastType type     = ToastType::Info;
    float     lifespan = 0.0f;   // 剩余寿命（倒计时）
    float     maxLife  = 0.0f;   // 原始寿命（用于计算淡出比例）
    float     age      = 0.0f;   // 已存活时间（正计时，用于滑入动画）
    bool      active   = false;
};

struct Res_ToastState {
    static constexpr int   MAX_TOASTS        = 8;
    static constexpr float DEFAULT_LIFESPAN  = 4.0f;

    ToastEntry entries[MAX_TOASTS] = {};
    uint8_t    head  = 0;   // 下一个写入位置
    uint8_t    count = 0;   // 累计写入数（用于调试，不回绕）

    /// 推送一条新 Toast
    void PushToast(const char* msg, ToastType type, float life = DEFAULT_LIFESPAN) {
        auto& e   = entries[head];
        snprintf(e.text, sizeof(e.text), "%s", msg);
        e.type     = type;
        e.lifespan = life;
        e.maxLife  = life;
        e.age      = 0.0f;
        e.active   = true;
        head = (head + 1) % MAX_TOASTS;
        ++count;
    }

    /// 每帧 tick：递减寿命，递增年龄，归零则停用
    void Update(float dt) {
        for (auto& e : entries) {
            if (!e.active) continue;
            e.lifespan -= dt;
            e.age      += dt;
            if (e.lifespan <= 0.0f) {
                e.active = false;
            }
        }
    }

    /// 获取第 i 个活跃 toast（0=最旧，返回 nullptr 表示越界）
    /// 用于渲染排序：从旧到新依次绘制
    const ToastEntry* GetActive(int i) const {
        int found = 0;
        // 从 head 位置向后遍历（最旧的在 head 之后）
        for (int k = 0; k < MAX_TOASTS; ++k) {
            int idx = (head + k) % MAX_TOASTS;
            if (entries[idx].active) {
                if (found == i) return &entries[idx];
                ++found;
            }
        }
        return nullptr;
    }

    /// 获取当前活跃 toast 数量
    int ActiveCount() const {
        int n = 0;
        for (const auto& e : entries) {
            if (e.active) ++n;
        }
        return n;
    }
};

} // namespace ECS
