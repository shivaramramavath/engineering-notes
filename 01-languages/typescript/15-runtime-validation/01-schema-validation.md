# Schema Validation

A **schema** is a runtime description of what valid data looks like. Schema libraries give you one definition that does two jobs: it **checks data at runtime** and it **produces the static TypeScript type**, so the two cannot drift apart. This note covers the ideas common to every schema library: how they work, what they all provide, and the design decisions that matter. The next notes apply them to [Zod](./02-zod.md) and [other libraries](./03-other-validation-libraries.md).

**Prerequisites:**
- [Trust boundaries](./00-trust-boundaries.md)
- [Type guards and assertion functions](../03-unions-and-narrowing/05-type-guards-and-assertion-functions.md)
- [infer](../10-advanced-types/02-infer.md) and [mapped types](../10-advanced-types/03-mapped-types.md) (for the "how it works" section)

---

## The idea: one definition, two uses

Without a schema you write the type and the check separately:

```ts
interface User { id: number; email: string }          // compile time

function isUser(x: unknown): x is User { /* ... */ }  // runtime, maintained by hand
```

With a schema you write one thing:

```ts
const UserSchema = object({ id: number(), email: string() });  // runtime check

type User = Infer<typeof UserSchema>;                          // static type, derived
```

If you change the schema, the type changes with it, and the runtime check is the same code. A hand-written pair can silently disagree. This is the central benefit.

## How it works: a tiny schema library

A schema is a **parser**: a function from `unknown` to either a typed value or an error. Building a minimal version shows exactly what libraries do, and reuses ideas from earlier sections.

```ts
type Result<T> = { ok: true; value: T } | { ok: false; error: string };

type Parser<T> = (input: unknown) => Result<T>;

const ok = <T>(value: T): Result<T> => ({ ok: true, value });
const fail = (error: string): Result<never> => ({ ok: false, error });

const string: Parser<string> = (x) =>
  typeof x === "string" ? ok(x) : fail("expected string");

const number: Parser<number> = (x) =>
  typeof x === "number" && !Number.isNaN(x) ? ok(x) : fail("expected number");

// Derive the static type from a parser
type Infer<P> = P extends Parser<infer T> ? T : never;

function object<S extends Record<string, Parser<any>>>(
  shape: S,
): Parser<{ [K in keyof S]: Infer<S[K]> }> {
  return (input) => {
    if (typeof input !== "object" || input === null) return fail("expected object");
    const out: Record<string, unknown> = {};
    for (const key in shape) {
      const r = shape[key]((input as Record<string, unknown>)[key]);
      if (!r.ok) return fail(`${key}: ${r.error}`);
      out[key] = r.value;
    }
    return ok(out as { [K in keyof S]: Infer<S[K]> });
  };
}

const User = object({ id: number, email: string });
type User = Infer<typeof User>;      // { id: number; email: string }

User({ id: 1, email: "a@b.com" });   // { ok: true, value: {...} }
User({ id: "1" });                   // { ok: false, error: "id: expected number" }
```

The real libraries add many more building blocks, better errors, and optimizations, but the structure is the same: each schema is a checker, combinators build bigger checkers out of smaller ones, and a **conditional type with `infer`** extracts the TypeScript type from the schema's type. The `as` inside is the library's own contained, one-time assertion: the runtime check just proved it correct.

## What every schema library provides

| Capability | What it gives you |
|---|---|
| **Primitives and literals** | `string`, `number`, `boolean`, `null`, exact literals, enums |
| **Constraints** | min/max length and value, regex, formats (email, URL, UUID), integer |
| **Objects** | required and optional fields, nested objects, what to do with extra keys |
| **Collections** | arrays, tuples, records/maps, sets |
| **Unions** | "one of these", including **discriminated unions** keyed by a field |
| **Optional, nullable, default** | distinguishing missing, `null`, and a fallback value |
| **Transforms** | convert after validating (string to `Date`, trim, rename) |
| **Refinements** | custom checks (`end` after `start`) |
| **Coercion** | convert strings from forms and query strings to numbers or dates |
| **Composition** | pick, omit, extend, partial, merge schemas |
| **Parse API** | throwing (`parse`) and non-throwing (`safeParse`) |
| **Type inference** | the static type of the output (and sometimes the input) |
| **Structured errors** | list of issues with a path, a code, and a message |

## Design decisions you will face

### Input type vs output type

Schemas that transform data have **two** types: what they accept and what they produce.

```ts
// conceptual: a schema that accepts a string and produces a Date
//   input:  string
//   output: Date
```

Examples: default values (input optional, output required), coercion (input `string`, output `number`), and transforms. Most libraries let you infer both. Use the **output** type inside your program, and the **input** type when describing what clients must send (for example, in API documentation).

### Unknown keys: strip, reject, or keep

When input has fields the schema does not mention:

| Policy | Result | Use when |
|---|---|---|
| **Strip** (common default) | extra keys silently removed | untrusted input: avoids mass assignment |
| **Strict / reject** | validation error | you want clients to know they sent something invalid |
| **Passthrough** | extra keys kept | proxying or wrapping data you do not own |

Stripping is the safest default for request bodies. Never spread an unvalidated body into a database update.

### Optional, nullable, and undefined

Three different things, and APIs mix them up:

- **Optional:** the key may be **absent** (`{}`) or `undefined`.
- **Nullable:** the value may be `null` (the key is present).
- **Both:** `string | null | undefined` when the key may be missing or null.

Decide per field and match your API contract. JSON has `null` but no `undefined`, so `undefined` never arrives from a JSON body, only absent keys.

### Coercion

Query strings, form data, and environment variables are **always strings**. Coercion converts them (`"42"` to `42`). It is convenient and easy to get wrong:

- Coercing to boolean with the language's truthiness rules turns the string `"false"` into `true`. Parse boolean strings explicitly (`"true"` / `"false"`).
- Coercing an empty string to a number often gives `0`, which may be a valid-looking wrong value.
- Coerce at the **boundary** only, and only for fields that arrive as strings.

### Discriminated unions

When data can take several shapes, key them by a literal field (`type: "created" | "deleted"`). Libraries have a dedicated discriminated union that checks the discriminant first and reports errors for only the matching branch, which gives clearer messages and faster validation than a plain union. See [discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md).

### Refinements and cross-field rules

Per-field checks cover most cases. Rules across fields ("`password` equals `confirm`", "`start` before `end`") need a refinement on the object. Refinements usually run **after** the base shape passes, and often attach the error to a specific path so forms can show it on the right field.

### Errors

Good errors tell the caller **where** and **why**:

```ts
// shape of a typical issue
{ path: ["user", "email"], code: "invalid_string", message: "Invalid email" }
```

Return a list of all issues (not just the first), so a form or API client can show every problem at once. Do not expose internal details beyond path and message.

## Schemas and the rest of your types

- **Do not couple your API schemas to your database model.** A request schema, a database row, and a domain object often differ (passwords, internal ids, timestamps). Keep separate schemas or derive carefully ([DTO pattern](../16-type-safe-apis/02-dto-pattern.md)).
- **Derive, do not duplicate.** Use `pick`, `omit`, and `partial` operations on schemas the way you use [utility types](../07-utility-types/README.md) on types.
- **Share schemas between client and server** (in a monorepo or shared package) to get a single contract ([API contracts](../16-type-safe-apis/00-api-contracts.md)).
- **JSON Schema** is a language-neutral format that many tools understand (OpenAPI, form generators, other languages). Some libraries produce it from their schemas, and some are built on it ([other validation libraries](./03-other-validation-libraries.md)).
- **Standard Schema** is a shared interface that several libraries implement, so tools can accept "any compliant schema" instead of one specific library. Check whether your libraries support it.

## Performance and practical limits

- **Create schemas once** at module level and reuse them. Building a schema inside a request handler or loop repeats work.
- Validation cost grows with data size and schema complexity. For very large payloads or hot paths, compare libraries that compile schemas to optimized code.
- **Limit input size** before validating (body size limits, array length caps). A schema cannot protect you from a 500 MB JSON body that has already been read.
- **Async refinements** (database lookups) are slower and should be used sparingly. Consider doing those checks in the service layer instead.

## Common mistakes

- Writing the TypeScript type and the schema separately, then letting them drift.
- Using coercion on booleans and numbers without thinking about `"false"` and `""`.
- Treating `optional` and `nullable` as the same.
- Passing the original input onward instead of the parsed output.
- Returning only the first error when a user needs all of them.
- Putting database lookups in synchronous-looking refinements without handling async.
- Letting unknown keys through on request bodies.
- Defining schemas inside hot paths.
- Using one giant schema for create, update, and response.

## Debugging

- Print the **issues** (path, code, message), not just "validation failed".
- If valid-looking data fails, check coercion, optional versus nullable, and trailing whitespace.
- If the inferred type looks wrong, hover the inferred alias and check whether you are looking at the **input** or **output** type.
- Add a test with the real failing payload and keep it ([validation recipes](./04-validation-recipes.md)).
- If checks pass but behavior is wrong, remember that a schema proves **shape**, not **business correctness**.

## Quick summary

- A schema is a runtime parser from `unknown` to a typed value, with the static type derived from it.
- Every library offers primitives, constraints, objects, unions, defaults, transforms, refinements, coercion, composition, and structured errors.
- Decide input vs output type, unknown-key policy (strip by default for untrusted data), optional vs nullable, and where to coerce.
- Create schemas once, validate at boundaries, return all issues, and keep API schemas separate from storage models.
- Under the hood, a schema is a function plus an `infer`-based type extraction.

**Next:** [Zod](./02-zod.md)