# Programming to Interfaces

"Program to an interface, not an implementation" is the main technique for lowering coupling. Code that depends on a contract can work with any implementation of it, including ones that don't exist yet.

## What it means

Declare variables, parameters and return types using the abstract type, and keep the concrete class in only one place: where the object is created.

```java
// Coupled to the implementation
ArrayList<String> names = new ArrayList<>();

// Coupled to the contract
List<String> names = new ArrayList<>();
```

Swapping `ArrayList` for `LinkedList` now changes one line.

"Interface" here means *any contract*: a Java `interface`, an abstract class, or just the public methods a caller relies on.

## A realistic example

A notification feature that must support email today and SMS later.

### Before

```java
class OrderService {
    private final SmtpEmailSender sender = new SmtpEmailSender();

    void place(Order order) {
        // ...
        sender.sendEmail(order.customerEmail(), "Order confirmed");
    }
}
```

`OrderService` knows about SMTP, and can't be tested without sending email.

### After

```java
interface Notifier {
    void notify(String recipient, String message);
}

class EmailNotifier implements Notifier { ... }
class SmsNotifier   implements Notifier { ... }

class OrderService {
    private final Notifier notifier;

    OrderService(Notifier notifier) { this.notifier = notifier; }

    void place(Order order) {
        // ...
        notifier.notify(order.customerContact(), "Order confirmed");
    }
}
```

Wiring happens at the edge of the application:

```java
var service = new OrderService(new EmailNotifier());
```

A test passes a fake:

```java
class FakeNotifier implements Notifier {
    final List<String> sent = new ArrayList<>();
    public void notify(String r, String m) { sent.add(m); }
}
```

```text
Before:  OrderService ───────────► SmtpEmailSender

After:   OrderService ──► Notifier ◄── EmailNotifier
                              ▲
                              └─────── SmsNotifier / FakeNotifier
```

## Who owns the interface?

Define the interface based on what the **caller needs**, not on what one implementation happens to offer.

```java
// Shaped by the implementation (leaky)
interface SmtpClient { void connect(); void send(Message m); void disconnect(); }

// Shaped by the caller's need
interface Notifier { void notify(String recipient, String message); }
```

The caller-owned version hides the protocol steps. This also leads naturally to [Interface Segregation](../02-solid-principles/04-interface-segregation.md) and [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md).

## When not to create an interface

An interface adds a layer of indirection. Don't pay for it without a reason.

Create one when:

- You already have, or realistically will have, **more than one implementation**.
- You need to **replace a slow or external dependency in tests** (database, network, clock).
- You are drawing a **boundary** between modules or layers.

Skip it when:

- It's a simple value object or data holder.
- There is one implementation and no boundary or testing need. An interface named `FooService` with a lone `FooServiceImpl` is a common smell.

You can extract an interface later, when the need appears.

## Language notes

- **Java / C# / TypeScript:** `interface` keyword; TypeScript uses structural typing, so any object with the right shape satisfies it.
- **Python / JavaScript:** duck typing, with no keyword required. The contract still exists; it is just implicit. Protocols (Python) or JSDoc and TypeScript make it explicit.
- **Functional style:** a single-method interface is often just a function type (`Function<A, B>`, a lambda). Strategy and Command frequently collapse into passing a function.

## Common mistakes

- **Declaring with the concrete type** (`ArrayList<String> x`) and then depending on methods only that class has.
- **Creating the implementation inside the consumer** (`new EmailNotifier()` in `OrderService`). The interface exists but is bypassed.
- **Interface that mirrors a class one-to-one**, with every method exposed. This is just indirection.
- **Leaky interfaces:** method names or exceptions that reveal the implementation (`SQLException` from a generic repository).

## Quick Summary

- Depend on contracts, not concrete classes.
- Construct the concrete object at one point, usually the application's entry or composition root, and pass it in.
- Define interfaces by what the caller needs.
- Don't add an interface unless it earns its place: multiple implementations, testing, or a boundary.

## Next

Continue to the SOLID principles: [../02-solid-principles/00-overview.md](../02-solid-principles/00-overview.md). Dependency injection is covered in [../03-patterns-intro/02-dependency-injection.md](../03-patterns-intro/02-dependency-injection.md).
