#include "Scene_MainMenu.h"

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/UI/UI_Toast.h"
#endif

// ============================================================
// OnEnter
// ============================================================

void Scene_MainMenu::OnEnter(ECS::Registry&          registry,
                              ECS::SystemManager&     systems,
                              const Res_NCL_Pointers& /*nclPtrs*/)
{
#ifdef USE_IMGUI
    // Res_UIState 必须在 AwakeAll 之前初始化，
    // 否则 Sys_UI::OnAwake 会以默认值创建它，导致 firstLaunch 判断失效
    {
        bool firstLaunch = !registry.has_ctx<ECS::Res_UIState>();
        if (firstLaunch) {
            registry.ctx_emplace<ECS::Res_UIState>();
        }
        auto& ui = registry.ctx<ECS::Res_UIState>();

        // 重置导航状态，但保留用户设置（音量/灵敏度/全屏等）
        ui.activeScreen        = ECS::UIScreen::TitleScreen;
        ui.titleTimer          = 0.0f;
        ui.splashTimer         = 0.0f;
        ui.menuSelectedIndex       = 0;
        ui.settingsSelectedIndex   = 0;
        ui.pauseSelectedIndex      = 0;
        ui.gameOverSelectedIndex   = 0;
        ui.previousScreen          = ECS::UIScreen::None;
        ui.prePauseScreen          = ECS::UIScreen::None;
        ui.pendingSceneRequest     = ECS::SceneRequest::None;

        // New UI state fields
        ui.loadoutSelectedIndex    = 0;
        ui.inventorySelectedSlot   = 0;
        ui.teamStartTime           = 0.0f;
        ui.itemWheelOpen           = false;
        ui.itemWheelSelected       = -1;
        ui.transitionTimer         = 0.0f;
        ui.transitionActive        = false;
        ui.transitionType          = 0;
        ui.transitionSceneRequest  = ECS::SceneRequest::None;
    }

    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    systems.Register<ECS::Sys_ImGui>(300);
    systems.Register<ECS::Sys_UI>  (500);
#endif

    systems.AwakeAll(registry);

#ifdef USE_IMGUI
    ECS::UI::PushToast(registry, "SYSTEM ONLINE", ECS::ToastType::Success, 2.5f);
#endif

    LOG_INFO("[Scene_MainMenu] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit
// ============================================================

void Scene_MainMenu::OnExit(ECS::Registry&      registry,
                             ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);

#ifdef USE_IMGUI
    // 清除场景级 ctx 资源，防止跨场景状态泄漏（registry.Clear() 不清除 ctx）
    if (registry.has_ctx<Res_UIFlags>()) registry.ctx_erase<Res_UIFlags>();
#endif

    registry.Clear();

    LOG_INFO("[Scene_MainMenu] OnExit complete. All systems destroyed, entities cleared.");
}
