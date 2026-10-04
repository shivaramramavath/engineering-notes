# Structural Typing

## Definition
TypeScript compares types by their **structure** (the members they have), not by their declared names. If a value has everything a type requires, it is assignable to that type, regardless of where it was declared. This is sometimes called "duck typing" checked at compile time.

## Why It Matters
Structural typing explains most behaviour that surprises newcomers: why unrelated types are interchangeable, why extra properties are sometimes accepted, and why `type UserId = string` gives no protection.

## Prerequisites
[interfaces.md](00-interfaces.md), [objects](../01-fundamentals/03-objects.md)

## Basic Example

```ts
interface Point { x: number; y: number }

class Coordinate {
  constructor(public x: number, public y: number) {}
}

function plot(p: Point) {}

plot(new Coordinate(1, 2));      // ok: Coordinate has x and y
plot({ x: 1, y: 2 });            // ok
```
`Coordinate` never mentions `Point`, but its structure fits.

## How It Works

### Assignability rule
`S` is assignable to `T` if `S` has every required property of `T` with compatible types. Extra properties on `S` do not matter:

```ts
interface Named { name: string }

const dog = { name: "Rex", breed: "Lab" };
const n: Named = dog;   // ok: has name
```

### Nested and method members are compared the same way
Each property is compared recursively; function members follow the rules in [function types](../02-functions/00-function-types.md).

### Names do not matter
```ts
type A = { x: number };
type B = { x: number };
const a: A = { x: 1 };
const b: B = a;   // ok
```

## Important Concepts

### Excess property checking
When you assign a **fresh object literal** directly to a typed target, TypeScript also rejects unknown properties, as a typo guard:

```ts
interface Named { name: string }

const a: Named = { name: "Ada", age: 36 };
// Error: Object literal may only specify known properties, and 'age' does not exist in type 'Named'

const obj = { name: "Ada", age: 36 };
const b: Named = obj;   // ok: not a fresh literal
```

The check applies at the point where a literal is created and checked against a target type: assignments, arguments, return values, nested literals.

Ways to handle it:
```ts
// 1. Fix the typo / remove the property
// 2. Declare the property on the type
// 3. Allow extras with an index signature
interface Open { name: string; [key: string]: unknown }
// 4. Use an intermediate variable (only if extras are intentional)
// 5. Assertion (last resort): ({ name: "x", age: 1 }) as Named
```

### Excess checks and unions
For unions, a property is acceptable if it exists on **any** member (non-discriminated unions) or on the matching member (discriminated unions).

### Weak type detection
If every property of the target is optional, the source must share at least one property:

```ts
interface Opts { retries?: number }
const o: Opts = { retry: 3 };   // Error: no properties in common
```

### Spread and extra properties
Properties that arrive through a spread are not subject to excess property checking:

```ts
const base = { name: "Ada", age: 36 };
const x: Named = { ...base };   // ok: 'age' comes from the spread, so it is not flagged
```
Do not rely on excess property checks for objects built with spreads; they only reliably protect properties you write out explicitly.

### Classes are structural too, except for private members
```ts
class A { private secret = 1; x = 0 }
class B { private secret = 1; x = 0 }

const a: A = new B();   // Error: Types have separate declarations of a private property
```
A class with `private` or `protected` members is compared **nominally** for those members: only subclasses (the same declaration) are compatible. This is a lightweight way to get nominal behaviour; see also `#private` fields.

### Empty and near-empty types
```ts
function f(x: {}) {}
f("hello");     // ok: anything except null/undefined is assignable to {}
```
`{}` is rarely what you want; see [objects](../01-fundamentals/03-objects.md).

### Subtypes and substitutability
A type with more properties is a **subtype** of one with fewer. Anywhere a supertype is expected, a subtype can be used:

```ts
interface Animal { name: string }
interface Dog extends Animal { bark(): void }

declare let animal: Animal;
declare let dog: Dog;
animal = dog;   // ok
dog = animal;   // Error
```
More in [assignability and subtyping](../14-type-system-internals/01-assignability-and-subtyping.md).

### Getting nominal behaviour when you need it
Structural typing means `UserId` and `OrderId` (both `string`) are interchangeable. Use [branded types](../10-advanced-types/07-branded-types.md):

```ts
type UserId = string & { readonly __brand: "UserId" };
type OrderId = string & { readonly __brand: "OrderId" };
```

### Structural typing and runtime checks
Types are erased, so `instanceof` works only with classes. Checking structure at runtime requires explicit code: [type guards](../03-unions-and-narrowing/05-type-guards-and-assertion-functions.md) or [schemas](../15-runtime-validation/01-schema-validation.md).

## Common Mistakes
- Assuming two different named types cannot be assigned to each other.
- Expecting excess property errors when passing a variable rather than a literal.
- Using `type Id = string` for different kinds of ids and mixing them up.
- Relying on excess property checks for objects built from spreads.
- Expecting `instanceof` to work with interfaces.

## Best Practices
- Embrace structural typing: depend on the smallest interface a function needs.
- Pass literals directly (or annotate variables) to keep excess property checks.
- Use branded types for ids and units where mix-ups are costly.
- Remember excess checks protect typos, not data validity; validate external data at runtime.

## Interview Questions
- What is structural typing and how does it differ from nominal typing?
- Why does `const n: Named = obj` compile when `const n: Named = { ... }` fails?
- How can you make TypeScript treat two identical-shape types as different?
- Why do classes with private members behave differently?

## Quick Reference
```ts
// assignable if S has all required members of T
// fresh literal => excess properties are errors
// private/protected members => nominal-like
// brand: string & { readonly __brand: "X" }
```

## Related Topics
- [interfaces.md](00-interfaces.md)
- [Assignability and subtyping](../14-type-system-internals/01-assignability-and-subtyping.md)
- [Branded types](../10-advanced-types/07-branded-types.md)
- [Access modifiers](../05-classes/01-access-modifiers.md)
