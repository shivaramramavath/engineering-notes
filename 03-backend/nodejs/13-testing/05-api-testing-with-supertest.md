# API Testing & Mocking

Driving your whole Express app through HTTP, handling authentication in tests, faking third-party services, and testing the awkward parts: middleware, uploads, webhooks, and contracts.

> Examples use the Jest-style API shared with Vitest (`01-unit-and-integration-testing.md`). Replace `jest` with `vi` for Vitest.

## What an API test is

An **API test** (also called a component or functional test) sends real HTTP requests to your real application code (routing, middleware, validation, controllers, services, repositories) and checks the HTTP response. Only the *edges* of your system are faked: payment providers, email services, and anything else outside your control.

```
 test ──HTTP──▶ [ middleware ▶ routes ▶ controller ▶ service ▶ repository ] ──▶ test database
                      all real                                                      (real)
                                         │
                                         └──▶ Stripe, SendGrid, ... → FAKED (nock / MSW / injected fakes)
```

It's the highest-value test type for most APIs: one test exercises validation, authentication, authorization, error formatting, serialization, and SQL together. If these tests pass, the endpoint works for a real client.

---

## Setup: `supertest` and a testable app

The prerequisite is the split from `10-architecture/`: **`app.js` builds the app and doesn't call `listen`; `server.js` starts it.**

```js
// src/app.js
import express from "express";
import { buildContainer } from "./container.js";

export function buildApp(overrides = {}) {
  const container = buildContainer(overrides);          // dependency injection (10-architecture/04-dependency-injection.md)
  const app = express();
  app.use(express.json({ limit: "100kb" }));
  app.use("/api/v1", container.routes);
  app.use(notFoundHandler);
  app.use(errorHandler);
  return app;
}
```

```bash
npm install -D supertest
```

```js
import request from "supertest";
import { buildApp } from "../src/app.js";

const app = buildApp();

test("GET /health returns 200", async () => {
  const res = await request(app).get("/health");

  expect(res.status).toBe(200);
});
```

`supertest` receives the `app` and **starts it on an ephemeral port internally**, so there's no port to manage and nothing to close. (If you pass an already-listening server, close it in `afterAll`.)

### The request API

```js
const res = await request(app)
  .post("/api/v1/posts")
  .set("Authorization", `Bearer ${token}`)              // headers
  .set("Accept", "application/json")
  .query({ draft: "true" })                              // ?draft=true
  .send({ title: "Hello", body: "World" });              // JSON body (sets Content-Type automatically)

res.status;        // 201
res.body;          // parsed JSON
res.headers;       // lowercase header names: res.headers.location
res.text;          // raw body as a string
res.type;          // "application/json"
```

`supertest` also has built-in `.expect(...)` shortcuts:

```js
await request(app)
  .get("/api/v1/posts/42")
  .expect(200)
  .expect("Content-Type", /json/);
```

Plain `expect(res.status).toBe(200)` gives clearer failure output with Jest/Vitest and is generally preferred (a `.expect(200)` failure doesn't show the response body unless you add logging). If you want the body on failure, assert on `res.status` *and* `res.body` together:

```js
expect({ status: res.status, body: res.body }).toMatchObject({ status: 201 });
```

### Don't test through a mocked app

If you replace so much of the app with fakes that no real routing, validation, or database is involved, you're back to a unit test with extra steps. API tests earn their keep by involving the **real** stack.

---

## Authentication in tests

You need requests that are logged in as specific users. Don't bypass auth: it's the most important thing to test (`08-authentication-security/`). Create real tokens instead.

### A token helper

```js
// test/helpers/auth.js
import jwt from "jsonwebtoken";

export function tokenFor(user, { expiresIn = "15m", secret = process.env.JWT_ACCESS_SECRET } = {}) {
  return jwt.sign({ sub: user.id, role: user.role }, secret, { algorithm: "HS256", expiresIn });
}

export const authHeader = (user) => ({ Authorization: `Bearer ${tokenFor(user)}` });
```

```js
test("GET /me returns the current user", async () => {
  const user = await createUser({ name: "Sam" });                     // inserted in the test DB (03)

  const res = await request(app).get("/api/v1/me").set(authHeader(user));

  expect(res.status).toBe(200);
  expect(res.body.data).toMatchObject({ id: user.id, name: "Sam" });
  expect(res.body.data).not.toHaveProperty("passwordHash");           // never leak secrets
});
```

Set a **test secret** in your test environment (`test/setup.js` or `.env.test`) so tokens are valid without real keys.

### Cookie / session auth

`request.agent(app)` **persists cookies across requests**, like a browser:

```js
test("logging in sets a session cookie, and /me then works", async () => {
  const agent = request.agent(app);

  await agent.post("/api/v1/auth/login").send({ email: "a@b.com", password: "correct-password" }).expect(200);
  const me = await agent.get("/api/v1/me");

  expect(me.status).toBe(200);
});

test("the session cookie is HttpOnly and SameSite", async () => {
  const res = await request(app).post("/api/v1/auth/login").send(creds);
  const cookie = res.headers["set-cookie"].find((c) => c.startsWith("sid="));
  expect(cookie).toMatch(/HttpOnly/i);
  expect(cookie).toMatch(/SameSite=Lax/i);
});
```

(`Secure` cookies aren't sent over plain HTTP by browsers, but `supertest`'s agent will store them. If your cookie logic depends on `trust proxy`, set `X-Forwarded-Proto: https` in the request.)

### A convenient test client

```js
// test/helpers/client.js
export function makeClient(app, user) {
  const auth = user ? authHeader(user) : {};
  return {
    get: (url) => request(app).get(url).set(auth),
    post: (url, body) => request(app).post(url).set(auth).send(body),
    patch: (url, body) => request(app).patch(url).set(auth).send(body),
    delete: (url) => request(app).delete(url).set(auth),
  };
}

const api = makeClient(app, alice);
const res = await api.post("/api/v1/posts", { title: "Hi", body: "..." });
```

---

## What to test for each endpoint

A solid endpoint test suite covers a predictable checklist. Not every box for every route, but think through each:

| Scenario | Expected | Why it matters |
|---|---|---|
| **Happy path** | `200`/`201`, correct body shape | The feature works |
| **Persistence** | The data is actually saved/changed (query the DB or follow-up `GET`) | A `201` that saved nothing is a bug |
| **No credentials** | `401`, `code: "unauthenticated"` | Auth is actually enforced |
| **Expired / tampered / wrong-secret token** | `401` | `08-authentication-security/02-jwt-and-tokens.md` |
| **Wrong role** | `403` | Role checks work |
| **Someone else's resource** | `404` (or `403`) | **IDOR protection**, the most valuable security test |
| **Invalid body** (missing, wrong type, too long) | `422`/`400` + field errors | `09-api-development/04-validation.md` |
| **Unknown/extra fields** | Rejected or ignored | Mass assignment (`08-authentication-security/05-common-vulnerabilities.md`) |
| **Not found** | `404` | Correct error format |
| **Conflict / duplicate** | `409` | Uniqueness enforced |
| **Malformed JSON** | `400` | Error handler maps parse errors |
| **Pagination / filtering / sorting** | Correct page, cursor, order, caps | `09-api-development/02-versioning-and-pagination.md` |
| **Response never leaks secrets** | No `passwordHash`, tokens, internal IDs | Data exposure |
| **Error format is consistent** | `{ error: { code, message, requestId } }` | Clients depend on it |

### Example: a full endpoint suite

```js
describe("POST /api/v1/posts", () => {
  let alice;
  beforeEach(async () => { alice = await createUser(); });

  test("creates a post and returns 201 with a Location header", async () => {
    const res = await request(app)
      .post("/api/v1/posts")
      .set(authHeader(alice))
      .send({ title: "Hello world", body: "First post" });

    expect(res.status).toBe(201);
    expect(res.headers.location).toBe(`/api/v1/posts/${res.body.data.id}`);
    expect(res.body.data).toMatchObject({ title: "Hello world", author: { id: alice.id } });

    const saved = await db.query("SELECT * FROM posts WHERE id = $1", [res.body.data.id]);    // really persisted
    expect(saved.rowCount).toBe(1);
  });

  test("401 without a token", async () => {
    const res = await request(app).post("/api/v1/posts").send({ title: "x", body: "y" });
    expect(res.status).toBe(401);
    expect(res.body.error.code).toBe("unauthenticated");
  });

  test.each([
    ["missing title", { body: "b" }, "title"],
    ["title too short", { title: "ab", body: "b" }, "title"],
    ["body wrong type", { title: "Hello", body: 42 }, "body"],
  ])("422 for %s", async (_name, payload, field) => {
    const res = await request(app).post("/api/v1/posts").set(authHeader(alice)).send(payload);

    expect(res.status).toBe(422);
    expect(res.body.error.code).toBe("validation_error");
    expect(res.body.error.details).toEqual(expect.arrayContaining([expect.objectContaining({ field })]));
  });

  test("ignores attempts to set the author", async () => {
    const bob = await createUser();
    const res = await request(app)
      .post("/api/v1/posts").set(authHeader(alice))
      .send({ title: "Hello", body: "b", authorId: bob.id });                      // mass-assignment attempt

    expect(res.status === 201 ? res.body.data.author.id : alice.id).toBe(alice.id);   // either rejected or ignored, never honored
  });
});

describe("GET /api/v1/posts/:id", () => {
  test("returns 404 for another user's private post (no IDOR)", async () => {
    const [alice, bob] = [await createUser(), await createUser()];
    const post = await createPost({ authorId: bob.id, visibility: "private" });

    const res = await request(app).get(`/api/v1/posts/${post.id}`).set(authHeader(alice));

    expect(res.status).toBe(404);                      // not 200, not 403: don't confirm it exists
  });
});
```

### Test pagination and ordering with real data

```js
test("cursor pagination returns each post exactly once, newest first", async () => {
  const user = await createUser();
  await Promise.all(Array.from({ length: 25 }, (_, i) => createPost({ authorId: user.id, title: `Post ${i}` })));

  const seen = [];
  let cursor;
  do {
    const res = await request(app).get("/api/v1/posts").query({ limit: 10, ...(cursor && { cursor }) }).set(authHeader(user));
    seen.push(...res.body.data.map((p) => p.id));
    cursor = res.body.meta.nextCursor;
  } while (cursor);

  expect(seen).toHaveLength(25);
  expect(new Set(seen).size).toBe(25);                 // no duplicates, no gaps
});

test("limit is capped at the maximum", async () => {
  const res = await request(app).get("/api/v1/posts?limit=100000").set(authHeader(user));
  expect(res.body.meta.limit).toBeLessThanOrEqual(100);
});
```

### Test error handling and the format

```js
test("malformed JSON returns 400 with the standard error shape", async () => {
  const res = await request(app)
    .post("/api/v1/posts")
    .set(authHeader(alice))
    .set("Content-Type", "application/json")
    .send('{"title": "oops"');                          // invalid JSON (sent as a raw string)

  expect(res.status).toBe(400);
  expect(res.body.error).toMatchObject({ code: "invalid_json" });
});

test("unknown routes return the standard 404", async () => {
  const res = await request(app).get("/api/v1/does-not-exist");
  expect(res.status).toBe(404);
  expect(res.body.error.code).toBe("route_not_found");
});

test("unexpected failures return 500 without leaking internals", async () => {
  const app = buildApp({ postRepository: { create: async () => { throw new Error("db password is hunter2"); } } });
  const res = await request(app).post("/api/v1/posts").set(authHeader(alice)).send(validBody);

  expect(res.status).toBe(500);
  expect(JSON.stringify(res.body)).not.toContain("hunter2");
  expect(res.body.error.code).toBe("internal_error");
});
```

Injecting a failing dependency is the cleanest way to force the failure path (`10-architecture/04-dependency-injection.md`). (`09-api-development/05-error-responses.md` defines the format asserted here.)

---

## Mocking third-party services

Your tests must **not** call real Stripe, SendGrid, or Slack: they're slow, flaky, cost money, have rate limits, and need credentials. Replace them at the boundary. There are three levels, and the right one depends on what you want to prove.

### Level 1: Inject a fake (best when you own the boundary)

If the service goes through your own interface (`mailer`, `paymentGateway`), swap it with dependency injection:

```js
function makeFakeMailer() {
  const sent = [];
  return {
    async sendWelcome(to) { sent.push({ kind: "welcome", to }); },
    async sendOrderConfirmation(to, order) { sent.push({ kind: "order", to, orderId: order.id }); },
    sent,
  };
}

test("registering sends a welcome email", async () => {
  const mailer = makeFakeMailer();
  const app = buildApp({ mailer });

  await request(app).post("/api/v1/auth/register").send({ email: "a@b.com", name: "A", password: "long-enough-password" }).expect(201);

  expect(mailer.sent).toEqual([{ kind: "welcome", to: "a@b.com" }]);
});

test("a failing mailer doesn't fail registration", async () => {
  const mailer = { sendWelcome: async () => { throw new Error("SMTP down"); } };
  const app = buildApp({ mailer });

  const res = await request(app).post("/api/v1/auth/register").send(validBody);

  expect(res.status).toBe(201);                         // the user was created; email is best-effort
});
```

Simple, fast, no HTTP interception. The downside: it doesn't test your actual SDK usage or the HTTP calls it makes.

### Level 2: Intercept HTTP (best for testing your real client code)

When you want your real integration code (the SDK, `fetch`, or `axios`) to run, but against a pretend server, **intercept HTTP at the network layer.**

#### `nock`

```bash
npm install -D nock
```

```js
import nock from "nock";

beforeAll(() => {
  nock.disableNetConnect();                              // any un-mocked outgoing request FAILS loudly
  nock.enableNetConnect("127.0.0.1");                    // ...but allow supertest to reach the app on localhost
});
afterEach(() => nock.cleanAll());                        // don't leak interceptors between tests
afterAll(() => nock.enableNetConnect());

test("creates a charge via the payment provider", async () => {
  const scope = nock("https://api.payments.example")
    .post("/v1/charges", { amount: 5000, currency: "usd" })        // matches the request body (partial match with a function or object)
    .matchHeader("authorization", /^Bearer sk_test_/)
    .reply(200, { id: "ch_123", status: "succeeded" });

  const res = await request(app).post("/api/v1/payments").set(authHeader(user)).send({ amountCents: 5000 });

  expect(res.status).toBe(201);
  expect(res.body.data.providerChargeId).toBe("ch_123");
  expect(scope.isDone()).toBe(true);                     // the expected call was actually made
});

test("maps a provider timeout to 504", async () => {
  nock("https://api.payments.example").post("/v1/charges").delayConnection(5000).reply(200, {});
  const res = await request(app).post("/api/v1/payments").set(authHeader(user)).send({ amountCents: 5000 });
  expect(res.status).toBe(504);                          // (your client must set a shorter timeout)
});

test("retries transient provider failures", async () => {
  const scope = nock("https://api.payments.example")
    .post("/v1/charges").reply(503)                      // first attempt fails
    .post("/v1/charges").reply(200, { id: "ch_1", status: "succeeded" });   // retry succeeds
  const res = await request(app).post("/api/v1/payments").set(authHeader(user)).send({ amountCents: 5000 });
  expect(res.status).toBe(201);
  expect(scope.isDone()).toBe(true);
});

test("maps provider errors to a safe 502", async () => {
  nock("https://api.payments.example").post("/v1/charges").reply(500, { error: "internal db leak" });
  const res = await request(app).post("/api/v1/payments").set(authHeader(user)).send({ amountCents: 5000 });
  expect(res.status).toBe(502);
  expect(JSON.stringify(res.body)).not.toContain("leak");
});
```

Notes:

- `nock.disableNetConnect()` is the most valuable line: it guarantees tests can't accidentally hit the real internet. Remember to allow localhost, or `supertest` can't reach your app.
- Recent `nock` versions intercept Node's built-in `fetch` as well as `http`/`https` clients. Check the version you install, since older ones only patched `http`/`https`.
- Each interceptor is **consumed once** by default. Add `.persist()` to answer repeatedly, or `.times(3)`.
- Simulate failures: `.replyWithError("ECONNRESET")`, `.delay(ms)`, `.reply(429, {}, { "Retry-After": "1" })`.

#### Mock Service Worker (MSW)

MSW defines handlers in a style that can be **shared between tests and browser development**, and it intercepts `fetch`, `http`, and `XMLHttpRequest`.

```bash
npm install -D msw
```

```js
// test/mocks/server.js
import { setupServer } from "msw/node";
import { http, HttpResponse } from "msw";

export const handlers = [
  http.post("https://api.payments.example/v1/charges", async ({ request }) => {
    const body = await request.json();
    return HttpResponse.json({ id: "ch_123", status: "succeeded", amount: body.amount });
  }),
  http.get("https://api.geo.example/lookup", () => HttpResponse.json({ country: "IN" })),
];

export const server = setupServer(...handlers);
```

```js
// test/setup.js
import { server } from "./mocks/server.js";

beforeAll(() => server.listen({ onUnhandledRequest: "error" }));   // un-mocked requests fail the test
afterEach(() => server.resetHandlers());                             // drop per-test overrides
afterAll(() => server.close());
```

```js
// a test overriding the default handler for ONE scenario
import { server } from "./mocks/server.js";
import { http, HttpResponse } from "msw";

test("handles the payment provider being down", async () => {
  server.use(http.post("https://api.payments.example/v1/charges", () => new HttpResponse(null, { status: 503 })));

  const res = await request(app).post("/api/v1/payments").set(authHeader(user)).send({ amountCents: 5000 });

  expect(res.status).toBe(502);
});
```

MSW needs `onUnhandledRequest` configured thoughtfully: the `"error"` setting also flags requests from `supertest` to your own app unless you let localhost through (`http.all("http://127.0.0.1:*/*", passthrough)` style handlers, or use `"warn"`). Check the MSW docs for your version.

#### Which to use

| | `nock` | MSW | Plain DI fake |
|---|---|---|---|
| Intercepts | Node `http`/`https` (and `fetch` in current versions) | `fetch`, `http`, XHR | Nothing: you replace the object |
| Tests your real HTTP client code | ✅ | ✅ | ❌ |
| Sharing handlers with frontend dev/tests | ❌ | ✅ | ❌ |
| Assertion API on requests | Rich (`scope.isDone`, body/header matching) | Handler-based (inspect `request`) | Whatever you build |
| Best for | Backend-only teams, precise request expectations | Full-stack teams, shared mocks | Your own abstractions |

Either works. Pick one per project and use it consistently. There's also `undici`'s built-in `MockAgent` if you want zero dependencies for `fetch`/`undici`.

### Level 3: A sandbox or the real thing (rarely, and not in the main suite)

Some providers offer **test mode/sandbox** endpoints (Stripe test keys, Twilio test credentials). A small, separate suite of **contract/smoke tests** that call the sandbox helps catch drift when the provider changes their API. Keep it out of the per-commit run (slow, flaky, rate-limited) and run it nightly.

### When to mock and when not to

| Dependency | Approach |
|---|---|
| Your own database | **Real** (test DB, `03`) |
| Redis / queue (cheap to run locally) | **Real** (container), or a fake via DI for unit tests |
| Third-party HTTP APIs | **Fake** (DI or `nock`/MSW) |
| Email / SMS / push | **Fake** (DI) and assert on what would have been sent |
| Payments | **Fake** with `nock`/MSW; separate sandbox tests |
| Clock / randomness | **Inject** or fake timers |
| Filesystem | Temp directories |
| Your own services | Real in the API tests; fake only at the process boundary |

**Fake what you can't control or what's slow. Use the real thing for everything you own.**

---

## Testing middleware in isolation

Most middleware is exercised through API tests, but pure-logic middleware (auth, role checks, request IDs) benefits from fast unit tests with fake `req`/`res`/`next`:

```js
// middleware/requireRole.js
export const requireRole = (...roles) => (req, res, next) => {
  if (!req.user) return next(new AppError(401, "unauthenticated", "Authentication required"));
  if (!roles.includes(req.user.role)) return next(new AppError(403, "forbidden", "Insufficient role"));
  next();
};
```

```js
describe("requireRole", () => {
  const run = (user) => {
    const req = { user };
    const next = jest.fn();
    requireRole("admin")(req, {}, next);
    return next;
  };

  test("calls next() with no error for an allowed role", () => {
    expect(run({ role: "admin" })).toHaveBeenCalledWith();
  });

  test("forwards a 403 error for a disallowed role", () => {
    const next = run({ role: "user" });
    expect(next.mock.calls[0][0]).toMatchObject({ status: 403, code: "forbidden" });
  });

  test("forwards a 401 error when there is no user", () => {
    expect(run(undefined).mock.calls[0][0]).toMatchObject({ status: 401 });
  });
});
```

For middleware that uses more of `req`/`res` (headers, `res.status().json()`), `node-mocks-http` provides realistic fakes:

```js
import httpMocks from "node-mocks-http";

const req = httpMocks.createRequest({ method: "GET", url: "/x", headers: { authorization: "Bearer abc" } });
const res = httpMocks.createResponse();
await authenticate(req, res, next);
expect(res._getStatusCode()).toBe(401);
expect(res._getJSONData()).toMatchObject({ error: { code: "unauthenticated" } });
```

Or sidestep fakes entirely by mounting the middleware on a tiny throwaway Express app and calling it with `supertest`:

```js
const app = express();
app.get("/admin-only", fakeAuth({ role: "user" }), requireRole("admin"), (req, res) => res.sendStatus(200));
app.use(errorHandler);
expect((await request(app).get("/admin-only")).status).toBe(403);
```

---

## Rate limiting, security headers, and other cross-cutting behavior

```js
test("login is rate limited after repeated failures", async () => {
  const app = buildApp({ rateLimits: { login: { limit: 3, windowMs: 60_000 } } });     // low limit for the test, injected via config

  for (let i = 0; i < 3; i++) {
    await request(app).post("/api/v1/auth/login").send({ email: "a@b.com", password: "wrong" }).expect(401);
  }
  const res = await request(app).post("/api/v1/auth/login").send({ email: "a@b.com", password: "wrong" });

  expect(res.status).toBe(429);
  expect(res.headers["retry-after"]).toBeDefined();
});

test("sets security headers", async () => {
  const res = await request(app).get("/health");
  expect(res.headers["x-content-type-options"]).toBe("nosniff");
  expect(res.headers["x-powered-by"]).toBeUndefined();
  expect(res.headers["strict-transport-security"]).toBeDefined();
});

test("CORS only allows configured origins", async () => {
  const ok = await request(app).options("/api/v1/posts").set("Origin", "https://app.example.com").set("Access-Control-Request-Method", "POST");
  expect(ok.headers["access-control-allow-origin"]).toBe("https://app.example.com");

  const bad = await request(app).options("/api/v1/posts").set("Origin", "https://evil.example").set("Access-Control-Request-Method", "POST");
  expect(bad.headers["access-control-allow-origin"]).toBeUndefined();
});
```

(Make limits and windows **configurable** so tests can use tiny values instead of sending thousands of requests. A fresh `buildApp()` per test also gives each test a fresh in-memory limiter. With a Redis store, give the limiter a unique key prefix per test or flush between tests.) See `08-authentication-security/06-rate-limiting.md` and `07-helmet.md`.

---

## File uploads

```js
import path from "node:path";

test("uploads an avatar", async () => {
  const res = await request(app)
    .post("/api/v1/me/avatar")
    .set(authHeader(user))
    .attach("file", Buffer.from("fake-image-bytes"), { filename: "a.png", contentType: "image/png" });
    // or attach from disk: .attach("file", path.join(__dirname, "fixtures/avatar.png"))

  expect(res.status).toBe(201);
  expect(res.body.data.url).toMatch(/^https:\/\//);
});

test("rejects files over the size limit", async () => {
  const big = Buffer.alloc(6 * 1024 * 1024);                                     // 6 MB; limit is 5 MB
  const res = await request(app).post("/api/v1/me/avatar").set(authHeader(user)).attach("file", big, "big.png");
  expect(res.status).toBe(413);
});

test("rejects disallowed file types even if the extension lies", async () => {
  const res = await request(app).post("/api/v1/me/avatar").set(authHeader(user))
    .attach("file", Buffer.from("<?php echo 1;"), { filename: "evil.png", contentType: "image/png" });
  expect(res.status).toBe(415);                                                  // content sniffed, not trusted from the name
});
```

Fake the object-storage client (S3) by injection and assert on what was stored. See `06-express/07-file-upload.md`.

---

## Webhooks

Testing *receiving* webhooks means constructing correctly signed (and incorrectly signed) requests (`09-api-development/07-webhooks.md`).

```js
// test/helpers/webhook.js
import crypto from "node:crypto";

export function signedWebhook({ secret, payload, timestamp = Math.floor(Date.now() / 1000) }) {
  const body = JSON.stringify(payload);                                          // the EXACT bytes that get signed AND sent
  const signature = crypto.createHmac("sha256", secret).update(`${timestamp}.${body}`).digest("hex");
  return { body, header: `t=${timestamp},v1=${signature}` };
}
```

```js
const secret = "whsec_test";

test("accepts a validly signed event and enqueues it", async () => {
  const queue = makeFakeQueue();
  const app = buildApp({ webhookQueue: queue, webhookSecret: secret });
  const { body, header } = signedWebhook({ secret, payload: { id: "evt_1", type: "payment.succeeded" } });

  const res = await request(app)
    .post("/webhooks/provider")
    .set("Content-Type", "application/json")
    .set("Webhook-Signature", header)
    .send(body);                                                                  // a STRING: preserves the exact bytes

  expect(res.status).toBe(200);
  expect(queue.jobs).toHaveLength(1);
});

test("rejects a bad signature", async () => {
  const { body } = signedWebhook({ secret, payload: { id: "evt_1" } });
  const res = await request(app).post("/webhooks/provider").set("Content-Type", "application/json")
    .set("Webhook-Signature", "t=1,v1=deadbeef").send(body);
  expect(res.status).toBe(401);
});

test("rejects a replayed old timestamp", async () => {
  const { body, header } = signedWebhook({ secret, payload: { id: "evt_1" }, timestamp: Math.floor(Date.now() / 1000) - 3600 });
  const res = await request(app).post("/webhooks/provider").set("Content-Type", "application/json").set("Webhook-Signature", header).send(body);
  expect(res.status).toBe(401);
});

test("duplicate deliveries are acknowledged but processed once", async () => {
  const { body, header } = signedWebhook({ secret, payload: { id: "evt_dup", type: "payment.succeeded" } });
  const send = () => request(app).post("/webhooks/provider").set("Content-Type", "application/json").set("Webhook-Signature", header).send(body);

  expect((await send()).status).toBe(200);
  expect((await send()).status).toBe(200);
  expect(queue.jobs).toHaveLength(1);
});
```

For **sending** webhooks, use `nock` to play the customer's endpoint and assert on the signed request and the retry behavior.

---

## Background jobs triggered by endpoints

Don't run real workers in API tests. Inject a fake queue and assert that the right job was enqueued; test the worker's handler separately as a plain function (`11-async-processing/01-queues-and-bullmq.md`):

```js
test("POST /reports enqueues a job and returns 202", async () => {
  const reportQueue = { jobs: [], add: async function (name, data, opts) { this.jobs.push({ name, data, opts }); return { id: "job-1" }; } };
  const app = buildApp({ reportQueue });

  const res = await request(app).post("/api/v1/reports").set(authHeader(user)).send({ range: "last-30-days" });

  expect(res.status).toBe(202);
  expect(res.headers.location).toBe("/api/v1/reports/jobs/job-1");
  expect(reportQueue.jobs[0]).toMatchObject({ name: "generate", data: { userId: user.id } });
});
```

## Idempotency

```js
test("a repeated Idempotency-Key replays the first response and creates one payment", async () => {
  const key = crypto.randomUUID();
  const send = () => request(app).post("/api/v1/payments").set(authHeader(user)).set("Idempotency-Key", key).send({ amountCents: 5000 });

  const first = await send();
  const second = await send();

  expect(second.status).toBe(first.status);
  expect(second.body).toEqual(first.body);
  expect(second.headers["idempotent-replayed"]).toBe("true");
  expect(await countPayments(user.id)).toBe(1);
});

test("concurrent duplicates are processed once", async () => {
  const key = crypto.randomUUID();
  await Promise.all(Array.from({ length: 5 }, () =>
    request(app).post("/api/v1/payments").set(authHeader(user)).set("Idempotency-Key", key).send({ amountCents: 5000 })));
  expect(await countPayments(user.id)).toBe(1);                                   // the race is won exactly once
});
```

(`09-api-development/06-idempotency.md`.)

## Realtime

Test Socket.IO handlers with a real `socket.io-client` against a server on port `0`, covering unauthenticated connections, forbidden room joins, and payload validation (`12-realtime/01-websocket-and-socketio.md` and `02-auth-and-rooms.md` have full examples).

---

## Contract testing

API tests prove *your* server behaves as *your* tests expect. A **contract** test proves the server's behavior matches what a *consumer* (or the documentation) expects.

### Validate responses against your OpenAPI spec

If you maintain an OpenAPI document, assert that real responses conform to it. It catches undocumented fields, wrong types, and missing properties:

```bash
npm install -D jest-openapi        # (Vitest users: `chai-openapi-response-validator` or a manual Ajv check)
```

```js
import jestOpenAPI from "jest-openapi";
jestOpenAPI.default("./openapi.yaml");

test("GET /posts/:id matches the documented schema", async () => {
  const res = await request(app).get(`/api/v1/posts/${post.id}`).set(authHeader(user));
  expect(res.status).toBe(200);
  expect(res).toSatisfyApiSpec();
});
```

### Consumer-driven contracts (Pact)

When multiple teams or services consume your API, **Pact** lets each consumer record its expectations (a "pact"), and the provider verifies it satisfies all of them in CI, so a breaking change fails *before* deploy rather than in production (`10-architecture/05-modular-monolith-vs-microservices.md`).

### Snapshot the API surface (cheaply)

At minimum, assert the **shape** of key responses with `toMatchObject` / `toEqual` and asymmetric matchers, so accidental field renames or removals are caught:

```js
expect(res.body).toEqual({
  data: {
    id: expect.any(String),
    title: "Hello",
    author: { id: expect.any(String) },
    createdAt: expect.stringMatching(/^\d{4}-\d{2}-\d{2}T/),
    updatedAt: expect.stringMatching(/^\d{4}-\d{2}-\d{2}T/),
  },
});
```

`toEqual` fails on **extra** fields too, which doubles as a "no accidental data leak" check.

---

## Organizing API tests

```
test/
├── helpers/
│   ├── app.js            ← buildTestApp(): the app wired to the test DB + fakes
│   ├── auth.js           ← tokenFor(), authHeader()
│   ├── builders.js       ← buildUser(), buildPost() (in-memory) and createUser(), createPost() (persisted)
│   ├── db.js             ← connection, reset, transaction helpers (03)
│   └── webhook.js
├── mocks/
│   └── server.js         ← MSW handlers
├── api/
│   ├── auth.api.test.js
│   ├── posts.api.test.js
│   └── orders.api.test.js
└── setup.js              ← nock/MSW setup, test env vars
```

```js
// test/helpers/app.js
import { buildApp } from "../../src/app.js";
import { pool } from "./db.js";

export function buildTestApp(overrides = {}) {
  return buildApp({
    db: pool,                                                       // the test database
    mailer: makeFakeMailer(),
    clock: () => new Date("2026-09-30T10:00:00Z"),
    rateLimits: { login: { limit: 1000, windowMs: 1000 } },        // generous by default; specific tests tighten it
    ...overrides,
  });
}
```

Every test gets the same easy starting point; each overrides only what it needs.

---

## Speed and reliability tips

- **Build the app once per file** (`beforeAll`) when it has no per-test state; rebuild per test when you inject different fakes.
- **Reset data between tests** (`03`), not by rebuilding everything.
- **Run files in parallel** (the default in Jest/Vitest), each with an isolated database or schema (`03`).
- **Never `sleep`.** Poll with a timeout (`waitFor`) or make the thing deterministic.
- **Don't test the framework:** you don't need to prove Express can parse JSON; test that *your* error handler maps parse errors to your format.
- **Avoid asserting on full response bodies** when only part matters (use `toMatchObject`), but do assert exactly where a **leak** would be bad (`toEqual`).
- **Keep tests independent of data IDs and ordering** unless ordering is the behavior being tested.

---

## Common mistakes

```js
// ❌ bypassing authentication in tests ("req.user = ...") so the auth layer is never exercised
// ❌ testing only the happy path: failures, auth, and validation are where bugs hide
// ❌ asserting just `status === 201` without checking the data was saved or the body is right
// ❌ real outbound HTTP in tests (slow, flaky, costs money): disableNetConnect/onUnhandledRequest catches it
// ❌ forgetting nock.enableNetConnect("127.0.0.1") → supertest can't reach the app ("Nock: Disallowed net connect")
// ❌ nock interceptors leaking between tests (no cleanAll in afterEach) → tests pass or fail depending on order
// ❌ a fake whose behavior differs from the real service: failures only show up in production
// ❌ `app.listen()` called inside app.js → open ports and "EADDRINUSE" across test files
// ❌ sharing one database with no isolation, so test files interfere when run in parallel (03)
// ❌ sending webhook bodies as parsed objects (.send({...})): bytes differ from what was signed, so signature checks fail
const body = { id: "evt_1" };
await request(app).post("/webhooks").set("Webhook-Signature", sign(JSON.stringify(body))).send(body);   // ❌ re-serialization may differ
// ✅ send the same string you signed
// ❌ mocking the module under test, so the test proves nothing
// ❌ tests that need production credentials or a specific developer's machine
```

## Checklist

- [ ] `app.js` exports a `buildApp(overrides)`; `server.js` calls `listen` (tests never do)
- [ ] Every endpoint: happy path **plus** 401, 403/404 (ownership), 422 validation, 404, and conflict cases where relevant
- [ ] Real authentication in tests via a token helper or agent; IDOR tests for every resource route
- [ ] Persistence verified (follow-up `GET` or DB query), not just status codes
- [ ] Error responses asserted against the standard format; no leaked internals or secrets (`toEqual` on shapes)
- [ ] Third-party HTTP faked with DI, `nock`, or MSW; `disableNetConnect` / `onUnhandledRequest: "error"` guard against real calls
- [ ] Webhook tests sign the exact bytes; replay and duplicate cases covered
- [ ] Queues replaced with fakes in API tests; workers tested as plain functions
- [ ] Cross-cutting behavior covered: rate limits (configurable), security headers, CORS
- [ ] Optional: OpenAPI conformance or Pact contracts for API consumers

## Next

**`03-test-database-and-coverage.md`** tackles the hardest part of backend testing: giving tests a real, clean database (Docker, Testcontainers, transactions, parallel workers), then measuring what your tests actually cover, and what coverage can't tell you.
