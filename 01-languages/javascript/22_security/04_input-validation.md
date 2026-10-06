# Input Validation

Every value that crosses a trust boundary (HTTP bodies, query strings, headers, file uploads, environment, message queues, even your own database when it holds user-supplied data) is **untrusted until proven otherwise**. Validation is how you prove it.

Good validation shrinks the attack surface for injection, prototype pollution, mass assignment, and resource exhaustion, and it gives users clear errors.

## Prerequisites

- [Type conversion and equality](../01_fundamentals/03_type-conversion-and-equality.md)
- [HTTP server basics](../16_nodejs/08_http-server.md)
- [Error handling](../10_error-handling/README.md)

---

## Validate, Sanitize, Escape: Three Different Jobs

| Step | Question it answers | Example |
|---|---|---|
| **Validate** | "Is this input acceptable at all?" | Age is an integer between 18 and 120; reject otherwise |
| **Sanitize / normalize** | "Can I safely clean it into the form I expect?" | Trim whitespace, lowercase an email |
| **Escape / encode** | "How do I safely place data in a specific *output* context?" | HTML-encode when rendering; parameterize in SQL |

Validation at the boundary **does not replace** escaping at the sink. A perfectly valid name like `O'Brien` is still dangerous to concatenate into SQL. Do both.

---

## Principles

1. **Validate at the boundary**, as early as possible, on the **server**. Client-side validation is a UX nicety; attackers bypass it.
2. **Allowlist, don't blocklist.** Define what is allowed (types, ranges, patterns, enums) rather than trying to enumerate bad input.
3. **Check type, shape, length, and range**, not only format.
4. **Reject unknown fields** instead of silently passing them through.
5. **Fail closed** with a clear `400`/`422`, without leaking internals.
6. **Convert once** into a trusted, typed object and use that object afterward.

---

## Schema Validation

Hand-written `if` chains drift and miss cases. Use a schema library such as **Zod**, **Joi**, or **Ajv** (JSON Schema).

```js
import { z } from 'zod';

const CreateUser = z.object({
  username: z.string().min(3).max(30).regex(/^[a-z0-9_]+$/),
  age: z.number().int().min(18).max(120),
  role: z.enum(['member', 'editor']),         // allowlist; 'admin' not accepted here
}).strict();                                  // unknown keys → error

app.post('/users', (req, res) => {
  const result = CreateUser.safeParse(req.body);
  if (!result.success) {
    return res.status(400).json({ errors: result.error.issues.map(i => ({ path: i.path, message: i.message })) });
  }
  const data = result.data;   // typed, trimmed to known fields
  // ... use `data`, never `req.body`
});
```

Using `.strict()` (or your library's equivalent) is what prevents **mass assignment**, where an attacker adds fields like `isAdmin: true` and your code blindly saves the whole body. It also blocks `__proto__`-style payloads from reaching merge logic ([Prototype pollution](./03_prototype-pollution.md)).

Library APIs change between major versions, so confirm exact method names against the docs for the version you install.

### Coercion warning

Query strings and form fields arrive as **strings**. `'5' + 1` is `'51'`. Be explicit: parse with schema coercion or `Number()`, then validate. See [Type conversion](../01_fundamentals/03_type-conversion-and-equality.md).

---

## Injection: Validation Isn't Enough

### SQL injection → parameterized queries

```js
// Vulnerable: input becomes part of the query text
db.query(`SELECT * FROM users WHERE email = '${email}'`);

// Safe: value is sent separately from the query
db.query('SELECT * FROM users WHERE email = $1', [email]);   // placeholder syntax depends on the driver
```

Placeholders only work for **values**. Table/column names and `ORDER BY` directions can't be parameterized; map them from an allowlist:

```js
const SORTABLE = { name: 'name', created: 'created_at' };
const column = SORTABLE[req.query.sort] ?? 'created_at';
```

### Command injection → avoid the shell

```js
import { execFile } from 'node:child_process';

// Vulnerable: shell parses user-controlled text
exec(`convert ${filename} out.png`);

// Safer: no shell, arguments passed as an array
execFile('convert', [filename, 'out.png'], callback);
```

Still validate `filename` (and beware arguments that start with `-`). See [Child processes](../16_nodejs/09_child-process-and-cluster.md).

### Path traversal → resolve and verify

```js
import path from 'node:path';

const BASE = path.resolve('uploads');

function resolveUserPath(userPath) {
  const full = path.resolve(BASE, userPath);
  if (full !== BASE && !full.startsWith(BASE + path.sep)) {
    throw new Error('Invalid path');
  }
  return full;
}
```

A request like `../../etc/passwd` resolves outside `BASE` and is rejected. See [Filesystem and path](../16_nodejs/04_filesystem-and-path.md).

### NoSQL injection

If query objects are built from request bodies (e.g. `{ email: req.body.email }`), an attacker can send an **object** (`{ "$ne": null }`) instead of a string. Validate that fields are the **expected primitive type** before querying.

---

## Resource Exhaustion

Validation also protects availability:

- **Limit body size** (`express.json({ limit: '100kb' })`) and upload size; limit array lengths and nesting depth.
- **Cap pagination:** `limit` ≤ some max, `page` ≥ 1.
- **Beware ReDoS:** a regex with nested quantifiers (`(a+)+$`) can take exponential time on crafted input. Keep patterns simple, bound input length *before* matching, and avoid running complex user-supplied regexes. See [RegExp](../09_built-in-objects/05_regexp.md).
- Add [rate limiting](../23_real-world-patterns/09_rate-limiting.md) for expensive endpoints.

---

## File Uploads

- Don't trust the filename or the `Content-Type` header: both are client-controlled. Check size limits and, if the type matters, inspect the file's actual content signature.
- Store with a **generated name**, outside the web root, and serve with a safe `Content-Type`.
- Process untrusted images/documents in a constrained environment if possible.

---

## Environment and Config Validation

Validate `process.env` once at startup so the app fails fast instead of misbehaving later:

```js
const Env = z.object({
  NODE_ENV: z.enum(['development', 'test', 'production']),
  PORT: z.coerce.number().int().min(1).max(65535).default(3000),
  DATABASE_URL: z.string().min(1),
});
export const env = Env.parse(process.env);   // throws with a clear message if invalid
```

See [Process and env](../16_nodejs/03_process-and-env.md).

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Only validating on the client | Always validate on the server |
| Blocklisting "bad characters" | Allowlist expected format/values |
| Using `req.body` directly after validating a few fields | Use the validated output object |
| Allowing unknown properties | `strict()` / `additionalProperties: false` |
| Concatenating input into SQL/shell commands | Parameterize / `execFile` |
| Checking length *after* an expensive operation | Limit size first |
| Revealing stack traces or internals in validation errors | Return clean, minimal error messages |
| Validating `typeof x === 'object'` and stopping there (`null`, arrays pass) | Use schema validation |
| Believing validation removes the need for output encoding | Do both: validate in, encode out |

---

## Testing Validation

Test boundaries and hostile cases, not only valid data: empty values, `null`, wrong types, huge strings, extra fields, `__proto__` keys, and path-traversal strings. Table-driven tests suit this well (see [Unit testing](../21_testing/02_unit-testing.md)).

```js
it.each([
  [{ username: 'ab', age: 30, role: 'member' }],            // too short
  [{ username: 'okname', age: '30', role: 'member' }],      // wrong type
  [{ username: 'okname', age: 30, role: 'admin' }],         // not allowed
  [{ username: 'okname', age: 30, role: 'member', x: 1 }],  // unknown key
])('rejects %j', (input) => {
  expect(CreateUser.safeParse(input).success).toBe(false);
});
```

---

## Quick Summary

- Treat all external input as untrusted; **validate on the server at the boundary**.
- **Allowlist** types, ranges, formats, and enums; reject unknown fields.
- Validation ≠ escaping: still parameterize SQL, avoid the shell, resolve and verify paths, and encode output.
- Cap sizes, depth, and pagination; beware ReDoS.
- Validate environment config at startup.
- Use a schema library and use its **validated output**, not the raw input.

**Next:** [Dependency Security](./05_dependency-security.md)
