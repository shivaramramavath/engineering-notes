# Facade

**Intent:** provide a simple, unified interface to a set of interfaces in a subsystem, making the subsystem easier to use.

A facade doesn't add new capability. It offers a **convenient entry point** for the common cases and shields clients from the subsystem's internal complexity.

## The problem

Doing one meaningful thing requires coordinating many classes in the right order:

```java
// Every caller must know all of this
InventoryService inventory = new InventoryService();
PaymentService payments = new PaymentService();
ShippingService shipping = new ShippingService();
EmailService email = new EmailService();

if (!inventory.reserve(order.items())) throw new OutOfStockException();
PaymentResult result = payments.charge(order.customerId(), order.total());
if (!result.success()) { inventory.release(order.items()); throw new PaymentFailedException(); }
shipping.schedule(order.address(), order.items());
email.sendConfirmation(order.email());
```

Every caller (web controller, batch job, admin tool) repeats this sequence, and each is coupled to four services.

## Structure

```text
Client ──► Facade ──┬──► InventoryService
                    ├──► PaymentService
                    ├──► ShippingService
                    └──► EmailService

(Subsystem classes remain accessible if a client needs full control)
```

## Example

```java
public class OrderFulfillmentFacade {
    private final InventoryService inventory;
    private final PaymentService payments;
    private final ShippingService shipping;
    private final EmailService email;

    public OrderFulfillmentFacade(InventoryService inventory, PaymentService payments,
                                  ShippingService shipping, EmailService email) {
        this.inventory = inventory;
        this.payments = payments;
        this.shipping = shipping;
        this.email = email;
    }

    public void placeOrder(Order order) {
        if (!inventory.reserve(order.items())) throw new OutOfStockException();

        PaymentResult result = payments.charge(order.customerId(), order.total());
        if (!result.success()) {
            inventory.release(order.items());
            throw new PaymentFailedException();
        }

        shipping.schedule(order.address(), order.items());
        email.sendConfirmation(order.email());
    }
}

// Clients
facade.placeOrder(order);
```

The sequence and compensation logic live in one place. Callers depend on one class, and the subsystem can change behind it.

## Facade vs related patterns

| Pattern | Difference |
|---|---|
| [Adapter](./01-adapter.md) | Converts *one* interface to match an *existing* expected one; a facade defines a *new, simpler* interface over *many* classes |
| [Mediator](../06-behavioral-patterns/08-mediator.md) | Coordinates peers that talk to each other; a facade is a one-way convenience front, and the subsystem doesn't know it exists |
| Service layer | A facade-like layer for application use cases; see [service layer](../07-application-patterns/04-service-layer.md) |
| [Proxy](./07-proxy.md) | Same interface as its target, controls access; a facade has its own interface |

## Where you see facades

- A simple `Client` class over a complicated library or SDK.
- An API gateway or BFF in front of several services.
- Application services that orchestrate several domain objects.
- A "Home theater" style `watchMovie()` over projector, amplifier and player (the classic example).

## Designing a good facade

- Expose **use-case level** methods (`placeOrder`), not a mirror of every subsystem method.
- Keep subsystem classes accessible when advanced users need them. A facade is a convenience, not a wall.
- Use it to define **layer boundaries**: the upper layer talks only to the facade.
- Keep it thin: orchestration and translation, not business rules.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Simpler client code | Can become a god object if it grows too many responsibilities |
| Subsystem can change without affecting clients | May hide features clients need |
| Fewer dependencies for clients (lower coupling) | An extra layer to maintain |
| Clear entry point for a layer | Can duplicate subsystem logic if poorly designed |

## Common mistakes

- **A god facade** that handles every use case in the system. Split by area (`OrderFacade`, `AccountFacade`).
- **Business logic in the facade**, instead of in the domain or subsystem classes.
- **Forcing all access through it**, so clients can't reach the subsystem when needed.
- **One-to-one wrapping** that only renames methods, adding no simplification.
- **Hiding errors** instead of translating them into meaningful ones.

## Quick Summary

- A facade gives a simple entry point over a complex subsystem.
- It reduces coupling and repeated orchestration code.
- Keep it focused and thin; don't let it become a god class.
- Different from Adapter: new simple interface over many classes, not conversion of one.

## Related

[Adapter](./01-adapter.md) · [Mediator](../06-behavioral-patterns/08-mediator.md) · [Service layer](../07-application-patterns/04-service-layer.md)

## Next

[06-flyweight.md](./06-flyweight.md)
