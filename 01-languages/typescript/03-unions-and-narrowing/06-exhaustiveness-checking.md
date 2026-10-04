# Exhaustiveness Checking

## Definition
**Exhaustiveness checking** makes the compiler report an error when a `switch` or `if` chain over a union does not handle every member. It relies on the `never` type: after all members are handled, the remaining type is `never`.

## Why It Matters
Unions grow. Without exhaustiveness checks, adding a new variant compiles cleanly and then fails at runtime in code that forgot to handle it. With them, the compiler lists every place to update.

## Prerequisites
[discriminated-unions.md](04-discriminated-unions.md), [never and void](../01-fundamentals/07-never-and-void.md)

## Syntax

```ts
function assertNever(value: never): never {
  throw new Error(`Unhandled value: ${JSON.stringify(value)}`);
}
```

## Basic Example

```ts
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "square"; size: number };

function area(s: Shape): number {
  switch (s.kind) {
    case "circle": return Math.PI * s.radius ** 2;
    case "square": return s.size ** 2;
    default:       return assertNever(s);   // s is never here
  }
}
```

Add `| { kind: "triangle"; base: number; height: number }` to `Shape` and the `default` line now errors: `Argument of type '{ kind: "triangle"; ... }' is not assignable to parameter of type 'never'`. The compiler tells you exactly what you forgot.

## How It Works
Each `case` narrows `s`. After the handled cases, `s` has type `never` in `default`. Passing `never` to a parameter typed `never` compiles; any leftover member is not assignable to `never`, so you get an error. The runtime `throw` also protects against bad data that slipped past the types.

## Important Concepts

### Inline version
```ts
default: {
  const _exhaustive: never = s;
  throw new Error(`Unhandled: ${JSON.stringify(_exhaustive)}`);
}
```

### Using the return type
If a function declares a return type that does not include `undefined`, a `switch` missing a case triggers `Function lacks ending return statement and return type does not include 'undefined'`. This works without a `default`, as in the `area` example in [discriminated-unions.md](04-discriminated-unions.md), but gives a vaguer message and does not guard the runtime.

### `satisfies never`
```ts
default:
  s satisfies never;
  throw new Error("unreachable");
```
A compact, compile-time-only check.

### Exhaustive object lookups
Use a mapped type to force a handler for each member:

```ts
type Kind = Shape["kind"];

const labels: Record<Kind, string> = {
  circle: "Circle",
  square: "Square",
};   // adding a new kind errors until you add a label
```

### Exhaustive handlers keyed by tag
```ts
type Handlers = { [K in Shape["kind"]]: (s: Extract<Shape, { kind: K }>) => number };

const areaHandlers: Handlers = {
  circle: s => Math.PI * s.radius ** 2,
  square: s => s.size ** 2,
};
```

### Lint rule
`@typescript-eslint/switch-exhaustiveness-check` reports non-exhaustive `switch` statements, including without a `default`. See [linting and formatting](../21-production-tooling/00-linting-and-formatting.md).

### Exhaustiveness for literal unions and enums
Works the same on `"a" | "b"` and on enum members.

### Where `default` can hide bugs
A catch-all `default: return 0` makes a new variant silently take that path. Use `assertNever` in `default` unless you intentionally want a fallback.

## Common Mistakes
- Using `default: return somethingDefault` and losing the check.
- Switching on a non-literal discriminant (`string`), so `never` is never reached.
- Handling cases with `if` chains but no final `never` check.
- Swallowing the `assertNever` error at runtime (it should surface as a bug).

## Best Practices
- End every `switch` over a union with `assertNever` (or `satisfies never`).
- Enable the ESLint exhaustiveness rule.
- Prefer `Record<Union, ...>` for maps from variants to data.
- Keep a single shared `assertNever` helper.

## Interview Questions
- How do you make TypeScript error when a union member is not handled?
- Why is the value `never` in the `default` branch?
- What are the risks of a `default` branch in a `switch` over a union?
- How can `Record<Union, T>` act as an exhaustiveness check?

## Quick Reference
```ts
default: return assertNever(x);
default: { const _: never = x; throw new Error(); }
default: x satisfies never;
const map: Record<Union, T> = { /* all keys required */ };
```

## Related Topics
- [discriminated-unions.md](04-discriminated-unions.md)
- [never and void](../01-fundamentals/07-never-and-void.md)
- [State machines](../17-design-patterns/06-state-machines.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md)
