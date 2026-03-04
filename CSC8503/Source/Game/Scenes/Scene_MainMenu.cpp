#include "Scene_MainMenu.h"

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Components/Res_UIState.h"
#endif

// ============================================================
// OnEnter
// ============================================================

void Scene_MainMenu::OnEnter(ECS::Registry&          registry,
                              ECS::SystemManager&     systems,
                              const Res_NCL_Pointers& /*nclPtrs*/)
{
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>(300);
    systems.Register<ECS::Sys_UI>  (500);
#endif

    systems.AwakeAll(registry);

#ifdef USE_IMGUI
    {
        bool firstLaunch = !registry.has_ctx<ECS::Res_UIState>();
        if (firstLaunch) {
            registry.ctx_emplace<ECS::Res_UIState>();
        }
        auto& ui = registry.ctx<ECS::Res_UIState>();

        ui.activeScreen      = firstLaunch ? ECS::UIScreen::TitleScreen
                                           : ECS::UIScreen::Splash;
        ui.titleTimer        = 0.0f;
        ui.splashTimer       = 0.0f;
        ui.menuSelectedIndex = 0;
        ui.previousScreen    = ECS::UIScreen::None;
        ui.prePauseScreen    = ECS::UIScreen::None;
        ui.pendingSceneRequest = ECS::SceneRequest::None;
    }
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

    LOG_INFO("[Scene_MainMenu] OnExit complete. All systems destroyed.");
}
