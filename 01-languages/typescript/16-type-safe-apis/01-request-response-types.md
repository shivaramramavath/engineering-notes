# Request and Response Types

Typing an HTTP API means describing each piece of a request (path parameters, query string, headers, body) and each possible response (a body per status code). The details that cause bugs are mostly about the **wire format**: everything in a URL is a string, JSON has no `Date` or `undefined`, and a handler can legitimately return several different shapes. This note covers how to type these precisely, and how to avoid the common lies.

**Prerequisites:**
- [API contracts](./00-api-contracts.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Generic types](../06-generics/01-generic-types.md)

---

## The parts of a request

| Part | What arrives | Typing notes |
|---|---|---|
| **Path params** (`/users/:id`) | always `string` | convert and validate (`"42"` is not `42`) |
| **Query string** (`?page=2&tag=a&tag=b`) | `string`, or `string[]` for repeated keys, or absent | coerce numbers and booleans, expect arrays |
| **Headers** | `string`, `string[]`, or absent | names are case-insensitive |
| **Body** | JSON, form data, bytes | `unknown` until validated |

Model them as separate types, so each can be validated and documented on its own:

```ts
type GetPostsRequest = {
  params: { userId: string };
  query: { page?: number; tag?: string[] };   // the *validated* shape, not the raw one
};

type CreatePostRequest = {
  params: { userId: string };
  body: { title: string; body: string };
};
```

Note the distinction: what **arrives** (strings) versus what your handler **receives after validation** (numbers, arrays, defaults). Describe the second in your handler types, and let a schema do the conversion ([validation recipes](../15-runtime-validation/04-validation-recipes.md)).

## Responses: one body per status

A single `Promise<User>` return type hides the fact that the same endpoint can return a user, a validation error, or a not-found error. Model that as a union keyed by status:

```ts
type GetUserResponse =
  | { status: 200; body: UserDto }
  | { status: 404; body: ErrorBody }
  | { status: 401; body: ErrorBody };
```

A client can then branch on `status` and TypeScript narrows `body` ([discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)):

```ts
function handle(res: GetUserResponse) {
  switch (res.status) {
    case 200: return res.body.name;       // UserDto
    case 404: return "not found";
    case 401: return "sign in";
  }
}
```

Document **every** status the endpoint can return, not only the success case. Error body shapes are covered in [error response types](./04-error-response-types.md).

### Envelopes

You can return the data directly (`{ id, name }`) or wrap it (`{ data: { id, name } }`). Envelopes make room for metadata (pagination, warnings) without breaking the shape, at the cost of an extra layer:

```ts
interface Envelope<T> { data: T }
type UserResponse = Envelope<UserDto>;
```

Pick one convention for the whole API and use a generic helper for it.

## Wire format vs domain types

JSON is what travels over the network, and JSON cannot represent every TypeScript value:

| In your code | After `JSON.stringify` / `JSON.parse` |
|---|---|
| `Date` | `string` (ISO 8601) |
| `undefined` property | **dropped** |
| `undefined` in an array | `null` |
| `bigint` | **throws** on `stringify` |
| `Map`, `Set` | `{}` (empty object) |
| `NaN`, `Infinity` | `null` |
| class instance | plain object (methods lost) |
| function | dropped |

So a type that is correct inside your server is **wrong** as a description of the response:

```ts
interface User { id: string; name: string; createdAt: Date }

// a client that declares `createdAt: Date` is lying: it receives a string
```

Keep **two types**, and convert between them explicitly:

```ts
interface User { id: string; name: string; createdAt: Date }          // domain
interface UserDto { id: string; name: string; createdAt: string }     // wire (ISO string)

function toUserDto(u: User): UserDto {
  return { id: u.id, name: u.name, createdAt: u.createdAt.toISOString() };
}
```

This is the heart of the [DTO pattern](./02-dto-pattern.md). On the client, parse the string back into a `Date` (or keep it as a string) in a validation step, not by assuming the type.

A helper type can describe the serialization mechanically for simple cases:

```ts
type Serialized<T> =
  T extends Date ? string :
  T extends (infer U)[] ? Serialized<U>[] :
  T extends object ? { [K in keyof T]: Serialized<T[K]> } :
  T;

type Wire = Serialized<{ id: string; createdAt: Date; tags: string[] }>;
// { id: string; createdAt: string; tags: string[] }
```

It is a convenience for plain data, not a complete model: it ignores `undefined` removal, functions, `Map`/`Set`, and custom `toJSON` methods. For important endpoints, write the wire type out explicitly.

### Practical wire-format conventions

- **Dates:** ISO 8601 strings in UTC. Consider a branded alias to mark them (`type IsoDate = string & { readonly __brand: "IsoDate" }`, see [branded types](../10-advanced-types/07-branded-types.md)).
- **IDs:** strings. Large numeric ids lose precision as JSON numbers in JavaScript (above 2^53), and strings also keep the door open for UUIDs.
- **Money:** integer minor units (`amountCents: number`) or a decimal string. Avoid floating point.
- **Enums:** string literal unions (`"draft" | "published"`), not TypeScript `enum`, which is a runtime object that does not exist on the other side ([enums and const objects](../01-fundamentals/05-enums-and-const-objects.md)).
- **Absence:** decide between a missing key (`optional`) and `null` (explicit "no value"), document it, and stay consistent. JSON has `null` but not `undefined`.

## Typing handlers (Express)

Express's types take generics for the request and response parts:

```ts
import type { RequestHandler } from "express";

type Params = { id: string };
type ResBody = UserDto | ErrorBody;
type ReqBody = never;               // GET has no body
type Query = { expand?: string };

const getUser: RequestHandler<Params, ResBody, ReqBody, Query> = async (req, res) => {
  const user = await users.find(req.params.id);
  if (!user) {
    return res.status(404).json({ error: { code: "NOT_FOUND", message: "User not found" } });
  }
  res.json(toUserDto(user));        // checked against ResBody
};
```

The generics order is: path params, response body, request body, query. They help check what you **send**, and give you typed access to what you read. But they are **assertions about input**: Express does not validate anything. `req.body` typed as `CreateUserBody` is only true if you validated it ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)). Validate first, then use the parsed value, or you are back to a claim the compiler cannot check.

## Request methods and semantics

- **`GET` and `DELETE`** usually have no body. Put filters and pagination in the query string.
- **`POST`** creates or triggers actions. Respond `201` with the created resource or its location.
- **`PUT`** replaces a whole resource, so the body type is the full resource.
- **`PATCH`** updates part of one. The body is a partial shape, and you must decide how to express "clear this field" (usually `null`) versus "leave it alone" (absent):

```ts
type UpdateUserBody = { name?: string; bio?: string | null };
// absent -> unchanged, null -> clear, string -> set
```

- **Idempotency:** `PUT`, `DELETE`, and `GET` should be safe to retry. For `POST`, an idempotency key header lets clients retry safely ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)).

## Using `satisfies` for response literals

When a handler or a mock builds a response literal, `satisfies` checks it against the contract without widening the inferred type:

```ts
const ok = {
  status: 200,
  body: { id: "1", name: "Asha", createdAt: "2024-01-01T00:00:00Z" },
} satisfies GetUserResponse;
```

A missing or misspelled field is a compile error, and `ok.status` is still the literal `200`.

## Important rules and misconceptions

- **The compiler cannot check what crosses the network.** A response type is a statement of what you expect.
- **Express generics are not validation.** They describe, they do not enforce.
- **A server type and a client type for the same payload are different types** when serialization is involved (`Date` versus string).
- **`req.query` values are never numbers or booleans.** They are strings (or arrays of strings).
- **Optional is not nullable.** Absent keys and `null` values are different on the wire.
- **`status` is part of the response type.** A function returning `Promise<User>` hides the error cases.

## Common mistakes

- Typing a response field as `Date` when it arrives as a string.
- Using TypeScript `enum` in shared contract types.
- Typing `req.query.page` as `number`.
- Describing only the `200` response.
- Using `any` for `req.body` and `res.json` arguments.
- Using one interface for the create request, the update request, and the response.
- Using floating-point numbers for money.
- Treating missing and `null` as interchangeable without documenting it.

## Debugging

- Print the **raw** payload (`await res.text()`) and compare it with the type. Mismatches in dates, nulls, and numbers-as-strings show up immediately.
- If a type says `Date` but the value has no `getTime`, you are holding a string.
- If a query value is `"undefined"` or `"NaN"`, a client serialized `undefined` into the URL.
- Add a validation step that reports the path of the first mismatching field ([Zod](../15-runtime-validation/02-zod.md)).
- Check response headers (`Content-Type`) when parsing fails: error pages are often HTML.

## Quick summary

- Type each request part separately. Path and query values arrive as strings, so validate and convert before handlers use them.
- Model responses as a union of `{ status, body }` shapes and document every status, not only the success case.
- JSON changes types: `Date` becomes a string and `undefined` disappears. Keep separate wire and domain types, and convert explicitly.
- Use string IDs, ISO date strings, integer money, and string literal unions for enums.
- Express handler generics describe, they do not validate. Validate input before trusting its type.

**Next:** [The DTO pattern](./02-dto-pattern.md)