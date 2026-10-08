# E2E Testing with Playwright

An **end-to-end (E2E) test** drives your real app, built and served, in a **real browser**, the way a user would: navigating, clicking, typing, and checking what appears. It's the only layer that verifies the *whole system works together*: your bundle, routing, CSS, browser behavior, and (optionally) the real backend.

E2E tests are the **slowest and most expensive** tests, so you write **few** of them, for the flows that matter most (sign up, log in, checkout), and leave the rest to cheaper layers ([the trophy](./00-testing-fundamentals.md#the-testing-trophy-not-the-pyramid)).

**[Playwright](https://playwright.dev)** is a popular E2E tool: it drives Chromium, Firefox, and WebKit; **auto-waits** for elements; has first-class tracing; and runs tests in parallel. (Cypress is the other common choice; the concepts here mostly transfer.)

## Setup

```bash
npm init playwright@latest
```

This creates `playwright.config.ts`, an `e2e/` (or `tests/`) folder, an example test, and offers to install browsers and a CI workflow. If you need browsers later:

```bash
npx playwright install --with-deps      # --with-deps installs OS libraries on Linux (CI)
```

### Configuration

```ts
// playwright.config.ts
import { defineConfig, devices } from "@playwright/test"

export default defineConfig({
  testDir: "./e2e",
  fullyParallel: true,
  forbidOnly: !!process.env.CI,                  // fail CI if someone commits test.only
  retries: process.env.CI ? 2 : 0,
  workers: process.env.CI ? 1 : undefined,
  reporter: [["html"], ["list"]],

  use: {
    baseURL: "http://localhost:4173",
    trace: "on-first-retry",                     // record a trace when a test is retried
    screenshot: "only-on-failure",
  },

  projects: [
    { name: "chromium", use: { ...devices["Desktop Chrome"] } },
    { name: "firefox", use: { ...devices["Desktop Firefox"] } },
    { name: "webkit", use: { ...devices["Desktop Safari"] } },
    { name: "mobile", use: { ...devices["Pixel 7"] } },
  ],

  webServer: {
    command: "npm run build && npm run preview -- --port 4173",   // test the PRODUCTION build
    url: "http://localhost:4173",
    reuseExistingServer: !process.env.CI,
  },
})
```

Notable settings:

- **`webServer`** starts your app before the tests and stops it afterward. Testing the **production build** (`vite preview`) catches bugs the dev server hides (minification, chunking, missing env vars). For a faster dev loop you can point at `npm run dev`.
- **`baseURL`** lets tests use relative paths (`page.goto("/projects")`).
- **`projects`** run the same tests across browsers and viewports. Start with Chromium, and add others once the suite is stable.
- **`trace: "on-first-retry"`** records a full trace for failures without slowing every run.
- **`forbidOnly`** stops a stray `test.only` from silently disabling the rest of the suite in CI.

## Your first test

```ts
// e2e/projects.spec.ts
import { test, expect } from "@playwright/test"

test("a user can create a project", async ({ page }) => {
  await page.goto("/projects")

  await page.getByRole("button", { name: "New project" }).click()
  await page.getByLabel("Name").fill("Launch plan")
  await page.getByRole("button", { name: "Create" }).click()

  await expect(page.getByText("Project created")).toBeVisible()
  await expect(page.getByRole("row", { name: /launch plan/i })).toBeVisible()
  await expect(page).toHaveURL(/\/projects/)
})
```

```bash
npx playwright test                    # run everything, headless
npx playwright test --ui               # interactive UI mode: pick tests, watch, time-travel
npx playwright test --headed           # watch the browser
npx playwright test projects.spec.ts   # one file
npx playwright test -g "create a project"   # by name
npx playwright show-report             # open the HTML report
```

## Locators and auto-waiting

Playwright's design removes most flakiness by **waiting automatically**. Each action waits for the element to be attached, visible, stable, enabled, and receiving events *before* acting. Assertions retry until they pass or time out.

### Locators: same philosophy as Testing Library

Find elements the way users and assistive tech do:

```ts
page.getByRole("button", { name: "Save" })      // preferred
page.getByLabel("Email")
page.getByPlaceholder("Search…")
page.getByText("Welcome back")
page.getByAltText("Company logo")
page.getByTestId("projects-skeleton")           // data-testid: last resort
page.locator("css=.something")                  // CSS/XPath: avoid unless necessary

page.getByRole("row", { name: /ana/i }).getByRole("button", { name: "Delete" })   // chain to narrow
page.getByRole("listitem").filter({ hasText: "Roadmap" })
```

A locator is a **lazy description**, not a found element. It's resolved fresh each time it's used, so it stays valid across re-renders (unlike a stored DOM node). By default a locator that matches multiple elements causes an error in strict mode, which forces you to be specific.

### Web-first assertions

```ts
await expect(locator).toBeVisible()
await expect(locator).toHaveText("Saved")
await expect(locator).toContainText(/saved/i)
await expect(locator).toBeDisabled()
await expect(locator).toHaveValue("ana@example.com")
await expect(locator).toHaveCount(3)
await expect(page).toHaveURL("/dashboard")
await expect(page).toHaveTitle(/Projects/)
```

These **retry** until the condition holds (default ~5 s). Don't pull values out and assert with plain `expect`, which doesn't wait:

```ts
// ✗ checks once, immediately: races with rendering
expect(await page.getByText("Saved").isVisible()).toBe(true)

// ✓ web-first: waits
await expect(page.getByText("Saved")).toBeVisible()
```

### Never use fixed waits

```ts
await page.waitForTimeout(2000)   // ✗ slow when too long, flaky when too short
```

Wait for a **condition**: an assertion, `await page.waitForURL(...)`, `await locator.waitFor()`, or `await page.waitForResponse(...)`. If you reach for `waitForTimeout`, you're missing an observable signal. Find it.

## Actions

```ts
await page.goto("/login")
await locator.click()
await locator.fill("text")                 // sets the value (clears first)
await locator.pressSequentially("slowly")  // types key by key (rarely needed)
await locator.press("Enter")
await locator.check() / uncheck()
await locator.selectOption("admin")
await locator.hover()
await locator.setInputFiles("fixtures/avatar.png")
await page.keyboard.press("Escape")
await locator.dragTo(other)
await page.goBack()
```

## Authentication: log in once, reuse it

Logging in through the UI before every test is slow and fragile. Log in **once** in a setup step, save the session to a file, and let tests load it:

```ts
// e2e/auth.setup.ts
import { test as setup, expect } from "@playwright/test"

const authFile = ".auth/user.json"

setup("authenticate", async ({ page }) => {
  await page.goto("/login")
  await page.getByLabel("Email").fill(process.env.E2E_USER_EMAIL!)
  await page.getByLabel("Password").fill(process.env.E2E_USER_PASSWORD!)
  await page.getByRole("button", { name: "Sign in" }).click()
  await expect(page).toHaveURL("/dashboard")
  await page.context().storageState({ path: authFile })     // cookies + localStorage
})
```

```ts
// playwright.config.ts (projects)
projects: [
  { name: "setup", testMatch: /.*\.setup\.ts/ },
  {
    name: "chromium",
    use: { ...devices["Desktop Chrome"], storageState: ".auth/user.json" },
    dependencies: ["setup"],                    // run setup first
  },
],
```

- Add `.auth/` to `.gitignore`. It contains session credentials.
- Use a **dedicated test account**, with credentials from environment variables, never committed.
- Keep **one dedicated test of the real login flow** that doesn't use the saved state.
- If the app keeps its access token **in memory** ([authentication](../11-api-integration/03-authentication.md#where-to-store-a-token)), `storageState` only restores cookies/storage. The app's silent refresh on load then has to re-obtain the token, which is a good thing to exercise.

## Test data and isolation

E2E tests share a running system, so isolation is your job:

- **Create the data each test needs**, preferably through the **API** (fast and reliable) rather than clicking through the UI:

```ts
test("a user can delete a project", async ({ page, request }) => {
  const res = await request.post("/api/projects", { data: { name: "To delete" } })   // uses baseURL
  const { id } = await res.json()

  await page.goto("/projects")
  await page.getByRole("row", { name: /to delete/i }).getByRole("button", { name: "Delete" }).click()
  await page.getByRole("button", { name: "Confirm" }).click()
  await expect(page.getByRole("row", { name: /to delete/i })).toHaveCount(0)
})
```

- **Use unique names** (`Project ${Date.now()}`) so parallel tests don't collide.
- **Clean up** what you create, or run against a database that's reset between runs.
- Don't depend on **test order** or leftover data from previous tests.
- Run against a **dedicated test backend**, never production.

## Mocking the network (sparingly)

E2E's value is *real* integration, so don't mock the backend by default. But for hard-to-produce situations, intercept specific requests:

```ts
test("shows an error banner when the API is down", async ({ page }) => {
  await page.route("**/api/projects", (route) => route.fulfill({ status: 500, json: { message: "boom" } }))
  await page.goto("/projects")
  await expect(page.getByRole("alert")).toContainText(/something went wrong/i)
})
```

Useful for error states, slow responses (`route.fulfill` after a delay), and third-party services (payments, maps). Those cases are usually cheaper to cover in [integration tests with MSW](./05-integration-testing.md#testing-the-unhappy-paths), so reserve E2E interception for what truly needs a real browser.

## What belongs in E2E

**Do E2E:**

- Critical user journeys: sign up, log in/out, checkout, core create-edit-delete flow
- Things only a real browser can verify: layout at breakpoints, real focus and keyboard behavior, file upload/download, drag and drop, redirects and deep links, **the deployed build**
- Smoke tests after deployment ("the app loads and key pages render")

**Don't E2E:**

- Every validation message and edge case (integration/unit tests are far cheaper)
- Pure logic
- Variations of the same flow

A small suite of reliable E2E tests beats a large, flaky one. If a test fails intermittently, fix it or delete it. Unreliable tests train the team to ignore failures.

## Structuring tests

```ts
test.describe("projects", () => {
  test.beforeEach(async ({ page }) => { await page.goto("/projects") })

  test("lists existing projects", async ({ page }) => { /* … */ })
  test("filters by status", async ({ page }) => { /* … */ })
})
```

Each test should be independent and runnable alone. Prefer **several short, focused tests** over one marathon.

### Page objects (optional)

Wrap a page's locators and actions to avoid repeating selectors:

```ts
// e2e/pages/projects-page.ts
import type { Page } from "@playwright/test"

export class ProjectsPage {
  constructor(private page: Page) {}
  async goto() { await this.page.goto("/projects") }
  async create(name: string) {
    await this.page.getByRole("button", { name: "New project" }).click()
    await this.page.getByLabel("Name").fill(name)
    await this.page.getByRole("button", { name: "Create" }).click()
  }
  row(name: RegExp) { return this.page.getByRole("row", { name }) }
}
```

Useful once the same UI appears in many tests. Don't over-engineer it for a handful of tests, and keep **assertions** in the tests, not hidden in page objects.

### Fixtures

Playwright **fixtures** provide setup per test, such as an authenticated page or a seeded project, with automatic teardown. Extending `test` with your own fixtures is the idiomatic way to share setup. See the Playwright docs on fixtures.

## Accessibility and visual checks

- **Accessibility**: `@axe-core/playwright` runs axe on a live page:

```ts
import AxeBuilder from "@axe-core/playwright"
const results = await new AxeBuilder({ page }).analyze()
expect(results.violations).toEqual([])
```

Automated checks catch only some issues. They complement manual testing ([accessibility checklist](../08-accessibility/04-accessibility-checklist.md)).

- **Visual comparisons** (`await expect(page).toHaveScreenshot()`) catch unintended style changes. They're sensitive to fonts, OS, and browser version, so generate and compare baselines in the **same environment** (typically CI or Docker), keep them few, and mask dynamic content. Many teams skip them.

## Debugging E2E failures

Playwright's tooling is a major strength:

- **Trace viewer**: a timeline of every action with DOM snapshots, network, console, and screenshots at each step.

```bash
npx playwright test --trace on          # record always (or rely on on-first-retry in CI)
npx playwright show-trace trace.zip     # or open from the HTML report
```

- **UI mode** (`--ui`): run, watch, and time-travel through tests interactively.
- **Debug mode** (`--debug`, or `await page.pause()`): step through with the Playwright Inspector.
- **Codegen** (`npx playwright codegen http://localhost:4173`): records your clicks into locators and code. Use it to *discover* good locators, then clean up the output.
- **Headed mode** and `--slow-mo` to watch.
- Failures in CI: download the **HTML report and traces** as build artifacts.

Common causes of failures: a locator that's ambiguous or changed text, not waiting on a real signal, shared data between tests, a race with a background request, or an environment difference (viewport, timezone, locale). Reproduce locally with the same browser project and `--repeat-each=20` to expose flakiness.

## Running in CI

```yaml
# .github/workflows/e2e.yml (excerpt)
- run: npm ci
- run: npx playwright install --with-deps chromium
- run: npx playwright test --project=chromium
- uses: actions/upload-artifact@v4
  if: ${{ !cancelled() }}
  with:
    name: playwright-report
    path: playwright-report/
    retention-days: 14
```

- Install **only the browsers you use** in CI to save time.
- **Upload the report and traces** so failures are diagnosable.
- Shard across machines for large suites (`--shard=1/4`).
- Run the full browser matrix on a schedule or before release, and a Chromium smoke set on every PR.
- Cache browsers and dependencies to speed up runs. See [CI/CD](../19-production/04-ci-cd.md).

## Playwright component testing?

Playwright also offers an experimental component-testing mode (mounting components in a real browser). For most teams, Vitest + RTL for components and Playwright for full-app flows is simpler. Check current docs if you need real-browser component tests (virtualized lists, layout-dependent widgets).

## Common mistakes

- **Writing too many E2E tests**, producing a slow, flaky suite. Keep them for critical flows.
- **`waitForTimeout`** instead of waiting on an observable condition.
- **Plain `expect` on extracted values** (`isVisible()`, `textContent()`) instead of web-first assertions that retry.
- **Brittle locators** (CSS classes, nth-child) instead of roles, labels, and text.
- **Logging in via the UI in every test**, instead of reusing `storageState`.
- **Committing `.auth/` or credentials.**
- **Tests that depend on each other** or on leftover data; no isolation or cleanup.
- **Testing against production**, or a shared, unstable environment.
- **Mocking the whole backend** and losing the point of E2E.
- **Testing the dev server only**, missing production-build issues.
- **Ignoring flaky tests** (retries hide them) instead of diagnosing with traces.
- **Visual snapshots generated on a different OS** than CI, causing constant diffs.
- **No artifacts uploaded in CI**, so failures can't be debugged.

## Quick summary

- E2E tests drive the **real app in a real browser**. They're the slowest layer, so keep them **few and focused on critical journeys**.
- Playwright **auto-waits** and its **web-first assertions retry**. Never use fixed sleeps, and never extract values to assert without waiting.
- Locate by **role, label, and text** (like Testing Library); use `data-testid` as a last resort.
- **Log in once** via a setup project and `storageState`; use a dedicated test account and gitignore the saved state.
- **Isolate data**: create it via API, use unique names, clean up; test the **production build**.
- Mock the network only for hard-to-produce cases; cheaper layers (MSW) cover most error states.
- Debug with **trace viewer**, UI mode, and codegen; in CI, install only needed browsers and upload reports.

## Next

[07 — Debugging](./07-debugging.md)
