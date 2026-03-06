#include "Sys_Camera.h"

#include "Window.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "GameWorld.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_UIState.h"
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
        Vector3(0.0f, 15.0f, 40.0f),
        -20.0f,  // pitch：略俯视
        0.0f     // yaw：朝 -Z 方向
    );

    // ── 注册 Res_CameraContext（供其他 System 快速获取相机信息）──
    {
        Res_CameraContext ctx{};
        ctx.active_camera = entity_camera_main;
        registry.ctx_emplace<Res_CameraContext>(ctx);
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

    LOG_INFO("[Sys_Camera] OnAwake - 主相机实体 id=" << entity_camera_main);
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_Camera::OnUpdate(Registry& registry, float dt) {
    if (!m_GameWorld) return;

    auto* win = Window::GetWindow();
    const bool windowActive = (win != nullptr) && win->IsActiveWindow();

    // UI 阻塞时（菜单/暂停等）跳过所有相机输入，防止 WarpCursorToCenter
    // 拉回光标、鼠标旋转干扰菜单操作、WASD 意外移动相机
    bool uiBlocking = false;
    float uiSensitivity = -1.0f;   // < 0 表示未配置，使用组件默认值
    if (registry.has_ctx<Res_UIState>()) {
        const auto& ui = registry.ctx<Res_UIState>();
        uiBlocking    = ui.isUIBlockingInput;
        uiSensitivity = ui.mouseSensitivity;
    }

    bool cursorFree = false;

    registry.view<C_T_MainCamera, C_D_Camera, C_D_Transform>().each(
        [&](EntityID /*id*/, C_T_MainCamera&, C_D_Camera& cam, C_D_Transform& tf)
        {
            // ── 同步 Settings 鼠标灵敏度到相机组件 ──────────────────────────
            if (uiSensitivity >= 0.0f) cam.sensitivity = uiSensitivity;
            // ── Alt 键：切换鼠标自由模式（按住 Alt 显示光标，不旋转相机）────
            // UI 阻塞时仍跟踪，但不影响光标（Sys_UI 覆盖）
            auto* kb = Window::GetKeyboard();
            if (kb && windowActive) {
                cam.cursor_free = kb->KeyDown(KeyCodes::MENU);
            }
            cursorFree = cam.cursor_free;

            // 光标状态通过 Res_UIState 传递，不直接调用 Window API。
            // Sys_UI（优先级更高）在有 UI 阻塞时覆盖此处设置的值。

            // ── 鼠标旋转（cursor_free 或 UI 阻塞时禁用）─────────────────────
            auto* mouse = Window::GetMouse();
            if (mouse && windowActive && !cam.cursor_free && !uiBlocking) {
                const Vector2 delta = mouse->GetRelativePosition();
                cam.yaw   -= delta.x * cam.sensitivity;
                cam.pitch -= delta.y * cam.sensitivity;
                cam.pitch  = std::clamp(cam.pitch, -89.0f, 89.0f);

                // 每帧将光标归位到窗口中心：防止光标漂到边缘导致原始输入受限
                if (win) win->WarpCursorToCenter();
            }

            // ── 键盘平移（WASD + Q/E，UI 阻塞时禁用）────────────────────────
            if (kb && windowActive && !uiBlocking) {
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
                if (kb->KeyDown(KeyCodes::W)) tf.position += forward * speed;
                if (kb->KeyDown(KeyCodes::S)) tf.position -= forward * speed;
                if (kb->KeyDown(KeyCodes::A)) tf.position -= right   * speed;
                if (kb->KeyDown(KeyCodes::D)) tf.position += right   * speed;
                if (kb->KeyDown(KeyCodes::Q)) tf.position -= up      * speed;
                if (kb->KeyDown(KeyCodes::E)) tf.position += up      * speed;
            }

            // ── Bridge：同步到 NCL GameWorld 相机 ───────────────────
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
    m_GameWorld = nullptr;
    LOG_INFO("[Sys_Camera] OnDestroy");
}

} // namespace ECS
