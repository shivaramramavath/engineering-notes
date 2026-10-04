# Literal Types and Const Assertions

## Definition
A **literal type** is a type that contains exactly one value: `"GET"`, `42`, `true`. A **const assertion** (`as const`) tells the compiler to infer the narrowest, readonly literal types for an expression.

## Why It Matters
Literal types let you restrict values to an exact set (HTTP methods, event names, config keys) and are the basis of discriminated unions. `as const` stops TypeScript from widening your data to `string` and `number`.

## Prerequisites
[union-types.md](00-union-types.md), [types-and-inference](../01-fundamentals/00-types-and-inference.md)

## Syntax

```ts
type Method = "GET" | "POST";
type Dice = 1 | 2 | 3 | 4 | 5 | 6;
type Yes = true;

let m: Method = "GET";
m = "PATCH";   // Error: '"PATCH"' is not assignable to type 'Method'
```

## Basic Example

```ts
function request(url: string, method: "GET" | "POST") { /* ... */ }

request("/a", "GET");      // ok
request("/a", "get");      // Error

const m = "POST";          // type "POST"
request("/a", m);          // ok

let m2 = "POST";           // type string (widened)
request("/a", m2);         // Error: string is not assignable to "GET" | "POST"
```

## How It Works

### Widening
- `const x = "a"` keeps the literal type `"a"`.
- `let x = "a"` is widened to `string` because it can be reassigned.
- Properties of object literals are widened even under `const`:

```ts
const req = { url: "/a", method: "GET" };
// type: { url: string; method: string }

request(req.url, req.method);   // Error: method is string
```

### Fixing it with `as const`
```ts
const req = { url: "/a", method: "GET" } as const;
// type: { readonly url: "/a"; readonly method: "GET" }

request(req.url, req.method);   // ok
```

### Other ways to fix it
```ts
const req1 = { url: "/a", method: "GET" as const };            // assert one property
const req2: { url: string; method: "GET" | "POST" } = { ... }; // annotate
```

## Important Concepts

### `as const` on arrays gives readonly tuples
```ts
const roles = ["admin", "editor", "viewer"] as const;
// readonly ["admin", "editor", "viewer"]

type Role = (typeof roles)[number];   // "admin" | "editor" | "viewer"
```
This is the best way to keep a runtime list and its type in sync.

### `as const` on objects
```ts
const Status = { Idle: "IDLE", Done: "DONE" } as const;
type Status = (typeof Status)[keyof typeof Status];   // "IDLE" | "DONE"
```
See [enums and const objects](../01-fundamentals/05-enums-and-const-objects.md).

### `as const` effects
1. Literals do not widen.
2. Object properties become `readonly`.
3. Arrays become `readonly` tuples.

It only works on literal expressions, not on variables or function calls.

### Literal unions in function signatures
Prefer them to booleans and magic strings:
```ts
function setAlign(align: "left" | "center" | "right") {}
```

### Number, boolean and bigint literals
```ts
type HttpOk = 200 | 201 | 204;
type Truthy = true;
```
`boolean` is itself the union `true | false`.

### Template literal types (preview)
```ts
type Event = `on${"Click" | "Hover"}`;   // "onClick" | "onHover"
```
See [template literal types](../10-advanced-types/04-template-literal-types.md).

### `const` type parameters (TS 5.0+)
```ts
function tuple<const T extends readonly unknown[]>(items: T): T { return items; }
const t = tuple(["a", 1]);   // readonly ["a", 1]
```
Lets callers get literal inference without writing `as const`.

### Enum-like narrowing by literal comparison
```ts
function handle(m: "GET" | "POST") {
  if (m === "GET") { /* m: "GET" */ } else { /* m: "POST" */ }
}
```

## Common Mistakes
- Using `let` for values that should keep their literal type.
- Forgetting object properties widen even under `const`.
- Writing `as const` on a variable (`x as const`) rather than a literal.
- Expecting `as const` to make the value immutable at runtime (it is compile-time only).
- Writing out literal unions by hand when they could be derived from a runtime array/object.

## Best Practices
- Derive literal unions from a single `as const` source so list and type cannot drift.
- Use literal unions instead of `string` for parameters with a fixed set of values.
- Use `satisfies` together with `as const` to check shape without losing literals; see [type-assertions-and-satisfies.md](07-type-assertions-and-satisfies.md).

## Interview Questions
- Why is `const x = "a"` typed `"a"` but `const o = { x: "a" }` has `x: string`?
- What does `as const` do?
- How do you create a union type from the values of an array?
- Difference between `as const` and `readonly`?

## Quick Reference
```ts
type A = "a" | "b";                       // literal union
const x = ["a", "b"] as const;            // readonly tuple
type X = (typeof x)[number];              // "a" | "b"
const o = { k: "v" } as const;            // readonly literals
```

## Related Topics
- [union-types.md](00-union-types.md)
- [Enums and const objects](../01-fundamentals/05-enums-and-const-objects.md)
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [Type widening and inference](../14-type-system-internals/03-type-widening-and-inference.md)
