---
title: "Dependency Injection Best Practices in NestJS"
description: "Architecting clean IoC containers: custom providers, interface tokens, dynamic module builders, and testing overrides"
tags: [nestjs, best-practices, dependency-injection, ioc, custom-providers]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Dependency Injection Best Practices in NestJS

## Core Principles
1. **Program to Interfaces, Not Implementations**: Bind TypeScript interfaces using `Symbol('TOKEN')` so implementations can be swapped without touching business logic.
2. **Avoid Service Locator Pattern**: Do not pass `ModuleRef` into domain services to look up dependencies manually; declare dependencies explicitly in the constructor.
3. **Keep Providers Idempotent**: A provider constructor should never execute network requests or database mutations; use `onModuleInit()` for startup I/O.

---

## Cross-References
- [02-dependency-injection-internals.md](../../06-internals/02-dependency-injection-internals.md) - DI internals
