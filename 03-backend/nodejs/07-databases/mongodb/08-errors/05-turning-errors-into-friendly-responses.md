# Turning Errors into Friendly Responses

The complete pattern: catching Mongoose's raw errors and mapping them to clean, meaningful API responses — specifically the "email already exists" case, generalized into a reusable centralized error handler.

## The naive approach, and why it falls short

```js
app.post("/users", async (req, res) => {
  try {
    const user = await User.create(req.body);
    res.status(201).json(user);
  } catch (err) {
    res.status(500).json({ error: "Something went wrong" }); // ❌ loses all useful information
  }
});
```

Every failure — a missing required field, a malformed email, a duplicate email, a genuine database outage — gets the exact same unhelpful `500` response. None of the detail covered in the previous files of this section (`ValidationError`'s per-field messages, the duplicate-key field identification) reaches the client at all.

---

## Step 1: a per-route mapping, to see the full picture

```js
app.post("/users", async (req, res, next) => {
  try {
    const user = await User.create(req.body);
    res.status(201).json(user);
  } catch (err) {
    if (err.name === "ValidationError") {
      const errors = Object.fromEntries(
        Object.entries(err.errors).map(([field, e]) => [field, e.message]),
      );
      return res
        .status(400)
        .json({ error: "Validation failed", details: errors });
    }

    if (err.code === 11000) {
      const field = Object.keys(err.keyValue)[0];
      return res.status(409).json({ error: `${field} already in use` });
    }

    next(err); // anything else — let the centralized handler deal with it
  }
});
```

```json
// duplicate email
{ "error": "email already in use" }
```

```json
// validation failure
{
  "error": "Validation failed",
  "details": { "email": "Please enter a valid email" }
}
```

This works, but repeating this same `if (err.name === "ValidationError") ... if (err.code === 11000) ...` block in every single route that touches the database is exactly the kind of repetition centralized error-handling middleware exists to eliminate.

---

## Step 2: centralize it into Express error-handling middleware

```js
// middleware/errorHandler.js
export function errorHandler(err, req, res, next) {
  // ValidationError
  if (err.name === "ValidationError") {
    const details = Object.fromEntries(
      Object.entries(err.errors).map(([field, e]) => [field, e.message]),
    );
    return res.status(400).json({ error: "Validation failed", details });
  }

  // CastError
  if (err.name === "CastError") {
    return res
      .status(400)
      .json({ error: `Invalid value for field "${err.path}"` });
  }

  // Duplicate key
  if (err.code === 11000) {
    const field = Object.keys(err.keyValue)[0];
    return res.status(409).json({ error: `${field} already in use` });
  }

  // Anything unrecognized — a genuine bug or unexpected failure
  console.error("Unhandled error:", err);
  res.status(500).json({ error: "Internal server error" });
}
```

```js
// app.js
import { errorHandler } from "./middleware/errorHandler.js";

app.use("/api", routes);
app.use(errorHandler); // registered LAST — see 06-express/04-error-handling.md for why this matters
```

Now every route just needs to `next(err)` (or let an async wrapper do it automatically, per `06-express/02-middleware.md`'s `asyncHandler` pattern), and this one file translates every Mongoose error type into the right response, consistently, everywhere in the app.

```js
app.post("/users", async (req, res, next) => {
  try {
    const user = await User.create(req.body);
    res.status(201).json(user);
  } catch (err) {
    next(err); // that's it — the centralized handler does the rest
  }
});
```

---

## Why `409 Conflict` for a duplicate, not `400 Bad Request`

```js
if (err.code === 11000) {
  return res.status(409).json({ error: `${field} already in use` });
}
```

Per `05-http-web/01-http-methods-and-status-codes.md`'s status code guidance: `409 Conflict` specifically communicates "this request is valid, but conflicts with the current state of the server" — which is exactly what a duplicate email is: the request was well-formed, it just can't be fulfilled because that email is already taken. `400` is more appropriate for genuinely malformed input (a missing field, invalid format), which is a different situation entirely, even though both feel like "the request failed" from a glance.

---

## A more specific duplicate-field message

```js
const FIELD_MESSAGES = {
  email: "This email is already registered",
  username: "This username is already taken",
};

if (err.code === 11000) {
  const field = Object.keys(err.keyValue)[0];
  const message = FIELD_MESSAGES[field] || `${field} already in use`;
  return res.status(409).json({ error: message, field });
}
```

A small lookup table turns the generic "field already in use" into genuinely user-facing copy, tailored per field — worth doing for any field where a specific, friendly message matters (an email/username in a signup flow is the classic case).

---

## Handling the same pattern outside Express (e.g. in a service function)

```js
// services/userService.js — no Express, no req/res, just business logic
export async function registerUser(name, email, password) {
  try {
    return await User.create({ name, email, password });
  } catch (err) {
    if (err.code === 11000) {
      throw new ConflictError("Email already registered"); // your OWN error class — see 06-custom-application-errors.md
    }
    throw err;
  }
}
```

For code outside a direct Express route handler (a service layer, `11-patterns-and-architecture/01-repository-and-service-pattern.md`), the same catch-and-translate idea applies, but translating into your **own** application error classes rather than directly building an HTTP response — keeping the service layer free of HTTP-specific concerns, and letting the Express layer (or a controller) translate _that_ error into a response. Full depth on this in `06-custom-application-errors.md`, the next file.

---

## A complete, realistic signup flow

```js
app.post("/signup", async (req, res, next) => {
  try {
    const user = await User.create({
      name: req.body.name,
      email: req.body.email,
      password: await hashPassword(req.body.password),
    });
    res.status(201).json({ id: user.id, name: user.name, email: user.email });
  } catch (err) {
    next(err);
  }
});
```

```js
// centralized handler catches ValidationError (missing/malformed fields),
// CastError (malformed input types), and code 11000 (duplicate email) —
// each mapped to the right status code and message, automatically, every time
```

## Common mistakes

- **Repeating the same error-mapping logic in every route** instead of centralizing it in error-handling middleware — violates DRY and risks inconsistent handling across different parts of the app.
- **Using `500` for a duplicate-key error** — it's a client-facing, expected situation (`409`), not a server failure.
- **Logging every error identically, including expected ones like duplicate keys** — reserve loud logging (`console.error`, alerting) for the genuinely unexpected `500` case; a duplicate email during signup isn't a bug worth an alert.
- **Forgetting to call `next(err)` in an async route handler** — per `06-express/04-error-handling.md`, on Express 4 this means the error never reaches the centralized handler at all.

## Quick summary

- Map each Mongoose error type to the right HTTP status and message in **one** centralized place — Express error-handling middleware — rather than repeating the logic per route
- `409 Conflict` (not `400`) is the semantically correct status for a duplicate-key violation
- `err.keyValue` (from `04-duplicate-key-errors.md`) is what lets you build a field-specific, friendly message like "email already in use"
- Outside Express (a service layer), the same catch-and-translate idea applies, but translates into your own application error classes instead of directly into an HTTP response

## Next

**`06-custom-application-errors.md`** covers wrapping Mongoose's errors in your own error classes, so the rest of your codebase doesn't need to know Mongoose-specific error shapes at all.
