# Event-Driven Architecture

In an event-driven architecture, parts of a system communicate by **publishing events** (facts about things that happened) and **reacting to them**, instead of calling each other directly. The publisher doesn't know who is listening or what they'll do.

It's the [Observer pattern](../06-behavioral-patterns/02-observer.md) scaled up to modules, services and organizations.

## Direct calls vs events

```text
Direct (orchestration):
OrderService ──► BillingService
             ──► InventoryService
             ──► EmailService        OrderService knows and depends on all three

Events (choreography):
OrderService ──publishes──► OrderPlaced
                                │
          ┌─────────────────────┼──────────────────┐
          ▼                     ▼                  ▼
      Billing               Inventory            Email         each reacts on its own
```

Adding a new reaction (analytics, fraud check) means adding a subscriber. The publisher doesn't change ([Open/Closed](../02-solid-principles/02-open-closed.md)).

## Key concepts

### Event vs command vs query

| | Meaning | Naming | Receivers |
|---|---|---|---|
| **Event** | Something **already happened** (fact) | Past tense: `OrderPlaced` | Zero to many; sender doesn't care |
| **Command** | A **request to do** something | Imperative: `PlaceOrder` | Exactly one handler |
| **Query** | A request for **data** | `GetOrder` | One responder |

Events are immutable facts. A publisher can't refuse or cancel an event, because it already occurred. If something must be undone, publish a new event (`OrderCancelled`).

### Producer, broker, consumer

```text
Producer ──► Broker / Event bus ──► Consumer(s)
                (Kafka, RabbitMQ, cloud queues, or an in-process bus)
```

| Where | Mechanism |
|---|---|
| **In one process** | An in-memory event publisher (for example Spring's application events), a simple listener list |
| **Across processes or services** | A message broker such as Apache Kafka or RabbitMQ, or cloud queues and topics |

Start in-process within a [modular monolith](./03-modular-monolith.md). Move to a broker only when you need cross-process delivery.

## An in-process example

```java
// Event: immutable fact
public record OrderPlaced(OrderId orderId, CustomerId customerId, Money total, Instant occurredAt) {}

// Publisher
public class PlaceOrderService {
    private final OrderRepository orders;
    private final EventPublisher events;

    public PlaceOrderService(OrderRepository orders, EventPublisher events) {
        this.orders = orders;
        this.events = events;
    }

    public OrderId place(PlaceOrderCommand cmd) {
        Order order = Order.create(cmd.customerId(), cmd.items());
        orders.save(order);
        events.publish(new OrderPlaced(order.id(), cmd.customerId(), order.total(), Instant.now()));
        return order.id();
    }
}

// Subscribers: separate modules, unknown to the publisher
public class BillingEventHandler {
    public void on(OrderPlaced event) { /* create invoice */ }
}
public class InventoryEventHandler {
    public void on(OrderPlaced event) { /* reserve stock */ }
}
```

## Common event styles

| Style | What the event carries | Use for |
|---|---|---|
| **Event notification** | Minimal: an ID and "something changed" | Light signals; consumers query for details (coupling back to the source) |
| **Event-carried state transfer** | The data consumers need | Consumers keep their own copy, so they don't call back |
| **Event sourcing** | Events are the **source of truth**; state is derived by replaying them | Full audit history, temporal queries; adds significant complexity |
| **CQRS** (often combined) | Separate models for writing (commands) and reading (queries), with events keeping the read model updated | Very different read and write needs |

Event sourcing and CQRS are powerful but heavy. Adopt them only for specific reasons, not as defaults.

## Consistency: the cost of decoupling

Once a use case spans several handlers, you lose a single atomic transaction.

```text
OrderPlaced  →  Billing creates invoice      (now)
             →  Inventory reserves stock     (a moment later; may fail)
```

You get **eventual consistency**: the system becomes consistent after events are processed, not instantly. Design for it:

- Make failure paths explicit (compensating events such as `StockReservationFailed → OrderCancelled`).
- Show users honest intermediate states ("processing").
- Coordinate long workflows with a **saga** (a sequence of local transactions with compensations).

## Reliable delivery

Three problems appear as soon as a broker is involved.

### 1. The dual-write problem

```java
orders.save(order);            // database commit succeeds
broker.send(orderPlaced);      // crashes here → event never sent, state and events disagree
```

You can't atomically write to a database and a broker. The standard fix is the **transactional outbox**:

```text
In ONE database transaction:
   1. save the order
   2. insert the event into an `outbox` table

A separate relay process reads the outbox and publishes to the broker,
marking rows as sent.
```

State and event are committed together, and the relay publishes at least once.

### 2. At-least-once delivery means duplicates

Most brokers guarantee delivery *at least once*, so a consumer may receive the same event twice (after a retry or crash). Consumers must be **idempotent**.

```java
public void on(OrderPlaced event) {
    if (processedEvents.alreadyHandled(event.eventId())) return;   // dedupe by event ID
    createInvoice(event);
    processedEvents.markHandled(event.eventId());                  // ideally in the same transaction
}
```

Include a unique event ID in every event.

### 3. Ordering

Order is typically guaranteed only within a partition or key (for example all events for one `orderId`), not globally. Don't assume global ordering. Design handlers to tolerate out-of-order arrival, or key events so related ones stay ordered.

## Event design

| Guideline | Why |
|---|---|
| Name events in the **past tense, in business language** | Reflects the [ubiquitous language](./04-domain-driven-design.md) |
| Include **event ID, timestamp, and a version** | Deduplication, auditing, evolution |
| Keep payloads **small but sufficient** | Avoid callbacks, and avoid leaking internals |
| Make events **immutable** | They are facts |
| Treat them as **public contracts** | Changing them can break unknown consumers |
| Evolve **compatibly**: add optional fields, don't remove or rename | Old consumers keep working |

Don't publish your internal entity structure as the event schema. Publish a deliberate, stable contract.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Loose coupling between producer and consumers | Control flow is **hard to follow** ("what happens when X?") |
| Easy to add new reactions | **Eventual consistency** |
| Natural fit for async work and spikes (buffering) | Need idempotency, retries, dead-letter handling |
| Services can scale and fail independently | Harder debugging and testing |
| Audit trail of what happened | Operational overhead of a broker |
| | Event schema becomes a long-lived contract |

## Debugging and observability

| Symptom | Likely cause / action |
|---|---|
| "Nothing happened after the event" | Consumer down, subscription missing, event stuck in the outbox or dead-letter queue |
| Duplicate side effects | Consumer isn't idempotent |
| Out-of-order effects | No ordering key, or assumed global order |
| Can't trace a flow | Missing **correlation ID** propagated across events and logs |
| Queue growing | Slow or failing consumer; check lag |
| Poison message retried forever | Add a retry limit and a **dead-letter queue** |

Always propagate a **correlation ID**, log event IDs, and monitor consumer lag and dead-letter counts. Document the main event flows, since no single class shows the whole picture.

## When to use it

- Several parts must react to the same occurrence, and new reactions are added often.
- Work can be asynchronous (emails, analytics, search indexing, notifications).
- You want modules or services decoupled in time and in knowledge.
- You integrate with many external consumers.

Avoid it for simple request/response flows where the caller needs an immediate result, or where strict transactional consistency is essential. A plain method call is clearer.

## Common mistakes

- **Events used as hidden remote calls**: publishing an event and *waiting* for a specific response, which is just a request in disguise.
- **Commands named as events** (`SendEmail` published as a fact).
- **Giant "god" events** carrying everything, tightly coupling consumers to the producer's model.
- **Skipping idempotency**, then double-charging customers on a retry.
- **Dual writes** without an outbox.
- **No dead-letter handling**, so a bad message blocks or silently disappears.
- **Chaining events into spaghetti**: A triggers B triggers C triggers A. Map the flows and keep chains short.
- **Assuming ordering**, or immediate consistency.
- **Changing event schemas** without versioning.
- **Adopting event sourcing or a broker** "because it's modern" for a simple application.

## Quick Summary

- Event-driven systems communicate through immutable facts, decoupling publishers from reactions.
- Start in-process in a modular monolith; add a broker when you must cross processes.
- Plan for eventual consistency, at-least-once delivery (idempotent consumers), ordering limits and the outbox pattern.
- Events are public contracts: name them in business language and evolve them compatibly.

## Related

[Observer](../06-behavioral-patterns/02-observer.md) · [Mediator](../06-behavioral-patterns/08-mediator.md) · [Modular monolith](./03-modular-monolith.md) · [Domain-driven design](./04-domain-driven-design.md) (domain events)

## Next

Testing and debugging: [../09-testing-and-debugging/00-overview.md](../09-testing-and-debugging/00-overview.md)
