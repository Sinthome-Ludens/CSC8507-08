/**
 * @file Sys_Door.cpp
 * @brief Key card pickup + locked door unlock implementation.
 */
#include "Sys_Door.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_KeyCard.h"
#include "Game/Components/C_D_DoorLocked.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/UI/UI_Toast.h"
#endif

#include <vector>

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_Door::OnAwake(Registry& registry) {
    LOG_INFO("[Sys_Door] OnAwake.");
}

// ============================================================
// OnUpdate — pick up key → destroy key + matching doors
// ============================================================
void Sys_Door::OnUpdate(Registry& registry, float /*dt*/) {
    // Gather player positions
    struct PlayerInfo { EntityID id; Vector3 pos; };
    std::vector<PlayerInfo> players;
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID pid, C_T_Player&, C_D_Transform& tf) {
            players.push_back({ pid, tf.position });
        }
    );
    if (players.empty()) return;

    // Collect keys picked up this frame (keyEnt + keyId)
    struct PickedKey { EntityID entity; uint8_t keyId; };
    std::vector<PickedKey> pickedKeys;

    registry.view<C_T_KeyCard, C_D_Transform>().each(
        [&](EntityID keyEnt, C_T_KeyCard& keyCard, C_D_Transform& keyTf) {
            for (auto& p : players) {
                float dx = p.pos.x - keyTf.position.x;
                float dz = p.pos.z - keyTf.position.z;
                if ((dx * dx + dz * dz) > kKeyPickupRadius * kKeyPickupRadius) continue;

                pickedKeys.push_back({ keyEnt, keyCard.keyId });

                LOG_INFO("[Sys_Door] Player " << p.id
                         << " picked up KeyCard id=" << (int)keyCard.keyId);
#ifdef USE_IMGUI
                UI::PushToast(registry, "DOOR UNLOCKED", ToastType::Success, 2.0f);
#endif
                break;
            }
        }
    );

    if (pickedKeys.empty()) return;

    // Destroy picked keys
    for (auto& pk : pickedKeys) {
        if (registry.Valid(pk.entity)) registry.Destroy(pk.entity);
    }

    // Destroy all doors whose requiredKeyId matches any picked key
    std::vector<EntityID> doorsToDestroy;
    registry.view<C_D_DoorLocked>().each(
        [&](EntityID doorEnt, C_D_DoorLocked& door) {
            for (auto& pk : pickedKeys) {
                if (door.requiredKeyId == pk.keyId) {
                    doorsToDestroy.push_back(doorEnt);
                    LOG_INFO("[Sys_Door] Destroyed door requiring keyId=" << (int)door.requiredKeyId);
                    break;
                }
            }
        }
    );
    for (auto ent : doorsToDestroy) {
        if (registry.Valid(ent)) registry.Destroy(ent);
    }
}

} // namespace ECS
