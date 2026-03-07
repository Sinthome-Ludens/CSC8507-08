# CSC8507-08 Copilot Repository Instructions

## Project positioning
- This repository is a C++20 game-tech coursework project built with CMake on Windows x64.
- Day-to-day feature work is ECS-first under `CSC8503/Source/**`.
- Treat legacy engine modules as infrastructure and rendering backend support.

## Change boundaries (high priority)
- Default editable area: `CSC8503/Source/**`.
- Do not modify engine/backend directories unless the user explicitly asks:
  - `NCLCoreClasses/**`
  - `CSC8503CoreClasses/**`
  - `OpenGLRendering/**`
  - `VulkanRendering/**`
- If cross-layer changes are unavoidable, keep them minimal and explain why.

## Build workflow (Windows)
- Use out-of-source build directories with explicit `-S` and `-B`.
- Preferred configure pattern:
  - `cmake -DCMAKE_BUILD_TYPE=Debug -S <repo-root> -B <repo-root>/cmake-build-debug-visual-studio`
- Preferred build pattern:
  - `cmake --build <repo-root>/cmake-build-debug-visual-studio --config Debug`
- Respect repository options when needed:
  - `-DUSE_OPENGL=ON/OFF`
  - `-DUSE_VULKAN=ON/OFF`
  - `-DUSE_IMGUI=ON/OFF`

## Architecture and implementation preferences
- ECS conventions are strict in `CSC8503/Source/**`:
  - Data components: `C_D_*`
  - Tag components: `C_T_*`
  - Global resources/context: `Res_*`
  - Systems: `Sys_*`
  - Events: `Evt_*`
- Keep gameplay logic in ECS systems and data in components.
- Keep Bridge layer (`Core/Bridge`) as adapter code, not gameplay ownership.

## Validation expectations
- Validate at least configure + build for the touched area and active backend.
- Prefer narrow, task-scoped checks; avoid unrelated large refactors.
- If a change cannot be validated locally, state what was not run and why.

## Documentation and review expectations
- Follow repository review instructions under `.github/instructions/review.instructions.md`.
- Pull requests should enforce Doxygen requirements defined in review instructions.
