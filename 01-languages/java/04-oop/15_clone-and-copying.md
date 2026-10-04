# Clone and Copying

Assigning an object variable copies the **reference**, not the object. To get an independent object you must **copy** it, and you must decide **how deep** the copy goes. Java's built-in `clone()` mechanism is notoriously awkward; this file explains why and what to use instead.

```java
Person a = new Person("Ada", new Address("London"));
Person b = a;                     // NOT a copy: same object
b.setName("Grace");
a.getName();                      // "Grace"
```

## Shallow vs deep copy

```
Original                 Shallow copy                Deep copy
┌─────────┐             ┌─────────┐                 ┌─────────┐
│ name ───┼─► "Ada"     │ name ───┼─► "Ada"         │ name ───┼─► "Ada"        (immutable: sharing is fine)
│ address─┼─► [London]  │ address─┼─┐               │ address─┼─► [London]'    (new, independent Address)
└─────────┘             └─────────┘ │               └─────────┘
                        both objects share ──► [London]
```

| | Shallow copy | Deep copy |
|---|--------------|-----------|
| Copies | The object and its **fields** (references are copied as references) | The object **and** everything reachable and mutable from it |
| Shared afterwards | The referenced objects | Nothing mutable |
| Danger | Changing a shared mutable part changes both copies | More code, more cost, cycles |
| Fine when | All referenced objects are **immutable** (`String`, wrappers, records of immutables) | Mutable nested state exists |

```java
Person copy = shallowCopy(a);
copy.getAddress().setCity("Paris");
a.getAddress().getCity();         // "Paris": the Address was shared
```

Immutable objects need no copying at all: share them freely ([03_final.md](./03_final.md), [23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md)).

## `Object.clone()` and `Cloneable`

```java
class Sheep implements Cloneable {
    String name;
    int[] tags = {1, 2, 3};

    @Override
    public Sheep clone() {
        try {
            return (Sheep) super.clone();          // field-by-field SHALLOW copy
        } catch (CloneNotSupportedException e) {
            throw new AssertionError(e);           // cannot happen: we implement Cloneable
        }
    }
}
```

How it works:
- `Cloneable` is a **marker interface** with no methods. If a class does not implement it, `super.clone()` throws `CloneNotSupportedException`
- `Object.clone()` is `protected` and returns `Object`; you override it (and normally widen it to `public` with a covariant return)
- It creates the new object **without calling a constructor** and copies fields bitwise

### Why `clone()` is considered broken

| Problem | Detail |
|---------|--------|
| Marker interface changes the behavior of a method in another class | `Cloneable` has no `clone()` method; the contract is "extra-linguistic" |
| **Bypasses constructors** | Invariants established in constructors are not enforced |
| **Shallow by default** | Arrays are cloned shallowly, mutable fields are shared unless you fix them by hand |
| Conflicts with `final` fields | A `final` field cannot be reassigned to a deep copy |
| Checked `CloneNotSupportedException` | Noise: callers must handle it |
| Hard to get right in inheritance | Every class in the chain must cooperate and call `super.clone()` |
| Not thread-safe or consistent by itself | Needs synchronization if the class is shared |
| Return type is `Object` | Casts needed unless overridden |

Josh Bloch's advice (*Effective Java*, Item 13): **avoid `clone`**, prefer copy constructors or copy factories. The exception: **arrays**, where `clone()` is the idiomatic, correct way to copy.

```java
int[] a = {1, 2, 3};
int[] b = a.clone();               // independent copy of primitives (no cast needed since Java 5)
```

### Getting `clone()` right (if you must)

```java
class Team implements Cloneable {
    private String name;
    private List<String> members = new ArrayList<>();
    private int[] scores = {0, 0};

    @Override
    public Team clone() {
        try {
            Team copy = (Team) super.clone();              // 1. shallow copy
            copy.members = new ArrayList<>(members);       // 2. deep-copy every mutable field
            copy.scores = scores.clone();
            return copy;
        } catch (CloneNotSupportedException e) {
            throw new AssertionError(e);
        }
    }
}
```

All mutable fields (not `final`) must be re-copied; subclasses must call `super.clone()` and fix their own fields.

## Better alternatives

### 1. Copy constructor

```java
public class Person {
    private final String name;
    private final Address address;

    public Person(String name, Address address) { this.name = name; this.address = address; }

    public Person(Person other) {                          // copy constructor
        this.name = other.name;                            // String is immutable: share
        this.address = new Address(other.address);         // Address is mutable: copy
    }
}
```

Explicit, type-safe, works with `final` fields and runs constructor validation ([01_constructors.md](./01_constructors.md)). Downside: the declared type is copied, so it cannot copy a subclass through a parent reference (no polymorphic copy).

### 2. Copy factory (static method)

```java
public static Person copyOf(Person other) { return new Person(other); }
List<String> copy = List.copyOf(original);              // JDK style: immutable copy
```

### 3. Polymorphic copy via an abstract method

```java
abstract class Shape { abstract Shape copy(); }
class Circle extends Shape {
    final double r;
    Circle(double r) { this.r = r; }
    @Override Circle copy() { return new Circle(r); }
}
```

Each subclass knows how to copy itself, so copying works through a `Shape` reference.

### 4. Immutable "wither" methods

```java
public record Point(int x, int y) {
    public Point withX(int newX) { return new Point(newX, y); }   // a modified copy
}
```

For immutable types, "changing" means creating a new object, and defensive copying disappears.

### 5. Serialization-based deep copy

```java
// Write the object graph to bytes and read it back (all classes must implement Serializable)
```

Easy for deep graphs, but **slow**, fragile (transient fields, non-serializable parts), and a security risk if the bytes are ever untrusted ([21-security/04_insecure-deserialization.md](../21-security/04_insecure-deserialization.md)). Prefer explicit copying. JSON mapping libraries (Jackson `convertValue`) can also deep-copy simple data objects ([17-json-and-data-formats](../17-json-and-data-formats/README.md)), with similar caveats.

## Copying arrays and collections

| Goal | Code | Depth |
|------|------|-------|
| Copy a 1D array | `arr.clone()`, `Arrays.copyOf(arr, n)`, `System.arraycopy` | Elements copied as-is (shallow for object arrays) |
| Copy a 2D array | Copy each row: `for (...) copy[i] = grid[i].clone();` | `grid.clone()` copies only the row **references** |
| Copy a list | `new ArrayList<>(list)`, `List.copyOf(list)` | Shallow: elements shared |
| Copy a map | `new HashMap<>(map)`, `Map.copyOf(map)` | Shallow |
| Deep copy of a list of mutable objects | `list.stream().map(Person::new).collect(Collectors.toList())` | Deep, using the copy constructor |

```java
int[][] grid = {{1, 2}, {3, 4}};
int[][] shallow = grid.clone();
shallow[0][0] = 99;
grid[0][0];                              // 99: row arrays are shared

int[][] deep = new int[grid.length][];
for (int i = 0; i < grid.length; i++) deep[i] = grid[i].clone();
```

More: [01-fundamentals/07_arrays.md](../01-fundamentals/07_arrays.md), [08-collections/13_immutable-and-unmodifiable-collections.md](../08-collections/13_immutable-and-unmodifiable-collections.md).

## Defensive copies

Copy mutable arguments **on the way in** and mutable internals **on the way out**, so callers cannot change your state:

```java
public class Booking {
    private final Date start;                           // Date is mutable (legacy)
    private final List<String> guests;

    public Booking(Date start, List<String> guests) {
        this.start = new Date(start.getTime());          // copy in
        this.guests = new ArrayList<>(guests);           // copy in
    }
    public Date start() { return new Date(start.getTime()); }   // copy out
    public List<String> guests() { return List.copyOf(guests); }
}
```

Use immutable types (`java.time.LocalDate` instead of `Date`) and the copies become unnecessary ([10-date-and-time](../10-date-and-time/README.md), [encapsulation](./04_encapsulation-and-access-modifiers.md), [pass-by-value](../02-methods/01_pass-by-value.md)).

## Choosing an approach

| Situation | Use |
|-----------|-----|
| Immutable object | Do not copy; share it |
| Array of primitives | `arr.clone()` / `Arrays.copyOf` |
| Collection of immutable elements | `List.copyOf` / `new ArrayList<>(c)` |
| Class with mutable fields | Copy constructor or copy factory |
| Hierarchy that must copy through a base-type reference | Abstract `copy()` method |
| "Modified version" of an immutable value | Wither methods, record + `with...` |
| Inheriting from a class that already implements `clone()` | Follow its contract (`super.clone()`) |
| Whole object graph, one-off, non-hot path | Serialization or JSON copy (with caution) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `b = a` thinking it copies | Changes appear in both | Create a copy |
| `list2 = list1` or `new ArrayList<>(list1)` thinking it deep-copies elements | Shared mutable elements | Copy each element too |
| Relying on `clone()` for mutable fields without fixing them | Shared state between "copies" | Deep-copy fields, or avoid `clone` |
| Implementing `clone()` without `Cloneable` | `CloneNotSupportedException` | Implement the marker interface (or avoid `clone`) |
| `super.clone()` omitted in a subclass | `ClassCastException` or wrong type | Always start with `super.clone()` |
| Cloning a 2D array with `grid.clone()` | Rows still shared | Clone each row |
| Copy constructors on non-final classes copying only the base part | Subclass data lost (slicing) | `final` classes or a polymorphic `copy()` |
| Returning internal mutable state directly | Callers break invariants | Return copies or immutable views |
| Deep copy of cyclic structures with naive recursion | `StackOverflowError` | Track visited objects, or redesign |
| Using serialization for routine copying | Slow, fragile, security risk | Explicit copy code |
| Expecting `clone()` to run constructors | Invariants and initializers skipped | Use a constructor-based copy |

## Key takeaways

- Assignment copies references; a real copy needs new objects
- **Shallow** copies share nested mutable objects; **deep** copies do not; immutable parts can always be shared
- `Object.clone()` is awkward (marker interface, no constructors, shallow, checked exception): use it only for arrays
- Prefer copy constructors, copy factories (`List.copyOf`) and immutability
- Make defensive copies at API boundaries

**Next:** [05-packages-and-modules](../05-packages-and-modules/README.md)
