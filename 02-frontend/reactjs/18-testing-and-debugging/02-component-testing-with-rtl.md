# Component Testing with React Testing Library

**React Testing Library (RTL)** renders your component into a (simulated) DOM and gives you tools to **find elements the way a user or assistive technology would** and **interact with them like a user would**. Its guiding principle:

> The more your tests resemble the way your software is used, the more confidence they can give you.

RTL deliberately gives you **no access** to component state, props, or instances, so you can't test implementation details even if you wanted to.

(Setup with Vitest: [01](./01-vitest.md#setup).)

## The basic loop

```tsx
import { render, screen } from "@testing-library/react"
import userEvent from "@testing-library/user-event"
import { Counter } from "./Counter"

it("increments when the button is clicked", async () => {
  const user = userEvent.setup()
  render(<Counter />)

  expect(screen.getByText("Count: 0")).toBeInTheDocument()

  await user.click(screen.getByRole("button", { name: /increment/i }))

  expect(screen.getByText("Count: 1")).toBeInTheDocument()
})
```

1. **`render`**: mounts the component.
2. **`screen`**: queries the whole document.
3. **`user`**: interacts (`await` every call).
4. **Assert** on what the user would see.

## Queries: how to find elements

### Priority order

Prefer queries that reflect what users and assistive tech perceive. In order:

| Priority | Query | Finds by | Use for |
|---|---|---|---|
| 1 | **`getByRole`** | ARIA role + accessible name | Almost everything interactive: buttons, links, headings, inputs, dialogs |
| 2 | **`getByLabelText`** | Associated `<label>` | Form fields |
| 3 | `getByPlaceholderText` | Placeholder | Only when there's no label (and fix the missing label) |
| 4 | **`getByText`** | Visible text | Non-interactive content |
| 5 | `getByDisplayValue` | Current value of an input | Checking filled fields |
| 6 | `getByAltText` | `alt` text | Images |
| 7 | `getByTitle` | `title` attribute | Rarely |
| Last resort | `getByTestId` | `data-testid` | When nothing semantic works |

`getByRole` is the workhorse, and it doubles as an **accessibility check**: if you can't find a button by role and name, a screen reader user probably can't either ([semantic HTML](../08-accessibility/00-semantic-html.md)).

```tsx
screen.getByRole("button", { name: /save/i })             // <button>Save changes</button>
screen.getByRole("heading", { name: "Projects", level: 1 })
screen.getByRole("textbox", { name: /email/i })           // <input> labelled "Email"
screen.getByRole("checkbox", { name: /remember me/i })
screen.getByRole("link", { name: "Settings" })
screen.getByRole("dialog")
screen.getByRole("row", { name: /ana/i })
screen.getByLabelText("Password")
```

Use a regex with `i` for tolerance, or an exact string when the exact text matters.

### `getBy` vs `queryBy` vs `findBy`

| Variant | No match | Multiple matches | Async | Use for |
|---|---|---|---|---|
| `getBy…` | **Throws** | Throws | No | Element must exist *now* |
| `queryBy…` | Returns **`null`** | Throws | No | Asserting an element is **absent** |
| `findBy…` | Rejects after timeout | Rejects | **Yes** (waits, ~1 s default) | Element will appear **soon** |
| `getAllBy…` / `queryAllBy…` / `findAllBy…` | | Return arrays | | Multiple elements |

```tsx
expect(screen.queryByRole("alert")).not.toBeInTheDocument()   // ✓ absence: use queryBy
expect(screen.getByRole("alert")).not.toBeInTheDocument()     // ✗ getBy throws before the assertion
const row = await screen.findByRole("row", { name: /ana/i })  // wait for it to appear
```

### Scoping with `within`

```tsx
import { within } from "@testing-library/react"

const dialog = screen.getByRole("dialog")
await user.click(within(dialog).getByRole("button", { name: /confirm/i }))
```

### Finding the right query

When you can't figure out how to select something:

- **`screen.debug()`** prints the DOM (or `screen.debug(element)`).
- **`screen.logTestingPlaygroundURL()`** opens an interactive tool suggesting the best query.
- **`logRoles(container)`** lists the roles in the rendered output.
- The error message from a failed `getBy` lists the available roles and names. Read it.

Avoid `container.querySelector(".some-class")`. It tests structure and styling, not what users perceive.

## Interacting: `user-event`

```tsx
const user = userEvent.setup()         // create ONE per test, before render

await user.click(button)
await user.dblClick(button)
await user.type(screen.getByLabelText("Email"), "ana@example.com")
await user.clear(input)
await user.keyboard("{Enter}")
await user.keyboard("{Shift>}A{/Shift}")
await user.tab()
await user.selectOptions(screen.getByRole("combobox"), "admin")
await user.upload(fileInput, new File(["x"], "a.png", { type: "image/png" }))
await user.hover(el)
```

**Prefer `user-event` over `fireEvent`.** `fireEvent.click` dispatches a single synthetic event. `userEvent.click` simulates the real sequence (pointer move, down, focus, up, click) and respects disabled state, focus changes, and `readOnly`, so it catches bugs `fireEvent` hides. `fireEvent` remains for rare low-level events.

`user.type` types character by character, firing `keydown`/`input`/`keyup` and updating the controlled input each time, which is how [`onChange` really behaves](../17-react-internals/04-event-system.md#events-that-dont-behave-like-their-dom-namesakes).

Special characters in `type`/`keyboard` use braces (`{Enter}`, `{Backspace}`), and literal `{` or `[` must be escaped by doubling (`{{`, `[[`).

## Assertions with jest-dom

The jest-dom matchers (loaded in [setup](./01-vitest.md#the-setup-file)) read naturally and give better failure messages:

```tsx
expect(el).toBeInTheDocument()
expect(el).toBeVisible()                          // considers CSS display/visibility/hidden
expect(el).toBeDisabled()  /  toBeEnabled()
expect(el).toBeRequired()
expect(el).toBeChecked()
expect(el).toHaveTextContent(/saved/i)
expect(el).toHaveValue("ana@example.com")
expect(el).toHaveAttribute("href", "/settings")
expect(el).toHaveFocus()
expect(el).toHaveAccessibleName("Close")
expect(el).toHaveAccessibleDescription("Required field")
expect(el).toHaveClass("active")                  // sparingly: prefer behavior over classes
expect(input).toBeInvalid()                       // aria-invalid or failed validation
```

## Async UI

Most real components load data, debounce, or animate. Wait for what the **user would wait for**:

```tsx
// Wait for an element to appear
expect(await screen.findByText("Project created")).toBeInTheDocument()

// Wait for a condition
await waitFor(() => expect(onSave).toHaveBeenCalledTimes(1))

// Wait for something to disappear
await waitForElementToBeRemoved(() => screen.queryByText("Loading…"))
```

Guidelines:

- **Prefer `findBy*`** over `waitFor(() => getBy*)`: shorter and better errors.
- **Put exactly one assertion (or one query) in `waitFor`.** It retries the callback until it stops throwing, so side effects inside it (clicks, mutations) run repeatedly.
- **Never use fixed delays** (`await sleep(500)`). They're slow when too long and flaky when too short.
- If a `findBy*` times out, the element really isn't appearing: debug the DOM, not the timeout (raise `timeout` only for known-slow cases).

### `act` warnings

"An update to X inside a test was not wrapped in `act(...)`" means state updated **outside** something React was tracking, usually because the test finished (or asserted) before async work settled. RTL's async utilities and `user-event` already wrap updates in `act`. The fix is almost always **wait for the result** (`findBy*`/`waitFor`), not wrapping manually or silencing the warning. If you need `act` directly, import it from `react` (React 19) or from RTL.

## Providers: a custom `render`

Real components need context: router, query client, theme, i18n. Create one helper so tests stay short:

```tsx
// src/test/utils.tsx
import { render, type RenderOptions } from "@testing-library/react"
import { QueryClient, QueryClientProvider } from "@tanstack/react-query"
import { MemoryRouter } from "react-router"

export function createTestQueryClient() {
  return new QueryClient({
    defaultOptions: {
      queries: { retry: false, gcTime: Infinity },   // failures surface immediately; no stray timers
      mutations: { retry: false },
    },
  })
}

type Options = RenderOptions & { route?: string; queryClient?: QueryClient }

export function renderWithProviders(ui: React.ReactElement, { route = "/", queryClient = createTestQueryClient(), ...options }: Options = {}) {
  function Wrapper({ children }: { children: React.ReactNode }) {
    return (
      <QueryClientProvider client={queryClient}>
        <MemoryRouter initialEntries={[route]}>{children}</MemoryRouter>
      </QueryClientProvider>
    )
  }
  return { queryClient, ...render(ui, { wrapper: Wrapper, ...options }) }
}
```

Key details:

- **A fresh `QueryClient` per test**, so cached data doesn't leak between tests ([TanStack Query testing](../12-server-state/03-tanstack-query.md#testing)).
- **`retry: false`** or failing requests retry with backoff and time out your test.
- A **`MemoryRouter`** (or `createMemoryRouter` for data routers) controls the starting URL without a browser ([routing](../10-routing/00-react-router.md#router-types-briefly)).
- Let tests pass options (`route`, a preloaded query client, an authenticated user) to vary the setup.
- Re-export RTL from this file if you like, so tests import from one place.

### Data routers

If the component relies on loaders or actions, use a data router:

```tsx
import { createMemoryRouter, RouterProvider } from "react-router"

const router = createMemoryRouter(
  [{ path: "/projects/:id", element: <ProjectPage />, loader: projectLoader }],
  { initialEntries: ["/projects/42"] }
)
render(<RouterProvider router={router} />)
expect(await screen.findByRole("heading", { name: /roadmap/i })).toBeInTheDocument()
```

## A realistic example: a form

```tsx
it("shows validation errors and then submits valid data", async () => {
  const user = userEvent.setup()
  const onSubmit = vi.fn()
  render(<SignupForm onSubmit={onSubmit} />)

  // invalid submit
  await user.click(screen.getByRole("button", { name: /sign up/i }))
  expect(await screen.findByText(/email is required/i)).toBeInTheDocument()
  expect(onSubmit).not.toHaveBeenCalled()

  // fix and resubmit
  await user.type(screen.getByLabelText(/email/i), "ana@example.com")
  await user.type(screen.getByLabelText(/password/i), "correct-horse-battery")
  await user.click(screen.getByRole("button", { name: /sign up/i }))

  await waitFor(() => expect(onSubmit).toHaveBeenCalledWith({
    email: "ana@example.com",
    password: "correct-horse-battery",
  }))
})
```

The test reads like a user story, and it would survive swapping React Hook Form for plain state. Callback props (`onSubmit`) are a component's public contract, so asserting on them is fine.

## What to assert

- **Visible output**: text, roles, values, states (disabled, checked, expanded).
- **Accessibility**: roles/names are findable, `aria-invalid` and error text appear, focus lands where expected after an action (`toHaveFocus`).
- **Callbacks**: with the right arguments.
- **Network effects**: through [MSW](./04-mocking-and-msw.md) (assert on the outcome, or capture the request).
- **Not**: internal state, hook calls, private functions, class names (usually).

### Accessibility checks

`getByRole` already catches many problems. For automated rule checks, an axe integration (such as `vitest-axe` or `jest-axe`) can scan the rendered DOM:

```tsx
const { container } = render(<LoginForm />)
expect(await axe(container)).toHaveNoViolations()
```

Automated tools catch only a portion of accessibility issues (commonly estimated at a minority), so treat them as a safety net and keep manual and screen-reader testing ([accessibility checklist](../08-accessibility/04-accessibility-checklist.md)).

## Special cases

**Portals and dialogs.** Portaled content lives in `document.body`, not in the render `container`. `screen` queries the whole document, so `screen.getByRole("dialog")` works ([portals](../16-advanced-react/00-portals.md#testing)).

**Error boundaries.** React logs caught errors to `console.error`. Silence it in that test so output stays readable:

```tsx
vi.spyOn(console, "error").mockImplementation(() => {})
render(<ErrorBoundary fallback={<p>Oops</p>}><Bomb /></ErrorBoundary>)
expect(screen.getByText("Oops")).toBeInTheDocument()
```

**Radix/shadcn components** (Select, Dropdown, Tooltip) rely on pointer events and layout APIs jsdom lacks. You may need to stub `ResizeObserver`, `scrollIntoView`, `hasPointerCapture`, and so on in your setup file ([04](./04-mocking-and-msw.md#browser-apis-jsdom-doesnt-provide)). If a component is painful to test in jsdom, cover it in a Playwright test ([06](./06-e2e-testing-playwright.md)).

**Suspense and lazy components.** Use `findBy*` to wait for the resolved content.

**Timers/animations.** Prefer fake timers ([Vitest](./01-vitest.md#fake-timers)), or disable animations in tests.

**Controlled vs uncontrolled inputs.** `user.type` works for both. For controlled inputs, the displayed value only changes if your `onChange` updates state, which makes that bug visible.

## Rerendering and unmounting

```tsx
const { rerender, unmount } = render(<Greeting name="Ana" />)
rerender(<Greeting name="Ben" />)               // same instance, new props: tests update behavior
unmount()                                       // tests cleanup effects
```

## Common mistakes

- **Using `container.querySelector`** or class names instead of roles and labels.
- **Using `getBy` to assert absence** (use `queryBy`).
- **Forgetting `await`** on `user-event` calls and `findBy*`.
- **Fixed `setTimeout`/sleep** instead of `findBy*`/`waitFor`.
- **Multiple assertions or side effects inside `waitFor`.**
- **Testing implementation details**: state, instances, "was this hook called".
- **`fireEvent` everywhere** instead of `user-event`.
- **Sharing a `QueryClient` or store between tests**, leaking state.
- **No `retry: false`** on the test query client, so error tests time out.
- **Overusing `data-testid`** when a role or label query would work (and verify accessibility).
- **Silencing `act` warnings** instead of waiting for the async result.
- **Over-specific text matches** (`"Welcome back, Ana! You have 3 new messages"`) that break on copy edits. Use regexes or key phrases.
- **Testing the library**: asserting that a Radix dialog traps focus rather than that *your* dialog does what you need.

## Quick summary

- RTL renders components and lets you query and interact **like a user**: no access to state or props.
- Query priority: **`getByRole`** (with name) → **`getByLabelText`** → `getByText` → … → `getByTestId` last. `getBy` (must exist), `queryBy` (absence), `findBy` (appears soon).
- Use **`user-event`** (`const user = userEvent.setup()`, `await user.click(...)`) rather than `fireEvent`.
- Assert with **jest-dom** matchers; wait with **`findBy*`/`waitFor`**, never fixed delays.
- Wrap providers in a custom `render` with a **fresh `QueryClient`** (retry off) and a memory router.
- Test visible behavior and public callbacks, not internals; use `screen.debug()` and Testing Playground when stuck.

## Next

[03 — Hook testing](./03-hook-testing.md)
