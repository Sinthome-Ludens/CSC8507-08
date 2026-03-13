/**
 * @file Sys_ImGuiEntityDebug.cpp
 * @brief 全量实体调试窗口系统实现。
 *
 * @details
 * 通过独立 ImGui 系统渲染所有存活实体的列表与详情，避免将实体监控逻辑继续耦合在测试物体控制窗口中。
 */
#include "Sys_ImGuiEntityDebug.h"
#ifdef USE_IMGUI

#include <imgui.h>

#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Utils/Log.h"

#include <cstdio>
#include <vector>

namespace ECS {

/**
 * @brief 收集当前注册表中所有活跃实体。
 * @param registry 当前场景注册表
 * @return 所有活跃实体 ID 的列表
 */
static std::vector<EntityID> CollectActiveEntities(const Registry& registry) {
    std::vector<EntityID> entities;
    entities.reserve(registry.EntityCount());
    registry.ForEachActiveEntity([&](EntityID entity) {
        entities.push_back(entity);
    });
    return entities;
}

/**
 * @brief 记录实体调试系统已启动。
 * @param registry 当前场景注册表
 */
void Sys_ImGuiEntityDebug::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiEntityDebug] OnAwake");
}

/**
 * @brief 记录实体调试系统已销毁。
 * @param registry 当前场景注册表
 */
void Sys_ImGuiEntityDebug::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiEntityDebug] OnDestroy");
}

/**
 * @brief 按 UI 标志决定是否渲染实体调试窗口。
 * @details 当开发者模式相关窗口开启时，渲染所有活跃实体的统一调试视图。
 * @param registry 当前场景注册表
 * @param dt 本帧时间步长（当前实现未直接使用）
 */
void Sys_ImGuiEntityDebug::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIFlags>()) {
        return;
    }

    auto& flags = registry.ctx<Res_UIFlags>();
    if (!flags.showEntityDebug) {
        return;
    }

    RenderEntityDebugWindow(registry);
}

/**
 * @brief 渲染实体调试窗口主体。
 * @details 使用双栏布局呈现实体列表与详情；若窗口被关闭，同步回写 UI 标志。
 * @param registry 当前场景注册表
 */
void Sys_ImGuiEntityDebug::RenderEntityDebugWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();
    if (!ImGui::Begin("Entity Debug Info", &flags.showEntityDebug, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const std::vector<EntityID> entities = CollectActiveEntities(registry);

    ImGui::Text("Active Entities: %d", static_cast<int>(entities.size()));
    ImGui::Separator();

    ImGui::Columns(2, "entity_debug_columns", true);
    RenderEntityList(registry);
    ImGui::NextColumn();
    RenderEntityDetails(registry);
    ImGui::Columns(1);

    ImGui::End();
}

/**
 * @brief 渲染实体列表栏。
 * @details 收集并列出所有活跃实体，为每项附加常见组件缩写标签，便于快速定位目标实体。
 * @param registry 当前场景注册表
 */
void Sys_ImGuiEntityDebug::RenderEntityList(Registry& registry) {
    const std::vector<EntityID> entities = CollectActiveEntities(registry);

    ImGui::TextUnformatted("Entities");
    ImGui::Separator();
    if (!ImGui::BeginChild("entity_list_child", ImVec2(0.0f, 0.0f), true)) {
        ImGui::EndChild();
        return;
    }

    for (EntityID entity : entities) {
        char label[96];
        const char* tf  = registry.Has<C_D_Transform>(entity) ? "T" : "-";
        const char* rb  = registry.Has<C_D_RigidBody>(entity) ? "R" : "-";
        const char* col = registry.Has<C_D_Collider>(entity) ? "C" : "-";
        const char* mr  = registry.Has<C_D_MeshRenderer>(entity) ? "M" : "-";
        const char* net = registry.Has<C_D_NetworkIdentity>(entity) ? "N" : "-";
        snprintf(label, sizeof(label), "%u  [%s%s%s%s%s]", entity, tf, rb, col, mr, net);

        if (ImGui::Selectable(label, m_SelectedEntity == entity)) {
            m_SelectedEntity = entity;
        }
    }

    if (entities.empty()) {
        ImGui::TextDisabled("No active entities.");
    }

    ImGui::EndChild();
}

/**
 * @brief 渲染选中实体的详情栏。
 * @details 展示实体是否有效，以及 Transform、RigidBody、Collider、MeshRenderer、NetworkIdentity、Camera、Input 等常见组件快照。
 * @param registry 当前场景注册表
 */
void Sys_ImGuiEntityDebug::RenderEntityDetails(Registry& registry) {
    ImGui::TextUnformatted("Details");
    ImGui::Separator();
    if (!ImGui::BeginChild("entity_detail_child", ImVec2(0.0f, 0.0f), true)) {
        ImGui::EndChild();
        return;
    }

    if (!Entity::IsValid(m_SelectedEntity)) {
        ImGui::TextDisabled("Select an entity from the list.");
        ImGui::EndChild();
        return;
    }

    if (!registry.Valid(m_SelectedEntity)) {
        ImGui::TextDisabled("Selected entity is no longer valid.");
        m_SelectedEntity = Entity::NULL_ENTITY;
        ImGui::EndChild();
        return;
    }

    ImGui::Text("Entity ID: %u", m_SelectedEntity);
    ImGui::Text("Index: %u", Entity::GetIndex(m_SelectedEntity));
    ImGui::Text("Version: %u", Entity::GetVersion(m_SelectedEntity));
    ImGui::Spacing();

    if (registry.Has<C_D_Transform>(m_SelectedEntity)) {
        const auto& tf = registry.Get<C_D_Transform>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: %.2f, %.2f, %.2f", tf.position.x, tf.position.y, tf.position.z);
            ImGui::Text("Rotation: %.2f, %.2f, %.2f, %.2f", tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w);
            ImGui::Text("Scale: %.2f, %.2f, %.2f", tf.scale.x, tf.scale.y, tf.scale.z);
        }
    }

    if (registry.Has<C_D_RigidBody>(m_SelectedEntity)) {
        const auto& rb = registry.Get<C_D_RigidBody>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_RigidBody", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Mass: %.2f", rb.mass);
            ImGui::Text("Gravity Factor: %.2f", rb.gravity_factor);
            ImGui::Text("Body Created: %s", rb.body_created ? "true" : "false");
            ImGui::Text("Static: %s", rb.is_static ? "true" : "false");
            ImGui::Text("Kinematic: %s", rb.is_kinematic ? "true" : "false");
        }
    }

    if (registry.Has<C_D_Collider>(m_SelectedEntity)) {
        const auto& col = registry.Get<C_D_Collider>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Type: %d", static_cast<int>(col.type));
            ImGui::Text("Half Extents: %.2f, %.2f, %.2f", col.half_x, col.half_y, col.half_z);
            ImGui::Text("Trigger: %s", col.is_trigger ? "true" : "false");
        }
    }

    if (registry.Has<C_D_MeshRenderer>(m_SelectedEntity)) {
        const auto& mr = registry.Get<C_D_MeshRenderer>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_MeshRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Mesh Handle: %u", mr.meshHandle);
            ImGui::Text("Material Handle: %u", mr.materialHandle);
        }
    }

    if (registry.Has<C_D_NetworkIdentity>(m_SelectedEntity)) {
        const auto& net = registry.Get<C_D_NetworkIdentity>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_NetworkIdentity", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("NetID: %u", net.netID);
            ImGui::Text("Owner Client ID: %u", net.ownerClientID);
        }
    }

    if (registry.Has<C_D_Camera>(m_SelectedEntity)) {
        const auto& cam = registry.Get<C_D_Camera>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Pitch: %.2f", cam.pitch);
            ImGui::Text("Yaw: %.2f", cam.yaw);
        }
    }

    if (registry.Has<C_D_Input>(m_SelectedEntity)) {
        const auto& input = registry.Get<C_D_Input>(m_SelectedEntity);
        if (ImGui::CollapsingHeader("C_D_Input", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Move: %.2f, %.2f", input.moveX, input.moveZ);
            ImGui::Text("Has Input: %s", input.hasInput ? "true" : "false");
            ImGui::Text("Shift: %s", input.shiftDown ? "true" : "false");
        }
    }

    ImGui::EndChild();
}

} // namespace ECS
#endif
