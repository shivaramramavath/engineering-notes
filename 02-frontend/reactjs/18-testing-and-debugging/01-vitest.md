# Vitest

**Vitest** is the test runner built for the Vite ecosystem. It reuses your `vite.config.ts` (aliases, plugins, TypeScript, JSX), starts fast, supports ESM natively, and offers a **Jest-compatible API** (`describe`, `it`, `expect`, mocks), so most Jest knowledge transfers directly. It runs the tests; React Testing Library ([02](./02-component-testing-with-rtl.md)) renders the components inside them.

## Setup

```bash
npm install -D vitest jsdom @testing-library/react @testing-library/dom @testing-library/jest-dom @testing-library/user-event
```

(`@testing-library/dom` is a peer dependency of recent `@testing-library/react` versions, so install it explicitly.)

### Configure it in `vite.config.ts`

```ts
/// <reference types="vitest/config" />
import { defineConfig } from "vite"
import react from "@vitejs/plugin-react"
import path from "path"

export default defineConfig({
  plugins: [react()],
  resolve: { alias: { "@": path.resolve(__dirname, "./src") } },   // reused by tests automatically
  test: {
    environment: "jsdom",                  // simulated browser DOM
    globals: true,                         // describe/it/expect without imports
    setupFiles: ["./src/test/setup.ts"],   // runs before each test file
    css: true,                             // process CSS imports (so class-based visibility works)
    restoreMocks: true,                    // reset vi.spyOn mocks between tests
  },
})
```

You can also use a separate `vitest.config.ts` (importing `defineConfig` from `vitest/config`) if you want to keep test config apart. A single shared config keeps your aliases and plugins in sync.

### The setup file

```ts
// src/test/setup.ts
import "@testing-library/jest-dom/vitest"     // adds matchers: toBeInTheDocument, toBeDisabled, …
```

With `globals: true`, React Testing Library automatically **cleans up** (unmounts rendered components) after each test. Without globals, call `cleanup()` yourself in an `afterEach`.

### TypeScript

Make the globals and matcher types visible:

```json
// tsconfig.app.json
{
  "compilerOptions": { "types": ["vite/client", "vitest/globals"] },
  "include": ["src"]            // includes src/test/setup.ts so the jest-dom types apply
}
```

### Scripts

```json
{
  "scripts": {
    "test": "vitest",
    "test:run": "vitest run",
    "test:coverage": "vitest run --coverage"
  }
}
```

- `vitest` starts **watch mode**: it re-runs only the tests affected by your changes (using the module graph). It's the daily driver.
- `vitest run` runs once and exits (for CI).
- Filter by file name (`vitest cart`) or test name (`vitest -t "shows an error"`).

## Writing tests

```ts
import { describe, it, expect } from "vitest"      // optional when globals: true
import { calculateTotal } from "./cart"

describe("calculateTotal", () => {
  it("sums item prices times quantity", () => {
    expect(calculateTotal([{ price: 10, qty: 3 }])).toBe(30)
  })

  it("returns 0 for an empty cart", () => {
    expect(calculateTotal([])).toBe(0)
  })
})
```

`test` is an alias of `it`. Files named `*.test.ts(x)` or `*.spec.ts(x)` are picked up by default.

### Organizing and controlling

```ts
describe.each / it.each(...)   // table-driven tests
it.skip("…")                   // temporarily skip (leave a reason!)
it.only("…")                   // run only this one (don't commit it)
it.todo("handles expired coupons")   // reminder in the report
it.fails("known bug", …)       // expected to fail
```

Setup and teardown:

```ts
beforeAll(() => {})     // once per file
beforeEach(() => {})    // before every test
afterEach(() => {})
afterAll(() => {})
```

Prefer **fresh setup in each test** (or `beforeEach`) over shared mutable state between tests.

## Assertions

```ts
expect(value).toBe(3)                       // Object.is: primitives and identity
expect(obj).toEqual({ a: 1 })               // deep equality (ignores undefined properties)
expect(obj).toStrictEqual({ a: 1 })         // deep equality, stricter (types, undefined)
expect(arr).toContain("x")
expect(arr).toHaveLength(2)
expect(str).toMatch(/error/i)
expect(obj).toMatchObject({ id: 1 })        // subset match
expect(n).toBeCloseTo(0.3)                  // floating point
expect(() => parse("bad")).toThrow("invalid")

await expect(fetchUser("1")).resolves.toEqual({ id: "1" })
await expect(fetchUser("x")).rejects.toThrow("not found")

expect(fn).toHaveBeenCalled()
expect(fn).toHaveBeenCalledTimes(2)
expect(fn).toHaveBeenCalledWith("a", 1)
```

Plus the DOM matchers from jest-dom, covered in [02](./02-component-testing-with-rtl.md#assertions-with-jest-dom). A few tips:

- **`toEqual` vs `toBe`**: `toBe` for primitives, `toEqual` for objects and arrays.
- Always `await` or `return` async assertions. A forgotten `await` on `resolves`/`rejects` makes the test pass without checking anything.
- Assertions on async DOM updates use RTL's `findBy*`/`waitFor`, not arbitrary sleeps.

## Mock functions: `vi.fn()`

```ts
const onSave = vi.fn()                                  // records calls
const getUser = vi.fn().mockResolvedValue({ id: "1" })  // returns a resolved promise
const random = vi.fn().mockReturnValueOnce(0.1).mockReturnValue(0.9)
const impl = vi.fn((a: number, b: number) => a + b)     // custom implementation

render(<Form onSave={onSave} />)
await user.click(screen.getByRole("button", { name: /save/i }))
expect(onSave).toHaveBeenCalledWith({ name: "Ana" })
```

Passing a `vi.fn()` as a prop and asserting it was called is a legitimate way to test a component's **public contract** (its callbacks).

### Spies

Wrap an existing method, record calls, and optionally replace the implementation:

```ts
const errorSpy = vi.spyOn(console, "error").mockImplementation(() => {})   // silence expected errors
// … trigger the error …
expect(errorSpy).toHaveBeenCalled()
// restored automatically when `restoreMocks: true`, or call errorSpy.mockRestore()
```

### Mocking modules: `vi.mock`

```ts
import { analytics } from "@/lib/analytics"
vi.mock("@/lib/analytics", () => ({ analytics: { track: vi.fn() } }))

it("tracks the signup", async () => {
  // …
  expect(vi.mocked(analytics.track)).toHaveBeenCalledWith("signup")
})
```

Key behaviors:

- **`vi.mock` is hoisted** to the top of the file, before imports. That's why it can mock a module that's imported above it, and why the factory can't reference variables declared in the file (use `vi.hoisted()` if you must share one).
- The factory replaces the **entire module**. To keep the real exports and override some, use `importOriginal`:

```ts
vi.mock("@/lib/date", async (importOriginal) => ({
  ...(await importOriginal<typeof import("@/lib/date")>()),
  now: () => new Date("2026-01-01"),
}))
```

- `vi.mocked(fn)` gives you typed access to mock methods.
- Mocks persist across tests in a file unless reset. Use `restoreMocks`/`clearMocks` config, or `vi.resetAllMocks()` in `beforeEach`. (`clear` resets call history; `reset` also clears implementations; `restore` also puts original implementations back for spies.)

**Mock at boundaries**, not your own modules. For network use [MSW](./04-mocking-and-msw.md); module mocks are best for things like analytics, third-party SDKs, and `window` APIs.

## Fake timers

Control time so tests are fast and deterministic (debounce, polling, timeouts):

```ts
beforeEach(() => { vi.useFakeTimers() })
afterEach(() => { vi.useRealTimers() })

it("debounces the search", () => {
  const onSearch = vi.fn()
  const debounced = debounce(onSearch, 300)

  debounced("a"); debounced("ab")
  expect(onSearch).not.toHaveBeenCalled()

  vi.advanceTimersByTime(300)
  expect(onSearch).toHaveBeenCalledTimes(1)
  expect(onSearch).toHaveBeenCalledWith("ab")
})
```

- `vi.advanceTimersByTime(ms)`, `vi.runAllTimers()`, `vi.runOnlyPendingTimers()`.
- `vi.setSystemTime(new Date("2026-01-01"))` fixes `Date.now()`/`new Date()`.
- **Always restore real timers** after each test, or later tests hang.
- With React Testing Library and user-event, fake timers need care: pass `advanceTimers` to user-event (`userEvent.setup({ advanceTimers: vi.advanceTimersByTime })`) and prefer `vi.useFakeTimers({ shouldAdvanceTime: true })` so `waitFor` and `findBy*` still make progress.

## Environment and globals

```ts
vi.stubEnv("VITE_API_URL", "https://api.test")        // import.meta.env.VITE_API_URL in this test
vi.stubGlobal("matchMedia", (q: string) => ({ matches: false, media: q, addEventListener() {}, removeEventListener() {} }))
afterEach(() => { vi.unstubAllEnvs(); vi.unstubAllGlobals() })
```

jsdom doesn't implement every browser API (`matchMedia`, `IntersectionObserver`, `ResizeObserver`, `scrollTo`), so stub them in the setup file when components use them ([04](./04-mocking-and-msw.md#browser-apis-jsdom-doesnt-provide)).

## Test environments

| Environment | What it is | Use |
|---|---|---|
| **`node`** (default) | No DOM | Pure logic, utilities, server code |
| **`jsdom`** | JS implementation of the DOM | Component tests (the common choice) |
| **`happy-dom`** | A lighter, faster DOM implementation | An alternative to jsdom; a few APIs differ |
| **Browser mode** | Runs tests in a real browser | Tests needing real layout, CSS, or browser APIs (check current docs for status) |

Set per file with a comment if needed:

```ts
// @vitest-environment node
```

jsdom has **no layout engine**: `getBoundingClientRect` returns zeros and elements have no real size or visibility from layout. Tests that depend on actual layout (virtualization, measuring) belong in a real browser (browser mode or Playwright, [06](./06-e2e-testing-playwright.md)).

## Coverage

```bash
npm install -D @vitest/coverage-v8
```

```ts
// vite.config.ts → test
coverage: {
  provider: "v8",
  reporter: ["text", "html"],
  include: ["src/**/*.{ts,tsx}"],
  exclude: ["src/**/*.test.*", "src/test/**", "src/main.tsx"],
}
```

```bash
npx vitest run --coverage     # prints a table; open coverage/index.html for line-by-line detail
```

Use it to **find untested code**, not as a score ([coverage](./00-testing-fundamentals.md#coverage)).

## Debugging tests

- **`screen.debug()`** prints the current DOM. **`screen.logTestingPlaygroundURL()`** suggests queries ([02](./02-component-testing-with-rtl.md)).
- **`it.only`** / `-t` to isolate.
- **Vitest UI**: `npm i -D @vitest/ui` then `vitest --ui` for a browser view of tests, with module graph and re-run controls.
- **Debugger**: in VS Code, use a JavaScript Debug Terminal and run `npx vitest --no-file-parallelism` with `debugger` statements, or the Vitest VS Code extension.
- Add `--reporter=verbose` to see every test name.

More in [07 — Debugging](./07-debugging.md#debugging-tests).

## Differences from Jest

If you know Jest, the API maps almost one to one:

| Jest | Vitest |
|---|---|
| `jest.fn()`, `jest.spyOn()` | `vi.fn()`, `vi.spyOn()` |
| `jest.mock()` | `vi.mock()` (hoisted; ESM-aware) |
| `jest.useFakeTimers()` | `vi.useFakeTimers()` |
| Config in `jest.config.js` + Babel/ts-jest | Reuses `vite.config.ts` |
| CommonJS-first | ESM-first |
| Globals on by default | Opt in with `globals: true` |

Most Jest tutorials work after swapping `jest` → `vi`.

## Common mistakes

- **Forgetting `await`** on async assertions (`resolves`, `rejects`) or user-event calls, so tests pass vacuously or race.
- **Not restoring** spies, timers, env, and globals, which leak into later tests.
- **Variables in the `vi.mock` factory** (hoisting breaks them; use `vi.hoisted`).
- **Committing `it.only`/`it.skip`.**
- **jsdom assumptions**: expecting real layout, `matchMedia`, or `IntersectionObserver` to exist.
- **Missing the jest-dom import**, so matchers like `toBeInTheDocument` are undefined.
- **Shared mutable state** across tests (modules, singletons, stores). Reset it in `beforeEach`.
- **Fake timers left on**, hanging `waitFor` and subsequent tests.
- **Mocking your own modules heavily** instead of the boundary (network).
- **A separate config that drifts** from the app's aliases and plugins.

## Quick summary

- Vitest = Jest-compatible runner that reuses your Vite config; install it with `jsdom`, RTL, `jest-dom`, and `user-event`.
- Config: `test.environment: "jsdom"`, `globals`, a `setupFiles` entry importing `@testing-library/jest-dom/vitest`.
- `vi.fn()` for mock functions, `vi.spyOn` to wrap, `vi.mock` (hoisted) for modules, `vi.useFakeTimers()` for time; always restore.
- `await` everything async; use `test.each` for tables; filter with `-t`; watch mode while developing, `vitest run` in CI.
- jsdom has no layout; stub missing browser APIs in setup; coverage reveals gaps, not quality.

## Next

[02 — Component testing with RTL](./02-component-testing-with-rtl.md)
