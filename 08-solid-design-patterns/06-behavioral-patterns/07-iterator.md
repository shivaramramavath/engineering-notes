# Iterator

**Intent:** provide a way to access the elements of an aggregate object sequentially without exposing its underlying representation.

Callers can traverse a collection (list, tree, graph, database result) in a uniform way, without knowing how it stores its data.

## The problem

Different collections store data differently: arrays, linked nodes, trees, hash tables. Without a common traversal mechanism, client code depends on each structure's internals:

```java
for (int i = 0; i < array.length; i++) { ... }          // array
for (Node n = list.head; n != null; n = n.next) { ... } // linked list
// tree: recursion with its own rules...
```

Changing the data structure means rewriting every loop.

## Structure

```text
Client ──► Iterator (interface)         Aggregate (interface)
             + hasNext()                   + iterator(): Iterator
             + next()                           ▲
                 ▲                              │
          ConcreteIterator ◄──────────── ConcreteAggregate
          (tracks position)
```

## In Java: built in

Java's collections framework implements this pattern:

```java
public interface Iterable<T> { Iterator<T> iterator(); }

public interface Iterator<T> {
    boolean hasNext();
    T next();
    default void remove() { throw new UnsupportedOperationException(); }
}
```

Anything that implements `Iterable` works in the enhanced `for` loop:

```java
for (String name : names) { ... }       // compiler uses names.iterator() under the hood
```

## Writing your own iterator

A class that stores items in a custom structure, here a simple linked chain:

```java
public class Playlist implements Iterable<Song> {
    private static class Node {
        final Song song;
        Node next;
        Node(Song song) { this.song = song; }
    }

    private Node head, tail;

    public void add(Song song) {
        Node n = new Node(song);
        if (head == null) head = tail = n;
        else { tail.next = n; tail = n; }
    }

    @Override
    public Iterator<Song> iterator() {
        return new Iterator<>() {
            private Node current = head;

            public boolean hasNext() { return current != null; }

            public Song next() {
                if (current == null) throw new NoSuchElementException();
                Song s = current.song;
                current = current.next;
                return s;
            }
        };
    }
}

for (Song s : playlist) { play(s); }      // the client doesn't know it's a linked chain
```

The internal `Node` structure stays private. If `Playlist` later switches to an `ArrayList`, only its `iterator()` changes.

## Multiple traversals

One structure can offer several ways to walk it. Each is its own iterator:

```java
public class Tree<T> implements Iterable<T> {
    public Iterator<T> iterator()          { return inOrder(); }
    public Iterator<T> inOrder()           { ... }
    public Iterator<T> breadthFirst()      { ... }
}
```

Independent iterators keep separate positions, so two loops over the same collection don't interfere.

## External vs internal iteration

| | External | Internal |
|---|---|---|
| Who controls the loop | The client (`hasNext`/`next`) | The collection (`forEach`, streams) |
| Flexibility | Full control: break, skip, interleave | Concise and declarative |
| Java | `Iterator`, `for-each` | `forEach`, `Stream` pipelines |

```java
names.forEach(System.out::println);                            // internal
names.stream().filter(n -> n.startsWith("A")).toList();        // internal, composable
```

Modern Java uses internal iteration for most transformations, and external iteration when you need control.

## Common pitfalls

| Issue | Detail |
|---|---|
| **`ConcurrentModificationException`** | Modifying a standard collection while iterating over it (except through the iterator's own `remove`). Most Java collections are *fail-fast* |
| **Removing during iteration** | Use `iterator.remove()` or `collection.removeIf(...)` |
| **Reusing an exhausted iterator** | Iterators are one-pass; get a new one for another traversal |
| **Streams are single-use** | A `Stream` can't be consumed twice |
| **Concurrency** | Standard iterators aren't thread-safe. Use concurrent collections (their iterators are *weakly consistent*) or copy first |
| **Infinite or lazy sequences** | `hasNext` may block or never become false; document it |

```java
// Wrong: throws ConcurrentModificationException
for (String s : names) { if (s.isEmpty()) names.remove(s); }

// Right
names.removeIf(String::isEmpty);
```

## Iterator vs related patterns

- [Composite](../05-structural-patterns/03-composite.md): iterators traverse a composite tree without exposing its nodes.
- [Visitor](./10-visitor.md): applies an operation to each element of a structure; an iterator only provides access order.
- [Memento](./09-memento.md): an iterator may keep state about its position.

## When to implement your own

- You expose a custom collection or data structure.
- A structure has several meaningful traversal orders.
- Elements are computed lazily or fetched in pages (database cursors, remote API pages).

Don't write one for a plain `List` or `Map`. The standard library already has it.

## Common mistakes

- **Exposing the internal list** instead of offering an iterator, so callers can mutate it. Return an unmodifiable view or an iterator.
- **Implementing `next()` without checking `hasNext()` semantics**, and not throwing `NoSuchElementException`.
- **Keeping iterator state in the collection** instead of in the iterator, which breaks concurrent traversals.
- **Calling `next()` twice per loop pass** by accident.

## Quick Summary

- Iterator provides uniform sequential access without exposing the collection's internals.
- In Java it's built in through `Iterable`, `Iterator` and the `for-each` loop.
- Write your own only for custom structures or lazy sources.
- Beware of fail-fast behavior and one-pass iterators.

## Related

[Composite](../05-structural-patterns/03-composite.md) · [Visitor](./10-visitor.md) · [Memento](./09-memento.md)

## Next

[08-mediator.md](./08-mediator.md)
