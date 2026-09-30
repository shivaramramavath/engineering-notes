# Validation

Checking that every piece of data entering your API has the right shape, types, and values — before any business logic runs.

## Why validate on the server

Client-side validation is a convenience for users. **Server-side validation is a security and correctness boundary.** Anyone can bypass your front-end with `curl`, so the server must assume every request is hostile or broken.

Validation protects against:

- **Injection** — objects where strings are expected (`08-authentication-security/05-common-vulnerabilities.md`)
- **Mass assignment** — clients setting fields like `role: "admin"`
- **Crashes** — `undefined.toLowerCase()` turning into a `500`
- **Corrupt data** — a negative quantity, a 10 MB username, a date of `"banana"`
- **Resource abuse** — huge arrays, long strings, deeply nested JSON

### Validation vs sanitization vs authorization

| | Question answered | Example |
|---|---|---|
| **Validation** | Is this input acceptable? | Email looks like an email; age is an integer ≥ 0 |
| **Sanitization / normalization** | Can I clean it up? | Trim whitespace, lowercase the email |
| **Business rules** | Is it allowed in the current state? | Email not already registered; stock available |
| **Authorization** | Is this user permitted? | Only the owner can edit |

Schemas handle the first two. The last two need database lookups and live in your service layer.

---

## Where input comes from

Validate **all** of these, not just the body:

| Source | Express | Notes |
|---|---|---|
| Body | `req.body` | JSON (needs `express.json()`) or form data |
| Route params | `req.params` | Always strings |
| Query string | `req.query` | Always strings / arrays of strings |
| Headers | `req.headers` | `Content-Type`, `Idempotency-Key`, custom headers |
| Cookies | `req.cookies` | Signed or not, still untrusted |
| Uploaded files | `req.file` | Type, size, name — see `06-express/07-file-upload.md` |

---

## Choosing a library

| Library | Notes |
|---|---|
| **zod** | TypeScript-first, composable, great error messages, infers types. Used in this course. |
| **joi** | Mature, expressive, popular in older Express codebases |
| **yup** | Similar to Joi, common in React form libraries |
| **ajv** | JSON Schema validator, extremely fast; pairs with OpenAPI |
| **express-validator** | Validation chains as middleware; wraps `validator.js` |
| **valibot** | Smaller bundle alternative to zod |

The concepts are identical across libraries. Zod is used below.

```bash
npm install zod
```

---

## Zod essentials

```js
import { z } from "zod";

// primitives with constraints
const name = z.string().trim().min(1).max(100);
const age = z.number().int().min(0).max(150);
const email = z.string().trim().toLowerCase().email();
const id = z.string().uuid();
const role = z.enum(["user", "editor", "admin"]);
const tags = z.array(z.string().min(1).max(30)).max(10);

// objects
const createPostSchema = z.object({
  title: z.string().trim().min(3).max(200),
  body: z.string().min(1).max(50_000),
  tags: z.array(z.string().min(1).max(30)).max(10).default([]),
  published: z.boolean().default(false),
});

// parse: throws ZodError if invalid, returns typed & cleaned data if valid
const data = createPostSchema.parse(req.body);

// safeParse: never throws, returns { success, data } or { success, error }
const result = createPostSchema.safeParse(req.body);
if (!result.success) {
  console.log(result.error.issues);
}
```

### What `parse` does for you

```js
createPostSchema.parse({
  title: "  Hello  ",
  body: "World",
  hacker: "extra field",          // unknown key — stripped by default
});
// → { title: "Hello", body: "World", tags: [], published: false }
```

- **Trims and normalizes** (`trim`, `toLowerCase`)
- **Applies defaults**
- **Strips unknown keys** (use `.strict()` to reject them instead)
- **Throws** with structured issues on failure

---

## Query and param strings need coercion

Everything in `req.query` and `req.params` is a string, so use `z.coerce`:

```js
const listQuery = z.object({
  page: z.coerce.number().int().min(1).default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
  published: z.enum(["true", "false"]).transform((v) => v === "true").optional(),
  createdAfter: z.coerce.date().optional(),
});
```

### Booleans are a classic trap

```js
z.coerce.boolean().parse("false");   // true!  (coerce uses Boolean("false") → true)
```

Any non-empty string is truthy, so `?published=false` becomes `true`. Use an explicit enum plus transform, as above.

### Params

```js
const idParams = z.object({ id: z.string().uuid() });    // or z.coerce.number().int().positive()
```

A malformed ID rejected as `400` is better than a database error or a confusing `404`. For MongoDB ObjectIds:

```js
const objectId = z.string().regex(/^[a-f\d]{24}$/i, "Invalid id");
```

---

## A reusable validation middleware

Write the wiring once; every route just declares its schemas.

```js
// middleware/validate.js
export const validate = (schemas) => (req, res, next) => {
  try {
    if (schemas.params) req.params = schemas.params.parse(req.params);
    if (schemas.query)  req.validatedQuery = schemas.query.parse(req.query);
    if (schemas.body)   req.body = schemas.body.parse(req.body);
    next();
  } catch (err) {
    next(err);            // ZodError is translated by the central error handler (05-error-responses.md)
  }
};
```

> Express 5 made `req.query` a read-only getter, which is why validated query values go on a new property (`req.validatedQuery`) rather than overwriting `req.query`. Assigning to `req.params` and `req.body` works normally.

Use it in routes:

```js
// routes/posts.js
import { validate } from "../middleware/validate.js";
import { createPostSchema, updatePostSchema, idParams, listQuery } from "../schemas/posts.js";

router.get("/",     validate({ query: listQuery }), posts.list);
router.post("/",    validate({ body: createPostSchema }), posts.create);
router.get("/:id",  validate({ params: idParams }), posts.get);
router.patch("/:id", validate({ params: idParams, body: updatePostSchema }), posts.update);
```

Now the controller receives clean data and can assume it's valid:

```js
export async function create(req, res) {
  // req.body is already validated, trimmed, typed, defaults applied
  const post = await postService.create(req.user.id, req.body);
  res.status(201).json({ data: toPostDto(post) });
}
```

---

## Schemas for create vs update

Derive schemas from one another instead of duplicating them:

```js
// schemas/posts.js
export const createPostSchema = z.object({
  title: z.string().trim().min(3).max(200),
  body: z.string().min(1).max(50_000),
  published: z.boolean().default(false),
}).strict();                                          // reject unknown fields

// PATCH: every field optional, but at least one must be present
export const updatePostSchema = createPostSchema
  .partial()
  .refine((obj) => Object.keys(obj).length > 0, {
    message: "Provide at least one field to update",
  });
```

Note the `default(false)` in `createPostSchema`: with `.partial()` in an update, make sure defaults don't silently overwrite existing values. Safest is to build the update schema without defaults.

---

## Cross-field and custom rules

```js
const registerSchema = z
  .object({
    email: z.string().email(),
    password: z.string().min(8).max(128),
    confirmPassword: z.string(),
  })
  .refine((d) => d.password === d.confirmPassword, {
    path: ["confirmPassword"],                      // where the error is attached
    message: "Passwords do not match",
  });

const dateRange = z
  .object({ from: z.coerce.date(), to: z.coerce.date() })
  .refine((d) => d.from <= d.to, { message: "'from' must be before 'to'", path: ["to"] });
```

For rules that need the database (is this email taken?), don't put async lookups in the schema — check in the service layer and throw a `409 Conflict`, which also avoids race conditions. The real guarantee is a **unique index**; validation is just the friendly first line.

---

## Validating more than the JSON body

### Content-Type and body size

```js
app.use(express.json({ limit: "100kb" }));           // 413 if larger

// Reject non-JSON bodies on write methods
app.use((req, res, next) => {
  if (["POST", "PUT", "PATCH"].includes(req.method) && req.headers["content-length"] !== "0") {
    if (!req.is("application/json")) {
      return res.status(415).json({ error: { code: "unsupported_media_type", message: "Use application/json" } });
    }
  }
  next();
});
```

### Headers

```js
const idempotencyHeader = z.string().min(16).max(128);   // see 06-idempotency.md
idempotencyHeader.parse(req.get("Idempotency-Key"));
```

### Malformed JSON

`express.json()` throws a `SyntaxError` with `type: "entity.parse.failed"` on invalid JSON. Translate it to a clean `400` in the error handler (see `05-error-responses.md`) instead of letting it fall through as a `500`.

---

## Limits: validate the *size* of things

Attackers don't need a type error to hurt you — a valid-but-enormous payload works too.

| Limit | Example |
|---|---|
| String length | `z.string().max(10_000)` |
| Array length | `z.array(...).max(100)` |
| Nesting depth | Avoid recursive schemas on untrusted input, or cap depth |
| Body size | `express.json({ limit: "100kb" })` |
| Number range | `z.number().min(0).max(1_000_000)` |
| Pagination `limit` | `max(100)` |
| Uploaded file size | `multer({ limits: { fileSize: 5 * 1024 * 1024 } })` |

Regexes on unvalidated long strings invite ReDoS, so set a `.max()` **before** a `.regex()` or a custom check.

---

## Error output

Zod errors contain a list of issues:

```js
const result = createPostSchema.safeParse({ title: "Hi", body: "" });

result.error.issues;
// [
//   { code: "too_small", minimum: 3, path: ["title"], message: "..." },
//   { code: "too_small", minimum: 1, path: ["body"],  message: "..." }
// ]
```

Convert them to your API's standard error format (defined in `05-error-responses.md`):

```jsonc
{
  "error": {
    "code": "validation_error",
    "message": "Request validation failed",
    "details": [
      { "field": "title", "message": "Must be at least 3 characters" },
      { "field": "body",  "message": "Required" }
    ]
  }
}
```

```js
function formatZodError(err) {
  return err.issues.map((i) => ({
    field: i.path.join("."),
    message: i.message,
  }));
}
```

Return **every** problem at once, not just the first, so a client can fix a whole form in one round trip. Don't echo back sensitive input values (passwords) in error messages.

---

## Testing validation

Validation is cheap to unit-test because schemas are plain objects:

```js
import { createPostSchema } from "../src/schemas/posts.js";

test("rejects a too-short title", () => {
  const r = createPostSchema.safeParse({ title: "Hi", body: "x" });
  expect(r.success).toBe(false);
});

test("strips nothing it shouldn't and rejects unknown fields", () => {
  const r = createPostSchema.safeParse({ title: "Hello", body: "x", role: "admin" });
  expect(r.success).toBe(false);          // .strict() in action
});
```

And test at the HTTP level too: wrong types, missing fields, oversized strings, extra fields (`13-testing/02-api-testing-and-mocking.md`).

---

## OpenAPI and shared schemas

Schemas can double as documentation and client types:

- `zod-to-openapi` (or similar) generates an OpenAPI spec from the same zod schemas you validate with — one source of truth.
- In TypeScript, `z.infer<typeof createPostSchema>` gives you the request type for free (`17-typescript/02-interfaces-and-generics.md`).

---

## Common mistakes

```js
// ❌ validating only on the client
// ❌ validating inside the controller after doing work
// ❌ trusting req.query/req.params types
const page = req.query.page + 1;        // "2" + 1 → "21"

// ❌ z.coerce.boolean() for query flags ("false" → true)
// ❌ passing req.body straight to the database after validating "some" fields
await Post.create(req.body);            // use the PARSED output, not the raw body

// ❌ no .max() on strings and arrays
// ❌ regex validation without a length cap first (ReDoS)
// ❌ returning only the first error
// ❌ inconsistent status codes for validation failures across endpoints
```

```js
// ✅ always use the parsed result
const data = createPostSchema.parse(req.body);
await Post.create({ ...data, authorId: req.user.id });
```

## Next

**`05-error-responses.md`** defines the error format that validation failures (and every other failure) are translated into, with a custom error class and a single central error handler.
