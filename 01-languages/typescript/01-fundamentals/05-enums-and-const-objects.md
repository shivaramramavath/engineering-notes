# Enums and Const Objects

## Definition
An **enum** defines a named set of constants. TypeScript has its own `enum` construct; a common modern alternative is a plain object marked `as const` combined with a derived union type.

## Why It Matters
Fixed sets of values (statuses, roles, directions) appear in nearly every codebase. Choosing between enums and const objects affects bundle output, interoperability and type safety.

## Prerequisites
[type-aliases.md](04-type-aliases.md)

## Syntax

### Numeric enum
```ts
enum Direction {
  Up,      // 0
  Down,    // 1
  Left,    // 2
  Right,   // 3
}
```

### String enum
```ts
enum Status {
  Idle = "IDLE",
  Loading = "LOADING",
  Done = "DONE",
}
```

### Const object + union type
```ts
const Status = {
  Idle: "IDLE",
  Loading: "LOADING",
  Done: "DONE",
} as const;

type Status = (typeof Status)[keyof typeof Status];  // "IDLE" | "LOADING" | "DONE"
```

## Basic Example

```ts
function describe(s: Status) {
  switch (s) {
    case Status.Idle:    return "Waiting";
    case Status.Loading: return "Working";
    case Status.Done:    return "Finished";
  }
}
```

## How It Works
Enums are one of the few TypeScript features that **generate runtime code** (an object). Const objects are plain JavaScript objects; only the derived type is erased.

```ts
enum Color { Red = "RED" }
// compiles to roughly:
// var Color; (function (Color) { Color["Red"] = "RED"; })(Color || (Color = {}));
```

## Important Concepts

### Numeric enums have a reverse mapping
```ts
enum E { A }
E[0]; // "A"
```
Numeric enums also accept any number in older behaviour, which weakens safety. Prefer string enums or const objects.

### `const enum`
`const enum` is inlined at compile time and leaves no object behind. It causes problems with `isolatedModules` and with transpilers like esbuild/swc, so avoid it in application code.

### Using the object at runtime
Const objects let you iterate values easily:

```ts
Object.values(Status);   // ["IDLE", "LOADING", "DONE"]
```

### String-literal unions (the lightest option)
```ts
type Role = "admin" | "editor" | "viewer";
```
If you do not need runtime access to the list, this is simplest. See [literal types](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md).

### Comparison

| | `enum` (string) | const object + union | literal union |
|---|---|---|---|
| Runtime code | Yes | Yes (plain object) | None |
| Iterate values | Awkward | Easy | Not possible |
| Accepts plain `"IDLE"` string | No (nominal-like) | Yes | Yes |
| Works with type stripping / `isolatedModules` | Needs care | Yes | Yes |
| Interop with JSON/API strings | Needs conversion | Direct | Direct |

## Common Mistakes
- Using numeric enums with arbitrary numbers.
- Using `const enum` with transpilers that cannot inline it.
- Assuming a string enum accepts the literal string: `const s: Status = "IDLE"` is an error.
- Mixing numeric and string members in one enum.

## Best Practices
- Default to a **literal union** or **const object + derived type**.
- Use string enums when you need them (existing code, Angular conventions) and give explicit values.
- Never rely on the numeric order of enum members.

## Interview Questions
- Why do some teams avoid TypeScript enums?
- What is the difference between `enum` and `const enum`?
- How do you get a union type from an object's values?
- Do enums exist at runtime?

## Quick Reference
```ts
enum S { A = "A" }                              // string enum
const S = { A: "A" } as const;                  // const object
type S = (typeof S)[keyof typeof S];            // derive union
type S = "A" | "B";                             // literal union
```

## Related Topics
- [Literal types and const assertions](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [Type erasure](../14-type-system-internals/00-type-erasure-and-runtime.md)
