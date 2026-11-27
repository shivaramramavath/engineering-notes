# Shotgun Surgery & Divergent Change

| Field | Value |
| :--- | :--- |
| **Category** | Code Smells (Change Coupling) |
| **Violates** | [Single Responsibility Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/01-single-responsibility.md) |
| **Fix** | Move Method / Inline Class (for Shotgun Surgery); Extract Class (for Divergent Change) |

> **TL;DR**: **Divergent Change** = 1 class gets edited for 5 unrelated reasons (*too few classes*). **Shotgun Surgery** = 1 simple change forces tiny edits across 12 scattered files (*responsibility split too thinly*).

| Smell | Symptom | Fix |
| :--- | :--- | :--- |
| **Divergent Change** | You open `OrderService` for UI changes, tax changes, *and* SQL changes | **Extract Class** (split along axes of change) |
| **Shotgun Surgery** | Adding 1 `currency` field requires editing 12 files across 4 folders | **Move Method / Inline Class** to colocate cohesive logic |
