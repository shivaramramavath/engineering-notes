# Validation

Checking that incoming request data is well-formed and meets your app's rules _before_ it reaches business logic — catching bad input at the door rather than letting it cause confusing failures deeper in the code.

## Why not just check manually?

```js
// works, but doesn't scale — imagine this for a 15-field request body
app.post("/users", (req, res) => {
  const { name, email, age } = req.body;

  if (!name || typeof name !== "string") {
    return res
      .status(400)
      .json({ error: "name is required and must be a string" });
  }
  if (!email || !email.includes("@")) {
    return res
      .status(400)
      .json({ error: "email must be a valid email address" });
  }
  if (age !== undefined && (typeof age !== "number" || age < 0)) {
    return res.status(400).json({ error: "age must be a positive number" });
  }

  // ... actual logic, finally
});
```

This gets unwieldy fast, is easy to get subtly wrong (missing an edge case), and duplicates similar checks across many routes. A validation library expresses the same rules declaratively, once, and applies them consistently.

---

## Zod — schema-based validation

```bash
npm install zod
```

```js
import { z } from "zod";

const createUserSchema = z.object({
  name: z.string().min(1),
  email: z.string().email(),
  age: z.number().int().positive().optional(),
});

app.post("/users", (req, res, next) => {
  const result = createUserSchema.safeParse(req.body);

  if (!result.success) {
    return res.status(400).json({ error: result.error.flatten() });
  }

  const { name, email, age } = result.data; // fully typed AND validated
  // ...
});
```

`safeParse` returns a result object rather than throwing — `result.success` tells you whether validation passed, and `result.data` is the parsed, type-safe value on success. Zod also integrates naturally with TypeScript (`17-typescript/`), inferring a static type directly from the schema.

### As reusable middleware

```js
function validate(schema) {
  return (req, res, next) => {
    const result = schema.safeParse(req.body);
    if (!result.success) {
      return res.status(400).json({ error: result.error.flatten() });
    }
    req.body = result.data; // replace with the parsed/validated version
    next();
  };
}

app.post("/users", validate(createUserSchema), (req, res) => {
  // req.body is now guaranteed valid here
  res.status(201).json(req.body);
});
```

Another closure-based configurable middleware (`03-javascript-for-node/03-closures.md`), the same pattern as `requireRole(role)` in `02-middleware.md` — `validate(schema)` returns a middleware function specific to that schema.

---

## Joi — an older, widely-used alternative

```bash
npm install joi
```

```js
import Joi from "joi";

const createUserSchema = Joi.object({
  name: Joi.string().min(1).required(),
  email: Joi.string().email().required(),
  age: Joi.number().integer().positive().optional(),
});

app.post("/users", (req, res, next) => {
  const { error, value } = createUserSchema.validate(req.body);
  if (error) {
    return res.status(400).json({ error: error.details[0].message });
  }
  req.body = value;
  next();
});
```

Functionally similar to Zod, predates it, and remains common in existing codebases. Zod has become the more popular default for new TypeScript-heavy projects due to its type inference, but Joi is still a completely reasonable, well-maintained choice.

---

## Validating more than just the body

```js
const paramsSchema = z.object({
  id: z.string().uuid(),
});

const querySchema = z.object({
  page: z.coerce.number().int().positive().default(1),
  limit: z.coerce.number().int().positive().max(100).default(20),
});

app.get("/users/:id", (req, res) => {
  const params = paramsSchema.parse(req.params);
  const query = querySchema.parse(req.query);
  // ...
});
```

`z.coerce.number()` is worth noting specifically: `req.query`/`req.params` values are always strings (`05-http-web/01-http-methods-and-status-codes.md`'s query string discussion), so coercing them into the expected type is a routine, necessary part of validating anything other than the request body.

---

## Validation vs sanitization

```js
const commentSchema = z.object({
  text: z.string().trim().max(1000),
});
```

- **Validation** — rejecting input that doesn't meet the rules (too long, wrong type, missing)
- **Sanitization** — transforming input into an acceptable/safe form (`.trim()` removing whitespace, stripping unexpected fields)

Both matter for security, not just correctness — unsanitized input flowing into a database query or rendered HTML is a large part of how injection and XSS vulnerabilities happen (`08-authentication-security/05-common-vulnerabilities.md`).

---

## Where validation fits in the request lifecycle

```
Request arrives
  ↓
express.json() — parse the body
  ↓
Validation middleware — reject if malformed/invalid
  ↓
Auth middleware — reject if unauthenticated/unauthorized
  ↓
Controller — business logic, now trusting the data's shape
```

Validating early means every layer after it can simply trust the data's shape is correct, rather than every function down the chain needing its own defensive checks.

## Common mistakes

- **Hand-rolling validation with scattered `if` checks** — works for a couple of fields, becomes unmaintainable and inconsistent across a real app.
- **Trusting `req.query`/`req.params` values' types without coercion** — they're always strings; comparing `req.query.page === 1` is always false without converting first.
- **Validating only the body, ignoring params/query** — a malformed `:id` or an out-of-range `?limit=999999` can cause just as many problems as a malformed body.
- **Confusing validation with sanitization** — rejecting bad input and cleaning up borderline-acceptable input are related but distinct concerns; often you need both.
- **Not returning validation errors in a consistent shape** — makes client-side error handling harder; standardize the error response format (`09-api-development/05-error-responses.md`).

## Quick summary

- A schema library (Zod or Joi) expresses validation rules declaratively, once, instead of scattered manual `if` checks
- Zod's `safeParse` (or Joi's `.validate()`) returns a result you check, rather than throwing by default
- `req.query`/`req.params` values are always strings — coerce them to the expected type as part of validation
- Validation and sanitization are related but distinct: rejecting bad input vs. cleaning up acceptable-but-messy input
- Validate as early as possible in the middleware chain, so everything downstream can trust the data's shape

## Next

**`06-auth-and-authorization.md`** covers the next layer typically applied after validation — confirming who's making the request, and what they're allowed to do.
