# Recursive Types

A recursive type refers to itself in its own definition. That is how you describe data that nests to arbitrary depth (JSON, trees, menus, ASTs) and how you write type-level "loops" that walk tuples, strings, and object paths. Recursion is the one feature that makes the type system Turing-complete in practice, and also the one that most often triggers "Type instantiation is excessively deep".

**Prerequisites:**
- [Conditional types](./00-conditional-types.md)
- [infer](./02-infer.md)
- [Mapped types](./03-mapped-types.md)

---

## Recursive data shapes

The simplest case: a type alias that mentions itself inside an object, array, or union.

```ts
type Json =
  | string
  | number
  | boolean
  | null
  | Json[]
  | { [key: string]: Json };

const config: Json = { name: "app", tags: ["a", "b"], nested: { debug: false } };
```

```ts
interface TreeNode<T> {
  value: T;
  children: TreeNode<T>[];
}

type MenuItem = {
  label: string;
  items?: MenuItem[];
};
```

Both `interface` and `type` can be recursive. The reference must appear in a position the compiler can defer: inside an object property, an array, a tuple, or a generic type argument. A bare self-reference like `type A = A | string` is an error ("circularly references itself").

## Recursive conditional types

A conditional type can call itself in a branch, which gives you a loop:

```ts
type Flatten<T> = T extends (infer U)[] ? Flatten<U> : T;

type A = Flatten<number[][][]>;   // number
```

Each step peels one array layer off until the type is no longer an array.

### Walking a tuple

`infer` with spread lets you take a tuple apart one element at a time:

```ts
type Reverse<T extends unknown[]> =
  T extends [infer H, ...infer R] ? [...Reverse<R>, H] : [];

type R = Reverse<[1, 2, 3]>;   // [3, 2, 1]
```

### Deep object transformations

```ts
type DeepReadonly<T> =
  T extends (...args: any[]) => any ? T :
  T extends object ? { readonly [K in keyof T]: DeepReadonly<T[K]> } :
  T;
```

Functions are returned unchanged, because mapping over a function type would turn it into an object. Built-ins like `Date`, `Map`, and `Set` need explicit handling too. See [building custom utility types](../07-utility-types/04-building-custom-utility-types.md).

## Practical patterns

### All property paths of an object

```ts
type Paths<T> = T extends object
  ? { [K in keyof T & string]: T[K] extends object ? K | `${K}.${Paths<T[K]>}` : K }[keyof T & string]
  : never;

interface Config {
  server: { host: string; port: number };
  debug: boolean;
}

type P = Paths<Config>;
// "server" | "server.host" | "server.port" | "debug"
```

How it works: the mapped type builds one entry per key. If the value is an object, the entry is the key itself plus `key.` joined to each path inside it (the recursion). Indexing the mapped type with `[keyof T & string]` turns the object of entries into a union. See [template literal types](./04-template-literal-types.md).

### The value at a path

```ts
type PathValue<T, P extends string> =
  P extends `${infer K}.${infer Rest}`
    ? K extends keyof T ? PathValue<T[K], Rest> : never
    : P extends keyof T ? T[P] : never;

type V = PathValue<Config, "server.port">;   // number
```

Combine `Paths` and `PathValue` to type a `get(obj, "server.port")` helper that autocompletes valid paths and returns the right type.

Arrays are objects, so `Paths` descends into them and produces keys like `"items.length"` and method names. If your data has arrays, add a branch for them (for example, stop at arrays or use `number` keys deliberately).

## Recursion limits

The compiler protects itself from infinite type expansion:

- A **non-tail** recursive type has a fairly low depth limit (around 50 levels of nested instantiation). Exceeding it gives:
  `Type instantiation is excessively deep and possibly infinite` (TS2589).
- Since TS 4.5, **tail-recursive conditional types** are optimized and can go much deeper (the limit is on the order of a thousand iterations).

A conditional type is tail-recursive when the recursive call is the **entire** result of a branch, not nested inside another type. The usual trick is an **accumulator parameter**:

```ts
// not tail-recursive: the recursive call is wrapped in a tuple
type ReverseSlow<T extends unknown[]> =
  T extends [infer H, ...infer R] ? [...ReverseSlow<R>, H] : [];

// tail-recursive: the call is the whole branch result
type Reverse<T extends unknown[], Acc extends unknown[] = []> =
  T extends [infer H, ...infer R] ? Reverse<R, [H, ...Acc]> : Acc;
```

Same result, but the second form can handle much longer inputs. The accumulator carries the work-in-progress, and the final branch returns it.

Practical limits to keep in mind:

- Counting and arithmetic that build tuples element by element stop working somewhere around a thousand elements.
- Deep recursion makes type checking slow even when it succeeds. See [type-checking performance](../22-performance/00-type-checking-performance.md).

## Important rules and misconceptions

**Recursion in type aliases is evaluated lazily.** A recursive object or array type is fine. A type that must be fully resolved to be defined is not.

**Both branches matter.** Every recursive conditional needs a base case that stops it. If the recursion never reaches the base case for some input (for example `number` instead of a literal), you get the depth error.

**A generic like `T extends string` that is not narrowed to a literal can loop or return something vague.** Recursive types usually expect literals or tuples as input.

**Recursion is type-level only.** There is no runtime cost, and no runtime behavior. JSON typed as `Json` is not validated.

## Common mistakes

- **Writing the recursive call wrapped in another type** (tuple, template literal, union) and then hitting the depth limit on moderate inputs. Use an accumulator when possible.
- **Missing base case.** The type recurses forever or returns `never` unexpectedly.
- **Applying a deep mapped type to built-ins** (`Date`, `Map`, functions) and breaking them.
- **Recursing on `number` or `string` instead of literals,** which cannot terminate.
- **Over-using recursion for things a plain type could express.** Keep these utilities small, tested, and hidden behind simple names.

## Debugging

- Start with a tiny input (one or two levels) and hover the result, then grow it.
- If you see TS2589, restructure for tail recursion, reduce the input size, or add an early exit for the cases you do not need.
- Break the type into a helper and inspect intermediate results.
- Add type tests so changes are caught ([type testing](../18-testing-and-debugging/04-type-testing.md)).
- If the editor slows down while a recursive type is in view, simplify. Slow types hurt everyone who opens the file.

## Quick summary

- A type can refer to itself inside an object, array, tuple, or generic argument. That is how JSON, trees, and menus are typed.
- Conditional types can recurse to loop over tuples, strings, and object paths. Always have a base case.
- Non-tail recursion has a low depth limit (TS2589). Tail-recursive conditionals with an accumulator can go far deeper.
- Common patterns: `Flatten`, `Reverse`, `DeepReadonly`, `Paths`, `PathValue`.
- Recursive types are compile-time only and can be slow. Keep them small and tested.

**Next:** [Variadic tuple types](./06-variadic-tuple-types.md)
