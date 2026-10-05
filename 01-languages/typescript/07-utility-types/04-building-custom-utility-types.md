# Building Custom Utility Types

The built-in utilities cover common cases. For the rest (deep variants, "make these keys optional", pick-by-value-type) you compose **mapped types**, **conditional types**, and **key remapping** into your own. This note collects the patterns worth keeping in a project's `types/` folder and the rules for writing them well.

**Prerequisites:**
- [Mapped types](../10-advanced-types/03-mapped-types.md)
- [Conditional types](../10-advanced-types/00-conditional-types.md) and [distributive conditionals](../10-advanced-types/01-distributive-conditional-types.md)
- [`infer`](../10-advanced-types/02-infer.md)
- [Partial, Required, Readonly](./00-partial-required-readonly.md) and [Pick, Omit, Record](./01-pick-omit-record.md)

---

## The building blocks

| Tool | What it gives you |
|---|---|
| `[K in keyof T]` | iterate over keys, preserving modifiers (homomorphic) |
| `?`, `-?`, `readonly`, `-readonly` | add or remove modifiers |
| `as` clause (TS 4.1+) | rename or filter keys while mapping |
| `T extends U ? X : Y` | branch on type relationships |
| `infer` | capture part of a matched type |
| `T[K]`, `T[number]` | indexed access |

Almost every custom utility is one of these combined with one other.

## Modifier utilities

```ts
type Mutable<T> = { -readonly [K in keyof T]: T[K] };
```

Removes `readonly` from every property. Useful in tests and builders, where you need to construct something the public type forbids you to mutate.

### Make only some keys optional or required

`Partial<T>` is all-or-nothing. This version targets specific keys:

```ts
type PartialBy<T, K extends keyof T> = Omit<T, K> & Partial<Pick<T, K>>;
type RequiredBy<T, K extends keyof T> = Omit<T, K> & Required<Pick<T, K>>;

interface Post { id: number; title: string; body: string; draft?: boolean }

type NewPost = PartialBy<Post, "id">;
// id optional; title, body required; draft stays optional
```

Intersections display poorly in tooltips and can slow checking if overused. Flatten them with `Expand`:

```ts
type Expand<T> = { [K in keyof T]: T[K] } & {};

type PartialBy2<T, K extends keyof T> = Expand<Omit<T, K> & Partial<Pick<T, K>>>;
```

`Expand` (often called `Prettify` or `Simplify`) forces the compiler to print the resolved object instead of `A & B`.

## Selecting keys by value type

Use **key remapping** with `as` to filter properties during the mapping. Mapping a key to `never` drops it:

```ts
type PickByType<T, V> = {
  [K in keyof T as T[K] extends V ? K : never]: T[K];
};

interface Product { id: number; name: string; price: number; sku: string }

type StringFields = PickByType<Product, string>; // { name: string; sku: string }
```

If you only need the **keys**, not the object:

```ts
type KeysOfType<T, V> = {
  [K in keyof T]-?: T[K] extends V ? K : never;
}[keyof T];

type NumericKeys = KeysOfType<Product, number>; // "id" | "price"
```

The trailing `[keyof T]` indexes the mapped object to turn its values into a union. `-?` is there so optional properties do not leak `undefined` into the union.

## Renaming keys

`as` plus template literal types and intrinsic string utilities gives a typed getter/setter generator:

```ts
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K];
};

type ProductGetters = Getters<Pick<Product, "name" | "price">>;
// { getName: () => string; getPrice: () => number }
```

`string & K` narrows keys to strings, since `keyof T` may also include `number` and `symbol`. See [template literal types](../10-advanced-types/04-template-literal-types.md).

## Value and union helpers

```ts
type ValueOf<T> = T[keyof T];

const Status = { Idle: "idle", Busy: "busy" } as const;
type Status = ValueOf<typeof Status>; // "idle" | "busy"

type ElementOf<T extends readonly unknown[]> = T[number];
type Nullable<T> = T | null;
type Maybe<T> = T | null | undefined;
```

`ValueOf` plus `as const` objects is the usual replacement for `enum` (see [enums and const objects](../01-fundamentals/05-enums-and-const-objects.md)).

## Recursive (deep) utilities

```ts
type DeepReadonly<T> =
  T extends (...args: any[]) => any ? T :
  T extends object ? { readonly [K in keyof T]: DeepReadonly<T[K]> } :
  T;

type DeepPartial<T> =
  T extends (...args: any[]) => any ? T :
  T extends object ? { [K in keyof T]?: DeepPartial<T[K]> } :
  T;
```

Behavior to be aware of:

- **Functions** are returned as-is. Mapping over a function type would turn it into an object type and lose its call signature.
- **Arrays and tuples:** homomorphic mapped types over an array type produce an array type, so `DeepReadonly<string[]>` becomes `readonly string[]`. But `DeepPartial<string[]>` becomes `(string | undefined)[]`, which is rarely what you want.
- **Built-in objects** like `Date`, `Map`, `Set`, and `Promise` are `object`, so they get mapped structurally and stop behaving like the real class. Exclude them explicitly if your data has them:

```ts
type Builtin = Date | RegExp | Map<any, any> | Set<any> | Promise<any>;

type DeepReadonlySafe<T> =
  T extends (...args: any[]) => any ? T :
  T extends Builtin ? T :
  T extends object ? { readonly [K in keyof T]: DeepReadonlySafe<T[K]> } :
  T;
```

Recursive aliases are evaluated lazily, but very deep types can hit instantiation limits. See [recursive types](../10-advanced-types/05-recursive-types.md) and [type-checking performance](../22-performance/00-type-checking-performance.md).

## Fixing built-ins

Two built-ins have well-known weaknesses. Both are worth redefining in a shared types file:

```ts
// Omit that rejects keys that are not on T
type StrictOmit<T, K extends keyof T> = Omit<T, K>;

// Omit that keeps discriminated unions intact
type DistributiveOmit<T, K extends PropertyKey> =
  T extends unknown ? Omit<T, K> : never;
```

Details are in [Pick, Omit, Record](./01-pick-omit-record.md).

## Design rules

- **Constrain the generics.** `K extends keyof T` turns typos into errors at the call site, not into silently wrong types.
- **Keep mapped types homomorphic** (`[K in keyof T]`) when you want modifiers preserved. Mapping over a bare union of keys (`[K in "a" | "b"]`) drops them.
- **Distribute deliberately.** A naked `T extends ...` distributes over unions. Wrap it as `[T] extends [...]` to turn that off.
- **Name by behavior**, not mechanism: `PartialBy`, `PickByType`, not `MyMapped1`.
- **Prefer composing built-ins** (`Omit` + `Partial` + `Pick`) over a new mapped type when the result is clear. Write a new one when composition becomes hard to read.
- **Test them.** Type-level code regresses silently. See [type testing](../18-testing-and-debugging/04-type-testing.md).

A minimal type-test harness:

```ts
type Equal<X, Y> =
  (<T>() => T extends X ? 1 : 2) extends (<T>() => T extends Y ? 1 : 2) ? true : false;

type Expect<T extends true> = T;

type _1 = Expect<Equal<PickByType<Product, string>, { name: string; sku: string }>>;
type _2 = Expect<Equal<KeysOfType<Product, number>, "id" | "price">>;
```

If an assertion is wrong, the `Expect<...>` line fails to compile.

## Common mistakes

- **Using `Omit<T, K> & Partial<...>` everywhere and ending up with unreadable tooltips.** Wrap the result in `Expand`.
- **Forgetting `-?` in the `KeysOfType` pattern.** Optional keys then add `undefined` to the key union.
- **Writing `DeepPartial` without handling functions and built-ins.** Calls and `Date` methods break in surprising ways.
- **Mapping over `keyof T` without `string & K` when building string keys.** You get an error because `symbol` cannot go in a template literal.
- **Over-engineering.** If a type needs a paragraph of comments to explain, an explicit hand-written type may be the better choice.

## Debugging

- Break a utility into named steps and hover each intermediate type.
- Use `Expand` to inspect the final shape.
- Test with a small concrete type first, then with unions, optional properties, arrays, and `any`. Utilities usually fail on those edge shapes.
- If checking becomes slow, look for deep recursion or large unions inside the utility and simplify them.

## Quick summary

- Custom utilities are mapped types (`-?`, `-readonly`, `as`) plus conditional types (`extends`, `infer`).
- Key remapping with `as ... ? K : never` filters properties. Indexing a mapped type with `[keyof T]` turns it into a union of keys.
- Deep utilities must special-case functions and built-in classes.
- Constrain generics, keep mappings homomorphic, wrap intersections in `Expand`, and test with `Equal`/`Expect`.

**Next:** practice in [type challenges](../26-projects/exercises/00-type-challenges.md), or review in the [utility types cheatsheet](../28-cheatsheets/01-utility-types.md).
