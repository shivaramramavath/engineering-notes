# Factory Method

**Intent:** define a method for creating an object, but let subclasses (or the implementation) decide which concrete class to instantiate.

The caller works with the *product interface* and never names the concrete class, so new product types can be added without changing the caller.

## The problem

```java
public class OrderService {
    public void confirm(Order order, String channel) {
        Notification n;
        if (channel.equals("EMAIL")) n = new EmailNotification();
        else if (channel.equals("SMS")) n = new SmsNotification();
        else throw new IllegalArgumentException(channel);

        n.send(order.contact(), "Order confirmed");
    }
}
```

`OrderService` knows every concrete notification class. Each new channel means editing this method (an [Open/Closed](../02-solid-principles/02-open-closed.md) problem).

## Structure

```text
Creator (abstract)                 Product (interface)
  + createProduct(): Product  ───►   + send(...)
  + someOperation()                      ▲
        ▲                                │
        │                    ┌───────────┴───────────┐
ConcreteCreatorA         EmailNotification      SmsNotification
  + createProduct()
```

- **Product**: the interface of the objects being created.
- **Creator**: declares the factory method, and may use it inside its own logic.
- **ConcreteCreator**: overrides the factory method to return a specific product.

## Classic form (inheritance-based)

```java
public interface Notification {
    void send(String to, String message);
}

public class EmailNotification implements Notification {
    public void send(String to, String message) { /* SMTP */ }
}

public class SmsNotification implements Notification {
    public void send(String to, String message) { /* SMS gateway */ }
}

public abstract class OrderNotifier {
    protected abstract Notification createNotification();   // the factory method

    public void orderConfirmed(Order order) {
        Notification n = createNotification();               // creator uses the product
        n.send(order.contact(), "Order confirmed");
    }
}

public class EmailOrderNotifier extends OrderNotifier {
    @Override protected Notification createNotification() { return new EmailNotification(); }
}

public class SmsOrderNotifier extends OrderNotifier {
    @Override protected Notification createNotification() { return new SmsNotification(); }
}
```

`OrderNotifier.orderConfirmed` contains shared logic and never mentions a concrete notification. Adding push notifications means adding one product class and one creator subclass; nothing existing changes.

## Simpler variations

The inheritance form is the strict GoF definition. In practice you'll often see lighter forms.

### Simple factory (static or instance method)

```java
public final class NotificationFactory {
    public static Notification create(Channel channel) {
        return switch (channel) {
            case EMAIL -> new EmailNotification();
            case SMS   -> new SmsNotification();
        };
    }
}
```

Not a GoF pattern, but very useful. It centralizes the `switch` in one place. The switch is acceptable here because this is the one creation point (see the note on switches in [Open/Closed](../02-solid-principles/02-open-closed.md)). Adding a channel edits this factory only.

### Injected factory using a lambda

```java
public class OrderService {
    private final Supplier<Notification> notificationFactory;

    public OrderService(Supplier<Notification> notificationFactory) {
        this.notificationFactory = notificationFactory;
    }

    public void confirm(Order order) {
        notificationFactory.get().send(order.contact(), "Order confirmed");
    }
}

new OrderService(EmailNotification::new);
```

With first-class functions, a `Supplier` often replaces a whole creator hierarchy.

### Static factory methods on the class

```java
public final class Money {
    private Money(long cents) { ... }
    public static Money ofCents(long cents) { return new Money(cents); }
    public static Money zero() { return new Money(0); }
}
```

These give creation methods meaningful names, can return cached or subtype instances, and aren't required to create a new object each time. A common Java idiom, though not the full GoF pattern.

## Comparison

| Form | Variation chosen by | Use when |
|---|---|---|
| Factory Method (GoF) | Subclass overrides a method | The creator has shared logic that uses the product |
| Simple factory | A parameter and a `switch` in one place | Few types, central creation |
| Injected `Supplier` | The caller passes it in | You only need "make me one of these" |
| Static factory method | The class itself | Named constructors, caching |

## Benefits and costs

| Benefits | Costs |
|---|---|
| Caller depends on the interface only | More classes (in the inheritance form) |
| New products without editing callers | Indirection that can obscure simple code |
| Creation logic is in one place | Needs a subclass per product in the strict form |

## Common mistakes

- **Using a factory for an object that never varies.** Plain `new` is clearer.
- **Leaking the concrete type** through the return type or a downcast after creation, which defeats the pattern.
- **Huge `switch` factories that mix unrelated products.** Split by product family.
- **Confusing it with Abstract Factory.** Factory Method creates *one* product type; [Abstract Factory](./03-abstract-factory.md) creates a *family*.
- **Creating a factory per class by reflex.** Add one when creation varies or is complex.

## Quick Summary

- Factory Method moves "which class to instantiate" behind a method so callers depend on an interface.
- Strict form: subclasses override the creation method. Everyday forms: a simple factory or an injected `Supplier`.
- Use it when new variants appear regularly; skip it for stable, simple objects.

## Related

[Abstract Factory](./03-abstract-factory.md) · [Template Method](../06-behavioral-patterns/05-template-method.md) (the creator's logic is a template that calls the factory method) · [Dependency injection](../03-patterns-intro/02-dependency-injection.md)

## Next

[03-abstract-factory.md](./03-abstract-factory.md)
