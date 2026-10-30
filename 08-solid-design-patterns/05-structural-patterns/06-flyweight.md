# Flyweight

**Intent:** use sharing to support large numbers of fine-grained objects efficiently.

When you need huge numbers of similar objects, much of each object's data is often identical. Flyweight stores that common part **once** and shares it, so memory use drops sharply.

## The problem

A game or map renders 1,000,000 trees. Each `Tree` stores:

- a name, a color, and a texture image (several megabytes, identical for all oaks)
- its own position (x, y) and size

```java
class Tree {
    String species;
    byte[] texture;     // large, identical for every tree of the same species
    int x, y;           // different for every tree
}
```

A million trees means a million copies of the texture. Memory runs out.

## Key idea: intrinsic vs extrinsic state

| State | Meaning | Where it lives |
|---|---|---|
| **Intrinsic** | Shared, context-independent, identical across many objects | Inside the flyweight (shared) |
| **Extrinsic** | Varies per use; depends on context | Passed in by the client, or stored outside |

```text
Tree
  intrinsic: species, texture           → shared flyweight (few objects)
  extrinsic: x, y                       → per tree (cheap)
```

## Structure

```text
Client ──► FlyweightFactory ──► pool: Map<key, Flyweight>
   │                                     │
   │ keeps extrinsic state               ▼
   └───────────────────────► Flyweight.operation(extrinsicState)
```

- **Flyweight**: holds intrinsic state, and takes extrinsic state as method parameters.
- **FlyweightFactory**: creates and caches flyweights, returning an existing one when possible.
- **Client**: stores or computes the extrinsic state.

## Example

```java
// Flyweight: immutable, shared
public final class TreeType {
    private final String species;
    private final byte[] texture;

    public TreeType(String species, byte[] texture) {
        this.species = species;
        this.texture = texture;
    }

    public void draw(Canvas canvas, int x, int y) {      // x and y are extrinsic
        canvas.drawImage(texture, x, y);
    }
}

// Factory: ensures sharing
public class TreeTypeFactory {
    private static final Map<String, TreeType> cache = new HashMap<>();

    public static TreeType get(String species) {
        return cache.computeIfAbsent(species, s -> new TreeType(s, loadTexture(s)));
    }
}

// Context: small, holds extrinsic state plus a reference to the flyweight
public class Tree {
    private final int x, y;
    private final TreeType type;

    public Tree(int x, int y, String species) {
        this.x = x;
        this.y = y;
        this.type = TreeTypeFactory.get(species);
    }

    public void draw(Canvas canvas) { type.draw(canvas, x, y); }
}
```

A million trees now share a handful of `TreeType` objects (one per species). Each `Tree` costs only two ints and a reference.

## Rules that make it work

- Flyweights **must be immutable** (or at least their intrinsic state must be). If one client modifies shared state, all users are affected.
- Always obtain flyweights through the factory, never with `new`.
- Extrinsic state must be supplied per call or kept by the client.
- In concurrent code, the factory's cache must be thread-safe (use `ConcurrentHashMap`, or `computeIfAbsent` on a concurrent map).

## In the Java standard library

| Example | Detail |
|---|---|
| `Integer.valueOf(int)` | Caches `Integer` objects for values from -128 to 127 (by default), so equal small values are the same object |
| String interning / the string pool | Identical string literals share one instance |
| `Boolean.TRUE` / `Boolean.FALSE` | Shared constants |

This is also why comparing boxed integers with `==` gives surprising results: values in the cache are identical objects, and values outside it are not. Always use `equals`.

## When to use it

Use it only when **all** of these hold:

- You have a very large number of objects (typically thousands to millions).
- Storage cost is a measured problem.
- Much of the objects' state can be made extrinsic or shared.
- Object identity doesn't matter (clients can't rely on `==`).

Typical uses: characters in a text editor, tiles or particles in games, map markers, glyphs, cached configuration objects.

## Trade-offs

| Benefits | Costs |
|---|---|
| Large memory savings | More complex code: splitting intrinsic and extrinsic state |
| Fewer objects, less GC pressure | Extrinsic state must be computed or passed on every call, which costs CPU |
| | Shared mutable state becomes a hazard |
| | Factory lookups add overhead |

It trades CPU and complexity for memory. Measure before adopting it.

## Common mistakes

- **Applying it without measuring**, adding complexity to code that was never a memory problem.
- **Mutable flyweights**, causing changes to leak across unrelated users.
- **Mixing intrinsic and extrinsic state** in the same class.
- **Creating flyweights directly**, bypassing the cache.
- **Unbounded caches** that grow forever with unique keys, becoming a memory leak.
- **Relying on object identity** (`==`) for equality.

## Quick Summary

- Flyweight shares the common part of many similar objects to save memory.
- Split state into intrinsic (shared, immutable) and extrinsic (per use).
- A factory guarantees sharing; flyweights must be immutable.
- Use only for large object counts with real, measured memory pressure.

## Related

[Singleton](../04-creational-patterns/01-singleton.md) (one instance; flyweight is one instance *per key*) · [Prototype](../04-creational-patterns/05-prototype.md) · [Composite](./03-composite.md) (leaves are often flyweights)

## Next

[07-proxy.md](./07-proxy.md)
