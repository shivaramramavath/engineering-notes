# Composition vs Inheritance

"Favor composition over inheritance" is one of the most repeated lines in design, and one of the least explained. This note covers what it means, when it's true, and when inheritance is still the right choice.

## The two relationships

| | Inheritance | Composition |
|---|---|---|
| Relationship | **is-a** | **has-a** / **uses-a** |
| Binding | Fixed at compile time | Can change at runtime |
| Coupling | Subclass depends on superclass internals | Depends only on the other type's public contract |
| Reuse | Inherits everything, including what you don't want | Pick exactly what you delegate |

## Why inheritance is risky: the fragile base class

A subclass relies on *how* the superclass works, not only on what it promises. When the superclass changes internally, the subclass can silently break.

Classic example: count how many elements were added to a set.

```java
public class CountingSet<E> extends HashSet<E> {
    private int addCount = 0;

    @Override public boolean add(E e) {
        addCount++;
        return super.add(e);
    }

    @Override public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return super.addAll(c);   // HashSet.addAll calls add() internally
    }

    public int addCount() { return addCount; }
}

var set = new CountingSet<String>();
set.addAll(List.of("a", "b", "c"));
set.addCount();   // 6, not 3
```

`addAll` is counted once directly and again through `add()`. The subclass depended on an undocumented implementation detail.

## The composition version

Wrap the object and delegate to it. Only the methods you choose are exposed.

```java
public class CountingSet<E> implements Set<E> {
    private final Set<E> delegate;
    private int addCount = 0;

    public CountingSet(Set<E> delegate) { this.delegate = delegate; }

    @Override public boolean add(E e) {
        addCount++;
        return delegate.add(e);
    }

    @Override public boolean addAll(Collection<? extends E> c) {
        addCount += c.size();
        return delegate.addAll(c);
    }

    // remaining Set methods simply delegate
}
```

Now it doesn't matter how `addAll` is implemented inside the wrapped set. This wrapping idea is the basis of the Decorator and Proxy patterns.

## Behavior that varies: composition wins

Suppose different ducks fly in different ways.

Inheritance approach (breaks quickly):

```text
Duck
 ├─ MallardDuck      (flies)
 ├─ RubberDuck       (doesn't fly, must override fly() to do nothing)
 └─ RocketDuck       (flies differently, needs another override)
```

Composition approach:

```java
interface FlyBehavior { void fly(); }

class Duck {
    private FlyBehavior flyBehavior;
    Duck(FlyBehavior fb) { this.flyBehavior = fb; }
    void performFly() { flyBehavior.fly(); }
    void setFlyBehavior(FlyBehavior fb) { this.flyBehavior = fb; }   // change at runtime
}
```

This is the Strategy pattern, and it's the most common example of composition replacing a hierarchy.

## When inheritance is still right

Use inheritance when **all** of these hold:

1. There is a true is-a relationship, and the subclass honors every promise of the superclass (see [Liskov Substitution](../02-solid-principles/03-liskov-substitution.md)).
2. The superclass was designed for extension and documents how overridable methods are used.
3. You want to be used polymorphically wherever the superclass is expected.
4. The hierarchy is shallow, ideally one or two levels.

Typical good uses: framework base classes designed for extension, and abstract classes that fix an algorithm skeleton (Template Method).

## Decision guide

```text
Do you need polymorphism over this type?
  No  → composition (just use it)
  Yes → is it truly an "is-a" with the same contract?
          No  → interface + composition
          Yes → is the base class designed for extension?
                  No  → composition (wrap it)
                  Yes → inheritance is acceptable
```

In Java, you can lock down unintended inheritance with `final` classes, or control it with `sealed` hierarchies.

## Common mistakes

- **Inheriting to reuse one method.** Use a helper object instead.
- **Deep hierarchies** (four or more levels). Hard to understand and to change.
- **"Is-a" in everyday language but not in behavior.** A square is a rectangle in geometry, but a mutable `Square` extending `Rectangle` breaks callers that set width and height separately.
- **Overriding to disable behavior** (empty or throwing methods). A sign the hierarchy is wrong.

## Quick Summary

- Inheritance couples you to the parent's internals; composition couples you only to its contract.
- Use inheritance for real, stable is-a relationships in designs meant for extension.
- Use composition + interfaces for behavior that varies or changes at runtime.
- Strategy, Decorator and Proxy are composition-based patterns you'll see later.

## Next

[03-coupling-and-cohesion.md](./03-coupling-and-cohesion.md)
