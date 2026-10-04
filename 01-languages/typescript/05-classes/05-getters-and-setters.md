# Getters and Setters

## Definition
**Accessors** (`get` and `set`) look like properties to callers but run code when read or written. They let a class expose computed values and validate or transform assignments.

## Why It Matters
Accessors let you keep a stable public surface (`user.fullName`) while changing how the value is stored or computed, and enforce invariants on writes.

## Prerequisites
[classes.md](00-classes.md), [access-modifiers.md](01-access-modifiers.md)

## Syntax

```ts
class Temperature {
  private _celsius = 0;

  get celsius(): number {
    return this._celsius;
  }

  set celsius(value: number) {
    if (value < -273.15) throw new RangeError("Below absolute zero");
    this._celsius = value;
  }

  get fahrenheit(): number {            // computed, read-only
    return this._celsius * 9 / 5 + 32;
  }
}
```

## Basic Example

```ts
const t = new Temperature();
t.celsius = 25;          // runs the setter
t.fahrenheit;            // 77
t.fahrenheit = 100;      // Error: Cannot assign to 'fahrenheit' because it is a read-only property
```

## How It Works
- A property with only a `get` is **read-only** to the type system.
- A property with a `set` only is write-only (rare).
- Accessors need a target of ES5 or later.
- Accessors are defined on the prototype, not as own properties of each instance, which affects `Object.keys`, spreading and `JSON.stringify` (getters on the prototype are not serialised by default).

### Type rules
The getter's type must be assignable to the setter's parameter type. Newer TypeScript versions allow the getter and setter to have unrelated types, which is handy for accepting looser input:

```ts
class Config {
  private _timeout = 1000;

  get timeout(): number { return this._timeout; }
  set timeout(value: number | string) {
    this._timeout = typeof value === "string" ? Number.parseInt(value, 10) : value;
  }
}
```
If you need to support older compilers, keep the getter type assignable to the setter type.

## Important Concepts

### Backing field naming
Use a private field with a different name than the accessor:

```ts
private _name = "";     // or #name for runtime privacy
get name() { return this._name; }
```
A common trap is a setter that assigns to its own name, causing infinite recursion:

```ts
set name(v: string) { this.name = v; }   // stack overflow
```

### Computed / derived properties
```ts
class Order {
  constructor(private items: { price: number; qty: number }[]) {}
  get total(): number {
    return this.items.reduce((sum, i) => sum + i.price * i.qty, 0);
  }
}
```
Getters should be cheap and side-effect free; if the work is expensive, use a method or cache the result.

### Validation and invariants
Setters are a natural place to enforce rules (non-negative quantity, valid email). If invalid values should be impossible, prefer validating in the constructor or factory so the object is never invalid.

### Accessors in interfaces
Interfaces describe properties, not whether they are implemented as fields or accessors:

```ts
interface HasTotal { readonly total: number }
class Order implements HasTotal {
  get total() { return 0; }
}
```

### `accessor` keyword (auto-accessors)
TypeScript 4.9+ supports `accessor`, which creates a private backing field with a generated getter and setter. It is mostly useful as the target for [decorators](07-decorators.md):

```ts
class Counter {
  accessor count = 0;
}
```

### Static accessors
```ts
class Registry {
  private static _size = 0;
  static get size() { return Registry._size; }
}
```

### Accessors and inheritance
A subclass must not turn an accessor into a plain field (or the reverse) for the same name; the compiler reports an error for these overrides.

### When to prefer plain properties
If an accessor does nothing but return a field, use a `readonly` or public field. Add an accessor only when you need computation, validation or encapsulation.

## Common Mistakes
- Infinite recursion by assigning to the accessor's own name inside the setter.
- Doing expensive or side-effecting work in a getter.
- Expecting getters to appear in `JSON.stringify` or object spread of an instance.
- Writing trivial getters/setters around public data (Java-style) with no added value.
- Making the getter and setter semantically different (reading what you wrote should give back the same thing).

## Best Practices
- Keep getters pure and fast.
- Use `get`-only for derived values.
- Keep setters validation-focused and throw clear errors.
- Prefer methods (`setTimeout()`, `recalculate()`) when the operation is an action rather than a property assignment.

## Interview Questions
- How does the type system treat a property with only a getter?
- Do accessors exist on the instance or the prototype?
- Why must the backing field have a different name?
- When is a getter a bad idea?

## Quick Reference
```ts
class A {
  private _x = 0;
  get x(): number { return this._x; }
  set x(v: number) { this._x = v; }
  get double(): number { return this._x * 2; }   // read-only
  accessor y = 0;                                // auto-accessor
}
```

## Related Topics
- [classes.md](00-classes.md)
- [access-modifiers.md](01-access-modifiers.md)
- [decorators.md](07-decorators.md)
- [Readonly and optional properties](../04-objects-and-interfaces/02-readonly-and-optional-properties.md)
