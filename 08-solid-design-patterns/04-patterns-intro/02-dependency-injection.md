# Dependency Injection (DI)

Dependency injection means an object **receives** the collaborators it needs from the outside instead of creating or looking them up itself. It is the most common technique for achieving the [Dependency Inversion Principle](../02-solid-principles/05-dependency-inversion.md) and for making code testable.

## The idea in one comparison

```java
// Creates its own dependency: tightly coupled
class OrderService {
    private final EmailNotifier notifier = new EmailNotifier();
}

// Receives its dependency: loosely coupled
class OrderService {
    private final Notifier notifier;
    OrderService(Notifier notifier) { this.notifier = notifier; }
}
```

The second version doesn't know or care which `Notifier` it gets. Something else decides.

## Why it matters

| Benefit | How |
|---|---|
| Testability | Pass a fake in tests, no real email, database or network |
| Flexibility | Swap implementations without editing the consumer |
| Clarity | The constructor lists exactly what the class needs |
| Reuse | The same class works in different configurations |

## Prerequisites

[Programming to interfaces](../01-foundations/04-programming-to-interfaces.md): DI is most useful when the injected type is an abstraction.

## Forms of injection

### Constructor injection (preferred)

```java
class OrderService {
    private final OrderRepository repository;
    private final Notifier notifier;

    OrderService(OrderRepository repository, Notifier notifier) {
        this.repository = Objects.requireNonNull(repository);
        this.notifier = Objects.requireNonNull(notifier);
    }
}
```

- Dependencies are required and visible.
- Fields can be `final`, so the object is never half-built.
- Easy to construct in a test.

### Setter injection

```java
class ReportService {
    private Formatter formatter = new DefaultFormatter();   // sensible default
    void setFormatter(Formatter f) { this.formatter = f; }
}
```

Use for genuinely *optional* dependencies with a default. The object can be used before the setter is called, so required dependencies don't belong here.

### Method (parameter) injection

```java
class ReportService {
    String render(Report report, Formatter formatter) { return formatter.format(report); }
}
```

Use when the dependency differs per call and isn't part of the object's state.

| Form | Use for |
|---|---|
| Constructor | Required dependencies (default choice) |
| Setter | Optional dependencies with defaults |
| Method | Per-call variation |

Field injection (annotating a private field and letting a framework set it) is possible in many frameworks, but it hides dependencies and makes plain construction in tests awkward. Avoid it in your own code.

## Manual DI and the composition root

You don't need a framework. Build the object graph in **one place**, close to the program's entry point, called the **composition root**.

```java
public class Main {
    public static void main(String[] args) {
        DataSource ds = createDataSource();
        OrderRepository repo = new JdbcOrderRepository(ds);
        Notifier notifier = new EmailNotifier(smtpConfig());
        OrderService service = new OrderService(repo, notifier);

        new HttpServer(service).start();
    }
}
```

```text
Main (composition root)
  ├─ creates JdbcOrderRepository
  ├─ creates EmailNotifier
  └─ creates OrderService(repo, notifier)

Everything else only sees interfaces.
```

The rest of the code should not call `new` on collaborators; that's the composition root's job.

## DI containers

A container automates the wiring. You declare what depends on what, and the container constructs and supplies the objects.

Examples in Java: Spring, Guice, Jakarta CDI. A typical Spring setup:

```java
@Service
public class OrderService {
    private final OrderRepository repository;
    private final Notifier notifier;

    public OrderService(OrderRepository repository, Notifier notifier) {   // single constructor: injected
        this.repository = repository;
        this.notifier = notifier;
    }
}
```

What a container gives you:

- Automatic construction of the object graph.
- **Lifetime/scope management**: one shared instance (singleton scope), a new one each time, or one per request.
- Cross-cutting features such as transactions or proxying.

| Scope | Meaning |
|---|---|
| Singleton | One instance shared across the application |
| Prototype / transient | A new instance every time it's requested |
| Request / session | One instance per request or session (web frameworks) |

A "singleton" in the container sense is just *one shared instance*, and the class itself doesn't enforce it. This is why the [Singleton pattern](../04-creational-patterns/01-singleton.md) is rarely hand-written today.

### Container vs manual wiring

| | Manual | Container |
|---|---|---|
| Visibility | Explicit, easy to follow | Hidden behind annotations/config |
| Setup cost | Grows with the app | Low per class |
| Errors | Compile-time | Often runtime (missing bean, ambiguity) |
| Best for | Small apps, libraries, learning | Larger apps with many components |

Starting manually is a good way to understand what the container does for you.

## Service locator: the thing DI is not

```java
class OrderService {
    void place(Order o) {
        Notifier n = ServiceLocator.get(Notifier.class);   // pulls from a global registry
        n.notify(...);
    }
}
```

Here the class *asks* for its dependency from a global registry. The problems:

- Dependencies are hidden. You can't tell what the class needs from its constructor.
- Tests must configure global state.
- Missing registrations fail at runtime.

Prefer constructor injection; keep any locator usage at the composition root, if at all.

## Testing with DI

```java
class FakeNotifier implements Notifier {
    final List<String> sent = new ArrayList<>();
    public void notify(String to, String msg) { sent.add(msg); }
}

@Test
void placingAnOrderSendsConfirmation() {
    var notifier = new FakeNotifier();
    var service = new OrderService(new InMemoryOrderRepository(), notifier);

    service.place(new Order("a@example.com"));

    assertEquals(1, notifier.sent.size());
}
```

More on fakes, stubs and mocks: [test doubles](../09-testing-and-debugging/02-test-doubles.md).

## Common mistakes

- **Too many constructor parameters** (five or more). Usually a sign the class has too many responsibilities (SRP). Split it rather than hiding the symptom.
- **Injecting the container itself** into a class so it can fetch what it likes. That's a service locator.
- **Circular dependencies** (A needs B, B needs A). A design problem; extract the shared part into a third type or reverse one dependency (for example with an event).
- **Injecting concrete classes** everywhere and calling it DI. The benefit comes from depending on abstractions at volatile boundaries.
- **Injecting everything**, including simple values and stable utilities. Inject things that vary or need isolation in tests.
- **Calling `new` on collaborators outside the composition root.**
- **Letting a framework annotation spread** throughout business classes when plain constructors would do.

## Debugging DI problems

| Symptom | Likely cause |
|---|---|
| "No qualifying bean" / missing dependency | Implementation isn't registered or scanned |
| "Multiple candidates" / ambiguity | Two implementations of the same interface; qualify or choose one |
| Circular dependency error | Two classes depend on each other; redesign |
| `NullPointerException` on an injected field | Object created with `new` outside the container, or field injection not applied |
| Shared state bugs | A singleton-scoped object holds mutable per-request state |

## Quick Summary

- DI: receive your collaborators, don't build or fetch them.
- Prefer constructor injection; use setters only for optional dependencies.
- Wire everything once at the composition root, by hand or with a container.
- A container automates wiring and manages scope; it isn't required to practice DI.
- Avoid service locators, field injection in your own code, and constructors with many parameters.

## Next

[Creational patterns](../04-creational-patterns/00-overview.md)
