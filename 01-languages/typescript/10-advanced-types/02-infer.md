# infer

`infer` lets a conditional type **capture** a piece of the type it is matching and give it a name. It is pattern matching for types: "if `T` looks like this shape, tell me what is in this position." Every "extract X from Y" utility (`ReturnType`, `Parameters`, `Awaited`, element-of-array, first-of-tuple) is built from it.

**Prerequisites:**
- [Conditional types](./00-conditional-types.md)
- [Distributive conditional types](./01-distributive-conditional-types.md)
- [Generic functions](../06-generics/00-generic-functions.md)

---

## Syntax

`infer` can only appear in the `extends` clause of a conditional type. The name it introduces is available in the **true** branch.

```ts
type ElementOf<T> = T extends (infer U)[] ? U : never;

type A = ElementOf<string[]>;    // string
type B = ElementOf<number[]>;    // number
type C = ElementOf<boolean>;     // never (does not match the pattern)
```

Read it as: "if `T` is an array of *something*, call that something `U` and return it."

## How it works

```text
T extends (infer U)[] ? U : never

  T = string[]
        |
        v   match against the pattern  ( ? )[]
        |
     U = string   -->   true branch returns string
```

The compiler tries to make `T` fit the pattern. Wherever `infer U` sits, it records what `T` has in that position. If the pattern cannot match, the false branch is used.

## The standard library, rebuilt

```ts
type ReturnType<T extends (...args: any) => any> =
  T extends (...args: any) => infer R ? R : any;

type Parameters<T extends (...args: any) => any> =
  T extends (...args: infer P) => any ? P : never;

type InstanceType<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: any) => infer R ? R : any;
```

These are what [function and class utilities](../07-utility-types/03-function-and-class-utilities.md) use. Nothing special: just `infer` in the return, parameter, and instance positions.

## Practical patterns

### Unwrap a Promise, recursively

```ts
type Unwrap<T> = T extends Promise<infer U> ? Unwrap<U> : T;

type A = Unwrap<Promise<Promise<string>>>;  // string
type B = Unwrap<number>;                    // number
```

The built-in `Awaited<T>` is the full version and also handles thenables.

### Head and tail of a tuple

```ts
type Head<T extends unknown[]> = T extends [infer H, ...unknown[]] ? H : never;
type Tail<T extends unknown[]> = T extends [unknown, ...infer R] ? R : [];
type Last<T extends unknown[]> = T extends [...unknown[], infer L] ? L : never;

type H = Head<[1, 2, 3]>;  // 1
type R = Tail<[1, 2, 3]>;  // [2, 3]
type L = Last<[1, 2, 3]>;  // 3
```

These are the building blocks of tuple recursion ([variadic tuple types](./06-variadic-tuple-types.md)).

### Pull a type out of an object property

```ts
type PropType<T, K extends string> =
  T extends { [P in K]: infer V } ? V : never;

type N = PropType<{ id: number; name: string }, "name">;  // string
```

(For a known key, plain `T[K]` is simpler. Use `infer` when the shape being matched is more complex than a single property.)

### Parse strings

`infer` works inside template literal types:

```ts
type Split<S extends string, D extends string> =
  S extends `${infer Head}${D}${infer Rest}`
    ? [Head, ...Split<Rest, D>]
    : [S];

type S = Split<"a.b.c", ".">;  // ["a", "b", "c"]
```

More in [template literal types](./04-template-literal-types.md).

### Narrow what gets inferred: `infer X extends Y`

TS 4.7 lets you constrain an inferred type, and TS 4.8 improved its handling of primitives like `number`:

```ts
type ParseInt<S> = S extends `${infer N extends number}` ? N : never;

type A = ParseInt<"42">;     // 42  (a number literal type, not the string "42")
type B = ParseInt<"hello">;  // never
```

Without the `extends number` constraint, `N` would just be the string `"42"`.

## Multiple candidates for one `infer`

If the same name is inferred in several positions, the result depends on the **variance** of those positions.

**Covariant positions (property types, return types)** combine into a **union**:

```ts
type Collect<T> = T extends { a: infer U; b: infer U } ? U : never;

type R = Collect<{ a: string; b: number }>;  // string | number
```

**Contravariant positions (function parameters)** combine into an **intersection**:

```ts
type Collect2<T> =
  T extends { a: (x: infer U) => void; b: (x: infer U) => void } ? U : never;

type R2 = Collect2<{ a: (x: string) => void; b: (x: number) => void }>;
// string & number  ->  never
```

This is surprising until you think about what a function accepting both a `string` and a `number` argument must accept: a value that is both. It is the basis of a well-known trick, turning a union into an intersection:

```ts
type UnionToIntersection<U> =
  (U extends unknown ? (x: U) => void : never) extends (x: infer I) => void
    ? I
    : never;

type R = UnionToIntersection<{ a: 1 } | { b: 2 }>;  // { a: 1 } & { b: 2 }
```

See [variance](../14-type-system-internals/02-variance.md) for the reasoning.

## Important rules and misconceptions

**`infer` only works inside a conditional's `extends` clause.** You cannot use it in an ordinary type alias or a function signature.

**The inferred name is only in scope in the true branch.**

**Overloaded functions:** `infer` against a function type uses the **last** overload signature.

**Generic functions:** type parameters of a generic function are replaced by their constraints (or `unknown`) when matched, so `ReturnType<typeof identity>` for `<T>(x: T) => T` is `unknown`.

**String inference is shortest-match for each non-final `infer`.** In ``S extends `${infer A}.${infer B}` `` with `"a.b.c"`, `A` is `"a"` and `B` is `"b.c"`. The last `infer` gets the remainder.

**`infer` is not a runtime feature.** It describes types only and vanishes at compile time.

## Common mistakes

- **Using `infer` outside a conditional.** It is a syntax error.
- **Forgetting the false branch.** A non-matching input silently becomes `never` (or whatever you wrote). Decide what no-match should mean.
- **Expecting a single type from several positions.** You get a union (covariant) or an intersection (contravariant), which may be `never`.
- **Matching too loosely.** `T extends (infer U)[]` also matches arrays of `any`. Constrain the input type if it matters.
- **Forgetting that tuples and arrays differ.** `[infer H, ...infer R]` matches tuples, not `string[]` with certainty. For `string[]`, `H` becomes `string` and `R` becomes `string[]`.

## Debugging

- Hover the alias with several concrete types. If you get `never`, the pattern did not match.
- Reduce the pattern to just the position you care about, confirm it works, then add more structure.
- Check whether a union was *distributed*: a bare `T` distributes over unions before the pattern is tested ([distributive conditional types](./01-distributive-conditional-types.md)).
- For string patterns, test with strings that have zero, one, and several delimiters.

## Quick summary

- `infer U` inside an `extends` clause captures whatever is in that position into `U`, usable in the true branch.
- It powers `ReturnType`, `Parameters`, `Awaited`, tuple head/tail, and string parsing.
- `infer U extends X` constrains what is captured (useful for turning `"42"` into `42`).
- Multiple inferences of one name give a union in covariant positions and an intersection in contravariant ones.
- Overloads use the last signature, and generics collapse to their constraints.

**Next:** [Mapped types](./03-mapped-types.md)
