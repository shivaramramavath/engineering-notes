# Dependency Inversion Principle (DIP)

> 1. High-level modules should not depend on low-level modules. Both should depend on abstractions.
> 2. Abstractions should not depend on details. Details should depend on abstractions.

**High-level** code holds your business rules (what the application does). **Low-level** code handles details (databases, HTTP, files, email, frameworks). DIP says the business rules should not be tied to those details.

## The problem

```java
public class ReportService {
    private final MySqlReportRepository repository = new MySqlReportRepository();

    public String monthlyReport() {
        var data = repository.fetchMonthlyData();
        // business logic...
        return format(data);
    }
}
```

`ReportService` (high-level) depends directly on `MySqlReportRepository` (low-level):

- Changing the database forces changes to business code.
- You can't unit-test the logic without a MySQL instance.
- The business rules can't be reused with another data source.

```text
ReportService ──► MySqlReportRepository ──► MySQL
(business)            (detail)
```

## The inversion

Introduce an abstraction **owned by the high-level module**, and make the low-level code implement it.

```java
// Lives with the business code, shaped by what the business needs
public interface ReportRepository {
    List<Row> fetchMonthlyData();
}

public class ReportService {
    private final ReportRepository repository;

    public ReportService(ReportRepository repository) {   // injected
        this.repository = repository;
    }

    public String monthlyReport() {
        var data = repository.fetchMonthlyData();
        return format(data);
    }
}

// Lives in the infrastructure code
public class MySqlReportRepository implements ReportRepository {
    public List<Row> fetchMonthlyData() { /* JDBC */ }
}
```

```text
Before:   ReportService ─────────► MySqlReportRepository

After:    ReportService ──► ReportRepository ◄── MySqlReportRepository
          (business)         (abstraction)         (detail)
```

The source-code arrow from low-level to high-level is what got **inverted**. The *runtime* call still goes from the service into the database, but the *compile-time* dependency now points toward the business layer.

## The important part: who owns the abstraction

If `ReportRepository` were defined inside the database package and shaped like SQL operations, the dependency would still point the wrong way. The interface belongs to the **consumer** and speaks the consumer's language (`fetchMonthlyData`), not the provider's (`executeQuery`).

## Three terms that get mixed up

| Term | What it is |
|---|---|
| **Dependency Inversion Principle** | A design *principle*: depend on abstractions |
| **Dependency Injection** | A *technique*: pass dependencies in (constructor, setter) instead of creating them inside |
| **Inversion of Control (IoC)** | A broader idea: a framework or caller controls the flow and wiring, not your code |

Injection is the most common way to *achieve* inversion, but you can use injection without inverting anything (injecting a concrete class), and you can follow DIP without a framework. See [dependency injection](../03-patterns-intro/02-dependency-injection.md).

## Wiring it up

Concrete classes are chosen at the application's edge, often called the **composition root**.

```java
public class Main {
    public static void main(String[] args) {
        ReportRepository repo = new MySqlReportRepository(dataSource);
        ReportService service = new ReportService(repo);
        // start the application
    }
}
```

In tests:

```java
ReportRepository fake = () -> List.of(new Row("jan", 100));   // ReportRepository has one method
var service = new ReportService(fake);
assertEquals(expected, service.monthlyReport());
```

## Where DIP appears in architecture

Layered designs that follow DIP place the domain at the center and make infrastructure depend on it. This is the common thread of [clean, hexagonal and onion architecture](../08-architecture/02-clean-hexagonal-onion.md).

## Common mistakes

- **Interface that mirrors the implementation** (`MySqlReportRepository` with an `IMySqlReportRepository` copy). No inversion, just indirection.
- **Leaky abstractions**: the interface exposes SQL types, HTTP status codes or provider-specific exceptions.
- **Creating the implementation inside the consumer** (`new MySqlReportRepository()`). The abstraction is bypassed.
- **Abstracting everything**, including stable things like `String` or `List`. Depend on abstractions for *volatile* things: databases, external services, clocks, randomness, file systems.
- **Service locator as a shortcut.** Pulling dependencies from a global registry hides them. Prefer explicit constructor parameters.

## Quick Summary

- Business rules should not know about databases, frameworks or other details.
- The consumer defines the abstraction; the detail implements it.
- DIP is the principle, DI is a technique, IoC is the broader idea.
- Wire concrete classes once, at the edge of the application.
- Abstract what is volatile, not everything.

## Next

[06-solid-in-practice.md](./06-solid-in-practice.md)
