# Indexed Access Types

## Definition
An **indexed access type** `T[K]` looks up the type of property `K` on type `T`, just like `obj["key"]` looks up a value, but at the type level.

## Why It Matters
It lets you reuse part of an existing type (a field, an element, a nested property) without copying it. When the source type changes, everything derived from it updates automatically.

## Prerequisites
[keyof-and-typeof.md](03-keyof-and-typeof.md), [generic constraints](02-generic-constraints-and-defaults.md)

## Syntax

```ts
interface User {
  id: number;
  name: string;
  address: { city: string; zip: string };
  roles: string[];
}

type Id = User["id"];                     // number
type City = User["address"]["city"];      // string
type Roles = User["roles"];               // string[]
```

## Basic Example

```ts
function updateName(id: User["id"], name: User["name"]) { /* ... */ }
updateName(1, "Ada");
updateName("1", "Ada");   // Error: string is not assignable to number
```
If `User["id"]` becomes `string` later, `updateName` follows automatically.

## How It Works

### The index must be a type that is a valid key
```ts
type A = User["nope"];    // Error: Property 'nope' does not exist on type 'User'
```
The index is a **type**, not a value. To use a variable's key, use `typeof` or a generic `K extends keyof T`.

### Unions of keys give unions of values
```ts
type IdOrName = User["id" | "name"];   // number | string
type AnyValue = User[keyof User];      // union of every property type
```

### Array and tuple element types with `number`
```ts
type Role = User["roles"][number];     // string

type Tuple = [string, number, boolean];
type First = Tuple[0];                 // string
type Elem = Tuple[number];             // string | number | boolean
type Len = Tuple["length"];            // 3
```
`T[number]` is the standard way to get an array's element type.

### Deriving from `as const` arrays
```ts
const methods = ["GET", "POST", "PUT"] as const;
type Method = (typeof methods)[number];   // "GET" | "POST" | "PUT"
```

### Nested access
```ts
type Zip = User["address"]["zip"];
```
Each step must exist; optional properties add `undefined`:

```ts
interface Profile { bio?: { text: string } }
type Bio = Profile["bio"];                // { text: string } | undefined
type Text = NonNullable<Profile["bio"]>["text"];   // string
```

### Discriminated unions: get the tags
```ts
type Shape =
  | { kind: "circle"; radius: number }
  | { kind: "square"; size: number };

type Kind = Shape["kind"];   // "circle" | "square"
```
Indexing a union returns the union of that property across members, and only works if the property exists on **all** members.

### In generic functions
```ts
function getProp<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key];
}
```
The return type depends on both arguments, so `getProp(user, "name")` is `string` and `getProp(user, "id")` is `number`.

### Inside function types
```ts
type Handler = (event: { type: string; payload: unknown }) => void;
type Event = Parameters<Handler>[0];
type Payload = Event["payload"];
```

### Mapped types use the same lookup
```ts
type Getters<T> = { [K in keyof T]: () => T[K] };
```
See [mapped types](../10-advanced-types/03-mapped-types.md).

### Index signatures
```ts
type Dict = { [key: string]: number };
type V = Dict[string];   // number
```

### Readonly and optional modifiers
Indexed access returns the property's **declared type** (including `| undefined` for optional ones); modifiers like `readonly` do not appear in the result type.

## Common Mistakes
- Trying to index with a value (`User[key]`) instead of a type (`User["id"]`).
- Forgetting `NonNullable` before drilling into optional properties.
- Indexing a union by a key that is missing from one member.
- Copy-pasting field types instead of deriving them.
- Using `User[keyof User]` when a specific field is intended.

## Best Practices
- Derive field types from the source (`Order["status"]`) rather than redefining them.
- Use `T[number]` for array elements and `T[keyof T]` for value unions.
- Combine with `K extends keyof T` for precise generic accessors.
- Keep chains short; very long chains are hard to read, so name an intermediate type.

## Interview Questions
- What does `T[K]` mean?
- How do you get the element type of an array type?
- What is `T[keyof T]`?
- Why can't you write `User[someVariable]` in a type?

## Quick Reference
```ts
T["a"]                    // property type
T["a" | "b"]              // union of two properties
T[keyof T]                // all value types
T["a"]["b"]               // nested
T[number]                 // array/tuple element
(typeof arr)[number]      // element of `as const` array
```

## Related Topics
- [keyof-and-typeof.md](03-keyof-and-typeof.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Function and class utilities](../07-utility-types/03-function-and-class-utilities.md)
