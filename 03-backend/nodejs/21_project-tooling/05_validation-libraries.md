# Validation Libraries

Everything that crosses a **boundary** into your program is untrusted: HTTP request bodies, query strings, environment variables, files, messages from queues, responses from third-party APIs, even `localStorage` and `JSON.parse` results. TypeScript types and JSDoc comments **disappear at runtime**, so they do not protect you from a malformed payload. A **schema validation library** checks the real data against a declared shape and gives you trustworthy values (or clear errors).

See also: [Input Validation](../22_security/04_input-validation.md), [Environment Variables](./02_environment-variables.md), [Error Handling](../10_error-handling/00_README.md), [Request and Response](../15_networking/04_request-and-response.md).

## Why validate at the edge

```js
// Trusting the input
app.post("/users", async (req, res) => {
  const user = await db.users.create({
    name: req.body.name,
    age: req.body.age,
  });
  // what if name is an object, age is "abc", or body is undefined?
});
```

Without validation you get: crashes from `undefined`, type confusion (`"5" + 1`), database errors, injection attempts, mass assignment (clients setting `isAdmin: true`), and bugs that appear far from their source.

| Validation does                                     | Validation does not replace                                         |
| --------------------------------------------------- | ------------------------------------------------------------------- |
| Reject malformed data early with clear errors       | Authorization (who may do this?)                                    |
| Convert types (`"42"` → `42`), apply defaults, trim | Output encoding (escaping for HTML, SQL parameters)                 |
| Strip unknown fields (stop mass assignment)         | Business rules that need the database (is the email already taken?) |
| Give you a single source of truth for a shape       | Rate limiting and authentication                                    |

## The options

| Library     | Style                                    | Strengths                                                     | Trade-offs                                           |
| ----------- | ---------------------------------------- | ------------------------------------------------------------- | ---------------------------------------------------- |
| **zod**     | Chainable schemas, TypeScript inference  | Excellent DX, huge ecosystem, `z.infer` types                 | Larger bundle than modular options                   |
| **Valibot** | Modular functions (`v.object`, `v.pipe`) | Tiny bundles through tree-shaking, similar capabilities       | Smaller ecosystem                                    |
| **Joi**     | Chainable schemas (hapi ecosystem)       | Mature, rich rules, great messages                            | No TypeScript inference; larger; Node-focused        |
| **Yup**     | Chainable schemas                        | Popular with form libraries (Formik)                          | Weaker TypeScript story than zod                     |
| **Ajv**     | **JSON Schema** compiler                 | Fastest, standard format, used by Fastify and OpenAPI tooling | Schemas are JSON, not code; error messages need work |
| **TypeBox** | Builds JSON Schema with TypeScript types | JSON Schema + types in one; pairs with Ajv/Fastify            | Smaller community                                    |
| **ArkType** | TypeScript-like string syntax            | Very fast, concise                                            | Newer: evaluate maturity                             |

Defaults: **zod** for most applications (especially TypeScript), **Ajv/TypeBox** for Fastify and OpenAPI-driven APIs, **Valibot** when bundle size matters on the client.

## zod

```bash
npm install zod
```

### Defining and parsing

```js
import { z } from "zod";

export const createUserSchema = z.object({
  name: z.string().trim().min(1, "name is required").max(100),
  email: z.string().trim().email(),
  age: z.coerce.number().int().min(13).max(120).optional(),
  role: z.enum(["member", "admin"]).default("member"),
  tags: z.array(z.string().min(1)).max(10).default([]),
});

// parse: returns the cleaned data or THROWS a ZodError
const user = createUserSchema.parse(input);

// safeParse: never throws; returns { success, data } or { success: false, error }
const result = createUserSchema.safeParse(input);
if (!result.success) {
  console.log(result.error.issues);
  // [{ path: ['email'], message: 'Invalid email', code: 'invalid_string', ... }]
} else {
  result.data; // typed, trimmed, defaults applied
}
```

By default, unknown keys are **stripped** from the output, so a client cannot smuggle extra fields into your database call. Use `.strict()` to reject them instead. (Zod 4 also offers top-level helpers such as `z.email()` and `z.url()`; the chained forms above work in both major versions.)

### Common building blocks

```js
z.string()
  .min(3)
  .max(20)
  .regex(/^[a-z0-9_]+$/);
z.string().uuid();
z.string().url();
z.number().int().positive().max(1000);
z.boolean();
z.date();
z.enum(["draft", "published"]);
z.literal("v1");
z.union([z.string(), z.number()]);
z.array(z.string()).nonempty();
z.record(z.string(), z.number()); // { [key: string]: number }
z.tuple([z.string(), z.number()]);
z.string().nullable(); // string | null
z.string().optional(); // string | undefined
z.object({ a: z.string() }).partial(); // all fields optional
z.object({ a: z.string(), b: z.number() }).pick({ a: true });
z.object({ a: z.string(), b: z.number() }).omit({ b: true });
z.object({ a: z.string() }).extend({ c: z.boolean() });
```

### Coercion, transforms, and refinements

```js
// Query strings are always strings: coerce them
const listQuery = z.object({
  page: z.coerce.number().int().min(1).default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
  sort: z.enum(["createdAt", "name"]).default("createdAt"),
});

// Transform after validating
const slug = z.string().transform((s) => s.toLowerCase().replace(/\s+/g, "-"));

// Cross-field rules
const signupSchema = z
  .object({ password: z.string().min(12), confirm: z.string() })
  .refine((v) => v.password === v.confirm, {
    message: "passwords do not match",
    path: ["confirm"],
  });

// Async checks (use parseAsync / safeParseAsync)
const uniqueEmail = z
  .string()
  .email()
  .refine(async (e) => !(await db.users.exists(e)), "email already registered");
```

**Careful with `z.coerce.boolean()`:** it uses JavaScript truthiness, so `"false"` becomes `true`. Parse boolean strings explicitly: `z.enum(['true', 'false']).transform((v) => v === 'true')`.

### Types from schemas

```ts
type CreateUserInput = z.infer<typeof createUserSchema>; // input after parsing
```

One schema yields both the runtime check and the static type, so they cannot drift apart.

## A validation middleware for Express

```js
// src/middleware/validate.js
export function validate(schema, source = "body") {
  return (req, res, next) => {
    const result = schema.safeParse(req[source]);

    if (!result.success) {
      return res.status(400).json({
        error: "ValidationError",
        issues: result.error.issues.map((i) => ({
          path: i.path.join("."),
          message: i.message,
        })),
      });
    }

    req.validated = { ...req.validated, [source]: result.data }; // cleaned data, separate from raw input
    next();
  };
}
```

```js
// src/modules/users/users.routes.js
import { Router } from "express";
import { validate } from "#middleware/validate.js";
import { createUserSchema, listQuery } from "./users.schema.js";

export function createUsersRouter({ usersController }) {
  const router = Router();
  router.get("/", validate(listQuery, "query"), usersController.list);
  router.post("/", validate(createUserSchema), usersController.create);
  return router;
}
```

```js
// controller: only ever sees validated data
export const create = async (req, res) => {
  const user = await usersService.create(req.validated.body);
  res.status(201).json(user);
};
```

Writing the cleaned result to a separate property (`req.validated`) avoids reassigning `req.query` or `req.body`, which some framework versions treat as read-only. Fastify can do this natively by attaching schemas to routes.

Also validate **route parameters**:

```js
const idParam = z.object({ id: z.string().uuid() });
router.get("/:id", validate(idParam, "params"), usersController.get);
```

## Valibot

Same ideas, but built from small standalone functions, so bundlers include only what you use.

```bash
npm install valibot
```

```js
import * as v from "valibot";

const CreateUser = v.object({
  name: v.pipe(v.string(), v.trim(), v.minLength(1), v.maxLength(100)),
  email: v.pipe(v.string(), v.trim(), v.email()),
  age: v.optional(v.pipe(v.number(), v.integer(), v.minValue(13))),
});

const result = v.safeParse(CreateUser, input);
if (!result.success) console.log(result.issues);
else console.log(result.output);
```

A good fit for browser code where every kilobyte counts.

## Joi

```bash
npm install joi
```

```js
import Joi from "joi";

const schema = Joi.object({
  name: Joi.string().trim().min(1).max(100).required(),
  email: Joi.string().email().required(),
  age: Joi.number().integer().min(13).max(120),
  role: Joi.string().valid("member", "admin").default("member"),
}).options({ stripUnknown: true, abortEarly: false });

const { value, error } = schema.validate(input);
if (error) {
  console.log(error.details.map((d) => d.message));
} else {
  console.log(value);
}
```

Joi does not infer TypeScript types, but it is mature and expressive, and common in hapi-based and older Express codebases.

## Ajv and JSON Schema

**JSON Schema** is a language-neutral standard for describing JSON. **Ajv** compiles schemas into very fast validator functions. Fastify uses it by default, and OpenAPI documents are JSON Schema based.

```bash
npm install ajv ajv-formats
```

```js
import Ajv from "ajv";
import addFormats from "ajv-formats";

const ajv = new Ajv({
  allErrors: true,
  coerceTypes: true,
  removeAdditional: true,
  useDefaults: true,
});
addFormats(ajv);

const schema = {
  type: "object",
  properties: {
    name: { type: "string", minLength: 1, maxLength: 100 },
    email: { type: "string", format: "email" },
    age: { type: "integer", minimum: 13 },
  },
  required: ["name", "email"],
  additionalProperties: false,
};

const validate = ajv.compile(schema); // compile ONCE at startup; reuse the function

const data = { name: "Ada", email: "ada@example.com" };
if (!validate(data)) {
  console.log(validate.errors); // [{ instancePath: '/email', message: '...', ... }]
}
```

Fastify route schemas:

```js
fastify.post(
  "/users",
  {
    schema: {
      body: schema,
      response: {
        201: { type: "object", properties: { id: { type: "string" } } },
      }, // response schema also speeds up serialization
    },
  },
  handler,
);
```

**TypeBox** lets you write the schema in TypeScript-friendly code and get the static type:

```ts
import { Type, Static } from "@sinclair/typebox";

const User = Type.Object({
  name: Type.String({ minLength: 1 }),
  email: Type.String({ format: "email" }),
});
type User = Static<typeof User>;
```

## What to validate, and where

| Boundary                          | Validate                                                                          |
| --------------------------------- | --------------------------------------------------------------------------------- |
| HTTP request                      | `body`, `query`, `params`, relevant `headers`                                     |
| Environment                       | All variables at startup ([Environment Variables](./02_environment-variables.md)) |
| Database reads                    | Usually trust your own schema; validate when data is loose (JSON columns)         |
| Third-party API responses         | Validate what you rely on; APIs change without notice                             |
| Messages from queues and webhooks | Always (verify signatures, then validate the payload)                             |
| User-uploaded files               | Type, size, and content, not just file extension                                  |
| Client-side forms                 | For UX only: **always** validate again on the server                              |
| `JSON.parse` of untrusted text    | Parse, then validate the shape                                                    |

Use the **same schema package** for server and client when you can (a shared `packages/shared` in a monorepo), so rules cannot diverge.

## Error responses

Give clients errors they can act on, without leaking internals:

```json
{
  "error": "ValidationError",
  "issues": [
    { "path": "email", "message": "Invalid email" },
    { "path": "age", "message": "Number must be greater than or equal to 13" }
  ]
}
```

- Use `400` (or `422`) for validation failures
- Report **all** issues at once so users fix them in one pass
- Do not echo sensitive input values back, and do not include stack traces

## Schemas for responses and DTOs

Validation is not only for incoming data. A schema can also **shape outgoing data**, so you never leak internal fields such as password hashes:

```js
const publicUser = z.object({
  id: z.string(),
  name: z.string(),
  email: z.string().email(),
});

res.json(publicUser.parse(userFromDb)); // strips passwordHash and anything else not declared
```

## Pitfalls

| Pitfall                                              | Why it hurts                        | Better                                                          |
| ---------------------------------------------------- | ----------------------------------- | --------------------------------------------------------------- |
| Trusting TypeScript types for external data          | Types vanish at runtime             | Validate at every boundary                                      |
| Validating only on the client                        | Trivially bypassed                  | Always re-validate on the server                                |
| Passing raw `req.body` to the database               | Mass assignment, type confusion     | Use the validated, stripped output                              |
| Not stripping unknown keys                           | Clients set fields like `isAdmin`   | Zod's default stripping, `.strict()`, or Ajv `removeAdditional` |
| Forgetting coercion for query strings and params     | Everything is a string              | `z.coerce.number()`, Ajv `coerceTypes`                          |
| `z.coerce.boolean()` on `"false"`                    | Becomes `true`                      | Explicit enum plus transform                                    |
| Compiling Ajv schemas per request                    | Slow                                | Compile once at startup                                         |
| Throwing raw validation errors to clients            | Leaks internals; poor UX            | Map issues to a stable error format                             |
| Using validation for authorization or business rules | Wrong tool                          | Separate checks in the service layer                            |
| Duplicated schemas for the same shape                | They drift apart                    | One schema, derived types, shared package                       |
| Heavy regexes in schemas (ReDoS)                     | Denial of service via crafted input | Simple patterns; length limits first                            |
| No size limits on strings, arrays, and bodies        | Memory and CPU abuse                | `.max()` everywhere; body size limits in the server             |
| Validating after use                                 | Too late                            | Validate first thing in the request path                        |

## Key takeaways

- Runtime data needs runtime validation: types do not exist after compilation
- Validate every boundary: requests, environment, webhooks, third-party responses; on the client too, but **never only** there
- **zod** is the default choice (great DX, inferred types); **Valibot** for small bundles; **Ajv/TypeBox** for JSON Schema and Fastify; **Joi** in established codebases
- Use schemas to coerce, trim, default, and **strip unknown fields**
- Put validation in middleware (or route schemas) so controllers only see clean data
- Return consistent `400` errors with all issues, without leaking internals
- Use schemas on outgoing data as well to avoid leaking sensitive fields

**Next:** [Logging](./06_logging.md)
