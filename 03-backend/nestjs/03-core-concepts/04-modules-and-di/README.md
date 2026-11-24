# Modules and Dependency Injection

Modules decide **what exists and who can see it**. Dependency injection decides **how pieces get wired together**. Most "weird" Nest errors (`Nest can't resolve dependencies of...`, circular imports, services with duplicated state) come from not having a clear mental model of these two systems.

```text
        ┌──────────────┐  imports   ┌──────────────┐
        │  OrdersModule│ ─────────► │ PaymentsModule│
        │              │            │               │
        │ OrdersService│            │ providers:    │
        │ (needs       │            │   PaymentsSvc │
        │  PaymentsSvc)│            │   Internal    │   ← not exported: private
        └──────────────┘            │ exports:      │
                                    │   PaymentsSvc │   ← visible to importers
                                    └──────────────┘
```

Rule of thumb: a provider is injectable somewhere if it is **in that module's `providers`** or **exported by a module that is imported**. Everything else is invisible.

> Applies to NestJS 10/11. This folder continues in notes 07-09 (scopes, `ModuleRef`, lifecycle).

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Feature and shared modules](./01-feature-and-shared-modules.md) | How to split an app into modules; exports; single-instance behavior |
| 02 | [Global modules](./02-global-modules.md) | `@Global()`, when it helps, when it hurts |
| 03 | [Dynamic modules](./03-dynamic-modules.md) | `forRoot` / `register` / `forFeature`, async options, `ConfigurableModuleBuilder` |
| 04 | [Circular dependencies](./04-circular-dependencies.md) | Why they happen, `forwardRef`, and how to design them away |
| 05 | [Custom providers](./05-custom-providers.md) | `useClass`, `useValue`, `useFactory`, `useExisting` |
| 06 | [Injection tokens and optional dependencies](./06-injection-tokens-and-optional-dependencies.md) | Injecting interfaces, symbols, `@Optional()` |
| 07 | [Scopes and request context](./07-scopes-and-request-context.md) | Singleton vs request vs transient providers |
| 08 | [ModuleRef and lazy loading](./08-module-ref-and-lazy-loading.md) | Resolving providers dynamically |
| 09 | [Application lifecycle](./09-application-lifecycle.md) | Init/shutdown hooks |

## Prerequisites

- [Modules](../../02-fundamentals/02-modules.md)
- [Providers and services](../../02-fundamentals/04-providers-and-services.md)
- [Dependency injection](../../02-fundamentals/05-dependency-injection.md)

## Related

- [DI internals](../../06-internals/02-dependency-injection-internals.md)
- [Modular monolith](../../08-architecture-and-patterns/01-architecture/02-modular-monolith.md)
- [Configuration](../03-configuration/README.md) (uses dynamic and global modules heavily)
