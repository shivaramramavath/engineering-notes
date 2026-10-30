# Single Responsibility Principle (SRP)

> A module should have one, and only one, reason to change.

The usual shorthand, "a class should do one thing", is misleading. A class can have many methods and still follow SRP. What matters is *who or what would force it to change*.

A more precise version: **a module should be responsible to one actor**, meaning one group of people or one concern that requests changes.

## Why it matters

When one class serves several reasons to change:

- A change for one reason can break behavior for another.
- Merge conflicts increase, because unrelated work touches the same file.
- Tests get large, because the class has many unrelated paths.

## Example

```java
public class Employee {
    private String name;
    private double hourlyRate;
    private int hours;

    public double calculatePay() {        // accounting rules
        return hourlyRate * hours;
    }

    public String reportHours() {         // HR reporting format
        return name + ": " + hours + "h";
    }

    public void save() {                  // database schema / persistence
        // INSERT INTO employee ...
    }
}
```

Three different groups can request changes:

| Method | Changes when... |
|---|---|
| `calculatePay` | Accounting changes overtime rules |
| `reportHours` | HR changes the report format |
| `save` | The database or schema changes |

If accounting changes `calculatePay`, an unrelated edit to the report might ship with it, and a bug in either can affect the others.

### Refactored

```java
public class Employee {              // data and core behavior of the entity
    private final String name;
    private final double hourlyRate;
    private final int hours;
    // constructor and getters
}

public class PayCalculator {
    public double calculate(Employee e) { return e.hourlyRate() * e.hours(); }
}

public class HoursReporter {
    public String report(Employee e) { return e.name() + ": " + e.hours() + "h"; }
}

public interface EmployeeRepository {
    void save(Employee e);
}
```

Each class now changes for one reason.

```text
Before:  Employee ──► pay + report + persistence

After:   Employee
         PayCalculator       (accounting)
         HoursReporter       (HR)
         EmployeeRepository  (persistence)
```

## How to spot a violation

- A class name includes `And`, `Manager`, `Handler` or `Util`, or is hard to name precisely.
- Imports cover unrelated areas (SQL, HTTP, formatting, business rules).
- You describe the class with "and": "it validates the order *and* sends the email *and* saves it".
- Different teammates edit the same file for different reasons.
- Tests need unrelated mocks to cover one behavior.

## Finding the right size

SRP does not mean "one method per class".

```java
// Too fine-grained: five classes for one simple concept
class EmailValidator {}
class EmailFormatter {}
class EmailSender {}
class EmailLogger {}
class EmailResultMapper {}
```

Ask: **do these parts change for different reasons, at different times?** If two pieces always change together, keep them together. Splitting them just spreads one responsibility across files.

## Works at every level

| Level | Example |
|---|---|
| Method | A function that parses *and* validates *and* saves should be split |
| Class | The `Employee` example above |
| Module / package | A `utils` package mixing unrelated helpers |
| Service | A microservice that owns unrelated business areas |

## Common mistakes

- **Over-splitting** into dozens of tiny classes that make the flow hard to follow.
- **Treating getters and setters as a "responsibility"** and moving them elsewhere.
- **Splitting by technical layer only** while one business change still touches every class.
- **Ignoring SRP in "just one more method"** additions until a class becomes a god object.

## Quick Summary

- One reason to change, one actor to answer to.
- Split code that changes for different reasons; keep together code that changes together.
- Symptoms: "and" in descriptions, unrelated imports, merge conflicts, heavy test setup.
- Avoid both god classes and a class per method.

## Related

[Coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md) · [Refactoring to patterns](../09-testing-and-debugging/03-refactoring-to-patterns.md)

## Next

[02-open-closed.md](./02-open-closed.md)
