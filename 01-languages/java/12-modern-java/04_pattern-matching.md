# Pattern Matching

A **pattern** combines two things Java code does constantly: **test** whether a value has a certain shape, and, if it does, **extract** parts of it into variables. Pattern matching removes the cast-after-`instanceof` ritual and lets `switch` branch on *types* and *structure*, not just constants.

```java
// Before
if (obj instanceof String) {
    String s = (String) obj;
    System.out.println(s.length());
}

// After
if (obj instanceof String s) {
    System.out.println(s.length());
}
```

It arrived in pieces, so versions matter:

| Feature | Final in |
|---|---|
| Pattern matching for `instanceof` | Java 16 |
| Pattern matching for `switch` (type patterns, guards, `case null`) | Java 21 |
| Record patterns (deconstruction) | Java 21 |
| Unnamed patterns and variables (`_`) | Java 22 |
| Primitive types in patterns | **Still preview** (fifth preview in Java 27); needs `--enable-preview` |

**Prerequisites:** [Records](01_records.md), [Sealed Classes](02_sealed-classes.md), [Switch Expressions](03_switch-expressions.md).

---

## 1. `instanceof` patterns

```java
if (obj instanceof String s && s.length() > 3) {
    System.out.println(s.toUpperCase());          // s is in scope and already a String
}
```

`s` is a **pattern variable**. It's only usable where the compiler can prove the match succeeded (*flow scoping*):

```java
if (!(obj instanceof String s)) {
    return;                    // leaves the method when it does NOT match
}
System.out.println(s.length()); // s is in scope here: the only way to get here is a match

boolean bad = !(obj instanceof String s) || s.isEmpty();   // right side runs only if it matched
```

Pattern variables **don't leak** where a match isn't guaranteed: `if (a instanceof String s || b)` can't use `s`.

A classic use is `equals`:

```java
@Override public boolean equals(Object o) {
    return o instanceof Point p && x == p.x && y == p.y;
}
```

(See [equals and hashCode](../04-oop/14_equals-and-hashcode.md).) `instanceof` is `false` for `null`, so no explicit null check is needed.

---

## 2. Pattern matching for `switch`

A `switch` can now select on **types**:

```java
static String describe(Object o) {
    return switch (o) {
        case null                 -> "null";
        case Integer i when i > 100 -> "big int " + i;       // guard: `when` + boolean condition
        case Integer i            -> "int " + i;
        case String s             -> "string of length " + s.length();
        case int[] arr            -> "int array of " + arr.length;
        default                   -> "something else";
    };
}
```

Rules:

- **Order matters.** Cases are tested top to bottom, and a case that is *dominated* by an earlier one is a **compile error**. `case Integer i` before `case Integer i when i > 100` fails, because the second can never run. Put specific cases (and guarded ones) first.
- **Guards** use `when` followed by any boolean expression.
- **`null`:** a pattern switch throws `NullPointerException` for `null` unless it has `case null` (or `case null, default`).
- **Exhaustive:** a pattern switch (expression or statement) must cover all possibilities, by a `default`, a *total* pattern (`case Object o`), or a sealed hierarchy.
- Multiple patterns can't share an arm if they bind variables, so use separate arms (or a common supertype pattern).

### With sealed types: no `default`

```java
sealed interface Shape permits Circle, Square, Rect {}
record Circle(double r) implements Shape {}
record Square(double side) implements Shape {}
record Rect(double w, double h) implements Shape {}

static double area(Shape s) {
    return switch (s) {
        case Circle c -> Math.PI * c.r() * c.r();
        case Square q -> q.side() * q.side();
        case Rect   r -> r.w() * r.h();
    };                                         // compiler proves this is exhaustive
}
```

Add a new `Triangle` to the sealed interface and `area` stops compiling until you handle it.

---

## 3. Record patterns: deconstruction

A **record pattern** tests for a record type *and* pulls out its components in one step:

```java
record Point(int x, int y) {}

if (obj instanceof Point(int x, int y)) {
    System.out.println("x=" + x + ", y=" + y);
}
```

They **nest**, so you can match a whole structure at once:

```java
record Line(Point start, Point end) {}

static double length(Object o) {
    if (o instanceof Line(Point(var x1, var y1), Point(var x2, var y2))) {
        return Math.hypot(x2 - x1, y2 - y1);
    }
    return 0;
}
```

`var` in a nested pattern infers the component's type ([`var`](05_var-and-type-inference.md)).

Combined with sealed types, record patterns give compact, readable logic:

```java
sealed interface Expr permits Num, Add, Mul {}
record Num(int value) implements Expr {}
record Add(Expr left, Expr right) implements Expr {}
record Mul(Expr left, Expr right) implements Expr {}

static int eval(Expr e) {
    return switch (e) {
        case Num(int v)          -> v;
        case Add(Expr l, Expr r) -> eval(l) + eval(r);
        case Mul(Expr l, Expr r) -> eval(l) * eval(r);
    };
}

// Nested patterns express special cases directly
static Expr simplify(Expr e) {
    return switch (e) {
        case Mul(Num(int v), Expr r) when v == 1 -> simplify(r);      // 1 * r  →  r
        case Mul(Num(int v), Expr r) when v == 0 -> new Num(0);       // 0 * r  →  0
        case Add(Expr l, Expr r) -> new Add(simplify(l), simplify(r));
        default -> e;
    };
}
```

Behind the scenes, deconstruction calls the record's **accessor methods**. If an accessor throws, the exception is wrapped in a `MatchException`.

---

## 4. Unnamed patterns and variables: `_` (Java 22)

When you don't need a value, say so with `_`:

```java
case Circle _  -> "a circle";                         // type test, no variable
if (o instanceof Point(int x, _)) { ... }             // only x is needed
for (var _ : requests) { counter++; }                 // loop variable unused
try { Integer.parseInt(s); } catch (NumberFormatException _) { return false; }
list.forEach(_ -> count.incrementAndGet());           // unused lambda parameter
```

`_` can't be read, and it tells both the compiler and readers "intentionally unused". Before Java 22, `_` was not usable as an identifier at all (it was already an error from Java 9).

Several `_` patterns can share one arm, because none binds a variable: `case Circle _, Square _ -> "round or square"`.

---

## 5. Pattern matching, polymorphism, and the visitor pattern

Two ways to add behavior across a family of types:

| | Virtual methods (polymorphism) | `switch` over a closed hierarchy |
|---|---|---|
| Logic lives | Inside each type | In one place, outside the types |
| Adding a new **operation** | Edit every class | Add one method |
| Adding a new **type** | Add one class | Compiler flags every switch (sealed) |
| Best when | Types open-ended, behavior intrinsic to them | Types fixed (sealed), operations keep growing, data is "just data" |

Pattern-matching `switch` over sealed records is the modern replacement for the **Visitor pattern**: same goal (operations outside the types, checked for completeness) without `accept`/`visit` boilerplate. Use polymorphism when the behavior truly belongs to the object ([Polymorphism](../04-oop/07_polymorphism.md)).

Avoid long chains of `instanceof`/pattern switches over *open* hierarchies (a switch with a `default` that quietly absorbs new types is a design smell: use a virtual method instead).

---

## 6. Primitive patterns (preview)

Allowing primitive types in patterns (`instanceof`/`switch`) is still a **preview feature** (fifth preview in Java 27). It needs `--enable-preview` and its details may change, so don't rely on it in code you ship. Check the current JEP before using it ([Release Timeline](00_java-release-timeline.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Dominated case label (general pattern before specific) | Order from most specific to least specific; guarded cases first |
| Switching on `null` without `case null` | Add `case null`, or check before |
| Using `default` on a sealed switch | Remove it so new subtypes cause compile errors |
| Using a pattern variable where the match isn't guaranteed | Restructure with `&&`, or negate and return early |
| Expecting `instanceof` patterns to work with generics erased at runtime (`List<String> l`) | Use `List<?>`: a runtime check can't see type arguments ([Type Erasure](../07-generics/03_type-erasure.md)) |
| Writing `case A a, B b ->` (two binding patterns) | Not allowed. Use separate arms, or unnamed `_` patterns |
| Repeating logic across many pattern switches on the same hierarchy | Put the logic in one method, or consider a virtual method |

### Debugging

- "this case label is dominated by a preceding case label" → reorder, or merge into a guard.
- "the switch statement/expression does not cover all possible input values" → add the missing type, a total pattern, or `default`.
- `MatchException` → a record accessor threw, or the sealed hierarchy changed after compilation.
- `NullPointerException` from a pattern `switch` → `null` input with no `case null`.
- "incompatible types" in `instanceof` patterns → the static type can never be that type (for example `String` vs `Integer`).

---

## Quick Summary

- A pattern **tests** and **binds** in one step: `obj instanceof String s`.
- `switch` can match **types** with guards (`when`) and `case null`, in order from specific to general, and must be exhaustive.
- **Record patterns** deconstruct (and nest): `case Add(Num(int a), Num(int b))`.
- With **sealed** types, pattern switches need no `default`, so the compiler enforces completeness.
- `_` marks unused variables and patterns (Java 22).
- Versions: `instanceof` 16, `switch` and record patterns 21, `_` 22; primitive patterns are still preview.

**Next:** [`var` and Type Inference](05_var-and-type-inference.md)