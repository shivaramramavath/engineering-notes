# Prototype

**Intent:** create new objects by **copying an existing object** (the prototype) instead of building them from scratch.

It is useful when creating an object is expensive or complicated, or when you want many variations of a pre-configured object without a subclass for each.

## The problem

- Constructing the object is costly (loading data, many setup steps), but copies of a configured instance are cheap.
- The code creating objects shouldn't depend on their concrete classes, and only knows it has "something that can copy itself".
- You need many objects that differ only slightly from a few standard configurations.

## Structure

```text
Prototype (interface)
  + copy(): Prototype
        ▲
        │
ConcretePrototype
  + copy()  → returns a new object with the same state

Client:  Prototype clone = original.copy();
```

## Example: copy method

```java
public interface Shape {
    Shape copy();
}

public class Circle implements Shape {
    private int radius;
    private String color;
    private final List<String> tags;

    public Circle(int radius, String color, List<String> tags) {
        this.radius = radius;
        this.color = color;
        this.tags = new ArrayList<>(tags);
    }

    // Copy constructor
    private Circle(Circle other) {
        this.radius = other.radius;
        this.color = other.color;
        this.tags = new ArrayList<>(other.tags);   // deep copy of mutable state
    }

    @Override public Shape copy() { return new Circle(this); }

    public void setColor(String color) { this.color = color; }
}

Shape template = new Circle(10, "red", List.of("default"));
Shape variant = template.copy();       // independent copy
```

The client calls `copy()` without knowing the concrete class, which is the core of the pattern.

## Prototype registry

Keep a set of ready-made prototypes and copy the one you need.

```java
public class ShapeRegistry {
    private final Map<String, Shape> prototypes = new HashMap<>();

    public void register(String key, Shape prototype) { prototypes.put(key, prototype); }

    public Shape create(String key) {
        Shape prototype = prototypes.get(key);
        if (prototype == null) throw new IllegalArgumentException("Unknown: " + key);
        return prototype.copy();
    }
}

registry.register("small-red-circle", new Circle(5, "red", List.of()));
Shape s = registry.create("small-red-circle");
```

This replaces a hierarchy of subclasses that differ only in their initial values.

## Shallow vs deep copy

The most important thing to get right.

| Copy type | What it does | Risk |
|---|---|---|
| Shallow | Copies field values; references still point to the **same** nested objects | Changing a nested object in the copy changes the original |
| Deep | Copies nested mutable objects too | Correct, but costs more and needs care with cycles |

```java
// Shallow: the lists are shared
this.tags = other.tags;                    // bug: copy and original share one list

// Deep: independent list
this.tags = new ArrayList<>(other.tags);   // each has its own list (strings are immutable)
```

Immutable fields (`String`, `Integer`, records of immutables) are safe to share. Only copy mutable ones.

## Java's built-in `clone()`: avoid it

Java has `Cloneable` and `Object.clone()`, and they are widely considered flawed:

- `Cloneable` is a marker interface with **no `clone` method**; `Object.clone()` is `protected`.
- The default behavior is a **shallow** copy.
- It bypasses constructors, so invariants may not be established.
- It interacts badly with `final` fields and subclassing.

Prefer a **copy constructor** or a **copy factory method**:

```java
public Circle(Circle other) { ... }              // copy constructor
public static Circle copyOf(Circle other) { ... } // copy factory
```

These are explicit, type-safe and easy to reason about.

## Records and immutability

If an object is immutable, "copying with a change" is done by creating a new one, and prototype is rarely needed. Records support this style with a helper method:

```java
public record Settings(String theme, int fontSize) {
    public Settings withFontSize(int size) { return new Settings(theme, size); }
}

Settings bigger = defaults.withFontSize(18);
```

## When to use it

- Object creation is expensive and copying is cheaper.
- Many near-identical objects with small differences (game units, document templates, test fixtures).
- You want to create objects at runtime from configured examples, without knowing their classes.

Skip it when objects are small and cheap to construct, or when the object graph has complex shared references and cycles that make correct copying difficult.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Avoids repeating expensive construction | Deep copying can be tricky and error-prone |
| Hides concrete classes from the client | Each class must implement copying correctly |
| Replaces subclasses that differ only by state | Shallow-copy bugs are subtle |

## Common mistakes

- **Shallow-copying mutable fields**, leaving two objects sharing state.
- **Using `Object.clone()`** without understanding its pitfalls.
- **Forgetting to update the copy method** when new fields are added. Cover it with a test that compares all fields.
- **Copying objects that hold external resources** (connections, file handles), which must not be duplicated.
- **Copying when a new object would be simpler** or equally cheap.

## Debugging

| Symptom | Likely cause |
|---|---|
| Editing a copy changes the original | Shallow copy of a mutable field |
| Copy is missing a recent field | Copy method not updated after adding a field |
| Copy behaves differently in a subclass | Copy method not overridden, so it returns the parent type |

## Quick Summary

- Prototype creates objects by copying a configured instance.
- Use copy constructors or copy methods, and watch the shallow vs deep distinction.
- Avoid `Cloneable` and `clone()` in new code.
- Often replaced by immutable objects with `with...` methods, or by builders.

## Related

[Builder](./04-builder.md) · [Memento](../06-behavioral-patterns/09-memento.md) (also copies state) · [Factory Method](./02-factory-method.md)

## Next

Structural patterns: [../05-structural-patterns/00-overview.md](../05-structural-patterns/00-overview.md)
