# `any` and `unknown`

## Definition
- **`any`** turns off type checking for a value. Anything can be assigned to it, and it can be used as anything.
- **`unknown`** is the type-safe counterpart: anything can be assigned to it, but you must narrow it before using it.

## Why It Matters
`any` silently removes the safety TypeScript provides and spreads through code. `unknown` is the correct type for values whose shape you genuinely do not know yet (parsed JSON, caught errors, external input).

## Prerequisites
[types-and-inference.md](00-types-and-inference.md), [objects.md](03-objects.md)

## Syntax

```ts
let a: any = 5;
let u: unknown = 5;
```

## Basic Example

```ts
let a: any = "hello";
a.toFixed();        // no error, crashes at runtime
a.foo.bar.baz;      // no error

let u: unknown = "hello";
u.toUpperCase();    // Error: 'u' is of type 'unknown'

if (typeof u === "string") {
  u.toUpperCase();  // ok: narrowed to string
}
```

## How It Works
- `any` is both a top type (everything is assignable **to** it) and effectively a bottom type (it is assignable **to** everything). This is what makes it unsafe.
- `unknown` is only a top type. You cannot use it, or assign it to a specific type, without narrowing or an assertion.

```ts
let a: any = 1;
let s1: string = a;      // allowed, no check

let u: unknown = 1;
let s2: string = u;      // Error
```

## Important Concepts

### `any` is contagious
```ts
const data: any = JSON.parse(text);
const name = data.user.name;   // name is any
const upper = name.toUpperCase(); // upper is any
```
One `any` poisons everything derived from it.

### Narrowing `unknown`
```ts
function format(value: unknown): string {
  if (typeof value === "string") return value.trim();
  if (typeof value === "number") return value.toFixed(2);
  if (value instanceof Date) return value.toISOString();
  return String(value);
}
```
More in [type narrowing](../03-unions-and-narrowing/03-type-narrowing.md).

### Validating external data
```ts
function isUser(x: unknown): x is { id: number; name: string } {
  return (
    typeof x === "object" && x !== null &&
    "id" in x && typeof (x as any).id === "number" &&
    "name" in x && typeof (x as any).name === "string"
  );
}
```
For real projects use a schema library; see [runtime validation](../15-runtime-validation/README.md).

### `unknown` in `catch`
With `strict` (`useUnknownInCatchVariables`), caught values are `unknown`:
```ts
try { risky(); }
catch (err) {
  if (err instanceof Error) console.error(err.message);
}
```

### Implicit `any`
`noImplicitAny` (part of `strict`) makes the compiler error when it would silently infer `any`:
```ts
function log(x) {}   // Error: Parameter 'x' implicitly has an 'any' type
```

### Controlled escape hatches
- `// @ts-expect-error` documents a known error and fails when it stops being an error.
- A type assertion (`value as Foo`) is a promise to the compiler; see [type assertions](../03-unions-and-narrowing/07-type-assertions-and-satisfies.md).

## Common Mistakes
- Reaching for `any` to silence an error instead of fixing the type.
- Typing `JSON.parse` results as `any` and never validating.
- Using `any[]` or `Function` instead of precise types.
- Assuming `unknown` and `any` behave the same.

## Best Practices
- Enable `strict` and treat `any` as a code smell; use ESLint (`no-explicit-any`) to enforce it.
- Use `unknown` for untrusted data, then narrow or validate.
- Use generics, unions or overloads when you need flexibility instead of `any`.
- When you must use `any`, contain it in a small, commented spot.

## Security
`any` removes checks at exactly the points where untrusted data enters; see [unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md).

## Interview Questions
- Difference between `any` and `unknown`?
- Why is `unknown` safer?
- What happens to the type of an expression derived from an `any` value?
- What does `noImplicitAny` do?

## Quick Reference

| | `any` | `unknown` |
|---|---|---|
| Assign anything to it | Yes | Yes |
| Use it without checks | Yes | No |
| Assign it to other types | Yes | Only `unknown`/`any` |
| Safety | None | Full |

## Related Topics
- [never-and-void.md](07-never-and-void.md)
- [Type narrowing](../03-unions-and-narrowing/03-type-narrowing.md)
- [Trust boundaries](../15-runtime-validation/00-trust-boundaries.md)
- [Soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)
