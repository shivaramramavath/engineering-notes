# Adapter

**Intent:** convert the interface of a class into another interface that clients expect, so classes with incompatible interfaces can work together.

Think of a travel plug adapter: the device and the socket stay unchanged, and the adapter sits between them.

## The problem

Your code is written against one interface, but the class you need (a third-party library, a legacy system, a generated client) has a different one, and you can't or shouldn't change it.

```java
// What your application expects
public interface PaymentProcessor {
    PaymentResult pay(String customerId, long amountCents);
}

// What the legacy gateway provides (cannot be modified)
public class LegacyGateway {
    public int makePayment(String account, double amountInDollars, String currency) { ... }
}
```

## Structure

```text
Client ──► Target (interface)
              ▲
              │ implements
           Adapter ──► Adaptee (existing class, different interface)
```

- **Target**: the interface the client wants.
- **Adaptee**: the existing class with the incompatible interface.
- **Adapter**: implements the target and translates calls to the adaptee.

## Example (object adapter)

```java
public class LegacyGatewayAdapter implements PaymentProcessor {
    private final LegacyGateway gateway;

    public LegacyGatewayAdapter(LegacyGateway gateway) {
        this.gateway = gateway;
    }

    @Override
    public PaymentResult pay(String customerId, long amountCents) {
        double dollars = amountCents / 100.0;
        int code = gateway.makePayment(customerId, dollars, "USD");
        return code == 0 ? PaymentResult.success() : PaymentResult.failed("Code " + code);
    }
}

PaymentProcessor processor = new LegacyGatewayAdapter(new LegacyGateway());
checkout.pay(processor, "c-42", 1999);
```

The adapter does the translation:

- Units: cents to dollars.
- Parameters: adding the currency.
- Results: an integer code to a result object.

Client code never sees `LegacyGateway`.

## Object adapter vs class adapter

| | Object adapter | Class adapter |
|---|---|---|
| Mechanism | **Composition**: holds the adaptee | **Inheritance**: extends the adaptee |
| Flexibility | Can adapt any subclass of the adaptee | Bound to one concrete class |
| Java support | Always possible | Limited (single class inheritance) |
| Preferred? | **Yes** | Rarely |

## In the Java standard library

| Class | Adapts |
|---|---|
| `InputStreamReader` | A byte stream (`InputStream`) to a character reader (`Reader`) |
| `Arrays.asList(array)` | An array to the `List` interface |
| `Collections.enumeration(...)` | A `Collection` to an `Enumeration` |

## Where adapters earn their keep

- **Wrapping third-party libraries** so the rest of your code depends on *your* interface. Swapping the library later changes one class. This is [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md) in practice.
- **Legacy integration** while migrating gradually.
- **Anti-corruption layer** between your domain and an external model.
- **Testing**: an adapter boundary is a natural place to substitute a fake.

## Adapter vs related patterns

| Pattern | Difference |
|---|---|
| [Facade](./05-facade.md) | Simplifies a *group* of classes with a new interface; an adapter converts *one* interface to match an existing one |
| [Decorator](./04-decorator.md) | Keeps the **same** interface and adds behavior; an adapter **changes** the interface |
| [Proxy](./07-proxy.md) | Same interface, controls access |

## Common mistakes

- **Putting business logic in the adapter.** It should translate, not decide.
- **Leaking the adaptee's types** (its exceptions, data classes) through the target interface.
- **Adapting something you control.** Change the class (or interface) directly if you own both sides.
- **Two-way translations that lose information** without noticing (rounding, enum mapping).
- **Too many adapter layers** stacked on each other.

## Quick Summary

- Adapter makes an existing class fit the interface your code expects.
- Prefer the object adapter (composition).
- Keep the adapter thin: translate data, calls and errors, nothing more.
- Use it at boundaries with third-party and legacy code.

## Related

[Facade](./05-facade.md) · [Decorator](./04-decorator.md) · [Bridge](./02-bridge.md)

## Next

[02-bridge.md](./02-bridge.md)
