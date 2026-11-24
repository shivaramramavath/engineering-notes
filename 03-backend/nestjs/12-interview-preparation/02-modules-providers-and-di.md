---
title: "NestJS Interview: Modules, Providers, and Dependency Injection"
description: "In-depth questions on the NestJS IoC container, custom providers, dynamic modules, and circular dependencies"
tags: [nestjs, interview, dependency-injection, modules, providers]
---

# NestJS Interview: Modules, Providers, and Dependency Injection

### Q1: What is the difference between `useClass`, `useValue`, `useFactory`, and `useExisting`?
**Answer**:
- `useClass`: Maps an injection token to a concrete class to be instantiated by the IoC container.
- `useValue`: Injects a static, already-instantiated value or mock object (ideal for testing).
- `useFactory`: Instantiates a provider dynamically via a factory function that can accept injected dependencies and execute asynchronous initialization.
- `useExisting`: Creates an alias for an existing registered provider.

### Q2: How do you resolve a circular dependency between two NestJS services?
**Answer**: Use `forwardRef(() => ServiceName)` inside the constructor `@Inject()` decorator on both services. This wraps the class reference in a closure, deferring resolution until the container dependency graph is fully scanned.
