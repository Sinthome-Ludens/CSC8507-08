---
applyTo: "CSC8503/Source/**/*.h,CSC8503/Source/**/*.cpp"
---

# ECS Path Instructions

## Core principles
- Implement gameplay features in ECS (`Components`, `Systems`, `Events`, `Scenes`).
- Components are data-only where possible.
- Systems own behavior and orchestration.
- Use events for decoupled cross-system communication.

## Naming conventions (strict)
- Data components: `C_D_*`
- Tag components: `C_T_*`
- Global resources/context: `Res_*`
- Systems: `Sys_*`
- Events: `Evt_*`

## Layering constraints
- Keep `Core/Bridge` as an adapter between framework input/render/asset interfaces and ECS.
- Do not move gameplay state ownership into Bridge.
- Prefer extending ECS systems instead of adding ad-hoc logic to legacy OOP gameplay classes.

## Change style
- Keep edits minimal and coherent with existing ECS patterns.
- Preserve existing scene/system registration order unless the task requires a specific ordering change.
- Avoid introducing unrelated renames or broad architectural rewrites.
