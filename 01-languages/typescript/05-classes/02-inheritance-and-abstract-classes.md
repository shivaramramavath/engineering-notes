# Inheritance and Abstract Classes

## Definition
**Inheritance** lets a class (`extends`) reuse and specialise another class. An **abstract class** is a base class that cannot be instantiated directly and may declare members that subclasses must implement.

## Why It Matters
Inheritance models genuine "is-a" relationships and shares implementation. It also couples subclasses tightly to their parents, so it should be used deliberately.

## Prerequisites
[classes.md](00-classes.md), [access-modifiers.md](01-access-modifiers.md)

## Syntax

```ts
class Animal {
  constructor(public name: string) {}
  speak(): string { return `${this.name} makes a sound`; }
}

class Dog extends Animal {
  constructor(name: string, public breed: string) {
    super(name);                       // must come before using `this`
  }
  override speak(): string {
    return `${super.speak()}: Woof`;
  }
}
```

## Basic Example

```ts
const d = new Dog("Rex", "Lab");
d.speak();          // "Rex makes a sound: Woof"
d instanceof Dog;   // true
d instanceof Animal; // true

const a: Animal = d;   // a Dog is usable as an Animal
```

## How It Works

### `super`
- In a subclass constructor, call `super(...)` before accessing `this`.
- In methods, `super.method()` calls the parent implementation.
- A subclass without its own constructor inherits the parent's.

### `override` and `noImplicitOverride`
`override` states that a method replaces a parent method. The compiler errors if the parent does not have it (for example after a rename):

```ts
class Cat extends Animal {
  override speek() {}   // Error: not declared in the base class
}
```
Enable `noImplicitOverride` to *require* the keyword whenever you override, which catches accidental overrides and renames.

### Overriding rules
An override must be compatible with the base member: parameters accepted by the base must be accepted by the override, and the return type must be assignable to the base's.

### Properties and `useDefineForClassFields`
Redeclaring a field in a subclass can reset it, and the base constructor runs before subclass field initialisers. Do not read subclass fields from a base constructor.

### Extending built-ins
```ts
class HttpError extends Error {
  constructor(public status: number, message: string) {
    super(message);
    this.name = "HttpError";
  }
}
```
With a modern target (`ES2015`+) this works as expected, including `instanceof`. See [custom errors](../11-error-handling/01-custom-errors.md).

## Abstract Classes

```ts
abstract class Shape {
  abstract area(): number;             // no body: subclasses must implement

  describe(): string {                 // concrete, shared
    return `Area: ${this.area().toFixed(2)}`;
  }
}

class Circle extends Shape {
  constructor(private radius: number) { super(); }
  area() { return Math.PI * this.radius ** 2; }
}

new Shape();           // Error: Cannot create an instance of an abstract class
new Circle(2).describe();
```

Abstract classes can have abstract properties and accessors, constructors, concrete members and `protected` helpers. A subclass that does not implement all abstract members must also be `abstract`.

### Template method pattern
```ts
abstract class Exporter {
  export(data: unknown[]): string {
    return this.header() + data.map(d => this.row(d)).join("\n");
  }
  protected abstract header(): string;
  protected abstract row(item: unknown): string;
}
```

### Abstract class vs. interface

| | Abstract class | Interface |
|---|---|---|
| Exists at runtime (`instanceof`) | Yes | No |
| Can hold implementation / state | Yes | No |
| Multiple inheritance | No (single `extends`) | Yes (`implements` many) |
| Best for | Shared base behaviour | Contracts and dependency boundaries |

### Abstract constructor types
```ts
type AbstractCtor<T> = abstract new (...args: any[]) => T;
```

## When Not to Use Inheritance
Prefer **composition** (hold a collaborator) when:
- The relationship is "has-a" or "uses-a", not "is-a".
- You would inherit only to reuse a few methods.
- The hierarchy goes more than two or three levels deep.
- You need to combine behaviours from several sources (use [mixins](06-mixins.md) or composition).

```ts
// composition
class Report {
  constructor(private readonly formatter: Formatter) {}
  render(data: Data) { return this.formatter.format(data); }
}
```

## Common Mistakes
- Using `this` before calling `super()`.
- Overriding a method accidentally or silently after a rename (use `override` + `noImplicitOverride`).
- Building deep hierarchies for code reuse.
- Reading subclass fields in the base-class constructor.
- Making a class abstract but still instantiating it in tests.

## Best Practices
- Enable `noImplicitOverride`.
- Keep hierarchies shallow; prefer composition and interfaces.
- Make base classes designed for extension explicit (`abstract`, `protected` hooks).
- Follow the Liskov principle: a subclass must work wherever its base is expected.

## Interview Questions
- What does `override` do and why use `noImplicitOverride`?
- Difference between an abstract class and an interface?
- When must `super()` be called?
- Why prefer composition over inheritance?

## Quick Reference
```ts
class B extends A { constructor() { super(); } override m() {} }
abstract class S { abstract f(): void; }
// compiler: "noImplicitOverride": true
```

## Related Topics
- [implements.md](03-implements.md)
- [mixins.md](06-mixins.md)
- [Factory and strategy patterns](../17-design-patterns/00-factory.md)
- [Custom errors](../11-error-handling/01-custom-errors.md)
