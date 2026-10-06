# Records

A **record** is a concise way to declare a class whose only job is to **carry immutable data**. You list the components, and the compiler generates the constructor, accessors, `equals`, `hashCode`, and `toString`.

```java
public record Point(int x, int y) {}
```

That one line replaces roughly 30 lines of class boilerplate (private final fields, constructor, getters, `equals`, `hashCode`, `toString`), and unlike generated or Lombok-style code, the semantics are **defined by the language**: records are *transparent* carriers of their components.

Records became final in **Java 16** (preview in 14 and 15).

**Prerequisites:** [Classes and Objects](../04-oop/00_classes-and-objects.md), [Constructors](../04-oop/01_constructors.md), [equals and hashCode](../04-oop/14_equals-and-hashcode.md), [Immutability](../23-design-and-clean-code/04_immutability.md).

---

## 1. What you get

```java
var p = new Point(3, 4);

p.x();                          // 3        accessor: x(), not getX()
p.y();                          // 4
p.toString();                   // Point[x=3, y=4]
p.equals(new Point(3, 4));      // true     component-wise equality
p.hashCode() == new Point(3, 4).hashCode();   // true
```

What the compiler generates for `record Point(int x, int y)`:

| Member | Behavior |
|---|---|
| `private final int x, y` | One field per component |
| Canonical constructor `Point(int x, int y)` | Assigns every field |
| Accessors `x()`, `y()` | Return the field (same name as the component) |
| `equals` / `hashCode` | Based on all components |
| `toString` | `Point[x=3, y=4]` |

The record class itself is implicitly `final` and extends `java.lang.Record`.

---

## 2. Validating and normalizing: the compact constructor

To check or adjust values, write a **compact constructor**: the parameter list is omitted, and the field assignments happen automatically at the end.

```java
public record Range(int lo, int hi) {
    public Range {                                   // no parentheses
        if (lo > hi) {
            throw new IllegalArgumentException("lo > hi: " + lo + " > " + hi);
        }
    }
}

public record Email(String value) {
    public Email {
        value = value.strip().toLowerCase();         // reassign the PARAMETER; the field gets it afterwards
        if (!value.contains("@")) throw new IllegalArgumentException("Invalid email: " + value);
    }
}
```

Rules:

- Inside a compact constructor you assign to the **parameters** (`value = ...`), never to `this.value`. Direct field assignment is a compile error.
- Validation here means *every* way of creating the record is checked, including deserialization (a big advantage over ordinary classes; see [Serialization](../11-io-and-networking/03_java-serialization.md)).

### Other constructors

Any additional constructor must delegate to another constructor, ultimately the canonical one:

```java
public record Point(int x, int y) {
    public Point() { this(0, 0); }                    // convenience constructor

    public static Point origin() { return new Point(0, 0); }   // static factory
}
```

You can also write the full canonical constructor explicitly (with a parameter list), but then you must assign every field yourself. The compact form is almost always better.

---

## 3. Adding behavior

Records can have methods, static members, and implement interfaces:

```java
public record Point(int x, int y) implements Comparable<Point> {

    public static final Point ORIGIN = new Point(0, 0);

    public double distanceTo(Point other) {
        return Math.hypot(x - other.x, y - other.y);
    }

    public Point withX(int newX) { return new Point(newX, y); }   // "wither": records have no built-in one

    @Override public int compareTo(Point o) { return Integer.compare(x * x + y * y, o.x * o.x + o.y * o.y); }
}
```

You can **override** accessors, `equals`, `hashCode`, and `toString`, but an accessor must be `public`, have the same name and return type, and normally just return the component. Don't override them to return something different; that breaks the "transparent carrier" contract.

### Generic and nested records

```java
public record Pair<A, B>(A first, B second) {}

Pair<String, Integer> p = new Pair<>("age", 30);
```

```java
List<String> topNames(List<Person> people) {
    record Scored(String name, int score) {}         // a local record: declared inside the method, before use

    return people.stream()
        .map(pe -> new Scored(pe.name(), score(pe))) // a tiny ad-hoc tuple
        .sorted(Comparator.comparingInt(Scored::score).reversed())
        .map(Scored::name)
        .toList();
}
```

Records declared inside another class (or inside a method) are **implicitly `static`**: they can't capture the outer instance.

---

## 4. What records cannot do

| Restriction | Why |
|---|---|
| Can't `extend` another class | They already extend `java.lang.Record` |
| Can't be `abstract` or have non-`final` fields | A record *is* its components |
| No instance fields other than the components | The state is fully described by the header |
| No instance initializer blocks | Use the compact constructor |
| Can't be subclassed (`final`) | Equality must mean "same components" |

Records **can** implement interfaces (including `sealed` ones: see [Sealed Classes](02_sealed-classes.md)), carry annotations, and have static fields and methods.

---

## 5. Immutability is shallow

The *fields* of a record are final, but a component that refers to a mutable object is still mutable:

```java
public record Team(String name, List<String> members) {}

var members = new ArrayList<>(List.of("Asha", "Ravi"));
var team = new Team("Core", members);

members.add("Intruder");
team.members().add("Another");        // both modify the record's state!
```

Make the record truly immutable by copying in the compact constructor:

```java
public record Team(String name, List<String> members) {
    public Team {
        members = List.copyOf(members);   // unmodifiable copy (also rejects null elements)
    }
}
```

`List.copyOf` returns an unmodifiable list, so both the caller's later changes and `team.members().add(...)` are prevented. For mutable types without an immutable equivalent (arrays, `Date`), copy defensively in the constructor *and* in the accessor ([Defensive Programming](../23-design-and-clean-code/05_defensive-programming.md)).

**Arrays are especially bad components:** record `equals` uses `equals` on each component, and arrays use reference equality, so two records holding equal-content arrays are *not* equal, and `toString` prints `[I@1b6d3586`. Use a `List` instead.

---

## 6. Equality details

- Reference components are compared with `Objects.equals`, so they need good `equals` themselves. Primitive components are compared by value (floating-point values via `Double.compare`/`Float.compare` semantics, so `NaN` equals `NaN`).
- `hashCode` is consistent with `equals`, but the exact algorithm is unspecified. Don't depend on specific hash values.
- Because equality is purely by value, records work well as `Map` keys and `Set` elements, provided their components are themselves stable.

---

## 7. Practical usage

**Good fits**

- **DTOs and API responses**: `record UserDto(long id, String name)`
- **Value objects**: `Money(BigDecimal amount, Currency currency)`, `Email`, `OrderId`, with validation in the compact constructor
- **Multiple return values**: instead of a one-off class or `Map.Entry`
- **Map keys / composite keys**: `record CacheKey(String tenant, long id)`
- **Algebraic data types** with sealed interfaces: [Sealed Classes](02_sealed-classes.md)
- **Pattern matching and deconstruction**: [Pattern Matching](04_pattern-matching.md)

**Poor fits**

- Objects with **mutable state or identity**, such as entities with lifecycle and setters.
- **JPA/Hibernate entities** (they need non-final classes and mutable state). Records are fine for query projections and DTOs.
- Types that need **inheritance** or a hidden representation different from the constructor arguments.
- Classes with lots of optional parameters. A record has no builder or default values, so you need a builder or factory methods.

### Library support

- **Jackson** supports records (2.12+) for both serialization and deserialization ([Jackson](../17-json-and-data-formats/01_jackson.md)).
- **Java serialization** treats records specially: only components are serialized and the canonical constructor runs on read ([Serialization](../11-io-and-networking/03_java-serialization.md)).
- **Reflection:** `Class.isRecord()` and `Class.getRecordComponents()` ([Reflection](../13-advanced-language-features/01_reflection.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Calling `p.getX()` | Accessors are `x()` |
| Assuming a record is deeply immutable | `List.copyOf` / defensive copies in the compact constructor |
| `this.x = x` in a compact constructor | Assign the parameter: `x = ...` |
| Array components | Use `List` (arrays break `equals`/`toString`) |
| Putting heavy logic or mutable state into a record | Use a regular class |
| Using a record as a JPA entity | Use a class for the entity, a record for the DTO |
| Expecting a "copy with one field changed" | Write `withX(...)` methods yourself |
| Overriding accessors to compute something different | Add a separate method instead |
| Adding a field in the body (`private int cache;`) | Not allowed: derive values in methods, or use a class |

### Debugging

- `error: field declaration must be static` → you tried to add an instance field. Only static fields are allowed in the body.
- Compiler complains about an "invalid canonical constructor" → an explicit canonical constructor must be at least as accessible as the record itself (make it `public` for a `public` record).
- Jackson can't deserialize a record → check the Jackson version (2.12+), and that parameter names are available (compile with `-parameters` or use `@JsonProperty`), per your setup.
- Two "equal-looking" records not equal → check for array components or mutable components with weak `equals`.

---

## Quick Summary

- `record Name(Type a, Type b) {}` gives you a final class with final fields, a canonical constructor, accessors (`a()`), `equals`, `hashCode`, and `toString`.
- Customize with a **compact constructor** (validate, normalize) and add methods, statics, and interfaces. No other instance fields and no inheritance.
- Immutability is **shallow**. Copy mutable components in the constructor. Avoid arrays.
- Records are ideal for DTOs, value objects, composite keys, tuples, and sealed hierarchies; they are a poor fit for mutable, identity-based objects like JPA entities.
- Final since Java 16.

**Next:** [Sealed Classes](02_sealed-classes.md)
