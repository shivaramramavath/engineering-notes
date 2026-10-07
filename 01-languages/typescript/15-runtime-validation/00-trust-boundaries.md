# Trust Boundaries

TypeScript checks that your code is **consistent with its own declarations**. It cannot check that data arriving from outside the program matches those declarations, because types are erased before the program runs. A **trust boundary** is any point where data enters your code from somewhere you do not control. Validate there, once, and the rest of your code can rely on its types.

**Prerequisites:**
- [Type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)
- [`any` and `unknown`](../01-fundamentals/06-any-and-unknown.md)
- [Type guards and assertion functions](../03-unions-and-narrowing/05-type-guards-and-assertion-functions.md)

---

## The problem

```ts
interface User { id: number; email: string }

const res = await fetch("/api/user/1");
const user: User = await res.json();     // compiles: res.json() returns Promise<any>

user.email.toLowerCase();                 // crashes if the server sent { id: 1, email: null }
```

The annotation `: User` is a *claim*, not a check. TypeScript accepted it because `any` is assignable to everything. Nothing verifies the claim, and the failure surfaces far from its cause, as a `TypeError` somewhere deep in your code.

## What counts as a boundary

Anything produced outside your program's own type-checked code:

| Source | Why it is untrusted |
|---|---|
| HTTP request body, query string, path params, headers, cookies | the client controls them, including malicious clients |
| Responses from APIs and SDKs | the other side can change, break, or lie |
| `JSON.parse`, `res.json()`, `JSON` files | return `any`, or whatever the text contains |
| Environment variables and CLI arguments | always `string \| undefined`, often missing or malformed |
| `localStorage`, cookies, `sessionStorage`, files | old versions of your own app, users editing them |
| URL search params, `FormData`, `postMessage`, WebSocket messages | strings and arbitrary payloads |
| Message queue and webhook payloads | produced by other systems, possibly stale or forged |
| Database rows from raw SQL or after schema drift | the table can differ from your model |
| Output of an LLM or other generator | free-form text, even when asked for JSON |

Data produced by your own typed code, passed between your own functions, is **inside** the boundary and does not need re-checking.

## The pattern: parse at the edge

```text
   UNTRUSTED                    BOUNDARY                     TRUSTED
 +-----------+            +----------------+            +---------------+
 |  unknown  |  -------->  |   validate /   |  -------->  |  typed value  |
 | (any data)|            |     parse      |            | (User, Config)|
 +-----------+            +-------+--------+            +---------------+
                                  |
                                  v
                          reject with a clear error
                          (400, startup failure, log)
```

1. **Receive as `unknown`.** Not `any`, not an assertion. `unknown` forces you to check before use.
2. **Validate** shape, types, and formats against a schema or guard.
3. **Produce a typed value** or a structured error.
4. **Pass the typed value inward.** Core logic never sees raw input.

This idea is often summarized as **"parse, don't validate"**: instead of checking data and then continuing to use the unchecked original, convert it into a value whose *type* proves it was checked.

```ts
// Boundary function: honest about what it knows
async function fetchJson(url: string): Promise<unknown> {
  const res = await fetch(url);
  return res.json();            // any -> unknown on the way out
}

const raw = await fetchJson("/api/user/1");
const user = UserSchema.parse(raw);   // User, or throws a validation error
```

Declaring the boundary function as returning `unknown` means callers cannot use the result without validating it. The compiler enforces the discipline.

## Hand-written guard vs schema

A guard is a function that checks and narrows:

```ts
function isUser(x: unknown): x is User {
  return (
    typeof x === "object" && x !== null &&
    typeof (x as { id?: unknown }).id === "number" &&
    typeof (x as { email?: unknown }).email === "string"
  );
}
```

It works, but you must keep it in sync with `User` by hand, it only returns a boolean (no error detail), it does not strip or transform, and the two can drift apart silently. A **schema** describes the shape once and derives both the runtime check and the TypeScript type, so they cannot disagree. That is the subject of the next notes: [schema validation](./01-schema-validation.md) and [Zod](./02-zod.md).

## What to validate, and what not

**Shape and format** belong at the boundary: required fields, types, allowed values, string formats (email, UUID, URL), numeric ranges, array lengths.

**Business rules that need other state** do not: "this email is not already registered", "the account has enough balance". They need the database or other services, so they live in your domain or service layer and return domain errors ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)).

A useful split:

| Layer | Question | Example |
|---|---|---|
| Boundary | Is this well-formed? | `email` is a string that looks like an email |
| Domain | Is this allowed right now? | that email is not taken |

## Security reasons

Validation at the boundary is a security measure, not only a type-safety one. See [input validation](../23-security/01-input-validation.md).

- **Allow-list, do not deny-list.** Accept only fields and values you expect.
- **Strip or reject unknown keys.** Spreading raw request bodies into database updates enables mass assignment (`{ "role": "admin" }`).
- **Bound everything:** string length, array size, number range, request body size, nesting depth.
- **Beware of regexes** with catastrophic backtracking (ReDoS) in user-facing validation.
- **Never trust client-side validation.** It improves UX. The server must validate again.
- **Validate before using data** in queries, file paths, shell commands, or HTML.
- **Fail closed.** If validation cannot decide, reject.

## Where to put the boundary

- **HTTP server:** in middleware or at the top of each handler, for `body`, `query`, and `params` ([middleware](../20-nodejs-backend/03-middleware.md)).
- **Configuration:** at **startup**. If environment variables are wrong, crash immediately with a clear message ([config and environment](../20-nodejs-backend/01-config-and-environment.md)).
- **API clients:** in the client wrapper, so every call returns a validated type ([typed fetch and API client](../16-type-safe-apis/05-typed-fetch-and-api-client.md)).
- **Persistence:** when reading `localStorage`, files, or loosely typed rows, with a fallback for old or corrupt data.
- **Message consumers:** at the entry of each handler.

Validate **once per entry point**, not at every function call. Repeated validation inside the core is wasted work and a sign that the boundary is unclear.

## Trusted does not mean correct

Once validated, a value has the right *shape*. It may still be wrong in business terms. Brand validated values when the distinction matters (`Email`, `UserId`) so unvalidated strings cannot be passed where validated ones are required ([branded types](../10-advanced-types/07-branded-types.md)).

## Important rules and misconceptions

- **"TypeScript validates my API responses."** It does not. It trusts whatever type you give it.
- **"`as User` is safe because the server is mine."** Servers change, deploy out of order, and return error bodies. Your frontend and backend can run different versions at the same time.
- **"A generated client makes validation unnecessary."** Generated types are still claims unless the client validates at runtime.
- **"The database is trusted."** Mostly, via an ORM, but raw queries, migrations, and JSON columns can all produce unexpected shapes.
- **"Validation is only for user input."** Third-party APIs and your own other services are boundaries too.

## Common mistakes

- Casting `JSON.parse(...)` or `res.json()` with `as`.
- Typing a function parameter as `any` for "flexibility" at a boundary.
- Validating deep inside the core after raw data has already spread.
- Validating in some code paths and not others.
- Passing the **original** unvalidated object onward instead of the parsed result.
- Writing a guard that checks only some fields.
- Skipping validation for internal service-to-service calls.
- Returning raw validation internals (stack traces, schema dumps) to clients.
- Logging full invalid payloads that may contain personal data.

## Debugging

- When a `TypeError` appears for "cannot read property of undefined" on a typed value, trace it **upstream** to where the data entered, and look for an assertion or `any`.
- Make boundaries easy to find: return `unknown` from them, and use `typescript-eslint` rules (`no-unsafe-assignment`, `no-unsafe-member-access`, `no-explicit-any`) to surface `any` flowing inward.
- Log validation failures with the **path** and **reason**, not the whole payload.
- Reproduce with the real payload: capture one failing response and add it as a test case ([validation recipes](./04-validation-recipes.md)).

## Quick summary

- Types describe what you *believe*. Runtime validation checks what actually arrived.
- Boundaries: HTTP input, external API responses, `JSON.parse`, env vars, storage, files, webhooks, messages, and loosely typed database rows.
- Receive as `unknown`, validate once at the edge, and pass typed values inward.
- Validate shape and format at the boundary. Business rules belong in the domain layer.
- Allow-list fields, bound sizes, reject or strip unknown keys, and never rely on client-side validation.
- Prefer a schema over hand-written guards so the runtime check and the type stay in sync.

**Next:** [Schema validation](./01-schema-validation.md)