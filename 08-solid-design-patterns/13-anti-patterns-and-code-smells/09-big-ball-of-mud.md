# Big Ball of Mud

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (System Architecture) |
| **Origin** | Brian Foote & Joseph Yoder (1997) |
| **Fix** | [Modular Monolith](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/03-modular-monolith.md), [Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) |

> **TL;DR**: A **Big Ball of Mud** is a haphazardly structured system with no enforced module boundaries—every package imports every other package, circular dependencies abound, and touching one line breaks three unrelated features.

## Escape Plan (Never Big-Bang Rewrite!)
1. Add **Characterization Tests** around critical flows.
2. Draw vertical module boundaries and enforce **no new cross-module internal imports** via CI Fitness Functions (`ArchUnit` / `dependency-cruiser` / `import-linter`).
3. Strangle out one module at a time.
