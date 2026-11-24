---
title: "Cohesion, Coupling, and Separation of Concerns in NestJS"
description: "Architecting high cohesion, loose coupling, and evaluating module instability metrics in enterprise NestJS"
tags: [nestjs, clean-code, cohesion, coupling, metrics, separation-of-concerns]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Cohesion, Coupling, and Separation of Concerns in NestJS

## Conceptual Overview
- **Cohesion**: The degree to which elements inside a module belong together. High cohesion is desirable.
- **Coupling**: The degree of direct knowledge that one module has of another. Loose coupling is desirable.
- **Instability ($I$)**:
  $$I = rac{C_e}{C_a + C_e}$$
  where $C_e$ is Efferent Coupling (outgoing dependencies) and $C_a$ is Afferent Coupling (incoming dependencies).
  $I = 0$ represents maximally stable modules (e.g. core domain models); $I = 1$ represents flexible, volatile modules.

---

## Key Takeaways & Cheat Sheet

- Keep NestJS modules highly cohesive: all providers in a module should serve the same bounded context.
- Keep coupling loose by communicating across modules via interfaces or events.
- Core domain modules should be stable ($I pprox 0$).

---

## Cross-References
- [02-modular-monolith.md](../01-architecture/02-modular-monolith.md) - Modular monolith
- [02-solid-principles.md](./02-solid-principles.md) - SOLID
