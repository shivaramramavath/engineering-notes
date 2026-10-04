# Primitive Types

## Definition
Primitives are the basic, immutable value types of JavaScript: `string`, `number`, `boolean`, `bigint`, `symbol`, plus `null` and `undefined` (covered in [null-and-undefined.md](08-null-and-undefined.md)).

## Why It Matters
Every other type is built from these. Mixing them up (string vs. number) is the most common source of basic bugs TypeScript catches.

## Prerequisites
[types-and-inference.md](00-types-and-inference.md)

## Syntax

```ts
let title: string = "TypeScript";
let price: number = 19.99;
let active: boolean = true;
let big: bigint = 9007199254740993n;
let key: symbol = Symbol("id");
```

## Basic Example

```ts
function total(price: number, qty: number): number {
  return price * qty;
}

total(10, "2"); // Error: Argument of type 'string' is not assignable to parameter of type 'number'
```

## Important Concepts

### `number` covers all numeric values
There is no separate `int` or `float`. `NaN` and `Infinity` are also `number`.

```ts
const a: number = 42;
const b: number = 3.14;
const c: number = 0xff;
const d: number = NaN;
```

### Lowercase primitives, not wrapper objects
Use `string`, `number`, `boolean`. Never use `String`, `Number`, `Boolean`: those are wrapper object types and almost never what you want.

```ts
let ok: string = "a";
let bad: String = "a"; // legal but wrong in practice
```

### `bigint` and `number` do not mix
```ts
const x = 10n + 5;  // Error: Operator '+' cannot be applied to types 'bigint' and 'number'
```
`bigint` needs a target of ES2020 or later.

### `symbol` and `unique symbol`
```ts
const ID: unique symbol = Symbol("id");   // a unique type, usable as a property key
```

### Template literal strings are `string`
```ts
const msg = `Hello ${"x"}`; // string
```

### Literal types (preview)
```ts
const mode = "dark";   // type is "dark", not string
```
See [literal types](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md).

## Common Mistakes
- Using `String`/`Number`/`Boolean`.
- Assuming `number` includes only integers.
- Comparing `number` and `string` values loosely (`"1" == 1`); TypeScript flags many of these, but use `===`.
- Forgetting form inputs and `process.env` values are always `string`, so convert explicitly:

```ts
const port = Number(process.env.PORT ?? "3000");
```

## Best Practices
- Let inference give literal types for `const` values.
- Convert at the boundary (parse input strings to numbers/dates once).
- Prefer `number` for most numeric work; use `bigint` only for values beyond `Number.MAX_SAFE_INTEGER`.

## Interview Questions
- Is there a difference between `string` and `String`?
- Why is `NaN` a `number`?
- What does `const x = 5` infer vs. `let x = 5`?

## Quick Reference

| Type | Example |
|---|---|
| `string` | `"a"`, `` `a${b}` `` |
| `number` | `1`, `1.5`, `NaN` |
| `boolean` | `true`, `false` |
| `bigint` | `10n` |
| `symbol` | `Symbol("x")` |

## Related Topics
- [types-and-inference.md](00-types-and-inference.md)
- [null-and-undefined.md](08-null-and-undefined.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)
