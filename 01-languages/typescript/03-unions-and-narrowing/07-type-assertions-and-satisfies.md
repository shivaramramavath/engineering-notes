# Type Assertions and `satisfies`

## Definition
- A **type assertion** (`value as T`) tells the compiler to treat a value as type `T`. It does not check or convert anything at runtime.
- The **non-null assertion** (`value!`) tells the compiler a value is not `null` or `undefined`.
- **`satisfies`** (`value satisfies T`) checks that a value matches `T` while keeping the value's own, more specific inferred type.

## Why It Matters
Assertions are escape hatches: they override the compiler, so they are a common source of runtime bugs. `satisfies` is the safer modern tool for the cases where you used to reach for an annotation or assertion just to get validation.

## Prerequisites
[type-narrowing.md](03-type-narrowing.md), [any and unknown](../01-fundamentals/06-any-and-unknown.md)

## Syntax

```ts
const input = document.getElementById("name") as HTMLInputElement;   // assertion
const el = document.getElementById("app")!;                          // non-null assertion
const config = { port: 3000 } satisfies { port: number };            // satisfies
```

The angle-bracket form `<HTMLInputElement>value` is equivalent but not allowed in `.tsx` files; use `as`.

## Basic Example

```ts
const canvas = document.querySelector("canvas") as HTMLCanvasElement;
canvas.getContext("2d");   // compiles; crashes at runtime if there is no canvas
```

## How It Works

### Assertions only go "sideways"
TypeScript allows `as` only when the types sufficiently overlap (one is assignable to the other):

```ts
"a" as string;          // ok (widening)
({ id: 1 }) as { id: number; name: string };   // ok (narrowing to a subtype), unsafe
"a" as number;          // Error: types do not sufficiently overlap
```

### Double assertion (almost always a mistake)
```ts
const x = "a" as unknown as number;   // compiles, completely unchecked
```
If you need this, the types are telling you something is wrong.

### Assertions are erased
There is no runtime check:
```ts
const data = JSON.parse(text) as User;   // no validation; data may be anything
```
Use [validation](../15-runtime-validation/01-schema-validation.md) at boundaries instead.

## Important Concepts

### When assertions are reasonable
1. **DOM APIs** where you know more than the types do (`getElementById` returning `HTMLElement | null`), ideally after a runtime check.
2. **Narrowing something the compiler cannot see**, with a comment explaining why it is safe.
3. **Test code** building partial fixtures.
4. **`as const`** (not a real assertion; see [literal types](02-literal-types-and-const-assertions.md)).

### Safer alternatives
```ts
// Instead of: document.getElementById("app")!
const el = document.getElementById("app");
if (!el) throw new Error("#app not found");

// Instead of: value as User
if (isUser(value)) { /* value: User */ }       // type guard
const user = UserSchema.parse(value);           // schema validation
```

### Non-null assertion `!`
Removes `null | undefined` from the type with no runtime check:
```ts
map.get("a")!.push(1);   // throws if missing
```
Fine in tests or after an explicit invariant; elsewhere prefer a check or `??`.

### `satisfies`
Validates a value against a type **without widening it to that type**.

```ts
type Color = "red" | "green" | "blue";
type Palette = Record<string, Color | [number, number, number]>;

// Annotation: validates, but loses specific info
const p1: Palette = { primary: "red", custom: [1, 2, 3] };
p1.primary.toUpperCase();   // Error: string | [number, number, number]

// satisfies: validates AND keeps literal/specific types
const p2 = { primary: "red", custom: [1, 2, 3] } satisfies Palette;
p2.primary.toUpperCase();   // ok: "red"
p2.custom.map(n => n * 2);  // ok: number[]
```

It also catches typos and missing keys:
```ts
const routes = {
  home: "/",
  about: "/about",
} satisfies Record<"home" | "about" | "contact", string>;
// Error: Property 'contact' is missing
```

### `satisfies` + `as const`
```ts
const config = {
  env: "production",
  ports: [80, 443],
} as const satisfies { env: string; ports: readonly number[] };
// keeps readonly literal types and is validated
```

### Annotation vs. assertion vs. `satisfies`

| | Checks value matches | Resulting type | Safe? |
|---|---|---|---|
| `const x: T = v` | Yes | `T` (widened) | Yes |
| `v as T` | Only overlap | `T` | No |
| `v satisfies T` | Yes | Inferred type of `v` | Yes |

### Assertion functions and guards
Prefer [type guards and assertion functions](05-type-guards-and-assertion-functions.md), which at least perform a runtime check.

### Suppression comments
`// @ts-expect-error` is better than `// @ts-ignore` because it fails when the error disappears. Use it sparingly and with an explanation.

## Common Mistakes
- Using `as` on `JSON.parse`, API responses or `unknown` instead of validating.
- Using `!` to silence "possibly undefined" without a real guarantee.
- Chaining `as unknown as T`.
- Using an annotation (`const x: Palette = ...`) when you wanted to keep specific types (use `satisfies`).
- Thinking `as` converts values (it does not; use `Number(x)`, etc.).

## Best Practices
- Treat every `as` as a debt: add a comment saying why it is safe.
- Prefer, in order: inference, annotation, `satisfies`, type guard, validation, and `as` last.
- Lint against `no-non-null-assertion` and unnecessary assertions.
- Keep assertions at the edges, never in the middle of business logic.

## Security
Assertions on untrusted input hide missing validation; see [unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md).

## Interview Questions
- Does `as` change the runtime value?
- What is the difference between `x as T` and `x satisfies T`?
- When is the non-null assertion acceptable?
- Why is `as unknown as T` a red flag?

## Quick Reference
```ts
v as T                  // assertion (unchecked)
v!                      // non-null assertion
v satisfies T           // validate, keep inferred type
{ ... } as const        // narrowest literals, readonly
{ ... } as const satisfies T
```

## Related Topics
- [type-guards-and-assertion-functions.md](05-type-guards-and-assertion-functions.md)
- [literal-types-and-const-assertions.md](02-literal-types-and-const-assertions.md)
- [Soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)
- [Common mistakes](../24-best-practices/01-common-mistakes-and-anti-patterns.md)
