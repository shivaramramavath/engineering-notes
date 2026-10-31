# Observer

**Intent:** define a one-to-many dependency so that when one object (the *subject*) changes state or raises an event, all its dependents (*observers*) are notified automatically.

The subject doesn't know who its observers are or what they do. It only knows they implement an interface. This keeps the subject [loosely coupled](../01-foundations/03-coupling-and-cohesion.md) from the reactions to its events.

## The problem

When an order is placed, you need to send an email, update inventory, record analytics and notify a warehouse. If `OrderService` calls each directly:

```java
public void place(Order order) {
    repository.save(order);
    emailService.sendConfirmation(order);
    inventory.reserve(order);
    analytics.track(order);
    warehouse.notify(order);       // every new reaction edits this method
}
```

`OrderService` is coupled to everything that cares about orders, and every new reaction means editing it.

## Structure

```text
Subject                         Observer (interface)
  - observers: List               + onEvent(event)
  + subscribe(o)                        ▲
  + unsubscribe(o)              ┌───────┼────────┐
  + notifyAll(event) ─────────► Email  Inventory  Analytics
```

## Example

```java
public record OrderPlaced(String orderId, String customerEmail) {}

public interface OrderListener {
    void onOrderPlaced(OrderPlaced event);
}

public class OrderService {
    private final List<OrderListener> listeners = new CopyOnWriteArrayList<>();

    public void subscribe(OrderListener l)   { listeners.add(l); }
    public void unsubscribe(OrderListener l) { listeners.remove(l); }

    public void place(Order order) {
        // ... save the order
        var event = new OrderPlaced(order.id(), order.email());
        for (OrderListener l : listeners) {
            try {
                l.onOrderPlaced(event);
            } catch (RuntimeException e) {
                // one failing observer must not break the others
                log.warn("Listener failed", e);
            }
        }
    }
}

// Observers
class EmailListener implements OrderListener {
    public void onOrderPlaced(OrderPlaced e) { /* send email */ }
}
class AnalyticsListener implements OrderListener {
    public void onOrderPlaced(OrderPlaced e) { /* track */ }
}

// Wiring
orderService.subscribe(new EmailListener());
orderService.subscribe(new AnalyticsListener());
```

Adding a reaction means writing a listener and subscribing it. `OrderService` doesn't change.

A single-method listener can be a lambda:

```java
orderService.subscribe(event -> auditLog.add(event.orderId()));
```

## Push vs pull

| Style | The subject sends | Trade-off |
|---|---|---|
| **Push** | The event data itself (as above) | Simple for observers; may send data they don't need |
| **Pull** | Just a signal; observers query the subject | Observers get what they need; subject must expose its state |

Push with an immutable event object is usually the cleanest choice.

## In Java

- The old `java.util.Observable` and `Observer` are **deprecated since Java 9**. Don't use them; write a small listener interface or use a library.
- `java.beans.PropertyChangeListener` supports observable properties.
- GUI toolkits use listeners (`ActionListener` and similar).
- `java.util.concurrent.Flow` and reactive libraries (for example Reactor, RxJava) are Observer with back-pressure and async support.
- Frameworks offer event publishing (for example Spring's application events).

## In-process observer vs messaging

| | Observer (in-process) | Message broker / event bus |
|---|---|---|
| Scope | Same JVM | Across services |
| Delivery | Synchronous by default | Usually async, persisted |
| Failure | Observer exceptions affect the subject unless handled | Isolated, retried |

For cross-service communication, see [event-driven architecture](../08-architecture/05-event-driven.md).

## Pitfalls to design for

| Issue | Guidance |
|---|---|
| **Memory leaks ("lapsed listener")** | A subscribed observer is referenced by the subject and can't be garbage collected. Always unsubscribe, or use weak references |
| **Exceptions in an observer** | Catch and log so others still run (as shown above) |
| **Notification order** | Don't rely on it unless documented |
| **Concurrent modification** | Iterating while a listener subscribes or unsubscribes. Use `CopyOnWriteArrayList` or copy the list first |
| **Slow observers** | Synchronous notification blocks the subject. Dispatch to an executor if needed |
| **Cascading updates** | Observers that trigger other notifications can loop or become hard to follow |
| **Hidden control flow** | Behavior is spread across listeners; document events and keep them few |

## Observer vs Mediator

| | Observer | [Mediator](./08-mediator.md) |
|---|---|---|
| Direction | One subject → many observers | Many colleagues ↔ one coordinator |
| Knowledge | Subject doesn't know observers | Mediator knows all colleagues |
| Purpose | Broadcast change | Centralize interaction logic |

## Common mistakes

- Forgetting to unsubscribe.
- Publishing mutable event objects that observers can alter.
- Letting an observer exception stop notification of the rest.
- Putting business-critical sequential logic into observers, where the order and failure handling become unclear. If steps must happen in order or atomically, call them explicitly.
- Overusing events for simple direct calls.

## Quick Summary

- Observer lets a subject notify subscribers of changes without knowing them.
- Prefer small listener interfaces with immutable event objects.
- Handle exceptions, unsubscription, threading and ordering deliberately.
- Don't use the deprecated `java.util.Observable`.

## Related

[Mediator](./08-mediator.md) · [Command](./03-command.md) · [Event-driven architecture](../08-architecture/05-event-driven.md)

## Next

[03-command.md](./03-command.md)
