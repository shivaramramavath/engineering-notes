# Refactoring to Patterns

**Refactoring** is changing the internal structure of code without changing its observable behavior. **Refactoring to patterns** means doing that deliberately, in small safe steps, to move code from a smelly state toward a pattern that solves a real problem.

The key idea: patterns are best discovered **from a problem in existing code**, not imposed up front. You see the pain (a growing `switch`, duplicated flow, tangled creation), then apply the pattern that removes it.

## The safe process

```text
1. Make sure behavior is covered by tests        (characterization tests if none exist)
        ↓
2. Identify the smell and the pain it causes
        ↓
3. Choose the target (pattern or simple restructuring)
        ↓
4. Apply SMALL steps; run the tests after each one
        ↓
5. Stop when the pain is gone     (don't finish the whole textbook structure)
```

Never refactor without a safety net. If tests don't exist, write **characterization tests** first: tests that record what the code currently does, right or wrong, so you notice any change. See [debugging design issues](./04-debugging-design-issues.md).

Don't mix refactoring with behavior changes in the same step or commit. Change structure *or* behavior, then switch.

## Smells and the patterns that address them

| Smell (what you see) | Often addressed by | Principle |
|---|---|---|
| Growing `switch` / `if-else` on a type or mode | [Strategy](../06-behavioral-patterns/01-strategy.md), polymorphism | OCP |
| Behavior depends on a status field, with checks in many methods | [State](../06-behavioral-patterns/04-state.md) | OCP, SRP |
| Same algorithm skeleton copied with small differences | [Template Method](../06-behavioral-patterns/05-template-method.md) or Strategy | DRY |
| `new ConcreteClass()` scattered; creation logic varies | [Factory Method](../04-creational-patterns/02-factory-method.md) / factory | DIP |
| Constructor with many parameters, many optional | [Builder](../04-creational-patterns/04-builder.md) | Readability |
| Clients depend on a complex subsystem | [Facade](../05-structural-patterns/05-facade.md) | Low coupling |
| Third-party/legacy interface doesn't fit | [Adapter](../05-structural-patterns/01-adapter.md) | DIP |
| Cross-cutting code (logging, caching) tangled into logic | [Decorator](../05-structural-patterns/04-decorator.md) / [Proxy](../05-structural-patterns/07-proxy.md) | SRP |
| One class notifies several others by direct calls | [Observer](../06-behavioral-patterns/02-observer.md) | Low coupling |
| Many classes talk to each other in a tangle | [Mediator](../06-behavioral-patterns/08-mediator.md) | Low coupling |
| Type checks on hierarchy members everywhere | [Visitor](../06-behavioral-patterns/10-visitor.md) or sealed types + `switch` | OCP |
| Code that treats single and grouped objects differently | [Composite](../05-structural-patterns/03-composite.md) | Uniformity |
| Undo/queue/retry needs | [Command](../06-behavioral-patterns/03-command.md) | |
| Global state accessed everywhere | Dependency injection (instead of [Singleton](../04-creational-patterns/01-singleton.md)) | DIP |

Notice that for each smell, a plain refactoring (extract method, move method) is sometimes enough. The pattern is one possible destination, not the goal.

## Worked example: replace conditional with polymorphism

### Starting point

```java
public class Employee {
    private final EmployeeType type;
    private final double baseSalary;
    private final double bonus;

    public double pay() {
        switch (type) {
            case ENGINEER: return baseSalary;
            case MANAGER:  return baseSalary + bonus;
            case SALES:    return baseSalary + bonus * 2;
            default: throw new IllegalStateException();
        }
    }
}
```

Pain: every new employee type edits `pay()` and any other method with the same `switch` (`title()`, `canApprove()`).

### Step 1: ensure tests exist

```java
@Test void engineerGetsBaseSalary() { assertEquals(5000, engineer(5000, 1000).pay()); }
@Test void managerGetsBonus()       { assertEquals(6000, manager(5000, 1000).pay()); }
@Test void salesGetsDoubleBonus()   { assertEquals(7000, sales(5000, 1000).pay()); }
```

### Step 2: extract the strategy interface

```java
interface PayPolicy { double pay(double baseSalary, double bonus); }
```

### Step 3: move each branch into a policy class

```java
class EngineerPay implements PayPolicy { public double pay(double b, double bonus) { return b; } }
class ManagerPay  implements PayPolicy { public double pay(double b, double bonus) { return b + bonus; } }
class SalesPay    implements PayPolicy { public double pay(double b, double bonus) { return b + bonus * 2; } }
```

### Step 4: delegate, then remove the switch

```java
public class Employee {
    private final PayPolicy policy;
    private final double baseSalary;
    private final double bonus;

    public double pay() { return policy.pay(baseSalary, bonus); }
}
```

The one remaining `switch` (choosing which policy to create) moves to a factory. Run the tests after each step. A new employee type is now a new class, and nothing existing changes.

Alternative end states: an enum with abstract methods (smaller), or a map of lambdas. Choose the lightest form that removes the pain.

## Everyday refactorings (not patterns, but the building blocks)

| Refactoring | Use when |
|---|---|
| **Extract method** | A block does one nameable thing |
| **Extract class** | A class has two responsibilities (SRP) |
| **Move method / field** | A method uses another class's data more than its own |
| **Introduce parameter object** | Several parameters always travel together |
| **Replace inheritance with delegation** | A subclass uses only part of the parent, or breaks [LSP](../02-solid-principles/03-liskov-substitution.md) |
| **Replace constructor with factory method** | Creation needs a name or varies |
| **Introduce interface** | You need a seam for a dependency |
| **Replace magic numbers / primitives with value objects** | Primitives carry business meaning (`Money`, `Email`) |
| **Inline method / class** | An abstraction no longer earns its place |

## Code smells and anti-patterns

A **code smell** is a surface sign of a possible design problem. It's a prompt to investigate, not an automatic defect.

| Smell / anti-pattern | Description | Typical fix |
|---|---|---|
| **God object** | One class knows and does too much | Extract classes by responsibility |
| **Anemic domain model** | Entities are data bags; logic lives in services | Move behavior into the entities ([service layer](../07-application-patterns/04-service-layer.md)) |
| **Service locator** | Classes fetch dependencies from a global registry | Constructor injection |
| **Feature envy** | A method uses another object's data more than its own | Move the method |
| **Shotgun surgery** | One change requires small edits in many classes | Gather the scattered responsibility |
| **Divergent change** | One class changes for many unrelated reasons | Split it (SRP) |
| **Primitive obsession** | Strings and ints stand in for domain concepts | Value objects |
| **Long parameter list** | Many arguments | Parameter object, Builder |
| **Long method** | A method does many steps | Extract method |
| **Duplicated code** | The same logic in several places | Extract and reuse (but see below) |
| **Speculative generality** | Abstractions with only one use, "just in case" | Inline them |
| **Refused bequest** | Subclass ignores or rejects parent behavior | Composition instead of inheritance |
| **Message chains** | `a.getB().getC().getD()` | Hide delegation (Law of Demeter) |
| **Circular dependencies** | A depends on B depends on A | Introduce an interface or event, or merge |
| **Shared mutable state** | Many parts change the same data | Encapsulate or make immutable |

## When duplication is fine

Two blocks of code that *look* alike but change for **different reasons** are not true duplication. Merging them couples unrelated concerns. Wait until a third occurrence reveals the real shared concept ("rule of three"), and prefer a little duplication over the wrong abstraction.

## When not to refactor to a pattern

- The code is stable and rarely changes.
- There are only two variants and no sign of more.
- The pattern makes the code harder to follow than the original.
- You have no tests and no time to add them.
- It's a throwaway script or prototype.

A good refactoring leaves the code **simpler to understand**, not merely "more patterned". See [avoiding overengineering](../10-production/05-avoiding-overengineering.md).

## Common mistakes

- **Big-bang rewrites** instead of small, tested steps.
- **Refactoring without tests**, then discovering behavior changed.
- **Mixing refactoring with feature work** in one commit, which makes bugs hard to locate.
- **Starting with the pattern** you want to use, then looking for a place to put it.
- **Stopping halfway** through, leaving both the old and new structure.
- **Renaming and restructuring on a shared file** without coordinating, causing merge pain.
- **Treating every smell as a bug** and refactoring code nobody touches.
- **Over-abstracting** with interfaces for a single implementation.

## Quick Summary

- Refactor in small steps with tests as a safety net, never changing behavior and structure together.
- Let a real smell lead you to a pattern; stop when the pain is gone.
- Many smells have simple fixes (extract, move, rename) that need no pattern at all.
- Not every duplication or conditional deserves an abstraction.

## Related

[Debugging design issues](./04-debugging-design-issues.md) · [SOLID in practice](../02-solid-principles/06-solid-in-practice.md) · [Pattern selection](../13-quick-reference/03-pattern-selection.md) · [Testable design](./01-testable-design.md)

## Next

[04-debugging-design-issues.md](./04-debugging-design-issues.md)
