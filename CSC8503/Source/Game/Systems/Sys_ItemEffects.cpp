/**
 * @file Sys_ItemEffects.cpp
 * @brief 道具效果系统实现：订阅 Evt_Item_Use，执行并每帧维护五种道具的效果状态。
 *
 * @details
 * 算法概要：
 *  - OnAwake：订阅 Evt_Item_Use，初始化 Res_RadarState
 *  - OnUpdate：依次调用 UpdateHoloBait / UpdateDDoSFrozen / UpdateRoamAI / UpdateRadar
 *  - 各 Effect* 方法：即时执行使用效果（创建实体、挂载组件、修改 hp）
 *
 * ## 敌人不弄（设计约束）
 * 需求文档注明"敌人不弄"，即敌人 AI 行为不在本系统实现。
 * 本系统仅：
 *  - HoloBait：记录吸引目标到 C_D_HoloBaitState，由敌人 AI 读取（存根）
 *  - DDoS：挂载 C_D_DDoSFrozen，Sys_EnemyAI 读取后跳过移动
 *  - RoamAI：碰撞检测后对敌人 hp 归零（触发 Sys_DeathJudgment）
 *  - TargetStrike：对最近敌人 hp 归零（触发 Sys_DeathJudgment）
 */
#include "Sys_ItemEffects.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_HoloBaitState.h"
#include "Game/Components/C_D_DDoSFrozen.h"
#include "Game/Components/C_D_RoamAI.h"
#include "Game/Components/C_T_RoamAI.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_RadarState.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Events/Evt_Item_Use.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"
#include "Core/ECS/EntityID.h"

#include <cmath>
#include <cstring>
#include <cfloat>
#include <vector>

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// 内部辅助：简单伪随机数
// ============================================================
float Sys_ItemEffects::RandFloat() {
    m_RandSeed = m_RandSeed * 1664525u + 1013904223u;
    return static_cast<float>(m_RandSeed & 0xFFFFu) / 65535.0f;
}

// ============================================================
// OnAwake
// ============================================================
void Sys_ItemEffects::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_RadarState>()) {
        registry.ctx_emplace<Res_RadarState>();
    }

    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            m_UseSubId = bus->subscribe<Evt_Item_Use>(
                [this, &registry](const Evt_Item_Use& evt) {
                    switch (evt.itemId) {
                        case ItemID::HoloBait:     EffectHoloBait    (registry, evt); break;
                        case ItemID::PhotonRadar:  EffectPhotonRadar (registry, evt); break;
                        case ItemID::DDoS:         EffectDDoS        (registry, evt); break;
                        case ItemID::RoamAI:       EffectRoamAI      (registry, evt); break;
                        case ItemID::TargetStrike: EffectTargetStrike(registry, evt); break;
                        default: break;
                    }
                }
            );
        }
    }

    LOG_INFO("[Sys_ItemEffects] OnAwake complete.");
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_ItemEffects::OnUpdate(Registry& registry, float dt) {
    UpdateHoloBait  (registry, dt);
    UpdateDDoSFrozen(registry, dt);
    UpdateRoamAI    (registry, dt);
    UpdateRadar     (registry, dt);
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_ItemEffects::OnDestroy(Registry& registry) {
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus && m_UseSubId != 0) {
            bus->unsubscribe<Evt_Item_Use>(m_UseSubId);
            m_UseSubId = 0;
        }
    }
}

// ============================================================
// FindNearestEnemy — 找出距 origin 最近的敌人
// ============================================================
EntityID Sys_ItemEffects::FindNearestEnemy(Registry& registry, const Vector3& origin) {
    EntityID nearest = Entity::NULL_ENTITY;
    float    minDist2 = FLT_MAX;

    registry.view<C_T_Enemy, C_D_Transform>().each(
        [&](EntityID eid, C_T_Enemy&, C_D_Transform& tf) {
            float dx = tf.position.x - origin.x;
            float dy = tf.position.y - origin.y;
            float dz = tf.position.z - origin.z;
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 < minDist2) {
                minDist2 = d2;
                nearest  = eid;
            }
        }
    );

    return nearest;
}

// ============================================================
// EffectHoloBait — 在目标位置创建诱饵实体并吸引附近安全状态敌人
// ============================================================
void Sys_ItemEffects::EffectHoloBait(Registry& registry, const Evt_Item_Use& evt) {
    // 创建诱饵实体（占位：无渲染，仅数据）
    EntityID baitEntity = registry.Create();

    auto& tf = registry.Emplace<C_D_Transform>(baitEntity);
    tf.position = evt.targetPos;
    tf.scale    = Vector3(0.5f, 0.5f, 0.5f);

    auto& bait = registry.Emplace<C_D_HoloBaitState>(baitEntity);
    bait.worldPos      = evt.targetPos;
    bait.remainingTime = 3.0f;
    bait.active        = true;

    // 挂载 DebugName
    auto& dn = registry.Emplace<C_D_DebugName>(baitEntity);
    std::strncpy(dn.name, "ENTITY_HoloBait", sizeof(C_D_DebugName::name) - 1);

    // 寻找最近的安全状态敌人并记录为吸引目标（敌人 AI 读取此字段以移动）
    EntityID attracted = Entity::NULL_ENTITY;
    float    minD2     = FLT_MAX;
    registry.view<C_T_Enemy, C_D_Transform, C_D_AIState>().each(
        [&](EntityID eid, C_T_Enemy&, C_D_Transform& etf, C_D_AIState& ai) {
            if (ai.current_state != EnemyState::Safe) return;
            float dx = etf.position.x - evt.targetPos.x;
            float dz = etf.position.z - evt.targetPos.z;
            float d2 = dx*dx + dz*dz;
            if (d2 < minD2) { minD2 = d2; attracted = eid; }
        }
    );
    bait.attractedEnemy = attracted;

    LOG_INFO("[Sys_ItemEffects] HoloBait deployed at ("
             << evt.targetPos.x << "," << evt.targetPos.z
             << ") attracted=" << attracted);
}

// ============================================================
// EffectPhotonRadar — 激活雷达并立即执行一次敌人位置扫描
// ============================================================
void Sys_ItemEffects::EffectPhotonRadar(Registry& registry, const Evt_Item_Use& /*evt*/) {
    if (!registry.has_ctx<Res_RadarState>()) {
        registry.ctx_emplace<Res_RadarState>();
    }
    auto& radar = registry.ctx<Res_RadarState>();
    radar.isActive    = true;
    radar.refreshTimer = 0.0f; // 立即触发第一次刷新

    LOG_INFO("[Sys_ItemEffects] PhotonRadar activated.");
}

// ============================================================
// EffectDDoS — 对最近敌人挂载冻结组件（5 秒）
// ============================================================
void Sys_ItemEffects::EffectDDoS(Registry& registry, const Evt_Item_Use& evt) {
    // 确定目标：若 evt.targetEntity 有效则优先使用，否则找最近敌人
    EntityID target = evt.targetEntity;
    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        target = FindNearestEnemy(registry, evt.targetPos);
    }

    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        LOG_INFO("[Sys_ItemEffects] DDoS: no valid target found.");
        return;
    }

    // 已被冻结则刷新计时器，否则挂载组件
    if (registry.Has<C_D_DDoSFrozen>(target)) {
        registry.Get<C_D_DDoSFrozen>(target).frozenTimer = C_D_DDoSFrozen::kFrozenDuration;
    } else {
        registry.Emplace<C_D_DDoSFrozen>(target);
    }

    if (registry.Has<C_D_RoamAI>(target)) {
        registry.Get<C_D_RoamAI>(target).active = false;
    }
    LOG_INFO("[Sys_ItemEffects] DDoS frozen entity " << target << " for 5s.");
}

// ============================================================
// EffectRoamAI — 在玩家位置创建流窜 AI 实体
// ============================================================
void Sys_ItemEffects::EffectRoamAI(Registry& registry, const Evt_Item_Use& evt) {
    EntityID roamId = registry.Create();

    auto& tf = registry.Emplace<C_D_Transform>(roamId);
    tf.position = evt.targetPos + Vector3(0.0f, 0.5f, 0.0f); // 略高于玩家
    tf.scale    = Vector3(0.4f, 0.4f, 0.4f);

    registry.Emplace<C_T_RoamAI>(roamId);

    auto& roam = registry.Emplace<C_D_RoamAI>(roamId);
    roam.targetPos        = evt.targetPos;
    roam.roamSpeed        = 6.0f;
    roam.waypointInterval = 2.0f;
    roam.detectRadius     = 1.5f;
    roam.active           = true;

    auto& dn = registry.Emplace<C_D_DebugName>(roamId);
    std::strncpy(dn.name, "ENTITY_RoamAI", sizeof(C_D_DebugName::name) - 1);

    LOG_INFO("[Sys_ItemEffects] RoamAI spawned at ("
             << evt.targetPos.x << "," << evt.targetPos.z << ").");
}

// ============================================================
// EffectTargetStrike — 将最近敌人 hp 归零（触发死亡判定）
// ============================================================
void Sys_ItemEffects::EffectTargetStrike(Registry& registry, const Evt_Item_Use& evt) {
    EntityID target = evt.targetEntity;
    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        target = FindNearestEnemy(registry, evt.targetPos);
    }

    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        LOG_INFO("[Sys_ItemEffects] TargetStrike: no valid target found.");
        return;
    }

    if (registry.Has<C_D_Health>(target)) {
        auto& hp = registry.Get<C_D_Health>(target);
        hp.hp        = 0.0f;
        hp.deathCause = DeathType::EnemyHpZero;
        LOG_INFO("[Sys_ItemEffects] TargetStrike killed entity " << target);
    }
}

// ============================================================
// UpdateHoloBait — 每帧倒计时，到期销毁诱饵实体
// ============================================================
void Sys_ItemEffects::UpdateHoloBait(Registry& registry, float dt) {
    std::vector<EntityID> toDestroy;

    registry.view<C_D_HoloBaitState>().each(
        [&](EntityID eid, C_D_HoloBaitState& bait) {
            if (!bait.active) { toDestroy.push_back(eid); return; }

            if (bait.enemyArrived) {
                bait.remainingTime -= dt;
                if (bait.remainingTime <= 0.0f) {
                    bait.active = false;
                    toDestroy.push_back(eid);
                    LOG_INFO("[Sys_ItemEffects] HoloBait expired, entity " << eid << " destroyed.");
                }
            }
        }
    );

    for (EntityID e : toDestroy) {
        if (registry.Valid(e)) registry.Destroy(e);
    }
}

// ============================================================
// UpdateDDoSFrozen — 每帧递减冻结计时器，到期移除组件
// ============================================================
void Sys_ItemEffects::UpdateDDoSFrozen(Registry& registry, float dt) {
    std::vector<EntityID> toUnfreeze;

    registry.view<C_D_DDoSFrozen>().each(
        [&](EntityID eid, C_D_DDoSFrozen& frozen) {
            frozen.frozenTimer -= dt;
            if (frozen.frozenTimer <= 0.0f) {
                toUnfreeze.push_back(eid);
                LOG_INFO("[Sys_ItemEffects] DDoS unfreeze entity " << eid);
            }
        }
    );

    for (EntityID e : toUnfreeze) {
        if (registry.Valid(e) && registry.Has<C_D_DDoSFrozen>(e)) {
            registry.Remove<C_D_DDoSFrozen>(e);
        }
    }
}

// ============================================================
// UpdateRoamAI — 每帧随机游走并检测与敌人的碰撞
// ============================================================
void Sys_ItemEffects::UpdateRoamAI(Registry& registry, float dt) {
    std::vector<EntityID> toDestroy;

    registry.view<C_T_RoamAI, C_D_RoamAI, C_D_Transform>().each(
        [&](EntityID roamId, C_T_RoamAI&, C_D_RoamAI& roam, C_D_Transform& tf) {
            if (!roam.active) { toDestroy.push_back(roamId); return; }

            // 移向当前目标
            Vector3 dir = roam.targetPos - tf.position;
            dir.y = 0.0f;
            float dist = std::sqrt(dir.x*dir.x + dir.z*dir.z);

            if (dist > 0.5f) {
                float inv = roam.roamSpeed * dt / dist;
                tf.position.x += dir.x * inv;
                tf.position.z += dir.z * inv;
            } else {
                // 到达目标，更换随机目标（在 [-20, 20] 范围内）
                roam.waypointTimer += dt;
                if (roam.waypointTimer >= roam.waypointInterval) {
                    roam.waypointTimer = 0.0f;
                    roam.targetPos = Vector3(
                        (RandFloat() - 0.5f) * 40.0f,
                        tf.position.y,
                        (RandFloat() - 0.5f) * 40.0f
                    );
                }
            }

            // 检测与敌人的碰撞
            float r2 = roam.detectRadius * roam.detectRadius;
            registry.view<C_T_Enemy, C_D_Transform, C_D_Health>().each(
                [&](EntityID eid, C_T_Enemy&, C_D_Transform& etf, C_D_Health& hp) {
                    float dx = etf.position.x - tf.position.x;
                    float dz = etf.position.z - tf.position.z;
                    if ((dx*dx + dz*dz) <= r2) {
                        hp.hp        = 0.0f;
                        hp.deathCause = DeathType::EnemyHpZero;
                        roam.active   = false;
                        toDestroy.push_back(roamId);
                        LOG_INFO("[Sys_ItemEffects] RoamAI " << roamId
                                 << " killed enemy " << eid);
                    }
                }
            );
        }
    );

    for (EntityID e : toDestroy) {
        if (registry.Valid(e)) registry.Destroy(e);
    }
}

// ============================================================
// UpdateRadar — 每 3 秒刷新一次敌人位置列表
// ============================================================
void Sys_ItemEffects::UpdateRadar(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_RadarState>()) return;
    auto& radar = registry.ctx<Res_RadarState>();
    if (!radar.isActive) return;

    radar.refreshTimer -= dt;
    if (radar.refreshTimer > 0.0f) return;

    // 重置计时器并重新扫描所有敌人位置
    radar.refreshTimer = Res_RadarState::kRefreshInterval;
    radar.contactCount = 0;

    registry.view<C_T_Enemy, C_D_Transform>().each(
        [&](EntityID /*eid*/, C_T_Enemy&, C_D_Transform& tf) {
            if (radar.contactCount >= Res_RadarState::kMaxContacts) return;
            auto& c   = radar.contacts[radar.contactCount++];
            c.worldPos = tf.position;
            c.valid    = true;
        }
    );

    // 清空未使用的槽位
    for (int i = radar.contactCount; i < Res_RadarState::kMaxContacts; ++i) {
        radar.contacts[i].valid = false;
    }

    LOG_INFO("[Sys_ItemEffects] Radar refreshed: " << radar.contactCount << " contacts.");
}

} // namespace ECS
