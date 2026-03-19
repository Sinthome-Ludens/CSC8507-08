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
#include "Game/Utils/PauseGuard.h"

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
#include "Game/Components/Res_DeathEffectConfig.h"
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
    PAUSE_GUARD(registry);
    Res_DeathEffectConfig defaultFxCfg;
    const auto& fx = registry.has_ctx<Res_DeathEffectConfig>() ? registry.ctx<Res_DeathEffectConfig>() : defaultFxCfg;
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
            // Phase 1: 数字冲击
            // ════════════════════════════════════════════════════
            if (t < fx.phase1End) {
                dv.colourOverride    = {1.0f, 1.0f, 1.0f, 1.0f};
                mat.emissiveColor    = {0.0f, 1.0f, 1.0f};
                mat.emissiveStrength = fx.p1_emissiveStrength;
                mat.rimPower         = fx.p1_rimPower;
                mat.rimStrength      = fx.p1_rimStrength;
            }
            // ════════════════════════════════════════════════════
            // Phase 2: 霓虹故障
            // ════════════════════════════════════════════════════
            else if (t < fx.phase2End) {
                float phase2T = t - fx.phase1End;

                int colorIdx = (int)(phase2T * fx.p2_colorCycleHz) % 3;
                switch (colorIdx) {
                    case 0: mat.emissiveColor = {0.0f, 1.0f, 1.0f}; break;
                    case 1: mat.emissiveColor = {1.0f, 0.0f, 0.8f}; break;
                    case 2: mat.emissiveColor = {1.0f, 0.8f, 0.0f}; break;
                }

                float sinPulse = sinf(phase2T * fx.p2_pulseHz * kPI * 2.0f);
                mat.emissiveStrength = fx.p2_pulseMid + fx.p2_pulseAmp * sinPulse;

                uint32_t flickerHash = QuickHash(dying.seed, (uint32_t)(t * fx.p2_flickerHz));
                float alpha = (flickerHash % 3 == 0) ? fx.p2_flickerOff : fx.p2_flickerOn;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, alpha};

                auto jitter = [&](uint32_t axis) -> float {
                    uint32_t h = QuickHash(dying.seed + axis, (uint32_t)(t * fx.p2_jitterHz));
                    return 1.0f + ((float)(h % 100) / 100.0f - 0.5f) * fx.p2_jitterAmp;
                };
                tf.scale.x = dv.originalScale.x * jitter(0);
                tf.scale.y = dv.originalScale.y * jitter(1);
                tf.scale.z = dv.originalScale.z * jitter(2);

                mat.rimPower   = fx.p2_rimPower;
                mat.rimStrength = fx.p2_rimStrength;
            }
            // ════════════════════════════════════════════════════
            // Phase 3: 数据溶解
            // ════════════════════════════════════════════════════
            else if (t < fx.phase3End) {
                float p3Dur = fx.phase3End - fx.phase2End;
                float phase3T = (p3Dur > 0.001f) ? (t - fx.phase2End) / p3Dur : 1.0f;

                mat.emissiveColor    = {0.0f, 1.0f, 1.0f};
                mat.emissiveStrength = fx.p3_emissiveStart - fx.p3_emissiveDecay * phase3T;

                float baseAlpha = fx.p3_alphaStart - fx.p3_alphaDecay * phase3T;
                uint32_t stutterHash = QuickHash(dying.seed, (uint32_t)(t * fx.p3_stutterHz));
                if (stutterHash % fx.p3_stutterModulo == 0) baseAlpha = fx.p3_stutterAlpha;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, baseAlpha};

                float yScale  = 1.0f + fx.p3_yStretch * phase3T;
                float xzScale = 1.0f - fx.p3_xzShrink * phase3T;
                tf.scale.x = dv.originalScale.x * xzScale;
                tf.scale.y = dv.originalScale.y * yScale;
                tf.scale.z = dv.originalScale.z * xzScale;

                mat.rimPower   = fx.p3_rimPowerStart + fx.p3_rimPowerGrow * phase3T;
                mat.rimStrength = fx.p3_rimStrengthStart - fx.p3_rimStrengthDecay * phase3T;
            }
            // ════════════════════════════════════════════════════
            // Phase 4: 最终崩塌
            // ════════════════════════════════════════════════════
            else {
                float p4Dur = fx.phase4End - fx.phase3End;
                float phase4T = (p4Dur > 0.001f) ? std::min((t - fx.phase3End) / p4Dur, 1.0f) : 1.0f;

                mat.emissiveColor    = {1.0f, 0.0f, 0.8f};
                mat.emissiveStrength = fx.p4_emissiveStart * (1.0f - phase4T);

                float alpha = fx.p4_alphaStart * (1.0f - phase4T);
                dv.colourOverride = {1.0f, 1.0f, 1.0f, alpha};

                float scaleFactor = 1.0f - fx.p4_scaleCollapse * phase4T;
                tf.scale.x = dv.originalScale.x * fx.p3EndXZScale * scaleFactor;
                tf.scale.y = dv.originalScale.y * fx.p3EndYScale  * scaleFactor;
                tf.scale.z = dv.originalScale.z * fx.p3EndXZScale * scaleFactor;

                mat.rimPower   = fx.p4_rimPower;
                mat.rimStrength = fx.p4_rimStrength;
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
