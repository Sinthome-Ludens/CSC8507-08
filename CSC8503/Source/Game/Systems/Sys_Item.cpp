/**
 * @file Sys_Item.cpp
 * @brief 道具系统实现：拾取检测、使用输入分发、携带数量管理与开局/结算逻辑。
 *
 * @details
 * 实现要点：
 *  - OnAwake 中初始化 Res_ItemInventory2 并调用 OnRoundStart()，
 *    订阅 Evt_Item_Pickup 以更新携带数量。
 *  - OnUpdate 分两步：DetectPickup（XZ 范围检测）和 ProcessItemUseInput（数字键 1-5）。
 *  - OnDestroy 取消订阅并执行 OnRoundEnd()（游戏结束结算）。
 *
 * ## 道具使用按键约定（C_D_Input 字段，由 Sys_InputDispatch 填入）
 *  - 数字键 1 → ItemID::HoloBait   （全息诱饵炸弹）
 *  - 数字键 2 → ItemID::PhotonRadar（光子雷达，激活/关闭雷达 HUD）
 *  - 数字键 3 → ItemID::DDoS       （冻结目标）
 *  - 数字键 4 → ItemID::RoamAI     （释放流窜 AI）
 *  - 数字键 5 → ItemID::TargetStrike（靶向打击，目标即死）
 *  紧急更新修复
 */
#include "Sys_Item.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_ItemPickup.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Events/Evt_Item_Pickup.h"
#include "Game/Events/Evt_Item_Use.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Network.h"
#include "Core/ECS/EventBus.h"
#include "Core/ECS/EntityID.h"

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
void Sys_Item::OnUpdate(Registry& registry, float /*dt*/) {
    DetectPickup(registry);
    ProcessItemUseInput(registry);
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
        registry.ctx<Res_ItemInventory2>().OnRoundEnd();
        LOG_INFO("[Sys_Item] OnDestroy: round end settled.");
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
        LOG_INFO("[Sys_Item] Player " << evt.pickerEntity
                 << " picked up " << picked << "x item " << static_cast<int>(evt.itemId)
                 << " -> carried=" << (int)slot.carriedCount);
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
            if      (input.item1JustPressed) slotPressed = 0;
            else if (input.item2JustPressed) slotPressed = 1;
            else if (input.item3JustPressed) slotPressed = 2;
            else if (input.item4JustPressed) slotPressed = 3;
            else if (input.item5JustPressed) slotPressed = 4;

            if (slotPressed < 0) return;

            ItemID id   = static_cast<ItemID>(slotPressed);
            auto&  slot = inv.Get(id);

            if (!slot.CanUse()) {
                LOG_INFO("[Sys_Item] Player " << playerId
                         << " tried to use item " << slotPressed << " but count=0");
                return;
            }

            slot.UseOne();
            LOG_INFO("[Sys_Item] Player " << playerId
                     << " used item " << slotPressed
                     << " -> remaining=" << (int)slot.carriedCount);

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

} // namespace ECS
