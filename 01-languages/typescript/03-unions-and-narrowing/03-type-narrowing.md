# Type Narrowing

## Definition
**Narrowing** is the compiler refining a broad type (like `string | number`) to a more specific one inside a branch, based on checks in your code. TypeScript calls the analysis **control flow analysis**.

## Why It Matters
Narrowing is how you use union values safely. Without it, every union would be unusable beyond its common members.

## Prerequisites
[union-types.md](00-union-types.md), [null and undefined](../01-fundamentals/08-null-and-undefined.md)

## Syntax and Basic Example

```ts
function print(value: string | number) {
  if (typeof value === "string") {
    value.toUpperCase();   // value: string
  } else {
    value.toFixed(2);      // value: number
  }
}
```

## How It Works: the narrowing operators

### `typeof`
Narrows to `"string" | "number" | "bigint" | "boolean" | "symbol" | "undefined" | "object" | "function"`.

```ts
if (typeof x === "number") { x; }   // number
```
Gotcha: `typeof null === "object"`, so `typeof x === "object"` includes `null`:

```ts
function f(x: string[] | null) {
  if (typeof x === "object") {
    x;   // string[] | null
  }
}
```

### Truthiness
```ts
function len(s: string | null | undefined) {
  if (s) { return s.length; }   // s: string
}
```
Careful: `0`, `""`, `NaN`, `false`, `null` and `undefined` are falsy, so `if (n)` also rejects `0`.

### Equality
```ts
function f(a: string | number, b: string | boolean) {
  if (a === b) {
    a;   // string (the only common type)
  }
}

function g(x: string | null | undefined) {
  if (x != null) { x; }   // string (!= null removes null and undefined)
}
```

### `instanceof`
```ts
function format(d: Date | string) {
  return d instanceof Date ? d.toISOString() : d;
}
```
Works for classes and constructor functions, not interfaces or type aliases (they do not exist at runtime).

### `in`
Checks for a property and narrows to members that may have it:

```ts
type Fish = { swim(): void };
type Bird = { fly(): void };

function move(pet: Fish | Bird) {
  if ("swim" in pet) pet.swim();   // Fish
  else pet.fly();                  // Bird
}
```
Prefer a [discriminant property](04-discriminated-unions.md) over `in` when you design the types.

### `Array.isArray`
```ts
function f(x: string | string[]) {
  return Array.isArray(x) ? x.join(",") : x;
}
```

### Assignment narrowing
```ts
let x: string | number = "a";
x;            // string
x = 5;
x;            // number
```

### Optional chaining and `??`
These handle `null`/`undefined` without explicit narrowing:
```ts
const city = user?.address?.city;
```

### Early return and `throw`
```ts
function f(x: string | undefined) {
  if (!x) return;
  x;   // string for the rest of the function
}
```

### Custom guards
When built-in checks are not enough, write a [type guard](05-type-guards-and-assertion-functions.md).

## Important Concepts

### Narrowing follows control flow
```ts
function f(x: string | number | null) {
  if (x === null) return;
  x;                              // string | number
  if (typeof x === "string") return;
  x;                              // number
}
```

### Narrowing of `const` vs `let` and closures
Narrowing of a `const` or parameter persists inside callbacks. For `let` variables that are reassigned, narrowing may be lost inside closures (newer TypeScript versions preserve it when there are no later assignments):

```ts
function f(x: string | undefined) {
  if (x === undefined) return;
  [1, 2].map(n => x.length + n);   // ok: x is a parameter that is not reassigned
}
```

### Narrowing property access paths
```ts
if (user.address !== undefined) {
  user.address.city;   // ok, until something that could change user.address
}
```
Function calls between the check and the use do not reset narrowing of `readonly`/property paths in most cases, but mutations through other references are invisible to the compiler. Copy to a local `const` for certainty:

```ts
const { address } = user;
if (address) address.city;
```

### Narrowing does not cross function boundaries
```ts
function isOk(x: string | undefined) { return x !== undefined; }

const s: string | undefined = get();
if (isOk(s)) {
  s.length;   // Error in older versions: s still string | undefined
}
```
Use a [type predicate](05-type-guards-and-assertion-functions.md) (`x is string`). Recent TypeScript versions can infer simple predicates, but do not rely on it for public APIs.

### The `unknown` workflow
```ts
function parse(value: unknown) {
  if (typeof value === "object" && value !== null && "id" in value) {
    value.id;   // unknown (property exists, type is unknown)
  }
}
```

## Common Mistakes
- Using truthiness when `0` or `""` are valid values.
- Forgetting `typeof null === "object"`.
- Using `instanceof` with an interface or type alias.
- Expecting a helper function's boolean result to narrow the argument.
- Relying on narrowing after the value may have been mutated elsewhere.

## Best Practices
- Prefer `=== undefined` / `=== null` (or `== null` deliberately) over truthiness for optionals.
- Narrow early and return early to keep code flat.
- Copy nested values into local `const` variables before narrowing.
- Design unions with a discriminant so narrowing is a single `switch`.

## Debugging
Hover over the variable inside each branch to see the narrowed type. When a type is not narrowing, check whether it is a `let`, a property of a mutable object, or a result of a function call.

## Interview Questions
- Name five ways to narrow a type.
- Why does `typeof x === "object"` not exclude `null`?
- Why can't `instanceof` narrow to an interface?
- Why might narrowing be lost inside a callback?

## Quick Reference

| Check | Narrows |
|---|---|
| `typeof x === "string"` | Primitives, `"function"` |
| `x instanceof Date` | Class instances |
| `"key" in x` | Object unions |
| `x === 1`, `x !== null` | Equality, literals |
| `if (x)` | Truthy values |
| `Array.isArray(x)` | Arrays |
| `x.kind === "a"` | Discriminated unions |
| `isFoo(x)` (predicate) | Custom |

## Related Topics
- [discriminated-unions.md](04-discriminated-unions.md)
- [type-guards-and-assertion-functions.md](05-type-guards-and-assertion-functions.md)
- [exhaustiveness-checking.md](06-exhaustiveness-checking.md)
- [any and unknown](../01-fundamentals/06-any-and-unknown.md)
