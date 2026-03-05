---
applyTo: "NCLCoreClasses/**,CSC8503CoreClasses/**,OpenGLRendering/**,VulkanRendering/**"
---

# Engine Read-Only Instructions

- These directories are treated as stable infrastructure/render backends.
- Default policy: do not modify files in these paths.
- Only change these files when the user explicitly requests it.
- If a change is required, keep it minimal and include in the PR description:
  - reason for touching engine/backend code,
  - impact scope,
  - rollback plan.
