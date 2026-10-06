# Conditional Types

A conditional type chooses between two types based on a type-level test: `T extends U ? X : Y`. It is the `if` statement of the type system. Almost every advanced utility (`Exclude`, `ReturnType`, `Awaited`, `NonNullable`) is a conditional type underneath, so understanding it unlocks the rest of this section.

**Prerequisites:**
- [Generic types](../06-generics/01-generic-types.md)
- [Generic constraints and defaults](../06-generics/02-generic-constraints-and-defaults.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)

---

## Syntax and basic behavior

```ts
type IsString<T> = T extends string ? true : false;

type A = IsString<string>;   // true
type B = IsString<"hi">;     // true  ("hi" is assignable to string)
type C = IsString<number>;   // false
```

The test `T extends U` means "**is `T` assignable to `U`?**" It is not an equality check. `"hi"` extends `string`, so the first branch is taken.

Conditional types are most useful when they compute a type *from* another type:

```ts
type ElementOf<T> = T extends any[] ? T[number] : T;

type X = ElementOf<string[]>;  // string
type Y = ElementOf<number>;    // number
```

## Chaining

Nest conditionals the way you would chain `else if`:

```ts
type TypeName<T> =
  T extends string ? "string" :
  T extends number ? "number" :
  T extends boolean ? "boolean" :
  T extends undefined ? "undefined" :
  T extends (...args: any[]) => any ? "function" :
  "object";

type N = TypeName<42>;           // "number"
type F = TypeName<() => void>;   // "function"
```

## How it works

When the compiler sees a conditional type, it asks whether the check type is assignable to the extends type.

```text
T extends U ? X : Y

   T known (concrete)          T still generic (unresolved)
        |                              |
  assignable to U?             deferred: the compiler keeps
     /        \                the conditional as-is until T
   yes         no              is filled in
    |           |
    X           Y
```

The **deferred** case is important. Inside a generic function, `T extends string ? A : B` is not resolved, because `T` is not yet known. TypeScript cannot assume either branch, which affects how you write implementations (see below).

### Narrowing inside the true branch

In the true branch, TypeScript knows more about `T`:

```ts
type Lengthy<T> = T extends { length: number } ? T["length"] : never;

type L1 = Lengthy<string>;      // number
type L2 = Lengthy<number[]>;    // number
type L3 = Lengthy<boolean>;     // never
```

`T["length"]` is allowed in the true branch because the compiler treats `T` as having at least the shape it was checked against. Outside that branch it would be an error.

## Practical usage

### Types that depend on options

```ts
type ApiResult<T, Raw extends boolean> =
  Raw extends true ? Response : T;

type Parsed = ApiResult<User, false>;  // User
type RawRes = ApiResult<User, true>;   // Response
```

### Removing or converting wrapper types

```ts
type Unwrap<T> = T extends Promise<infer U> ? U : T;

type A = Unwrap<Promise<string>>;  // string
type B = Unwrap<number>;           // number
```

`infer` captures part of the matched type. It gets its own note: [infer](./02-infer.md).

### Selecting object keys by their value type

Conditionals work inside mapped types too:

```ts
type FunctionKeys<T> = {
  [K in keyof T]-?: T[K] extends (...args: any[]) => any ? K : never;
}[keyof T];

interface Service { name: string; start(): void; stop(): void }
type M = FunctionKeys<Service>;  // "start" | "stop"
```

See [mapped types](./03-mapped-types.md).

## Conditional return types in functions

A tempting pattern is a function whose return type depends on its argument type:

```ts
function parse<T extends string | number>(
  input: T
): T extends string ? number : string {
  return (typeof input === "string" ? Number(input) : String(input)) as any;
}
```

The `as any` is the problem. Because the return type is *deferred*, TypeScript cannot narrow it by checking `typeof input`. It does not link the runtime check to the type-level conditional. The cast makes it compile, but the compiler no longer verifies your implementation.

**Overloads are usually a better fit** for this shape. Each signature is checked precisely, and the implementation signature stays loose:

```ts
function parse(input: string): number;
function parse(input: number): string;
function parse(input: string | number): number | string {
  return typeof input === "string" ? Number(input) : String(input);
}
```

Reach for a conditional return type when the relationship is *generic* and cannot be listed as overloads. See [function overloads](../02-functions/03-function-overloads.md).

## Important rules and misconceptions

**`extends` is assignability, not equality.** `IsString<"hi">` is `true`. If you need exact equality, you need a different technique (see [type-level programming](./08-type-level-programming.md)).

**A conditional on a bare type parameter distributes over unions.** `IsString<string | number>` is `boolean` (`true | false`), not `false`. This is the most surprising behavior in the feature and has its own note: [distributive conditional types](./01-distributive-conditional-types.md).

**`any` as the check type returns both branches.** `IsString<any>` is `boolean`, because `any` is treated as possibly matching and not matching.

**`never` as the check type returns `never`.** This follows from distribution over an empty union.

**Conditional types are lazy.** They only resolve when the type parameters are concrete. Hover over an alias with a generic still unresolved and you will see the conditional itself.

**Recursion is allowed.** A conditional can refer to itself, which is how deep and tuple-walking types are built ([recursive types](./05-recursive-types.md)).

## Common mistakes

- **Expecting equality.** `T extends U` is "assignable to", so subtypes match.
- **Assuming the result is a plain `boolean` for unions.** Distribution can produce `boolean` (a union) instead of a single answer.
- **Using a conditional return type and casting the implementation.** Use overloads if the cases are enumerable.
- **Over-nesting.** A long chain of conditionals is hard to read and slow to check. Name intermediate types.
- **Forgetting that `T extends X ? ...` hides the unresolved state in generics.** Autocomplete and errors inside the generic function will look vague. Test with concrete types.

## Debugging

- Substitute concrete types, one by one, and hover the result:

```ts
type Test1 = TypeName<string>;
type Test2 = TypeName<string | number>;
type Test3 = TypeName<never>;
```

- If the result is `never` when you expect a type, check for distribution over `never` or an over-narrow `extends` clause.
- If you get a union where you expected one answer, wrap both sides in a tuple to stop distribution (`[T] extends [U]`).
- Break a large conditional into named steps and hover each one.
- Add type-level tests so regressions are caught ([type testing](../18-testing-and-debugging/04-type-testing.md)).

## Quick summary

- `T extends U ? X : Y` picks `X` if `T` is assignable to `U`, otherwise `Y`.
- Conditionals are deferred while `T` is generic and resolved once it is concrete.
- On a bare type parameter they distribute over unions. `any` yields both branches and `never` yields `never`.
- Prefer overloads to conditional return types when the cases are enumerable. A cast in the implementation means the compiler is no longer verifying it.
- Combine with `infer`, mapped types, and recursion to build real utilities.

**Next:** [Distributive conditional types](./01-distributive-conditional-types.md)
