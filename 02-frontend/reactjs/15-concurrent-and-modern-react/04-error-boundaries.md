# Error Boundaries

By default, an error thrown while React is **rendering** takes down the **entire component tree**: users see a blank white page. An **error boundary** catches render errors in its subtree and shows a fallback instead, so a bug in one widget doesn't destroy the whole app.

```text
<App>
 ├─ <Header />                      ← keeps working
 ├─ <ErrorBoundary>                 ← catches errors from below
 │    └─ <RevenueChart />           ← throws during render → fallback shown here only
 └─ <Footer />                      ← keeps working
```

## Boundaries are still class components

There's no hook equivalent. Error boundaries rely on two class lifecycle methods:

```tsx
import { Component, type ErrorInfo, type ReactNode } from "react"

type Props = { fallback: ReactNode; children: ReactNode }
type State = { hasError: boolean }

export class ErrorBoundary extends Component<Props, State> {
  state: State = { hasError: false }

  static getDerivedStateFromError(): State {
    return { hasError: true }                         // render the fallback on the next render
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    reportError(error, info.componentStack)           // side effect: log it
  }

  render() {
    return this.state.hasError ? this.props.fallback : this.props.children
  }
}
```

- **`getDerivedStateFromError`**: pure; updates state so the fallback renders.
- **`componentDidCatch`**: for side effects such as logging to a monitoring service ([error monitoring](../19-production/06-error-monitoring-and-logging.md)).

You write this class once, or (better) use a library.

## Use `react-error-boundary`

The `react-error-boundary` package wraps the class and adds what you actually need: resetting, function fallbacks, and a hook.

```bash
npm install react-error-boundary
```

```tsx
import { ErrorBoundary, type FallbackProps } from "react-error-boundary"

function Fallback({ error, resetErrorBoundary }: FallbackProps) {
  return (
    <div role="alert" className="rounded border p-4">
      <p className="font-medium">Something went wrong.</p>
      <pre className="text-sm text-muted-foreground">{(error as Error).message}</pre>
      <button onClick={resetErrorBoundary}>Try again</button>
    </div>
  )
}

<ErrorBoundary
  FallbackComponent={Fallback}
  onError={(error, info) => reportError(error, info.componentStack)}
  onReset={() => { /* clear state/caches that caused the failure */ }}
>
  <RevenueChart />
</ErrorBoundary>
```

Useful props:

| Prop | Purpose |
|---|---|
| `fallback` / `fallbackRender` / `FallbackComponent` | What to render on error (element, function, or component) |
| `onError` | Log/report the error |
| `onReset` | Run when the user retries: reset query state, clear stores |
| `resetKeys` | Array of values: when any change, the boundary **resets automatically** |

`resetKeys` is the elegant fix for "error persists after navigating":

```tsx
<ErrorBoundary resetKeys={[projectId]} FallbackComponent={Fallback}>
  <ProjectView id={projectId} />       {/* new project → boundary clears the old error */}
</ErrorBoundary>
```

## What boundaries catch (and don't)

| Caught | **Not** caught |
|---|---|
| Errors thrown while **rendering** | Errors in **event handlers** |
| Errors in **lifecycle methods/effects of descendants** during commit | **Asynchronous** code (`setTimeout`, promises, `async` handlers) |
| Errors thrown by **hooks** and **`use(promise)`** rejections | **Server-side rendering** errors |
| Errors in child **constructors** | Errors **inside the boundary itself** (it can't catch its own) |

Event handlers don't need boundaries, because they aren't part of rendering. Wrap them in `try/catch` and show UI feedback ([API error handling](../11-api-integration/05-api-error-handling.md)):

```tsx
async function onSave() {
  try { await save() } catch (e) { toast.error(getErrorMessage(e)) }
}
```

### Sending async errors to a boundary

If you do want an async failure to show the boundary fallback, re-throw it **during render**:

```tsx
import { useErrorBoundary } from "react-error-boundary"

function Widget() {
  const { showBoundary } = useErrorBoundary()

  useEffect(() => {
    loadThing().catch(showBoundary)         // routes the error into the nearest boundary
  }, [showBoundary])
}
```

(Without a library: `setState(() => { throw error })` achieves the same thing, because the throw happens during the state update's render.)

## Where to put boundaries

Granularity decides how much UI a failure takes with it.

```text
Root boundary      → last resort: "The app crashed. Reload."
Route boundary     → one page failed; navigation/layout still work
Widget boundary    → one chart/panel/feed failed; the rest of the page works
```

- **Always have a root boundary.** A blank white screen is the worst outcome.
- **Add route-level boundaries.** With React Router, the route's [`errorElement`](../10-routing/05-route-data-loading.md#errors) *is* this, and it also catches loader/action errors.
- **Wrap independent, failure-prone widgets** (third-party embeds, charts, anything depending on messy data) so they degrade alone.
- Don't wrap *every* component. Too many boundaries scatter inconsistent error UI and hide real bugs.

## Recovering

A fallback with no way out is a dead end. Offer:

- **Retry** (`resetErrorBoundary`), after clearing whatever caused the failure.
- **Navigate away/home** when retrying won't help.
- **Reload the page** at the root level.

Retrying only helps if the *cause* is cleared. If a component throws because of bad state, re-rendering with the same state throws again. Reset the data too:

```tsx
<QueryErrorResetBoundary>
  {({ reset }) => (
    <ErrorBoundary onReset={reset} FallbackComponent={Fallback}>
      <Suspense fallback={<Skeleton />}>
        <ProjectList />          {/* useSuspenseQuery */}
      </Suspense>
    </ErrorBoundary>
  )}
</QueryErrorResetBoundary>
```

`QueryErrorResetBoundary` (from TanStack Query) tells failed queries to retry when the boundary resets ([loading and error states](../12-server-state/02-loading-and-error-states.md#suspense-and-error-boundaries)).

## React 19: root-level error hooks

`createRoot` accepts callbacks for central reporting, regardless of which boundary (if any) caught the error:

```tsx
createRoot(rootElement, {
  onCaughtError(error, info) {        // caught by an error boundary
    reportError(error, { componentStack: info.componentStack, handled: true })
  },
  onUncaughtError(error, info) {      // not caught by any boundary
    reportError(error, { componentStack: info.componentStack, handled: false })
  },
  onRecoverableError(error, info) {   // React recovered on its own (e.g. hydration mismatch)
    reportError(error, { componentStack: info.componentStack, recoverable: true })
  },
}).render(<App />)
```

This is the cleanest place to wire up monitoring once, instead of repeating `componentDidCatch` logging everywhere. React 19 also reports each error once rather than the duplicate logging of earlier versions. Check the React docs for the exact `info` shape in your version.

## Fallback UI guidelines

- Say **what** failed and **what the user can do**, not a stack trace. Show technical details only in development or behind a "details" toggle.
- Use `role="alert"` so assistive tech announces it.
- Match the **size** of the failed region to avoid layout jumps (an error card in place of a chart, not a full-page error).
- Never leak secrets or internal details in the message.
- Keep the fallback **extremely simple**. If it can throw, it can crash its parent boundary.

## Testing

- Trigger a render error with a component that throws, and assert the fallback appears and the rest of the page remains ([component testing](../18-testing-and-debugging/02-component-testing-with-rtl.md)).
- React logs caught errors to the console, so silence `console.error` in those tests to keep output readable.
- Verify the retry path actually clears the failure.

## Common mistakes

- **No boundary at all**, so any render error blanks the app.
- **Expecting boundaries to catch event handler or async errors.** They don't. Use `try/catch` or `showBoundary`.
- **One giant root boundary only**, so every failure kills the whole UI.
- **A boundary around everything**, hiding bugs behind generic fallbacks.
- **Retry that doesn't reset the cause**, throwing the same error again.
- **Forgetting `resetKeys`**, so the error stays after the user navigates to a different record.
- **Error boundary inside the Suspense boundary** (it should wrap it).
- **Swallowing errors** (no logging), so production failures are invisible.
- **A fallback that can itself throw.**
- **Showing raw error messages/stack traces** to users.

## Quick summary

- A render error unmounts the whole tree unless an **error boundary** catches it and shows a fallback.
- Boundaries are class components; use **`react-error-boundary`** for `resetKeys`, `onReset`, function fallbacks, and `useErrorBoundary`.
- They catch **render/commit errors only**, not event handlers or async code. Handle those with `try/catch` or `showBoundary`.
- Layer them: **root → route (`errorElement`) → risky widgets**; keep the fallback simple, accessible, and recoverable.
- Pair with Suspense (**boundary outside, Suspense inside**) and reset data on retry (`QueryErrorResetBoundary`).
- Use React 19's `onCaughtError` / `onUncaughtError` / `onRecoverableError` on `createRoot` for central error reporting.

## Next

[05 — React 19 features](./05-react-19-features.md)