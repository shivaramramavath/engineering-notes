# Soundness and Escape Hatches

A type system is **sound** if a program that type-checks can never hit a type error at runtime. TypeScript is **not** sound, and this is a stated design choice: it aims to be a useful tool for JavaScript developers, not a proof system. That means there are places where the compiler accepts code that can fail, plus explicit escape hatches (`any`, `as`, `!`, `@ts-ignore`) for when you know better. Knowing where the holes are lets you avoid them, contain them, and not be surprised by them.

**Prerequisites:**
- [Assignability and subtyping](./01-assignability-and-subtyping.md)
- [Variance](./02-variance.md)
- [Type erasure and runtime](./00-type-erasure-and-runtime.md)

---

## Why TypeScript is deliberately unsound

TypeScript has to type real-world JavaScript, which is dynamic. A fully sound system would reject a lot of valid, common patterns, and its error messages would be harder to act on. The project's non-goals include applying a "provably correct" type system. The aim is to catch most bugs while staying out of the way.

The practical stance: **types are a strong safety net, not a guarantee.** Treat the holes below as known risk areas and keep your own code out of them where it matters.

## Built-in holes

### 1. `any` switches checking off

```ts
const data: any = JSON.parse(text);
data.user.profile.name.toUpperCase();   // no errors, can crash at runtime
```

`any` is contagious: values derived from it are `any` too, so one `any` can silently disable checking across a whole call chain. Prefer `unknown`, which forces you to narrow.

### 2. Type assertions (`as`)

```ts
const el = document.getElementById("app") as HTMLCanvasElement;   // might be null, might be a div
```

An assertion tells the compiler to believe you. It is checked only for *comparability* (the types must overlap), and `as unknown as T` bypasses even that.

### 3. Non-null assertion (`!`)

```ts
const name = user.profile!.name;   // erased: no runtime check
```

### 4. Mutable arrays are covariant

```ts
const dogs: Dog[] = [new Dog()];
const animals: Animal[] = dogs;
animals.push(new Cat());            // dogs now contains a Cat
```

See [variance](./02-variance.md). Use `readonly` arrays when you do not mutate.

### 5. Method parameters are bivariant

A method declared with method syntax accepts a handler for a narrower argument type than it will actually be called with. Function-typed properties are stricter under `strictFunctionTypes`.

### 6. `readonly` is ignored by assignability

```ts
const ro: { readonly x: number } = { x: 1 };
const rw: { x: number } = ro;   // allowed
rw.x = 2;                       // mutates the "readonly" object
```

### 7. Index signatures and optional access

```ts
const scores: Record<string, number> = {};
scores["missing"].toFixed(2);   // typed number, actually undefined
```

Turn on `noUncheckedIndexedAccess` to make index reads `T | undefined` ([strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)).

### 8. `Object.keys` returns `string[]`

```ts
const key = Object.keys(user)[0];   // string, not keyof typeof user
user[key];                          // error under noImplicitAny: no index signature
```

This is intentional: because of structural typing, an object can have extra keys beyond what its type mentions, so `keyof T` would be unsafe. Cast deliberately where you know the object is exact.

### 9. Narrowing is not invalidated by function calls

```ts
function f(x: { a?: string }) {
  if (x.a !== undefined) {
    clearA(x);              // might set x.a = undefined
    x.a.length;             // still considered string, may crash
  }
}
```

TypeScript does not track side effects of calls on the narrowed object. Copy the value to a `const` before calling anything that might change it.

### 10. Type predicates are trusted

```ts
function isString(x: unknown): x is string {
  return true;              // compiles: the compiler cannot verify your check
}
```

A user-defined type guard is an unchecked claim. A wrong guard poisons everything it narrows.

### 11. Declarations are unchecked

`.d.ts` files, `declare` statements, and `@types` packages describe code the compiler never sees. If they are wrong, so is every type derived from them ([declaration files](../09-declaration-files/00-declaration-files.md)).

### 12. Data from outside the program

`JSON.parse`, `fetch().json()`, `localStorage`, `process.env`, and database results are all typed by assertion or by `any`. The compiler cannot know their shape ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).

### 13. Other known gaps

- `definite assignment` assertions (`x!: number`) promise a field gets set.
- Overload implementation signatures are only loosely checked against the overload list.
- Some operations such as `Object.assign` and spread typing can merge types more optimistically than runtime behavior.
- Excess property checks apply only to fresh literals.
- Enums are looser than unions of literals (numeric enums accept arbitrary numbers in some forms).

## The escape hatches, ranked by safety

From safest to riskiest. Reach for the earliest one that solves the problem.

| Tool | What it does | Risk |
|---|---|---|
| `unknown` + narrowing | accept anything, check before use | none: the compiler enforces the check |
| `satisfies` | verify a value matches a type, keep the precise type | none: it checks and does not cast |
| User-defined type guard / assertion function | run a real check and narrow | low, if the check is correct |
| `as T` (assertion) | override the compiler where types overlap | medium: unchecked claim |
| `!` (non-null assertion) | claim non-null | medium: crashes if wrong |
| `as unknown as T` | force any conversion | high: bypasses comparability |
| `any` | opt out of checking | high: spreads through the code |
| `// @ts-expect-error` | suppress an error on the next line, and **fail if no error occurs** | medium, self-cleaning |
| `// @ts-ignore` | suppress errors on the next line unconditionally | high: stays after the error is gone |

**Prefer `@ts-expect-error` to `@ts-ignore`.** If the underlying problem is later fixed, `@ts-expect-error` becomes an error itself, so stale suppressions do not accumulate.

## Using escape hatches responsibly

- **Contain them.** Put the cast in one small function with a descriptive name and a comment, so the rest of the code stays fully checked.

```ts
// The only place that trusts the DOM
function getCanvas(id: string): HTMLCanvasElement {
  const el = document.getElementById(id);
  if (!(el instanceof HTMLCanvasElement)) {
    throw new Error(`#${id} is not a canvas`);
  }
  return el;
}
```

- **Check, then assert.** An assertion right after a runtime check is fine. An assertion instead of a check is not.
- **Validate at boundaries.** Convert untrusted data once, with a schema or guard, and use real types afterwards.
- **Explain suppressions.** `// @ts-expect-error: lib types missing overload (issue #123)` is reviewable. A bare suppression is not.
- **Tighten the compiler.** `strict`, `noUncheckedIndexedAccess`, and `exactOptionalPropertyTypes` close several holes.
- **Lint for them.** `typescript-eslint` rules such as `no-explicit-any`, the `no-unsafe-*` family, `no-non-null-assertion`, and `ban-ts-comment` make escape hatches visible and reviewable.
- **Measure.** Tools such as `type-coverage` report how much of a codebase flows through `any`.

See [unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md) and [common mistakes](../24-best-practices/01-common-mistakes-and-anti-patterns.md).

## Choosing the safe tool

| You want to... | Instead of | Use |
|---|---|---|
| accept "any value" | `any` | `unknown` and narrow |
| check a literal object against a type without widening | `as T` | `satisfies T` |
| read external JSON as a type | `as T` | a schema (Zod and similar) or a guard |
| handle a possibly-null DOM element | `el!` | an explicit check or helper that throws |
| call something with a mismatched type once | `as any` | a minimal, commented `as unknown as T` in one function |
| silence a known third-party typing error | `@ts-ignore` | `@ts-expect-error` with a reason, or a wrapper |

## Important rules and misconceptions

- **"If it compiles, it works."** Not necessarily. Compiling means the code is consistent with its declared types, not that the declarations match reality.
- **"`as` converts types."** It does not change values ([type erasure](./00-type-erasure-and-runtime.md)).
- **"Unsoundness means TypeScript is not worth it."** The holes are well known, and most real bugs it prevents (typos, missing cases, null misuse, wrong argument order) are not in them.
- **"Strict mode makes it sound."** It closes many holes. It does not make the system sound.
- **`@ts-ignore` and `any` are the same as a bug.** They are tools with costs, to be used deliberately and contained.

## Common mistakes

- Using `any` for convenience and letting it spread through return types.
- Using `as` to *get past* an error rather than to express something already checked.
- Writing type guards that do not actually check enough.
- Leaving bare `@ts-ignore` comments for years.
- Trusting API response types without validation.
- Mutating objects between a narrowing check and its use.

## Debugging

- Search the codebase for the hatches: `as any`, `as unknown as`, `@ts-ignore`, `: any`, `!.`, and `!;`. Each is a place where the compiler is not helping.
- When a runtime error contradicts the types, look upstream for an assertion, an `any`, or an untrusted source. The bug is usually where the type was *claimed*, not where it crashed.
- Enable the `typescript-eslint` unsafe-rules to surface `any` flows.
- Add a runtime assertion at a suspected boundary to find where reality diverges from the declared type.

## Quick summary

- TypeScript is intentionally unsound: `any`, assertions, `!`, covariant arrays, bivariant methods, ignored `readonly`, unchecked index access, trusted type predicates, and untyped external data are the main holes.
- Prefer `unknown`, `satisfies`, and real runtime checks over assertions. Prefer `@ts-expect-error` over `@ts-ignore`.
- Contain unavoidable casts in small, named, commented functions. Validate at system boundaries.
- Tighten with `strict`, `noUncheckedIndexedAccess`, and lint rules, and treat every escape hatch as a reviewable piece of debt.

**Next:** [Compiler architecture](./05-compiler-architecture.md)
