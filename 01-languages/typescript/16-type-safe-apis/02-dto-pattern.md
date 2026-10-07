# The DTO Pattern

A **DTO** (data transfer object) is a type that describes data as it **crosses a boundary**: what a client sends in, or what the server sends out. It is deliberately separate from your database entity and your domain model. The point is control: DTOs decide exactly which fields travel, in what form, and keep your internal model free to change without breaking clients.

**Prerequisites:**
- [Request and response types](./01-request-response-types.md)
- [Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md) and [Partial, Required, Readonly](../07-utility-types/00-partial-required-readonly.md)
- [Schema validation](../15-runtime-validation/01-schema-validation.md)

---

## The problem with using one type for everything

```ts
interface User {
  id: string;
  email: string;
  passwordHash: string;
  role: "admin" | "member";
  createdAt: Date;
}

app.get("/users/:id", async (req, res) => {
  const user = await db.users.find(req.params.id);
  res.json(user);                 // sends passwordHash to the client
});

app.post("/users", async (req, res) => {
  const user = await db.users.create(req.body);   // a client can send { role: "admin" }
  res.json(user);
});
```

Using the entity directly in the API causes several different problems at once:

- **Leaks internals:** `passwordHash`, internal flags, soft-delete markers.
- **Mass assignment:** clients can set fields they should not (`role`, `id`, `createdAt`).
- **Tight coupling:** renaming a database column becomes a breaking API change.
- **Wrong wire types:** `Date` becomes a string in JSON ([request and response types](./01-request-response-types.md)).
- **One shape for different purposes:** creating, updating, and reading need different required fields.

## The pattern

Define separate types per purpose, and convert between them at the edge:

```text
   request body               domain / entity               response body
  +-------------+   parse    +----------------+   map      +--------------+
  | CreateUserDto| ---------> |     User      | ---------> |   UserDto    |
  +-------------+  validate  +----------------+            +--------------+
   what clients                what your code               what clients
   may send                    and database use             may see
```

```ts
// Domain / entity (internal)
interface User {
  id: string;
  email: string;
  passwordHash: string;
  role: "admin" | "member";
  createdAt: Date;
}

// Input DTO: only what a client may provide
interface CreateUserDto {
  email: string;
  password: string;               // plain password in, hash stored internally
}

// Output DTO: only what a client may see, in wire format
interface UserDto {
  id: string;
  email: string;
  role: "admin" | "member";
  createdAt: string;              // ISO string
}
```

## Mapper functions

Write explicit conversion functions at the boundary, with the DTO as the declared return type:

```ts
function toUserDto(user: User): UserDto {
  return {
    id: user.id,
    email: user.email,
    role: user.role,
    createdAt: user.createdAt.toISOString(),
  };
}

app.get("/users/:id", async (req, res) => {
  const user = await users.find(req.params.id);
  if (!user) return res.status(404).json(notFound("User"));
  res.json(toUserDto(user));      // passwordHash cannot appear: the mapper does not copy it
});
```

Why explicit mappers beat spreading (`{ ...user }`) or deleting fields:

- A **new internal field** (`mfaSecret`) is not exposed by accident, because the mapper is an **allow-list**.
- If the DTO gains a required field, the mapper fails to compile until you provide it.
- Conversions (dates, money, renamed fields) live in one place and can be tested.

For input, validate **and** convert in one step with a schema ([Zod](../15-runtime-validation/02-zod.md)):

```ts
import { z } from "zod";

const CreateUserSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8).max(200),
});

type CreateUserDto = z.output<typeof CreateUserSchema>;

app.post("/users", async (req, res) => {
  const parsed = CreateUserSchema.safeParse(req.body);
  if (!parsed.success) return res.status(400).json(validationError(parsed.error));

  const user = await users.create({
    email: parsed.data.email,
    passwordHash: await hash(parsed.data.password),
    role: "member",                         // set by the server, never by the client
  });
  res.status(201).json(toUserDto(user));
});
```

Unknown keys (like `role`) are stripped by the schema, and anything that must be controlled by the server is set by the server.

## Deriving DTOs from the entity

You can derive DTO types with utility types, which keeps them in sync with the model:

```ts
type PublicUser = Pick<User, "id" | "email" | "role">;        // allow-list
type CreateUserInternal = Omit<User, "id" | "createdAt">;     // deny-list
type UpdateUserDto = Partial<Pick<User, "email">>;
```

Trade-offs:

- **Derivation reduces duplication** and keeps names aligned.
- **It also couples the API to the model:** adding a field to `User` can change `Omit`-derived types, and a rename flows into the API.
- Prefer `Pick` (allow-list) over `Omit` (deny-list) for **outputs** and **security-sensitive inputs**, so new fields are not exposed by default ([Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md)).
- Wire types differ from domain types in more than field names (`Date` versus `string`), so a plain `Pick` often is not enough. Use `Pick` plus an explicit conversion, or write the DTO out.

Writing DTOs by hand is more verbose but makes the API surface visible and stable in code review. For public APIs, that is usually worth it.

## One DTO per purpose

| DTO | Purpose | Typical differences from the entity |
|---|---|---|
| `CreateXDto` | create input | no `id`, no timestamps, no server-controlled fields; may carry a plain password |
| `UpdateXDto` | change input | all fields optional (`PATCH`), still excludes server-controlled fields |
| `XDto` / `XResponseDto` | read output | no secrets, wire-format types, maybe flattened or renamed fields |
| `XListItemDto` | list output | a smaller summary shape |
| `XDetailDto` | detail output | includes related data |

An update DTO needs one extra check. A `PATCH` whose body is `{}` is valid for an all-optional type but usually means nothing. Reject it if that is a mistake in your API:

```ts
const UpdateUserSchema = z
  .object({ email: z.string().email().optional() })
  .refine((v) => Object.keys(v).length > 0, { message: "No fields to update" });
```

## Class-based DTOs

Some frameworks, most notably NestJS, use **classes** with decorators for DTOs:

```ts
export class CreateUserDto {
  @IsEmail() email!: string;
  @MinLength(8) password!: string;
}
```

The class serves as the type and holds validation metadata. This ties into framework features such as automatic validation pipes ([NestJS](../20-nodejs-backend/08-nestjs.md)). The ideas are the same: separate input and output types, an allow-list of fields, and mapping at the boundary. If you use schemas instead, you get the type from `z.output`, with no class needed.

## Mapping at scale

- **Nested data:** map relationships with their own mappers (`toOrderDto` calls `toUserSummaryDto`).
- **Lists:** `users.map(toUserDto)` keeps the mapping one function, reused.
- **Many shapes:** a small set of mappers per entity is manageable. If it grows unwieldy, consider whether endpoints are returning too many variations.
- **Database selects:** fetch only the columns you need (`select`) so sensitive data never even enters memory, as a second layer of defense.
- **Libraries** can generate mappers, but explicit functions are easy to read and type-check.

## When you can skip it

DTOs add code. They are less useful for:

- Prototypes and internal tools where leaking fields is not a risk.
- Entities that are already pure data with no secrets and no wire-format differences, and the API is internal.

Even then, for **input** you should still validate and allow-list fields. The output mapping is the part you might skip. For anything public or security-relevant, keep both.

## Important rules and misconceptions

- **A DTO is not a database model.** It describes a contract, not storage.
- **Mapping is not validation.** Mapping shapes data you already trust. Validation proves untrusted input is well-formed. Input needs both: validate, then convert.
- **`Omit<User, "passwordHash">` is a type, not a runtime operation.** It does not remove the field from an object. Only an actual mapper (or a database `select`) does.
- **Types do not strip properties.** Returning a `User` where a `UserDto` is expected compiles (extra properties are allowed for non-literal values) and sends everything at runtime.
- **Do not reuse request DTOs as response DTOs.** They have different required fields and different audiences.

## Common mistakes

- Returning ORM entities straight from controllers.
- Using `Omit<User, "passwordHash">` as a type and then returning the full object.
- Letting the client set server-controlled fields (`id`, `role`, `createdAt`).
- Using `Partial<User>` as the update DTO and exposing every field.
- Using one `User` interface for every purpose.
- Repeating field-by-field mapping inline in several handlers.
- Spreading `req.body` into a database call.
- Forgetting to convert `Date` and similar types to wire format.

## Debugging

- **Check the actual JSON** a handler returns, not just its type: log or snapshot it.
- **Snapshot-test mappers** with a full entity including sensitive fields, and assert those fields are absent from the output.
- If a field appears in a response unexpectedly, search for `res.json(` calls that pass an entity or a spread object.
- Use a lint rule or code review checklist: handlers return a `*Dto`, never an entity type.
- If a mapper compiles but a field is `undefined` at runtime, the entity and its declared type disagree (see [trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).

## Quick summary

- A DTO describes data crossing a boundary. Keep it separate from the entity and domain model.
- Use **input DTOs** (validated, allow-listed, no server-controlled fields) and **output DTOs** (no secrets, wire-format types).
- Convert with explicit mapper functions. They act as allow-lists and fail to compile when a DTO changes.
- Derive with `Pick`, `Partial`, and friends when it helps, preferring allow-lists. Types never remove fields at runtime, only mappers and selects do.
- One DTO per purpose: create, update, detail, list.

**Next:** [Pagination types](./03-pagination-types.md)