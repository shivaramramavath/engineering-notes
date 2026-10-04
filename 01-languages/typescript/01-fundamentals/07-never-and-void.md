# `never` and `void`

## Definition
- **`void`**: the return type of a function that returns no useful value.
- **`never`**: the type of values that can never occur. Used for functions that never return and for impossible branches.

## Why It Matters
`void` documents "this function is for its effect". `never` is the foundation of exhaustiveness checking, which makes unions safe to extend.

## Prerequisites
[any-and-unknown.md](06-any-and-unknown.md)

## Syntax

```ts
function log(msg: string): void {
  console.log(msg);
}

function fail(msg: string): never {
  throw new Error(msg);
}
```

## Basic Example

```ts
function greet(name: string): void {
  console.log(`Hi ${name}`);
}

const result = greet("Ada");   // result is void; do not use it
```

## How It Works

### `void`
A function typed to return `void` can still return a value in some positions, and the value is ignored. This matters for callbacks:

```ts
type Callback = () => void;

const cb: Callback = () => 42;   // allowed: the return value is ignored
```
This is why `[1, 2, 3].forEach(n => results.push(n))` compiles even though `push` returns a number.

A function **declared** with `: void` cannot return a value:
```ts
function f(): void {
  return 1;  // Error
}
```

### `void` vs `undefined`
```ts
function a(): void {}
function b(): undefined { return undefined; }   // must explicitly return
```
`undefined` is an actual value type; `void` means "ignore the result".

### `never`
`never` is the empty set: no value has this type. It appears when:

1. A function always throws or loops forever.
2. Narrowing has eliminated every possibility.
3. An intersection of incompatible types is formed.

```ts
function loop(): never {
  while (true) {}
}

type Impossible = string & number;   // never
```

### Exhaustiveness checking
```ts
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "square"; size: number };

function area(s: Shape): number {
  switch (s.kind) {
    case "circle": return Math.PI * s.radius ** 2;
    case "square": return s.size ** 2;
    default: {
      const _exhaustive: never = s;   // error if a new variant is added
      return _exhaustive;
    }
  }
}
```
If you later add `{ kind: "triangle" }` to `Shape`, the `default` branch errors until you handle it. See [exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md).

### `never` in unions disappears
```ts
type A = string | never;   // string
```
Conditional and mapped types use this to filter members out; see [conditional types](../10-advanced-types/00-conditional-types.md).

### `never` is assignable to everything
`never` is the bottom type, so it fits anywhere; but nothing (except `never`) is assignable to it.

## Common Mistakes
- Using `void` for a value you actually intend to return.
- Annotating a function `never` when it can return normally.
- Forgetting the exhaustive `never` check, then silently ignoring new union members.
- Confusing `void` with `undefined`.

## Best Practices
- Mark functions that only throw (e.g., `assertNever`, `fail`) as `never`.
- Add an exhaustiveness helper:

```ts
function assertNever(x: never): never {
  throw new Error(`Unexpected value: ${JSON.stringify(x)}`);
}
```

- Use `void` for callbacks whose result is ignored.

## Interview Questions
- What is the difference between `void` and `undefined`?
- When does TypeScript infer `never`?
- How does `never` help with exhaustive `switch` statements?
- Why does `string | never` simplify to `string`?

## Quick Reference
```ts
(): void            // no useful return
(): never           // never returns normally
string & number     // never
const x: never = v  // exhaustiveness check
```

## Related Topics
- [any-and-unknown.md](06-any-and-unknown.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)
- [Function types](../02-functions/00-function-types.md)
