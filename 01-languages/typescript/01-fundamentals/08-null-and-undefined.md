# `null` and `undefined`

## Definition
`undefined` means a value has not been assigned; `null` means an intentional absence of a value. In TypeScript, each is its own type, and with `strictNullChecks` neither is assignable to other types unless you say so.

## Why It Matters
"Cannot read properties of undefined" is the most common JavaScript runtime error. `strictNullChecks` (part of `strict`) turns it into a compile-time error.

## Prerequisites
[primitive-types.md](01-primitive-types.md)

## Syntax

```ts
let a: string | null = null;
let b: number | undefined = undefined;
let c: string | null | undefined;
```

## Basic Example

```ts
function length(s: string | null): number {
  return s.length;       // Error: 's' is possibly 'null'
}

function safeLength(s: string | null): number {
  return s === null ? 0 : s.length;   // narrowed to string
}
```

## How It Works
With `strictNullChecks` **off**, `null` and `undefined` are assignable to every type, and the error above disappears. Always keep it on (it is included in `strict`).

TypeScript narrows away `null`/`undefined` using checks it understands:

```ts
function f(x: string | undefined) {
  if (x) { x; }               // string (truthy)
  if (x !== undefined) { x; } // string
  if (x != null) { x; }       // string (removes both null and undefined)
}
```

## Important Concepts

### Optional chaining `?.`
Stops and returns `undefined` if the left side is `null`/`undefined`.
```ts
const city = user?.address?.city;      // string | undefined
const first = list?.[0];
const result = callback?.();
```

### Nullish coalescing `??`
Falls back only for `null`/`undefined` (unlike `||`, which also triggers on `0`, `""`, `false`).
```ts
const port = config.port ?? 3000;      // 0 is kept
const bad = config.port || 3000;       // 0 becomes 3000
```

### Non-null assertion `!`
Tells the compiler "this is not null". It does **no** runtime check.
```ts
const el = document.getElementById("app")!;   // crashes if missing
```
Use sparingly; prefer a real check:
```ts
const el = document.getElementById("app");
if (!el) throw new Error("#app not found");
```

### Optional vs. `| undefined`
```ts
type A = { x?: number };            // property may be missing, or undefined
type B = { x: number | undefined }; // property must exist, may be undefined
```
Enable `exactOptionalPropertyTypes` to treat these differently.

### Optional parameters
```ts
function greet(name?: string) {
  return `Hi ${name ?? "guest"}`;
}
```

### Nullish assignment
```ts
cache.value ??= computeValue();   // assigns only if null/undefined
```

### `null` vs `undefined`: conventions
- `undefined`: not set, missing property, no return value, unset optional.
- `null`: explicitly empty (JSON `null`, database nulls, "no selection").
Pick one convention per codebase and convert at boundaries (API <-> domain).

### Arrays and lookup results
`Map.get`, `Array.find` and `Record` lookups can return `undefined`. Handle it:
```ts
const user = users.find(u => u.id === id);   // User | undefined
if (!user) throw new Error("Not found");
user.name;                                   // User
```

## Common Mistakes
- Turning off `strictNullChecks` to "fix" errors.
- Overusing `!`, which hides real bugs.
- Using `||` for defaults when `0`, `""` or `false` are valid values.
- Mixing `null` and `undefined` inconsistently across an API.

## Best Practices
- Keep `strict: true`.
- Prefer `??` and `?.` over manual checks where readable.
- Return `T | undefined` from lookups and force callers to handle absence.
- Validate nullable input at the boundary so the inside of your app deals with real values.

## Interview Questions
- What does `strictNullChecks` change?
- Difference between `??` and `||`?
- What does the `!` operator do, and what are its risks?
- Difference between `x?: number` and `x: number | undefined`?

## Quick Reference
```ts
string | null | undefined    // nullable
a?.b?.c                      // optional chaining
a ?? "default"               // nullish coalescing
a ??= "default"              // nullish assignment
a!                           // non-null assertion (unsafe)
```

## Related Topics
- [Type narrowing](../03-unions-and-narrowing/03-type-narrowing.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)
- [Readonly and optional properties](../04-objects-and-interfaces/02-readonly-and-optional-properties.md)
- [Strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)
- [exclude-extract-nonnullable.md](../07-utility-types/02-exclude-extract-nonnullable.md)
