# Composite

**Intent:** compose objects into tree structures to represent part-whole hierarchies, and let clients treat individual objects and compositions of objects **uniformly**.

## The problem

Some data is naturally a tree: files and folders, UI components containing other components, menus with submenus, organization charts, product bundles.

Without the pattern, client code has to check what it's looking at:

```java
long totalSize(Object node) {
    if (node instanceof File f) return f.size();
    long sum = 0;
    for (Object child : ((Directory) node).children()) sum += totalSize(child);
    return sum;
}
```

Every operation repeats this type check and recursion.

## Structure

```text
Component (interface)
  + size()
      ▲
      ├─ Leaf (File)           size() → its own size
      └─ Composite (Directory) size() → sum of children's size()
            holds: List<Component> children
```

- **Component**: common interface for leaves and composites.
- **Leaf**: an object with no children.
- **Composite**: holds children (leaves or other composites) and delegates to them.

## Example

```java
public interface FileSystemItem {
    String name();
    long size();
}

public class File implements FileSystemItem {
    private final String name;
    private final long bytes;

    public File(String name, long bytes) { this.name = name; this.bytes = bytes; }

    public String name() { return name; }
    public long size() { return bytes; }
}

public class Directory implements FileSystemItem {
    private final String name;
    private final List<FileSystemItem> children = new ArrayList<>();

    public Directory(String name) { this.name = name; }

    public Directory add(FileSystemItem item) { children.add(item); return this; }

    public String name() { return name; }

    public long size() {
        return children.stream().mapToLong(FileSystemItem::size).sum();
    }
}

FileSystemItem root = new Directory("root")
        .add(new File("a.txt", 100))
        .add(new Directory("docs")
                .add(new File("b.pdf", 500))
                .add(new File("c.pdf", 250)));

root.size();   // 850: the caller doesn't care whether root is a file or a folder
```

The client calls `size()` on any node. The recursion lives inside `Directory`, where it belongs.

## Transparency vs safety

Where should child-management methods (`add`, `remove`) live?

| Approach | Put `add`/`remove` on | Trade-off |
|---|---|---|
| **Transparent** | `Component` | Uniform interface, but leaves must reject the call (throw), which hurts type safety and [Liskov](../02-solid-principles/03-liskov-substitution.md) |
| **Safe** | `Composite` only | Safer, but the client must know whether it holds a composite to add children |

The safe version (as in the example above, where `add` exists only on `Directory`) is usually preferable in Java, because the type system prevents invalid calls.

## Where you'll see it

- File systems and directory trees.
- UI component trees (a container holding buttons and other containers).
- Organization charts, category trees, menus.
- Expression trees (binary operators containing operands or other expressions).
- Document structures (sections containing paragraphs and subsections).

## Operations over the tree

Anything that applies recursively fits naturally: size, price of a bundle, rendering, searching, counting.

```java
// Price of a bundle of products or other bundles
interface Priceable { Money price(); }

class Product implements Priceable { /* own price */ }
class Bundle implements Priceable {
    private final List<Priceable> items;
    public Money price() { return items.stream().map(Priceable::price).reduce(Money.ZERO, Money::plus); }
}
```

If you need to add *many different operations* over a stable tree without touching its classes, consider the [Visitor](../06-behavioral-patterns/10-visitor.md) pattern, which is commonly paired with Composite. To walk a tree you can also use an [Iterator](../06-behavioral-patterns/07-iterator.md).

## Common mistakes

- **Cycles:** adding a node to its own descendants causes infinite recursion. Guard against it if the tree is user-built.
- **Making the component interface too broad**, so leaves implement meaningless methods.
- **Ignoring depth**: very deep trees can cause a stack overflow with naive recursion.
- **Exposing the child list** (returning the internal mutable list), allowing outside modification. Return an unmodifiable view.
- **Parent references** left out when needed (for example, to walk up to the root), or kept inconsistent when moving nodes.
- **Using it when there's no real hierarchy.** A plain list is simpler.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Uniform treatment of single and grouped objects | The interface can become too general |
| Easy to add new node types | Hard to restrict which components may be combined |
| Simple client code: no type checks | Recursion needs care with depth and cycles |

## Quick Summary

- Composite represents tree structures with one shared interface for leaves and branches.
- The client calls operations on any node; composites delegate to their children.
- Prefer the safe approach: child management on the composite only.
- Pairs well with Visitor and Iterator.

## Related

[Decorator](./04-decorator.md) (similar recursive structure, different intent) · [Visitor](../06-behavioral-patterns/10-visitor.md) · [Iterator](../06-behavioral-patterns/07-iterator.md)

## Next

[04-decorator.md](./04-decorator.md)
