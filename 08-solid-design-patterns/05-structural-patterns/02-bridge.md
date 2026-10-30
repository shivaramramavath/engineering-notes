# Bridge

**Intent:** decouple an abstraction from its implementation so the two can vary independently.

Use Bridge when a class has **two independent dimensions of variation**. Without it, you end up with a subclass for every combination.

## The problem

A messaging system has different **message types** (alert, report) and different **delivery channels** (email, SMS, push).

Using inheritance only:

```text
Message
 ├─ EmailAlert
 ├─ SmsAlert
 ├─ PushAlert
 ├─ EmailReport
 ├─ SmsReport
 └─ PushReport
```

Two types × three channels = six classes. Add one more channel and you add two more classes; add a message type and you add three. Growth is **multiplicative**.

## Structure

```text
Abstraction ─────────────► Implementor (interface)
  + operation()             + send(...)
      ▲                          ▲
      │                          │
RefinedAbstraction     ┌─────────┴─────────┐
 (Alert, Report)    EmailSender         SmsSender   PushSender
```

- **Abstraction**: the high-level control layer; it holds a reference to an implementor.
- **Implementor**: the interface for the low-level work.
- **Concrete implementors**: the actual implementations.
- The reference from abstraction to implementor is the **bridge**.

## Example

```java
// Implementation side: how it's delivered
public interface MessageSender {
    void send(String recipient, String subject, String body);
}

public class EmailSender implements MessageSender {
    public void send(String to, String subject, String body) { /* SMTP */ }
}
public class SmsSender implements MessageSender {
    public void send(String to, String subject, String body) { /* SMS gateway: subject + body */ }
}

// Abstraction side: what is being sent
public abstract class Message {
    protected final MessageSender sender;      // the bridge

    protected Message(MessageSender sender) { this.sender = sender; }

    public abstract void deliver(String recipient);
}

public class AlertMessage extends Message {
    private final String text;
    public AlertMessage(MessageSender sender, String text) { super(sender); this.text = text; }

    @Override public void deliver(String recipient) {
        sender.send(recipient, "ALERT", text.toUpperCase());
    }
}

public class ReportMessage extends Message {
    private final String summary;
    public ReportMessage(MessageSender sender, String summary) { super(sender); this.summary = summary; }

    @Override public void deliver(String recipient) {
        sender.send(recipient, "Report", summary);
    }
}

new AlertMessage(new SmsSender(), "Server down").deliver("+15551234567");
new ReportMessage(new EmailSender(), "Weekly numbers").deliver("ops@example.com");
```

Now there are 2 abstractions + 3 implementors = **5 classes**, not 6. The gap widens quickly: with 5 types and 5 channels it's 10 classes instead of 25. Adding `PushSender` touches no message classes, and adding a new message type touches no sender.

## Bridge vs Strategy vs Adapter

All three involve holding a reference to another object, but intent differs.

| Pattern | Purpose | When decided |
|---|---|---|
| **Bridge** | Structure two hierarchies to vary independently | Up front, at design time |
| [Strategy](../06-behavioral-patterns/01-strategy.md) | Swap an algorithm for one operation | At runtime, for behavior |
| [Adapter](./01-adapter.md) | Make an existing incompatible class fit | After the fact, to reconcile |

A short way to remember it: **Adapter** is for things that already exist and don't fit; **Bridge** is designed in advance so the parts *will* fit and stay independent.

## Real-world examples

- **JDBC** is often cited: application code works against `Connection` and `Statement`, while database drivers provide the implementations.
- GUI toolkits: a window abstraction independent of the platform's rendering implementation.
- Shape × renderer (vector vs raster) in graphics code.
- Remote controls × devices (the classic textbook example).

## When to use it

- You see (or expect) a class hierarchy growing in **two directions**.
- You want to switch the implementation at runtime.
- You want to hide implementation details from clients and let them change without recompiling the abstraction.

Skip it when there's only one dimension of variation. A simple interface with several implementations is enough.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Avoids class explosion | More up-front design and indirection |
| Each hierarchy evolves independently | Requires identifying the two dimensions correctly |
| Follows Open/Closed and Single Responsibility | Overkill for small, stable designs |

## Common mistakes

- **Using Bridge where one dimension would do.** An interface plus implementations is simpler.
- **Wrong split**: the "implementation" interface is shaped by the abstraction, so the two can't truly vary separately.
- **Abstraction leaking implementation details** (sender-specific options in message classes).
- **Confusing it with Adapter** and applying it to a retrofit problem.

## Quick Summary

- Bridge separates *what* (abstraction) from *how* (implementation) using composition.
- It replaces a multiplicative class hierarchy with two small ones.
- Apply it when two dimensions of variation are already causing, or clearly will cause, a class explosion.

## Related

[Strategy](../06-behavioral-patterns/01-strategy.md) · [Abstract Factory](../04-creational-patterns/03-abstract-factory.md) (can create the matching implementors) · [Adapter](./01-adapter.md)

## Next

[03-composite.md](./03-composite.md)
