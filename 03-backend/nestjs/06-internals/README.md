# Module 06: NestJS Internals

Welcome to the **NestJS Internals** curriculum module. This module provides a comprehensive, source-level exploration of the NestJS framework runtime, dependency injection engine, metadata reflection, request pipeline, and platform adapters.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-how-nest-boots.md](./01-how-nest-boots.md) | **How Nest Boots** | Bootstrap lifecycle, `NestFactory`, `DependenciesScanner`, `InstanceLoader`, and startup/shutdown hooks order in NestJS 11. |
| [02-dependency-injection-internals.md](./02-dependency-injection-internals.md) | **DI Internals** | The IoC container, DAG resolution, circular dependencies with `forwardRef()`, and provider scope bubble-up mechanics. |
| [03-metadata-and-reflection.md](./03-metadata-and-reflection.md) | **Metadata & Reflection** | `reflect-metadata` shim, TypeScript decorator metadata, `Reflector` service APIs, and custom decorator composition. |
| [04-request-lifecycle-internals.md](./04-request-lifecycle-internals.md) | **Request Lifecycle Internals** | Complete execution pipeline: Middleware, Guards, Pre-Interceptors, Pipes, Route Handlers, Post-Interceptors, and Filters. |
| [05-platform-adapters.md](./05-platform-adapters.md) | **Platform Adapters** | `AbstractHttpAdapter`, Express 5 vs Fastify 5 architecture, wildcard routing changes, and custom HTTP engines. |
| [06-custom-transporters.md](./06-custom-transporters.md) | **Custom Transporters** | Authoring custom microservice servers and client proxies extending `Server` and `ClientProxy`. |
| [07-custom-enhancers.md](./07-custom-enhancers.md) | **Custom Enhancers** | Dynamic metaprogramming and auto-discovery using `DiscoveryService` and `MetadataScanner`. |
| [08-source-code-walkthrough.md](./08-source-code-walkthrough.md) | **Source Code Walkthrough** | Architectural tour of the official `nestjs/nest` monorepo: `core`, `common`, and `microservices`. |

---

## Learning Objectives

1. **Understand Container Initialization**: Trace the exact sequence of events from `NestFactory.create()` to `app.listen()`.
2. **Master the DI Engine**: Debug circular dependencies, understand topological DAG resolution, and avoid the Request-scope bubble-up trap.
3. **Harness Reflection & Metadata**: Build advanced custom decorators and evaluate method/class metadata using `Reflector`.
4. **Master the Pipeline**: Predict the exact execution order of Middleware, Guards, Interceptors, Pipes, and Filters.
5. **Architect Platform & Transport Adapters**: Build custom HTTP and microservice transport adapters for proprietary infrastructure.
