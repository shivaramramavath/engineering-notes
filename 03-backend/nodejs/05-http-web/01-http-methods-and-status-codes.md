# HTTP Methods & Status Codes

The vocabulary every HTTP API is built from — what action a request represents, and what outcome a response communicates.

## HTTP Methods

| Method   | Meaning                                               | Has a body? | Safe to retry?                                                    |
| -------- | ----------------------------------------------------- | ----------- | ----------------------------------------------------------------- |
| `GET`    | Retrieve a resource                                   | No          | Yes — has no side effects (idempotent)                            |
| `POST`   | Create a resource, or trigger a non-idempotent action | Yes         | No — retrying can create duplicates                               |
| `PUT`    | Replace a resource entirely                           | Yes         | Yes — same result no matter how many times                        |
| `PATCH`  | Partially update a resource                           | Yes         | Usually, but depends on the specific patch                        |
| `DELETE` | Remove a resource                                     | Sometimes   | Yes — deleting an already-deleted resource has the same end state |

### `PUT` vs `PATCH`: the distinction that trips people up

```http
PUT /users/42
{ "name": "Alice", "email": "alice@example.com", "role": "admin" }
```

`PUT` conceptually **replaces the entire resource** — any field not included is expected to be cleared or reset to a default, since you're sending the complete new representation.

```http
PATCH /users/42
{ "email": "alice@newdomain.com" }
```

`PATCH` **updates only the given fields**, leaving everything else on the resource untouched. In practice, many real-world APIs use `PATCH`-like partial-update semantics even under a `PUT` route out of convenience — but the technically correct distinction is worth knowing, especially when designing a new API (`09-api-development/01-rest-api-design.md`).

### Idempotency

An operation is **idempotent** if making the same request multiple times has the same effect as making it once. `GET`, `PUT`, and `DELETE` are meant to be idempotent by definition; `POST` generally is not (submitting the same "create an order" request twice usually creates two orders). This matters directly for retry logic — safely retrying a failed request requires knowing whether repeating it could cause harm (see `09-api-development/06-idempotency.md` for handling this properly even for non-idempotent operations like payments).

---

## Status Codes

Grouped by their first digit, which immediately signals the category of outcome:

```
1xx  →  Informational (rare in typical API work)
2xx  →  Success
3xx  →  Redirection
4xx  →  Client error (the request was wrong)
5xx  →  Server error (the server failed)
```

### 2xx — Success

| Code  | Name       | Use for                                                                                       |
| ----- | ---------- | --------------------------------------------------------------------------------------------- |
| `200` | OK         | A successful `GET`/`PUT`/`PATCH`, or a `POST` that doesn't create a new resource              |
| `201` | Created    | A successful `POST` that created a new resource — include its location/data                   |
| `202` | Accepted   | The request was accepted for async processing, not completed yet (see `11-async-processing/`) |
| `204` | No Content | Success, with nothing to return — a common choice for `DELETE`                                |

### 3xx — Redirection

| Code  | Name              | Use for                                                                          |
| ----- | ----------------- | -------------------------------------------------------------------------------- |
| `301` | Moved Permanently | The resource now lives at a new URL, permanently                                 |
| `304` | Not Modified      | The client's cached version is still valid — see `05-caching-and-compression.md` |

### 4xx — Client Error

| Code  | Name                 | Use for                                                                      |
| ----- | -------------------- | ---------------------------------------------------------------------------- |
| `400` | Bad Request          | Malformed request — invalid JSON, missing required fields                    |
| `401` | Unauthorized         | No valid authentication provided at all                                      |
| `403` | Forbidden            | Authenticated, but not allowed to do this                                    |
| `404` | Not Found            | The resource doesn't exist                                                   |
| `409` | Conflict             | The request conflicts with the current state (e.g. a duplicate unique field) |
| `422` | Unprocessable Entity | Well-formed request, but fails validation (e.g. an invalid email format)     |
| `429` | Too Many Requests    | Rate limit exceeded — see `08-authentication-security/06-rate-limiting.md`   |

### `401` vs `403`: the distinction that trips people up

```
401 Unauthorized  →  "I don't know who you are" — no valid credentials at all
403 Forbidden      →  "I know who you are, and you're not allowed to do this"
```

A logged-out user hitting a protected route gets `401`; a logged-in user without admin rights hitting an admin-only route gets `403`. Despite its name, `401` is really about _authentication_, and `403` is about _authorization_ — a very common source of confusion given how similarly named they are.

### `400` vs `422`: a subtler distinction

```
400  →  the request itself is malformed (invalid JSON, wrong content type)
422  →  the request is well-formed, but the data fails business/validation rules
```

Many real-world APIs use `400` for both cases, which is defensible — the distinction is a nicety some API style guides insist on rather than a hard rule.

### 5xx — Server Error

| Code  | Name                  | Use for                                                                          |
| ----- | --------------------- | -------------------------------------------------------------------------------- |
| `500` | Internal Server Error | A generic, unexpected server-side failure                                        |
| `502` | Bad Gateway           | A reverse proxy (see the Nginx docs) got an invalid response from your app       |
| `503` | Service Unavailable   | The server is temporarily unable to handle requests (overloaded, in maintenance) |
| `504` | Gateway Timeout       | A reverse proxy's upstream (your app) took too long to respond                   |

A `5xx` should generally indicate **your** system's fault, not the client's — receiving a `500` for bad user input is a common API design mistake; that's what the `4xx` range exists for.

---

## Choosing the right combination in practice

```
GET    /users        → 200 (list of users)
GET    /users/42       → 200 (found) or 404 (not found)
POST   /users            → 201 (created) or 400/422 (invalid input)
PUT    /users/42           → 200 (replaced) or 404 (doesn't exist)
PATCH  /users/42             → 200 (updated) or 404
DELETE /users/42               → 204 (deleted) or 404
```

A more thorough breakdown of the full 400+ status code list (including less common ones like `418`) lives in `http-errors/00_status.md` — this file focuses on the ones that come up constantly in everyday API work.

## Common mistakes

- **Returning `200` for every response, with the actual error inside the JSON body** — defeats the purpose of status codes, and breaks tooling (HTTP clients, monitoring, caching) that relies on them being accurate.
- **Using `401` when you mean `403`, or vice versa** — a meaningful distinction for how a client should respond (re-authenticate vs. give up and show "not allowed").
- **Returning `500` for invalid user input** — that's a `4xx` situation; reserve `5xx` for genuine server-side failures.
- **Using `GET` for an action that changes state** — breaks the assumption that `GET` requests are safe to retry, cache, and prefetch.

## Quick summary

- `GET`/`PUT`/`DELETE` are meant to be idempotent; `POST` generally is not — this matters for safe retries
- `PUT` replaces a resource entirely; `PATCH` updates only the given fields
- `401` = no valid identity at all; `403` = identified, but not allowed — a very commonly confused pair
- `4xx` = the client's fault; `5xx` = the server's fault — keep that boundary consistent in your API
- `201`/`204` communicate more specific success outcomes than a generic `200`

## Next

**`02-headers-and-content-negotiation.md`** covers the headers that carry metadata alongside every request and response.
