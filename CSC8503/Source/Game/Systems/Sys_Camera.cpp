#include "Sys_Camera.h"

#include "Window.h"
#include "GameWorld.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_Input.h"
#include "Core/Bridge/ImGuiAdapter.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/Assert.h"

#include <algorithm>  // std::clamp
#include <cmath>      // sinf, cosf

using namespace NCL;
using namespace NCL::Maths;
using namespace NCL::CSC8503;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_Camera::OnAwake(Registry& registry) {
    GAME_ASSERT(registry.has_ctx<Res_NCL_Pointers>(),
                "[Sys_Camera] Res_NCL_Pointers 未注册，请在 AwakeAll 之前 ctx_emplace。");

    m_GameWorld = registry.ctx<Res_NCL_Pointers>().world;
    GAME_ASSERT(m_GameWorld != nullptr,
                "[Sys_Camera] GameWorld 指针为空。");

    // ── 通过 PrefabFactory 创建主相机实体 ──────────────────────
    EntityID entity_camera_main = PrefabFactory::CreateCameraMain(
        registry,
        Vector3(0.0f, 25.0f, 6.7f),
        -75.0f,  // pitch：75° 俯视角
        0.0f     // yaw：朝 -Z 方向
    );

    // ── 注册 Res_CameraContext（供其他 System 快速获取相机信息）──
    // 无条件覆盖：场景重进时旧 active_camera 可能悬空
    {
        Res_CameraContext camCtx{};
        camCtx.active_camera = entity_camera_main;
        registry.ctx_emplace<Res_CameraContext>(camCtx);
    }

    // ── 初始化场景光照（Bridge：写入 NCL GameWorld）──────────────
    m_GameWorld->SetSunPosition(Vector3(-200.0f, 60.0f, -200.0f));
    m_GameWorld->SetSunColour(Vector3(0.8f, 0.8f, 0.5f));

    // ── 首帧同步：将相机实体状态推送到 NCL ────────────────────────
    auto& tf  = registry.Get<C_D_Transform>(entity_camera_main);
    auto& cam = registry.Get<C_D_Camera>(entity_camera_main);
    m_GameWorld->GetMainCamera()
        .SetPosition(tf.position)
        .SetPitch(cam.pitch)
        .SetYaw(cam.yaw);

    // ── 注册 Sys_Camera* 到 ctx，供 Sys_PlayerCamera 读取 debug 状态 ──
    registry.ctx_emplace<Sys_Camera*>(this);

    LOG_INFO("[Sys_Camera] OnAwake - 主相机实体 id=" << entity_camera_main);
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_Camera::OnUpdate(Registry& registry, float dt) {
    if (!m_GameWorld) return;

    auto* win = Window::GetWindow();
    const bool windowActive = (win != nullptr) && win->IsActiveWindow();
    const auto& input = registry.ctx<Res_Input>();

    // ── 读取 UI 状态（Linyn-UIdesign）────────────────────────────
    bool uiBlocking     = false;
    bool itemWheelOpen  = false;
    float uiSensitivity = -1.0f;   // < 0 表示未配置，使用组件默认值
    if (registry.has_ctx<Res_UIState>()) {
        const auto& ui = registry.ctx<Res_UIState>();
        uiBlocking     = ui.isUIBlockingInput;
        uiSensitivity  = ui.mouseSensitivity;
        itemWheelOpen  = ui.itemWheelOpen;
    }

    // ImGui 捕获检查：正在拖拽/输入 ImGui 控件时禁用相机
    // 用 IsAnyItemActive() 而非 WantCaptureMouse：后者在 hover 时就为 true，
    // 会导致相机锁定模式下光标经过 ImGui 窗口时旋转中断。
#ifdef USE_IMGUI
    bool imguiCapturingMouse    = ImGui::IsAnyItemActive() && ImGui::GetIO().WantCaptureMouse;
    bool imguiCapturingKeyboard = ImGui::GetIO().WantCaptureKeyboard && ImGui::IsAnyItemActive();
#else
    bool imguiCapturingMouse    = false;
    bool imguiCapturingKeyboard = false;
#endif

    bool cursorFree = false;

    registry.view<C_T_MainCamera, C_D_Camera, C_D_Transform>().each(
        [&](EntityID /*id*/, C_T_MainCamera&, C_D_Camera& cam, C_D_Transform& tf)
        {
            // ── 同步 Settings 鼠标灵敏度到相机组件 ──────────────────────────
            if (uiSensitivity >= 0.0f) cam.sensitivity = uiSensitivity;

            // ── Alt 键：切换鼠标自由模式（按住 Alt 显示光标，不旋转相机）──
            // 始终可用，不受 Debug 模式限制
            if (windowActive) {
                cam.cursor_free = input.keyStates[KeyCodes::MENU];
            }

            // ── Debug 模式：WASD/鼠标自由飞行（默认关闭）────────────────────
            if (m_DebugMode) {

                // ── 鼠标旋转（cursor_free 或 UI 阻塞或 ImGui 捕获时禁用）─────
                if (windowActive && !cam.cursor_free && !uiBlocking && !itemWheelOpen && !imguiCapturingMouse) {
                    const Vector2 delta = input.mouseDelta;
                    cam.yaw   -= delta.x * cam.sensitivity;
                    cam.pitch -= delta.y * cam.sensitivity;
                    cam.pitch  = std::clamp(cam.pitch, -89.0f, 89.0f);

                    if (win) win->WarpCursorToCenter();
                }

                // ── 键盘平移（WASD + Q/E，UI 阻塞或 ImGui 捕获键盘时禁用）──
                if (!m_SyncToPlayer && windowActive && !uiBlocking && !imguiCapturingKeyboard) {
                    const float yawRad   = cam.yaw   * (3.14159265f / 180.0f);
                    const float pitchRad = cam.pitch * (3.14159265f / 180.0f);

                    // 前方向量（包含 pitch，实现仰望/俯视时仍能前进）
                    const Vector3 forward(
                        -sinf(yawRad) * cosf(pitchRad),
                         sinf(pitchRad),
                        -cosf(yawRad) * cosf(pitchRad)
                    );
                    const Vector3 right(cosf(yawRad), 0.0f, -sinf(yawRad));
                    const Vector3 up(0.0f, 1.0f, 0.0f);

                    const float speed = cam.move_speed * dt;
                    if (input.keyStates[KeyCodes::W]) tf.position += forward * speed;
                    if (input.keyStates[KeyCodes::S]) tf.position -= forward * speed;
                    if (input.keyStates[KeyCodes::A]) tf.position -= right   * speed;
                    if (input.keyStates[KeyCodes::D]) tf.position += right   * speed;
                    if (input.keyStates[KeyCodes::Q]) tf.position -= up      * speed;
                    if (input.keyStates[KeyCodes::E]) tf.position += up      * speed;
                }
            }

            cursorFree = cam.cursor_free;

            // ── Bridge：始终同步到 NCL GameWorld 相机 ───────────────────────
            m_GameWorld->GetMainCamera()
                .SetPosition(tf.position)
                .SetPitch(cam.pitch)
                .SetYaw(cam.yaw);
        }
    );

    // ── 光标状态写入 Res_UIState（单通道，由 Main.cpp 统一应用）──────
    // 此处为 fallback：没有 Sys_UI 的场景（如 NavTest）直接使用此值。
    // 有 Sys_UI 的场景中，Sys_UI（优先级 500，后运行）会覆盖这些值。
    if (registry.has_ctx<Res_UIState>()) {
        auto& ui = registry.ctx<Res_UIState>();
        ui.gameCursorFree = cursorFree;
        ui.cursorVisible  = cursorFree;
        ui.cursorLocked   = !cursorFree;
    }
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_Camera::OnDestroy(Registry& registry) {
    if (registry.has_ctx<Sys_Camera*>()) {
        registry.ctx<Sys_Camera*>() = nullptr;
    }
    m_GameWorld = nullptr;
    LOG_INFO("[Sys_Camera] OnDestroy");
}

} // namespace ECS