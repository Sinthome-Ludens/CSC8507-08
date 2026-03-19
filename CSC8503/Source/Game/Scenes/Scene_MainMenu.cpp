/**
 * @file Scene_MainMenu.cpp
 * @brief 主菜单场景生命周期实现。
 */
#include "Scene_MainMenu.h"

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Utils/Log.h"

#include "Game/Systems/Sys_Input.h"
#include "Game/Systems/Sys_Audio.h"
#include "Game/Components/Res_AudioConfig.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/UI/UI_Toast.h"
#endif

// ============================================================
// OnEnter
// ============================================================

/**
 * @brief 进入主菜单场景并初始化菜单相关系统与 UI 资源。
 * @details 在系统启动前准备 UI 状态、窗口显隐标志和菜单所需的上下文资源，然后注册 ImGui/UI 系统并唤醒全部系统。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 * @param nclPtrs NCL 桥接资源（当前函数未直接使用）
 */
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
        ui.inventorySelectedSlot   = 0;
        ui.teamStartTime           = 0.0f;
        ui.itemWheelOpen           = false;
        ui.itemWheelSelected       = -1;
        ui.transitionTimer         = 0.0f;
        ui.transitionActive        = false;
        ui.transitionType          = 0;
        ui.transitionSceneRequest  = ECS::SceneRequest::None;

        // 重置场景过渡锁（防止卡在 Loading screen）
        ui.sceneRequestDispatched = false;

        // 重置光标状态（菜单场景需要自由光标）
        ui.gameCursorFree = false;  // 菜单不是游戏模式
        ui.cursorVisible  = true;   // 菜单需要可见光标
        ui.cursorLocked   = false;  // 菜单不锁定光标
    }

    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    systems.Register<ECS::Sys_Input>            ( 10);
    systems.Register<ECS::Sys_ImGui>           (300);
    systems.Register<ECS::Sys_ImGuiEntityDebug>(310);
    systems.Register<ECS::Sys_UI>              (500);
#endif
    systems.Register<ECS::Sys_Audio>           (275);

    systems.AwakeAll(registry);

    // ── Audio state（必须在 AwakeAll 之后，Sys_Audio::OnAwake 已创建 Res_AudioState）──
    if (registry.has_ctx<ECS::Res_AudioState>()) {
        auto& audio = registry.ctx<ECS::Res_AudioState>();
        audio.isGameplay   = false;
        audio.requestedBgm = ECS::BgmId::Menu;
        audio.bgmOverride  = false;
    }

#ifdef USE_IMGUI
    ECS::UI::PushToast(registry, "SYSTEM ONLINE", ECS::ToastType::Success, 2.5f);
#endif

    LOG_INFO("[Scene_MainMenu] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit
// ============================================================

/**
 * @brief 退出主菜单场景并清理菜单相关上下文资源。
 * @details 逆序销毁当前场景系统，移除本场景持有的 UI 相关 context，并最终清空 Registry 中的实体与组件数据。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 */
void Scene_MainMenu::OnExit(ECS::Registry&      registry,
                              ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);

#ifdef USE_IMGUI
    // 清除场景级 ctx 资源，防止跨场景状态泄漏（registry.Clear() 不清除 ctx）
    if (registry.has_ctx<Res_UIFlags>())            registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<ECS::Res_ToastState>())    registry.ctx_erase<ECS::Res_ToastState>();
    if (registry.has_ctx<ECS::Res_ChatState>())     registry.ctx_erase<ECS::Res_ChatState>();
    if (registry.has_ctx<ECS::Res_InventoryState>()) registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())    registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())  registry.ctx_erase<ECS::Res_DialogueData>();
#endif

    registry.Clear();

    LOG_INFO("[Scene_MainMenu] OnExit complete. All systems destroyed, entities cleared.");
}
