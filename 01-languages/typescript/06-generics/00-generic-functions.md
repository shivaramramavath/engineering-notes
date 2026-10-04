# Generic Functions

## Definition
A **generic function** has one or more **type parameters** (written in angle brackets, like `<T>`) that stand for types decided by the caller. The function's parameters and return type can refer to them, so the relationship between input and output types is preserved.

## Why It Matters
Without generics you would either duplicate a function per type or use `any` and lose type safety. Generics let one implementation work for many types while still telling the compiler exactly what comes out.

## Prerequisites
[function types](../02-functions/00-function-types.md), [union types](../03-unions-and-narrowing/00-union-types.md)

## Syntax

```ts
function identity<T>(value: T): T {
  return value;
}

const a = identity("hello");   // T is inferred from the argument; the result is a string
const b = identity(42);        // T is inferred from the argument; the result is a number
const c = identity<boolean>(true);   // explicit type argument
```

## Basic Example

The `any` version loses information:

```ts
function firstAny(items: any[]): any {
  return items[0];
}
const x = firstAny([1, 2, 3]);   // any: no type safety
```

The generic version keeps it:

```ts
function first<T>(items: T[]): T | undefined {
  return items[0];
}

const n = first([1, 2, 3]);       // number | undefined
const s = first(["a", "b"]);      // string | undefined
const e = first([]);              // undefined (T inferred as never)
```

## How It Works
- A type parameter is a placeholder, like a function parameter but for types.
- At each call, TypeScript infers `T` from the arguments (see [generic-inference.md](05-generic-inference.md)). If inference is impossible or you want something else, pass it explicitly.
- Inside the function, `T` is opaque: you can only do what is valid for **every** possible `T` unless you add a [constraint](02-generic-constraints-and-defaults.md).

```ts
function bad<T>(value: T) {
  return value.length;   // Error: Property 'length' does not exist on type 'T'
}
```

## Important Concepts

### Multiple type parameters
```ts
function pair<A, B>(a: A, b: B): [A, B] {
  return [a, b];
}
const p = pair("x", 1);   // [string, number]

function mapArray<T, U>(items: T[], fn: (item: T) => U): U[] {
  return items.map(fn);
}
const lengths = mapArray(["a", "bb"], s => s.length);   // number[]
```
`T` is inferred from `items`, then `U` from what the callback returns.

### Generic arrow functions
```ts
const identity = <T>(value: T): T => value;

// In .tsx files `<T>` looks like JSX; add a comma or a constraint:
const identityTsx = <T,>(value: T): T => value;
const identityTsx2 = <T extends unknown>(value: T): T => value;
```

### Generic function types and signatures
```ts
type Mapper = <T, U>(items: T[], fn: (item: T) => U) => U[];

interface Identity {
  <T>(value: T): T;
}
```
Note `<T>(x: T) => T` (generic function) differs from `Identity<T>` (generic type that fixes `T` first); see [generic types](01-generic-types.md).

### Generic methods
```ts
class Store {
  get<T>(key: string): T | undefined { /* ... */ return undefined; }
}
```
Be wary of this shape: `T` appears only in the return type, so the caller is just asserting a type (see Common Mistakes).

### Rules of thumb for good generics
1. **Use a type parameter at least twice** (in a parameter and the return type, or two parameters). If it appears once, a generic adds nothing:

```ts
// unnecessary generic
function log<T>(value: T): void { console.log(value); }
// simpler and equivalent
function log2(value: unknown): void { console.log(value); }
```

2. **Push the type parameter down**: constrain the smallest piece you need.
```ts
function firstOf<T extends unknown[]>(arr: T) { return arr[0]; }   // returns unknown-ish
function firstOf2<E>(arr: E[]): E | undefined { return arr[0]; }   // better
```

3. **Use fewer type parameters**: each one increases complexity and the chance of inference problems.

### Generics vs. unions vs. overloads
- Same type in and out -> generic.
- Several unrelated accepted types, same output type -> union.
- Different outputs for different inputs -> [overloads](../02-functions/03-function-overloads.md) or a conditional return type.

### Generic async functions
```ts
async function fetchJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  return res.json() as Promise<T>;   // unchecked: T is a promise, not a guarantee
}
```
See [async generics](../12-async-and-iteration/03-async-generics.md) and why unchecked `T` is unsafe.

## Common Mistakes
- **Return-only generics** (`get<User>("key")`): `T` appears only in the return type, so it is an unchecked assertion in disguise. Validate or use a schema instead.
- Declaring a type parameter used only once.
- Using `any` inside generics, defeating the point.
- Over-engineering with many type parameters when a simple union works.
- Forgetting the comma in generic arrow functions in `.tsx` files.

## Best Practices
- Name simple parameters `T`, `U`, `K`, `V`; use descriptive names (`TItem`, `TResponse`) when there are several.
- Let inference work; add explicit type arguments only when needed.
- Add constraints to express what you need from `T`.
- Prefer returning `T`-based types over casting.

## Interview Questions
- What problem do generics solve compared to `any`?
- When is a type parameter unnecessary?
- How do you write a generic arrow function in a `.tsx` file?
- Why is `get<T>(key): T` considered unsafe?

## Quick Reference
```ts
function f<T>(x: T): T
function f<T, U>(x: T, g: (x: T) => U): U
const f = <T,>(x: T): T => x
f<string>("a")           // explicit
type Fn = <T>(x: T) => T
```

## Related Topics
- [generic-types.md](01-generic-types.md)
- [generic-constraints-and-defaults.md](02-generic-constraints-and-defaults.md)
- [generic-inference.md](05-generic-inference.md)
- [Callbacks](../02-functions/02-callbacks.md)
