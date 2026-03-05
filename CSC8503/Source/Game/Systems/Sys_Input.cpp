#include "Sys_Input.h"
#include "Window.h"
#include "Core/Bridge/InputAdapter.h"
#include "Game/Components/Res_Input.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_Input::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_Input>()) {
        registry.ctx_emplace<Res_Input>();
    }
    LOG_INFO("[Sys_Input] OnAwake - Res_Input registered.");
}

void Sys_Input::OnUpdate(Registry& registry, float /*dt*/) {
    NCL::Window* window = NCL::Window::GetWindow();
    if (!window) return;

    auto& input = registry.ctx<Res_Input>();
    InputAdapter::Update(window, input);
}

} // namespace ECS