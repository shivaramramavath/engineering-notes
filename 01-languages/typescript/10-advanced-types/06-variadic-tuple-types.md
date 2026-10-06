# Variadic Tuple Types

A tuple is a fixed-length array with a type per position. **Variadic tuple types** (TS 4.0) let you use a *generic* spread inside a tuple, `[...T]`, so tuples can be concatenated, split, and pattern-matched with types. They are what makes typed `concat`, `curry`, `bind`, `Promise.all`, and "pass through the same arguments" wrappers possible.

**Prerequisites:**
- [Arrays and tuples](../01-fundamentals/02-arrays-and-tuples.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [infer](./02-infer.md)

---

## Tuple refresher

```ts
type Pair = [string, number];
type Labeled = [name: string, age: number];       // labels are documentation only
type WithOptional = [string, number?];            // optional element
type WithRest = [string, ...number[]];            // string followed by any number of numbers
type RO = readonly [string, number];              // readonly tuple
```

Useful facts: a tuple's `length` is a literal type (`[string, number]["length"]` is `2`), and `T[number]` gives the union of its element types.

## Spreading tuples into tuples

Before 4.0 you could only spread an array type at the end. Now you can spread a generic tuple type anywhere, and spreading **keeps the structure**:

```ts
type Concat<A extends unknown[], B extends unknown[]> = [...A, ...B];

type R = Concat<[1, 2], [3, 4]>;   // [1, 2, 3, 4]
```

```ts
type Push<T extends unknown[], V>    = [...T, V];
type Unshift<T extends unknown[], V> = [V, ...T];

type A = Push<[1, 2], 3>;      // [1, 2, 3]
type B = Unshift<[2, 3], 1>;   // [1, 2, 3]
```

The constraint `extends unknown[]` is required: only array and tuple types can be spread. For `readonly` tuples, constrain to `readonly unknown[]`.

## Matching tuples with `infer`

Spread works in patterns too, so you can take tuples apart:

```ts
type Head<T extends unknown[]> = T extends [infer H, ...unknown[]] ? H : never;
type Tail<T extends unknown[]> = T extends [unknown, ...infer R] ? R : [];
type Last<T extends unknown[]> = T extends [...unknown[], infer L] ? L : never;
type Init<T extends unknown[]> = T extends [...infer I, unknown] ? I : [];

type H = Head<[1, 2, 3]>;   // 1
type T = Tail<[1, 2, 3]>;   // [2, 3]
type L = Last<[1, 2, 3]>;   // 3
type I = Init<[1, 2, 3]>;   // [1, 2]
```

A rest element can appear **in the middle or at the start** of a tuple type since TS 4.2, which is what makes `Last` and `Init` possible. A tuple type can still have only one rest element.

## Capturing function parameters

A generic rest parameter infers the **whole argument list as a tuple**:

```ts
function call<A extends unknown[], R>(fn: (...args: A) => R, ...args: A): R {
  return fn(...args);
}

function greet(name: string, times: number) {
  return name.repeat(times);
}

call(greet, "hi", 2);       // ok, returns string
call(greet, "hi", "two");   // error: string is not number
```

`A` becomes `[name: string, times: number]`, the arguments of the function you passed, and the following `...args` must match. This is the foundation of wrappers that forward arguments with full type safety (decorators, retry, memoize, logging). Compare with `Parameters<F>` in [function and class utilities](../07-utility-types/03-function-and-class-utilities.md).

## Practical usage

### Partial application

```ts
function partial<A extends unknown[], B extends unknown[], R>(
  fn: (...args: [...A, ...B]) => R,
  ...head: A
): (...tail: B) => R {
  return (...tail) => fn(...head, ...tail);
}

const add3 = (a: number, b: number, c: number) => a + b + c;

const add2 = partial(add3, 1);   // (b: number, c: number) => number
add2(2, 3);                      // 6
```

`A` is fixed by the arguments you supply, and `B` is whatever remains of the function's parameters. The returned function expects exactly those.

### Prepend or append a parameter

```ts
type WithContext<F extends (...args: any[]) => any> =
  F extends (...args: infer P) => infer R ? (ctx: Context, ...args: P) => R : never;

type Handler = (id: string) => void;
type CtxHandler = WithContext<Handler>;   // (ctx: Context, id: string) => void
```

### Typed `concat` for tuples

```ts
function concat<A extends unknown[], B extends unknown[]>(a: [...A], b: [...B]): [...A, ...B] {
  return [...a, ...b];
}

const r = concat([1, "a"] as [number, string], [true] as [boolean]);
// [number, string, boolean]
```

The `[...A]` parameter form hints that A should be inferred as a tuple, not a plain array.

### Literal tuples from values

`as const` produces readonly tuples with literal element types, which then feed type-level code:

```ts
const steps = ["start", "run", "stop"] as const;   // readonly ["start", "run", "stop"]
type Step = (typeof steps)[number];                // "start" | "run" | "stop"
```

Constrain generics with `readonly unknown[]` to accept these.

## Important rules and misconceptions

**A spread of a plain array makes the tuple open-ended.** `[...number[], string]` means "any number of numbers, then a string". The length is no longer a literal.

**Only one rest element per tuple type**, but generic spreads (`[...A, ...B]`) are allowed because the compiler resolves them when `A` and `B` become concrete.

**Variadic types are type-level only.** At runtime a tuple is just an array.

**Tuples are inferred as arrays by default.** `const x = [1, "a"]` is `(string | number)[]`. Use `as const`, an explicit annotation, or the `[...T]` hint in a generic parameter to get a tuple.

**Labels carry no semantics.** They improve editor hints and are preserved through `Parameters<F>`, but two tuples differing only in labels are the same type.

## Common mistakes

- **Constraining to `any[]` and losing tuple structure.** Use `unknown[]` (or `readonly unknown[]`) and let inference produce a tuple.
- **Forgetting `readonly`.** An `as const` tuple is not assignable to `unknown[]`. Use `readonly unknown[]` in the constraint, or the helper will reject it.
- **Expecting inference of a tuple from an array literal.** Without a hint, `[1, "a"]` is an array.
- **Spreading a union of tuples and expecting per-member results,** which depends on distribution ([distributive conditional types](./01-distributive-conditional-types.md)).
- **Building long tuples recursively.** Very long ones hit recursion limits ([recursive types](./05-recursive-types.md)).

## Debugging

- Hover the generic parameter at the call site to see the inferred tuple. If it is an array, add the `[...T]` hint or `as const`.
- Test helpers with `[]`, a one-element tuple, and a longer one.
- If `Head`/`Tail` return `never`, check that you passed a tuple and not `number[]`: for `number[]`, `Head` is `number`, but the length is unknown.
- When a wrapper's argument errors look unreadable, expose the captured tuple with a named alias and hover it.

## Quick summary

- `[...T]` in a tuple type spreads a generic tuple, enabling `Concat`, `Push`, `Unshift`, and similar helpers. `T` must extend `unknown[]` (or `readonly unknown[]`).
- With `infer`, spreads let you split tuples into head, tail, last, and init (rest elements can sit anywhere since TS 4.2).
- A generic rest parameter `...args: A` captures a function's whole parameter list as a tuple. This powers typed wrappers, `bind`, and partial application.
- Tuples are arrays at runtime, and arrays infer as arrays unless you hint otherwise.

**Next:** [Branded types](./07-branded-types.md)
