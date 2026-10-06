# Distributive Conditional Types

When a conditional type is applied to a **union**, TypeScript can evaluate the conditional once for each member and union the results. This is called *distribution*. It is why `Exclude` and `Extract` filter unions, and also why simple-looking conditionals sometimes return surprising answers. Knowing when it happens, and how to turn it off, removes a whole class of confusion.

**Prerequisites:**
- [Conditional types](./00-conditional-types.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)

---

## The rule

A conditional type distributes when its **check type is a naked type parameter**, meaning a bare `T` with nothing wrapped around it, and `T` is instantiated with a union.

```ts
type ToArray<T> = T extends unknown ? T[] : never;

type R = ToArray<string | number>;
// string[] | number[]
```

What the compiler does:

```text
ToArray<string | number>
  = ToArray<string> | ToArray<number>
  = string[]        | number[]
```

Each member is checked separately, then the results are combined into a union.

## Turning distribution off

Wrap both sides of `extends` in a tuple (or any non-naked form). The check type is then `[T]`, not a bare `T`, so there is nothing to distribute:

```ts
type ToArrayNonDist<T> = [T] extends [unknown] ? T[] : never;

type R = ToArrayNonDist<string | number>;
// (string | number)[]
```

Side by side:

| Definition | `X<string \| number>` |
|---|---|
| `T extends unknown ? T[] : never` (distributive) | `string[] \| number[]` |
| `[T] extends [unknown] ? T[] : never` (not distributive) | `(string \| number)[]` |

Pick the one that matches the question you are asking: "transform each member" or "treat the union as one unit".

## What counts as "naked"

Only a type parameter checked directly distributes:

```ts
type A<T> = T extends string ? 1 : 2;       // distributes
type B<T> = [T] extends [string] ? 1 : 2;   // does NOT
type C<T> = T[] extends string[] ? 1 : 2;   // does NOT (T is not the check type itself)
type D<T> = (T & {}) extends string ? 1 : 2;// does NOT
```

Also, distribution only happens in a generic alias. A directly written conditional on a literal union is just evaluated:

```ts
type E = (string | number) extends string ? 1 : 2;   // 2, no type parameter involved
```

## The built-ins rely on it

```ts
type Exclude<T, U> = T extends U ? never : T;
type Extract<T, U> = T extends U ? T : never;
```

Distribution is what lets these filter a union. `never` disappears from unions, so members that map to `never` are dropped:

```ts
type R = Exclude<"a" | "b" | "c", "a">;
// = (never) | ("b") | ("c")
// = "b" | "c"
```

See [Exclude, Extract, NonNullable](../07-utility-types/02-exclude-extract-nonnullable.md).

## Practical usage

### Apply an operation to every member

```ts
type Boxed<T> = T extends unknown ? { value: T } : never;

type R = Boxed<string | number>;
// { value: string } | { value: number }
```

Without distribution you would get `{ value: string | number }`, which allows mixed shapes you did not intend.

### Operate on each member of a discriminated union

```ts
type DistributiveOmit<T, K extends PropertyKey> =
  T extends unknown ? Omit<T, K> : never;
```

Plain `Omit` flattens a union to its common keys. Distributing it keeps each member's own shape. See [Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md).

### Detect `never`

Because `never` is an empty union, a distributive conditional over `never` has nothing to evaluate and returns `never`, no matter what the branches are:

```ts
type IsNever<T> = T extends never ? true : false;
type R1 = IsNever<never>;   // never  (not true!)

type IsNeverFixed<T> = [T] extends [never] ? true : false;
type R2 = IsNeverFixed<never>;   // true
```

The tuple wrapper is the standard fix.

### Detect a union

A known trick combines distribution with a second copy of the type:

```ts
type IsUnion<T, U = T> =
  T extends unknown
    ? [U] extends [T] ? false : true
    : never;

type U1 = IsUnion<string | number>;  // true
type U2 = IsUnion<string>;           // false
```

How it works: `U` keeps the **whole** union while `T` is distributed to one member at a time. If the whole union is not assignable to a single member, it must have more than one.

### `boolean` is a union

`boolean` is `true | false`, so it distributes too:

```ts
type Label<T> = T extends true ? "yes" : "no";
type R = Label<boolean>;  // "yes" | "no"
```

If you want one answer for `boolean`, use `[T] extends [true]`.

## Important rules and misconceptions

**Distribution is about the type parameter, not the word "union".** If the argument is not a union, there is nothing to distribute and you get the ordinary result.

**It happens at instantiation.** A union you write directly in the conditional is not distributed. Only a union substituted for a type parameter is.

**`never` in gives `never` out** for distributive conditionals.

**`any` is different.** A conditional over `any` returns the union of both branches (`1 | 2`), not a per-member result.

**Distribution is often the behavior you want.** Do not wrap in a tuple by reflex. Wrap only when you want the union treated as one type.

## Common mistakes

- **Expecting one answer for a union argument** and getting a union back (`boolean` from a yes/no check).
- **Using `T extends never ? ...` to detect `never`.** It returns `never`. Use `[T] extends [never]`.
- **Wrapping in a tuple and breaking filtering utilities.** `[T] extends [U] ? never : T` is not `Exclude`: it removes nothing from a union unless the whole union matches.
- **Trying to distribute over `T[]` or `T & X`.** Those forms are not naked parameters.
- **Debugging with a union argument first.** Test with a single member, then a union, then `never`.

## Debugging

- Substitute one member, then two, then `never`:

```ts
type T1 = MyType<string>;
type T2 = MyType<string | number>;
type T3 = MyType<never>;
```

- If a result is a union you did not expect, look for a naked `T extends ...` at the start of the conditional.
- If a result is unexpectedly `never`, check for `never` as an argument.
- To force one-answer behavior, wrap both sides: `[T] extends [U]`.
- To force distribution when you wrapped it by accident, add an identity step: `T extends unknown ? ... : never`.

## Quick summary

- A conditional on a naked type parameter distributes over a union argument and unions the results.
- Distribution is why `Exclude`, `Extract`, and `NonNullable` filter unions, and why `boolean` can produce `"yes" | "no"`.
- Wrap with `[T] extends [U]` to stop distribution. A distributive conditional over `never` returns `never`.
- `IsNever` needs the tuple wrapper. `IsUnion` needs distribution plus a second copy of the type.
- Use distribution deliberately: per-member transforms want it; whole-union tests do not.

**Next:** [infer](./02-infer.md)
