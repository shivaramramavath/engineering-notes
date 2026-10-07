# Validation Recipes

Concrete patterns for validating data at the places it enters your program. The examples use Zod, since it is the most common choice, but each recipe translates to any schema library. Check the [Zod version note](./02-zod.md) before copying code, because some APIs differ between Zod 3 and 4.

**Prerequisites:**
- [Trust boundaries](./00-trust-boundaries.md)
- [Schema validation](./01-schema-validation.md)
- [Zod](./02-zod.md)
- [The Result pattern](../11-error-handling/02-result-pattern.md)

---

## 1. A typed `fetchJson` wrapper

Every external API call goes through one function that returns a **validated** value, not `any`.

```ts
import { z } from "zod";

class HttpError extends Error {
  constructor(readonly status: number, message: string) {
    super(message);
    this.name = "HttpError";
  }
}

export async function fetchJson<S extends z.ZodType>(
  url: string,
  schema: S,
  init?: RequestInit,
): Promise<z.output<S>> {
  const res = await fetch(url, init);
  if (!res.ok) throw new HttpError(res.status, res.statusText);
  const body: unknown = await res.json();
  return schema.parse(body);
}

const PostSchema = z.object({ id: z.number(), title: z.string() });
const post = await fetchJson("/api/posts/1", PostSchema);   // { id: number; title: string }
```

Notes:

- `res.json()` is assigned to `unknown` immediately. The cast to `any` never leaks.
- `z.output<S>` gives the parsed type for any schema passed in.
- If the server's contract changes, the failure is a clear `ZodError` at the boundary, not a mystery `undefined` later. See [typed fetch and API client](../16-type-safe-apis/05-typed-fetch-and-api-client.md).

## 2. Express request validation middleware

Validate `body`, `query`, and `params` before the handler runs, and hand the **parsed** values to it.

```ts
import type { Request, Response, NextFunction } from "express";
import { z } from "zod";

type Source = "body" | "query" | "params";

export const validate =
  <S extends z.ZodType>(source: Source, schema: S) =>
  (req: Request, res: Response, next: NextFunction) => {
    const result = schema.safeParse(req[source]);
    if (!result.success) {
      return res.status(400).json({
        error: {
          code: "VALIDATION",
          issues: result.error.issues.map((i) => ({
            path: i.path.join("."),
            message: i.message,
          })),
        },
      });
    }
    res.locals[source] = result.data;   // parsed, typed output
    next();
  };
```

Using it:

```ts
const CreatePost = z.object({ title: z.string().min(1).max(200), body: z.string() });

app.post("/posts", validate("body", CreatePost), (req, res) => {
  const data = res.locals.body as z.output<typeof CreatePost>;
  // data.title is string
});
```

Notes:

- The result is stored on `res.locals` rather than overwriting `req.query`, because in newer Express versions `req.query` is a read-only getter. Check your Express version.
- `res.locals` is typed loosely, so the handler needs one assertion. The assertion is safe because the middleware just validated the value. To avoid it, validate inside the handler, or write a small typed helper that parses and returns the data. See [middleware](../20-nodejs-backend/03-middleware.md) and [Express](../20-nodejs-backend/02-express.md).
- Return a stable error shape and never expose Zod internals ([error response types](../16-type-safe-apis/04-error-response-types.md)).

## 3. Environment configuration at startup

```ts
// config.ts
import { z } from "zod";

const EnvSchema = z.object({
  NODE_ENV: z.enum(["development", "production", "test"]).default("development"),
  PORT: z.coerce.number().int().positive().default(3000),
  DATABASE_URL: z.string().url(),
  LOG_LEVEL: z.enum(["debug", "info", "warn", "error"]).default("info"),
});

const parsed = EnvSchema.safeParse(process.env);

if (!parsed.success) {
  console.error("Invalid environment configuration:");
  for (const issue of parsed.error.issues) {
    console.error(`  ${issue.path.join(".")}: ${issue.message}`);
  }
  process.exit(1);
}

export const config = parsed.data;
```

- Environment variables are strings or `undefined`, so numbers need `coerce`.
- The program refuses to start with a bad configuration and names every problem at once.
- Import `config` everywhere instead of reading `process.env` directly, so the rest of the code sees typed values ([config and environment](../20-nodejs-backend/01-config-and-environment.md)).
- The error output lists variable names and problems, not their values, so secrets are not logged.

## 4. Query strings and `FormData` (everything is a string)

```ts
const SearchParams = z.object({
  q: z.string().trim().min(1).optional(),
  page: z.coerce.number().int().min(1).default(1),
  pageSize: z.coerce.number().int().min(1).max(100).default(20),
  sort: z.enum(["newest", "oldest"]).default("newest"),
});

type Search = z.output<typeof SearchParams>;

const raw = Object.fromEntries(new URLSearchParams(window.location.search));
const search = SearchParams.parse(raw);
```

For `FormData`:

```ts
const raw = Object.fromEntries(formData);   // values are string | File
const data = ProfileSchema.safeParse(raw);
```

Reminders: repeated keys (`?tag=a&tag=b`) need explicit handling (`getAll`), and booleans from forms (checkboxes) arrive as `"on"` or are absent, so do not use `z.coerce.boolean()` for them.

## 5. Safely reading `localStorage`

Stored data can be from an older version of your app, corrupted, or edited by the user. Parse it, and fall back to defaults.

```ts
const SettingsSchema = z.object({
  theme: z.enum(["light", "dark"]).default("light"),
  fontSize: z.number().int().min(8).max(32).default(14),
});

type Settings = z.output<typeof SettingsSchema>;

function loadSettings(): Settings {
  try {
    const raw = localStorage.getItem("settings");
    if (raw === null) return SettingsSchema.parse({});          // all defaults
    const result = SettingsSchema.safeParse(JSON.parse(raw));
    return result.success ? result.data : SettingsSchema.parse({});
  } catch {
    return SettingsSchema.parse({});                             // JSON.parse failed
  }
}
```

Three layers of failure are handled: the key is missing, the JSON is invalid, and the JSON has the wrong shape. Every path returns a valid `Settings`. For persisted data that changes over time, include a `version` field and migrate old versions explicitly.

## 6. Webhook payloads (discriminated union)

```ts
const WebhookEvent = z.discriminatedUnion("type", [
  z.object({
    type: z.literal("invoice.paid"),
    invoiceId: z.string(),
    amountCents: z.number().int(),
  }),
  z.object({
    type: z.literal("customer.deleted"),
    customerId: z.string(),
  }),
]);

type WebhookEvent = z.output<typeof WebhookEvent>;

function handle(event: WebhookEvent) {
  switch (event.type) {
    case "invoice.paid":
      return markPaid(event.invoiceId, event.amountCents);
    case "customer.deleted":
      return removeCustomer(event.customerId);
    default: {
      const _exhaustive: never = event;
      return _exhaustive;
    }
  }
}
```

- Branch on `event.type` and each case sees only its own fields.
- The `never` check makes adding a new event type a compile error until it is handled ([exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)).
- **Verify the webhook's signature before parsing** the payload. Validation checks shape, not authenticity.
- Decide what to do with unknown event types: reject, or acknowledge and ignore, depending on the provider's contract.

## 7. A Result-returning parse helper

Throwing in the middle of business code is awkward. Convert the schema's output to your own `Result` once and use it everywhere.

```ts
type Result<T, E> = { ok: true; value: T } | { ok: false; error: E };

type ValidationIssue = { path: string; message: string };

export function parse<S extends z.ZodType>(
  schema: S,
  input: unknown,
): Result<z.output<S>, ValidationIssue[]> {
  const r = schema.safeParse(input);
  if (r.success) return { ok: true, value: r.data };
  return {
    ok: false,
    error: r.error.issues.map((i) => ({ path: i.path.join("."), message: i.message })),
  };
}

const res = parse(CreatePost, req.body);
if (!res.ok) return reply400(res.error);
save(res.value);
```

This keeps the library behind a boundary. Application code sees only `Result` and `ValidationIssue`, so changing libraries or Zod versions is a one-file change ([the Result pattern](../11-error-handling/02-result-pattern.md)).

## 8. Preventing mass assignment

Never spread a request body into a database update:

```ts
// BAD: a client can send { "role": "admin" }
await db.user.update({ where: { id }, data: req.body });
```

Define exactly what clients may change, and use the **parsed output** (which strips unknown keys):

```ts
const UpdateProfile = z.object({
  name: z.string().min(1).max(100).optional(),
  bio: z.string().max(500).optional(),
});

const result = UpdateProfile.safeParse(req.body);
if (!result.success) return reply400(result.error);

await db.user.update({ where: { id }, data: result.data });   // only name and bio can pass through
```

If you derive the update schema from a larger one, use `pick` (an allow-list) rather than `omit` (a deny-list), so new sensitive fields are not exposed by default ([Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md)).

## 9. Validating at the data-access layer

Typed ORMs already return typed rows, but validation still helps for raw SQL, JSON columns, caches, and data written by other systems:

```ts
const MetadataSchema = z.object({
  source: z.enum(["web", "mobile", "import"]),
  tags: z.array(z.string()).default([]),
});

async function getDocument(id: string) {
  const row = await db.query("select id, metadata from documents where id = $1", [id]);
  return {
    id: row.id as string,
    metadata: MetadataSchema.parse(row.metadata),   // JSON column: untrusted shape
  };
}
```

Validate where the loosely typed data enters. Keep the rest of the code working with the parsed type ([repository pattern](../17-design-patterns/04-repository.md)).

## 10. Testing schemas

Schemas are code with edge cases. Test them with real examples of valid and invalid input.

```ts
import { describe, it, expect } from "vitest";

describe("CreatePost", () => {
  it("accepts a valid post", () => {
    expect(CreatePost.safeParse({ title: "Hello", body: "..." }).success).toBe(true);
  });

  it("rejects an empty title", () => {
    const r = CreatePost.safeParse({ title: "", body: "..." });
    expect(r.success).toBe(false);
  });

  it("strips unknown keys", () => {
    const r = CreatePost.parse({ title: "Hi", body: "x", role: "admin" });
    expect(r).not.toHaveProperty("role");
  });
});
```

- Include the payloads that have actually broken in production as permanent test cases.
- Test boundaries: empty, maximum length, zero, negative, missing, `null`, wrong type.
- Test the **output**, not just the success flag, when transforms, defaults, or stripping matter.
- A suite like this also makes upgrading Zod or switching libraries safe ([unit testing](../18-testing-and-debugging/00-unit-testing.md)).

## Common mistakes

- Using `schema.parse` in a handler and letting `ZodError` become a 500.
- Passing `req.body` onward after validating, instead of the parsed result.
- Validating in one route and forgetting another.
- Using `z.coerce.boolean()` for form checkboxes and string flags.
- Building schemas inside the handler on every request.
- Logging the full invalid payload (which may contain personal data) instead of the issue paths.
- Trusting webhooks after validating shape but without verifying their signature.
- Spreading validated-but-unfiltered input into database updates.
- Skipping a size limit on the request body before parsing.

## Debugging

- **Log issues,** not payloads: path and message for each failure.
- **Reproduce with a saved real payload** in a test, then fix the schema or the sender.
- **Check which type you are using:** the schema's input, the parsed output, or the original unvalidated value.
- **If valid data is rejected,** look at coercion (`""`, `"false"`), optional versus nullable, and unknown-key policy.
- **If invalid data gets through,** look for a missing validation call, a use of the original object, `z.any()`, or `passthrough`/`loose` objects.
- **If the editor is slow** when hovering schema-derived types, split large schemas into named pieces.

## Quick summary

- Wrap external calls in a `fetchJson(url, schema)` that returns validated types.
- Validate HTTP input in middleware or at the top of handlers, and pass **parsed** values onward.
- Parse environment variables once at startup and crash with a clear message if they are wrong.
- Coerce strings from query strings and forms deliberately. Parse booleans explicitly.
- Treat `localStorage`, JSON columns, webhooks, and caches as untrusted, with fallbacks for bad data.
- Allow-list updatable fields to prevent mass assignment.
- Hide the library behind a `Result`-returning helper, and test schemas with real valid and invalid payloads.

**Next:** [16 Type-Safe APIs](../16-type-safe-apis/README.md)