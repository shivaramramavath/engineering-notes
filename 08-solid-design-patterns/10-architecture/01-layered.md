# Layered Architecture

Layered architecture divides an application into **horizontal layers**, each with one broad responsibility, where a layer may depend only on the layer(s) beneath it. It's the most widely used style, and the baseline that clean/hexagonal architecture later refines.

## The classic layers

```text
┌───────────────────────────────┐
│ Presentation                  │  controllers, UI, API endpoints
├───────────────────────────────┤
│ Application / Service         │  use cases, transactions, orchestration
├───────────────────────────────┤
│ Domain / Business             │  entities, business rules
├───────────────────────────────┤
│ Data access / Infrastructure  │  repositories, ORM, external clients
└───────────────────────────────┘
        dependencies point downward
```

| Layer | Responsibility | Should not contain |
|---|---|---|
| Presentation | Handle requests, translate to and from DTOs, return responses | Business rules, SQL |
| Application | Define use cases, coordinate, manage transactions | HTTP details, SQL |
| Domain | Business concepts and rules | Framework, UI or database code |
| Data access | Persist and fetch, talk to external systems | Business decisions |

Three-tier applications often collapse this to **presentation / business / data**. See [service layer](../07-application-patterns/04-service-layer.md) and [repository](../07-application-patterns/01-repository.md) for what lives where.

## Rules of layering

1. **Dependencies go one way** (downward). A lower layer never references a higher one.
2. **Each layer has a clear job**, and code goes in the layer that matches it.
3. **Cross-layer calls go through public interfaces**, not around them.

## Strict vs relaxed layering

| | Strict | Relaxed |
|---|---|---|
| Rule | A layer calls only the layer directly beneath it | A layer may call any layer below it |
| Benefit | Maximum isolation; easy to swap a layer | Less pass-through code |
| Cost | Boilerplate: delegating methods that just forward calls | Layers become coupled to several below them |

Most applications use relaxed layering with some discipline (for example, controllers shouldn't touch repositories directly).

## Package structure in Java

Two ways to slice the code:

### Package by layer

```text
com.shop
├── controller    OrderController, CustomerController
├── service       OrderService, CustomerService
├── repository    OrderRepository, CustomerRepository
└── model         Order, Customer
```

Simple and familiar, but a single feature is scattered across four packages, and everything must be public so layers can see each other.

### Package by feature

```text
com.shop
├── orders        OrderController, OrderService, OrderRepository, Order
└── customers     CustomerController, CustomerService, CustomerRepository, Customer
```

All code for a feature sits together, so most classes can stay package-private, and each feature can later become a module (see [modular monolith](./03-modular-monolith.md)). Layers still exist *inside* each feature.

Package by feature scales better as the codebase grows.

## Example flow

```java
@RestController
class OrderController {
    private final OrderService service;
    OrderController(OrderService service) { this.service = service; }

    @PostMapping("/orders")
    OrderResponse place(@RequestBody PlaceOrderRequest req) {
        return service.place(req.toCommand());           // presentation → application
    }
}

class OrderService {
    private final OrderRepository repository;
    OrderService(OrderRepository repository) { this.repository = repository; }

    OrderResponse place(PlaceOrderCommand cmd) {
        Order order = Order.create(cmd.customerId(), cmd.items());   // domain rules
        repository.save(order);                                      // application → data
        return OrderResponse.from(order);
    }
}
```

Each layer talks to the one below and returns plain results upward.

## Benefits

- Familiar to almost every developer.
- Clear place for each kind of code.
- Straightforward to start with, and well supported by frameworks.
- Layers can be tested separately (mock the layer below).

## Weaknesses

| Problem | Why it happens |
|---|---|
| **Domain depends on the database layer** | In the classic form, business logic sits *above* data access, so it ends up depending on persistence details. This is exactly what [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md) fixes in clean/hexagonal architecture |
| **Anemic domain** | Business logic drifts into the service layer; entities become data bags |
| **Database-centric design** | Everything is modeled around tables first |
| **Architecture sinkhole** | A request passes through every layer with no processing, only forwarding |
| **Leaky layers** | Entities exposed to the UI, or SQL concepts in services |
| **Horizontal change scatter** | One feature change touches every layer |
| **Hard to extract parts** | By-layer packaging couples features together |

## Common mistakes

- **Skipping layers** (controllers calling repositories) "just this once".
- **Using entities as API models**, tying the API to the database. Use [DTOs](../07-application-patterns/05-data-transfer-object.md).
- **Business rules in controllers or repositories.**
- **Circular dependencies** between layers or services.
- **A "common" or "util" layer** that everything depends on, becoming a dumping ground.
- **Over-layering** a trivial application with five layers and pass-through methods.

## Enforcing the rules

Layer rules erode silently, so check them automatically. Architecture tests (for example with ArchUnit) can assert:

```java
@ArchTest
static final ArchRule controllers_do_not_use_repositories =
        noClasses().that().resideInAPackage("..controller..")
                   .should().dependOnClassesThat().resideInAPackage("..repository..");
```

## When to use it

- Small to medium applications with straightforward logic.
- Teams new to architecture, who benefit from a well-known structure.
- CRUD-oriented systems where the domain is simple.

When the domain grows rich or you need independence from frameworks and databases, move to the [clean/hexagonal](./02-clean-hexagonal-onion.md) variation.

## Quick Summary

- Layered architecture separates presentation, application, domain and data access, with one-way downward dependencies.
- Prefer packaging by feature, with layers inside.
- Its weakness: the domain tends to depend on persistence and drift into anemic logic.
- Enforce layer rules with architecture tests.

## Related

[Clean / Hexagonal / Onion](./02-clean-hexagonal-onion.md) · [Service Layer](../07-application-patterns/04-service-layer.md) · [Repository](../07-application-patterns/01-repository.md) · [Modular monolith](./03-modular-monolith.md)

## Next

[02-clean-hexagonal-onion.md](./02-clean-hexagonal-onion.md)
