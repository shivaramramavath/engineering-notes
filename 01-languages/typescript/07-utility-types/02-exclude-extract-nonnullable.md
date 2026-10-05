# Exclude, Extract, and NonNullable

`Exclude`, `Extract`, and `NonNullable` operate on **union types**, not on object keys. They filter a union: remove some members, keep some members, or strip `null` and `undefined`. They are the building blocks behind `Omit` and many custom utilities.

**Prerequisites:**
- [Union types](../03-unions-and-narrowing/00-union-types.md)
- [Conditional types](../10-advanced-types/00-conditional-types.md)
- [Distributive conditional types](../10-advanced-types/01-distributive-conditional-types.md) (this is the mechanism that makes them work)

---

## Definitions

```ts
type Exclude<T, U> = T extends U ? never : T;
type Extract<T, U> = T extends U ? T : never;
type NonNullable<T> = T & {};
```

`NonNullable<T> = T & {}` is the definition since TS 4.8. Before that it was `T extends null | undefined ? never : T`. The behavior for ordinary unions is the same.

## How the filtering works

`Exclude` and `Extract` are conditional types applied to a **naked type parameter**, so they distribute over each member of a union. `never` disappears from a union, which is what makes the filtering work:

```text
Exclude<"a" | "b" | "c", "a">
  = ("a" extends "a" ? never : "a")
  | ("b" extends "a" ? never : "b")
  | ("c" extends "a" ? never : "c")
  = never | "b" | "c"
  = "b" | "c"
```

`Extract` is the mirror image: it keeps the members assignable to `U`.

## Basic usage

```ts
type Status = "idle" | "loading" | "success" | "error";

type Settled = Exclude<Status, "idle" | "loading">; // "success" | "error"
type Busy    = Extract<Status, "loading" | "idle">;  // "idle" | "loading"
```

Matching is by **assignability**, not equality. `U` can be a wide type:

```ts
type Mixed = string | number | boolean | (() => void);

type Primitives = Extract<Mixed, string | number | boolean>; // string | number | boolean
type NotFns     = Exclude<Mixed, Function>;                   // string | number | boolean
```

## Practical usage

### Pick one member of a discriminated union

```ts
type AppEvent =
  | { type: "click"; x: number; y: number }
  | { type: "key"; code: string }
  | { type: "scroll"; delta: number };

type ClickEvent = Extract<AppEvent, { type: "click" }>;
// { type: "click"; x: number; y: number }

function handleClick(e: ClickEvent) {
  console.log(e.x, e.y);
}
```

This works because the `click` member is assignable to `{ type: "click" }`. It keeps the handler type in sync with the union instead of redefining the shape. See [discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md).

### Remove keys from a key union

```ts
type KeysWithoutId<T> = Exclude<keyof T, "id">;
```

This is exactly how `Omit` is built.

### Extract all event names, or all keys of one type

```ts
type EventName = AppEvent["type"];                 // "click" | "key" | "scroll"
type Pointer   = Exclude<EventName, "key">;        // "click" | "scroll"

type StringKeys<T> = Extract<keyof T, string>;
```

`Extract<keyof T, string>` is a common idiom, because `keyof T` can include `number` and `symbol`, which do not fit in template literal types.

### NonNullable: strip null and undefined

```ts
type MaybeName = string | null | undefined;
type Name = NonNullable<MaybeName>; // string
```

The common runtime pairing is a type guard that narrows by this type:

```ts
function isDefined<T>(value: T): value is NonNullable<T> {
  return value != null;
}

const raw: (string | undefined)[] = ["a", undefined, "b"];
const clean = raw.filter(isDefined); // string[]
```

Without a guard, older compilers keep `undefined` in the result of `filter` because it only narrows with an explicit type predicate. Newer TypeScript can infer a predicate for simple arrow functions such as `x => x !== undefined`, but an explicit guard is still the clearest option when intent matters.

## Important rules and misconceptions

**These work on unions, not on object types.** `Exclude<{ a: 1; b: 2 }, "a">` does not remove property `a`. For removing object keys, use `Omit` (see [Pick, Omit, Record](./01-pick-omit-record.md)).

**You cannot subtract from a non-union type.** `Exclude<string, "a">` is `string`, because `string` is not assignable to `"a"` and TypeScript has no negated types. The same goes for `Exclude<number, 0>`.

**Neither type checks that `U` is related to `T`.** `Exclude<"a" | "b", "c">` is just `"a" | "b"`, with no error. A typo in `U` silently does nothing.

**`Exclude<T, never>` returns `T`, and `Extract<T, never>` returns `never`.** Edge cases worth knowing when you write generic code.

**`NonNullable` removes only `null` and `undefined`.** It does not remove `0`, `""`, `false`, or `NaN`. It describes types, not truthiness.

**With generics, `NonNullable<T>` is `T & {}`.** For `T = unknown` you get `{}`, which is what you want: "any non-null value".

**Optional properties are unaffected by `NonNullable<T>` on the object.** `NonNullable<{ a?: string }>` is still `{ a?: string }`. To make a property non-optional, use `Required`.

## Common mistakes

- **Using `Exclude` to remove properties from an object type.** Use `Omit`.
- **Filtering a union that is really `string`.** `Exclude<string, "admin">` cannot express "any string except admin".
- **Assuming `NonNullable` is a runtime check.** It only changes the type. Validate at runtime with a guard or a schema (see [runtime validation](../15-runtime-validation/00-trust-boundaries.md)).
- **Forgetting distribution.** If you wrap `T` (for example `[T] extends [U]`), the conditional no longer distributes, and the result is a single yes/no answer instead of a filtered union.

## Debugging

- Hover the result type alias. If you see `never`, nothing matched: check that `T` really is a union of what you think, and that members are assignable to `U`.
- If `Extract<Union, Shape>` returns too much or too little, remember the match is **assignability**: `{ type: "click"; x: number }` is assignable to `{ type: string }`, so a broad `U` pulls in extra members.
- For intermediate steps, break a complex alias into named pieces and hover each one.

## Quick summary

- `Exclude<T, U>` removes union members assignable to `U`. `Extract<T, U>` keeps them.
- Both are distributive conditional types, so they work member by member and rely on `never` vanishing from unions.
- They work on unions only. For object keys use `Omit`/`Pick`.
- `NonNullable<T>` strips `null` and `undefined` (`T & {}` since TS 4.8) and is a type-level operation only.

**Next:** [Function and class utilities](./03-function-and-class-utilities.md)
