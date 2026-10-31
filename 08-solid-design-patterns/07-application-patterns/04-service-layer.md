# Service Layer

**Intent:** define an application's boundary with a layer of services that establishes the set of available operations (use cases) and coordinates the application's response in each operation.

The service layer is where a request like "place an order" becomes a sequence of steps: load objects, apply business rules, save changes, trigger side effects. Controllers, message listeners and scheduled jobs all go through it, so every entry point behaves consistently.

## The problem

Without a service layer, use-case logic lands in whichever class received the request:

```java
@PostMapping("/orders")
public ResponseEntity<?> place(@RequestBody OrderRequest req) {
    // validate, load customer, check stock, calculate price, save, send email ...
    // 80 lines of orchestration inside the controller
}
```

Consequences:

- The logic can't be reused by a batch job, message consumer or another controller.
- Testing needs the web layer.
- Transaction handling is scattered.

## Structure

```text
Controller / Listener / Job       (entry points; translate protocols)
        │   DTOs
        ▼
   Service Layer                  (use cases, transactions, authorization checks)
        │
        ├──► Domain objects       (business rules)
        └──► Repositories         (persistence)
```

## What belongs in a service

| Responsibility | Example |
|---|---|
| Define use cases | `placeOrder`, `cancelOrder`, `registerCustomer` |
| Orchestrate | Load, call domain behavior, save, publish events |
| Transaction boundary | One use case = one [unit of work](./02-unit-of-work.md) |
| Authorization and input validation at the application level | "Can this user cancel this order?" |
| Coordination across aggregates and external systems | Reserve stock, then charge payment |
| Translate between DTOs and domain objects | Return `OrderSummary`, not the entity |

What does **not** belong: HTTP details (status codes, headers), SQL, and business rules that naturally belong to a domain object.

## Example

```java
public class OrderService {
    private final OrderRepository orders;
    private final CustomerRepository customers;
    private final StockService stock;
    private final PaymentGateway payments;
    private final EventPublisher events;

    // dependencies injected; see dependency injection
    public OrderService(OrderRepository orders, CustomerRepository customers,
                        StockService stock, PaymentGateway payments, EventPublisher events) {
        this.orders = orders;
        this.customers = customers;
        this.stock = stock;
        this.payments = payments;
        this.events = events;
    }

    // @Transactional (framework-managed boundary)
    public OrderSummary placeOrder(PlaceOrderCommand cmd) {
        Customer customer = customers.findById(cmd.customerId())
                .orElseThrow(() -> new CustomerNotFound(cmd.customerId()));

        Order order = customer.placeOrder(cmd.items());     // business rules live in the domain
        stock.reserve(order);                               // collaborator
        orders.save(order);

        events.publish(new OrderPlaced(order.id()));        // ideally dispatched after commit
        return OrderSummary.from(order);                    // DTO out
    }
}
```

The service reads like the use case: find the customer, place the order, reserve stock, save, announce, return a summary. The *rules* (is the customer allowed to order? what is the total?) are inside `Customer` and `Order`.

## Thin vs fat: the central design choice

This is the **domain model vs transaction script** question.

### Transaction Script

Each use case is a procedure containing all the logic, working with simple data objects.

```java
public void placeOrder(PlaceOrderCommand cmd) {
    double total = 0;
    for (Item i : cmd.items()) total += i.price() * i.quantity();
    if (customer.balance() < total) throw new InsufficientFunds();
    // ... update rows, send email, all in one method
}
```

| Pros | Cons |
|---|---|
| Simple, direct, easy to follow for small cases | Logic duplicates across scripts as the app grows |
| Little structure to learn | Rules are scattered; harder to reuse and test in isolation |
| Fits CRUD and reporting well | Gets messy with complex, interrelated rules |

### Domain Model with a thin service layer

Objects carry both data and behavior. The service coordinates; the domain decides.

| Pros | Cons |
|---|---|
| Rules live next to the data they govern | More upfront design |
| Complex logic is reusable and testable without infrastructure | Overkill for simple applications |
| Scales with business complexity | Needs team understanding of the domain |

### The anemic domain model smell

```java
class Order { /* only getters and setters */ }
class OrderService {                        // ALL logic here
    void addItem(Order o, Item i) { ... }
    double total(Order o) { ... }
    void cancel(Order o) { ... }
}
```

This looks like objects but is really transaction script in disguise. Warning signs: entities with no behavior, services full of `if` statements about entity state, and rules duplicated across services. If the application is complex, move behavior into the domain objects.

### Rule of thumb

| Situation | Approach |
|---|---|
| Simple CRUD, few rules | Transaction scripts or a thin service are fine |
| Real business rules that interact and change | Domain model + thin service layer |
| Mixed | Use both: scripts for plain CRUD areas, domain model where rules are rich |

## Service layer and facade

A service layer can look like a [Facade](../05-structural-patterns/05-facade.md): a simple entry point over domain and infrastructure details. The difference is purpose: a facade primarily *simplifies*; a service layer *defines use cases and transaction boundaries* for the application.

## Design guidelines

- **Name methods after use cases** (`cancelOrder`), not CRUD (`updateOrder`).
- **Organize by feature or aggregate** (`OrderService`, `BillingService`), not one giant `ApplicationService`.
- **Take and return DTOs or commands**, not entities ([DTO](./05-data-transfer-object.md)).
- **Keep it free of framework types** (`HttpServletRequest`).
- **One public method per use case**, with a clear transaction boundary.
- **Depend on repository and gateway interfaces** so it's testable ([DIP](../02-solid-principles/05-dependency-inversion.md)).
- **Do slow external calls outside the transaction** where possible.

## Testing

```java
@Test
void placeOrderSavesOrderAndPublishesEvent() {
    var orders = new InMemoryOrderRepository();
    var events = new RecordingEventPublisher();
    var service = new OrderService(orders, customers, stock, payments, events);

    OrderSummary summary = service.placeOrder(new PlaceOrderCommand("c1", items));

    assertTrue(orders.findById(summary.orderId()).isPresent());
    assertEquals(1, events.published().size());
}
```

No web server or database is needed.

## Common mistakes

- **God services** with dozens of methods covering unrelated use cases.
- **Anemic domain**: all rules in services, entities as data bags.
- **Services calling other services in cycles** (A → B → A). Extract shared logic.
- **Business logic in controllers** or, the reverse, HTTP concerns in services.
- **Transaction scope too wide** (including remote calls) or too narrow (one per repository call).
- **Returning entities** to controllers, which then trigger lazy loading or leak internals.
- **Sending emails or events before the transaction commits**, so they announce changes that later roll back.
- **One service per entity by default** with generic CRUD wrappers that add nothing.

## Quick Summary

- The service layer defines the application's use cases and their transaction boundaries.
- Keep services thin: orchestrate, and delegate business rules to the domain.
- Transaction script suits simple CRUD; a domain model suits rich rules.
- Watch for anemic domain models and god services.

## Related

[Repository](./01-repository.md) · [Unit of Work](./02-unit-of-work.md) · [Data Transfer Object](./05-data-transfer-object.md) · [Facade](../05-structural-patterns/05-facade.md) · [Layered architecture](../08-architecture/01-layered.md)

## Next

[05-data-transfer-object.md](./05-data-transfer-object.md)
