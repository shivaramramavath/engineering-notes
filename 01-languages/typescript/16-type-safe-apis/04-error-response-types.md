# Error Response Types

Success responses are easy to type. Error responses are where APIs become inconsistent: one endpoint returns `{ message }`, another `{ error: "..." }`, a third an HTML page from a proxy. A well-designed error contract gives every failure the same shape, a **stable machine-readable code**, and enough detail for clients to react. This note covers designing that shape, typing it on both sides, and keeping it safe.

**Prerequisites:**
- [Request and response types](./01-request-response-types.md)
- [Custom errors](../11-error-handling/01-custom-errors.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)

---

## What clients need from an error

A client wants to answer four questions:

| Question | Provided by |
|---|---|
| **What kind** of failure is it? | a stable `code` (`"NOT_FOUND"`, `"VALIDATION"`) |
| **Which HTTP status** applies? | the status line (`404`, `400`, ...) |
| **What can I show the user?** | a human-readable `message` |
| **Which input was wrong, and how?** | structured `details` (field paths, limits) |

Plus, for support: a **request id** to find the matching server log.

Clients should branch on `code` (and status), never on message text. Messages change, may be translated, and are not part of the contract.

## A consistent error body

```ts
interface ErrorBody {
  error: {
    code: ErrorCode;
    message: string;
    requestId?: string;
  };
}

type ErrorCode =
  | "VALIDATION"
  | "UNAUTHORIZED"
  | "FORBIDDEN"
  | "NOT_FOUND"
  | "CONFLICT"
  | "RATE_LIMITED"
  | "INTERNAL";
```

The code union can be the same one your server-side error classes use ([custom errors](../11-error-handling/01-custom-errors.md)), shared from one package. Wrapping the error in an `error` key leaves room for other top-level fields without ambiguity.

### Error codes mapped to status

```ts
const statusFor: Record<ErrorCode, number> = {
  VALIDATION: 400,
  UNAUTHORIZED: 401,
  FORBIDDEN: 403,
  NOT_FOUND: 404,
  CONFLICT: 409,
  RATE_LIMITED: 429,
  INTERNAL: 500,
};
```

`Record<ErrorCode, number>` forces you to handle every code: adding a code without a status is a compile error ([Pick, Omit, Record](../07-utility-types/01-pick-omit-record.md)). Some APIs use `422` for validation errors that are syntactically valid but semantically wrong. Pick one convention and document it.

## A discriminated union of errors

Different errors carry different details. Model that as a union keyed by `code`:

```ts
type ApiError =
  | { code: "VALIDATION"; message: string; issues: { path: string; message: string }[] }
  | { code: "NOT_FOUND"; message: string; resource: string }
  | { code: "CONFLICT"; message: string; conflictingField?: string }
  | { code: "RATE_LIMITED"; message: string; retryAfterSeconds: number }
  | { code: "UNAUTHORIZED" | "FORBIDDEN"; message: string }
  | { code: "INTERNAL"; message: string; requestId: string };

interface ErrorResponse { error: ApiError }
```

On the client, narrowing gives you exactly the details that exist for each code:

```ts
function describe(e: ApiError): string {
  switch (e.code) {
    case "VALIDATION":
      return e.issues.map((i) => `${i.path}: ${i.message}`).join("\n");
    case "NOT_FOUND":
      return `${e.resource} was not found`;
    case "RATE_LIMITED":
      return `Try again in ${e.retryAfterSeconds}s`;
    case "CONFLICT":
    case "UNAUTHORIZED":
    case "FORBIDDEN":
    case "INTERNAL":
      return e.message;
    default: {
      const _exhaustive: never = e;
      return _exhaustive;
    }
  }
}
```

The `never` check fails to compile if a new code is added and not handled ([exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)).

**Be tolerant of unknown codes on the client.** The server may add a new code before the client is updated. Types claim the union is closed, but the wire does not, so add a runtime fallback (`default` branch showing `message`) in production code.

## Validation errors

Validation failures are the error clients handle most, since they drive form feedback. Give every issue a **path** and a **message**:

```ts
interface ValidationIssue {
  path: string;          // "address.postcode", "items.2.quantity"
  message: string;       // human-readable
  code?: string;         // optional machine code: "too_small", "invalid_format"
}
```

Return **all** issues, not just the first, so the client can show them at once. Convert from your validation library's format into this stable shape on the server ([validation recipes](../15-runtime-validation/04-validation-recipes.md)). Do not expose library internals or the schema.

## Problem Details (RFC 9457)

There is an HTTP standard for error bodies: **Problem Details**, with media type `application/problem+json` (RFC 9457, which updates RFC 7807). The standard fields are:

| Field | Meaning |
|---|---|
| `type` | a URI identifying the kind of problem |
| `title` | short, human-readable summary of the type |
| `status` | the HTTP status code |
| `detail` | explanation specific to this occurrence |
| `instance` | a URI identifying this occurrence |

You may add extension members, such as `code` or `errors`:

```ts
interface ProblemDetails {
  type: string;
  title: string;
  status: number;
  detail?: string;
  instance?: string;
  // extensions
  code?: ErrorCode;
  errors?: ValidationIssue[];
}
```

Using it gives you a recognized format that tools understand. Using your own envelope is also fine, as long as it is **consistent** and documented. The key is one shape for every error in the API.

## Sending errors from the server

Centralize the translation from errors to responses in one place ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)):

```ts
app.use((err: unknown, req: Request, res: Response, _next: NextFunction) => {
  const requestId = String(req.headers["x-request-id"] ?? crypto.randomUUID());

  if (err instanceof AppError) {
    return res
      .status(statusFor[err.code])
      .json({ error: { code: err.code, message: err.message, requestId } });
  }

  logger.error({ err, requestId }, "Unhandled error");          // full detail stays in logs
  res.status(500).json({
    error: { code: "INTERNAL", message: "Internal server error", requestId },
  });
});
```

Security rules for error responses (see [security](../23-security/README.md)):

- **Never** include stack traces, SQL, file paths, or library error messages for unexpected errors.
- **Do not reveal whether a username exists** with different messages for "unknown user" and "wrong password".
- Messages for `401`/`403` should not leak which resource or permission check failed in detail.
- Keep the full error and its `cause` chain in **logs**, correlated by the request id.

## Receiving errors in the client

Do not assume a failed response has a JSON body in your format. Gateways, proxies, load balancers, and CDNs return their own `502`/`503`/`504` pages, and network failures never produce a response at all.

```ts
type ApiResult<T> =
  | { ok: true; status: number; data: T }
  | { ok: false; status: number; error: ApiError };

async function readError(res: Response): Promise<ApiError> {
  const contentType = res.headers.get("content-type") ?? "";
  if (contentType.includes("json")) {
    try {
      const body = ErrorResponseSchema.parse(await res.json());    // validate the error body too
      return body.error;
    } catch {
      /* fall through to the generic error */
    }
  }
  return {
    code: "INTERNAL",
    message: `Request failed with status ${res.status}`,
    requestId: res.headers.get("x-request-id") ?? "unknown",
  };
}
```

Principles:

- **Validate error bodies** the same way as success bodies. They are untrusted input too ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).
- **Always produce an `ApiError`**, even if the body is HTML or empty. Callers then handle a single type.
- **Distinguish transport errors** (no response: offline, DNS, timeout, abort) from HTTP errors. A `fetch` call rejects for the first and resolves for the second.
- Return a `Result`-style value rather than throwing, if callers are expected to handle specific failures ([the Result pattern](../11-error-handling/02-result-pattern.md)).

See [typed fetch and API client](./05-typed-fetch-and-api-client.md) for these pieces in a complete client.

## Useful extras

- **`Retry-After` header** with `429` and `503`. Expose it as `retryAfterSeconds` in the body, and parse it in the client (it may be seconds or an HTTP date).
- **Idempotency conflicts:** a `409` or specific code when a repeated idempotency key arrives with a different payload.
- **Localization:** localize on the client from `code`, using `message` as a fallback.
- **Warnings** (non-fatal issues) are better placed in the success response than in an error.

## Important rules and misconceptions

- **`message` is for humans, `code` is for programs.** Never parse messages.
- **A type does not guarantee the server sends it.** Proxies and bugs produce other bodies. Parse defensively.
- **`fetch` does not reject on `404` or `500`.** Check `res.ok` or the status yourself.
- **One shape everywhere.** A mix of error formats forces every client to special-case.
- **Status codes and error codes are different layers.** The status classifies for HTTP infrastructure, the code classifies for your clients.

## Common mistakes

- Returning `200` with `{ success: false }`.
- Different error shapes across endpoints.
- Returning raw exception messages or stack traces to clients.
- Branching on message text.
- Assuming error bodies are always JSON in your format.
- Treating network failures and HTTP errors as the same thing.
- Returning only the first validation issue.
- Adding a new error code without a `default` branch in the client.
- Using `404` for everything that is not a success.

## Debugging

- Log the **request id** on both sides and make it visible in the client's error UI, so a user report maps to a server log line.
- When a client shows a generic error, inspect the **raw response** (status, headers, body) to see whether a proxy produced it.
- If a form does not show field errors, check that the issue `path` format matches what the client expects (dots versus brackets, array indices).
- Replay a failing request with `curl -i` to see headers and body exactly.
- Add contract tests that assert the **shape** of error responses for every endpoint ([contract-first APIs](./06-contract-first-apis.md)).

## Quick summary

- Use one consistent error shape with a stable `code`, a human `message`, optional structured details, and a request id.
- Model errors as a discriminated union keyed by `code`, and map codes to HTTP statuses with a `Record` so none is forgotten.
- Return all validation issues with paths. Convert from library formats into your own stable shape.
- Consider Problem Details (RFC 9457) as a standard format, or keep your own consistent envelope.
- Centralize server-side error translation, never leak internals, and log details by request id.
- On the client, validate error bodies, handle non-JSON failures, separate transport errors from HTTP errors, and tolerate unknown codes.

**Next:** [Typed fetch and API client](./05-typed-fetch-and-api-client.md)