# Visitor

**Intent:** represent an operation to be performed on the elements of an object structure. Visitor lets you define a new operation without changing the classes of the elements on which it operates.

It's the most advanced behavioral pattern, and the easiest to misuse. The essential idea is **double dispatch**: the code that runs depends on the types of *two* objects, the element and the visitor.

## The problem

You have a stable hierarchy of element types (an expression tree, a document model, an AST), and you keep needing new operations over it: print, evaluate, export to JSON, validate, compute cost.

Putting every operation inside the element classes bloats them and violates [SRP](../02-solid-principles/01-single-responsibility.md):

```java
interface Shape {
    double area();
    String toSvg();
    String toJson();
    void validate();            // every new operation edits every Shape class
}
```

Checking types from the outside is also unappealing:

```java
if (shape instanceof Circle c) { ... } else if (shape instanceof Rectangle r) { ... }
```

## Structure

```text
Visitor (interface)                       Element (interface)
  + visitCircle(Circle)                      + accept(Visitor)
  + visitRectangle(Rectangle)                     ▲
        ▲                                  ┌──────┴───────┐
  ┌─────┴──────┐                         Circle       Rectangle
AreaVisitor  SvgVisitor                  accept(v) { v.visitCircle(this); }
```

- **Visitor**: declares one `visit` method per concrete element type.
- **Concrete visitor**: implements one operation across all element types.
- **Element**: has `accept(visitor)`, which calls back the matching visit method.

## Example

```java
public interface ShapeVisitor<R> {
    R visitCircle(Circle c);
    R visitRectangle(Rectangle r);
}

public interface Shape {
    <R> R accept(ShapeVisitor<R> visitor);
}

public class Circle implements Shape {
    final double radius;
    public Circle(double radius) { this.radius = radius; }
    public <R> R accept(ShapeVisitor<R> v) { return v.visitCircle(this); }     // dispatch #2
}

public class Rectangle implements Shape {
    final double width, height;
    public Rectangle(double w, double h) { this.width = w; this.height = h; }
    public <R> R accept(ShapeVisitor<R> v) { return v.visitRectangle(this); }
}

// Operations live in visitors
public class AreaVisitor implements ShapeVisitor<Double> {
    public Double visitCircle(Circle c)       { return Math.PI * c.radius * c.radius; }
    public Double visitRectangle(Rectangle r) { return r.width * r.height; }
}

public class SvgVisitor implements ShapeVisitor<String> {
    public String visitCircle(Circle c)       { return "<circle r=\"" + c.radius + "\"/>"; }
    public String visitRectangle(Rectangle r){ return "<rect width=\"" + r.width + "\" height=\"" + r.height + "\"/>"; }
}

List<Shape> shapes = List.of(new Circle(2), new Rectangle(3, 4));
double total = shapes.stream().mapToDouble(s -> s.accept(new AreaVisitor())).sum();
```

Adding a new operation (say, a perimeter calculator) means writing a new visitor. No shape class changes.

## How double dispatch works

```text
shape.accept(visitor)
   1st dispatch: runtime type of shape  → Circle.accept(...)
   inside: visitor.visitCircle(this)
   2nd dispatch: runtime type of visitor → AreaVisitor.visitCircle(...)
```

Java picks overloads by static type, so a plain `visit(Shape)` method wouldn't know the concrete type. That's why `accept` calls a type-specific method.

## The trade-off: which axis changes?

|  | New **operation** | New **element type** |
|---|---|---|
| Plain polymorphism (method in each class) | Edit every class | Add one class |
| **Visitor** | **Add one visitor** | Edit the visitor interface **and every visitor** |

Visitor is good **only when the element hierarchy is stable and operations keep growing**. If new element types appear often, it's painful.

## A modern alternative in Java: sealed types and pattern matching

With Java 21+, a sealed hierarchy and pattern matching for `switch` give the same result with far less ceremony, and the compiler checks exhaustiveness:

```java
public sealed interface Shape permits Circle, Rectangle {}
public record Circle(double radius) implements Shape {}
public record Rectangle(double width, double height) implements Shape {}

static double area(Shape s) {
    return switch (s) {
        case Circle c    -> Math.PI * c.radius() * c.radius();
        case Rectangle r -> r.width() * r.height();
    };
}
```

If you add a new `Shape` subtype, every `switch` without a default stops compiling until you handle it. This often replaces Visitor in new code. The classic pattern remains relevant when:

- you're on an older Java version,
- visitors need to carry state or be passed around as objects,
- you work with libraries or frameworks that use it (compilers, parsers, ASTs, code analyzers).

## Where it's used

- Compilers and interpreters: operations over an abstract syntax tree (type-check, optimize, generate code).
- Document processing: export a document model to HTML, PDF, plain text.
- Tax or pricing calculation across different item types.
- File-system walking (Java's `FileVisitor` and `Files.walkFileTree` follow this idea).

## Visitor and Composite

Visitor pairs naturally with [Composite](../05-structural-patterns/03-composite.md): a composite's `accept` visits itself and then passes the visitor to its children, so one visitor walks the whole tree.

```java
public <R> R accept(TreeVisitor<R> v) {
    children.forEach(c -> c.accept(v));
    return v.visitFolder(this);
}
```

## Benefits and costs

| Benefits | Costs |
|---|---|
| Add operations without touching element classes | Adding element types is expensive |
| Related logic for one operation sits in one place | Elements must expose enough state for visitors, which can weaken encapsulation |
| Visitors can accumulate state while traversing | Boilerplate: `accept` in every element, a method per type in every visitor |
| Keeps unrelated operations out of the model | Harder to understand for newcomers |

## Common mistakes

- **Using it when element types change often.** Prefer ordinary polymorphism.
- **Making element internals public** only to feed visitors.
- **Forgetting `accept` in a new element**, or implementing it with the wrong visit method.
- **Visitors that mix several operations**, losing the point of the pattern.
- **Unnecessary use** where a `switch` over a sealed type (or a single method) is simpler.
- **Ignoring traversal responsibility**: decide whether the elements or the visitor traverse children.

## Quick Summary

- Visitor adds new operations to a stable class hierarchy without modifying it, using double dispatch.
- Great for many operations over few, stable element types; poor for frequently changing element types.
- In Java 21+, sealed types with pattern-matching `switch` often replace it.
- Pairs well with Composite for tree traversal.

## Related

[Composite](../05-structural-patterns/03-composite.md) · [Iterator](./07-iterator.md) · [Strategy](./01-strategy.md)

## Next

Application patterns: [../07-application-patterns/00-overview.md](../07-application-patterns/00-overview.md)
