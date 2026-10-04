# Readonly and Optional Properties

## Definition
- **`readonly`** marks a property as not assignable after initialisation.
- **`?`** marks a property as optional: it may be absent, and its type includes `undefined`.

## Why It Matters
These two modifiers express intent: what callers may omit and what must not change. They are used in almost every interface and options object.

## Prerequisites
[interfaces.md](00-interfaces.md), [null and undefined](../01-fundamentals/08-null-and-undefined.md)

## Syntax

```ts
interface Account {
  readonly id: string;     // cannot be reassigned
  name: string;
  nickname?: string;       // may be missing
}
```

## Basic Example

```ts
const a: Account = { id: "a1", name: "Ada" };

a.name = "Grace";     // ok
a.id = "b2";          // Error: Cannot assign to 'id' because it is a read-only property
a.nickname;           // string | undefined
```

## How It Works: `readonly`

### Compile-time only
`readonly` is erased. It does not freeze the object at runtime:

```ts
const p: { readonly x: number } = { x: 1 };
(p as { x: number }).x = 2;   // legal via assertion; runtime allows it
Object.freeze(p);             // runtime immutability is a separate concern
```

### Shallow
```ts
interface State {
  readonly user: { name: string };
}
const s: State = { user: { name: "Ada" } };
s.user = { name: "x" };    // Error
s.user.name = "Grace";     // ok: the nested object is not readonly
```
For deep immutability use a `DeepReadonly` helper (see [building custom utility types](../07-utility-types/04-building-custom-utility-types.md)).

### Readonly arrays and tuples
```ts
const xs: readonly number[] = [1, 2, 3];
xs.push(4);        // Error
xs[0] = 9;         // Error
const ys: ReadonlyArray<number> = xs;   // same type
```

### `Readonly<T>`
```ts
type FrozenUser = Readonly<User>;   // all properties become readonly (shallow)
```

### `readonly` is not `const`
`const` applies to variable bindings; `readonly` applies to properties:

```ts
const o = { a: 1 };
o.a = 2;          // ok
o = { a: 3 };     // Error (const binding)
```

### A readonly value can be passed where mutable is expected
Properties are not tracked for aliasing, so this compiles:

```ts
interface W { x: number }
interface R { readonly x: number }

const r: R = { x: 1 };
const w: W = r;   // allowed
w.x = 2;          // mutates r.x
```
`readonly` is a guard rail, not a guarantee. (Readonly **arrays** are stricter: a `readonly T[]` is not assignable to `T[]`.)

### Class `readonly`
```ts
class Point {
  constructor(public readonly x: number, public readonly y: number) {}
}
```
Assignable only in the constructor or a property initialiser.

## How It Works: Optional Properties

### Type includes `undefined`
```ts
function show(a: Account) {
  a.nickname.toUpperCase();     // Error: possibly undefined
  a.nickname?.toUpperCase();    // ok
  const n = a.nickname ?? a.name;
}
```

### Optional vs. `| undefined`
```ts
type A = { x?: number };             // x may be omitted
type B = { x: number | undefined };  // x must be present (can be undefined)

const a: A = {};                     // ok
const b: B = {};                     // Error: Property 'x' is missing
```

### `exactOptionalPropertyTypes`
By default `{ x?: number }` also accepts `{ x: undefined }`. With `exactOptionalPropertyTypes` enabled, the optional property may be omitted but cannot be explicitly set to `undefined` unless you write `x?: number | undefined`. Useful when `undefined` and "missing" mean different things (for example in PATCH payloads).

### Optional methods
```ts
interface Plugin {
  name: string;
  init?(): void;
}
plugin.init?.();
```

### Optional parameters vs. optional properties
Different features with similar syntax; see [parameters and return types](../02-functions/01-parameters-and-return-types.md).

### Weak types
A type whose properties are **all** optional is a "weak type". Assigning an object that shares no properties with it is an error:

```ts
interface Opts { retries?: number; timeout?: number }
const o: Opts = { retry: 3 };   // Error: no properties in common (typo caught)
```

### Removing or adding modifiers
```ts
type Mutable<T> = { -readonly [K in keyof T]: T[K] };
type AllRequired<T> = { [K in keyof T]-?: T[K] };   // same as Required<T>
type AllOptional<T> = { [K in keyof T]?: T[K] };    // same as Partial<T>
```
See [partial, required, readonly](../07-utility-types/00-partial-required-readonly.md).

## Common Mistakes
- Assuming `readonly` makes an object immutable at runtime.
- Expecting `readonly` to protect nested objects.
- Treating `x?: T` as `T` and getting runtime `undefined` errors.
- Using many optional properties to model different states; use a [discriminated union](../03-unions-and-narrowing/04-discriminated-unions.md).
- Confusing `x?: number` with `x: number | undefined`.

## Best Practices
- Make properties `readonly` by default for data you do not mutate (entities, config, props).
- Keep optional properties for truly optional input, such as options objects.
- Provide defaults when reading optional properties (`opts.retries ?? 3`).
- Enable `exactOptionalPropertyTypes` for APIs that distinguish missing from `undefined`.

## Interview Questions
- Is `readonly` enforced at runtime?
- Is `readonly` shallow or deep?
- Difference between `x?: number` and `x: number | undefined`?
- How do you remove `readonly` from every property of a type?

## Quick Reference
```ts
{ readonly a: T }          // no reassignment
{ a?: T }                  // optional (T | undefined)
Readonly<T>  Partial<T>  Required<T>
{ -readonly [K in keyof T]: T[K] }   // strip readonly
{ [K in keyof T]-?: T[K] }           // strip optional
```

## Related Topics
- [interfaces.md](00-interfaces.md)
- [Partial, Required, Readonly](../07-utility-types/00-partial-required-readonly.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)
