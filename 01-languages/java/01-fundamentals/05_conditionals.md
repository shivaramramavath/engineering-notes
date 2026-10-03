# Conditionals

**Conditionals** choose which code runs based on a `boolean` condition. Java has `if`/`else`, the ternary operator `?:`, and `switch`.

```
          condition
         ┌────┴────┐
       true      false
         │          │
     run block   run else block (or skip)
```

## `if`, `else if`, `else`

```java
int score = 82;

if (score >= 90) {
    System.out.println("A");
} else if (score >= 80) {
    System.out.println("B");
} else if (score >= 70) {
    System.out.println("C");
} else {
    System.out.println("Below C");
}
```

- The condition must be a `boolean`: `if (x)` for an `int` does not compile
- Conditions are checked **top to bottom**; the first true branch runs, the rest are skipped
- Order matters: put the most specific or most restrictive tests first

### Always use braces

```java
if (valid)
    save();
    log();              // runs ALWAYS: it is not part of the if

if (valid) {
    save();
    log();              // both run only when valid
}
```

### Dangling `else`

An `else` belongs to the **nearest** `if`. Braces make the intent explicit.

```java
if (a > 0)
    if (b > 0) System.out.println("both");
    else System.out.println("?");     // belongs to the inner if
```

## Boolean expressions

```java
if (age >= 18 && hasId) { ... }
if (day == 6 || day == 7) { ... }
if (!list.isEmpty()) { ... }
```

Simplify instead of comparing to `true`/`false`:

```java
if (isValid == true) { ... }     // noisy
if (isValid) { ... }             // better

return score >= 50 ? true : false;   // noisy
return score >= 50;                  // better
```

### Beware `=` in conditions

```java
boolean done = false;
if (done = true) { ... }        // assigns true, always runs: compiles without warning
if (done == true) { ... }       // or just: if (done)
```

For `int`, `if (x = 5)` fails to compile, which protects you. For `boolean`, it compiles.

## Comparing the right way

| Type | Compare with | Not |
|------|--------------|-----|
| `int`, `long`, `char`, `boolean` | `==` | n/a |
| `double`, `float` | `Math.abs(a - b) < epsilon` | `==` ([why](./08_numeric-precision-and-math.md)) |
| `String` | `a.equals(b)`, `a.equalsIgnoreCase(b)` | `==` ([why](../03-strings-and-text/02_string-pool-and-comparison.md)) |
| Wrappers (`Integer`) | `a.equals(b)` | `==` ([why](./04_wrapper-classes-and-autoboxing.md)) |
| Other objects | `a.equals(b)` or `Objects.equals(a, b)` | `==` |

```java
if ("yes".equals(answer)) { ... }    // null-safe: a literal on the left never throws
if (answer.equals("yes")) { ... }    // NullPointerException if answer is null
```

## Guard clauses (early return)

Handle invalid or special cases first, then keep the main logic un-nested.

```java
// Deeply nested
double discount(Customer c) {
    if (c != null) {
        if (c.isActive()) {
            if (c.orders() > 10) {
                return 0.2;
            }
        }
    }
    return 0;
}

// Guard clauses
double discount(Customer c) {
    if (c == null || !c.isActive()) return 0;
    if (c.orders() <= 10) return 0;
    return 0.2;
}
```

## Ternary operator

```java
String label = count == 1 ? "item" : "items";
int abs = n < 0 ? -n : n;
```

Good for choosing a **value**. Avoid nesting; it hurts readability:

```java
String grade = s >= 90 ? "A" : s >= 80 ? "B" : s >= 70 ? "C" : "F";   // use if/else
```

## Classic `switch` statement

```java
int day = 3;
switch (day) {
    case 1:
        System.out.println("Mon");
        break;
    case 2:
        System.out.println("Tue");
        break;
    case 3:
        System.out.println("Wed");
        break;
    default:
        System.out.println("Other");
}
```

| Rule | Detail |
|------|--------|
| Allowed selector types | `byte`, `short`, `char`, `int`, their wrappers, `String`, `enum` (and patterns in modern Java) |
| Not allowed | `long`, `float`, `double`, `boolean` |
| `case` labels | Compile-time constants, unique |
| `default` | Optional, may appear anywhere, runs if nothing matches |
| `null` selector | `NullPointerException` (unless `case null` in modern Java) |

### Fall-through

Without `break`, execution **continues into the next case**.

```java
switch (n) {
    case 1:
        System.out.println("one");     // n = 1 prints "one" AND "two"
    case 2:
        System.out.println("two");
        break;
}
```

Fall-through is sometimes useful for grouping:

```java
switch (month) {
    case 12: case 1: case 2:
        season = "winter";
        break;
    case 3: case 4: case 5:
        season = "spring";
        break;
    default:
        season = "other";
}
```

Forgetting `break` is the number one `switch` bug.

## Modern `switch` (preview)

Arrow form: no fall-through, no `break`, and it can return a value.

```java
String name = switch (day) {
    case 1 -> "Mon";
    case 2 -> "Tue";
    case 6, 7 -> "Weekend";
    default -> "Other";
};
```

Standard since Java 14. Full coverage, `yield`, and pattern matching are in [12-modern-java/03_switch-expressions.md](../12-modern-java/03_switch-expressions.md). Prefer the arrow form in new code, and know the classic form for reading older code and interviews.

## `if` or `switch`?

| Use `if` / `else if` | Use `switch` |
|----------------------|--------------|
| Ranges (`score >= 90`) | Many discrete values of one variable |
| Different variables per branch | Enums, strings, small ints |
| Complex boolean conditions | Mapping a value to a result |

For long chains on **types or behaviour**, polymorphism or a `Map` often beats either ([04-oop/07_polymorphism.md](../04-oop/07_polymorphism.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Missing `break` in classic `switch` | Several cases run | Add `break` or use `->` |
| No braces on `if` | Second statement always runs | Always use braces |
| `if (s == "yes")` | Works sometimes, fails sometimes | `"yes".equals(s)` |
| `if (done = true)` | Always true | `if (done)` |
| Semicolon after the condition: `if (x > 5);` | Block always runs | Remove the `;` |
| Wrong branch order for ranges | A later case is never reached | Most restrictive first |
| `switch` on a `null` string | `NullPointerException` | Check for `null` first |
| Comparing doubles with `==` | Unexpected `false` | Use a tolerance |
| Overlong `else if` ladders | Hard to maintain | `switch`, a `Map`, or polymorphism |

## Key takeaways

- Conditions must be `boolean`; always use braces
- Use `.equals()` for objects, a tolerance for floating-point, `==` for primitives
- Prefer guard clauses over deep nesting
- Classic `switch` falls through unless you `break`; the arrow form does not
- The ternary operator is for simple value choices only

**Next:** [Loops](./06_loops.md)
