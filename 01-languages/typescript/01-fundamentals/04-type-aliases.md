# Type Aliases

## Definition
A **type alias** gives a name to any type using the `type` keyword. It does not create a new type; it is another name for an existing one.

## Why It Matters
Names make types reusable, readable and easy to change in one place. Aliases are also the only way to name unions, tuples, function types and mapped types.

## Prerequisites
[objects.md](03-objects.md)

## Syntax

```ts
type UserId = number;
type Point = { x: number; y: number };
type Status = "idle" | "loading" | "done";
type Pair = [string, number];
type Handler = (event: string) => void;
```

## Basic Example

```ts
type User = {
  id: number;
  name: string;
  email?: string;
};

function sendEmail(user: User) {
  if (user.email) {
    console.log(`Sending to ${user.email}`);
  }
}
```

## How It Works
An alias is substituted wherever it is used; there is no runtime representation. Two aliases with the same shape are fully interchangeable (structural typing):

```ts
type A = { x: number };
type B = { x: number };
const a: A = { x: 1 };
const b: B = a;   // fine
```

## Important Concepts

### Aliases can name anything
```ts
type ID = string | number;                    // union
type Callback = (err: Error | null) => void;  // function
type Matrix = number[][];                     // array
type Keys = keyof User;                       // computed type
```

### Combining with intersections
```ts
type Timestamps = { createdAt: Date; updatedAt: Date };
type Post = { title: string } & Timestamps;
```

### Generic aliases
```ts
type Nullable<T> = T | null;
type ApiResponse<T> = { data: T; error?: string };

const r: ApiResponse<User[]> = { data: [] };
```

### Recursive aliases
```ts
type Json =
  | string | number | boolean | null
  | Json[]
  | { [key: string]: Json };
```

### Aliases are not nominal
`type UserId = number` does not stop you passing any `number` where a `UserId` is expected. For that, see [branded types](../10-advanced-types/07-branded-types.md).

### Alias vs interface (short version)
Use `type` for unions, tuples, functions and computed types. For object shapes either works; the full decision guide is in [interface-vs-type.md](../04-objects-and-interfaces/01-interface-vs-type.md).

## Common Mistakes
- Believing an alias creates a distinct type.
- Using an alias that only wraps a primitive (`type Name = string`) with no extra meaning; it adds a name but no safety.
- Giving aliases vague names (`Data`, `Info`, `Thing`).
- Declaring the same alias twice (aliases cannot be merged, unlike interfaces).

## Best Practices
- Use PascalCase names that describe the domain (`OrderStatus`, not `Type1`).
- Export types that are part of a module's public API.
- Prefer small aliases that compose over one giant type.

## Interview Questions
- Can a type alias be extended? (via `&`, not `extends`)
- Can type aliases be merged like interfaces? (No)
- When must you use `type` rather than `interface`? (unions, tuples, mapped/conditional types)

## Quick Reference
```ts
type Name = string;
type Obj = { a: number };
type Union = "a" | "b";
type Fn = (x: number) => string;
type Generic<T> = { value: T };
```

## Related Topics
- [objects.md](03-objects.md)
- [Interface vs type](../04-objects-and-interfaces/01-interface-vs-type.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)
- [Generic types](../06-generics/01-generic-types.md)
