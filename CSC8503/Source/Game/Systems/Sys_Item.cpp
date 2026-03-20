/**
 * @file Sys_Item.cpp
 * @brief 道具系统实现：拾取检测、使用输入分发、携带数量管理与开局/结算逻辑。
 *
 * @details
 * 实现要点：
 *  - OnAwake 中初始化 Res_ItemInventory2 并调用 OnRoundStart()，
 *    订阅 Evt_Item_Pickup 以更新携带数量。
 *  - OnUpdate 分两步：DetectPickup（XZ 范围检测）和 ProcessItemUseInput（Q/E 装备槽）。
 *  - OnDestroy 取消订阅并执行 OnRoundEnd()（游戏结束结算）。
 *
 * ## 道具使用按键约定
 *  - Q 键 → 使用当前激活的道具槽（Gadget）
 *  - E 键 → 使用当前激活的武器槽（Weapon）
 *  - TAB 键 → 切换激活的道具/武器槽
 */
#include "Sys_Item.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_ItemPickup.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Events/Evt_Item_Pickup.h"
#include "Game/Events/Evt_Item_Use.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Network.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Core/ECS/EventBus.h"
#include "Core/ECS/EntityID.h"
#include "Game/Events/Evt_Audio.h"

#ifdef USE_IMGUI
#include "Game/UI/UI_ActionNotify.h"
#include "Game/Components/Res_ActionNotifyState.h"
#endif

#include <cmath>
#include <cstring>
#include <vector>

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_Item::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_ItemInventory2>()) {
        registry.ctx_emplace<Res_ItemInventory2>();
    }
    registry.ctx<Res_ItemInventory2>().OnRoundStart();

    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            m_PickupSubId = bus->subscribe<Evt_Item_Pickup>(
                [this, &registry](const Evt_Item_Pickup& evt) {
                    OnPickup(registry, evt);
                }
            );
        }
    }

    LOG_INFO("[Sys_Item] OnAwake: inventory initialized, round start settled.");
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_Item::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    if (!m_DidLogStartupState) {
        int playerCount = 0;
        int pickupCount = 0;
        registry.view<C_T_Player>().each([&](EntityID, C_T_Player&) { ++playerCount; });
        registry.view<C_T_ItemPickup>().each([&](EntityID, C_T_ItemPickup&) { ++pickupCount; });

        int matchPhase = -1;
        bool isMultiplayer = false;
        int carry0 = -1;
        int carry1 = -1;
        int carry2 = -1;
        int carry3 = -1;
        int carry4 = -1;
        if (registry.has_ctx<Res_GameState>()) {
            const auto& gs = registry.ctx<Res_GameState>();
            matchPhase = static_cast<int>(gs.matchPhase);
            isMultiplayer = gs.isMultiplayer;
        }
        if (registry.has_ctx<Res_ItemInventory2>()) {
            const auto& inv = registry.ctx<Res_ItemInventory2>();
            carry0 = inv.slots[0].carriedCount;
            carry1 = inv.slots[1].carriedCount;
            carry2 = inv.slots[2].carriedCount;
            carry3 = inv.slots[3].carriedCount;
            carry4 = inv.slots[4].carriedCount;
        }

        LOG_MPDBG("[Sys_Item] Startup snapshot: players=" << playerCount
                  << " pickups=" << pickupCount
                  << " isMultiplayer=" << isMultiplayer
                  << " matchPhase=" << matchPhase
                  << " carried=[" << carry0 << "," << carry1 << "," << carry2 << "," << carry3 << "," << carry4 << "]");
        m_DidLogStartupState = true;
    }

    DetectPickup(registry);
    ProcessItemUseInput(registry);
    SyncDisplaySlots(registry, dt);
    SyncInventoryState(registry);
    AnimatePickups(registry, dt);
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_Item::OnDestroy(Registry& registry) {
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus && m_PickupSubId != 0) {
            bus->unsubscribe<Evt_Item_Pickup>(m_PickupSubId);
            m_PickupSubId = 0;
        }
    }

    if (registry.has_ctx<Res_ItemInventory2>()) {
        bool isVictory = false;
        if (registry.has_ctx<Res_GameState>()) {
            isVictory = (registry.ctx<Res_GameState>().gameOverReason == GameOverReason::Success);
        }
        registry.ctx<Res_ItemInventory2>().OnRoundEnd(isVictory);
        LOG_INFO("[Sys_Item] OnDestroy: round end (victory=" << isVictory << ").");
    }
}

// ============================================================
// DetectPickup — 每帧扫描地图道具实体，检测玩家是否进入拾取范围
// ============================================================
void Sys_Item::DetectPickup(Registry& registry) {
    if (!registry.has_ctx<EventBus*>()) return;
    auto* bus = registry.ctx<EventBus*>();
    if (!bus) return;

    if (!registry.has_ctx<Res_ItemInventory2>()) return;
    auto& inv = registry.ctx<Res_ItemInventory2>();

    struct PlayerInfo { EntityID id; Vector3 pos; };
    std::vector<PlayerInfo> players;
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID pid, C_T_Player&, C_D_Transform& tf) {
            players.push_back({ pid, tf.position });
        }
    );
    if (players.empty()) return;

    registry.view<C_T_ItemPickup, C_D_Transform>().each(
        [&](EntityID pickupId, C_T_ItemPickup& pickup, C_D_Transform& pickupTf) {
            for (auto& p : players) {
                float dx = p.pos.x - pickupTf.position.x;
                float dz = p.pos.z - pickupTf.position.z;
                if ((dx * dx + dz * dz) > kPickupRadius * kPickupRadius) continue;

                if (!inv.Get(pickup.itemId).CanPickup()) {
                    LOG_INFO("[Sys_Item] Player " << p.id << " inventory full for item "
                             << static_cast<int>(pickup.itemId));
                    continue;
                }

                bus->publish(Evt_Item_Pickup{ p.id, pickupId, pickup.itemId });
                break;
            }
        }
    );
}

// ============================================================
// OnPickup — 拾取事件回调：根据拾取点数量字段更新携带数量，并在数量耗尽后销毁道具实体
// ============================================================
void Sys_Item::OnPickup(Registry& registry, const Evt_Item_Pickup& evt) {
    if (!registry.has_ctx<Res_ItemInventory2>()) return;
    auto& inv = registry.ctx<Res_ItemInventory2>();
    auto& slot = inv.Get(evt.itemId);

    if (!registry.Valid(evt.pickupEntity) ||
        !registry.Has<C_T_ItemPickup>(evt.pickupEntity)) {
        if (slot.PickupOne()) {
            LOG_INFO("[Sys_Item] Player " << evt.pickerEntity
                     << " picked up item " << static_cast<int>(evt.itemId)
                     << " -> carried=" << (int)slot.carriedCount);
            if (registry.Valid(evt.pickupEntity)) {
                registry.Destroy(evt.pickupEntity);
            }
        }
        return;
    }

    auto& pickup = registry.Get<C_T_ItemPickup>(evt.pickupEntity);
    int remaining = pickup.quantity > 0 ? pickup.quantity : 1;
    int picked = 0;

    while (remaining > 0 && slot.CanPickup()) {
        if (!slot.PickupOne()) {
            break;
        }
        ++picked;
        --remaining;
    }

    pickup.quantity -= picked;

    if (picked > 0) {
        if (registry.has_ctx<EventBus*>()) {
            auto* bus = registry.ctx<EventBus*>();
            if (bus) bus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{SfxId::ItemPickup});
        }
        LOG_INFO("[Sys_Item] Player " << evt.pickerEntity
                 << " picked up " << picked << "x item " << static_cast<int>(evt.itemId)
                 << " -> carried=" << (int)slot.carriedCount);

        // First pickup of a weapon → permanently unlock it + full carry
        if (slot.itemType == ItemType::Weapon && !slot.unlocked) {
            slot.unlocked = true;
            slot.carriedCount = slot.maxCarry;
            LOG_INFO("[Sys_Item] WEAPON UNLOCKED: " << slot.name
                     << " carry=" << (int)slot.maxCarry);
#ifdef USE_IMGUI
            if (registry.has_ctx<Res_ActionNotifyState>()) {
                ECS::UI::PushActionNotify(registry, "UNLOCKED", slot.name,
                                          0, ActionNotifyType::Weapon, 3.0f);
            }
#endif
        }

#ifdef USE_IMGUI
        // Pickup notification: target field is color-highlighted in HUD
        if (registry.has_ctx<Res_ActionNotifyState>()) {
            char targetBuf[48];
            snprintf(targetBuf, sizeof(targetBuf), "%s x%d", slot.name, picked);
            ECS::UI::PushActionNotify(registry, "PICKED UP", targetBuf,
                                      0, ActionNotifyType::ItemPickup);
        }
#endif

        // Auto-fill empty HUD display slot with newly picked-up item
        if (registry.has_ctx<Res_GameState>()) {
            auto& gs = registry.ctx<Res_GameState>();
            bool isWeapon = (slot.itemType == ItemType::Weapon);
            SlotDisplay* displaySlots = isWeapon ? gs.weaponSlots : gs.itemSlots;

            bool alreadyDisplayed = false;
            for (int s = 0; s < 2; ++s) {
                if (displaySlots[s].itemId == static_cast<int>(evt.itemId)) {
                    alreadyDisplayed = true;
                    break;
                }
            }
            if (!alreadyDisplayed) {
                for (int s = 0; s < 2; ++s) {
                    if (displaySlots[s].name[0] == '\0') {
                        strncpy_s(displaySlots[s].name, sizeof(displaySlots[s].name),
                                  slot.name, sizeof(displaySlots[s].name) - 1);
                        displaySlots[s].itemId   = static_cast<uint8_t>(evt.itemId);
                        displaySlots[s].count    = slot.carriedCount;
                        displaySlots[s].cooldown = 0.0f;
                        break;
                    }
                }
            }
        }
    }

    if (pickup.quantity <= 0 && registry.Valid(evt.pickupEntity)) {
        registry.Destroy(evt.pickupEntity);
    }
}

// ============================================================
// ProcessItemUseInput — 读取 C_D_Input 中的道具按键，发布 Evt_Item_Use
// ============================================================
void Sys_Item::ProcessItemUseInput(Registry& registry) {
    if (!registry.has_ctx<EventBus*>()) return;
    auto* bus = registry.ctx<EventBus*>();
    if (!bus) return;

    if (!registry.has_ctx<Res_ItemInventory2>()) return;
    auto& inv = registry.ctx<Res_ItemInventory2>();

    const bool isOnline = registry.has_ctx<Res_Network>();

    registry.view<C_T_Player, C_D_Input, C_D_Transform>().each(
        [&](EntityID playerId, C_T_Player&, C_D_Input& input, C_D_Transform& tf) {
            int slotPressed = -1;

            // Q: use active gadget slot
            if (slotPressed < 0 && input.gadgetUseJustPressed && registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                if (gs.activeItemSlot < 2) {
                    auto& display = gs.itemSlots[gs.activeItemSlot];
                    if (display.name[0] != '\0')
                        slotPressed = display.itemId;
                }
            }
            // E: use active weapon slot
            if (slotPressed < 0 && input.weaponUseJustPressed && registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                if (gs.activeWeaponSlot < 2) {
                    auto& display = gs.weaponSlots[gs.activeWeaponSlot];
                    if (display.name[0] != '\0')
                        slotPressed = display.itemId;
                }
            }

            if (slotPressed < 0) return;
            if (slotPressed >= Res_ItemInventory2::kItemCount) return;

            ItemID id   = static_cast<ItemID>(slotPressed);
            auto&  slot = inv.Get(id);

            // Cooldown check: find display slot and check if cooling down
            if (registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                for (int s = 0; s < 2; ++s) {
                    if (gs.itemSlots[s].itemId == slotPressed && gs.itemSlots[s].cooldown > 0.01f)
                        return;
                    if (gs.weaponSlots[s].itemId == slotPressed && gs.weaponSlots[s].cooldown > 0.01f)
                        return;
                }
            }

            if (!slot.CanUse()) {
                LOG_INFO("[Sys_Item] Player " << playerId
                         << " tried to use item " << slotPressed << " but count=0");
                return;
            }

            slot.UseOne();
            LOG_INFO("[Sys_Item] Player " << playerId
                     << " used item " << slotPressed
                     << " -> remaining=" << (int)slot.carriedCount);

            // Set cooldown on the display slot + flash timer
            if (registry.has_ctx<Res_GameState>() && slot.cooldownDuration > 0.0f) {
                auto& gs = registry.ctx<Res_GameState>();
                // Normalize cooldown: store as ratio (0..1) for HUD bar
                for (int s = 0; s < 2; ++s) {
                    if (gs.itemSlots[s].itemId == slotPressed) {
                        gs.itemSlots[s].cooldown = 1.0f;
                        gs.itemUseFlashTimer = 0.3f;
                        gs.itemUseFlashSlotType = 0;
                    }
                    if (gs.weaponSlots[s].itemId == slotPressed) {
                        gs.weaponSlots[s].cooldown = 1.0f;
                        gs.itemUseFlashTimer = 0.3f;
                        gs.itemUseFlashSlotType = 1;
                    }
                }
            }

            Evt_Item_Use useEvt;
            useEvt.userEntity   = playerId;
            useEvt.targetEntity = Entity::NULL_ENTITY;
            useEvt.itemId       = id;
            useEvt.targetPos    = tf.position;
            useEvt.isOnline     = isOnline;
            bus->publish_deferred(useEvt);
        }
    );
}

// ============================================================
// SyncDisplaySlots — 每帧同步 Res_ItemInventory2 → Res_GameState 显示槽
// ============================================================
void Sys_Item::SyncDisplaySlots(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_GameState>()) return;
    if (!registry.has_ctx<Res_ItemInventory2>()) return;
    auto& gs  = registry.ctx<Res_GameState>();
    auto& inv = registry.ctx<Res_ItemInventory2>();

    // Sync carried count from inventory to display slots + decay cooldown
    auto decayCooldown = [&](SlotDisplay& slot) {
        if (slot.name[0] == '\0') return;
        int id = slot.itemId;
        if (id >= 0 && id < Res_ItemInventory2::kItemCount) {
            slot.count = inv.slots[id].carriedCount;
            // Decay cooldown based on item's cooldownDuration
            if (slot.cooldown > 0.0f) {
                float dur = inv.slots[id].cooldownDuration;
                float rate = (dur > 0.001f) ? (dt / dur) : dt;
                slot.cooldown -= rate;
                if (slot.cooldown < 0.0f) slot.cooldown = 0.0f;
            }
        }
    };
    for (int s = 0; s < 2; ++s) decayCooldown(gs.itemSlots[s]);
    for (int s = 0; s < 2; ++s) decayCooldown(gs.weaponSlots[s]);

    // Decay flash timer
    if (gs.itemUseFlashTimer > 0.0f) {
        gs.itemUseFlashTimer -= dt;
        if (gs.itemUseFlashTimer < 0.0f)
            gs.itemUseFlashTimer = 0.0f;
    }
}

// ============================================================
// SyncInventoryState — 每帧同步 Res_ItemInventory2 → Res_InventoryState
// ============================================================
void Sys_Item::SyncInventoryState(Registry& registry) {
    if (!registry.has_ctx<Res_InventoryState>()) return;
    if (!registry.has_ctx<Res_ItemInventory2>()) return;
    auto& invState = registry.ctx<Res_InventoryState>();
    auto& inv      = registry.ctx<Res_ItemInventory2>();

    // Fill first 5 slots from inventory, rest stays empty
    for (int i = 0; i < Res_ItemInventory2::kItemCount && i < Res_InventoryState::kSlotCount; ++i) {
        auto& src = inv.slots[i];
        auto& dst = invState.slots[i];
        size_t nLen = strlen(src.name);
        if (nLen > sizeof(dst.name) - 1) nLen = sizeof(dst.name) - 1;
        memcpy(dst.name, src.name, nLen);
        dst.name[nLen] = '\0';
        size_t dLen = strlen(src.desc);
        if (dLen > sizeof(dst.description) - 1) dLen = sizeof(dst.description) - 1;
        memcpy(dst.description, src.desc, dLen);
        dst.description[dLen] = '\0';
        dst.itemId   = static_cast<uint8_t>(src.itemId);
        dst.quantity = src.carriedCount;
        dst.isEmpty  = (src.carriedCount == 0 && src.storeCount == 0);
    }
    for (int i = Res_ItemInventory2::kItemCount; i < Res_InventoryState::kSlotCount; ++i) {
        invState.slots[i] = {};
    }
}

// ============================================================
// AnimatePickups — 每帧旋转地图上的道具实体（视觉效果）
// ============================================================
/// @brief 每帧以 90°/s 旋转所有 C_T_ItemPickup 实体（yaw 轴视觉动画）。
void Sys_Item::AnimatePickups(Registry& registry, float dt) {
    m_PickupAnimTime += dt;
    float yawDeg = std::fmod(m_PickupAnimTime * 90.0f, 360.0f); // 90°/s rotation speed

    auto rot = Quaternion::EulerAnglesToQuaternion(0.0f, yawDeg, 0.0f);

    registry.view<C_T_ItemPickup, C_D_Transform>().each(
        [&](EntityID, C_T_ItemPickup&, C_D_Transform& tf) {
            tf.rotation = rot;
        });
}

} // namespace ECS
