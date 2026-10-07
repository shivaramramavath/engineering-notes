# Type-Level Programming

TypeScript's type system is expressive enough to compute with. Generic aliases act as **functions**, conditional types as **`if`**, recursion as **loops**, tuples as **arrays**, and template literal types as **string operations**. This note ties the previous techniques together, shows the idioms that recur, and, more importantly, says when to stop.

**Prerequisites:**
- [Conditional types](./00-conditional-types.md), [infer](./02-infer.md), [mapped types](./03-mapped-types.md)
- [Template literal types](./04-template-literal-types.md)
- [Recursive types](./05-recursive-types.md) and [variadic tuple types](./06-variadic-tuple-types.md)

---

## The mapping

| Value-level programming | Type-level equivalent |
|---|---|
| function | generic type alias: `type F<A, B> = ...` |
| `if / else` | `A extends B ? X : Y` |
| variable binding | `infer X` |
| `for` / `while` | recursion with a base case |
| array | tuple |
| number | `tuple["length"]` |
| `string.split`, `slice` | template literal pattern matching |
| `.map` over an object | mapped type |
| unit test | `Expect<Equal<A, B>>` |

Nothing here runs. The "result" is a type, computed by the compiler during checking.

## Idioms you will meet repeatedly

### Testing types

Type-level code needs tests, and they run at compile time:

```ts
type Equal<X, Y> =
  (<T>() => T extends X ? 1 : 2) extends (<T>() => T extends Y ? 1 : 2) ? true : false;

type Expect<T extends true> = T;

type _t1 = Expect<Equal<Reverse<[1, 2, 3]>, [3, 2, 1]>>;
type _t2 = Expect<Equal<CamelCase<"user_name">, "userName">>;
```

If an assertion is false, `Expect<...>` fails to compile. Plain `extends` is not equality (it is assignability), which is why `Equal` uses this generic-function comparison. See [type testing](../18-testing-and-debugging/04-type-testing.md).

### Arithmetic with tuple length

Numbers cannot be added directly, but tuple lengths can:

```ts
type BuildTuple<N extends number, T extends unknown[] = []> =
  T["length"] extends N ? T : BuildTuple<N, [...T, unknown]>;

type Add<A extends number, B extends number> =
  [...BuildTuple<A>, ...BuildTuple<B>]["length"];

type Five = Add<2, 3>;   // 5
```

It works by building tuples of the right length, concatenating them, and reading the length. The practical ceiling is around a thousand because of recursion limits, and `BuildTuple<number>` never terminates. Treat this as a demonstration of the mechanism more than a production tool.

### String transformations

```ts
type CamelCase<S extends string> =
  S extends `${infer Head}_${infer Tail}`
    ? `${Lowercase<Head>}${Capitalize<CamelCase<Tail>>}`
    : Lowercase<S>;

type CamelKeys<T> = {
  [K in keyof T as K extends string ? CamelCase<K> : K]: T[K];
};

type Row = { user_id: number; first_name: string };
type Model = CamelKeys<Row>;   // { userId: number; firstName: string }
```

This is a realistic one: typing the result of a snake_case to camelCase conversion at an API boundary.

### Union to intersection

```ts
type UnionToIntersection<U> =
  (U extends unknown ? (x: U) => void : never) extends (x: infer I) => void
    ? I
    : never;
```

This exploits distribution plus the contravariance of function parameters ([infer](./02-infer.md), [variance](../14-type-system-internals/02-variance.md)). It is a standard building block for merging a union of objects.

### Membership and counting

```ts
type Includes<T extends readonly unknown[], U> =
  T extends [infer H, ...infer R]
    ? Equal<H, U> extends true ? true : Includes<R, U>
    : false;

type Yes = Includes<["a", "b"], "b">;   // true
```

### Validate at the call site

A common real use: make invalid arguments fail to compile.

```ts
type ValidateKeys<T, Allowed extends string> =
  Exclude<keyof T, Allowed> extends never ? T : never;
```

Combine such checks with generic functions so the call site, not the implementation, reports the error. See [generic inference](../06-generics/05-generic-inference.md).

## Where it pays off

- **Library APIs** that must infer precise types from user input: routers, ORMs and query builders, form libraries, state machines, typed event emitters. The complexity lives in the library and users get autocomplete and errors for free.
- **Boundaries with strings:** route params, SQL-ish fragments, CSS values, path-based accessors.
- **Deriving types from a single source of truth** (a schema, a config object, a route table) so related types cannot drift.

## Where it does not

- **Application code.** If a teammate needs ten minutes to read a type, it is probably the wrong tool. A hand-written type, a function overload, or a runtime check is easier to maintain.
- **Anything that duplicates runtime logic.** Types that re-implement parsing or arithmetic compile slowly and rarely stay in sync.
- **Large inputs.** Recursion limits and union explosion are real, and the failures are cryptic.

## Costs to weigh

- **Compile time and editor speed.** Heavy types slow type checking and make autocomplete lag. See [type-checking performance](../22-performance/00-type-checking-performance.md).
- **Error messages.** Failures surface as huge expanded types or "excessively deep" errors (TS2589), which are hard to act on.
- **Maintainability.** The reader must reverse-engineer a program written in an awkward language, with no debugger.
- **Compiler version sensitivity.** Behavior at the edges can change between TypeScript releases. Pin versions and keep tests.

## Guidelines

- **Keep the public surface simple.** Expose a clearly named type (`Params<"/users/:id">`) and hide the machinery.
- **Write small, named helpers** instead of one giant conditional. Hover each step.
- **Use accumulators** for tail recursion ([recursive types](./05-recursive-types.md)).
- **Have a base case and an escape hatch** for inputs you do not support, such as `string` where a literal was expected.
- **Test with edge shapes:** `never`, `any`, `unknown`, unions, optional properties, readonly, empty tuples.
- **Document intent**, including the input shape the type assumes.
- **Stop when a runtime function would be clearer.** Types and values can complement each other: a typed function with a simple signature and a small runtime check is often better than a clever type.

## Common mistakes

- **Using `extends` as equality.** Use `Equal` for exact comparisons.
- **Forgetting distribution** and getting a union where one answer was expected ([distributive conditional types](./01-distributive-conditional-types.md)).
- **Missing base cases** or recursing on non-literal inputs.
- **Making types so clever that error messages become unreadable.**
- **Skipping tests.** Type-level code regresses silently.
- **Reaching for it too early.** Start with the simple version and add precision only when users benefit.

## Debugging

- Substitute concrete inputs and hover every intermediate type.
- Reduce the input to the smallest case that gives the wrong answer.
- Use `Expand<T>` (`{ [K in keyof T]: T[K] } & {}`) to flatten intersections in tooltips.
- Check for `never` from distribution and for `any` leaking into branches.
- Measure: `tsc --extendedDiagnostics` shows check time and instantiation counts.

```bash
tsc --noEmit --extendedDiagnostics
```

## Quick summary

- Types can compute: aliases are functions, conditionals are `if`, recursion is a loop, tuples and template literals are data.
- Idioms: `Equal`/`Expect` tests, tuple-length arithmetic, string parsing, `UnionToIntersection`, membership checks.
- Worth it in library and boundary code where precise inference helps many callers. Rarely worth it in application code.
- Cost: slower checking, harder errors, harder maintenance. Keep the public API simple and test the internals.

**Next:** [11 Error Handling](../11-error-handling/README.md)