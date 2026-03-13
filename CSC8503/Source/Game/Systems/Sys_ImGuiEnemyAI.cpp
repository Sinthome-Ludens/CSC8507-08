#include "Sys_ImGuiEnemyAI.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_ImGuiEnemyAI::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiEnemyAI] OnAwake");
}

void Sys_ImGuiEnemyAI::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiEnemyAI] OnDestroy");
}

void Sys_ImGuiEnemyAI::OnUpdate(Registry& registry, float /*dt*/) {
    if (m_ShowWindow) RenderEnemyMonitorWindow(registry);
}

// ============================================================
// RenderEnemyMonitorWindow — 通用敌人状态监控表格
// ============================================================

void Sys_ImGuiEnemyAI::RenderEnemyMonitorWindow(Registry& registry) {
    if (!ImGui::Begin("Enemy Monitoring Station", &m_ShowWindow)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("EnemyTable", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0.0f, 200.0f)))
    {
        ImGui::TableSetupColumn("Entity ID");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("AI State");
        ImGui::TableSetupColumn("Detection");
        ImGui::TableHeadersRow();

        auto view = registry.view<C_T_Enemy, C_D_Transform, C_D_AIState, C_D_AIPerception>();
        view.each([&](EntityID id, C_T_Enemy&, C_D_Transform& tf,
                      C_D_AIState& aiState, C_D_AIPerception& det)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", static_cast<unsigned>(id));

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f, %.1f, %.1f",
                        tf.position.x, tf.position.y, tf.position.z);

            ImGui::TableSetColumnIndex(2);
            const char* stateStr =
                (aiState.current_state == EnemyState::Safe)    ? "SAFE"    :
                (aiState.current_state == EnemyState::Search)  ? "SEARCH"  :
                (aiState.current_state == EnemyState::Alert)   ? "ALERT"   : "HUNT";
            ImGui::TextUnformatted(stateStr);

            ImGui::TableSetColumnIndex(3);
            ImGui::ProgressBar(det.detection_value / 100.0f, ImVec2(-1.0f, 0.0f));
        });

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace ECS
#endif
