# Type Testing

Most tests check what code **does** at runtime. If you write library types, utility types, or heavily generic APIs, the thing you need to check is what the **compiler concludes**: does `Paths<Config>` produce the right union, does this call fail to compile as it should, did a refactor quietly turn a precise type into `any`? Type-level code has no runtime to test, and it regresses silently, so it needs tests of its own. This note covers how to assert on types, how to assert that something is a *compile error*, and the tools that run these checks.

**Prerequisites:**
- [Type-level programming](../10-advanced-types/08-type-level-programming.md) (the `Equal` / `Expect` helpers)
- [Conditional types](../10-advanced-types/00-conditional-types.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)

---

## Why type tests

- **Utility types and generics** have edge cases (unions, `never`, `any`, optional properties) that are easy to break while editing.
- **Public API types** of a library are part of its contract: a change that makes a type wider or narrower can break consumers without any runtime change.
- **Precision can erode:** a type that silently degrades to `any` or `unknown` still compiles everywhere, and nothing complains.
- **"Should not compile" is behavior too:** invalid calls must be *rejected*, and only a test can check that they are.

## How type tests work

A type test is TypeScript code whose only job is to **fail to compile** when a type is wrong. The test runner is the compiler: if `tsc` reports no errors, the tests pass.

### Assert equality

```ts
type Equal<X, Y> =
  (<T>() => T extends X ? 1 : 2) extends (<T>() => T extends Y ? 1 : 2) ? true : false;

type Expect<T extends true> = T;

type Cases = [
  Expect<Equal<Reverse<[1, 2, 3]>, [3, 2, 1]>>,
  Expect<Equal<CamelCase<"user_first_name">, "userFirstName">>,
  Expect<Equal<ReturnType<typeof createUser>, User>>,
];
```

If any `Equal` is false, `Expect<false>` is an error ("Type 'false' does not satisfy the constraint 'true'") at that line. The tuple just groups cases.

Why not plain `extends`? Because `A extends B` tests **assignability**, which is looser than equality: `string extends string | number` is true, and anything extends `any`. The `Equal` helper compares types as the compiler's own identity check does, so it distinguishes `any` from other types and catches accidental widening.

### Assert that something is an error

Use `// @ts-expect-error` to say "the next line must produce a compile error":

```ts
// @ts-expect-error: a draft order cannot be paid
pay(draftOrder, "p1");

// @ts-expect-error: 'role' is not allowed in the create DTO
const dto: CreateUserDto = { email: "a@b.com", password: "x", role: "admin" };
```

If the line **stops** erroring (for example the type became too permissive), `@ts-expect-error` itself becomes an error ("Unused '@ts-expect-error' directive"). That is exactly what you want from a negative test. Put a reason after the directive, and keep the expression on the very next line.

Do **not** use `@ts-ignore` for this: it stays silent whether or not an error occurs, so it proves nothing.

### Assert values with expected types

```ts
const result = parse("42");
const check: number = result;          // compile error if result is not assignable to number
```

This checks assignability only. For exact checks, use `Equal`.

## Using a library: `expectTypeOf` (Vitest)

Vitest provides `expectTypeOf`, a fluent API for type assertions, plus a typecheck mode that runs the compiler over designated files:

```ts
// paths.test-d.ts
import { expectTypeOf, test } from "vitest";

test("Paths of a config object", () => {
  expectTypeOf<Paths<Config>>().toEqualTypeOf<"server" | "server.host" | "debug">();
});

test("createUser parameters and return type", () => {
  expectTypeOf(createUser).parameter(0).toBeString();
  expectTypeOf(createUser).returns.toEqualTypeOf<User>();
});

test("rejects unknown keys", () => {
  // @ts-expect-error
  expectTypeOf<CreateUserDto>().toHaveProperty("role");
});
```

```ts
// vitest.config.ts
export default defineConfig({
  test: {
    typecheck: { enabled: true },       // run `*.test-d.ts` through the type checker
  },
});
```

`expectTypeOf` assertions are checked by the **compiler**, not at runtime. Calling them at runtime does nothing, which is why they need the typecheck mode (or an equivalent `tsc` run) to mean anything. Details such as file naming and options depend on your Vitest version, so check its documentation.

## Using a library: `tsd`

`tsd` is a tool for testing the type definitions of a library. You write `.test-d.ts` files using `expectType`, `expectError`, and similar helpers, and `tsd` runs the compiler over them:

```ts
import { expectType, expectError } from "tsd";
import { createUser } from ".";

expectType<User>(createUser("Asha"));
expectError(createUser(42));
```

It is popular for published libraries because the tests live beside the package's `.d.ts` output and verify what consumers actually see. Other tools exist for specific ecosystems, for example the checks used for DefinitelyTyped packages.

## Running type tests

- **With the main compile:** put type tests in files included by `tsconfig`, and let `tsc --noEmit` fail on errors. This is the cheapest option, and it runs in your existing `typecheck` script ([test runners](./03-test-runners.md)).
- **With a dedicated tool:** `vitest --typecheck` or `tsd`, which also gives per-test reporting.
- **In CI:** make the type-test run required, so a type regression blocks the merge ([CI and deployment](../21-production-tooling/05-ci-and-deployment.md)).

Keep type tests in separate files (for example `*.test-d.ts` or a `type-tests/` folder) so they are not bundled or executed at runtime, and so the compile cost of heavy type tests is contained.

## What to test

| Target | Example assertion |
|---|---|
| **Return types** of utilities and functions | `Equal<ReturnType<typeof f>, X>` |
| **Generic inference** | `f("a")` infers `"a"`, not `string`, where intended |
| **Edge inputs** | `never`, `any`, `unknown`, unions, empty tuples, optional keys, readonly |
| **Negative cases** | invalid arguments, missing required fields, wrong event names |
| **Public API surface** | exported types keep the shape consumers rely on |
| **No accidental `any`** | `Expect<Equal<IsAny<ReturnType<typeof f>>, false>>` |

A helper for the last row:

```ts
type IsAny<T> = 0 extends 1 & T ? true : false;
```

For a type-level function, test the **cases that distinguish it from near-misses**: union inputs (does it distribute?), `never` (does it return `never`?), a readonly array (does it accept it?), a property that is optional ([distributive conditional types](../10-advanced-types/01-distributive-conditional-types.md)).

## Pitfalls of `Equal`

The strict `Equal` helper treats structurally identical but differently *written* types as different in some cases:

```ts
type A = { a: 1 } & { b: 2 };
type B = { a: 1; b: 2 };

type R = Equal<A, B>;   // false: an intersection is not the same type as the flattened object
```

If you mean "same shape", flatten first with an `Expand` helper:

```ts
type Expand<T> = { [K in keyof T]: T[K] } & {};

type R2 = Equal<Expand<A>, B>;   // true
```

Other points to remember:

- `Equal` distinguishes `any` from other types. This is usually desirable, but it can surprise you when a type unexpectedly contains `any`.
- A failing `Expect<Equal<A, B>>` only says "false". Hover `A` and `B` (or add a temporary `type Show = A;`) to see what the compiler computed.
- `readonly` modifiers and optionality count. `{ a?: 1 }` and `{ a: 1 | undefined }` are different types.

## Testing declaration output and exports

For a library, you can also test the **generated** `.d.ts` files: build the package, then type-check a file that imports from the built output as a consumer would. This catches problems such as missing exports, wrong `types` paths, and accidentally exposed internal types ([declaration files](../09-declaration-files/00-declaration-files.md)). Tools such as `@arethetypeswrong/cli` check module resolution across module modes ([third-party types](../09-declaration-files/04-third-party-types.md)).

## Practice

Type challenge collections (small type-level puzzles, each with its own test cases) are a good way to practice writing and testing types together: [type challenges](../26-projects/exercises/00-type-challenges.md).

## Important rules and misconceptions

- **Type tests run at compile time.** A "passing" test means the file compiled. There is no output at runtime.
- **`expectTypeOf` does nothing unless a type checker runs it.** Without typecheck mode or `tsc`, the file is never checked.
- **`@ts-expect-error` is the way to assert an error. `@ts-ignore` is not.**
- **`extends` is not equality.** Use an `Equal` helper or `toEqualTypeOf` for exact checks.
- **Type tests can slow the compiler** if they instantiate very heavy types. Keep them targeted.
- **A passing type test does not prove the runtime behaves.** Types and values need their own tests.

## Common mistakes

- Using plain `extends` in assertions and missing widening.
- Using `@ts-ignore` instead of `@ts-expect-error` in negative tests.
- Placing `@ts-expect-error` above the wrong line (it applies only to the next line).
- Writing type tests that no tool ever compiles.
- Testing only the happy path, not `never`, unions, `any`, and invalid input.
- Comparing intersection types to flattened ones with strict `Equal`.
- Forgetting to include type test files in `tsconfig`, so `tsc` skips them.
- Asserting on types that depend on `strict`, then running the tests under a different config.

## Debugging

- When an assertion fails, display both types: `type Actual = Computed<Input>;` and hover it, then compare with the expected type.
- Use the TypeScript Playground's `// ^?` annotation to inspect a type quickly.
- Check which `tsconfig` compiles the test file (strictness flags change results).
- If a negative test unexpectedly passes (the expected error is gone), your type became more permissive. That is the regression the test exists to catch.
- If compile time balloons, find the type test that instantiates the heavy type and reduce its input ([type-checking performance](../22-performance/00-type-checking-performance.md)).

## Quick summary

- Type tests make the **compiler** the test runner: they pass when the file compiles and fail when a type is wrong.
- Assert exact types with an `Equal`/`Expect` pair or `expectTypeOf(...).toEqualTypeOf`. Assert errors with `// @ts-expect-error`, never `@ts-ignore`.
- Run them via `tsc --noEmit`, Vitest typecheck mode, or `tsd`, and require them in CI.
- Test the edges: `never`, `any`, unions, optional and readonly members, invalid inputs, and accidental widening to `any`.
- Flatten intersections with `Expand` before strict equality, and keep type tests in separate files.

**Next:** [Debugging](./05-debugging.md)
