# Switch Expressions

Java's classic `switch` is a *statement*: it runs code, falls through between cases, and needs `break` everywhere. A **switch expression** (final in **Java 14**) is an *expression*: it **produces a value**, has no fall-through, and the compiler checks that every possible input is handled.

```java
String name = switch (day) {
    case MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY -> "weekday";
    case SATURDAY, SUNDAY                              -> "weekend";
};
```

**Prerequisites:** [Conditionals](../01-fundamentals/05_conditionals.md), [Enums](../04-oop/11_enums.md).

---

## 1. Old vs new

```java
// Old: statement, colon form, fall-through, mutable variable, easy to forget `break`
String result;
switch (day) {
    case SATURDAY:
    case SUNDAY:
        result = "weekend";
        break;
    default:
        result = "weekday";
}

// New: expression, arrow form, multiple labels, no fall-through
String result = switch (day) {
    case SATURDAY, SUNDAY -> "weekend";
    default               -> "weekday";
};
```

Two independent features arrived together, and it helps to separate them:

| Feature | What it is |
|---|---|
| **Arrow labels** (`case X ->`) | No fall-through, no `break`. Works in statements *and* expressions |
| **Switch as an expression** | The `switch` yields a value you can assign, return, or pass |

---

## 2. Arrow form

```java
switch (command) {                        // statement form: arrows, no fall-through
    case "start" -> start();
    case "stop"  -> stop();
    default      -> System.out.println("Unknown: " + command);
}
```

The right-hand side of an arrow is exactly one of:

1. an **expression** (`"weekend"`, `start()`),
2. a **block** `{ ... }`,
3. a **`throw`** statement.

```java
int days = switch (month) {
    case JANUARY, MARCH, MAY, JULY, AUGUST, OCTOBER, DECEMBER -> 31;
    case APRIL, JUNE, SEPTEMBER, NOVEMBER                     -> 30;
    case FEBRUARY -> leapYear ? 29 : 28;
};                                                             // semicolon: it's an expression

int parsed = switch (token) {
    case "one" -> 1;
    case "two" -> 2;
    default    -> throw new IllegalArgumentException("Unknown: " + token);
};
```

- Multiple labels share one arm: `case 1, 2, 3 ->`.
- Only the matching arm runs. There is **no fall-through** with arrows.
- You can't mix `case X ->` and `case X:` in the same switch.

---

## 3. Blocks and `yield`

When an arm needs several statements, use a block and **`yield`** the result:

```java
String grade = switch (score / 10) {
    case 10, 9 -> "A";
    case 8     -> "B";
    case 7     -> "C";
    default -> {
        String note = score >= 60 ? "pass" : "fail";
        yield "F (" + note + ")";                 // yield, not return
    }
};
```

`yield` gives the block's value to the switch. `return`, `break`, and `continue` **cannot jump out of a switch expression**. The expression must always complete with a value (or throw).

`yield` is also how you produce values in the old colon form of a switch expression, though the arrow form is clearer:

```java
int n = switch (s) {
    case "a": yield 1;
    case "b": yield 2;
    default:  yield 0;
};
```

---

## 4. Exhaustiveness

A switch **expression** must cover every possible value, or it doesn't compile.

| Selector type | How to be exhaustive |
|---|---|
| `enum` | List all constants. No `default` needed |
| `sealed` type | Cover all permitted subtypes ([Sealed Classes](02_sealed-classes.md)) |
| `int`, `String`, `char`, wrapper types | Needs a `default` (the compiler can't enumerate values) |
| Any other reference type with patterns | Needs a total pattern or `default` ([Pattern Matching](04_pattern-matching.md)) |

```java
enum Level { LOW, MEDIUM, HIGH }

int weight = switch (level) {
    case LOW    -> 1;
    case MEDIUM -> 5;
    case HIGH   -> 10;
};                    // adding `CRITICAL` to the enum later → compile error here, which is the point
```

Trade-off with `default` on enums: it silences the compile error when a new constant is added, and the new constant silently takes the default behavior. Prefer listing every constant when each deserves explicit thought. If a switch compiled against an older enum meets a new constant at runtime, an exhaustive switch without `default` throws a **`MatchException`** (Java 21+; earlier versions threw `IncompatibleClassChangeError`).

The exhaustiveness rule applies to switch *expressions*. A classic switch *statement* over an enum or `String` may still omit cases (unless it uses patterns or `case null`; see below).

---

## 5. Supported types and `null`

A switch can select on `int`/`char`/`short`/`byte` (and their wrappers), `String`, and enums. Since Java 21 it can also select on **any reference type using patterns** ([Pattern Matching](04_pattern-matching.md)).

`null` handling:

```java
switch (s) { ... }                 // s == null → NullPointerException  (unless there's a `case null`)

String r = switch (s) {            // Java 21+
    case null      -> "nothing";
    case "a", "b"  -> "ab";
    default        -> "other";
};

case null, default -> "nothing or other";    // combine them
```

Case labels must be **compile-time constants** (literals, `static final` constants, enum constants). A `String` switch compares with `equals` semantics, so it is case-sensitive.

---

## 6. Practical patterns

**Replace if/else-if chains and lookup maps**

```java
double fee = switch (account.type()) {
    case BASIC   -> 0.0;
    case PREMIUM -> 4.99;
    case BUSINESS -> account.seats() * 2.5;
};
```

**Return directly**

```java
static String describe(Status s) {
    return switch (s) {
        case ACTIVE   -> "Active";
        case SUSPENDED -> "Suspended";
        case CLOSED   -> "Closed";
    };
}
```

**Guard clauses with `throw`** keep the happy path flat and make unsupported inputs loud.

**Prefer the statement form with arrows** for side-effect-only branches: no fall-through bugs, no `break`.

When the behavior depends on the *type* of an object rather than a constant, use pattern matching ([Pattern Matching](04_pattern-matching.md)). When each enum constant has its own behavior that belongs *with* the constant, a method on the enum can be cleaner than a switch elsewhere.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Forgetting `break` in colon-form switches (fall-through) | Use arrows |
| `return` / `break` inside a switch expression arm | Use `yield`, or restructure |
| Missing `default` on `int`/`String` switch expression | Add one; the compiler can't prove coverage |
| Mixing `->` and `:` in one switch | Choose one form |
| `default` on an enum that hides new constants | List all constants unless "everything else" is truly one case |
| Switching on a possibly-`null` value | Check first, or use `case null` (21+) |
| Variable name clashes in the old colon form (one shared scope) | Arrow arms have their own scope |
| Expecting `String` switch to ignore case | Normalize with `toLowerCase()` first |
| A semicolon missing after a switch *expression* | It's an expression statement: `int x = switch ... ;` |

### Debugging

- "the switch expression does not cover all possible input values" → add the missing constants/subtypes, or a `default`.
- An error about `return`, `break`, or `continue` "out of a switch expression" → that control flow can't leave an expression; use `yield`, or restructure the code.
- "duplicate case label" → the same constant appears twice, including inside a multi-label arm.
- `MatchException` or `IncompatibleClassChangeError` at runtime → an enum/sealed type changed after this code was compiled. Rebuild dependents.
- Strange output with the colon form → trace fall-through: each matching case runs until a `break`/`yield`.

---

## Quick Summary

- Switch expressions **return a value**: `var x = switch (v) { case A -> 1; case B -> 2; };`
- Arrow (`->`) arms: no fall-through, no `break`. Multiple labels with commas. Arms are an expression, a block, or a `throw`.
- Use **`yield`** to produce a value from a block arm. No `return`/`break`/`continue` out of the expression.
- Switch expressions must be **exhaustive**: enums and sealed types without `default`, other types with `default`.
- `null` throws `NullPointerException` unless you add `case null` (Java 21+).
- Final since Java 14. Pattern labels came in 21.

**Next:** [Pattern Matching](04_pattern-matching.md)