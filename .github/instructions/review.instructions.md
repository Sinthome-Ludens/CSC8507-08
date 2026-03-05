---
applyTo: "**"
excludeAgent: "coding-agent"
---

# PR Review Instructions

## Review annotation format
- Use explicit tags in review comments:
  - `[BLOCKING]` must-fix issue before merge.
  - `[RISK]` non-trivial risk that should be addressed or justified.
  - `[STYLE]` non-blocking style/readability suggestion.
  - `[TEST]` missing or weak validation evidence.

## Doxygen compliance checks (required)
- For each changed `.h` and `.cpp` file in the PR:
  - File header must contain exactly one unique Doxygen file block.
  - In `.h` files, all methods must be documented with purpose-level Doxygen comments.
  - In `.cpp` files, all methods must be documented with implementation-oriented Doxygen comments.

## Commenting policy
- Default rule: keep main code body with zero inline comments.
- Allowed and recommended exception: add short Doxygen-style block comments above code that is hard to understand from naming alone, including:
  - complex math algorithms (for example quaternion interpolation, collision rebound vector math, path smoothing),
  - bitwise or memory-sensitive operations,
  - critical rule switching, network protocol handling, or state machine transitions.
- For regular business logic, inline comments are still discouraged.

## Scope of review
- Enforce these rules on files changed by the PR.
- Mark missing Doxygen requirements as `[BLOCKING]`.
