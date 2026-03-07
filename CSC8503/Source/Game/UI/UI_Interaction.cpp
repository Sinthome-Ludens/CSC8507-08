#include "UI_Interaction.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Vector.h"
#include "Matrix.h"
#include "Camera.h"
#include "GameWorld.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Interactable.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"

using namespace NCL;
using namespace NCL::Maths;

namespace ECS::UI {

// ============================================================
// RenderInteractionPrompts — World-space floating labels
// ============================================================

static constexpr int   kMaxPrompts    = 8;
static constexpr float kMaxRenderDist = 20.0f;

// Type -> color mapping
static ImU32 GetTypeColor(InteractableType type, uint8_t alpha) {
    switch (type) {
        case InteractableType::Pickup:   return IM_COL32(80, 200, 120, alpha);   // green
        case InteractableType::Door:     return IM_COL32(252, 111, 41, alpha);   // orange
        case InteractableType::Terminal: return IM_COL32(252, 111, 41, alpha);   // orange
        case InteractableType::NPC:      return IM_COL32(180, 120, 230, alpha);  // purple
        default:                         return IM_COL32(200, 200, 200, alpha);  // gray
    }
}

// Type -> action text
static const char* GetActionText(InteractableType type) {
    switch (type) {
        case InteractableType::Pickup:   return "[E] PICK UP";
        case InteractableType::Door:     return "[E] OPEN";
        case InteractableType::Terminal: return "[E] USE";
        case InteractableType::NPC:      return "[E] TALK";
        default:                         return "[E] INTERACT";
    }
}

void RenderInteractionPrompts(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_NCL_Pointers>()) return;

    auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
    if (!nclPtrs.world) return;

    // Get camera matrices
    PerspectiveCamera& cam = nclPtrs.world->GetMainCamera();
    Matrix4 viewMat = cam.BuildViewMatrix();
    float aspect = ImGui::GetIO().DisplaySize.x / ImGui::GetIO().DisplaySize.y;
    Matrix4 projMat = cam.BuildProjectionMatrix(aspect);
    Matrix4 vpMat = projMat * viewMat;

    Vector3 camPos = cam.GetPosition();
    float gameW = ImGui::GetIO().DisplaySize.x - Res_ChatState::PANEL_WIDTH;
    float displayH = ImGui::GetIO().DisplaySize.y;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* smallFont = UITheme::GetFont_Small();

    // Collect nearby interactables
    struct PromptData {
        float screenX, screenY;
        float dist;
        InteractableType type;
        const char* label;
        uint8_t alpha;
    };
    PromptData prompts[kMaxPrompts];
    int promptCount = 0;

    auto view = registry.view<C_D_Transform, C_D_Interactable>();
    view.each([&](EntityID /*eid*/, C_D_Transform& tf, C_D_Interactable& inter) {
        if (!inter.isEnabled) return;
        if (promptCount >= kMaxPrompts) return;

        // Distance check
        Vector3 diff = tf.position - camPos;
        float dist = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (dist > kMaxRenderDist) return;

        // World -> clip space
        Vector4 worldPos(tf.position.x, tf.position.y + 1.5f, tf.position.z, 1.0f);  // offset up
        Vector4 clipPos = vpMat * worldPos;

        // Behind camera check
        if (clipPos.w <= 0.001f) return;

        // NDC
        float ndcX = clipPos.x / clipPos.w;
        float ndcY = clipPos.y / clipPos.w;

        // Frustum check
        if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) return;

        // Screen coordinates (within game area)
        float screenX = (ndcX * 0.5f + 0.5f) * gameW;
        float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * displayH;

        // Distance-based alpha (fade from 255 at 0m to 80 at maxDist)
        float t = std::clamp(dist / kMaxRenderDist, 0.0f, 1.0f);
        uint8_t alpha = (uint8_t)(255.0f - t * 175.0f);

        prompts[promptCount++] = { screenX, screenY, dist, inter.type, inter.label, alpha };
    });

    // Sort by distance (closest first)
    for (int i = 0; i < promptCount - 1; ++i) {
        for (int j = i + 1; j < promptCount; ++j) {
            if (prompts[j].dist < prompts[i].dist) {
                PromptData tmp = prompts[i];
                prompts[i] = prompts[j];
                prompts[j] = tmp;
            }
        }
    }

    // Render floating labels
    if (smallFont) ImGui::PushFont(smallFont);

    for (int i = 0; i < promptCount; ++i) {
        auto& p = prompts[i];
        const char* actionText = GetActionText(p.type);
        ImU32 typeColor = GetTypeColor(p.type, p.alpha);

        // Label dimensions
        ImVec2 textSize = ImGui::CalcTextSize(actionText);
        float labelW = textSize.x + 20.0f;
        float labelH = textSize.y + 8.0f;
        float labelX = p.screenX - labelW * 0.5f;
        float labelY = p.screenY - labelH * 0.5f;

        // Clamp to screen
        labelX = std::clamp(labelX, 4.0f, gameW - labelW - 4.0f);
        labelY = std::clamp(labelY, 4.0f, displayH - labelH - 4.0f);

        // Dark background
        draw->AddRectFilled(
            ImVec2(labelX, labelY),
            ImVec2(labelX + labelW, labelY + labelH),
            IM_COL32(16, 13, 10, (uint8_t)(p.alpha * 0.7f)), 3.0f);

        // Left type-color bar
        draw->AddRectFilled(
            ImVec2(labelX, labelY),
            ImVec2(labelX + 3.0f, labelY + labelH),
            typeColor, 3.0f);

        // Text
        draw->AddText(
            ImVec2(labelX + 10.0f, labelY + 4.0f),
            IM_COL32(245, 238, 232, p.alpha),
            actionText);
    }

    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
