# equals and hashCode

`equals` defines when two objects are **logically equal**; `hashCode` produces an `int` summary used by hash-based collections (`HashMap`, `HashSet`). They form a **pair**: override both, consistently, or hash collections silently misbehave.

```java
Point a = new Point(1, 2);
Point b = new Point(1, 2);
a == b;                // false: different objects (identity)
a.equals(b);           // true once equals is overridden
```

## The defaults

`Object.equals` is identity (`this == other`) and `Object.hashCode` is identity-based. That is correct for **entities with their own identity** (a thread, a connection) and wrong for **value objects** (a point, money, a date range).

| Kind | Equality by | Override? |
|------|-------------|-----------|
| Value object (`Point`, `Money`, `Email`) | State | **Yes** |
| Entity (`User` with an id) | Often the id | Yes, carefully (see below) |
| Resource / service (`Connection`, `Thread`) | Identity | No |

## The `equals` contract

For non-null references `x`, `y`, `z`:

| Property | Meaning |
|----------|---------|
| **Reflexive** | `x.equals(x)` is `true` |
| **Symmetric** | `x.equals(y)` ⇔ `y.equals(x)` |
| **Transitive** | `x.equals(y)` and `y.equals(z)` ⇒ `x.equals(z)` |
| **Consistent** | Repeated calls give the same result unless the compared state changes |
| **Non-null** | `x.equals(null)` is `false` (never throws) |

## The `hashCode` contract

| Rule | Meaning |
|------|---------|
| **Consistent** | Within one run, `hashCode()` returns the same value as long as the fields used in `equals` do not change |
| **Equal objects ⇒ equal hash codes** | If `a.equals(b)` then `a.hashCode() == b.hashCode()` (**must**) |
| Unequal objects *may* share a hash | Collisions are allowed, but fewer is faster |

The reverse is not required: same hash does **not** imply equal.

## A correct implementation

```java
public final class Point {
    private final int x;
    private final int y;

    public Point(int x, int y) { this.x = x; this.y = y; }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;                   // fast path
        if (!(o instanceof Point p)) return false;    // also handles null
        return x == p.x && y == p.y;                  // compare the same fields hashCode uses
    }

    @Override
    public int hashCode() {
        return Objects.hash(x, y);
    }
}
```

Recipe:
1. `if (this == o) return true;`
2. Check the type (`instanceof`, or `getClass()`: see below); this also rejects `null`
3. Compare **significant fields**: primitives with `==`, objects with `Objects.equals`, `float`/`double` with `Float.compare`/`Double.compare`, arrays with `Arrays.equals`/`deepEquals`
4. `hashCode` uses **exactly the same fields** (or a subset): `Objects.hash(...)`

Records and IDEs do this for you ([12-modern-java/01_records.md](../12-modern-java/01_records.md)):

```java
public record Point(int x, int y) { }       // equals, hashCode and toString generated
```

### Fields: what to compare

| Field type | `equals` | `hashCode` |
|------------|----------|------------|
| `int`, `long`, `char`, `boolean`... | `==` | `Integer.hashCode(x)` (what `Objects.hash` does) |
| `double` | `Double.compare(a, b) == 0` | `Double.hashCode(d)` |
| Object | `Objects.equals(a, b)` | `Objects.hashCode(a)` |
| Array | `Arrays.equals(a, b)` | `Arrays.hashCode(a)` |
| `BigDecimal` | Beware: `equals` includes scale (`2.0` ≠ `2.00`) | Normalize first, or use `compareTo` semantic fields |

## What breaks when `hashCode` is missing

```java
class Point {
    final int x, y;
    Point(int x, int y) { this.x = x; this.y = y; }
    @Override public boolean equals(Object o) {
        return o instanceof Point p && x == p.x && y == p.y;
    }
    // hashCode NOT overridden: identity hash
}

Set<Point> set = new HashSet<>();
set.add(new Point(1, 2));
set.contains(new Point(1, 2));         // false (almost always): different identity hashes → different buckets

Map<Point, String> map = new HashMap<>();
map.put(new Point(1, 2), "A");
map.get(new Point(1, 2));              // null
```

```
HashMap lookup:
 1. hashCode() picks the bucket          ◄── wrong hash: wrong bucket, equals never even runs
 2. equals() compares inside the bucket
```

See how hash tables work: [08-collections/08_hashmap-and-hashset.md](../08-collections/08_hashmap-and-hashset.md).

## `instanceof` vs `getClass()`: the inheritance trap

### With `getClass()`: strict, preserves symmetry

```java
@Override public boolean equals(Object o) {
    if (this == o) return true;
    if (o == null || getClass() != o.getClass()) return false;
    Point p = (Point) o;
    return x == p.x && y == p.y;
}
```

A `Point` never equals a subclass instance. Safe and symmetric, but violates strict Liskov substitution (a subclass instance cannot be used where equality with a parent is expected).

### With `instanceof` in a non-final class: can break symmetry

```java
class Point { int x, y; /* equals uses instanceof Point */ }
class ColorPoint extends Point {
    Color color;
    @Override public boolean equals(Object o) {
        return o instanceof ColorPoint cp && super.equals(cp) && color == cp.color;
    }
}

Point p = new Point(1, 2);
ColorPoint cp = new ColorPoint(1, 2, RED);
p.equals(cp);      // true  (instanceof Point passes)
cp.equals(p);      // false (p is not a ColorPoint)   → NOT symmetric
```

There is **no way** to extend an instantiable class with a new value component and keep the `equals` contract (Effective Java, Item 10). Choices:

| Approach | |
|----------|--|
| Make the class `final` (or a record) and use `instanceof` | Simplest and recommended for value classes |
| Use `getClass()` | Strict; subclasses never equal parents |
| Use **composition** instead of inheritance (`ColorPoint` has a `Point`) | Avoids the problem ([10](./10_composition-and-object-relationships.md)) |
| Define equality only in the parent with a `final` `equals` | When subclasses add no equality-relevant state |

## Mutable fields and hash-based collections

If a field used in `hashCode` changes **after** the object is stored, the object is lost:

```java
Set<Point> set = new HashSet<>();
Point p = new MutablePoint(1, 2);
set.add(p);
p.setX(99);                       // hash changes
set.contains(p);                  // false: it sits in the old bucket
set.remove(p);                    // false: leaks forever
```

| Rule | |
|------|--|
| Use **immutable** fields in `equals`/`hashCode` | Best |
| Never mutate objects while they are keys or set elements | |
| Do not use mutable collections, arrays or `StringBuilder` as keys | |

## Entities: equals by id?

```java
class User {
    private Long id;               // assigned by the database after save
    private String name;
    @Override public boolean equals(Object o) {
        return o instanceof User u && id != null && id.equals(u.id);
    }
    @Override public int hashCode() { return getClass().hashCode(); }    // constant, stable across the id being assigned
}
```

Persistent entities are a known grey area: an id that changes from `null` to a value changes `hashCode` if you hash it. Common safe options: a **natural/business key** that never changes, a client-generated id (UUID) set at construction, or a constant `hashCode` per class (correct but slower for huge sets). See [16-jdbc-and-databases/07_from-jdbc-to-orm.md](../16-jdbc-and-databases/07_from-jdbc-to-orm.md).

## Consistency with `Comparable` and sorted collections

`TreeSet`/`TreeMap` use `compareTo`/`Comparator`, **not** `equals`. A comparator inconsistent with `equals` makes sorted collections treat "equal-comparing" elements as duplicates even when `equals` says otherwise.

```java
Set<BigDecimal> hs = new HashSet<>(List.of(new BigDecimal("2.0"), new BigDecimal("2.00")));
hs.size();     // 2 (equals differs by scale)
Set<BigDecimal> ts = new TreeSet<>(hs);
ts.size();     // 1 (compareTo says equal)
```

Recommended: make `compareTo` consistent with `equals` ([08-collections/05_comparable-and-comparator.md](../08-collections/05_comparable-and-comparator.md)).

## Quality of `hashCode`

| Aspect | Guidance |
|--------|----------|
| Distribution | Spread values; do not return a constant (legal, but turns a hash map into a list) |
| Cost | Cheap; cache in a `private int hash` field for large immutable objects (`String` does this) |
| `Objects.hash(...)` | Convenient; boxes primitives and allocates a varargs array: fine for most code, avoid in hot paths |
| Manual formula | `31 * result + field.hashCode()` style; the IDE can generate it |
| Stability across JVM runs | **Not guaranteed**: never persist or transmit hash codes |
| Collisions | Handled by `equals`; Java's `HashMap` converts large colliding buckets into trees |

## Testing your implementation

```java
@Test void equalsAndHashCodeContract() {
    Point a = new Point(1, 2), b = new Point(1, 2), c = new Point(1, 2), d = new Point(3, 4);
    assertEquals(a, a);                                         // reflexive
    assertEquals(a, b); assertEquals(b, a);                     // symmetric
    assertEquals(a, b); assertEquals(b, c); assertEquals(a, c); // transitive
    assertNotEquals(a, d);
    assertNotEquals(a, null);
    assertEquals(a.hashCode(), b.hashCode());
}
```

Libraries such as EqualsVerifier test the contract exhaustively ([19-testing](../19-testing/README.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Overriding `equals` without `hashCode` | Lookups in `HashMap`/`HashSet` fail | Override both, same fields |
| `equals(Point other)` instead of `equals(Object other)` | **Overloads**, not overrides; collections ignore it | Use `Object` and `@Override` |
| Not handling `null` | `NullPointerException` | `instanceof` / `Objects.equals` |
| Comparing `double`/`float` with `==` | NaN and `-0.0` oddities | `Double.compare` |
| Comparing arrays with `equals` | Identity comparison | `Arrays.equals` |
| Using different fields in `equals` and `hashCode` | Equal objects with different hashes | Same fields (hash may use a subset) |
| Including mutable fields | Objects vanish from sets and maps | Immutable fields |
| `instanceof` in a non-final class with extra state | Broken symmetry | `final` class, `getClass()`, or composition |
| `hashCode` returning a constant everywhere | Poor performance | Combine field hashes |
| Persisting or comparing `hashCode` values across runs | Wrong results | Do not rely on specific values |
| Using `equals` on `BigDecimal` for numeric equality | `2.0` ≠ `2.00` | `compareTo` |
| Forgetting `final` on the class in a record-like value type | Symmetry bugs | `final` or record |

## Key takeaways

- Override `equals` and `hashCode` **together**, using the **same fields**
- `equals` must be reflexive, symmetric, transitive, consistent and `null`-safe; equal objects must have equal hash codes
- Use `instanceof` with `final` classes/records, `getClass()` otherwise, and prefer composition over extending value classes
- Keep the fields used in them **immutable**
- Prefer records, or let the IDE generate the methods, and test the contract

**Next:** [Clone and Copying](./15_clone-and-copying.md)
