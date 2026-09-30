# API Development

How to design, build, and evolve HTTP APIs that other developers (including future you) can use without reading your source code.

## Why API design deserves its own section

Writing a route handler is easy. Writing an API that stays consistent across 80 endpoints, survives three years of change without breaking clients, and fails in predictable ways is a different skill. Once an API has users, every decision becomes a **contract**: renaming a field or changing a status code can break someone's production app.

The goal of this section is to make those decisions deliberately, once, and consistently.

---

## What makes an API good

| Quality | What it means | Where it's covered |
|---|---|---|
| **Predictable** | Same conventions everywhere: naming, status codes, error shape | `01`, `05` |
| **Evolvable** | You can add features without breaking existing clients | `02` |
| **Efficient** | Clients fetch what they need, in pages, with filters | `02`, `03` |
| **Safe** | Bad input is rejected early and clearly | `04` |
| **Debuggable** | Errors explain what went wrong and how to fix it | `05` |
| **Reliable** | Retries don't cause duplicate charges or duplicate records | `06` |
| **Integrable** | You can push events to other systems, and receive theirs securely | `07` |

---

## What's in this section

| File | What you learn |
|---|---|
| `01-rest-api-design.md` | Resources, URLs, HTTP methods, status codes, response shapes |
| `02-versioning-and-pagination.md` | Changing an API safely; offset vs cursor pagination |
| `03-filtering-and-sorting.md` | Query-string conventions and safe implementation |
| `04-validation.md` | Validating bodies, params, and queries with `zod` |
| `05-error-responses.md` | A consistent error format and a central error handler |
| `06-idempotency.md` | Making POST requests safe to retry |
| `07-webhooks.md` | Sending and receiving webhooks securely |

Read in order. Each file assumes the conventions established in the previous ones, and the running example (a small `posts` and `users` API) carries through.

---

## Prerequisites

- `05-http-web/01-http-methods-and-status-codes.md` — the vocabulary REST is built on
- `05-http-web/02-headers-and-content-negotiation.md` — `Content-Type`, `Accept`, custom headers
- `06-express/` — routing, middleware, controllers, and error handling
- `07-databases/` — queries, indexes (they matter a great deal for pagination and filtering)
- `08-authentication-security/` — every API in this section assumes authentication and rate limiting are in place

---

## The running example

Most snippets use a simple blog-style API so concepts stay concrete:

```
GET    /api/v1/posts              list posts (paginated, filterable, sortable)
POST   /api/v1/posts              create a post
GET    /api/v1/posts/:id          get one post
PATCH  /api/v1/posts/:id          partially update a post
DELETE /api/v1/posts/:id          delete a post
GET    /api/v1/users/:id/posts    posts belonging to a user
```

It's the same shape as the project in `20-projects/04-blog-api/`.

---

## A recommended project layout

```
src/
├── routes/            URL → controller wiring            (06-express/01)
├── controllers/       HTTP in, HTTP out                  (06-express/03)
├── services/          business logic                     (10-architecture/03)
├── schemas/           zod schemas for validation         (04-validation.md)
├── middleware/
│   ├── validate.js    runs schemas on requests
│   ├── idempotency.js
│   └── errorHandler.js
├── utils/
│   ├── AppError.js    error classes                      (05-error-responses.md)
│   ├── pagination.js  shared pagination helpers          (02-versioning-and-pagination.md)
│   └── query.js       filter/sort parsing                (03-filtering-and-sorting.md)
└── app.js
```

The idea: **controllers stay thin**. Validation, pagination parsing, error formatting, and idempotency are reusable pieces, not copy-pasted code in every handler.

---

## Principles that run through every file

1. **Consistency beats cleverness.** A boring, uniform API is easier to use than a clever one.
2. **Be strict in what you send, reasonably strict in what you accept.** Validate input hard; return the same shape every time.
3. **Never break existing clients silently.** Additive changes are free; removals and renames need a new version or a deprecation period.
4. **Errors are part of the API.** Clients write code against your error format just like your success format.
5. **Assume requests will be retried, duplicated, and reordered.** Networks are unreliable; design for it.
6. **Document as you build.** An API nobody can discover may as well not exist (consider an OpenAPI spec).

## Next

**`01-rest-api-design.md`** starts with the fundamentals: how to model resources, name URLs, and choose the right methods and status codes.
