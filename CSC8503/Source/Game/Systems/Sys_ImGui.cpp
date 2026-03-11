/**
 * @file Sys_ImGui.cpp
 * @brief ImGui 渲染系统实现：主菜单栏、Debug 窗口、测试场景控制面板（Cube/Capsule/Gravity）
 */
#include "Sys_ImGui.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_Network.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Utils/Log.h"
#include "GameWorld.h"
#include "Constraint.h"
#include "PhysicsSystem.h"
#include "GameTechRendererInterface.h"
#include <algorithm>
#include <iterator>
#include <cmath>

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// OnAwake / OnDestroy
// ============================================================

/**
 * @brief 初始化 ImGui 通用窗口系统。
 * @param registry 当前场景注册表
 */
void Sys_ImGui::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGui] OnAwake");
}

/**
 * @brief 销毁 ImGui 通用窗口系统。
 * @param registry 当前场景注册表
 */
void Sys_ImGui::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGui] OnDestroy");
}

// ============================================================
// OnUpdate
// ============================================================

/**
 * @brief 渲染 ImGui 通用菜单栏与基础调试窗口。
 * @details 当开发者模式开启时，负责驱动主菜单栏、性能调试、NCL 状态和测试控制等基础窗口。
 * @param registry 当前场景注册表
 * @param dt 本帧时间步长（秒）
 */
void Sys_ImGui::OnUpdate(Registry& registry, float dt) {
    // 开发者模式关闭时隐藏所有调试界面
    if (registry.has_ctx<Res_UIState>()
        && !registry.ctx<Res_UIState>().devMode) {
        return;
    }

    RenderMainMenuBar(registry);

    if (m_ShowDemoWindow)  ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    if (m_ShowDebugWindow) RenderDebugWindow(registry, dt);
    if (m_ShowNCLStatus)   RenderNCLStatus(registry);

    // 调试窗口：由 Res_UIFlags context 控制显隐
    if (registry.has_ctx<Res_UIFlags>()) {
        auto& flags = registry.ctx<Res_UIFlags>();
        if (flags.showTestControls) RenderTestControlsWindow(registry);
        if (flags.showNetworkDebug && registry.has_ctx<Res_Network>())
            RenderNetworkDebugWindow(registry);
    }
}

// ============================================================
// RenderMainMenuBar
// ============================================================

/**
 * @brief 渲染主菜单栏。
 * @details 提供基础窗口显隐开关、测试场景工具入口和调试场景切换请求入口。
 * @param registry 当前场景注册表
 */
void Sys_ImGui::RenderMainMenuBar(Registry& registry) {
    if (!ImGui::BeginMainMenuBar()) return;

    // ── Windows 菜单 ──────────────────────────────────────────────
    if (ImGui::BeginMenu("Windows")) {
        ImGui::MenuItem("Demo Window",  nullptr, &m_ShowDemoWindow);
        ImGui::MenuItem("Debug Window", nullptr, &m_ShowDebugWindow);
        ImGui::MenuItem("NCL Status",   nullptr, &m_ShowNCLStatus);
        if (registry.has_ctx<Res_UIFlags>()) {
            auto& flags = registry.ctx<Res_UIFlags>();
            ImGui::MenuItem("Test Controls", nullptr, &flags.showTestControls);
            ImGui::MenuItem("Entity Debug",  nullptr, &flags.showEntityDebug);
            ImGui::MenuItem("Network Debug", nullptr, &flags.showNetworkDebug);
        }
        ImGui::EndMenu();
    }

    // ── Test Scene 子菜单（master）───────────────────────────────
    // 集中管理所有测试浮窗的开关，包含 Raycast 开关
    if (registry.has_ctx<Res_UIFlags>()) {
        auto& flags = registry.ctx<Res_UIFlags>();
        if (ImGui::BeginMenu("Test Scene")) {
            ImGui::MenuItem("Test Controls", nullptr, &flags.showTestControls);
            ImGui::MenuItem("Entity Debug",  nullptr, &flags.showEntityDebug);
            ImGui::MenuItem("Network Debug", nullptr, &flags.showNetworkDebug);
            ImGui::MenuItem("Raycast",       nullptr, &flags.showRaycast);
            ImGui::EndMenu();
        }
    }

    // ── DebugSceneSelector 菜单（Linyn-UIdesign）─────────────────
    // 列出所有可用场景，点击即可通过 flags.debugSceneIndex 切换
    if (ImGui::BeginMenu("DebugSceneSelector")) {
        if (registry.has_ctx<Res_UIFlags>()) {
            auto& flags = registry.ctx<Res_UIFlags>();
            if (ImGui::MenuItem("Scene_MainMenu"))    flags.debugSceneIndex = 0;
            if (ImGui::MenuItem("Scene_PhysicsTest")) flags.debugSceneIndex = 1;
            if (ImGui::MenuItem("Scene_NavTest"))     flags.debugSceneIndex = 2;
            if (ImGui::MenuItem("Scene_NetworkGame")) flags.debugSceneIndex = 3;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ============================================================
// RenderDebugWindow
// ============================================================

/**
 * @brief 渲染基础调试窗口。
 * @details 展示帧率、实体数量、渲染状态，以及与相机调试相关的常用控制项。
 * @param registry 当前场景注册表
 * @param dt 本帧时间步长（秒）
 */
void Sys_ImGui::RenderDebugWindow(Registry& registry, float dt) {
    ImGui::Begin("Debug Window", &m_ShowDebugWindow);
    ImGui::Text("FPS: %.1f",           (dt > 0.0f) ? 1.0f / dt : 0.0f);
    ImGui::Text("Frame Time: %.3f ms", dt * 1000.0f);
    ImGui::Separator();
    ImGui::Text("ECS Entities: %d", (int)registry.EntityCount());
    ImGui::Separator();
    if (registry.has_ctx<Res_NCL_Pointers>()) {
        auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
        if (nclPtrs.renderer) {
            if (ImGui::Checkbox("Wireframe Mode", &m_WireframeMode)) {
                nclPtrs.renderer->SetWireframeMode(m_WireframeMode);
            }
        }
    }

    // ── 相机控制面板 ──
    ImGui::Separator();
    ImGui::Text("Camera Controls");
    if (registry.has_ctx<Sys_Camera*>()) {
        auto* camSys = registry.ctx<Sys_Camera*>();
        if (camSys) {
            bool debugMode = camSys->IsDebugMode();
            if (ImGui::Checkbox("Free Camera (WASD + Mouse)", &debugMode)) {
                camSys->SetDebugMode(debugMode);
                LOG_INFO("[ImGui] Camera Debug Mode: " << (debugMode ? "ON" : "OFF"));
            }

            // Sync WASD to Player 复选框（仅在 Free Camera 开启时显示）
            if (debugMode) {
                bool syncToPlayer = camSys->IsSyncToPlayer();
                if (ImGui::Checkbox("Sync WASD to Player", &syncToPlayer)) {
                    camSys->SetSyncToPlayer(syncToPlayer);
                }
                if (syncToPlayer) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f),
                        "WASD: Move Player | Mouse: Free Look | Alt: Cursor");
                } else {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                        "WASD: Move Camera | Q/E: Up/Down | Alt: Cursor");
                }
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Enable to unlock camera");
            }
        }
    }

    ImGui::End();
}

// ============================================================
// RenderNCLStatus
// ============================================================

/**
 * @brief 渲染 NCL 运行状态窗口。
 * @details 展示底层 GameWorld 对象数量、约束数量和 Physics Bridge 是否可用等状态信息。
 * @param registry 当前场景注册表
 */
void Sys_ImGui::RenderNCLStatus(Registry& registry) {
    ImGui::Begin("NCL Status", &m_ShowNCLStatus);

    if (registry.has_ctx<Res_NCL_Pointers>()) {
        auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
        if (nclPtrs.world) {
            NCL::CSC8503::GameObjectIterator first, last;
            nclPtrs.world->GetObjectIterators(first, last);
            int objCount = (int)std::distance(first, last);

            std::vector<NCL::CSC8503::Constraint*>::const_iterator cFirst, cLast;
            nclPtrs.world->GetConstraintIterators(cFirst, cLast);
            int constraintCount = (int)std::distance(cFirst, cLast);

            ImGui::Text("GameWorld Objects:  %d", objCount);
            ImGui::Text("Constraints:        %d", constraintCount);
        }
        if (nclPtrs.physics) {
            ImGui::Text("Physics: Active");
        }
    } else {
        ImGui::TextDisabled("Res_NCL_Pointers not registered");
    }

    ImGui::End();
}

// ============================================================
// RenderTestControlsWindow（集成控制面板）
// ============================================================
/**
 * @brief 渲染 ECS Test Controls ImGui 面板（集成控制面板实现）
 *
 * @details
 * - **Cube/Capsule 生成**：读取 Res_CameraContext 计算相机正前方 5 单位为基准生成点；
 *   Capsule 支持两种模式：Overlap Spawn（微扰动密集生成，便于观察挤压/分离）和
 *   Grid Spawn（GRID_STEP=1.0 / GRID_COLS=4 网格分散，避免同点爆冲）。
 * - **生成位置 clamp**：MIN_SPAWN_Y = -2.0f，防止生成在地板以下。
 * - **按钮禁用规则**：无存活实体时 Delete 按钮禁用；capsuleMeshHandle 无效时
 *   Spawn Capsule 按钮禁用。
 * - **实体列表维护**：列表仅在 Spawn/Delete 事件时更新，不在此渲染路径做每帧扫描；
 *   批量操作（SetGravityAll）触发时通过 CleanupTestEntities() 做防御性清理。
 * - **Overlap Spawn 开关**：由 Res_TestState::capsuleOverlapSpawn 显式管理，
 *   避免 static 局部变量跨场景隐式持久化。
 */
void Sys_ImGui::RenderTestControlsWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();

    ImGui::Begin("ECS Test Controls", &flags.showTestControls);

    ImGui::Text("== Cube Factory ==");
    ImGui::Separator();

    if (ImGui::Button("Spawn Cube",  ImVec2(120, 30))) SpawnCube(registry);
    ImGui::SameLine();
    const bool canDeleteCube = registry.has_ctx<Res_TestState>()
                            && !registry.ctx<Res_TestState>().cubeEntities.empty();
    if (!canDeleteCube) ImGui::BeginDisabled();
    if (ImGui::Button("Delete Cube", ImVec2(120, 30))) DeleteLastCube(registry);
    if (!canDeleteCube) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Text("== Capsule Factory ==");
    ImGui::Separator();

    if (registry.has_ctx<Res_TestState>()) {
        auto& state = registry.ctx<Res_TestState>();
        ImGui::Checkbox("Overlap Spawn", &state.capsuleOverlapSpawn);
        const bool canSpawnCapsule = (state.capsuleMeshHandle != ECS::INVALID_HANDLE);

        if (!canSpawnCapsule) ImGui::BeginDisabled();
        if (ImGui::Button("Spawn Capsule", ImVec2(120, 30))) SpawnCapsule(registry);
        if (!canSpawnCapsule) ImGui::EndDisabled();
        if (!canSpawnCapsule) ImGui::TextDisabled("Capsule mesh unavailable");

        ImGui::SameLine();
        const bool canDeleteCapsule = !state.capsuleEntities.empty();
        if (!canDeleteCapsule) ImGui::BeginDisabled();
        if (ImGui::Button("Delete Capsule", ImVec2(120, 30))) DeleteLastCapsule(registry);
        if (!canDeleteCapsule) ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::Text("== Gravity ==");
    ImGui::Separator();

    if (ImGui::Button("Enable Gravity",  ImVec2(120, 30))) SetGravityAll(registry, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Disable Gravity", ImVec2(120, 30))) SetGravityAll(registry, 0.0f);

    ImGui::Spacing();
    ImGui::Separator();

    if (registry.has_ctx<Res_TestState>()) {
        auto& state = registry.ctx<Res_TestState>();
        ImGui::Text("Cubes alive: %d",   (int)state.cubeEntities.size());
        ImGui::Text("Capsules alive: %d", (int)state.capsuleEntities.size());
    }

    ImGui::End();
}

// ============================================================
// RenderNetworkDebugWindow
// ============================================================

void Sys_ImGui::RenderNetworkDebugWindow(Registry& registry) {
    if (!registry.has_ctx<Res_Network>()) return;
    auto& resNet = registry.ctx<Res_Network>();
    auto& flags  = registry.ctx<Res_UIFlags>();

    ImGui::Begin("Network Debug", &flags.showNetworkDebug);

    // 1. 连接状态
    ImGui::Text("Mode: ");
    ImGui::SameLine();
    if (resNet.mode == PeerType::SERVER)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "SERVER");
    else if (resNet.mode == PeerType::CLIENT)
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "CLIENT");
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "OFFLINE");

    ImGui::Text("Status: %s",        resNet.connected ? "CONNECTED" : "DISCONNECTED");
    ImGui::Text("Local Client ID: %u", resNet.localClientID);
    ImGui::Text("RTT: %u ms",         resNet.rtt);

    ImGui::Separator();

    // 2. 流量统计
    if (ImGui::CollapsingHeader("Traffic Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "traffic");
        ImGui::Text("Packets Sent:");  ImGui::NextColumn();
        ImGui::Text("%llu", resNet.packetsSent);     ImGui::NextColumn();
        ImGui::Text("Packets Recv:");  ImGui::NextColumn();
        ImGui::Text("%llu", resNet.packetsReceived); ImGui::NextColumn();
        ImGui::Text("Bytes Sent:");    ImGui::NextColumn();
        ImGui::Text("%.2f KB", resNet.bytesSent     / 1024.0f); ImGui::NextColumn();
        ImGui::Text("Bytes Recv:");    ImGui::NextColumn();
        ImGui::Text("%.2f KB", resNet.bytesReceived / 1024.0f); ImGui::NextColumn();
        ImGui::Columns(1);
    }

    ImGui::Separator();

    // 3. NetID 映射表
    if (ImGui::CollapsingHeader("NetID Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("netid_table", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("NetID");
            ImGui::TableSetupColumn("EntityID");
            ImGui::TableSetupColumn("Owner");
            ImGui::TableHeadersRow();

            for (auto const& [netID, entityID] : resNet.netIdMap) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%u", netID);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", entityID);

                uint32_t owner = 0;
                if (registry.Valid(entityID)
                    && registry.Has<C_D_NetworkIdentity>(entityID)) {
                    owner = registry.Get<C_D_NetworkIdentity>(entityID).ownerClientID;
                }
                ImGui::TableSetColumnIndex(2); ImGui::Text("%u", owner);
            }
            ImGui::EndTable();
        }
    }

    ImGui::Separator();

    // 4. 插值状态
    if (ImGui::CollapsingHeader("Interpolation Buffers")) {
        registry.view<C_D_NetworkIdentity, C_D_InterpBuffer>().each(
            [&](EntityID id, auto& net, auto& buffer) {
                ImGui::Text("Entity %u (NetID %u): %d snapshots",
                            id, net.netID, buffer.count);
            });
    }

    ImGui::End();
}

// ============================================================
// SpawnCube（通过 PrefabFactory 生成动态方块）
// ============================================================

void Sys_ImGui::SpawnCube(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.cubeMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnCube: cube mesh handle is INVALID, skipping.");
        return;
    }

    // ── 计算生成位置：相机正前方 5 单位 ────────────────────────────────
    using namespace NCL::Maths;
    Vector3 spawnPos(0.0f, 8.0f, 0.0f);  // 默认兜底位置

    if (registry.has_ctx<Res_CameraContext>()) {
        auto& camCtx = registry.ctx<Res_CameraContext>();
        if (registry.Valid(camCtx.active_camera)
            && registry.Has<C_D_Transform>(camCtx.active_camera)
            && registry.Has<C_D_Camera>  (camCtx.active_camera))
        {
            auto& tf  = registry.Get<C_D_Transform>(camCtx.active_camera);
            auto& cam = registry.Get<C_D_Camera>   (camCtx.active_camera);

            // 计算水平前方向量（忽略 pitch，避免生成在地下或天上）
            const float yawRad = cam.yaw * (3.14159265f / 180.0f);
            const Vector3 forward(-sinf(yawRad), 0.0f, -cosf(yawRad));

            // 在相机前方 5 单位处、相机高度上方 2 单位生成
            spawnPos   = tf.position + forward * 5.0f;
            spawnPos.y = tf.position.y + 2.0f;

            // 确保 cube 生成在地板上方（floor 上表面 y ≈ -5.5，保底 -2.0）
            constexpr float MIN_SPAWN_Y = -2.0f;
            spawnPos.y = std::max(spawnPos.y, MIN_SPAWN_Y);
        }
    }

    EntityID entity_cube = PrefabFactory::CreatePhysicsCube(
        registry, state.cubeMeshHandle, state.spawnIndex, spawnPos);
    ++state.spawnIndex;

    state.cubeEntities.push_back(entity_cube);
    LOG_INFO("[Sys_ImGui] Spawned cube entity id=" << entity_cube
             << " (total=" << state.cubeEntities.size() << ")");
}

// ============================================================
// DeleteLastCube
// ============================================================

void Sys_ImGui::DeleteLastCube(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.cubeEntities.empty()) return;

    EntityID last = state.cubeEntities.back();
    state.cubeEntities.pop_back();

    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed cube entity id=" << last);
    }
}

// ============================================================
// SpawnCapsule（通过 PrefabFactory 生成动态胶囊）
// ============================================================
/**
 * @brief 在相机正前方生成一个物理胶囊实体，并记录到 Res_TestState。
 *
 * @details
 * 生成位置基于相机 yaw/pitch 计算前向量，取相机前方 5 单位处，clamp 至 MIN_SPAWN_Y=-2.0f。
 * 依据 Res_TestState::capsuleOverlapSpawn 决定偏移策略：
 *   - true：围绕基准点做 OVERLAP_RADIUS=0.05f 的八向微扰动，便于观察胶囊挤压/分离。
 *   - false：按 GRID_COLS=4 列的网格步长 GRID_STEP=1.0f 分散生成，避免同点爆冲。
 */
void Sys_ImGui::SpawnCapsule(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.capsuleMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnCapsule: capsule mesh handle is INVALID, skipping.");
        return;
    }

    using namespace NCL::Maths;
    Vector3 spawnPos(0.0f, 8.0f, 0.0f);  // 默认兜底位置

    if (registry.has_ctx<Res_CameraContext>()) {
        auto& camCtx = registry.ctx<Res_CameraContext>();
        if (registry.Valid(camCtx.active_camera)
            && registry.Has<C_D_Transform>(camCtx.active_camera)
            && registry.Has<C_D_Camera>(camCtx.active_camera))
        {
            auto& tf  = registry.Get<C_D_Transform>(camCtx.active_camera);
            auto& cam = registry.Get<C_D_Camera>(camCtx.active_camera);
            const float yawRad   = cam.yaw   * (3.14159265f / 180.0f);
            const float pitchRad = cam.pitch * (3.14159265f / 180.0f);
            const Vector3 forward(
                -sinf(yawRad) * cosf(pitchRad),
                 sinf(pitchRad),
                -cosf(yawRad) * cosf(pitchRad)
            );
            spawnPos = tf.position + forward * 5.0f;
            constexpr float MIN_SPAWN_Y = -2.0f;
            spawnPos.y = std::max(spawnPos.y, MIN_SPAWN_Y);
        }
    }

    const int idx = state.capsuleSpawnIndex;
    if (state.capsuleOverlapSpawn) {
        // 近重叠模式：围绕同一点做微小扰动，便于观察胶囊间挤压/分离
        constexpr float OVERLAP_RADIUS = 0.05f;
        constexpr float PI = 3.14159265f;
        const float angle = (idx % 8) * (2.0f * PI / 8.0f);
        spawnPos.x += cosf(angle) * OVERLAP_RADIUS;
        spawnPos.z += sinf(angle) * OVERLAP_RADIUS;
    } else {
        // 分散生成：按索引做网格偏移，降低同点重叠导致的爆冲
        constexpr float GRID_STEP = 1.0f;
        constexpr int   GRID_COLS = 4;
        const int gx = idx % GRID_COLS;
        const int gz = idx / GRID_COLS;
        spawnPos.x += (gx - (GRID_COLS / 2)) * GRID_STEP;
        spawnPos.z += gz * GRID_STEP;
    }

    EntityID entity_capsule = PrefabFactory::CreatePhysicsCapsule(
        registry, state.capsuleMeshHandle, state.capsuleSpawnIndex, spawnPos);
    ++state.capsuleSpawnIndex;
    state.capsuleEntities.push_back(entity_capsule);

    LOG_INFO("[Sys_ImGui] Spawned capsule entity id=" << entity_capsule
             << " (total=" << state.capsuleEntities.size() << ")");
}

// ============================================================
// DeleteLastCapsule
// ============================================================
/**
 * @brief 销毁 Res_TestState::capsuleEntities 中最后一个存活的胶囊实体。
 */
void Sys_ImGui::DeleteLastCapsule(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.capsuleEntities.empty()) return;

    EntityID last = state.capsuleEntities.back();
    state.capsuleEntities.pop_back();

    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed capsule entity id=" << last);
    }
}

// ============================================================
// CleanupTestEntities（清除实体列表中已失效的 EntityID）
// ============================================================
/**
 * @brief 从 Res_TestState 的 cube/capsule 实体列表中移除已无效的 EntityID。
 *
 * @details
 * 不在渲染路径（每帧）调用，仅在批量操作（如 SetGravityAll）触发时做防御性清理，
 * 以防外部途径（物理系统析构、场景切换）悄无声息地销毁实体后留下悬空 ID。
 *
 * @param registry  当前 ECS Registry
 * @param state     当前场景的 Res_TestState 引用（就地修改列表）
 */
void Sys_ImGui::CleanupTestEntities(Registry& registry, Res_TestState& state) {
    state.cubeEntities.erase(
        std::remove_if(state.cubeEntities.begin(), state.cubeEntities.end(),
            [&](ECS::EntityID id) { return !registry.Valid(id); }),
        state.cubeEntities.end());
    state.capsuleEntities.erase(
        std::remove_if(state.capsuleEntities.begin(), state.capsuleEntities.end(),
            [&](ECS::EntityID id) { return !registry.Valid(id); }),
        state.capsuleEntities.end());
}

// ============================================================
// SetGravityAll
// ============================================================
/**
 * @brief 批量设置所有测试实体（cube + capsule）的 gravity_factor。
 *
 * @details
 * - 操作前调用 CleanupTestEntities() 做防御性清理，移除已失效的 EntityID，
 *   防止遍历过程中操作悬空实体。
 * - 同时遍历 cubeEntities 和 capsuleEntities，对持有 C_D_RigidBody 的实体
 *   写入新的 gravity_factor 值。
 * - 仅在用户点击 Enable/Disable Gravity 按钮时触发，非每帧路径。
 *
 * @param registry  当前 ECS Registry
 * @param factor    重力系数（1.0f = 正常重力，0.0f = 禁用）
 */
void Sys_ImGui::SetGravityAll(Registry& registry, float factor) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    CleanupTestEntities(registry, state);

    int affected = 0;

    for (EntityID id : state.cubeEntities) {
        if (registry.Valid(id) && registry.Has<C_D_RigidBody>(id)) {
            registry.Get<C_D_RigidBody>(id).gravity_factor = factor;
            ++affected;
        }
    }

    for (EntityID id : state.capsuleEntities) {
        if (registry.Valid(id) && registry.Has<C_D_RigidBody>(id)) {
            registry.Get<C_D_RigidBody>(id).gravity_factor = factor;
            ++affected;
        }
    }

    LOG_INFO("[Sys_ImGui] gravity_factor=" << factor
             << " applied to " << affected << " test rigidbodies.");
}

} // namespace ECS
#endif
