# Encapsulation and Access Modifiers

**Encapsulation** means bundling data with the methods that work on it, and **hiding** the internal details so outside code can use the object only through a small, safe interface. **Access modifiers** are the language feature that enforces it.

```java
public class Account {
    private double balance;                        // hidden state

    public void deposit(double amount) {           // controlled access
        if (amount <= 0) throw new IllegalArgumentException("amount must be positive");
        balance += amount;
    }

    public double getBalance() { return balance; }
}
```

```
        outside code
             │  can only use
             ▼
   ┌──────────────────┐
   │ public methods   │   deposit(), getBalance()
   ├──────────────────┤
   │ private state    │   balance   ◄── cannot be touched directly
   └──────────────────┘
```

## Why encapsulate?

| Benefit | Explanation |
|---------|-------------|
| **Protects invariants** | `balance` can never become inconsistent because every change passes through validated methods |
| **Freedom to change** | You can alter the internals (store cents instead of a `double`) without breaking callers |
| **Smaller surface to learn** | Users see operations, not implementation details |
| **Easier debugging** | Only a few places can modify state |
| **Safer concurrency** | Controlled access is the first step toward thread safety |

An **invariant** is a rule that must always hold for an object (for example, "balance is never negative", "start is before end"). Encapsulation is how a class guarantees its invariants.

## The four access levels

| Modifier | Same class | Same package | Subclass (other package) | Everywhere |
|----------|:---------:|:------------:|:------------------------:|:----------:|
| `private` | ✅ | ❌ | ❌ | ❌ |
| *(none)*: package-private | ✅ | ✅ | ❌ | ❌ |
| `protected` | ✅ | ✅ | ✅ (through inheritance) | ❌ |
| `public` | ✅ | ✅ | ✅ | ✅ |

```java
package bank;

public class Account {
    private int pin;                 // only Account's own code
    int branchId;                    // anything in package bank
    protected double balance;        // package bank + subclasses anywhere
    public String owner;             // everyone
}
```

### Where each modifier applies

| Element | Allowed modifiers |
|---------|-------------------|
| Top-level class / interface | `public` or package-private only |
| Nested class | All four |
| Fields, methods, constructors | All four |
| Local variables | None |

### `private`

Visible only inside the **top-level class** that contains it (including its nested classes). It is a class-level rule, not an object-level one:

```java
class Point {
    private int x;
    boolean sameX(Point other) { return this.x == other.x; }   // OK: another object, same class
}
```

### Package-private (default)

No keyword. Visible to every class in the same package ([05-packages-and-modules/00_packages-and-imports.md](../05-packages-and-modules/00_packages-and-imports.md)). Good for helper classes that are internal to a package.

### `protected`

Visible in the package **and** to subclasses in other packages, with a catch:

```java
package a;
public class Base { protected void hook() { } }

package b;
public class Sub extends Base {
    void test(Base other, Sub mine) {
        hook();               // OK: inherited, called on this
        mine.hook();          // OK: through a Sub reference
        // other.hook();      // ERROR: through a Base reference from another package
    }
}
```

A subclass in a different package may access a protected member only through references of **its own type (or subtypes)**. `protected` also widens your API to every subclass author, so use it sparingly ([06_inheritance.md](./06_inheritance.md)).

### `public`

Part of your contract: anything public must be supported, documented and kept compatible.

## Getters and setters

```java
public class Person {
    private String name;
    private int age;

    public String getName() { return name; }
    public int getAge() { return age; }

    public void setAge(int age) {
        if (age < 0 || age > 150) throw new IllegalArgumentException("invalid age: " + age);
        this.age = age;
    }
}
```

Getters and setters are **not** encapsulation by themselves. A class with a private field and a public getter **and** setter that do nothing else is just a public field with extra typing.

| Instead of | Prefer |
|------------|--------|
| `account.setBalance(account.getBalance() + 50)` | `account.deposit(50)` (an operation with meaning and rules) |
| `person.setStatus("ACTIVE")` | `person.activate()` |
| Setters for everything | Constructor parameters + immutability; setters only where change is part of the model |
| `getItems()` returning the internal list | An unmodifiable copy or view, plus `addItem(...)` |

The principle is **"tell, don't ask"**: tell an object what to do instead of pulling out its data and deciding for it.

Naming: `getX()` / `setX(...)` / `isX()` for booleans, the JavaBeans convention that frameworks (Jackson, JPA, Spring) rely on. For plain data carriers, use records ([12-modern-java/01_records.md](../12-modern-java/01_records.md)).

## Do not leak internal state

Returning or storing mutable objects defeats encapsulation:

```java
public class Schedule {
    private final List<String> days = new ArrayList<>();

    public List<String> getDays() { return days; }            // LEAK: callers can add or clear
}
```

```java
// Fix 1: return an unmodifiable view
public List<String> getDays() { return Collections.unmodifiableList(days); }
// Fix 2: return a copy
public List<String> getDays() { return List.copyOf(days); }
// Fix 3: copy on the way in
public Schedule(List<String> days) { this.days = new ArrayList<>(days); }
```

Also copy mutable parameters (`Date`, arrays, collections) in constructors and setters ([pass-by-value](../02-methods/01_pass-by-value.md), [23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md), [08-collections/13_immutable-and-unmodifiable-collections.md](../08-collections/13_immutable-and-unmodifiable-collections.md)).

## Immutable classes: the strongest encapsulation

```java
public final class Point {
    private final int x, y;
    public Point(int x, int y) { this.x = x; this.y = y; }
    public int x() { return x; }
    public int y() { return y; }
    public Point withX(int newX) { return new Point(newX, y); }   // "change" = new object
}
```

No setters, all fields `final`, the class `final`, no leaked mutable state. Immutable objects are thread-safe and easy to reason about ([03_final.md](./03_final.md)).

## Designing a public API

| Guideline | Why |
|-----------|-----|
| **Start with the most restrictive access** and widen only when needed | Anything public is hard to take back |
| Make fields `private` (constants may be `public static final`) | State changes are controlled |
| Keep helper methods `private` | They are implementation details |
| Expose behavior, not data | Callers depend on what an object does |
| Validate at the boundary | Invalid state never enters |
| Prefer package-private for internal collaborators | Hidden from other packages |
| Document public members | Contracts need descriptions |
| Use **modules** for strong boundaries between libraries | Packages not exported are invisible even if their classes are `public` ([05-packages-and-modules/02_java-modules.md](../05-packages-and-modules/02_java-modules.md)) |

## Encapsulation is not a security boundary

Access modifiers prevent accidental misuse at compile time. **Reflection** can bypass them (`setAccessible(true)`), subject to module restrictions ([13-advanced-language-features/01_reflection.md](../13-advanced-language-features/01_reflection.md)). Do not rely on `private` to protect secrets.

## Encapsulation vs information hiding vs abstraction

| Term | Focus |
|------|-------|
| Encapsulation | Grouping data + behavior and restricting access to it |
| Information hiding | Hiding design decisions likely to change |
| Abstraction | Presenting a simplified model/contract ([08](./08_abstract-classes.md), [09](./09_interfaces.md)) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Public fields | Anyone can break invariants | `private` fields |
| Getter + setter for every field | No real encapsulation | Expose operations; limit setters |
| Returning internal mutable collections or arrays | Outside code modifies state | Copies or unmodifiable views |
| Storing a caller's mutable object without copying | Later outside changes leak in | Defensive copy |
| Setters without validation | Invalid objects | Validate in constructors and setters |
| Overusing `protected` | Fragile subclasses, wide API | `private` + a small protected hook, or composition |
| Making everything `public` "just in case" | Cannot evolve the class | Narrow access |
| Relying on `private` for security | Reflection bypass | Real security controls ([21-security](../21-security/README.md)) |
| Package-private by accident (forgot the modifier) | Not visible to other packages | Choose the access level deliberately |
| Accessing a `protected` member through a parent-typed reference | Compile error | Use `this` or a subclass reference |

## Key takeaways

- Encapsulation = private state + a validated public interface; its purpose is protecting invariants
- Four levels: `private` < package-private < `protected` < `public`; start restrictive
- Getters/setters alone are not encapsulation: expose operations, not raw data
- Never leak mutable internals; copy in and out, or use immutability
- Access modifiers prevent mistakes; they are not a security mechanism

**Next:** [Initialization Order](./05_initialization-order.md)
