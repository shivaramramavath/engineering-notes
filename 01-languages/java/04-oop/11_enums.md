# Enums

An **enum** is a type with a **fixed set of named instances**. It replaces "magic" constants (`int`, `String`) with type-safe values, and each constant can carry data and behavior.

```java
public enum Day { MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY }

Day d = Day.FRIDAY;
if (d == Day.FRIDAY) System.out.println("Almost weekend");
```

## Why not constants?

```java
// Fragile: any int is accepted; no type safety; unreadable in logs
static final int PENDING = 0, PAID = 1, SHIPPED = 2;
void update(int status) { }
update(42);                          // compiles, meaningless

// Type-safe
enum Status { PENDING, PAID, SHIPPED }
void update(Status status) { }
update(Status.PAID);                 // only valid values compile
```

## Enums are classes

An enum is a special class: each constant is a `public static final` **instance**, created once when the enum is loaded. You **cannot** instantiate one with `new` or extend an enum, but it may implement interfaces.

```
enum Status        Status.PENDING ─► [instance]
                   Status.PAID    ─► [instance]       exactly these objects exist
                   Status.SHIPPED ─► [instance]
```

## Built-in methods

| Method | Result |
|--------|--------|
| `values()` | A **new array** of all constants, in declaration order |
| `valueOf("PAID")` | The constant with that exact name; `IllegalArgumentException` if none |
| `name()` | The declared name: `"PAID"` (final) |
| `toString()` | `name()` by default; may be overridden for display |
| `ordinal()` | Zero-based position in the declaration |
| `compareTo(other)` | Compares by ordinal |
| `equals`, `hashCode` | Identity-based (use `==`) |
| `getDeclaringClass()` | The enum type |

```java
for (Status s : Status.values()) System.out.println(s + " " + s.ordinal());
Status s = Status.valueOf("PAID");
Status bad = Status.valueOf("paid");           // IllegalArgumentException (case-sensitive)
```

Compare enums with **`==`**: it is null-safe, type-checked at compile time, and correct because each constant is a singleton.

## Fields, constructors and methods

```java
public enum Planet {
    MERCURY(3.303e+23, 2.4397e6),
    EARTH  (5.976e+24, 6.37814e6);

    private final double mass;            // kg
    private final double radius;          // m

    Planet(double mass, double radius) {  // constructor: implicitly private
        this.mass = mass;
        this.radius = radius;
    }

    public double surfaceGravity() {
        return 6.67300E-11 * mass / (radius * radius);
    }
}

Planet.EARTH.surfaceGravity();            // about 9.8
```

Rules: constants come **first**, separated by commas, ending with `;` if more members follow. The constructor is private (implicitly); fields should be `final`.

### Override `toString` for display, keep `name()` for identity

```java
enum Level {
    LOW("Low priority"), HIGH("High priority");
    private final String label;
    Level(String label) { this.label = label; }
    @Override public String toString() { return label; }
}
```

Do **not** parse `toString()` back into an enum; use `name()` or a stable code field.

## Behavior per constant

### Abstract method with constant-specific bodies

```java
enum Operation {
    ADD("+")      { public double apply(double a, double b) { return a + b; } },
    MULTIPLY("*") { public double apply(double a, double b) { return a * b; } };

    private final String symbol;
    Operation(String symbol) { this.symbol = symbol; }
    public String symbol() { return symbol; }
    public abstract double apply(double a, double b);
}

Operation.ADD.apply(2, 3);               // 5.0
```

This is a compact **Strategy** ([24-design-patterns/03-behavioral/00_strategy.md](../24-design-patterns/03-behavioral/00_strategy.md)), and a polymorphic alternative to `switch`.

### Implementing an interface

```java
interface Discount { double apply(double price); }
enum Season implements Discount {
    SUMMER { public double apply(double p) { return p * 0.9; } },
    WINTER { public double apply(double p) { return p * 0.8; } }
}
```

## `switch` with enums

```java
switch (status) {
    case PENDING: ... break;          // no qualification needed in case labels
    case PAID:    ... break;
    default:      ... 
}

String text = switch (status) {       // switch expression: exhaustive without default if all constants are covered
    case PENDING -> "waiting";
    case PAID    -> "done";
    case SHIPPED -> "on its way";
};
```

The compiler checks that a switch **expression** over an enum covers every constant, which turns "forgot to handle the new value" into a compile error ([12-modern-java/03_switch-expressions.md](../12-modern-java/03_switch-expressions.md)).

## `EnumMap` and `EnumSet`

Specialized, very fast collections for enum keys ([08-collections/11_specialized-collections.md](../08-collections/11_specialized-collections.md)).

```java
Map<Day, List<String>> schedule = new EnumMap<>(Day.class);
schedule.computeIfAbsent(Day.MONDAY, d -> new ArrayList<>()).add("standup");

Set<Day> weekend = EnumSet.of(Day.SATURDAY, Day.SUNDAY);
Set<Day> weekdays = EnumSet.complementOf(weekend);
Set<Day> all = EnumSet.allOf(Day.class);
Set<Day> range = EnumSet.range(Day.MONDAY, Day.WEDNESDAY);
```

Backed by arrays or bit vectors, iterated in declaration order, and more efficient than `HashMap`/`HashSet` for enum keys.

## Looking up by a code

```java
enum HttpStatus {
    OK(200), NOT_FOUND(404), SERVER_ERROR(500);

    private final int code;
    HttpStatus(int code) { this.code = code; }
    public int code() { return code; }

    private static final Map<Integer, HttpStatus> BY_CODE = new HashMap<>();
    static { for (HttpStatus s : values()) BY_CODE.put(s.code, s); }

    public static Optional<HttpStatus> fromCode(int code) {
        return Optional.ofNullable(BY_CODE.get(code));
    }
}
```

Build the lookup map in a `static` block once; calling `values()` repeatedly allocates a new array each time.

## Singleton with an enum

```java
public enum AppConfig {
    INSTANCE;
    private String env = "dev";
    public String env() { return env; }
}
```

The JVM guarantees one instance, even against serialization and reflection attacks ([24-design-patterns/01-creational/00_singleton.md](../24-design-patterns/01-creational/00_singleton.md)).

## Enums and the rest of Java

| Topic | Note |
|-------|------|
| Serialization | Serialized by **name**, so renaming a constant breaks old data |
| JSON, databases | Persist `name()` (or an explicit code), **never** `ordinal()`: inserting or reordering constants silently changes meaning |
| Nested enums | Implicitly `static`; can be declared inside a class or interface |
| Inheritance | An enum implicitly extends `java.lang.Enum` and cannot extend another class |
| Adding constants later | `switch` statements without `default` and persisted data may need updating; switch expressions will flag it at compile time |
| Sealed hierarchies | Use sealed classes/records when each case needs **different data**, enums when it is a **fixed set of identical-shape values** ([12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)) |

## When to use an enum

| Use for | Examples |
|---------|----------|
| Fixed sets known at compile time | Days, order status, roles, directions, log levels |
| Constants with attached data or behavior | `Planet`, `Operation`, HTTP status codes |
| Strategy selection | Pricing rules per tier |
| Keys for maps and sets | `EnumMap<Role, Permissions>` |
| Not for | Values that come from a database or config and change at runtime |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Persisting `ordinal()` | Data corrupts when constants are reordered or inserted | Store `name()` or an explicit code |
| `equals` instead of `==` | Works but `NullPointerException`-prone | `==` |
| `valueOf` on user input | `IllegalArgumentException` | Catch it, or use a lookup returning `Optional` |
| Calling `values()` in a hot loop | A new array each call | Cache it in a `static final` list |
| Parsing `toString()` | Breaks when the display text changes | Parse `name()` or a code field |
| Mutable enum fields | Shared global state | Make fields `final` |
| `switch` statement with no `default` and a new constant added | Silently ignored case | Use a switch expression, or add `default` that throws |
| Using enums for data that changes at runtime | Cannot add values | Use a table or class |
| Huge enums with lots of logic | Hard to maintain | Move behavior to services; keep enums small |
| Using `HashMap` with enum keys | Slower, no ordering | `EnumMap` |

## Key takeaways

- An enum is a class with a fixed set of singleton instances: type-safe, readable, switchable
- Add fields, constructors and methods; use constant-specific bodies or interfaces for per-constant behavior
- Compare with `==`; persist `name()` or a code, never `ordinal()`
- Use `EnumMap`/`EnumSet` for enum-keyed collections, and switch expressions for compile-time exhaustiveness

**Next:** [Nested and Inner Classes](./12_nested-and-inner-classes.md)
