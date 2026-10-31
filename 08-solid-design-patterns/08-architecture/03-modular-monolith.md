# Modular Monolith

A modular monolith is an application that is **deployed as one unit** but **internally divided into independent modules** with clear, enforced boundaries. It aims for the main benefit people seek from microservices, separation and independent evolution, without the operational cost of a distributed system.

## Three shapes

```text
Big ball of mud            Modular monolith               Microservices
(one deployable,           (one deployable,               (many deployables,
 no boundaries)             strong boundaries)             network boundaries)

 ┌───────────┐              ┌────────┬────────┐            [A]──►[B]
 │ everything│              │ Orders │ Billing│            │      │
 │ calls     │              ├────────┼────────┤            ▼      ▼
 │ everything│              │Catalog │ Users  │            [C]    [D]
 └───────────┘              └────────┴────────┘
```

| | Big ball of mud | Modular monolith | Microservices |
|---|---|---|---|
| Deployment | One | One | Many |
| Boundaries | None | Enforced in code | Enforced by the network |
| Calls between parts | Anywhere | Through module APIs (in-process) | Over the network |
| Data | One shared schema, free access | Each module owns its data | Each service owns its database |
| Operational complexity | Low | Low | High |
| Team independence | Poor | Good | Best |
| Refactoring across boundaries | Chaotic | Easy (same codebase, compiler help) | Hard (API versioning, deployments) |

## What makes a module

A module is a cohesive slice of the business (Orders, Billing, Catalog) with:

1. **A small public API** that other modules may use.
2. **Hidden internals** that nothing outside can touch.
3. **Its own data**, which no other module reads or writes directly.
4. **A clear owner** (a team or person).

Modules should follow business capabilities, not technical layers. The boundaries often match DDD [bounded contexts](./04-domain-driven-design.md).

## Structure in Java

```text
com.shop
├── orders
│   ├── OrderApi            ← public: the module's interface
│   ├── OrderPlaced         ← public: event other modules may listen to
│   └── internal            ← everything else (package-private or in a hidden package)
│       ├── OrderService
│       ├── Order
│       └── OrderRepository
├── billing
│   ├── BillingApi
│   └── internal ...
└── catalog
    ├── CatalogApi
    └── internal ...
```

```java
// orders module: public API
public interface OrderApi {
    OrderId place(PlaceOrderCommand command);
    OrderSummary find(OrderId id);
}

// orders module: implementation is hidden
class OrderService implements OrderApi { ... }       // package-private
```

Another module uses only the API:

```java
class BillingListener {
    private final OrderApi orders;
    BillingListener(OrderApi orders) { this.orders = orders; }
}
```

## Enforcing boundaries

Conventions alone fail over time, so enforce them mechanically.

| Mechanism | How |
|---|---|
| **Package-private visibility** | Keep internals non-public so other packages can't see them |
| **Separate build modules** (Maven/Gradle) | Compiler prevents using classes that aren't on the dependency path |
| **Java Platform Module System (JPMS)** | `module-info.java` exports only public packages |
| **Architecture tests** (for example ArchUnit) | Fail the build when module A imports module B's internals |
| **Framework support** | Spring Modulith, for example, verifies module structure and supports module-level events and testing |

```java
@ArchTest
static final ArchRule orders_internals_are_private =
        noClasses().that().resideOutsideOfPackage("com.shop.orders..")
                   .should().dependOnClassesThat().resideInAPackage("com.shop.orders.internal..");
```

## How modules communicate

| Style | Use for | Trade-off |
|---|---|---|
| **Direct call through the module's API** | Queries and simple commands needing an immediate answer | Compile-time coupling to the API; keep it small |
| **Events** ([Observer](../06-behavioral-patterns/02-observer.md) / [event-driven](./05-event-driven.md)) | Reacting to something that happened (order placed → billing starts) | Decoupled, but flow is less visible |

Avoid a module reaching into another module's classes, entities or repository, even though it's technically in the same JVM.

## Data ownership

The most important boundary is the data.

```text
✘ Billing reads the `orders` table directly           (hidden coupling to Orders' schema)
✔ Billing calls OrderApi, or consumes OrderPlaced     (coupled to a stable contract)
```

Each module owns its tables (a separate schema or table prefix). Cross-module queries go through APIs, or through dedicated read models. No foreign keys across module boundaries if you want to keep extraction possible.

## The path to microservices

A well-built modular monolith makes extraction cheap, if you ever need it.

```text
1. Boundaries enforced, data separated      (modular monolith)
2. Replace in-process calls with network calls for ONE module
3. Move its data to its own database
4. Deploy it separately
```

If module APIs and events were clean, extraction is largely mechanical. If modules were entangled, extraction is as hard as untangling a monolith, which is why boundaries matter more than deployment.

## When to use it

- A growing codebase where a single team or a few teams share one repository.
- You want separation and clear ownership without running many services.
- The domain is still being discovered, so boundaries may move. Moving a boundary inside one codebase is far cheaper than between services.
- You don't yet have the operational maturity for microservices (tracing, deployment pipelines, service discovery).

Consider microservices when different parts need independent scaling or release cadence, or many teams are blocked by a shared deployable.

## Common mistakes

- **Modules in name only**: packages exist, but code freely crosses them.
- **Shared database tables** accessed by several modules, which creates the hidden coupling modules are meant to avoid.
- **A shared "common" module** that everything depends on and that grows into a ball of mud. Share only genuinely generic code.
- **Splitting by technical layer** (a "services" module, a "repositories" module) instead of by business capability.
- **Modules that are too fine-grained**, creating chatty APIs and constant cross-module calls.
- **Modules that are too coarse**, so boundaries mean little.
- **Circular dependencies** between modules (Orders ↔ Billing). Break them with events or by moving shared concepts.
- **Skipping enforcement**, so boundaries decay after a few months.
- **Premature microservices** to "get modularity", paying the distribution tax too early.

## Quick Summary

- A modular monolith is one deployable with strongly bounded, independently evolvable modules.
- Boundaries are drawn around business capabilities, expose small APIs, hide internals and own their data.
- Enforce boundaries with visibility, build modules or architecture tests.
- It's a strong default, and a good stepping stone if you later extract services.

## Related

[Layered](./01-layered.md) · [Domain-driven design](./04-domain-driven-design.md) (bounded contexts) · [Event-driven](./05-event-driven.md) · [Facade](../05-structural-patterns/05-facade.md) (a module API acts like one)

## Next

[04-domain-driven-design.md](./04-domain-driven-design.md)
