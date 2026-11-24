# Module 03: Dependency Injection

Welcome to **Module 03: Dependency Injection**. This module explores the core architectural feature of FastAPI: its composable, hierarchical, and testable dependency injection engine.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-depends-basics.md](./01-depends-basics.md) | **Depends Basics** | Injecting functions, callables, and services. |
| [02-dependency-caching-and-use_cache.md](./02-dependency-caching-and-use_cache.md) | **Caching & use_cache** | Per-request memoization, cache scope, and disabling caching. |
| [03-classes-and-callables-as-dependencies.md](./03-classes-and-callables-as-dependencies.md) | **Classes as Dependencies** | Using classes directly as dependencies with `__call__`. |
| [04-sub-dependencies.md](./04-sub-dependencies.md) | **Sub-Dependencies** | Building hierarchical, nested dependency graphs. |
| [05-dependencies-with-yield.md](./05-dependencies-with-yield.md) | **Dependencies with Yield** | Setup and teardown lifecycle, managing database sessions. |
| [06-dependencies-in-decorators-and-routers.md](./06-dependencies-in-decorators-and-routers.md) | **Router-Level Dependencies** | Applying security and validation guards to entire routers. |
| [07-global-dependencies.md](./07-global-dependencies.md) | **Global Dependencies** | Application-wide dependencies for logging, rate limiting, and auth. |
| [08-overriding-dependencies-in-tests.md](./08-overriding-dependencies-in-tests.md) | **Dependency Overrides** | `app.dependency_overrides` for clean mock testing. |
| [09-request-scoped-state-and-context.md](./09-request-scoped-state-and-context.md) | **Request-Scoped State** | Contextvars, request state, and multi-tenant tenant context. |
| [10-dependency-injection-vs-service-layer.md](./10-dependency-injection-vs-service-layer.md) | **DI vs Service Layer** | Trade-offs between pure DI and traditional service layers. |
| [11-dependency-pitfalls.md](./11-dependency-pitfalls.md) | **Dependency Pitfalls** | Circular dependencies, unclosed yield fixtures, and performance gotchas. |
