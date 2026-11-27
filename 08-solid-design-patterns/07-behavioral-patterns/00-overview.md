# Behavioral Design Patterns Overview

> **One-line summary:** Behavioral patterns assign dynamic communication and control-flow responsibilities—decoupling *who requests* an action from *who executes* it, *when* it runs, and *how* state evolves.

| Attribute | Specification |
|---|---|
| **Level** | Core / GoF Behavioral |
| **Prerequisites** | [Module 02: SOLID](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/README.md), [Module 06: Structural Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/README.md) |
| **Estimated Reading Time** | 15 minutes |
| **Status** | Complete |
| **Category** | Behavioral Patterns |

---

## 1. Taxonomy of Behavioral Patterns

```mermaid
graph TD
    Behav[Behavioral Patterns] --> Alg[Algorithms & Rules]
    Behav --> StateOp[State & Operations]
    Behav --> Comm[Communication & Traversal]

    Alg --> Strat[01 Strategy]
    Alg --> Tpl[05 Template Method]
    Alg --> Spec[12 Specification]
    Alg --> Vis[10 Visitor / 11 Interpreter]

    StateOp --> Cmd[03 Command]
    StateOp --> St[04 State]
    StateOp --> Mem[09 Memento]

    Comm --> Obs[02 Observer / 13 Event Bus / 14 Pub-Sub]
    Comm --> CoR[06 Chain of Responsibility]
    Comm --> Iter[07 Iterator]
    Comm --> Med[08 Mediator]
```

---

[← Module 07 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/README.md) | [Next: 01 - Strategy Pattern →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/01-strategy.md)
