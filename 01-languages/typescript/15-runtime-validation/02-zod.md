# Zod

Zod is the most widely used TypeScript-first schema library. You define a schema with a fluent API, parse unknown data with it, and get a fully typed result, with the static type inferred from the schema. This note covers the everyday API, the gotchas, and the patterns that make it useful in real applications.

> **Version note.** Zod 4 (released in 2025) changed or deprecated several APIs from Zod 3, for example top-level string formats like `z.email()`, a new way to customize error messages, and different error-formatting helpers. The core API in this note (`z.object`, `parse`, `safeParse`, `z.infer`, `.optional()`, `.refine`, and so on) works in both. Where behavior differs, it is called out. Run `npm ls zod` and check the docs for your installed version before relying on version-specific details.

**Prerequisites:**
- [Trust boundaries](./00-trust-boundaries.md)
- [Schema validation](./01-schema-validation.md)
- [Generic types](../06-generics/01-generic-types.md)

---

## Install and first schema

```bash
npm install zod
```

```ts
import { z } from "zod";

const UserSchema = z.object({
  id: z.number().int().positive(),
  name: z.string().min(1),
  email: z.string().email(),
  age: z.number().int().min(0).optional(),
});

type User = z.infer<typeof UserSchema>;
// { id: number; name: string; email: string; age?: number | undefined }
```

- `z.infer<typeof Schema>` extracts the static type (the **output** type).
- Schemas are plain values, so you can export, share, and compose them.
- In Zod 4, `z.email()` is the preferred top-level form for email validation, with `z.string().email()` still working but deprecated. Other formats follow the same pattern (`z.uuid()`, `z.url()`).

## Parsing

```ts
// 1. parse: returns the typed value or THROWS a ZodError
const user = UserSchema.parse(data);

// 2. safeParse: never throws, returns a discriminated union
const result = UserSchema.safeParse(data);

if (result.success) {
  result.data;        // User
} else {
  result.error;       // ZodError
}
```

Use `safeParse` at boundaries where invalid input is expected and handled (HTTP handlers, forms). Use `parse` where invalid data is a genuine failure (startup configuration), so it throws and stops. For async refinements or transforms, use `parseAsync` / `safeParseAsync`.

## Errors

A `ZodError` holds a list of **issues**, each with a path, code, and message:

```ts
const result = UserSchema.safeParse({ id: -1, name: "", email: "nope" });

if (!result.success) {
  for (const issue of result.error.issues) {
    console.log(issue.path.join("."), issue.code, issue.message);
  }
}
// id     too_small       ...
// name   too_small       ...
// email  invalid_string  ...  (exact codes and wording vary by Zod version)
```

Helpers to reshape errors differ by version:

- Zod 3: `error.flatten()` gives `{ formErrors, fieldErrors }`, and `error.format()` gives a nested structure.
- Zod 4: top-level helpers such as `z.flattenError(error)`, `z.treeifyError(error)`, and `z.prettifyError(error)`.

For an API response, map issues into your own stable shape instead of exposing Zod's internals ([error response types](../16-type-safe-apis/04-error-response-types.md)):

```ts
const issues = result.error.issues.map((i) => ({
  path: i.path.join("."),
  message: i.message,
}));
```

### Custom messages

```ts
z.string().min(3, "Name must be at least 3 characters");   // works in Zod 3 and 4
z.string().min(3, { error: "Too short" });                  // Zod 4 style
```

## Objects

```ts
const Base = z.object({ name: z.string() });

Base.parse({ name: "a", extra: 1 });   // { name: "a" }   (unknown keys are STRIPPED by default)
```

| Goal | Zod 3 | Zod 4 |
|---|---|---|
| strip unknown keys (default) | `z.object({...})` | `z.object({...})` |
| reject unknown keys | `.strict()` | `z.strictObject({...})` |
| keep unknown keys | `.passthrough()` | `z.looseObject({...})` |

Stripping by default is what you want for request bodies. It prevents extra fields from reaching your database layer.

### Reuse: pick, omit, partial, extend

These mirror TypeScript's utility types ([Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md)):

```ts
const CreateUser = UserSchema.omit({ id: true });
const UpdateUser = UserSchema.partial();                     // every field optional
const PublicUser = UserSchema.pick({ id: true, name: true });
const Admin = UserSchema.extend({ role: z.literal("admin") });
```

In Zod 4, prefer `.extend()` or spreading `.shape` over the older `.merge()`, which is deprecated.

## Optional, nullable, and default

```ts
z.string().optional();        // string | undefined  (key may be missing)
z.string().nullable();        // string | null
z.string().nullish();         // string | null | undefined
z.string().default("n/a");    // input may be missing, output is always string
```

`.default()` makes the **input** optional and the **output** required. This is the standard way to apply defaults at the boundary.

## Collections, enums, literals

```ts
z.array(z.string()).min(1).max(10);
z.tuple([z.string(), z.number()]);
z.record(z.string(), z.number());           // { [key: string]: number }
z.enum(["admin", "editor", "viewer"]);      // "admin" | "editor" | "viewer"
z.literal("ok");
z.set(z.string());
```

`z.enum([...])` gives a union of string literals, and `schema.options` gives the values at runtime. This pairs well with the "runtime value first, type derived" approach ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)).

## Unions and discriminated unions

```ts
const Result = z.union([z.string(), z.number()]);

const Shape = z.discriminatedUnion("kind", [
  z.object({ kind: z.literal("circle"), radius: z.number() }),
  z.object({ kind: z.literal("square"), size: z.number() }),
]);

type Shape = z.infer<typeof Shape>;
// { kind: "circle"; radius: number } | { kind: "square"; size: number }
```

Prefer `discriminatedUnion` for tagged data: it checks the discriminant first, gives clearer errors, and the inferred type narrows with a `switch` on `kind` ([discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)).

## Refinements and transforms

**`refine`** adds a custom check. **`superRefine`** adds several issues with custom paths.

```ts
const Signup = z
  .object({
    password: z.string().min(8),
    confirm: z.string(),
  })
  .refine((v) => v.password === v.confirm, {
    message: "Passwords do not match",
    path: ["confirm"],            // attach the error to a specific field
  });
```

**`transform`** converts the value after validation and changes the output type:

```ts
const DateString = z.string().transform((s) => new Date(s));
// input: string, output: Date

const Trimmed = z.string().trim().toLowerCase();   // built-in string transforms
```

Combining: a validated string into a refined date:

```ts
const IsoDate = z
  .string()
  .transform((s) => new Date(s))
  .refine((d) => !Number.isNaN(d.getTime()), { message: "Invalid date" });
```

### Input and output types

```ts
const Schema = z.object({
  page: z.coerce.number().default(1),
});

type In = z.input<typeof Schema>;    // what the schema accepts
type Out = z.output<typeof Schema>;  // what it returns (z.infer is the same as z.output)
```

Use `z.output` (or `z.infer`) inside your program. Use `z.input` for describing what callers may send.

## Coercion

For data that arrives as strings (query params, form data, env vars):

```ts
z.coerce.number();    // "42" -> 42
z.coerce.date();      // "2024-01-01" -> Date
z.coerce.boolean();   // WARNING: see below
```

Two traps:

- **`z.coerce.boolean()` uses JavaScript truthiness:** the string `"false"` becomes `true`. For string flags, parse explicitly:

```ts
const BoolString = z.enum(["true", "false"]).transform((v) => v === "true");
```

- **`z.coerce.number()` on `""` gives `0`,** and `null` also becomes `0`. If an empty field should be an error or "absent", handle it explicitly.

Coerce only fields that really arrive as strings, and only at the boundary.

## Branding

Attach a brand so validated values cannot be confused with plain strings ([branded types](../10-advanced-types/07-branded-types.md)):

```ts
const UserId = z.string().uuid().brand<"UserId">();
type UserId = z.infer<typeof UserId>;

function load(id: UserId) {}

load("raw-string");               // error
load(UserId.parse(input));        // ok
```

## Practical patterns

### Environment configuration

```ts
const Env = z.object({
  NODE_ENV: z.enum(["development", "production", "test"]).default("development"),
  PORT: z.coerce.number().int().positive().default(3000),
  DATABASE_URL: z.string().url(),
});

export const env = Env.parse(process.env);   // throws at startup if invalid
```

Failing at startup with a clear message beats discovering a missing variable mid-request ([config and environment](../20-nodejs-backend/01-config-and-environment.md)). In Zod 4, `z.url()` is the preferred form.

### Typed API client

```ts
async function fetchJson<S extends z.ZodType>(url: string, schema: S): Promise<z.output<S>> {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return schema.parse(await res.json());
}

const user = await fetchJson("/api/users/1", UserSchema);   // User
```

More patterns are in [validation recipes](./04-validation-recipes.md).

### Forms

Form libraries integrate through resolvers (for example `@hookform/resolvers/zod` for React Hook Form), so the same schema validates in the browser and on the server ([forms](../19-react-and-frontend/05-forms.md)).

### Generating JSON Schema

Zod 4 includes `z.toJSONSchema(schema)` for producing JSON Schema, which feeds OpenAPI documents and other tools. For Zod 3, a separate package (`zod-to-json-schema`) fills the role. Check what your version provides.

## Important rules and misconceptions

- **A schema is not the type.** `UserSchema` is a runtime value. `z.infer<typeof UserSchema>` is the type. You often export both under related names.
- **`parse` returns a new, cleaned object.** Use the return value, not the input. Stripped keys and transforms apply only to the output.
- **Types go through `.optional()` as `T | undefined`.** The key can be absent.
- **`refine` runs only if the base shape is valid.** Errors on missing fields appear first.
- **Schemas are immutable.** Methods like `.min()` and `.optional()` return new schemas.
- **Zod does not validate at compile time.** It validates at **runtime**, and TypeScript trusts its inferred type afterward.
- **Large schemas and deeply nested unions cost compile time too.** Heavy inference can slow editor feedback ([type-checking performance](../22-performance/00-type-checking-performance.md)).

## Common mistakes

- Using `.parse` and forgetting to catch `ZodError` in a request handler (returns 500 instead of 400).
- Using the **input** instead of the parsed result.
- Using `z.coerce.boolean()` on string flags.
- Using `.optional()` when the API sends `null` (use `.nullable()` or `.nullish()`).
- Building schemas inside handlers on every request.
- Duplicating the same shape as an `interface` and a schema.
- Using `z.any()` or `z.unknown()` as a shortcut, which discards the safety you added the schema for.
- Mixing Zod 3 and 4 idioms (deprecated methods, error helpers) across the codebase.
- Exposing raw `ZodError` objects to API clients.

## Debugging

- **Print issues:** `console.log(JSON.stringify(result.error.issues, null, 2))` shows path, code, and message for every failure.
- **Hover the inferred type** (`z.infer`) and compare to what you expect. If `age` is `number | undefined` and you wanted `number`, the schema is optional.
- **Check input versus output** when transforms, defaults, or coercion are involved (`z.input` / `z.output`).
- **Isolate:** parse the sub-schema (`UserSchema.shape.email.safeParse(value)`) to find which field fails.
- **Check the version** if an API from a tutorial does not exist or is deprecated.

## Quick summary

- Define a schema once, derive the type with `z.infer`, validate with `parse` or `safeParse`.
- Objects strip unknown keys by default. Use strict or loose variants deliberately.
- `.default()` and coercion convert at the boundary, with traps for `coerce.boolean()` and empty strings.
- `discriminatedUnion` for tagged data, `refine` for cross-field rules, `transform` for conversion, `brand` for nominal-style types.
- Use `z.output` inside the app and `z.input` for what callers send. Return a stable error shape, not raw `ZodError`.
- Zod 3 and Zod 4 differ in some APIs: check the version you have installed.

**Next:** [Other validation libraries](./03-other-validation-libraries.md)