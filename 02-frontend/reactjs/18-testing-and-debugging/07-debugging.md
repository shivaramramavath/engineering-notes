# Debugging

Debugging is the skill of turning *"it's broken"* into *"here is the exact line and the reason"*. Tools help, but the method matters more: reproduce, isolate, form a hypothesis, test it, fix the **cause**, and add a test so it stays fixed.

## A repeatable method

```text
1. Reproduce      make it fail reliably (exact steps, data, browser)
2. Observe        read the actual error, the actual values: don't guess
3. Isolate        narrow where: which component, which request, which commit?
4. Hypothesize    one specific, testable explanation
5. Test it        change ONE thing or inspect ONE value to confirm or refute
6. Fix the cause  not the symptom
7. Regress        write a failing test first, then verify it passes
```

Habits that save hours:

- **Read the error message fully**, including the component stack and the first line of *your* code in the trace. Most errors state their cause.
- **Don't change five things at once.** You won't know which one mattered.
- **Bisect**: comment out half the code or tree and see if the bug persists, then repeat. `git bisect` does the same across commits ("when did this break?").
- **Make the failing case smaller.** A minimal reproduction (a few components in a fresh sandbox) often reveals the cause by itself, and it's what you'll need if you report a bug.
- **Explain it aloud** (rubber-duck). Stating your assumptions often exposes the wrong one.
- **Question assumptions**: is this code actually running? Is it the version you think? Is it the data you think?

## Browser DevTools

### Breakpoints beat `console.log`

In **Sources**, click a line number to pause execution there and inspect everything: local variables, the call stack, closures, `this`.

- **`debugger;`** statement pauses when DevTools is open.
- **Conditional breakpoints** (right-click → *Add conditional breakpoint*): pause only when `item.id === "42"`. Essential inside loops and renders.
- **Logpoints**: print a value without editing code.
- **Step controls**: over, into, out; **Call stack** shows how you got here, and async stack traces follow promises across `await`.
- **Pause on exceptions** (the stop-sign icon): breaks at the throw site, even when it's caught.
- **Blackbox** library code (right-click a file → *Ignore*), so stepping skips `node_modules` internals.
- **Watch expressions** and the **Scope** panel show live values as you step.

### Console

```ts
console.log({ user, items })           // wrap in an object: labels each value
console.table(rows)                    // arrays of objects as a table
console.group("render"); …; console.groupEnd()
console.trace("who called me?")        // prints the call stack
console.time("filter"); …; console.timeEnd("filter")
console.assert(count >= 0, "count went negative", count)
console.dir(domNode)                   // object view of a DOM node
```

In the console, **`$0`** is the element currently selected in the Elements panel, and `copy(obj)` copies a value to your clipboard. Remove stray `console.log`s before committing (an ESLint `no-console` rule helps).

### Network panel

For anything involving the API ([API integration](../11-api-integration/README.md)):

- Is the request **sent** at all? To the **right URL**, with the right **method, headers, and payload**?
- What's the **status** and the **response body**? (A 200 with an error body is a classic.)
- **Preserve log** across navigations, and **disable cache** to rule out stale responses.
- **Copy as fetch / cURL** to replay a request outside the app (or in a terminal) and isolate client from server.
- **Throttling** (slow 4G, offline) reproduces race conditions and loading bugs.
- **CORS errors** appear as a generic `TypeError: Failed to fetch`. The Network panel and console show the real reason, and the fix is on the server or a dev proxy ([fetch and CORS](../11-api-integration/00-fetch.md#headers-credentials-and-cors)).

### Elements and Sources

- **Elements**: inspect the *actual* DOM and computed styles. Is the element there? Which CSS rule wins? Toggle rules to test fixes.
- **Source maps** map minified/bundled code back to your source. If breakpoints land in unreadable code, check that source maps are enabled in dev and uploaded to your monitoring tool for production.

## React DevTools

Install the **React Developer Tools** extension. It adds two tabs:

### Components

- **Browse the component tree**, and select a component to see its **props, state, hooks, and context** with live values.
- **Edit** props and state in the panel to test scenarios without changing code.
- **Search** components by name; **inspect the owner** (who rendered it) and *"Rendered by"* chain.
- **Jump to source** from a component, or log it to the console.
- Settings: **highlight updates when components render** (flashing borders show what re-renders on each interaction), and **hide components** you don't care about.
- With the [React Compiler](../15-concurrent-and-modern-react/06-react-compiler.md), compiled components show a **Memo ✨** badge.

### Profiler

Records commits so you can see **what rendered, why, and how long it took** ([profiling](../14-performance/00-profiling-and-measuring.md#react-devtools-profiler)). Enable *"Record why each component rendered"* to see whether a render came from changed props, state, hooks, a parent render, or context.

## Library-specific devtools

| Tool | Shows |
|---|---|
| **TanStack Query Devtools** | Every query's key, status (fresh/stale/fetching), data, observers; trigger refetch/invalidate ([TanStack Query](../12-server-state/03-tanstack-query.md#setup)) |
| **Redux DevTools** (also for Zustand with `devtools` middleware) | Action log, state diffs, time travel ([Redux Toolkit](../13-state-management/04-redux-toolkit.md#debugging-and-testing)) |
| **React Router** | Route match, loader data, and errors via `useRouteError` / the error element |
| **Playwright trace viewer** | A timeline of an E2E run ([06](./06-e2e-testing-playwright.md#debugging-e2e-failures)) |

For "why is the data wrong?" bugs, the Query devtools often answer immediately: wrong key, stale entry, or a request that never fired.

## Common React errors and what they mean

| Message | Usual cause | Fix |
|---|---|---|
| **Too many re-renders** / **Maximum update depth exceeded** | `setState` called during render, or in an effect whose dependencies it changes, creating a loop | Move the update into an event handler or effect with correct deps; for `onClick={fn()}` pass `onClick={fn}` or `() => fn()` |
| **Cannot update a component while rendering a different component** | A child's render triggers a parent's `setState` | Move the update to an effect or event handler |
| **Objects are not valid as a React child** | Rendering a plain object (or a Promise/Date) in JSX | Render a string/number or map over an array; `JSON.stringify` to inspect |
| **Each child in a list should have a unique "key" prop** | Missing/duplicate keys | Stable unique keys ([reconciliation](../17-react-internals/01-reconciliation.md#what-makes-a-good-key)) |
| **Invalid hook call** | Hook outside a component, a conditional hook, or two copies of React / mismatched `react` and `react-dom` | [How hooks work](../17-react-internals/02-how-hooks-work.md#invalid-hook-call) |
| **Rendered more/fewer hooks than during the previous render** | Hook call order changed (an early `return` or condition above a hook) | Move hooks above any return/condition |
| **Hydration failed / text content did not match** | Server and client first render differ (dates, randomness, `window`) | [Hydration mismatches](../15-concurrent-and-modern-react/07-server-components-and-ssr.md#hydration-mismatches) |
| **Cannot read properties of undefined (reading 'x')** | Data not loaded yet, or an unexpected shape | Handle loading/empty states; optional chaining; validate the response |
| **`x is not a function`** | Prop not passed, or passing the wrong thing | Check the parent, and typing |
| **Failed to fetch dynamically imported module** | Stale deployment chunk | [Chunk load errors](../14-performance/03-code-splitting-and-lazy-loading.md#chunk-load-errors-and-deployments) |
| **Missing/duplicate `act()` warning** (tests) | Async update after the test finished | Wait for it with `findBy*` ([02](./02-component-testing-with-rtl.md#act-warnings)) |

When an error appears inside a component, the **component stack** in the console tells you which component threw. Start there.

## Common bug patterns

| Symptom | Likely cause |
|---|---|
| UI shows an old value after `setState` | Reading state in the same handler (it's a snapshot); use the next render, an effect, or the functional update |
| Handler or interval uses outdated values | **Stale closure**: fix dependencies, use functional updates, or a ref ([hooks](../17-react-internals/02-how-hooks-work.md#closures-why-values-go-stale)) |
| Effect runs on every render | An object/function dependency recreated each render |
| Effect runs twice in development | StrictMode's deliberate mount-cleanup-mount; make cleanup correct |
| Fetch fires in an endless loop | Unstable dependency or `setState` in the effect that retriggers it |
| State resets unexpectedly | Component remounted: changed `key`, new component identity (defined inside another component), or changed wrapper type ([reconciliation](../17-react-internals/01-reconciliation.md)) |
| Input value jumps to the wrong row after delete | Index used as `key` |
| Whole page re-renders on every keystroke | State too high in the tree ([rendering performance](../14-performance/01-rendering-performance.md)) |
| Data from the previous page flashes | Query key missing a variable, or no `placeholderData` strategy |
| "Works locally, breaks in production" | Env vars, build minification, CORS, base path, cached old bundle, or SSR/hydration |
| Click does nothing | Overlay covering it, `pointer-events: none`, disabled, or handler on the wrong element |
| `onClick` fires immediately on render | `onClick={doThing()}` instead of `onClick={doThing}` |
| Layout/position bugs only in some browsers | CSS support differences; test in the real browser |

## Debugging state and data flow

When the screen shows the wrong thing, trace **where the wrong value originated**:

1. **What does the component actually receive?** (React DevTools props/state.)
2. **Where did that come from?** Parent props? Context? A store? The query cache?
3. **Is the source wrong, or is the display logic wrong?** Check the Query devtools entry, the Redux/Zustand state, or the network response.
4. **Is it the right *moment*?** Stale cache, race between requests, or an update that never fired.

Walk **backwards** from the symptom, one hop at a time, until you find the first place the value is wrong.

### Race conditions

Symptoms: results for a *previous* input appear, flicker, or occasionally wrong. Check the Network panel's timing. Responses out of order mean a missing cancellation, or a cache entry not keyed to the input ([fetching data](../12-server-state/01-fetching-data.md#race-conditions)).

## Debugging tests

- **`screen.debug()`** prints the DOM at that moment; `logRoles` lists roles; `logTestingPlaygroundURL()` suggests queries ([02](./02-component-testing-with-rtl.md#finding-the-right-query)).
- Read the **failure message**: `getByRole` errors list the available roles and names.
- **Isolate**: `it.only` or `vitest -t "name"`, then remove `.only`.
- **A test passing alone but failing in the suite** means shared state: caches, stores, mocks, timers, storage ([integration isolation](./05-integration-testing.md#test-isolation)).
- **Flaky async** usually means asserting before the work is done. Use `findBy*`/`waitFor`, never fixed sleeps.
- **Debugger in tests**: run Vitest from a VS Code JavaScript Debug Terminal (or `node --inspect-brk`), add breakpoints or `debugger`, and run single-threaded if needed. [Vitest UI](./01-vitest.md#debugging-tests) helps too.
- **E2E**: the Playwright trace viewer shows exactly what the browser saw ([06](./06-e2e-testing-playwright.md#debugging-e2e-failures)).
- **When you fix a bug, write the failing test first.** It proves you understood the bug and prevents its return.

## Debugging production issues

You can't open DevTools on a user's machine, so rely on **observability**:

- **Error monitoring** (Sentry or similar) captures exceptions with stack traces, component stacks, breadcrumbs, release version, and browser info. Upload **source maps** so traces are readable ([error monitoring](../19-production/06-error-monitoring-and-logging.md)). Report from React 19's root error hooks ([error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md#react-19-root-level-error-hooks)).
- **Include context**: user ID (not personal data), route, feature flags, release, request IDs.
- **Reproduce with the same conditions**: browser, locale, viewport, data, network. Try the **production build** locally (`vite build && vite preview`).
- **Check what changed**: a recent deploy, dependency bump, config or backend change. `git bisect` finds the commit.
- **Never log secrets** (tokens, passwords, personal data) in console output or error reports.

## Useful techniques

- **`useDebugValue`**: labels a custom hook's value in React DevTools.
- **A `useWhyDidYouRender`-style hook** (log which props changed between renders), but prefer the Profiler's "why did this render".
- **StrictMode** surfaces impure renders and missing effect cleanup in development. Keep it on.
- **ESLint** (`react-hooks/exhaustive-deps`, `rules-of-hooks`) catches a large share of hook bugs before you run anything.
- **TypeScript errors** often point at the real mistake. Don't silence with `any` or `as` until you understand why it complains.
- **Feature flags / environment toggles** to switch behavior and bisect quickly.
- **Logging with structure**: `console.log("[checkout]", { step, cartId })` is searchable and clear.

## Common mistakes

- **Guessing and changing code** before reproducing and observing the failure.
- **Changing multiple things at once.**
- **Only using `console.log`**, never breakpoints, the Network panel, or React DevTools.
- **Ignoring the first error** in the console (later ones are often consequences).
- **Not reading the component stack or the full error message.**
- **Treating the symptom** (suppressing the warning, adding a `setTimeout`, a `?.` everywhere) instead of the cause.
- **Disabling `exhaustive-deps` or using `any`** to silence the tool that was right.
- **Debugging against stale data**: cached responses, old service worker, old bundle. Hard reload and disable cache.
- **Assuming "it works on my machine"** means it's fine, with no check of the production build.
- **Fixing a bug without a regression test.**
- **Leaving debug code behind** (`console.log`, `debugger`, `.only`).
- **Skipping a minimal reproduction**, and fighting the whole app's complexity.
- **Logging sensitive data.**

## Quick summary

- Use a **method**: reproduce → observe → isolate → hypothesize → test one thing → fix the cause → add a regression test.
- **Breakpoints** (conditional, logpoints, pause-on-exception) and the **Network** panel beat guessing with `console.log`.
- **React DevTools** shows props/state/hooks live and the Profiler explains re-renders; **Query/Redux devtools** reveal data-layer problems.
- Learn the common error messages and bug patterns (stale closures, unstable deps, remounts, index keys, race conditions): most bugs are one of these.
- For wrong-data bugs, **trace backwards** hop by hop to find where the value first goes wrong.
- Debug tests with `screen.debug()`, isolation, and traces; in production, rely on **error monitoring with source maps**.
- Write the **failing test first**, then fix.

## Next

Continue to [19 — Production](../19-production/README.md).
