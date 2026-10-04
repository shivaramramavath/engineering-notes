# Discriminated Unions

## Definition
A **discriminated union** (tagged union) is a union of object types that share a common property, the **discriminant**, whose type is a different literal in each member. Checking the discriminant narrows the whole object.

## Why It Matters
This is the standard way to model data that has several distinct shapes: API results, UI state, events, commands, AST nodes. It makes impossible states unrepresentable and gives you exhaustive, compiler-checked branching.

## Prerequisites
[union-types.md](00-union-types.md), [literal-types-and-const-assertions.md](02-literal-types-and-const-assertions.md), [type-narrowing.md](03-type-narrowing.md)

## Syntax

```ts
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "rectangle"; width: number; height: number }
  | { kind: "square"; size: number };
```

`kind` is the discriminant. Each member has a unique literal value for it.

## Basic Example

```ts
function area(shape: Shape): number {
  switch (shape.kind) {
    case "circle":
      return Math.PI * shape.radius ** 2;        // shape: circle member
    case "rectangle":
      return shape.width * shape.height;
    case "square":
      return shape.size ** 2;
  }
}
```

## How It Works
Checking `shape.kind` against a literal narrows `shape` to the members whose `kind` can equal that literal. Inside each `case`, only that member's properties are available.

Works with `switch`, `if`/`else`, `===`, `!==`, and with destructured discriminants:

```ts
function f(shape: Shape) {
  const { kind } = shape;
  if (kind === "circle") {
    shape.radius;   // narrowed (supported for const destructuring in TS 4.6+)
  }
}
```

## Important Concepts

### Modelling state: "make illegal states unrepresentable"

Bad: one type with flags and optionals
```ts
type Request = {
  loading: boolean;
  data?: User;
  error?: string;
};
// allows { loading: true, data: ..., error: "x" }, and nonsense combos
```

Good: each state carries only what it needs
```ts
type Request =
  | { status: "idle" }
  | { status: "loading" }
  | { status: "success"; data: User }
  | { status: "error"; error: string };

function render(r: Request) {
  switch (r.status) {
    case "idle":    return "Start";
    case "loading": return "Loading...";
    case "success": return r.data.name;
    case "error":   return r.error;
  }
}
```

### Result type
```ts
type Result<T, E = Error> =
  | { ok: true; value: T }
  | { ok: false; error: E };

function parse(s: string): Result<number, string> {
  const n = Number(s);
  return Number.isNaN(n) ? { ok: false, error: "NaN" } : { ok: true, value: n };
}

const r = parse("5");
if (r.ok) r.value;   // number
else r.error;        // string
```
Booleans work as discriminants too. See [result pattern](../11-error-handling/02-result-pattern.md).

### Discriminants must be literal types
`string` or `number` as the tag does not narrow. Valid discriminants: string/number/boolean literals, `null`, `undefined`, enum members.

### Extracting a member
```ts
type Circle = Extract<Shape, { kind: "circle" }>;
type Kinds = Shape["kind"];   // "circle" | "rectangle" | "square"
```

### Events and actions (Redux-style)
```ts
type Action =
  | { type: "add"; item: string }
  | { type: "remove"; index: number }
  | { type: "clear" };

function reducer(state: string[], action: Action): string[] {
  switch (action.type) {
    case "add":    return [...state, action.item];
    case "remove": return state.filter((_, i) => i !== action.index);
    case "clear":  return [];
  }
}
```

### Adding a variant
When you add a new member, every `switch` that is [exhaustive](06-exhaustiveness-checking.md) now errors until updated. This is the main benefit over class hierarchies or flags.

### Alternatives to discriminants
- Unions without a tag need `in` or custom [type guards](05-type-guards-and-assertion-functions.md); they are fragile if shapes overlap.
- Class hierarchies with `instanceof` work, but cannot be serialized as plain data.

## Common Mistakes
- Using `string` instead of a literal for the tag.
- Giving two members the same tag value.
- Making the discriminant optional on some members.
- Modelling with one object type full of optional properties.
- Narrowing on a property that is not a unique literal (for example `type: string`).

## Best Practices
- Use a consistent discriminant name across your codebase (`kind`, `type` or `status`).
- Put shared fields in a base type and intersect: `type Event = Base & ({ kind: "a" } | { kind: "b" })`.
- Always pair `switch` with an [exhaustiveness check](06-exhaustiveness-checking.md).
- Validate external data into a discriminated union at the boundary; see [schema validation](../15-runtime-validation/01-schema-validation.md).

## Interview Questions
- What is a discriminated union and what makes a property a valid discriminant?
- How would you model a request with idle/loading/success/error states?
- How do you extract one member of a discriminated union?
- Why are discriminated unions preferred over optional-field objects?

## Quick Reference
```ts
type T =
  | { kind: "a"; x: number }
  | { kind: "b"; y: string };

switch (t.kind) { case "a": t.x; break; case "b": t.y; break; }
Extract<T, { kind: "a" }>
T["kind"]
```

## Related Topics
- [exhaustiveness-checking.md](06-exhaustiveness-checking.md)
- [type-narrowing.md](03-type-narrowing.md)
- [State machines](../17-design-patterns/06-state-machines.md)
- [Type design principles](../24-best-practices/00-type-design-principles.md)
