# Interface vs Type

## Definition
Both `interface` and `type` can name an object shape. They differ in what else they can express, how they compose, and a few edge-case behaviours.

## Why It Matters
This is one of the most common TypeScript questions. Knowing the real differences lets you pick one consistently and explain why, instead of following folklore.

## Prerequisites
[interfaces.md](00-interfaces.md), [type aliases](../01-fundamentals/04-type-aliases.md)

## Same Shape, Two Syntaxes

```ts
interface UserI {
  id: number;
  name: string;
}

type UserT = {
  id: number;
  name: string;
};
```
Both are used and assigned identically; structurally they are the same type.

## Differences

| Feature | `interface` | `type` |
|---|---|---|
| Object shapes | Yes | Yes |
| Unions (`A \| B`) | No | **Yes** |
| Tuples | No | **Yes** |
| Primitives / aliases (`type ID = string`) | No | **Yes** |
| Function types | Via call signature | **Yes**, concise form |
| Mapped / conditional / template-literal types | No | **Yes** |
| Extending | `extends` | `&` (intersection) |
| Declaration merging | **Yes** | No (duplicate = error) |
| `implements` by a class | Yes | Yes (if object-like) |
| Conflicting members when extending | Error at declaration | Silently becomes `never` |
| Implicit index signature | No | Yes |

### Extending
```ts
interface Animal { name: string }
interface Dog extends Animal { breed: string }

type AnimalT = { name: string };
type DogT = AnimalT & { breed: string };
```

### Declaration merging
```ts
interface Config { host: string }
interface Config { port: number }     // merged: { host; port }

type C = { host: string };
type C = { port: number };            // Error: Duplicate identifier
```
Merging is the reason libraries expose **interfaces** for things users augment (`Window`, Express `Request`).

### Only `type` can do these
```ts
type Status = "idle" | "loading";
type Pair = [string, number];
type Getters<T> = { [K in keyof T]: () => T[K] };
type IsString<T> = T extends string ? true : false;
```

### Implicit index signatures
A type alias of an object literal type is assignable to `Record<string, unknown>`; an interface is not:

```ts
interface I { a: string }
type T = { a: string };

const i: Record<string, unknown> = {} as I;   // Error
const t: Record<string, unknown> = {} as T;   // ok
```
This matters when passing objects to functions that expect index-signature types.

### Error messages and performance
- Interfaces are named and cached by the compiler, so error messages tend to show `User` rather than an expanded structure, and checking `extends` chains is usually cheaper than long chains of intersections.
- Intersections that conflict can collapse to `never` with confusing errors.
For most code the difference is not noticeable; it matters in very large type graphs. See [type checking performance](../22-performance/00-type-checking-performance.md).

## Decision Guide

Use **`interface`** when:
- Describing an object or class contract, especially in a public API.
- You want clear `extends` errors and possible augmentation by consumers.
- Modelling domain entities that other types build on.

Use **`type`** when:
- You need unions, tuples, primitives, mapped, conditional or template literal types.
- You are composing existing types (`Pick`, `Omit`, intersections).
- You want a closed definition that nobody can merge into.

**Consistency beats the choice.** A common convention: `interface` for object shapes and class contracts, `type` for everything else. Another valid convention: `type` everywhere except where merging is required. Pick one, put it in the style guide, and enforce it with `@typescript-eslint/consistent-type-definitions`.

## Common Mistakes
- Claiming "interfaces are faster" or "types are more powerful" without knowing the specifics.
- Mixing both styles for the same kind of thing with no rule.
- Unintended merging by reusing an interface name.
- Using `type` for a class contract, then being surprised by intersection conflicts.

## Best Practices
- Write the convention down in [code style and conventions](../24-best-practices/03-code-style-and-conventions.md).
- Do not convert existing code just for style.
- For library authors: use interfaces for extensible public shapes and document augmentation points.

## Interview Questions
- Difference between `interface` and `type`?
- Which can be merged? Which can express unions?
- What happens if two intersected types have conflicting property types?
- When would you prefer an interface?

## Quick Reference
```ts
interface A extends B {}     // extension, mergeable
type A = B & {};             // intersection, not mergeable
type U = A | B;              // only type
type T = [A, B];             // only type
```

## Related Topics
- [interfaces.md](00-interfaces.md)
- [Intersection types](../03-unions-and-narrowing/01-intersection-types.md)
- [Declaration merging](../09-declaration-files/03-declaration-merging.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md)
