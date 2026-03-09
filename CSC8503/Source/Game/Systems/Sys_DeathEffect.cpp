/**
 * @file Sys_DeathEffect.cpp
 * @brief 死亡视觉特效系统实现。
 *
 * 动画流程（总时长 ~0.8s）：
 *   1. 首帧初始化：记录 originalScale，移除 AI/物理组件，标记透明材质
 *   2. t < 0.15：闪白（colour = 白色高亮，emissive 全开）
 *   3. t >= 0.15：发光淡出（emissive 递减，alpha 1→0）+ 缩放 100%→30%
 *   4. t >= 1.0：收集实体 ID，循环外调用 registry.Destroy()
 */
#include "Sys_DeathEffect.h"

#include <algorithm>
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

void Sys_DeathEffect::OnUpdate(Registry& registry, float dt) {
    std::vector<EntityID> toDestroy;

    registry.view<C_D_Dying, C_D_Transform, C_D_DeathVisual>().each(
        [&](EntityID id, C_D_Dying& dying, C_D_Transform& tf, C_D_DeathVisual& dv) {

            // ── 首帧初始化 ──
            if (!dying.initialized) {
                dying.initialized = true;
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

                // 确保有 C_D_Material 用于 emissive 控制
                if (!registry.Has<C_D_Material>(id)) {
                    registry.Emplace<C_D_Material>(id);
                }

                LOG_INFO("[Sys_DeathEffect] Initialized death animation for entity " << (int)id);
            }

            // ── 推进计时器 ──
            dying.elapsed += dt;
            float t = std::min(dying.elapsed / dying.duration, 1.0f);

            // ── 动画逻辑 ──
            auto& mat = registry.Get<C_D_Material>(id);

            if (t < 0.15f) {
                // Phase 1: 闪白 — 强 emissive + 白色
                float flashIntensity = 1.0f;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, 1.0f};

                mat.emissiveColor    = {1.0f, 1.0f, 1.0f};
                mat.emissiveStrength = 3.0f * flashIntensity;
            } else {
                // Phase 2: 发光淡出 + alpha 淡出 + 缩放
                // 将 t 从 [0.15, 1.0] 映射到 [0, 1]
                float fadeT = (t - 0.15f) / 0.85f;

                // emissive 从强到弱
                float emissive = (1.0f - fadeT) * 2.0f;
                mat.emissiveColor    = {1.0f, 0.9f, 0.7f}; // 暖色发光
                mat.emissiveStrength = emissive;

                // alpha 从 1 → 0
                float alpha = 1.0f - fadeT;
                dv.colourOverride = {1.0f, 1.0f, 1.0f, alpha};

                // 缩放从 100% → 30%
                float scaleFactor = 1.0f - 0.7f * fadeT;
                tf.scale = dv.originalScale * scaleFactor;
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
