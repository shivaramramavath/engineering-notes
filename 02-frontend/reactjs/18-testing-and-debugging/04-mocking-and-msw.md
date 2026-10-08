# Mocking and MSW

Real code talks to things tests shouldn't: servers, clocks, random numbers, browser APIs. **Mocking** replaces those with controlled stand-ins so tests are fast, deterministic, and able to simulate situations that are hard to produce for real (a 500 error, a slow response, an offline browser).

The art is **mocking the right layer**. Mock too little and tests are flaky and slow. Mock too much and they pass while the app is broken.

## What to mock (and what not to)

**Mock at the boundaries of your system:**

| Boundary | Why | How |
|---|---|---|
| **Network** | Slow, flaky, unavailable in CI | **MSW** (below) |
| **Time** | Debounce, timers, "now" | Fake timers ([Vitest](./01-vitest.md#fake-timers)) |
| **Randomness / IDs** | Non-deterministic output | Stub `crypto.randomUUID`, `Math.random` |
| **Browser APIs jsdom lacks** | Components crash without them | Stubs in setup |
| **Third-party SDKs with side effects** | Analytics, payment widgets, maps | `vi.mock` the module |

**Don't mock:**

- **Your own components, hooks, and utilities.** Replacing them means the test no longer exercises the real integration.
- **React, the router, the query cache.** Use the real ones in a test wrapper ([providers](./02-component-testing-with-rtl.md#providers-a-custom-render)).
- **The thing under test.**

The test of a mock: *"if the real thing changed its behavior, would this test notice?"* If not, you're testing the mock.

## The tools in Vitest

```ts
const fn = vi.fn()                                   // mock function
vi.spyOn(obj, "method")                              // spy on / replace a method
vi.mock("@/lib/analytics", () => ({ track: vi.fn() })) // replace a module
vi.useFakeTimers()                                   // control time
vi.stubGlobal("matchMedia", …)                       // replace a global
vi.stubEnv("VITE_API_URL", "http://test")            // env vars
```

Details are in [01 — Vitest](./01-vitest.md#mock-functions-vifn). Module mocks suit **side-effect boundaries** like analytics. For the network, there's a better tool.

## Why not mock `fetch` directly?

```ts
// ✗ Brittle
global.fetch = vi.fn().mockResolvedValue({ ok: true, json: async () => [{ id: 1 }] })
```

- You must hand-build `Response`-like objects (`ok`, `status`, `json`, `headers`) and get them subtly wrong.
- Your API client's URL building, headers, error handling, and parsing are **bypassed**, so the code you most want to test never runs.
- Tests couple to *how* you call fetch, not what the server returns, so refactoring the client breaks tests.
- You can't easily reuse the mocks in the browser or E2E.

## MSW: mock the network, not the code

**[Mock Service Worker](https://mswjs.io)** intercepts requests at the **network level** and answers them with handlers you define. Your real client code, `fetch`, retries, and parsing all run. Only the server is fake.

```text
Component → hook → API client → fetch ──► [ MSW intercepts ] ──► your handler returns a Response
                (all real code runs)
```

```bash
npm install -D msw
```

This note uses MSW v2 (`http` + `HttpResponse`). Older v1 tutorials use `rest` and `res(ctx.json())`, which are different APIs.

### Handlers

```ts
// src/test/handlers.ts
import { http, HttpResponse } from "msw"

export const handlers = [
  http.get("/api/projects", () =>
    HttpResponse.json({ items: [{ id: "1", name: "Roadmap" }], total: 1 })
  ),

  http.get("/api/projects/:id", ({ params }) =>
    HttpResponse.json({ id: params.id, name: "Roadmap" })
  ),

  http.post("/api/projects", async ({ request }) => {
    const body = (await request.json()) as { name: string }
    return HttpResponse.json({ id: "2", name: body.name }, { status: 201 })
  }),
]
```

- Handlers receive a standard `Request` (`request.json()`, `request.headers`, `new URL(request.url).searchParams`) and `params` from `:id` segments.
- Return `HttpResponse.json(body, { status })`, `HttpResponse.text()`, `new HttpResponse(null, { status: 204 })`, etc.

### The server and lifecycle

```ts
// src/test/server.ts
import { setupServer } from "msw/node"
import { handlers } from "./handlers"

export const server = setupServer(...handlers)
```

```ts
// src/test/setup.ts
import "@testing-library/jest-dom/vitest"
import { server } from "./server"

beforeAll(() => server.listen({ onUnhandledRequest: "error" }))
afterEach(() => server.resetHandlers())        // drop per-test overrides
afterAll(() => server.close())
```

- **`onUnhandledRequest: "error"`** fails the test when code makes a request you didn't mock. This catches accidental real network calls and forgotten handlers. (The default only warns.)
- **`resetHandlers` after each test** restores your defaults so overrides don't leak.

### Per-test overrides: errors, empty states, slow responses

The defaults describe the happy path. Individual tests override them:

```ts
import { delay, http, HttpResponse } from "msw"

it("shows an error when loading fails", async () => {
  server.use(
    http.get("/api/projects", () => HttpResponse.json({ message: "Server error" }, { status: 500 }))
  )
  renderWithProviders(<ProjectList />)
  expect(await screen.findByRole("alert")).toHaveTextContent(/something went wrong/i)
})

it("shows the empty state", async () => {
  server.use(http.get("/api/projects", () => HttpResponse.json({ items: [], total: 0 })))
  renderWithProviders(<ProjectList />)
  expect(await screen.findByText(/no projects yet/i)).toBeInTheDocument()
})

it("shows a skeleton while loading", async () => {
  server.use(http.get("/api/projects", async () => { await delay(150); return HttpResponse.json({ items: [], total: 0 }) }))
  renderWithProviders(<ProjectList />)
  expect(screen.getByTestId("projects-skeleton")).toBeInTheDocument()
  await screen.findByText(/no projects yet/i)
})

it("handles being offline", async () => {
  server.use(http.get("/api/projects", () => HttpResponse.error()))   // network failure
  // …
})
```

This is where MSW shines: loading, empty, error (4xx, 5xx, network failure, malformed body) states are cheap to test and exercise your real error handling ([API error handling](../11-api-integration/05-api-error-handling.md)).

### Asserting on what was sent

Prefer asserting on **outcomes** (what the user sees). When the request itself is the behavior (the payload of a form submit), capture it in the handler:

```ts
it("sends the form values", async () => {
  let received: unknown
  server.use(
    http.post("/api/projects", async ({ request }) => {
      received = await request.json()
      return HttpResponse.json({ id: "9", name: "New" }, { status: 201 })
    })
  )
  // … fill the form and submit …
  await screen.findByText(/project created/i)
  expect(received).toEqual({ name: "New" })
})
```

### Stateful fakes

For flows like "create, then see it in the list", make the handlers share a tiny in-memory database:

```ts
let projects = [{ id: "1", name: "Roadmap" }]
beforeEach(() => { projects = [{ id: "1", name: "Roadmap" }] })       // reset per test

export const handlers = [
  http.get("/api/projects", () => HttpResponse.json({ items: projects, total: projects.length })),
  http.post("/api/projects", async ({ request }) => {
    const { name } = (await request.json()) as { name: string }
    const created = { id: String(projects.length + 1), name }
    projects.push(created)
    return HttpResponse.json(created, { status: 201 })
  }),
]
```

Now the list refetch after creation shows the new item, the way the real server would. This is what makes [integration tests](./05-integration-testing.md) read like user stories.

## Relative URLs

If your client uses relative URLs (`/api/projects`) as in the [API client](../11-api-integration/02-api-client.md), they resolve against jsdom's `window.location` in tests, and MSW handlers with relative paths match them. If a request isn't being matched, check how the final URL is built and consider using absolute URLs in both the client's test configuration (`vi.stubEnv("VITE_API_URL", "http://localhost:3000/api")`) and the handlers. Mismatched origins are the usual cause of "unhandled request" errors.

## Test data: factories

Hand-written JSON in every handler gets repetitive and drifts from your types. Use **factories** that return valid objects with overrides:

```ts
// src/test/factories.ts
import type { Project } from "@/features/projects/api"

let id = 0
export function buildProject(overrides: Partial<Project> = {}): Project {
  id += 1
  return { id: String(id), name: `Project ${id}`, status: "open", ...overrides }
}

// in a test
server.use(http.get("/api/projects", () =>
  HttpResponse.json({ items: [buildProject({ name: "Roadmap" }), buildProject({ status: "closed" })], total: 2 })
))
```

Typing factories with your real types means a schema change surfaces as a compile error in tests. A library like `@faker-js/faker` helps generate varied data, but **seed it** (or avoid it) where determinism matters.

A caveat: mocks encode **your assumptions** about the API. If the real API changes shape, mocked tests keep passing. Mitigate with shared types or generated clients, schema validation at the boundary ([validating responses](../11-api-integration/02-api-client.md#validating-responses)), and a few real-backend [E2E tests](./06-e2e-testing-playwright.md).

## Browser APIs jsdom doesn't provide

jsdom omits several APIs that UI libraries (Radix, shadcn, virtualizers, charts) call. Stub them once in the setup file:

```ts
// src/test/setup.ts (additions)
class ResizeObserverStub { observe() {} unobserve() {} disconnect() {} }
class IntersectionObserverStub {
  observe() {} unobserve() {} disconnect() {} takeRecords() { return [] }
}
vi.stubGlobal("ResizeObserver", ResizeObserverStub)
vi.stubGlobal("IntersectionObserver", IntersectionObserverStub)

window.matchMedia ??= ((query: string) => ({
  matches: false, media: query, onchange: null,
  addEventListener() {}, removeEventListener() {}, addListener() {}, removeListener() {}, dispatchEvent: () => false,
})) as typeof window.matchMedia

Element.prototype.scrollIntoView ??= () => {}
Element.prototype.hasPointerCapture ??= () => false       // Radix Select/Popover
Element.prototype.releasePointerCapture ??= () => {}
```

Add only what your components need. Since `vi.stubGlobal` is cleaned by `vi.unstubAllGlobals()`, assign directly in setup if you want the stubs to persist for every test. Stubs make code *not crash*. They don't implement real layout or observation. Tests that depend on actual behavior (virtualization, measuring, visibility) belong in a real browser.

## Mocking time and randomness

```ts
vi.useFakeTimers()
vi.setSystemTime(new Date("2026-01-15T10:00:00Z"))      // deterministic "now"

vi.spyOn(crypto, "randomUUID").mockReturnValue("00000000-0000-4000-8000-000000000000")
```

Prefer injecting time and IDs where practical (a `now()` helper, a passed-in generator), which makes logic testable without global stubs.

## Module mocks: when they're right

```ts
vi.mock("@/lib/analytics")                                    // auto-mock: all exports become vi.fn()
import { track } from "@/lib/analytics"

await user.click(screen.getByRole("button", { name: /subscribe/i }))
expect(track).toHaveBeenCalledWith("subscribed", { plan: "pro" })
```

Good for: analytics/telemetry, error reporters, payment/maps/chat SDKs, `window.location`-style navigation you can't perform in jsdom, and heavy modules (a chart library) that you replace with a lightweight stub when you aren't testing them.

Avoid for: your API layer, your hooks, your utilities. Use MSW and real code.

## MSW beyond tests

The same handlers can power:

- **Local development** with `setupWorker` (browser service worker), so you can build UI before the backend exists, or demo error states on demand.
- **Storybook** stories and **Playwright** tests that need deterministic backends (use sparingly in E2E, where real integration is the point).

Sharing one handlers file between environments keeps mocks consistent. Check the MSW docs for the browser setup (`npx msw init public/`) and environment-specific details.

## Common mistakes

- **Mocking `fetch` or `axios` by hand** instead of intercepting at the network with MSW.
- **Over-mocking internals** (your hooks, components, and utilities) so tests no longer prove integration.
- **Leaving `onUnhandledRequest` at "warn"**, letting forgotten handlers and real network calls slip through.
- **Not resetting handlers** (`resetHandlers`) or shared in-memory data between tests.
- **A shared `QueryClient`** whose cache serves stale data across tests, hiding handler changes.
- **Using MSW v1 syntax** (`rest`, `ctx`) with v2 (`http`, `HttpResponse`).
- **Only mocking the happy path.** Error, empty, slow, and offline states are where bugs live.
- **Hardcoding the same JSON everywhere** instead of typed factories.
- **Trusting mocks as truth.** They encode assumptions about the API, so back them with types, validation, and a few real E2E checks.
- **Stubbing browser APIs and then asserting on behavior that needs the real thing.**
- **Forgetting to restore timers/globals/spies.**

## Quick summary

- **Mock the boundaries**: network, time, randomness, missing browser APIs, side-effecting SDKs. Don't mock your own code.
- **MSW** intercepts requests at the network level, so your real client, hooks, and error handling run. Use `http`/`HttpResponse` (v2), `setupServer` from `msw/node`.
- Setup: `server.listen({ onUnhandledRequest: "error" })`, `resetHandlers` after each test, `close` after all.
- Defaults model the happy path; **`server.use(...)` overrides** per test for errors (`status: 500`, `HttpResponse.error()`), empty data, and `delay()`.
- Use **typed factories** and small stateful fakes for realistic flows.
- Stub missing jsdom APIs in setup; prefer a real browser (Playwright) when behavior depends on layout.
- Mocks encode assumptions, so pair them with shared types or validation and a few real E2E tests.

## Next

[05 — Integration testing](./05-integration-testing.md)
