# Strict Mode

`"strict": true` turns on a family of type-checking flags that make TypeScript catch the bugs it is best at catching: missing null checks, implicit `any`, unsound function assignments, uninitialized class fields. Almost every serious TypeScript project should enable it. This note explains what each flag does, what errors you will see, which useful flags are **not** included in `strict`, and how to adopt it in an existing codebase.

**Prerequisites:**
- [Compiler options](./00-compiler-options.md)
- [`any` and `unknown`](../01-fundamentals/06-any-and-unknown.md)
- [Null and undefined](../01-fundamentals/08-null-and-undefined.md)

---

## What `strict` includes

`strict` is a shorthand that enables all of the following. You can turn individual flags off after enabling it.

| Flag | Short description |
|---|---|
| `noImplicitAny` | no silent `any` when a type cannot be inferred |
| `strictNullChecks` | `null` and `undefined` are not assignable to other types |
| `strictFunctionTypes` | function parameters are checked contravariantly |
| `strictBindCallApply` | `bind`, `call`, `apply` are type-checked |
| `strictPropertyInitialization` | class properties must be initialized |
| `noImplicitThis` | `this` must have a known type |
| `useUnknownInCatchVariables` | `catch (e)` gives `unknown`, not `any` |
| `alwaysStrict` | parse in strict mode and emit `"use strict"` |
| `strictBuiltinIteratorReturn` | built-in iterators return `undefined`, not `any`, when done (TS 5.6+) |

Future TypeScript versions may add flags to `strict`. Upgrading can surface new errors. That is by design: `strict` means "the strictest set the team considers worthwhile".

## The flags in detail

### `noImplicitAny`

Without it, anything TypeScript cannot infer silently becomes `any`, and checking stops there.

```ts
function double(x) {          // error: Parameter 'x' implicitly has an 'any' type
  return x * 2;
}
```

Fix by annotating (`x: number`). When you truly need "anything", say so with `unknown` and narrow, or write `any` explicitly so the escape hatch is visible.

### `strictNullChecks`

The most valuable flag. Without it, `null` and `undefined` belong to every type. With it, they are separate types you must handle:

```ts
function length(s: string | undefined) {
  return s.length;            // error: 's' is possibly 'undefined'
}

function length2(s: string | undefined) {
  return s?.length ?? 0;      // ok
}

const el: HTMLElement = document.getElementById("app");
// error: 'HTMLElement | null' is not assignable to 'HTMLElement'
```

It catches the single most common JavaScript runtime error ("cannot read properties of undefined"). Handle it with narrowing, optional chaining, `??`, and early returns. Use the non-null assertion (`el!`) sparingly, since it is an unchecked claim.

### `strictFunctionTypes`

Function-typed **parameters** are compared contravariantly, closing a hole where a function that accepts only a subtype could be used as one that accepts any value of the supertype.

```ts
type Handler = (event: Event) => void;

const onClick = (e: MouseEvent) => console.log(e.clientX);
const h: Handler = onClick;   // error under strictFunctionTypes:
                              // a handler for MouseEvent cannot handle any Event
```

Methods declared with method syntax (`handle(e: Event): void`) stay bivariant for compatibility with common patterns like `Array<T>`. Function-property syntax (`handle: (e: Event) => void`) is checked strictly. See [variance](../14-type-system-internals/02-variance.md).

### `strictBindCallApply`

`bind`, `call`, and `apply` are checked against the real function signature:

```ts
function add(a: number, b: number) { return a + b; }

add.call(undefined, 1, "2");   // error: string is not assignable to number
```

Without the flag, those methods accept anything.

### `strictPropertyInitialization`

Class properties must be assigned in the constructor or have an initializer. Requires `strictNullChecks`.

```ts
class User {
  name: string;               // error: not definitely assigned in the constructor
  email?: string;             // ok: optional
  role = "member";            // ok: initialized
  id!: number;                // ok: definite assignment assertion (you promise it is set)

  constructor(name: string) {
    this.name = name;         // assigning here would fix the first error
  }
}
```

Use `!` only for properties set by a framework or lifecycle method that TypeScript cannot see (dependency injection, ORM entities).

### `noImplicitThis`

`this` inside a function with no known context is an error rather than `any`:

```ts
function greet() {
  return this.name;           // error: 'this' implicitly has type 'any'
}
```

Declare it: `function greet(this: { name: string }) { ... }`, or use arrow functions and classes. See [this parameters](../02-functions/04-this-parameters.md).

### `useUnknownInCatchVariables`

`catch (e)` gives `e: unknown`. Narrow before use. See [catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md).

### `alwaysStrict`

Emits `"use strict"` in output and parses source as strict mode JavaScript. ES modules are always strict anyway, so this mostly matters for scripts and CommonJS output.

### `strictBuiltinIteratorReturn`

Built-in iterators (from `Map.entries()`, array iterators, and so on) have their "return value" type set to `undefined` instead of `any`, so the final `{ done: true, value }` result is typed accurately. It rarely produces errors in ordinary code.

## Strictness beyond `strict`

These valuable flags are **not** part of `strict`. Enable them deliberately.

| Flag | Effect | Worth it? |
|---|---|---|
| `noUncheckedIndexedAccess` | `arr[i]` and `obj[key]` for index signatures become `T \| undefined` | Yes, for most projects. Catches out-of-bounds and missing-key bugs. Adds checks in loops. |
| `exactOptionalPropertyTypes` | `a?: string` means "absent", not "present and `undefined`" | Good for precise APIs. Can break libraries not written for it. |
| `noImplicitOverride` | `override` keyword required when overriding a base class member | Yes in class-heavy code. Prevents silent breakage when a base method is renamed. |
| `noImplicitReturns` | every code path must return a value | Yes. |
| `noFallthroughCasesInSwitch` | non-empty `case` must `break`/`return` | Yes. |
| `noPropertyAccessFromIndexSignature` | must write `obj["key"]` for index-signature properties | A matter of taste. |
| `noUnusedLocals` / `noUnusedParameters` | error on unused variables or parameters | Often left to ESLint, which is more flexible. |

Example of `noUncheckedIndexedAccess`:

```ts
const items = ["a", "b"];
const first = items[0];        // string | undefined with the flag
first.toUpperCase();           // error: possibly undefined
```

Community presets such as `@tsconfig/strictest` collect many of these.

## Adopting strict in an existing codebase

Turning everything on at once in a large project can produce thousands of errors. Incremental approaches:

1. **Enable one flag at a time,** easiest first: `noImplicitThis`, `alwaysStrict`, `strictBindCallApply`, `strictFunctionTypes`, `useUnknownInCatchVariables`, then `noImplicitAny`, then `strictNullChecks` (usually the biggest), then `strictPropertyInitialization`.
2. **Count errors per flag** with `tsc --noEmit --strictNullChecks | grep -c "error TS"` before committing to a change.
3. **Fix by type, not by suppression.** If you must suppress, prefer `// @ts-expect-error` with a comment over `// @ts-ignore`. The first fails when the error disappears, so stale suppressions do not accumulate.
4. **Use a ratchet:** a second tsconfig that includes only already-clean files with `strict` on, and grow its `include` over time. There is no per-file `strict` switch in the compiler, so separate configs (or project references) are the mechanism. Some teams use tools such as `typescript-strict-plugin` to get per-folder strictness.
5. **Block regressions** in CI so new code is strict from day one.

See [JS to TS migration](../21-production-tooling/04-js-to-ts-migration.md).

## Important rules and misconceptions

- **`strict: false` is not "the same as no types".** Types are still checked, but with the big holes (null, implicit any) left open.
- **Strictness flags change type checking only.** They do not change the emitted JavaScript (except `alwaysStrict` adding `"use strict"`).
- **Individual flags override `strict`.** `"strict": true, "strictNullChecks": false` leaves `strictNullChecks` off.
- **Strict mode does not validate runtime data.** A `string` from an API can still be `null` if the server sends it. See [runtime validation](../15-runtime-validation/README.md).
- **`!` and `as` are not strict-safe.** They silence checks. Count them as debt.

## Common mistakes

- Starting a new project with `strict` off because "we will turn it on later". It is much harder later.
- Using `any` or `!` to get past `strictNullChecks` instead of handling the case.
- Turning off `strictPropertyInitialization` globally for one framework pattern. Use `!` on those fields instead.
- Enabling `exactOptionalPropertyTypes` and then fighting libraries typed without it.
- Assuming `strict` includes `noUncheckedIndexedAccess` or `noImplicitOverride`. It does not.
- Using `@ts-ignore` and forgetting about it.

## Debugging

- When unsure what a flag does, toggle it in a scratch file and read the error.
- `tsc --showConfig` shows the final values of every strict flag, including those implied by `strict`.
- If errors appear after a TypeScript upgrade, the compiler may have added a flag to `strict`, or tightened an existing check. Check the release notes.
- To find where `any` sneaks in, use ESLint rules from `typescript-eslint` such as `no-explicit-any` and the `no-unsafe-*` family.

## Quick summary

- `strict: true` enables nine flags, including `noImplicitAny`, `strictNullChecks`, `strictFunctionTypes`, and `useUnknownInCatchVariables`.
- `strictNullChecks` and `noImplicitAny` give the most value. Handle `null` and `undefined` with narrowing, not assertions.
- `noUncheckedIndexedAccess`, `noImplicitOverride`, `noImplicitReturns`, and `exactOptionalPropertyTypes` are separate and worth considering.
- Adopt incrementally in old codebases: one flag at a time, `@ts-expect-error` for stragglers, a ratchet config for clean folders.
- Strictness affects checking, not emit, and it does not validate runtime data.

**Next:** [Target, module, and lib](./02-target-module-and-lib.md)
