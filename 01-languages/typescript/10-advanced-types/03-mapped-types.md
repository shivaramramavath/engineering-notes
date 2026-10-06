# Mapped Types

A mapped type builds a new object type by **looping over a set of keys** and deciding the property type (and modifiers) for each. It is the `for...in` of the type system. `Partial`, `Required`, `Readonly`, `Pick`, and `Record` are all mapped types, and most custom utilities you write will be too.

**Prerequisites:**
- [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [Indexed access types](../06-generics/04-indexed-access-types.md)
- [Partial, Required, Readonly](../07-utility-types/00-partial-required-readonly.md)

---

## Syntax

```ts
type Flags<T> = {
  [K in keyof T]: boolean;
};

interface Features { darkMode: string; beta: number }

type F = Flags<Features>;
// { darkMode: boolean; beta: boolean }
```

Read `[K in keyof T]` as: for each key `K` in the keys of `T`, create a property named `K`. The right-hand side is the property type, and it can refer to `K` and to `T[K]`.

The part after `in` can be any union of `string | number | symbol` types, not just `keyof T`:

```ts
type Role = "admin" | "editor" | "viewer";
type Permissions = { [R in Role]: boolean };
// { admin: boolean; editor: boolean; viewer: boolean }
```

This is exactly how `Record<K, V>` works.

## Homomorphic mapped types

When the keys come directly from `keyof T`, the mapped type is **homomorphic**: it copies each property's existing modifiers (`readonly`, `?`) from `T` unless you change them.

```ts
interface User { readonly id: number; name?: string }

type Copy<T> = { [K in keyof T]: T[K] };
type U = Copy<User>;   // { readonly id: number; name?: string }  (modifiers preserved)
```

Mapping over a plain union of keys (like `Role` above) has no source type, so there are no modifiers to preserve.

## Modifiers: `+`, `-`, `readonly`, `?`

```ts
type Mutable<T>  = { -readonly [K in keyof T]: T[K] };   // remove readonly
type Required<T> = { [K in keyof T]-?: T[K] };           // remove optional
type Frozen<T>   = { +readonly [K in keyof T]: T[K] };   // add readonly (the + is optional)
```

`-?` also removes `undefined` from the property type when the property was optional.

## Key remapping with `as`

TS 4.1 added an `as` clause to rename or **filter** keys while mapping:

```ts
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K];
};

interface Person { name: string; age: number }

type PG = Getters<Person>;
// { getName: () => string; getAge: () => number }
```

Mapping a key to `never` **removes** it:

```ts
type OmitByType<T, V> = {
  [K in keyof T as T[K] extends V ? never : K]: T[K];
};

type NoStrings = OmitByType<{ a: string; b: number }, string>;  // { b: number }
```

`string & K` in the getter example narrows `K` to string keys. `keyof T` can also include `number` and `symbol`, which cannot go into a template literal.

Key remapping keeps a mapped type homomorphic as long as the `in` part is `keyof T`, so modifiers are still preserved.

### Re-keying a union

`as` also lets you turn a union of objects into an object keyed by a property of each member:

```ts
type AppEvent =
  | { type: "click"; x: number }
  | { type: "key"; code: string };

type Handlers = {
  [E in AppEvent as E["type"]]: (event: E) => void;
};
// { click: (event: { type: "click"; x: number }) => void;
//   key:   (event: { type: "key"; code: string }) => void }
```

This is a typical building block for typed event systems ([typed event emitter](../17-design-patterns/07-typed-event-emitter.md)).

## Behavior worth knowing

### Arrays and tuples

A homomorphic mapped type applied to an array or tuple type produces an array or tuple, not an object:

```ts
type Stringify<T> = { [K in keyof T]: string };

type A = Stringify<[number, boolean]>;  // [string, string]
type B = Stringify<number[]>;           // string[]
```

This works when `T` is a type parameter that is instantiated with the array. Modifiers apply to the elements: `Partial<string[]>` is `(string | undefined)[]`.

### Distribution over unions

When `T` is a type parameter, a homomorphic mapped type distributes over a union argument:

```ts
type Nullify<T> = { [K in keyof T]: T[K] | null };

type R = Nullify<{ a: 1 } | { b: 2 }>;
// { a: 1 | null } | { b: 2 | null }
```

### Index signatures

Mapping over `string` produces an index signature:

```ts
type Dict = { [K in string]: number };   // { [x: string]: number }
```

## Practical usage

Form state from a data shape:

```ts
type FormState<T> = {
  [K in keyof T]: { value: T[K]; error?: string; touched: boolean };
};

type LoginForm = FormState<{ email: string; remember: boolean }>;
```

Making every method async:

```ts
type Async<T> = {
  [K in keyof T]: T[K] extends (...args: infer A) => infer R
    ? (...args: A) => Promise<R>
    : T[K];
};
```

A "patch" type where each field is wrapped in an updater function, derived views of an entity, DTO variants, config schemas: all of these are one mapped type away. Combine with [conditional types](./00-conditional-types.md) and [`infer`](./02-infer.md) for anything that depends on a property's own type.

## Mapped types vs index signatures vs `Record`

| Tool | Use when |
|---|---|
| `Record<K, V>` | keys are a known union or `string`, all values share one type |
| Mapped type | the property type or modifiers depend on each key or on `T[K]` |
| Index signature | an object with arbitrary keys, no source type to derive from |

## Common mistakes

- **Using `keyof T` in a template literal without `string & K`.** `symbol` keys cause an error.
- **Expecting modifiers to be preserved when mapping over a plain union.** Only `keyof T` mapping is homomorphic.
- **Forgetting `-?` when using the mapped result to build a key union.** Optional properties add `undefined` to the indexed result.
- **Reaching for a mapped type when `Pick`/`Omit` suffices.** Compose built-ins first.
- **Assuming a mapped type creates a class or interface.** It makes an anonymous object type. It cannot be extended by `extends` in a class declaration unless the result is statically known, and it cannot merge like an interface.
- **Applying `DeepX` mapped types to `Date`, `Map`, or functions** and mangling them ([building custom utility types](../07-utility-types/04-building-custom-utility-types.md)).

## Debugging

- Hover the alias. If you see the mapped type itself and not the expanded shape, use an expander:

```ts
type Expand<T> = { [K in keyof T]: T[K] } & {};
```

- If a property you expect is missing, check whether the `as` clause mapped its key to `never`.
- If modifiers disappeared, check that the mapping is `[K in keyof T]` and not over a separate key union.
- If an array became an object, the mapped type was applied to a concrete array type rather than a type parameter. Move the mapping behind a generic.

## Quick summary

- `[K in Keys]: ValueType` creates one property per key. Right side can use `K` and `T[K]`.
- `[K in keyof T]` is homomorphic and preserves `readonly` and `?`. Use `+`/`-` to add or remove modifiers.
- The `as` clause renames keys (with template literal types) or filters them (map to `never`).
- Homomorphic mappings turn arrays into arrays, tuples into tuples, and distribute over unions.
- Most utility types are mapped types. Learn this one well.

**Next:** [Template literal types](./04-template-literal-types.md)
