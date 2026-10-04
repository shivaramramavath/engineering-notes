# Union Types

## Definition
A **union type** `A | B` describes a value that is either `A` or `B`. It is TypeScript's way to model "one of several possibilities".

## Why It Matters
Real data varies: an API returns a result or an error, an input is a string or a number, a state is loading or loaded. Unions let the type system represent that variation and force you to handle every case.

## Prerequisites
[01-fundamentals](../01-fundamentals/README.md)

## Syntax

```ts
let id: string | number;
id = "abc";   // ok
id = 42;      // ok
id = true;    // Error

type Result = Success | Failure;
type MaybeUser = User | null | undefined;
```

A leading `|` is allowed for multi-line unions:

```ts
type Method =
  | "GET"
  | "POST"
  | "PUT"
  | "DELETE";
```

## Basic Example

```ts
function format(id: string | number): string {
  return id.toUpperCase();        // Error: 'toUpperCase' does not exist on type 'number'
}

function formatSafe(id: string | number): string {
  return typeof id === "string" ? id.toUpperCase() : id.toFixed(0);
}
```

## How It Works

### You can only use what all members share
```ts
type Bird = { fly(): void; layEggs(): void };
type Fish = { swim(): void; layEggs(): void };

function f(pet: Bird | Fish) {
  pet.layEggs();   // ok: common to both
  pet.fly();       // Error: 'fly' does not exist on type 'Fish'
}
```
To use member-specific features, [narrow](03-type-narrowing.md) first.

### Assigning to a union
A value is assignable to a union if it is assignable to **any** member. The reverse (using a union value as one member) requires narrowing.

### Unions of object types and excess properties
```ts
type A = { kind: "a"; x: number };
type B = { kind: "b"; y: string };
const v: A | B = { kind: "a", x: 1 };   // ok
```

### Unions with `null` / `undefined`
`T | null` and `T | undefined` are how absence is modelled under `strictNullChecks`; see [null and undefined](../01-fundamentals/08-null-and-undefined.md).

### Union of arrays vs. array of unions
```ts
let a: string[] | number[];     // all strings, or all numbers
let b: (string | number)[];     // each element may differ
```

### Distribution over members
Utility and conditional types often apply to each member of a union separately; see [distributive conditional types](../10-advanced-types/01-distributive-conditional-types.md).

### Subtype reduction
`"a" | string` collapses to `string`, and `never` vanishes from unions (`string | never` is `string`).

### Union order does not matter
`A | B` and `B | A` are the same type.

## Common Mistakes
- Calling a member-specific method without narrowing.
- Mixing up `string[] | number[]` with `(string | number)[]`.
- Adding `any` to a union (the whole union becomes `any`).
- Using a union where a [discriminated union](04-discriminated-unions.md) is needed, then writing fragile `in` checks.
- Piling optional fields into one object type instead of using a union of distinct shapes.

## Best Practices
- Prefer unions of distinct, named shapes over one type with many optional fields.
- Give each shape a literal tag so it can be [discriminated](04-discriminated-unions.md).
- Keep unions small and meaningful; very large unions slow type checking.

## Interview Questions
- What can you do with a value of type `string | number` without narrowing?
- Difference between `string[] | number[]` and `(string | number)[]`?
- Why does `string | never` equal `string`?

## Quick Reference
```ts
A | B               // union
A | null            // nullable
"a" | "b" | "c"     // literal union
(A | B)[]           // array of union
```

## Related Topics
- [intersection-types.md](01-intersection-types.md)
- [type-narrowing.md](03-type-narrowing.md)
- [discriminated-unions.md](04-discriminated-unions.md)
- [exclude-extract-nonnullable.md](../07-utility-types/02-exclude-extract-nonnullable.md)
