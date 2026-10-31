# Clean, Hexagonal and Onion Architecture

Three named architectures, one central idea: **business logic sits at the center and depends on nothing external. Frameworks, databases, UIs and other systems depend on the business logic, never the reverse.**

This is [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md) applied to the whole application, and it addresses the main weakness of [classic layered architecture](./01-layered.md), where the domain ends up depending on the database.

## The shared idea

```text
Classic layered:                     Dependency-inverted:

Presentation                          Web / CLI / Queue ─┐
     │                                                    ▼
Business ──► Data access ──► DB                   ┌──────────────┐
                                                  │   Business   │  ← knows nothing about the outside
                                                  └──────────────┘
                                                    ▲           ▲
                                      Database adapter     Email adapter
```

The source-code dependency arrows point **inward**, toward the business rules.

## The three names

| Name | Origin | Vocabulary | Emphasis |
|---|---|---|---|
| **Hexagonal** (Ports and Adapters) | Alistair Cockburn | Ports (interfaces), adapters (implementations) | Symmetry between inbound and outbound connections; many ways in, many ways out |
| **Onion** | Jeffrey Palermo | Concentric rings: domain model, domain services, application services, infrastructure | Domain model at the very center; layering by ring |
| **Clean** | Robert C. Martin | Entities, use cases, interface adapters, frameworks & drivers | The Dependency Rule, with use cases as a distinct ring |

They differ in diagrams and terminology, not in the essential rule. You can treat them as one architecture, and pick the vocabulary your team likes. The "ports and adapters" terms are the most practical.

## Core concepts (ports and adapters)

```text
              ┌────────────────────────────────────────┐
 HTTP ──►     │                                        │     ──► Database
 Adapter      │   Inbound port ► Application / Domain  │ Outbound port ► Adapter
 CLI  ──►     │   (use cases)         (rules)          │     ──► Email adapter
 Messages ──► │                                        │     ──► Payment API adapter
              └────────────────────────────────────────┘
              driving side            core            driven side
```

| Term | Meaning |
|---|---|
| **Core (domain + application)** | Business rules and use cases; pure Java, no framework |
| **Inbound (driving) port** | Interface the outside uses to *call* the core: a use case |
| **Outbound (driven) port** | Interface the core uses to *call out*: persistence, email, payments |
| **Inbound adapter** | Translates an external input (HTTP request) into a port call |
| **Outbound adapter** | Implements an outbound port using a technology (JPA, SMTP) |

The Dependency Rule: **adapters depend on ports, ports belong to the core**. The core never imports an adapter, a framework type or a database class.

## Example

```java
// ---------- Core ----------
// Inbound port (use case)
public interface PlaceOrderUseCase {
    OrderId place(PlaceOrderCommand command);
}

// Outbound ports (owned by the core, shaped by its needs)
public interface OrderStore {
    void save(Order order);
}
public interface PaymentPort {
    PaymentResult charge(CustomerId customer, Money amount);
}

// Use case implementation: pure business orchestration
public class PlaceOrderService implements PlaceOrderUseCase {
    private final OrderStore orders;
    private final PaymentPort payments;

    public PlaceOrderService(OrderStore orders, PaymentPort payments) {
        this.orders = orders;
        this.payments = payments;
    }

    public OrderId place(PlaceOrderCommand cmd) {
        Order order = Order.create(cmd.customerId(), cmd.items());
        PaymentResult result = payments.charge(cmd.customerId(), order.total());
        if (!result.successful()) throw new PaymentDeclined();
        order.markPaid();
        orders.save(order);
        return order.id();
    }
}

// ---------- Adapters ----------
// Inbound adapter (web)
@RestController
class OrderController {
    private final PlaceOrderUseCase placeOrder;
    OrderController(PlaceOrderUseCase placeOrder) { this.placeOrder = placeOrder; }

    @PostMapping("/orders")
    ResponseEntity<String> place(@RequestBody PlaceOrderRequest req) {
        OrderId id = placeOrder.place(req.toCommand());
        return ResponseEntity.status(201).body(id.value());
    }
}

// Outbound adapter (persistence)
class JpaOrderStore implements OrderStore {
    private final OrderJpaRepository jpa;
    private final OrderMapper mapper;
    // maps the domain Order to a JPA entity and saves it
    public void save(Order order) { jpa.save(mapper.toEntity(order)); }
}

// Outbound adapter (payment provider)
class StripePaymentAdapter implements PaymentPort {
    public PaymentResult charge(CustomerId c, Money amount) { /* call Stripe's API, translate result */ }
}
```

The use case has no idea whether it's driven by HTTP, a CLI or a message queue, or whether orders are saved in PostgreSQL or memory.

## Suggested package layout

```text
com.shop.orders
├── domain               Order, OrderId, Money (pure business model)
├── application
│   ├── port.in          PlaceOrderUseCase, PlaceOrderCommand
│   ├── port.out         OrderStore, PaymentPort
│   └── PlaceOrderService
└── adapter
    ├── in.web           OrderController, PlaceOrderRequest
    └── out
        ├── persistence  JpaOrderStore, OrderEntity, OrderMapper
        └── payment      StripePaymentAdapter
```

The rule to verify: `domain` and `application` import nothing from `adapter`, nor from framework packages.

## Testing benefits

```java
@Test
void declinedPaymentDoesNotSaveOrder() {
    var orders = new InMemoryOrderStore();
    PaymentPort declining = (customer, amount) -> PaymentResult.declined();
    var service = new PlaceOrderService(orders, declining);

    assertThrows(PaymentDeclined.class, () -> service.place(command));
    assertTrue(orders.isEmpty());
}
```

Core logic is tested with plain objects and in-memory fakes: fast, no containers, no database. Adapters are tested separately (an integration test for `JpaOrderStore` against a real or containerized database).

## Layered vs hexagonal

| | Layered | Hexagonal / clean |
|---|---|---|
| Dependency direction | Downward, ending at data | Inward toward the domain |
| Domain depends on persistence? | Often yes | **No** |
| Swapping database/framework | Hard | Contained to adapters |
| Test without infrastructure | Needs mocks of lower layers | Natural: use ports with fakes |
| Cost | Low | More interfaces and mapping |

## Costs and trade-offs

| Cost | Detail |
|---|---|
| **More types** | Ports, adapters, commands, mappers |
| **Mapping overhead** | Domain ↔ persistence entity ↔ DTO conversions |
| **Learning curve** | The team must follow the dependency rule consistently |
| **Overkill for simple CRUD** | Pure pass-through use cases add ceremony without benefit |

A pragmatic compromise: allow annotations such as persistence mappings on domain classes in a simple project, accepting some framework coupling for fewer mappers, and keep strict separation where the domain is rich.

## When to use it

- Business logic is complex and long-lived.
- You expect to change technologies (database, messaging, UI).
- Fast, infrastructure-free tests are valuable.
- The system has several entry points (web, CLI, queue consumers).

Skip or relax it for small CRUD services, prototypes and scripts.

## Common mistakes

- **Framework types in the core** (`@Entity`, Spring annotations or `HttpServletRequest` in domain classes) when you intended strict separation.
- **Ports shaped by the adapter**, for example an outbound port that exposes JPA concepts.
- **Pass-through use cases** that only call a repository, adding layers without logic.
- **Mapping everything everywhere**: create separate models only where they truly differ.
- **Letting adapters call each other directly**, bypassing the core.
- **One giant port interface** per module. Keep ports small ([ISP](../02-solid-principles/04-interface-segregation.md)).
- **Treating folder structure as the architecture.** Verify dependencies with tests.
- **Domain events or exceptions** that leak infrastructure details.

## Enforcing it

Add an architecture test that fails the build when the core imports an adapter or framework package:

```java
@ArchTest
static final ArchRule core_does_not_depend_on_adapters =
        noClasses().that().resideInAnyPackage("..domain..", "..application..")
                   .should().dependOnClassesThat().resideInAPackage("..adapter..");
```

## Quick Summary

- Clean, hexagonal and onion architecture share one rule: dependencies point inward to the business core.
- Ports are interfaces owned by the core; adapters implement them with specific technologies.
- Benefits: framework independence, fast tests, replaceable infrastructure.
- Costs: more types and mapping; not worth it for simple CRUD.

## Related

[Layered](./01-layered.md) · [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md) · [Adapter pattern](../05-structural-patterns/01-adapter.md) · [Domain-driven design](./04-domain-driven-design.md) · [Test doubles](../09-testing-and-debugging/02-test-doubles.md)

## Next

[03-modular-monolith.md](./03-modular-monolith.md)
