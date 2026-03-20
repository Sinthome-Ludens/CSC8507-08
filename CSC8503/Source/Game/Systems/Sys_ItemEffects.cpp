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
 *  - RoamAI：碰撞检测后 hp=0 + 直接挂 C_D_Dying/C_D_DeathVisual + 扣分（School A）
 *  - TargetStrike：有 C_D_Health 时 hp=0（委托 Sys_DeathJudgment），无 C_D_Health 时直接挂 C_D_Dying + 扣分
 */
#include "Sys_ItemEffects.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_HoloBaitState.h"
#include "Game/Components/C_D_DDoSFrozen.h"
#include "Game/Components/C_D_RoamAI.h"
#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/C_T_RoamAI.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_RadarState.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Events/Evt_Item_Use.h"
#include "Game/Events/Evt_Audio.h"
#include "Game/Events/Evt_Death.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Components/C_D_OrbitTriangle.h"
#include "Game/Components/C_D_TriangleProjectile.h"
#include "Game/Components/C_D_OrbitInventory.h"
#include "Game/Components/Res_OrbitConfig.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"
#include "Core/ECS/EntityID.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <cfloat>
#include <vector>

#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_MinimapState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ScoreConfig.h"
#ifdef USE_IMGUI
#include "Game/UI/UI_ActionNotify.h"
#endif

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// 内部辅助：简单伪随机数
// ============================================================
float Sys_ItemEffects::RandFloat() {
    m_RandSeed = m_RandSeed * 1664525u + 1013904223u;
    return static_cast<float>(m_RandSeed & 0xFFFFu) / 65536.0f;
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
                    // 统一道具使用扣分 -5
                    {
                        static const char* kItemNames[] = {
                            "HOLOBAIT", "RADAR", "DDOS", "ROAM AI", "TARGET STRIKE", "MAP"
                        };
                        const char* usedName = (static_cast<uint8_t>(evt.itemId) < static_cast<uint8_t>(ItemID::Count))
                            ? kItemNames[static_cast<uint8_t>(evt.itemId)] : "ITEM";
                        Res_ScoreConfig defaultScoreCfg;
                        const auto& sc = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg;
#ifdef USE_IMGUI
                        ECS::UI::PushActionNotify(registry, "USED", usedName,
                                                  -sc.penaltyItemUse, ActionNotifyType::Alert);
#endif
                        if (registry.has_ctx<Res_UIState>()
                            && registry.has_ctx<Res_GameState>()) {
                            auto& uiS = registry.ctx<Res_UIState>();
                            uiS.campaignScore = std::max(0, uiS.campaignScore - sc.penaltyItemUse);
                            uiS.scoreLost_items += sc.penaltyItemUse;
                            uiS.scoreItemUseCount++;
                        }
                    }
                    // SFX: 道具使用（通过 ItemType 判断，不硬编码 ItemID）
                    if (registry.has_ctx<EventBus*>() && registry.has_ctx<Res_ItemInventory2>()) {
                        auto* audioBus = registry.ctx<EventBus*>();
                        if (audioBus) {
                            bool isWeapon = registry.ctx<Res_ItemInventory2>().Get(evt.itemId).itemType == ItemType::Weapon;
                            SfxId sfx = isWeapon ? SfxId::WeaponUse : SfxId::ItemUse;
                            audioBus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{sfx});
                        }
                    }
                    switch (evt.itemId) {
                        case ItemID::HoloBait:     EffectHoloBait    (registry, evt); break;
                        case ItemID::PhotonRadar:  EffectPhotonRadar (registry, evt); break;
                        case ItemID::DDoS:         EffectDDoS        (registry, evt); break;
                        case ItemID::RoamAI:       EffectRoamAI      (registry, evt); break;
                        case ItemID::TargetStrike: EffectTargetStrike(registry, evt); break;
                        case ItemID::GlobalMap:    EffectGlobalMap   (registry, evt); break;
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
    PAUSE_GUARD(registry);
    UpdateHoloBait  (registry, dt);
    UpdateDDoSFrozen(registry, dt);
    UpdateRoamAI    (registry, dt);
    UpdateRadar     (registry, dt);
    UpdateMinimap   (registry, dt);
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
            if (registry.Has<C_D_Dying>(eid)) return; // 已死亡，跳过
            float dx = tf.position.x - origin.x;
            float dz = tf.position.z - origin.z;
            float d2 = dx*dx + dz*dz; // XZ 距离（2.5D 忽略 Y）
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
    EntityID baitEntity = PrefabFactory::CreateHoloBait(registry, evt.targetPos);

    auto& bait = registry.Get<C_D_HoloBaitState>(baitEntity);

    EntityID attracted = Entity::NULL_ENTITY;
    float    minD2     = FLT_MAX;
    registry.view<C_T_Enemy, C_D_Transform, C_D_AIState>().each(
        [&](EntityID eid, C_T_Enemy&, C_D_Transform& etf, C_D_AIState& ai) {
            if (ai.current_state != EnemyState::Safe) {
                return;
            }
            float dx = etf.position.x - bait.worldPos.x;
            float dz = etf.position.z - bait.worldPos.z;
            float d2 = dx*dx + dz*dz;
            if (d2 < minD2) {
                minD2    = d2;
                attracted = eid;
            }
        }
    );
    bait.attractedEnemy = attracted;

    LOG_INFO("[Sys_ItemEffects] HoloBait deployed at ("
             << bait.worldPos.x << "," << bait.worldPos.z
             << ") attracted=" << attracted);

#ifdef USE_IMGUI
    ECS::UI::PushActionNotify(registry, "HOLOBAIT", "DEPLOYED",
                              0, ActionNotifyType::Bonus);
#endif
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

#ifdef USE_IMGUI
    ECS::UI::PushActionNotify(registry, "RADAR", "ACTIVATED",
                              0, ActionNotifyType::Bonus);
#endif
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

#ifdef USE_IMGUI
    ECS::UI::PushActionNotify(registry, "DDOS", "GUARD",
                              0, ActionNotifyType::Weapon);
#endif
}

// ============================================================
// RoamAI prefab helper
// ============================================================
// ============================================================
// EffectRoamAI — 在玩家位置创建流窜 AI 实体
// ============================================================
void Sys_ItemEffects::EffectRoamAI(Registry& registry, const Evt_Item_Use& evt) {
    EntityID roamId = PrefabFactory::CreateRoamAI(registry, evt.targetPos);

    LOG_INFO("[Sys_ItemEffects] RoamAI spawned at ("
             << evt.targetPos.x << "," << evt.targetPos.z << ").");
}

// ============================================================
// EffectTargetStrike — 取环绕三角形发射弹射体；无三角形时 fallback 瞬杀
// ============================================================
void Sys_ItemEffects::EffectTargetStrike(Registry& registry, const Evt_Item_Use& evt) {
    EntityID playerId = evt.userEntity;

    // ── 尝试取环绕三角形发射 ──
    if (registry.Has<C_D_OrbitInventory>(playerId)) {
        auto& orbit = registry.Get<C_D_OrbitInventory>(playerId);
        if (orbit.count > 0) {
            // 取最后一个三角形
            EntityID triEntity = orbit.triangles[orbit.count - 1];
            orbit.triangles[orbit.count - 1] = Entity::NULL_ENTITY;
            orbit.count--;

            if (registry.Valid(triEntity) && registry.Has<C_D_OrbitTriangle>(triEntity)) {
                // 移除 orbit 组件
                registry.Remove<C_D_OrbitTriangle>(triEntity);

                // 从三角形当前位置找最近敌人（不是玩家位置）
                NCL::Maths::Vector3 triPos = registry.Get<C_D_Transform>(triEntity).position;
                EntityID target = FindNearestEnemy(registry, triPos);

                // 计算初始飞行方向
                NCL::Maths::Vector3 launchDir(0, 0, 1);
                if (Entity::IsValid(target) && registry.Valid(target) &&
                    registry.Has<C_D_Transform>(target)) {
                    auto& tgtTf = registry.Get<C_D_Transform>(target);
                    launchDir = tgtTf.position - triPos;
                    launchDir.y = 0.0f;
                    float len = std::sqrt(launchDir.x*launchDir.x + launchDir.z*launchDir.z);
                    if (len > 0.001f) {
                        launchDir = launchDir * (1.0f / len);
                    }
                }

                // 添加 projectile 组件（保持 kinematic，纯代码驱动）
                auto& proj = registry.Emplace<C_D_TriangleProjectile>(triEntity);
                proj.ownerPlayer  = playerId;
                proj.targetEnemy  = target;
                proj.launchDir    = launchDir;

                if (registry.has_ctx<Res_OrbitConfig>()) {
                    auto& cfg = registry.ctx<Res_OrbitConfig>();
                    proj.speed         = cfg.projectileSpeed;
                    proj.turnRate      = cfg.projectileTurnRate;
                    proj.remainingLife = cfg.projectileLife;
                }

                LOG_INFO("[Sys_ItemEffects] TargetStrike launched triangle " << triEntity
                         << " toward enemy " << target);

#ifdef USE_IMGUI
                ECS::UI::PushActionNotify(registry, "TARGET STRIKE", "LAUNCHED",
                                          0, ActionNotifyType::Weapon);
#endif
                return;
            }
        }
    }

    // ── Fallback：无环绕三角形，瞬杀最近敌人 ──
    EntityID target = evt.targetEntity;
    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        target = FindNearestEnemy(registry, evt.targetPos);
    }

    if (!Entity::IsValid(target) || !registry.Valid(target)) {
        LOG_INFO("[Sys_ItemEffects] TargetStrike: no valid target found.");
        return;
    }

    if (registry.Has<C_D_Dying>(target)) {
        LOG_INFO("[Sys_ItemEffects] TargetStrike: target " << target << " already dying, skipped.");
        return;
    }

    if (registry.Has<C_D_Health>(target)) {
        auto& hp = registry.Get<C_D_Health>(target);
        hp.hp        = 0.0f;
        hp.deathCause = DeathType::EnemyHpZero;
        LOG_INFO("[Sys_ItemEffects] TargetStrike killed entity " << target << " via health (fallback).");
#ifdef USE_IMGUI
        ECS::UI::PushActionNotify(registry, "TARGET STRIKE", "GUARD",
                                  0, ActionNotifyType::Weapon);
#endif
    } else if (registry.Has<C_T_Enemy>(target)) {
        if (!registry.Has<C_D_Dying>(target)) {
            registry.Emplace<C_D_Dying>(target);
            registry.Emplace<C_D_DeathVisual>(target);
            if (registry.has_ctx<EventBus*>()) {
                auto* deathBus = registry.ctx<EventBus*>();
                if (deathBus) { Evt_Death de{}; de.entity = target; de.deathType = DeathType::EnemyHpZero; deathBus->publish_deferred(de); }
            }
            if (registry.has_ctx<Res_UIState>()) {
                Res_ScoreConfig defaultScoreCfg2;
                const auto& sc2 = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg2;
                auto& uiS = registry.ctx<Res_UIState>();
                uiS.campaignScore = std::max(0, uiS.campaignScore - sc2.penaltyKill);
                uiS.scoreLost_kills += sc2.penaltyKill;
                uiS.scoreKillCount++;
            }
        }
        LOG_INFO("[Sys_ItemEffects] TargetStrike killed enemy entity "
                 << target << " (no health, fallback direct death).");
#ifdef USE_IMGUI
        ECS::UI::PushActionNotify(registry, "TARGET STRIKE", "GUARD",
                                  0, ActionNotifyType::Weapon);
#endif
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

            bait.remainingTime -= dt;
            if (bait.remainingTime <= 0.0f) {
                bait.active = false;
                toDestroy.push_back(eid);
                LOG_INFO("[Sys_ItemEffects] HoloBait expired, entity " << eid << " destroyed.");
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

            // 检测与敌人的碰撞（1:1 语义：一个 RoamAI 最多击杀一个敌人）
            float r2 = roam.detectRadius * roam.detectRadius;
            bool killed = false;
            registry.view<C_T_Enemy, C_D_Transform>().each(
                [&](EntityID eid, C_T_Enemy&, C_D_Transform& etf) {
                    if (killed) return;
                    if (registry.Has<C_D_Dying>(eid)) return; // 已死亡，跳过
                    float dx = etf.position.x - tf.position.x;
                    float dz = etf.position.z - tf.position.z;
                    if ((dx*dx + dz*dz) <= r2) {
                        killed = true;
                        roam.active = false;
                        toDestroy.push_back(roamId);  // 仅销毁 RoamAI 自身

                        // 标准 ECS 死亡流程（C_D_Dying 守卫防重复）
                        if (!registry.Has<C_D_Dying>(eid)) {
                            if (registry.Has<C_D_Health>(eid)) {
                                auto& hp = registry.Get<C_D_Health>(eid);
                                hp.hp = 0.0f;
                                hp.deathCause = DeathType::EnemyHpZero;
                            }
                            registry.Emplace<C_D_Dying>(eid);
                            registry.Emplace<C_D_DeathVisual>(eid);
                            if (registry.has_ctx<EventBus*>()) {
                                auto* deathBus = registry.ctx<EventBus*>();
                                if (deathBus) { Evt_Death de{}; de.entity = eid; de.deathType = DeathType::EnemyHpZero; deathBus->publish_deferred(de); }
                            }

                            if (registry.has_ctx<Res_UIState>()) {
                                Res_ScoreConfig defaultScoreCfg3;
                                const auto& sc3 = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg3;
                                auto& uiS = registry.ctx<Res_UIState>();
                                uiS.campaignScore = std::max(0, uiS.campaignScore - sc3.penaltyKill);
                                uiS.scoreLost_kills += sc3.penaltyKill;
                                uiS.scoreKillCount++;
#ifdef USE_IMGUI
                                ECS::UI::PushActionNotify(registry, "KILL PENALTY", "ROAM AI",
                                                          -sc3.penaltyKill, ActionNotifyType::Kill);
#endif
                            }
                        }
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

// ============================================================
// EffectGlobalMap — 激活小地图 + 联动雷达
// ============================================================
/// @brief 激活小地图（限时 kActiveDuration 秒）并联动开启雷达扫描。
void Sys_ItemEffects::EffectGlobalMap(Registry& registry, const Evt_Item_Use& /*evt*/) {
    if (!registry.has_ctx<Res_MinimapState>()) return;
    auto& minimap = registry.ctx<Res_MinimapState>();
    minimap.isActive    = true;
    minimap.activeTimer = Res_MinimapState::kActiveDuration;

    // 同时激活雷达（复用敌人位置扫描）
    if (registry.has_ctx<Res_RadarState>()) {
        auto& radar = registry.ctx<Res_RadarState>();
        if (!radar.isActive) {
            radar.isActive = true;
            radar.refreshTimer = 0.0f;  // 立即刷新
        }
    }

    LOG_INFO("[Sys_ItemEffects] GlobalMap activated.");

#ifdef USE_IMGUI
    ECS::UI::PushActionNotify(registry, "MAP", "ACTIVATED",
                              0, ActionNotifyType::Bonus);
#endif
}

// ============================================================
// UpdateMinimap — 每帧递减小地图激活计时器，到期自动关闭
// ============================================================
/// @brief 每帧递减小地图计时器，到期自动关闭 minimap.isActive。
void Sys_ItemEffects::UpdateMinimap(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_MinimapState>()) return;
    auto& minimap = registry.ctx<Res_MinimapState>();
    if (!minimap.isActive) return;

    minimap.activeTimer -= dt;
    if (minimap.activeTimer <= 0.0f) {
        minimap.isActive    = false;
        minimap.activeTimer = 0.0f;
    }
}

} // namespace ECS
