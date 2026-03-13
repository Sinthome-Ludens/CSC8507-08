/**
 * @file Sys_DeathEffect.cpp
 * @brief 赛博朋克风格死亡视觉特效系统实现。
 *
 * 动画流程（总时长 1.2s）：
 *   1. 首帧初始化：记录 originalScale，移除 AI/物理组件，标记透明材质，生成 seed
 *   2. Phase 1 (0.00-0.15s)：数字冲击 — 青色闪光 + 白色覆盖 + 强 rim 光
 *   3. Phase 2 (0.15-0.55s)：霓虹故障 — 三色高频循环 + emissive 脉冲 + alpha 闪烁 + 轴抖动
 *   4. Phase 3 (0.55-1.00s)：数据溶解 — 青色渐弱 + alpha 淡出 + Y拉伸/XZ收缩
 *   5. Phase 4 (1.00-1.20s)：最终崩塌 — 品红闪光 + 全轴收缩至 10%
 *   6. 动画结束后收集实体 ID，循环外调用 registry.Destroy()
 */
#include "Sys_DeathEffect.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_D_NavAgent.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_T_Pathfinder.h"
#include "Game/Utils/Log.h"

namespace ECS {

static constexpr float kPI = 3.14159265f;

// 简易 hash 用于 glitch 差异化
static uint32_t QuickHash(uint32_t seed, uint32_t salt) {
    seed ^= salt;
    seed *= 2654435761u;
    seed ^= (seed >> 16);
    return seed;
}

/**
 * @brief 每帧推进所有具有 C_D_Dying 实体的死亡动画（四阶段），动画结束后销毁实体。
 * @param registry ECS 注册表
 * @param dt       帧时间（秒）
 */
void Sys_DeathEffect::OnUpdate(Registry& registry, float dt) {
    std::vector<EntityID> toDestroy;

    registry.view<C_D_Dying, C_D_Transform, C_D_DeathVisual>().each(
        [&](EntityID id, C_D_Dying& dying, C_D_Transform& tf, C_D_DeathVisual& dv) {

            // ── 首帧初始化 ──
            if (!dying.initialized) {
                dying.initialized = true;
                dying.seed        = static_cast<uint32_t>(id) * 2654435761u;
                dv.originalScale  = tf.scale;
                dv.useTransparent = true;

                // 移除 AI / 物理相关组件，使死亡实体不再参与游戏逻辑
                if (registry.Has<C_T_Enemy>(id))        registry.Remove<C_T_Enemy>(id);
                if (registry.Has<C_D_AIState>(id))       registry.Remove<C_D_AIState>(id);
                if (registry.Has<C_D_AIPerception>(id))  registry.Remove<C_D_AIPerception>(id);
                if (registry.Has<C_D_EnemyDormant>(id))  registry.Remove<C_D_EnemyDormant>(id);
                if (registry.Has<C_D_Health>(id))        registry.Remove<C_D_Health>(id);
                if (registry.Has<C_D_RigidBody>(id))     registry.Remove<C_D_RigidBody>(id);
                if (registry.Has<C_D_Collider>(id))      registry.Remove<C_D_Collider>(id);
                if (registry.Has<C_T_Pathfinder>(id))    registry.Remove<C_T_Pathfinder>(id);
                if (registry.Has<C_D_NavAgent>(id))      registry.Remove<C_D_NavAgent>(id);

                // 确保有 C_D_Material 用于 emissive / rim 控制
                if (!registry.Has<C_D_Material>(id)) {
                    registry.Emplace<C_D_Material>(id);
                }

                LOG_INFO("[Sys_DeathEffect] Initialized death animation for entity " << (int)id);
            }

            // ── 推进计时器 ──
            dying.elapsed += dt;
            float t = dying.elapsed;
            auto& mat = registry.Get<C_D_Material>(id);

            // ════════════════════════════════════════════════════
            // Phase 1: 数字冲击 (0.00 - 0.15s)
            // ════════════════════════════════════════════════════
            if (t < 0.15f) {
                dv.colourOverride    = {1.0f, 1.0f, 1.0f, 1.0f};
                mat.emissiveColor    = {0.0f, 1.0f, 1.0f};   // 青色
                mat.emissiveStrength = 4.0f;
                mat.rimPower         = 2.0f;
                mat.rimStrength      = 1.5f;
            }
            // ════════════════════════════════════════════════════
            // Phase 2: 霓虹故障 (0.15 - 0.55s)
            // ════════════════════════════════════════════════════
            else if (t < 0.55f) {
                float phase2T = t - 0.15f;   // 0.0 ~ 0.4

                // 三色高频循环 (12.5Hz → 周期 0.08s)
                int colorIdx = (int)(phase2T * 12.5f) % 3;
                switch (colorIdx) {
                    case 0: mat.emissiveColor = {0.0f, 1.0f, 1.0f}; break; // 青色
                    case 1: mat.emissiveColor = {1.0f, 0.0f, 0.8f}; break; // 品红
                    case 2: mat.emissiveColor = {1.0f, 0.8f, 0.0f}; break; // 黄色
                }

                // emissive 脉冲 1.5~3.0 (25Hz sin 波)
                float sinPulse = sinf(phase2T * 25.0f * kPI * 2.0f);
                mat.emissiveStrength = 2.25f + 0.75f * sinPulse;

                // alpha 闪烁：hash 驱动 1.0 / 0.3
                uint32_t flickerHash = QuickHash(dying.seed, (uint32_t)(t * 80.0f));
                float alpha = (flickerHash % 3 == 0) ? 0.3f : 1.0f;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, alpha};

                // 轴抖动：各轴 +-5% 随机缩放
                auto jitter = [&](uint32_t axis) -> float {
                    uint32_t h = QuickHash(dying.seed + axis, (uint32_t)(t * 60.0f));
                    return 1.0f + ((float)(h % 100) / 100.0f - 0.5f) * 0.10f;
                };
                tf.scale.x = dv.originalScale.x * jitter(0);
                tf.scale.y = dv.originalScale.y * jitter(1);
                tf.scale.z = dv.originalScale.z * jitter(2);

                mat.rimPower   = 3.0f;
                mat.rimStrength = 0.8f;
            }
            // ════════════════════════════════════════════════════
            // Phase 3: 数据溶解 (0.55 - 1.00s)
            // ════════════════════════════════════════════════════
            else if (t < 1.00f) {
                float phase3T = (t - 0.55f) / 0.45f;  // 0.0 ~ 1.0

                // 青色 emissive 渐弱 2.0 → 0.3
                mat.emissiveColor    = {0.0f, 1.0f, 1.0f};
                mat.emissiveStrength = 2.0f - 1.7f * phase3T;

                // alpha 平滑淡出 1.0 → 0.15，偶发 stutter 跳回 0.6
                float baseAlpha = 1.0f - 0.85f * phase3T;
                uint32_t stutterHash = QuickHash(dying.seed, (uint32_t)(t * 20.0f));
                if (stutterHash % 8 == 0) baseAlpha = 0.6f;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, baseAlpha};

                // Y 轴拉伸 100%→120%，XZ 收缩 100%→60%
                float yScale  = 1.0f + 0.2f * phase3T;
                float xzScale = 1.0f - 0.4f * phase3T;
                tf.scale.x = dv.originalScale.x * xzScale;
                tf.scale.y = dv.originalScale.y * yScale;
                tf.scale.z = dv.originalScale.z * xzScale;

                mat.rimPower   = 3.0f + 2.0f * phase3T;
                mat.rimStrength = 0.8f - 0.6f * phase3T;
            }
            // ════════════════════════════════════════════════════
            // Phase 4: 最终崩塌 (1.00 - 1.20s)
            // ════════════════════════════════════════════════════
            else {
                float phase4T = std::min((t - 1.00f) / 0.20f, 1.0f);  // 0.0 ~ 1.0

                // 品红色闪光 emissive 1.0 → 0
                mat.emissiveColor    = {1.0f, 0.0f, 0.8f};
                mat.emissiveStrength = 1.0f - phase4T;

                // alpha 0.15 → 0
                float alpha = 0.15f * (1.0f - phase4T);
                dv.colourOverride = {1.0f, 1.0f, 1.0f, alpha};

                // 全轴收缩至 10%
                float scaleFactor = 1.0f - 0.9f * phase4T;
                // 从 Phase 3 终态出发：Y=120%, XZ=60%
                tf.scale.x = dv.originalScale.x * 0.6f * scaleFactor;
                tf.scale.y = dv.originalScale.y * 1.2f * scaleFactor;
                tf.scale.z = dv.originalScale.z * 0.6f * scaleFactor;

                mat.rimPower   = 5.0f;
                mat.rimStrength = 0.0f;
            }

            // ── 动画结束 ──
            if (dying.elapsed >= dying.duration) {
                toDestroy.push_back(id);
            }
        }
    );

    // 循环外销毁，避免迭代器失效
    for (EntityID id : toDestroy) {
        LOG_INFO("[Sys_DeathEffect] Death animation complete, destroying entity " << (int)id);
        registry.Destroy(id);
    }
}

} // namespace ECS
