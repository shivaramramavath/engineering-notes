# Architecture Cheat Sheet

- **Modular Monolith**: Strict `exports: [...]` boundaries.
- **Hexagonal**: Core domain has zero NestJS imports; ports are interfaces injected via `Symbol('PORT')`.
- **CQRS**: `CommandBus.execute(cmd)` for writes, `QueryBus.execute(query)` for reads.
