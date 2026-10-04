# Intersection Types

## Definition
An **intersection type** `A & B` describes a value that has **all** the members of both `A` and `B`.

## Why It Matters
Intersections combine small, focused types into larger ones, which is the basis of composition for object shapes, mixins and many utility types.

## Prerequisites
[union-types.md](00-union-types.md), [objects](../01-fundamentals/03-objects.md)

## Syntax

```ts
type Named = { name: string };
type Aged = { age: number };

type Person = Named & Aged;   // { name: string; age: number }
```

## Basic Example

```ts
const p: Person = { name: "Ada", age: 36 };
const q: Person = { name: "Ada" };      // Error: Property 'age' is missing
```

## How It Works

### Union = "or", intersection = "and"
```ts
type U = { a: number } | { b: string };   // has a OR b
type I = { a: number } & { b: string };   // has a AND b
```

### Composing reusable pieces
```ts
type Timestamps = { createdAt: Date; updatedAt: Date };
type SoftDelete = { deletedAt: Date | null };

type Post = { id: string; title: string } & Timestamps & SoftDelete;
```

### Adding to a type without editing it
```ts
type WithId<T> = T & { id: string };
type UserDraft = { name: string };
type User = WithId<UserDraft>;
```

## Important Concepts

### Conflicting properties
If the same property has incompatible types, that property becomes `never`:

```ts
type A = { id: string };
type B = { id: number };
type C = A & B;
declare const c: C;
c.id;   // never (string & number)
```
Objects of type `C` are effectively impossible to create. Intersecting incompatible primitives gives `never` directly:

```ts
type X = string & number;   // never
```

### Compatible overlapping properties are fine
```ts
type A = { id: string };
type B = { id: string; name: string };
type C = A & B;   // { id: string; name: string }
```

### Intersections with unions distribute
```ts
type T = (A | B) & C;   // (A & C) | (B & C)
```

### Intersection of function types acts like overloads
```ts
type F = ((x: string) => string) & ((x: number) => number);
```
Calls resolve by trying each signature in order.

### Branding with intersections
```ts
type UserId = string & { readonly __brand: "UserId" };
```
This makes structurally identical types incompatible; see [branded types](../10-advanced-types/07-branded-types.md).

### Intersection vs. `interface extends`
```ts
interface Employee extends Person { company: string }   // reports conflicts at declaration
type Employee2 = Person & { company: string };          // conflicts silently become never
```
`extends` gives clearer errors for conflicting members and is slightly faster for the compiler. Use `&` when combining with unions, mapped types or aliases. See [interface vs type](../04-objects-and-interfaces/01-interface-vs-type.md).

### Intersecting optional and required
```ts
type A = { x?: number };
type B = { x: number };
type C = A & B;   // x is required: number
```

## Common Mistakes
- Intersecting object types with conflicting property types and getting `never`.
- Using `&` when you meant `|` (for example expecting `A & B` to accept either shape).
- Long chains of intersections that produce unreadable errors and hurt performance; consider an interface that `extends` instead.
- Assuming intersections deep-merge nested objects with conflicts.

## Best Practices
- Use intersections to combine small independent concerns (`WithId`, `Timestamps`).
- Keep conflicting property names out of the types you combine.
- Use `Omit` first if you need to override a property:

```ts
type Override<T, U> = Omit<T, keyof U> & U;
```

## Interview Questions
- What is the difference between `A | B` and `A & B`?
- What is the type of `string & number`?
- What happens when two intersected types have the same property with different types?
- When would you choose `interface extends` over `&`?

## Quick Reference
```ts
A & B                    // has everything from both
T & { id: string }       // add a property
(A | B) & C              // distributes: (A & C) | (B & C)
string & number          // never
```

## Related Topics
- [union-types.md](00-union-types.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Mixins](../05-classes/06-mixins.md)
- [Branded types](../10-advanced-types/07-branded-types.md)
