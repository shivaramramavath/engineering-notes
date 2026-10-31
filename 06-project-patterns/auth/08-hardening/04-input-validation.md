# Input Validation

Every value that arrives from outside (body, query string, params, headers, cookies, uploaded files, OAuth callbacks, even data from your own database written earlier by a user) is **untrusted**. Validation makes sure inputs have the expected type, shape and size **before** any logic runs. In authentication code it also closes whole attack classes: NoSQL injection, mass assignment, parser abuse, and CPU-exhaustion via giant passwords.

**Prerequisites:** [00-project-setup.md](../00-setup/00-project-setup.md), [01-password-policy.md](../02-passwords/01-password-policy.md)

---

## Validate vs sanitize vs escape

Three different jobs that people mix up:

| | Question | Example | Where |
|---|---|---|---|
| **Validate** | "Is this acceptable?" | Reject `email` that isn't a string ≤ 254 chars | At the edge, on input |
| **Normalize** | "Make equivalent inputs identical" | `trim().toLowerCase()` on emails | At the edge, after validating |
| **Escape/encode** | "Make this safe for the place it's going" | HTML-escape before rendering; parameterize queries | At the **output/sink** |

Validation is **not** an XSS or injection defense by itself. A "valid" name can still contain `<script>`; protect HTML with output encoding and a CSP ([03-cors-and-headers.md](./03-cors-and-headers.md)), and queries with parameterization or typed filters.

Prefer **allowlisting** (define what's allowed) over denylisting (try to list what's bad).

---

## Schema validation with zod

```bash
npm i zod
```

```js
import { z } from "zod";

export const registerSchema = z
  .object({
    email: z.string().trim().toLowerCase().email().max(254),
    password: z.string().min(12).max(128),                    // never trim/alter passwords
  })
  .strict();                                                  // unknown keys → error (blocks mass assignment)

export const loginSchema = z
  .object({
    email: z.string().trim().toLowerCase().max(254),          // no email-format check: don't leak format rules at login
    password: z.string().min(1).max(128),
  })
  .strict();

export const resetSchema = z
  .object({
    token: z.string().min(20).max(200),
    password: z.string().min(12).max(128),
  })
  .strict();

export const objectId = z.string().regex(/^[a-f\d]{24}$/i);
```

Notes:

- zod's **default** behavior strips unknown keys silently. `.strict()` rejects them instead, which surfaces client bugs and attack attempts. Either is safe against mass assignment as long as you use the **parsed output**, not `req.body`.
- Always set **maximum lengths**. Without `max(128)` on passwords, an attacker posts a megabyte "password" to burn Argon2 CPU. Also set body size limits (below).
- In zod 4 the top-level helpers like `z.email()` are preferred and `z.string().email()` is deprecated but works; check the docs for your installed major version.
- **Email validation only checks shape.** It can't prove the address exists or belongs to the user; that's what [email verification](../03-tokens/08-email-verification-tokens.md) is for. Don't write giant email regexes (they're a ReDoS risk and reject valid addresses).
- Normalize identifiers (`trim`, `toLowerCase`) **in the schema**, so every consumer sees the same value: rate-limit keys, lockout keys and DB lookups all depend on it ([00-rate-limiting.md](./00-rate-limiting.md)).

---

## Validation middleware

```js
export const validate = (schemas) => (req, res, next) => {
  const out = {};
  for (const part of ["body", "query", "params"]) {
    if (!schemas[part]) continue;
    const result = schemas[part].safeParse(req[part]);
    if (!result.success) {
      return res.status(400).json({
        message: "Invalid request",
        errors: result.error.issues.map((i) => ({ path: i.path.join("."), message: i.message })),
      });
    }
    out[part] = result.data;
  }
  req.valid = out;                      // use req.valid.body etc. in controllers
  next();
};

router.post("/register", validate({ body: registerSchema }), register);
router.get("/sessions/:id", authenticate, validate({ params: z.object({ id: z.string().min(20).max(100) }) }), getSession);
```

- **Use the parsed data** (`req.valid.body`), never the raw `req.body`. In Express 5, `req.query` is a read-only getter, so you can't overwrite it with the parsed result; storing results on a separate property avoids that problem and works on Express 4 too.
- Return field-level errors for ordinary forms. For **auth endpoints** (login, forgot-password, reset), keep errors generic where specifics could leak account state ([01-enumeration-protection.md](./01-enumeration-protection.md)). A "password too short" message is fine at registration/reset; it isn't at login.
- Run validation **after `authenticate`** on protected routes, so anonymous callers can't probe your schemas.

---

## NoSQL injection (MongoDB)

Express parses JSON bodies into objects, so this valid request:

```json
{ "email": { "$gt": "" }, "password": { "$gt": "" } }
```

becomes a query that matches the first user:

```js
// ❌ vulnerable
const user = await User.findOne({ email: req.body.email });     // email is an object: { $gt: "" }
```

Defenses, in layers:

1. **Validate types**: `z.string()` rejects objects, so the attack never reaches the query.
2. **Mongoose `sanitizeFilter`**: wraps `$`-operators found in user-supplied filter values in `$eq`, neutralizing them:

```js
mongoose.set("sanitizeFilter", true);
```

3. Explicit coercion at the boundary: `String(x)`, and never spread user objects into queries (`find({ ...req.query })`).
4. Don't build queries from keys the user chooses (`find({ [req.query.field]: ... })`).

Also beware `$where`/server-side JS, and `req.query` objects produced by `qs` parsing (`?email[$gt]=`): the same defenses apply.

---

## Mass assignment

```js
// ❌ user controls every field, including role and isEmailVerified
await User.create(req.body);
await User.findByIdAndUpdate(id, req.body);

// ✅ construct from validated, explicitly-listed fields
const { email, password } = req.valid.body;
await User.create({ email, passwordHash: await hash(password) });
```

`.strict()` schemas + using only parsed output + per-actor writable-field lists ([01-permissions-and-policies.md](../07-authorization/01-permissions-and-policies.md)) shut this down.

---

## Request-level limits

```js
app.use(express.json({ limit: "10kb" }));                         // reject huge bodies early
app.use(express.urlencoded({ extended: false, limit: "10kb" }));
```

| Limit | Why |
|---|---|
| Body size | Memory/CPU exhaustion (and Argon2 DoS) |
| String max lengths | Same, per field |
| Array max length (`z.array(...).max(50)`) | Loops over attacker-sized arrays |
| Nesting depth | Deeply nested JSON stresses parsers and recursive code |
| Number ranges (`z.number().int().min(1).max(100)`) | Pagination abuse (`limit=1e9`), overflow |
| Timeouts | Slow-request (slowloris) handling at the server/proxy |
| Upload size/type | See below |

Check `Content-Type`: JSON endpoints should require `application/json` and reject others ([05-csrf.md](../04-sessions/05-csrf.md)).

---

## HTTP parameter pollution and type confusion

`?role=user&role=admin` makes `req.query.role` an **array**. A schema that expects a string rejects it; code that assumes a string may misbehave (e.g. `.toLowerCase()` throwing, or an array coerced to `"user,admin"`).

```js
const pagination = z.object({
  page: z.coerce.number().int().min(1).max(10_000).default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
});
```

`z.coerce.number()` is right for query strings (all strings). Be careful with `z.coerce.boolean()`: it treats any non-empty string, including `"false"`, as `true`. Parse explicit values instead (`z.enum(["true","false"]).transform(v => v === "true")`).

---

## Prototype pollution and unsafe merging

Keys like `__proto__`, `constructor` and `prototype` in untrusted JSON can poison object prototypes when merged recursively.

- Don't deep-merge or `Object.assign` raw request data into config/state objects.
- Use validated, flat output from the schema.
- Keep dependencies (`lodash.merge`, `qs`, etc.) updated; many past CVEs were prototype pollution.

---

## ReDoS (regex denial of service)

A regex with nested quantifiers (`(a+)+$`) can take exponential time on crafted input, freezing a single-threaded Node process.

- Avoid hand-rolled complex regexes for emails, URLs, etc.; use library validators.
- Bound input length **before** running any regex (another reason for `.max()`).
- Test suspicious patterns with a ReDoS checker, or use linear-time engines (`re2`) for user-supplied patterns.

---

## Other inputs

| Input | Rule |
|---|---|
| **Redirect targets** (`returnTo`, `next`) | Allowlist relative paths ([05-oauth-security.md](../06-oauth/05-oauth-security.md)) |
| **URLs the server fetches** (webhooks, avatars) | Allowlist hosts, block private/link-local ranges (SSRF) |
| **Headers** (`X-Forwarded-For`, `Host`, `User-Agent`) | Untrusted; don't build links from `Host` ([09-reset-tokens.md](../03-tokens/09-reset-tokens.md)); truncate before logging/storing |
| **Cookies** | Validate shape (token format) before use; treat as input |
| **Uploads** | Limit size, validate by content (magic bytes) not just extension/MIME, store outside the web root, re-encode images, random filenames |
| **IDs** | Validate format before DB queries (`isValidObjectId`) |
| **Webhook payloads** | Verify signatures first, then validate shape |
| **Data from the DB or other services** | Validate at trust boundaries too |

---

## Sharing schemas with the frontend

Using the same zod schemas in the React app gives instant form feedback and avoids drift. But:

> Client-side validation is **UX**. Server-side validation is **security**. The server must validate everything regardless.

Never skip server validation because "the form already checks it". Attackers don't use your form.

---

## joi and other options

| Library | Notes |
|---|---|
| **zod** | TypeScript-first, composable, works great in JS too; used above |
| **joi** | Mature, expressive, runtime-only (`Joi.object({...}).validate(body, { stripUnknown: true })`) |
| **express-validator** | Chain-style middleware wrapping `validator.js` |
| **ajv** + JSON Schema | Fast, standards-based, good if you share schemas across languages/OpenAPI |
| Mongoose schema validation | Last line of defense at the DB layer; doesn't replace request validation |

Choose one and use it consistently. The principles above apply to all of them.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Using `req.body` directly | NoSQL injection, mass assignment |
| No max lengths | CPU/memory DoS (Argon2, regex, JSON) |
| Trimming or lower-casing passwords | Changes what users typed; can lock them out |
| Normalizing email in some places but not others | Duplicate accounts, bypassed rate limits |
| Validating on the client only | Zero protection |
| Detailed validation errors at login/reset | Enumeration and probing |
| Treating validation as XSS/SQL/NoSQL protection | Wrong layer; encode and parameterize at sinks |
| `z.coerce.boolean()` on `"false"` | Evaluates `true` |
| Overwriting `req.query` in Express 5 | Silent no-op or error: store parsed data elsewhere |
| Giant custom regexes | ReDoS |
| Inconsistent validation between routes | The forgotten route is the weak one |

## Debugging

| Symptom | Cause |
|---|---|
| Valid requests get `400` | Strict schema rejects extra fields sent by the frontend; check `errors[].path` |
| `req.body` is `undefined` | `express.json()` missing, or wrong `Content-Type` |
| Numbers arrive as strings in query | Missing `z.coerce.number()` |
| Login works with `{ "$gt": "" }` in testing | No type validation and `sanitizeFilter` disabled |
| `ZodError` crashes with 500 | Used `.parse` without catching; use `safeParse` or an error handler mapping `ZodError` → `400` |
| Validation passes but DB stores something odd | Controller used `req.body`, not `req.valid.body` |

---

## Quick summary

- Validate every input at the edge against an allowlist schema (type, shape, **max sizes**), and use only the parsed output.
- `.strict()` schemas and explicit field lists stop mass assignment; typed strings plus `sanitizeFilter` stop NoSQL injection.
- Normalize emails in the schema; never alter passwords.
- Limit bodies, arrays, depth and numbers; avoid complex regexes.
- Validation ≠ output encoding; client-side validation ≠ security.

**Next:** [05-logging-and-audit.md](./05-logging-and-audit.md)
