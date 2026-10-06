# Sealed Classes

A **sealed** class or interface restricts which other classes may extend or implement it. You list the allowed subtypes, and the compiler enforces that list. The hierarchy becomes **closed**: nobody outside it can add a new subtype.

```java
public sealed interface Shape permits Circle, Rectangle {}

public record Circle(double radius) implements Shape {}
public record Rectangle(double width, double height) implements Shape {}
```

Why it matters: the compiler now knows *every* possible subtype of `Shape`, so a `switch` over a `Shape` can be checked for **exhaustiveness** ([Pattern Matching](04_pattern-matching.md)). Forget a case and it won't compile, which is how you find all the code that needs updating when you add a new variant.

Sealed classes became final in **Java 17** (preview in 15 and 16).

**Prerequisites:** [Inheritance](../04-oop/06_inheritance.md), [Abstract Classes](../04-oop/08_abstract-classes.md), [Interfaces](../04-oop/09_interfaces.md), [Records](01_records.md).

---

## 1. The problem sealed classes solve

Before sealed types you had two options for a type hierarchy:

| Option | Problem |
|---|---|
| `abstract class` / `interface` | **Open**: anyone can add subtypes, so you can't ever know you've handled all cases |
| `enum` | Closed, but every constant has the **same shape** (can't carry different data per variant) |
| `final` class | Closed, but no hierarchy at all |

Sealed types fill the gap: **a closed set of different shapes**: a payment is *either* a card (number, expiry), *or* a bank transfer (IBAN), *or* cash (nothing). This is the *sum type* / algebraic data type from functional languages.

---

## 2. Syntax and rules

```java
public sealed class Vehicle permits Car, Truck, Bike {}

public final class Car extends Vehicle {}                       // cannot be extended further

public sealed class Truck extends Vehicle permits PickupTruck {}  // continues the restriction
public final class PickupTruck extends Truck {}

public non-sealed class Bike extends Vehicle {}                 // opens the hierarchy again
```

Each permitted subclass **must declare exactly one** of these modifiers:

| Modifier | Meaning |
|---|---|
| `final` | No further subclasses |
| `sealed` | Further subclasses, but only those it permits |
| `non-sealed` | Anyone may extend it (this branch is open again) |

Records are implicitly `final` and enums are implicitly `final` (or `sealed` when constants have bodies), so they need no modifier.

Other rules:

- Permitted subclasses must **directly** extend (or implement) the sealed type.
- A permitted subclass must be in the **same module** as the sealed type, or in the **same package** if the code is in the unnamed module (no `module-info.java`). They don't have to be in the same file.
- A subclass must be accessible to the sealed class.
- A sealed *interface* is implemented by classes, records, and enums (or extended by sub-interfaces, which must themselves be `sealed` or `non-sealed`).

### Omitting `permits`

If the subtypes are declared **in the same source file**, `permits` can be omitted, and the compiler infers them:

```java
public sealed interface Expr {
    record Num(int value) implements Expr {}
    record Add(Expr left, Expr right) implements Expr {}
    record Mul(Expr left, Expr right) implements Expr {}
}
```

Nesting records inside their sealed interface is a popular layout for small hierarchies. It keeps the whole "type definition" in one place.

---

## 3. Exhaustive `switch`: the payoff

```java
static int eval(Expr e) {
    return switch (e) {
        case Expr.Num n          -> n.value();
        case Expr.Add(var l, var r) -> eval(l) + eval(r);     // record pattern: deconstruction
        case Expr.Mul(var l, var r) -> eval(l) * eval(r);
    };                                                         // no default needed
}
```

Because `Expr` is sealed, the compiler verifies all three variants are covered. If you later add `record Neg(Expr inner) implements Expr {}`, every such `switch` **fails to compile** until you handle `Neg`, which is a compile-time to-do list instead of a runtime bug.

> **Avoid `default` on a sealed switch.** A `default` branch makes the switch exhaustive by itself, so the compiler can no longer tell you that you forgot the new variant. Use it only when "everything else" really is one behavior.

See [Switch Expressions](03_switch-expressions.md) and [Pattern Matching](04_pattern-matching.md) for the switch side of this.

---

## 4. A realistic example: results and errors

```java
public sealed interface Result<T> {
    record Success<T>(T value) implements Result<T> {}
    record Failure<T>(String message, Exception cause) implements Result<T> {}
}

static Result<Integer> parse(String s) {
    try {
        return new Result.Success<>(Integer.parseInt(s));
    } catch (NumberFormatException e) {
        return new Result.Failure<>("Not a number: " + s, e);
    }
}

String text = switch (parse(input)) {
    case Result.Success<Integer>(var v)       -> "Parsed " + v;
    case Result.Failure<Integer>(var msg, var cause) -> "Failed: " + msg;
};
```

Other typical uses: domain events (`OrderPlaced | OrderShipped | OrderCancelled`), payment methods, AST nodes, UI commands, protocol messages, state machines. Anywhere you'd have written an enum plus a pile of nullable fields.

---

## 5. Sealed vs enum vs abstract/interface

| | Enum | Sealed hierarchy | Open abstract class / interface |
|---|---|---|---|
| Set of variants | Fixed | Fixed | Open |
| Variants are... | Singleton instances | **Types** (many instances, each with own data) | Types |
| Different fields per variant | Awkward | Natural (records) | Natural |
| Exhaustive `switch` | Yes | Yes | No |
| Third parties can extend | No | No (unless `non-sealed`) | Yes |

Rule of thumb: **constants** → enum; **a fixed set of data shapes** → sealed interface + records; **extension points for others** → ordinary interface/abstract class.

Sealed + polymorphism: if the *set of types* is stable but the *set of operations* keeps growing, put operations in `switch` expressions outside the types (data-oriented style). If the operations are stable but new types keep appearing, ordinary polymorphism and virtual methods fit better ([Polymorphism](../04-oop/07_polymorphism.md)).

---

## 6. Evolution and compatibility

- Adding a permitted subtype is a **source-incompatible** change for code that switches over the type, deliberately. Recompiling flags every `switch` that needs updating.
- If a class compiled against the old hierarchy meets a *new* subtype at runtime (the sealed type was updated but the switch wasn't recompiled), the exhaustive switch throws a **`MatchException`** (Java 21+). Rebuild dependents together.
- Sealing is a **design commitment**: if you publish a sealed type in a library, consumers can't add their own subtypes, and you can't add yours without breaking their exhaustive switches. Seal types in libraries only when you really mean "this is the complete list".
- Sealed types also work with the module system and are visible via reflection: `Class.isSealed()` and `Class.getPermittedSubclasses()` ([Reflection](../13-advanced-language-features/01_reflection.md)).

---

## Common mistakes and misconceptions

| Mistake / belief | Reality |
|---|---|
| "Sealed means `final`" | Sealed allows a *specific* set of subclasses; `final` allows none |
| "Subclasses must be in the same file" | Same file only lets you omit `permits`. Otherwise same package/module |
| Forgetting a modifier on a permitted subclass | Compile error: it must be `final`, `sealed`, or `non-sealed` |
| `default ->` in a switch over a sealed type | Defeats the compiler's completeness check |
| Making a subtype `non-sealed` casually | It reopens that branch, so exhaustiveness for subtypes is lost |
| Sealing something third parties are meant to extend | Use a normal interface or `non-sealed` |
| Using sealed for private implementation hiding | It's about *closed variants*, not information hiding. Use package-private/modules for that |

### Debugging

- `class is not allowed to extend sealed class` → the class isn't in the `permits` list, or it's in a different package/module than allowed.
- `sealed, non-sealed or final modifiers expected` → add one to the permitted subclass.
- "the switch expression does not cover all possible input values" → a permitted subtype is missing from your `switch`.
- `MatchException` at runtime → the hierarchy changed after the switch was compiled. Rebuild everything that depends on it.

---

## Quick Summary

- `sealed` + `permits` declares a **closed set of subtypes**. Each subtype must be `final`, `sealed`, or `non-sealed`.
- Permitted subclasses live in the same module (or same package in the unnamed module). `permits` can be omitted when they're in the same file.
- The real benefit is **exhaustive `switch`** with no `default`: the compiler tells you every place a new variant needs handling.
- Combine with records (data) and pattern matching (logic) for algebraic-data-type style modeling.
- Use enums for constants, sealed types for a fixed set of data shapes, and plain interfaces for open extension.
- Final since Java 17.

**Next:** [Switch Expressions](03_switch-expressions.md)
