# Static Members

## Definition
**Static** members belong to the class itself rather than to instances. You access them as `ClassName.member`, not `instance.member`.

## Why It Matters
Statics hold shared state and utilities tied to a class: factories, constants, counters, caches and type-level helpers. They are also easy to overuse as global state.

## Prerequisites
[classes.md](00-classes.md), [access-modifiers.md](01-access-modifiers.md)

## Syntax

```ts
class MathUtil {
  static readonly PI = 3.14159;
  static count = 0;

  static square(n: number): number {
    MathUtil.count++;
    return n * n;
  }
}

MathUtil.square(4);
MathUtil.PI;
```

## Basic Example

```ts
const m = new MathUtil();
m.square(2);      // Error: 'square' does not exist on type 'MathUtil'. Did you mean to access the static member 'MathUtil.square' instead?
```

## How It Works
- Statics live on the constructor function, so they are shared by everyone and are not part of the instance type (`MathUtil` the type). They *are* part of `typeof MathUtil`.
- Static members can use `public`, `protected`, `private` and `readonly`.
- Statics are **inherited** by subclasses (`class B extends A` gets `B.staticMethod`), and `this` inside a static method refers to the class it was called on.

### Static factory methods
Give construction a name, validate, or return different subtypes:

```ts
class Money {
  private constructor(readonly cents: number, readonly currency: string) {}

  static fromDollars(amount: number, currency = "USD"): Money {
    return new Money(Math.round(amount * 100), currency);
  }

  static zero(currency = "USD"): Money {
    return new Money(0, currency);
  }
}

Money.fromDollars(12.5);
new Money(1, "USD");   // Error: constructor is private
```

### Singleton (use sparingly)
```ts
class Config {
  private static instance?: Config;
  private constructor(readonly env: string) {}

  static get(): Config {
    return (Config.instance ??= new Config(process.env.NODE_ENV ?? "development"));
  }
}
```
Global singletons make testing harder; prefer passing dependencies explicitly ([dependency injection](../17-design-patterns/05-dependency-injection.md)).

### Static blocks
Run initialisation code once, when the class is defined:

```ts
class Registry {
  static readonly handlers = new Map<string, () => void>();

  static {
    Registry.handlers.set("start", () => console.log("start"));
  }
}
```
Static blocks require a target that supports them (ES2022+) or are downleveled by TypeScript.

### Static members and generics
Static members cannot reference the class's type parameters, because there is one class object shared by all instantiations:

```ts
class Box<T> {
  static empty: T;   // Error: Static members cannot reference class type parameters
}
```
Give the static method its own generic parameter instead:
```ts
class Box<T> {
  constructor(public value: T) {}
  static of<U>(value: U): Box<U> { return new Box(value); }
}
```

### Static `this` and inheritance
```ts
class Base {
  static create<T extends typeof Base>(this: T): InstanceType<T> {
    return new this() as InstanceType<T>;
  }
}
class Derived extends Base {}
const d = Derived.create();   // Derived
```

### Static vs. module-level functions
A static method with no relationship to the class is just a function. In modern TypeScript, a plain exported function or constant is usually simpler, more tree-shakeable and easier to test:

```ts
// prefer
export const square = (n: number) => n * n;
```
Use statics when the member truly belongs to the class (factories, class-level caches, `instanceof` helpers).

### Name clashes
Some names are reserved on functions (`name`, `length`, `prototype`, `caller`), so `static name = ...` is an error.

## Common Mistakes
- Trying to call a static through an instance.
- Using statics as mutable global state (hard to reset between tests).
- Referencing class type parameters in static members.
- Using static-only classes (a namespace of functions) instead of module exports.
- Forgetting statics are inherited and shared with subclasses (a mutable static array is shared unless redeclared).

## Best Practices
- Prefer module functions to static-only utility classes.
- Use private constructors with static factories for validated creation.
- Mark constants `static readonly`.
- Avoid mutable statics; if you need shared state, scope it explicitly and provide a reset for tests.

## Interview Questions
- What is the difference between a static and an instance member?
- Why can't a static member use the class's type parameter?
- How do you make a class constructible only through a factory?
- Are static members inherited?

## Quick Reference
```ts
class A {
  static x = 1;
  static readonly Y = 2;
  private static cache = new Map<string, A>();
  static create(): A { return new A(); }
  static { /* init */ }
}
A.x;  A.create();
```

## Related Topics
- [classes.md](00-classes.md)
- [access-modifiers.md](01-access-modifiers.md)
- [Factory pattern](../17-design-patterns/00-factory.md)
- [Modules](../08-modules/00-imports-and-exports.md)
