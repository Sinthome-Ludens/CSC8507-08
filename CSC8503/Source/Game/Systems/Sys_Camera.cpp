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

    registry.view<C_T_MainCamera, C_D_Camera, C_D_Transform>().each(
        [&](EntityID /*id*/, C_T_MainCamera&, C_D_Camera& cam, C_D_Transform& tf)
        {
            // ── Alt 键：切换鼠标自由模式（按住 Alt 显示光标，不旋转相机）────
            auto* kb = Window::GetKeyboard();
            if (kb && windowActive) {
                cam.cursor_free = kb->KeyDown(KeyCodes::MENU);
            }

            // 每帧设置光标状态（而非仅在状态变化时），确保进入 HUD 时正确初始化
            if (win && windowActive) {
                win->ShowOSPointer(cam.cursor_free);
                win->LockMouseToWindow(!cam.cursor_free);
            }

            // ── 鼠标旋转（cursor_free 模式下禁用）──────────────────────────
            auto* mouse = Window::GetMouse();
            if (mouse && windowActive && !cam.cursor_free) {
                const Vector2 delta = mouse->GetRelativePosition();
                cam.yaw   -= delta.x * cam.sensitivity;
                cam.pitch -= delta.y * cam.sensitivity;
                cam.pitch  = std::clamp(cam.pitch, -89.0f, 89.0f);

                // 每帧将光标归位到窗口中心：防止光标漂到边缘导致原始输入受限
                if (win) win->WarpCursorToCenter();
            }

            // ── 键盘平移（WASD + Q/E，cursor_free 时仍可移动）──────────────
            if (kb && windowActive) {
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
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_Camera::OnDestroy(Registry& registry) {
    m_GameWorld = nullptr;
    LOG_INFO("[Sys_Camera] OnDestroy");
}

} // namespace ECS
