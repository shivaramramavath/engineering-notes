# Copy-Paste Programming

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern / Code Smell |
| **Related** | [DRY & Rule of Three](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/01-dry-and-rule-of-three.md) |
| **Fix** | Extract Shared Helper / Template **after 3rd duplication** |

> **TL;DR**: Blindly copying and tweaking blocks of business logic creates multiple diverging copies—so fixing a bug in one place leaves the same bug alive in 4 other files.

## Nuance: Rule of Three
- **2 copies** of similar-looking code in *different* bounded contexts that will evolve differently? **Leave them separate** (duplication is cheaper than the wrong abstraction!).
- **3+ copies** of the *same* business rule/algorithm? Extract a well-named shared helper.
