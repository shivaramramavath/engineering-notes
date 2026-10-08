# Integration Testing

In a React app, an **integration test** renders a **meaningful chunk of the real app** (a page or feature, with its real components, hooks, router, and query cache) and drives it through the UI, with only the **network** faked. It's the highest-value kind of test for frontend code, because most bugs live in the **seams** between parts that unit tests exercise separately.

```text
         ┌──────────────── real, not mocked ────────────────┐
user ──► │ components → hooks → API client → query cache     │ ──► [ MSW fake server ]
 (user-  │ router, providers, validation, error handling    │      (only the network is fake)
  event) └───────────────────────────────────────────────────┘
```

## Why integration tests

A unit test of `ProjectForm` with a mocked `onSubmit` can pass, and the real feature can still be broken because:

- The page doesn't pass the right props.
- The mutation doesn't invalidate the list, so the new project never appears.
- The route isn't registered, or a guard redirects wrongly.
- The API client sends the wrong payload shape.
- A loading or error state was never wired up.

An integration test exercising *"create a project and see it in the list"* catches all of these in one go. See [the testing trophy](./00-testing-fundamentals.md#the-testing-trophy-not-the-pyramid).

## What counts, and what to keep real

**Keep real:** components, hooks, state (context, store), the router, the query client, the API client, validation, formatting.

**Fake:** the network (MSW), time if needed, and the usual jsdom gaps ([04](./04-mocking-and-msw.md)).

**Scope:** a page, a feature slice, or a multi-step flow. Not the whole application with every route unless you need to.

## A reusable app renderer

Build a helper that mounts your **real route tree** at a chosen URL, with fresh providers per test:

```tsx
// src/test/render-app.tsx
import { render } from "@testing-library/react"
import userEvent from "@testing-library/user-event"
import { QueryClientProvider } from "@tanstack/react-query"
import { createMemoryRouter, RouterProvider } from "react-router"
import { routes } from "@/routes"                   // the SAME route config the app uses
import { createTestQueryClient } from "./utils"
import { AuthProvider } from "@/features/auth/auth-provider"

export function renderApp({ route = "/" }: { route?: string } = {}) {
  const queryClient = createTestQueryClient()
  const router = createMemoryRouter(routes, { initialEntries: [route] })
  const user = userEvent.setup()

  render(
    <QueryClientProvider client={queryClient}>
      <AuthProvider>
        <RouterProvider router={router} />
      </AuthProvider>
    </QueryClientProvider>
  )
  return { user, router, queryClient }
}
```

Exporting `routes` from your router module (rather than building the router inline) is what makes this possible. See [route data loading](../10-routing/05-route-data-loading.md). If your real route config needs the `queryClient` for loaders, expose a `createRoutes(queryClient)` factory.

## Example: a complete flow

```tsx
describe("projects", () => {
  it("lets a signed-in user create a project and see it in the list", async () => {
    const { user } = renderApp({ route: "/projects" })

    // lists existing data from the (fake) server
    expect(await screen.findByRole("row", { name: /roadmap/i })).toBeInTheDocument()

    // open the create dialog
    await user.click(screen.getByRole("button", { name: /new project/i }))
    const dialog = await screen.findByRole("dialog", { name: /new project/i })

    // validation
    await user.click(within(dialog).getByRole("button", { name: /create/i }))
    expect(await within(dialog).findByText(/name is required/i)).toBeInTheDocument()

    // valid submission
    await user.type(within(dialog).getByLabelText(/name/i), "Launch plan")
    await user.click(within(dialog).getByRole("button", { name: /create/i }))

    // the dialog closes, a confirmation appears, and the list shows the new project
    await waitFor(() => expect(screen.queryByRole("dialog")).not.toBeInTheDocument())
    expect(await screen.findByText(/project created/i)).toBeInTheDocument()
    expect(await screen.findByRole("row", { name: /launch plan/i })).toBeInTheDocument()
  })
})
```

Notice what the test doesn't know about: component names, state management, the mutation hook, query keys. It would survive swapping React Hook Form for plain state, or TanStack Query for something else. That's the point. It depends on the **contract with the user** and the **contract with the network** (the stateful MSW handlers from [04](./04-mocking-and-msw.md#stateful-fakes)).

## Testing the unhappy paths

Integration tests are where you verify your **error and edge handling** end to end:

```tsx
it("shows an inline error and keeps the dialog open when creation fails", async () => {
  server.use(http.post("/api/projects", () => HttpResponse.json({ message: "Name already taken" }, { status: 409 })))
  const { user } = renderApp({ route: "/projects" })

  await user.click(await screen.findByRole("button", { name: /new project/i }))
  await user.type(screen.getByLabelText(/name/i), "Roadmap")
  await user.click(screen.getByRole("button", { name: /create/i }))

  expect(await screen.findByText(/name already taken/i)).toBeInTheDocument()
  expect(screen.getByRole("dialog")).toBeInTheDocument()
})

it("shows a retry option when the list fails to load", async () => {
  server.use(http.get("/api/projects", () => new HttpResponse(null, { status: 500 })))
  const { user } = renderApp({ route: "/projects" })

  expect(await screen.findByRole("alert")).toBeInTheDocument()

  server.resetHandlers()                       // the "server recovers"
  await user.click(screen.getByRole("button", { name: /try again/i }))
  expect(await screen.findByRole("row", { name: /roadmap/i })).toBeInTheDocument()
})
```

Cover: loading, empty, error (and recovery), validation, permission denied, not found, expired session (a 401 flowing through the [refresh logic](../11-api-integration/04-refresh-token-flow.md)).

## Auth and route protection

Start tests in the state you need rather than clicking through login each time:

```tsx
// Option A: handlers return an authenticated user
server.use(http.get("/api/auth/me", () => HttpResponse.json({ id: "u1", name: "Ana", role: "admin" })))

// Option B: unauthenticated
server.use(http.get("/api/auth/me", () => new HttpResponse(null, { status: 401 })))
```

```tsx
it("redirects anonymous users to the login page", async () => {
  server.use(http.get("/api/auth/me", () => new HttpResponse(null, { status: 401 })))
  const { router } = renderApp({ route: "/dashboard" })

  expect(await screen.findByRole("heading", { name: /sign in/i })).toBeInTheDocument()
  expect(router.state.location.pathname).toBe("/login")
})
```

Test the guard's behavior ([route protection](../10-routing/04-route-protection.md)), plus one full **login flow** test. Remember these test UX. Real authorization happens on the server.

Asserting `router.state.location` is acceptable for redirects. It's observable behavior (the URL), but prefer asserting on what's rendered when possible.

## Test isolation

Every test should start from a clean slate:

- **New `QueryClient`** per test (cache, in-flight requests).
- **Reset MSW handlers** and any in-memory fake data (`beforeEach`).
- **Reset stores** (Zustand/Redux) and `localStorage`/`sessionStorage` (`afterEach(() => localStorage.clear())`).
- **Restore timers/mocks** (`restoreMocks: true`).
- No test relies on another's side effects. Each can run alone (`it.only` should always pass).

Leaked state is the most common cause of tests that pass alone and fail in the suite.

## Keeping them fast and stable

- **Await the right thing.** Use `findBy*`/`waitFor` for async UI. Fixed sleeps make tests slow *and* flaky.
- **Don't test every combination here.** One integration test per important flow, with edge cases covered cheaper (unit tests for logic, targeted component tests).
- **Share setup**, not state: helpers like `renderApp`, factories, and default handlers.
- **Keep scope tight.** Render the feature's route, not the whole app, if you don't need the rest.
- **Disable animations/transitions** that delay UI or leave elements mid-transition.
- **Avoid over-asserting.** Check the outcomes that define the flow, not every element.
- Integration tests are slower than unit tests (they render real trees), so run the suite in parallel and keep individual tests focused.

## Integration vs component vs E2E

| | Component test ([02](./02-component-testing-with-rtl.md)) | Integration test (this note) | E2E ([06](./06-e2e-testing-playwright.md)) |
|---|---|---|---|
| Scope | One component, mocked callbacks | A feature/page: router + data + UI | The whole system |
| Network | Often unneeded or MSW | **MSW** | Real (or a test backend) |
| Environment | jsdom | jsdom | **Real browser** |
| Catches | Component logic, accessibility | **Wiring, data flow, state** | Deployment, CSS/layout, real backend, browser quirks |
| Speed | Fast | Moderate | Slow |
| Count | Many | **Many (the core)** | A few critical flows |

They overlap by design. Use the cheapest layer that can catch the bug.

## Where integration tests can't help

jsdom has no layout engine and no real browser behavior. These need a real browser:

- Actual layout, responsive breakpoints, CSS visibility, scrolling, sticky positioning
- Focus behavior and keyboard navigation across complex widgets (portaled menus, dialogs)
- File uploads/downloads, clipboard, drag and drop with real geometry
- Anything about how the app is built and served (routing at the server, headers, redirects)
- Integration with the real backend

Cover those with [Playwright](./06-e2e-testing-playwright.md).

## Organizing integration tests

```text
src/
├── test/
│   ├── setup.ts              # jest-dom, MSW lifecycle, jsdom stubs
│   ├── server.ts, handlers.ts
│   ├── factories.ts
│   ├── utils.tsx             # renderWithProviders
│   └── render-app.tsx        # renderApp (route-level)
└── features/projects/
    ├── ProjectsPage.tsx
    └── projects.integration.test.tsx    # or ProjectsPage.test.tsx
```

A naming convention (`*.integration.test.tsx`) lets you run them separately (`vitest integration`) if you want a fast unit-only loop.

## Common mistakes

- **Mocking your own hooks, API layer, and components** inside an "integration" test, which makes it a unit test with extra steps.
- **Rebuilding the route config inside tests** instead of reusing the app's real routes (drift).
- **A shared `QueryClient`/store/MSW data across tests**, causing order-dependent failures.
- **Only testing the happy path.**
- **Fixed delays** instead of `findBy*`/`waitFor`.
- **Logging in through the UI in every test**, rather than starting from a seeded auth state (keep one real login flow test).
- **Giant tests that verify everything**, which are slow and hard to diagnose. Prefer several focused flows.
- **Expecting jsdom to behave like a browser** for layout, focus quirks, or real pointer behavior.
- **Asserting implementation** (query keys, store contents, internal state) instead of visible results.
- **Treating mocked API behavior as the truth**, with no real-backend checks anywhere.
- **Not testing error recovery**, such as "retry after failure".

## Quick summary

- An integration test mounts a **real feature** (components, hooks, router, query cache) and fakes **only the network** with MSW.
- It's the highest-value layer for React apps, because it catches wiring and data-flow bugs that unit tests miss.
- Build a **`renderApp({ route })`** helper using the app's real route config, a fresh `QueryClient`, and a memory router.
- Write tests as **user stories**: list → open dialog → validate → submit → see the result. Use stateful MSW fakes so flows behave like a real server.
- Cover **unhappy paths** (errors, empty, permission, expired session, retry) and guard behavior.
- Isolate every test (new client, reset handlers/stores/storage), and wait with `findBy*`.
- Use Playwright for anything needing real layout, browser behavior, or the real backend.

## Next

[06 — E2E testing with Playwright](./06-e2e-testing-playwright.md)
