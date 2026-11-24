---
title: "NestJS Core Best Practices and Conventions"
description: "Industry-standard best practices for module design, DTO validations, controller layering, and dependency management in NestJS"
tags: [nestjs, best-practices, architecture, conventions, modularity]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# NestJS Core Best Practices and Conventions

## Conceptual Overview
Building maintainable enterprise software requires collective adherence to clear architectural conventions. This guide establishes canonical best practices across the NestJS ecosystem.

---

## 1. Modularity and Encapsulation
- **One Domain, One Module**: Group related controllers, services, entities, and repositories by feature.
- **Export Only What Is Needed**: Keep internal helper services, repositories, and entities private within the module.
- **Never Export Everything**: Avoid dumping all providers into `exports: [...]`.

## 2. Controllers and DTOs
- **Keep Controllers Paper-Thin**: Controllers must only receive requests, trigger use cases, and return responses. No business logic, no SQL queries.
- **Strict DTO Validation**: Every mutating endpoint must consume a strongly-typed DTO validated with `ValidationPipe({ whitelist: true, forbidNonWhitelisted: true })`.
- **Use Response DTOs**: Never return raw database entities (which leak passwords, internal IDs, or timestamps); map to explicit output DTOs.

## 3. Dependency Injection
- **Favor Singletons**: Default to `Scope.DEFAULT`. Avoid `Scope.REQUEST` unless multi-tenancy strictly demands it, to prevent V8 garbage collection penalties.
- **Use Symbols or String Tokens for Interfaces**: Inject interfaces using `@Inject('INTERFACE_TOKEN')`.

---

## Key Takeaways & Cheat Sheet

- Keep controllers thin; isolate domain logic in services.
- Never return raw database entities directly to HTTP clients.
- Default to `Scope.DEFAULT` (Singleton) for all providers.
