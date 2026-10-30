# Liskov Substitution Principle (LSP)

> If `S` is a subtype of `T`, objects of type `T` can be replaced with objects of type `S` without breaking the program.

In practice: **code written against the parent type must keep working correctly with every subtype**, without knowing which subtype it got. Compiling is not enough. The subtype must also keep the parent's *behavioral promises*.

LSP is what makes [polymorphism](../01-foundations/01-oop-pillars.md) safe, and therefore what makes the [Open/Closed Principle](./02-open-closed.md) work.

## Classic violation: Rectangle and Square

```java
public class Rectangle {
    protected int width, height;
    public void setWidth(int w)  { this.width = w; }
    public void setHeight(int h) { this.height = h; }
    public int area() { return width * height; }
}

public class Square extends Rectangle {
    @Override public void setWidth(int w)  { this.width = w; this.height = w; }
    @Override public void setHeight(int h) { this.width = h; this.height = h; }
}
```

A caller written for `Rectangle`:

```java
void resize(Rectangle r) {
    r.setWidth(5);
    r.setHeight(4);
    assert r.area() == 20;   // fails for Square: area is 16
}
```

A square *is* a rectangle in mathematics, but a **mutable `Square` does not behave like a mutable `Rectangle`**. The parent promises width and height are independent; the subtype breaks that.

Fix options: make them immutable, make both implement a common `Shape` interface, or do not model one as a subtype of the other.

```java
public interface Shape { int area(); }
public record Rectangle(int width, int height) implements Shape {
    public int area() { return width * height; }
}
public record Square(int side) implements Shape {
    public int area() { return side * side; }
}
```

## A real-world example

```java
List<String> names = Collections.unmodifiableList(new ArrayList<>());
names.add("x");   // throws UnsupportedOperationException
```

`List` promises `add`. This subtype refuses it at runtime. Any code written for `List` can now crash. It works in practice only because the class is clearly documented, but it shows how easily the contract weakens when a type must "support everything" in its parent.

## The rules

A subtype must honor the parent's **contract**:

| Rule | Meaning | Violation example |
|---|---|---|
| Preconditions can't be strengthened | Subtype must accept at least what the parent accepts | Parent accepts any positive amount, subtype only amounts up to 100 |
| Postconditions can't be weakened | Subtype must deliver at least what the parent promises | Parent guarantees a non-null result, subtype returns `null` |
| Invariants must be preserved | Rules that always hold for the parent still hold | Parent: balance never negative; subtype allows overdraft silently |
| No unexpected exceptions | Don't throw new kinds the parent's callers don't expect | Subtype throws `UnsupportedOperationException` for a method the parent implements |
| History constraint | Subtype must not change state in ways the parent forbids | A subtype of an immutable type that mutates |

## Spotting violations

- A subclass overrides a method with an **empty body** or throws `UnsupportedOperationException`.
- Callers use `instanceof` or type checks on a parent reference to treat subtypes differently.
- A subclass overrides a method to do something **opposite** to what the name or parent suggests.
- Documentation says "except for subclass X".
- Tests for the parent type fail when run against a subclass.

## Example and fix: "Bird can fly"

```java
class Bird { void fly() { /* ... */ } }
class Penguin extends Bird {
    @Override void fly() { throw new UnsupportedOperationException(); }
}
```

Any code that calls `bird.fly()` can fail. Put the capability where it belongs:

```java
interface Bird { void eat(); }
interface FlyingBird extends Bird { void fly(); }

class Sparrow implements FlyingBird { /* eat, fly */ }
class Penguin implements Bird { /* eat only */ }
```

Callers that need flight ask for `FlyingBird`, so a penguin can't be passed by mistake. This also relates to [Interface Segregation](./04-interface-segregation.md).

## Testing LSP

Write the parent's contract tests once, and run them against every subtype.

```java
abstract class ShapeContractTest {
    abstract Shape create();

    @Test void areaIsNeverNegative() {
        assertTrue(create().area() >= 0);
    }
}

class SquareTest extends ShapeContractTest {
    Shape create() { return new Square(3); }
}
```

## Common mistakes

- **Modeling by real-world "is-a"** without checking behavior. Behavior decides, not vocabulary.
- **Inheriting just to reuse code**, then disabling parts of the parent. Use [composition](../01-foundations/02-composition-vs-inheritance.md).
- **Strengthening input validation** in a subclass.
- **Documenting a restriction instead of fixing the model.**

## Quick Summary

- Subtypes must honor the parent's contract, not only its method signatures.
- Don't strengthen preconditions, weaken postconditions or break invariants.
- Empty overrides, `UnsupportedOperationException` and `instanceof` checks are the main smells.
- When a subtype can't keep the promise, change the hierarchy: split interfaces or use composition.

## Next

[04-interface-segregation.md](./04-interface-segregation.md)
