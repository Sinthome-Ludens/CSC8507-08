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
    // 主菜单场景只需要UI系统
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>(300);
    systems.Register<ECS::Sys_UI>  (500);
#endif

    systems.AwakeAll(registry);

    // 重置 UI 状态：确保从其他场景返回时显示 Splash 画面
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();
        ui.activeScreen      = ECS::UIScreen::Splash;
        ui.splashTimer       = 0.0f;
        ui.menuSelectedIndex = 0;
        ui.menuAnimTimer     = 0.0f;
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
