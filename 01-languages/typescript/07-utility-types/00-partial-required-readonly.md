# Partial, Required, and Readonly

`Partial<T>`, `Required<T>`, and `Readonly<T>` are the three built-in utility types that change the **modifiers** (`?` and `readonly`) on every property of an object type, without changing the property types themselves. They let you derive variants of a type instead of maintaining parallel definitions that drift apart.

**Prerequisites:**
- [Readonly and optional properties](../04-objects-and-interfaces/02-readonly-and-optional-properties.md)
- [Generic types](../06-generics/01-generic-types.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md) (helpful, but the definitions below are readable without it)

---

## The three at a glance

```ts
interface User {
  id: number;
  name: string;
  email?: string;
}

type A = Partial<User>;   // { id?: number; name?: string; email?: string }
type B = Required<User>;  // { id: number; name: string; email: string }
type C = Readonly<User>;  // { readonly id: number; readonly name: string; readonly email?: string }
```

| Utility | Effect on every property |
|---|---|
| `Partial<T>` | adds `?` |
| `Required<T>` | removes `?` |
| `Readonly<T>` | adds `readonly` |

## How they work

They are plain mapped types over `keyof T`. Knowing this explains every edge case below:

```ts
type Partial<T>  = { [P in keyof T]?: T[P] };
type Required<T> = { [P in keyof T]-?: T[P] };
type Readonly<T> = { readonly [P in keyof T]: T[P] };
```

- `-?` means "remove the optional modifier". The opposite of `?`.
- Because they iterate `keyof T` directly, these are **homomorphic** mapped types: they preserve existing modifiers unless told otherwise. That is why `Readonly<User>` keeps `email` optional.
- There is also a `-readonly` modifier. No built-in uses it, but it is how you write a `Mutable<T>` (see [building custom utility types](./04-building-custom-utility-types.md)).

## Partial: updates and patches

The classic use is an update function where the caller supplies only the fields that change.

```ts
function updateUser(user: User, changes: Partial<User>): User {
  return { ...user, ...changes };
}

updateUser(user, { name: "Asha" }); // ok
updateUser(user, {});               // ok, nothing to change
```

Also common for defaults and options objects:

```ts
interface Options { retries: number; timeoutMs: number; verbose: boolean }

const defaults: Options = { retries: 3, timeoutMs: 5000, verbose: false };

function configure(overrides: Partial<Options> = {}): Options {
  return { ...defaults, ...overrides };
}
```

### Gotcha: `undefined` overwrites on spread

`Partial` makes a property optional, so `undefined` is a legal value for it. Spread copies it faithfully:

```ts
const result = updateUser(user, { name: undefined });
// result.name is undefined at runtime, but the type says string
```

Under default settings this compiles, and `result` is typed `User`, so the lie is invisible. Two fixes:

- Enable [`exactOptionalPropertyTypes`](../13-compiler-and-tsconfig/00-compiler-options.md), which makes `{ name: undefined }` an error for `name?: string`.
- Filter out `undefined` values before merging if the input is not fully trusted.

### Gotcha: weak type detection

A type whose properties are all optional is a "weak type". TypeScript rejects assignments that share no properties with it:

```ts
const patch = { nmae: "typo" };
updateUser(user, patch); // error: no properties in common with Partial<User>
```

This is useful: it catches typos on variables. Object literals get excess property checks anyway.

## Required: enforcing what was optional

Use it when a value starts loosely typed (user input, config file) and becomes fully populated after normalization.

```ts
interface RawConfig { port?: number; host?: string }

function withDefaults(raw: RawConfig): Required<RawConfig> {
  return { port: raw.port ?? 3000, host: raw.host ?? "localhost" };
}
```

`-?` also strips `undefined` from the property type, so `Required<{ a?: string }>` gives `{ a: string }`, not `string | undefined`.

## Readonly: compile-time immutability

```ts
const point: Readonly<{ x: number; y: number }> = { x: 1, y: 2 };
point.x = 5; // error: Cannot assign to 'x' because it is a read-only property
```

Typical uses:

- Function parameters you promise not to mutate: `function render(props: Readonly<Props>)`.
- Config and constant objects.
- Return types of state selectors and reducers.

For arrays, use `readonly T[]` or `ReadonlyArray<T>`. `Readonly<string[]>` also works and produces `readonly string[]`.

```ts
function sum(nums: readonly number[]) {
  nums.push(1); // error: 'push' does not exist on type 'readonly number[]'
}
```

`Object.freeze` is typed to return `Readonly<T>`, which pairs the runtime freeze with the compile-time check.

## Important rules and misconceptions

**All three are shallow.** They only touch the top level.

```ts
interface Team { name: string; lead: { name: string; age: number } }

const t: Readonly<Team> = { name: "A", lead: { name: "Sam", age: 30 } };
t.name = "B";      // error
t.lead.age = 31;   // allowed: nested object is not readonly
```

The same applies to `Partial`: `Partial<Team>["lead"]` is still a *complete* `{ name; age }` object when present. For nested versions you need a recursive custom type:

```ts
type DeepPartial<T> = T extends object
  ? { [K in keyof T]?: DeepPartial<T[K]> }
  : T;
```

This simple version is fine for plain data. It misbehaves on arrays, functions, `Date`, and `Map`, so test it against the shapes you actually have. See [recursive types](../10-advanced-types/05-recursive-types.md) and [building custom utility types](./04-building-custom-utility-types.md).

**`Readonly` does not exist at runtime.** It is erased with the rest of the types. Code that bypasses the type system (`any`, assertions, plain JavaScript callers) can still mutate the object. Use `Object.freeze` if you need enforcement. It is also shallow. See [type erasure](../14-type-system-internals/00-type-erasure-and-runtime.md).

**`readonly` properties are assignable to mutable ones.** This is a known unsoundness:

```ts
const ro: Readonly<{ x: number }> = { x: 1 };
const rw: { x: number } = ro; // allowed
rw.x = 2;                     // mutates the "readonly" object
```

Property `readonly` is not part of assignability. (Readonly *arrays* are stricter: `readonly T[]` is not assignable to `T[]`.)

**`Partial` is not "nullable".** It allows a property to be *absent* (and `undefined`, unless `exactOptionalPropertyTypes` is on), not `null`.

## Common mistakes

- **Using `Partial<T>` for function parameters that are actually required.** The caller can now pass `{}` and you get runtime errors the types said were impossible. Reserve it for genuine patch semantics.
- **Using one `Partial<Entity>` type for create, update, and response.** These have different required fields. Define explicit DTOs instead (see [DTO pattern](../16-type-safe-apis/02-dto-pattern.md)), or combine with `Pick`/`Omit` from the [next note](./01-pick-omit-record.md).
- **Expecting `Readonly<T>` to protect nested data.** It does not. Use a deep version or restructure.
- **Typing a "required after validation" value as `Partial<T>`.** Then every access needs a check. Validate once and return `Required<T>` or a proper full type, as in the `withDefaults` example.

## Debugging

- Hover the alias in your editor, or force expansion with an identity trick, to see the resolved shape:
  ```ts
  type Expand<T> = { [K in keyof T]: T[K] } & {};
  type Result = Expand<Partial<User>>;
  ```
- If `Required<T>` seems to leave `undefined` in a type, check whether the property was declared `a: string | undefined` (required, explicitly unioned) rather than `a?: string`. `-?` only affects optional properties.
- Error "Type 'X' has no properties in common with type 'Partial<Y>'" almost always means a typo or the wrong object was passed.

## Quick summary

- `Partial<T>` adds `?`, `Required<T>` removes it (`-?`), `Readonly<T>` adds `readonly`.
- They are one-line homomorphic mapped types, so they preserve other modifiers and operate on the top level only.
- `Partial` suits patches and options. Beware `undefined` overwriting values on spread.
- `Readonly` is compile-time only and shallow. Reach for `Object.freeze` or deep variants when you need more.
- Prefer explicit types over a blanket `Partial` when the real contract is "these fields required, those optional".

**Next:** [Pick, Omit, and Record](./01-pick-omit-record.md)
