# Arrays and Tuples

## Definition
- **Array type**: a list of values of the same type, any length.
- **Tuple type**: a fixed-length array where each position has its own type.

## Why It Matters
Lists are everywhere. Tuples let you return several values of different types (like `useState`) with full type safety.

## Prerequisites
[primitive-types.md](01-primitive-types.md)

## Syntax

```ts
const names: string[] = ["Ada", "Linus"];
const ids: Array<number> = [1, 2, 3];         // same type, generic form

const pair: [string, number] = ["age", 30];   // tuple
```

`T[]` and `Array<T>` are identical. Prefer `T[]` for simple types and `Array<...>` when the element type is complex.

## Basic Example

```ts
const scores: number[] = [90, 85];
scores.push("100"); // Error: 'string' is not assignable to 'number'

const entry: [string, number] = ["Ada", 36];
entry[0].toUpperCase(); // ok: string
entry[1].toFixed(0);    // ok: number
entry[2];               // Error: Tuple of length 2 has no element at index 2
```

## Important Concepts

### Readonly arrays
```ts
const list: readonly number[] = [1, 2, 3];
list.push(4);   // Error: 'push' does not exist on type 'readonly number[]'
```
`ReadonlyArray<T>` is the same. Use `readonly` for parameters you do not intend to mutate.

```ts
function sum(values: readonly number[]): number {
  return values.reduce((a, b) => a + b, 0);
}
```

### Tuples: labels, optional and rest elements
```ts
type Range = [start: number, end: number];            // labelled (for docs/hover)
type Point = [x: number, y: number, z?: number];      // optional element
type Row = [id: number, ...tags: string[]];           // rest element
```

### Destructuring
```ts
const [name, age] = ["Ada", 36] as const;
```

### `as const` makes a readonly tuple
```ts
const a = [1, "x"];            // (string | number)[]
const b = [1, "x"] as const;   // readonly [1, "x"]
```

### Inference defaults to arrays, not tuples
```ts
function pair() {
  return ["a", 1];            // (string | number)[]
}
function pairTuple(): [string, number] {
  return ["a", 1];            // tuple
}
```

### Unsafe indexing
By default `arr[10]` is typed as `T` even if it is missing. Turn on `noUncheckedIndexedAccess` to make it `T | undefined`.

```ts
const xs: string[] = [];
const first = xs[0];          // string (default)  /  string | undefined (with noUncheckedIndexedAccess)
```

## Common Mistakes
- Using `Array` (no type argument) or `any[]`.
- Expecting `[1, "x"]` to infer a tuple.
- Mutating an array typed `readonly`, or passing a `readonly` array to a function that requires a mutable one.
- Using tuples for structures with many fields; use an object with names instead.

## Best Practices
- Use `readonly T[]` for inputs you do not modify.
- Use tuples only for small, positional results (2-3 items); otherwise return an object.
- Enable `noUncheckedIndexedAccess` for stricter safety.

## Interview Questions
- Difference between `T[]` and `Array<T>`?
- How do you make an array immutable at the type level?
- How would you type the return of `useState`?
- What does `as const` do to an array literal?

## Quick Reference
```ts
string[]                      // array
readonly string[]             // immutable array
[string, number]              // tuple
[x: number, y?: number]       // labelled + optional
[number, ...string[]]         // rest in tuple
```

## Related Topics
- [objects.md](03-objects.md)
- [Literal types and const assertions](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [Variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md)
