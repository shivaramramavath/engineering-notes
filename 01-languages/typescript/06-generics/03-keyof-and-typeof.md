# `keyof` and `typeof`

## Definition
Two **type operators** that build types from existing things:
- **`keyof T`** produces a union of the property names of `T`.
- **`typeof value`** (in a type position) produces the type of a value or variable.

## Why It Matters
They let you derive types from a single source of truth instead of writing them twice: key unions from object types, and types from runtime constants. Combined with generics they power typed property access, config objects and utility types.

## Prerequisites
[generic-constraints-and-defaults.md](02-generic-constraints-and-defaults.md), [literal types](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)

## `keyof`

### Syntax and basic example
```ts
interface User {
  id: number;
  name: string;
  email: string;
}

type UserKey = keyof User;   // "id" | "name" | "email"

function getField(user: User, key: UserKey) {
  return user[key];          // string | number
}
getField(user, "name");      // ok
getField(user, "age");       // Error
```

### Safe property access with generics
```ts
function getProp<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key];
}

const id = getProp(user, "id");     // number
const name = getProp(user, "name"); // string
```
`T[K]` is an [indexed access type](04-indexed-access-types.md).

### `keyof` with index signatures
```ts
type StringMap = { [key: string]: number };
type K = keyof StringMap;   // string | number
```
Numeric keys are allowed on string-indexed objects because JavaScript converts them to strings.

### `keyof` of arrays, tuples and primitives
```ts
type A = keyof string[];     // number | "length" | "push" | ... (all array members)
type S = keyof string;       // "length" | "charAt" | ... (members of String)
```
Rarely useful directly; use `number` for array indices.

### `keyof` and unions / intersections
```ts
type A = { a: 1; shared: 1 };
type B = { b: 2; shared: 2 };

keyof (A | B);   // "shared"            (only common keys)
keyof (A & B);   // "a" | "b" | "shared" (all keys)
```

### Typical uses
- Typed property getters/setters and `pick`-like helpers.
- Mapped types: `{ [K in keyof T]: ... }`.
- Constraining string parameters to real field names (sort keys, column names).

```ts
function sortBy<T>(items: T[], key: keyof T): T[] {
  return [...items].sort((a, b) => (a[key] > b[key] ? 1 : -1));
}
```

### `Object.keys` does not return `keyof T`
```ts
const keys = Object.keys(user);   // string[], not ("id" | "name" | "email")[]
```
Objects can have extra properties at runtime, so TypeScript is conservative. If you know the object is exact, assert narrowly with a comment:

```ts
const keys = Object.keys(user) as (keyof User)[];
```

## `typeof` (type operator)

TypeScript's `typeof` in a **type position** differs from the JavaScript `typeof` operator (which returns a string at runtime and narrows types in expressions).

```ts
const config = {
  host: "localhost",
  port: 3000,
  debug: false,
};

type Config = typeof config;   // { host: string; port: number; debug: boolean }

let c1: typeof config;          // type position -> TypeScript typeof
if (typeof c1.port === "number") {}   // expression -> JavaScript typeof, narrows
```

### Deriving types from values
```ts
function createUser(name: string, age: number) {
  return { id: crypto.randomUUID(), name, age };
}

type User = ReturnType<typeof createUser>;   // { id: string; name: string; age: number }
type Args = Parameters<typeof createUser>;   // [name: string, age: number]
```

### `keyof typeof` for runtime objects
```ts
const Status = {
  Idle: "idle",
  Loading: "loading",
  Done: "done",
} as const;

type StatusKey = keyof typeof Status;                 // "Idle" | "Loading" | "Done"
type StatusValue = (typeof Status)[StatusKey];        // "idle" | "loading" | "done"
```
Parentheses around `typeof Status` are needed before indexing. See [enums and const objects](../01-fundamentals/05-enums-and-const-objects.md).

### Arrays to unions
```ts
const roles = ["admin", "editor", "viewer"] as const;
type Role = (typeof roles)[number];   // "admin" | "editor" | "viewer"
```

### `typeof` classes and functions
```ts
class Service {}
type Ctor = typeof Service;      // the constructor (class object), not an instance
type Instance = InstanceType<typeof Service>;   // Service
```

### Restrictions
`typeof` only works on identifiers and property accesses (`typeof obj.prop`), not on arbitrary expressions or function calls:

```ts
type T = typeof fn();   // Error
```

## Common Mistakes
- Writing `keyof User` when you meant the key *values* of an object value; use `keyof typeof obj`.
- Forgetting `as const`, so `typeof obj` widens property types to `string`.
- Confusing the type-level `typeof x` with the runtime `typeof x === "..."`.
- Expecting `Object.keys(obj)` to give `keyof`.
- Indexing `typeof Status[...]` without parentheses.

## Best Practices
- Derive types from runtime constants (`as const` + `typeof`) so the list and the type cannot drift.
- Use `K extends keyof T` to constrain key arguments.
- Keep derived types near their source and export them.
- Avoid over-deriving: if a type is clearer written out, write it.

## Interview Questions
- What does `keyof` return for an interface? For an object with a string index signature?
- How do you get a union of an object's values?
- Difference between `typeof x` in a type position and in an expression?
- Why does `Object.keys` return `string[]`?

## Quick Reference
```ts
keyof T                          // union of keys
typeof value                     // type of a value
keyof typeof obj                 // keys of a runtime object
(typeof obj)[keyof typeof obj]   // union of values (use `as const`)
(typeof arr)[number]             // element union of an `as const` array
ReturnType<typeof fn>
```

## Related Topics
- [indexed-access-types.md](04-indexed-access-types.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md)
- [Literal types and const assertions](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [Function and class utilities](../07-utility-types/03-function-and-class-utilities.md)
