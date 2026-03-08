/**
 * @file Sys_UI.h
 * @brief ECS system — UI state machine, input handling, and render dispatch
 *
 * @details
 * Sys_UI owns all menu navigation logic (ESC/Enter/hotkeys), cursor arbitration,
 * and delegates rendering to stateless functions in UI/.
 *
 * ## ctx Resource Lifecycle
 *
 * | Level     | Resource            | Created          | Destroyed             |
 * |-----------|---------------------|------------------|-----------------------|
 * | Session   | Res_UIState         | first OnAwake    | never (cross-scene)   |
 * | Scene     | Res_ToastState      | OnAwake          | Scene::OnExit         |
 * | Scene     | Res_ChatState       | OnAwake          | Scene::OnExit         |
 * | Scene     | Res_InventoryState  | OnAwake          | Scene::OnExit         |
 * | Scene     | Res_LobbyState      | OnAwake          | Scene::OnExit         |
 * | Scene     | Res_DialogueData    | Sys_Chat::OnAwake| Scene::OnExit         |
 * | Scene     | Res_GameState       | Scene::OnEnter   | Scene::OnExit         |
 *
 * **Session** resources survive scene transitions (user settings).
 * **Scene** resources are erased in each Scene's OnExit() and re-created on
 * the next OnAwake() via `has_ctx` guard. When adding a new ctx resource,
 * decide which level it belongs to and add matching cleanup in Scene::OnExit().
 *
 * @see UI/ (stateless render functions called by OnUpdate)
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_UI : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;
};

} // namespace ECS

#endif // USE_IMGUI
