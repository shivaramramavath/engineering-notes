# Module 10 Overview: Software Architecture Patterns

> *"Architecture is about the important stuff. Whatever that is."* — Ralph Johnson (things that are hard to change later).

## Architecture Spectrum

```mermaid
flowchart LR
    M1["01 Layered Monolith"] --> M2["02 Clean / Hexagonal"]
    M2 --> M3["03 Modular Monolith"]
    M3 --> M4["04 DDD Bounded Contexts"]
    M4 --> M5["05 Event-Driven / 07 CQRS"]
    M4 --> M6["06 Microservices"]
    M6 --> M11["11 Cell-Based"]
    GOV["12 ADRs + 13 Fitness Functions + 14 Evolutionary Arch"] -.-> M1
    GOV -.-> M6
```

## Key Principle: Architecture Follows Team & Domain Complexity

1. **Start Simple**: A **Modular Monolith** with **Clean/Hexagonal** boundaries inside critical modules beats premature microservices 95% of the time.
2. **Protect Boundaries Automatically**: Use **Fitness Functions** (`ArchUnit`, `dependency-cruiser`, `import-linter`) so modules don't rot into a Big Ball of Mud.
3. **Record the "Why"**: Write **ADRs** for every hard-to-reverse decision.
