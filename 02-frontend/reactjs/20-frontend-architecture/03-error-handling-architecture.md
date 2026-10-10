# Error Handling Architecture

Individual pieces of error handling exist throughout this repo: [API errors](../11-api-integration/05-api-error-handling.md), [error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md), [route `errorElement`s](../10-routing/05-route-data-loading.md#errors), [monitoring](../19-production/06-error-monitoring-and-logging.md). Without an overall design they end up inconsistent: three different error shapes, a toast here, a blank page there, the same failure reported twice and another never.

An **error handling architecture** is the **one strategy** that connects them: how failures are **classified**, where each kind is **handled**, how users **see** them, and how you **find out**.

## Principles

1. **Classify first, then decide.** "Something went wrong" isn't a strategy. Different failures need different responses.
2. **Handle errors at the lowest level that can do something useful**, and let the rest bubble to a boundary that can show *something*.
3. **Normalize early.** Convert the chaos of `unknown` thrown values into one app-wide error type at the edges.
4. **Expected errors are part of the UI; unexpected errors are bugs.** Show the former helpfully and report the latter.
5. **Never fail silently, and never fail whole.** No swallowed errors, and no blank pages from one broken widget.
6. **Every error state offers a next step** (retry, go back, contact support).
7. **Report once, in one place.**

## Expected vs unexpected

| | Expected (operational) | Unexpected (programmer / system) |
|---|---|---|
| Examples | Validation failure, 404 for a deleted record, 403, 409 conflict, offline, rate limit | `undefined is not a function`, malformed response, impossible state, unhandled 500 |
| Cause | The world being messy | A bug or an outage |
| User sees | A specific, helpful message and action | A friendly generic fallback |
| You do | Handle in the UI flow | **Report to monitoring**, fix the bug |
| Report to monitoring? | Usually **no** (it's noise) | **Yes** |

Most of the architecture is about routing each kind to the right place.

## One error type

Turn everything thrown into a single, typed shape as soon as it crosses a boundary:

```ts
// shared/lib/errors/app-error.ts
export type AppErrorKind =
  | "network"        // offline, DNS, CORS, timeout
  | "unauthenticated"// 401 after refresh failed
  | "forbidden"      // 403
  | "not_found"      // 404
  | "validation"     // 400/422 with field details
  | "conflict"       // 409
  | "rate_limited"   // 429
  | "server"         // 5xx
  | "contract"       // response didn't match what we expected
  | "unknown"

export class AppError extends Error {
  kind: AppErrorKind
  status?: number
  code?: string                          // stable machine code from the API
  fields?: Record<string, string[]>      // validation details
  requestId?: string                     // correlation ID
  retryable: boolean
  cause?: unknown

  constructor(init: { kind: AppErrorKind; message: string; status?: number; code?: string; fields?: Record<string, string[]>; requestId?: string; retryable?: boolean; cause?: unknown }) {
    super(init.message)
    this.name = "AppError"
    this.kind = init.kind
    this.status = init.status
    this.code = init.code
    this.fields = init.fields
    this.requestId = init.requestId
    this.retryable = init.retryable ?? (init.kind === "network" || init.kind === "server" || init.kind === "rate_limited")
    this.cause = init.cause
  }
}
```

```ts
export function toAppError(error: unknown): AppError {
  if (error instanceof AppError) return error
  if (error instanceof ApiError) return fromApiError(error)          // status/code → kind
  if (error instanceof ZodError) return new AppError({ kind: "contract", message: "Unexpected response from the server", cause: error })
  if (error instanceof DOMException && error.name === "AbortError") throw error     // cancellations aren't errors
  return new AppError({ kind: "unknown", message: error instanceof Error ? error.message : "Unknown error", cause: error })
}
```

(The client's `ApiError` from [02 — API client](../11-api-integration/02-api-client.md#the-error-type) maps onto this; you can also fold the two together by having the client throw `AppError` directly.)

Benefits: **one `switch (error.kind)`** everywhere instead of ad hoc status-code checks, and one place to change the mapping. Branch on `kind` and `code`, never on message text.

## Where each error is handled

```text
 throw ─► API client ──────► normalized AppError
              │
              ▼
        Query layer ─────── retry policy (only transient kinds), global caches
              │
              ▼
        Component ───────── inline UI: field errors, loading/empty/error states, retry buttons
              │   (not handled here? it keeps bubbling)
              ▼
        Route boundary ──── errorElement / boundary: "this page failed" + navigation intact
              │
              ▼
        Root boundary ───── last resort: "The app hit a problem. Reload."
              │
              ▼
        Monitoring ──────── unexpected errors reported with context
```

Each layer has a job:

### 1. API client: normalize and classify

Convert transport failures and HTTP statuses into `AppError`. Attach the **request ID** from the response header. Handle **401 → refresh → retry** here ([refresh flow](../11-api-integration/04-refresh-token-flow.md)), invisible to everything above. Treat abort as non-error.

### 2. Query layer: policy

Decide **retries** by `kind` (retry network/5xx/429 with backoff; never 4xx), and set **global handlers** for cross-cutting policy ([retries and global handlers](../11-api-integration/05-api-error-handling.md#retries)):

```ts
new QueryClient({
  defaultOptions: { queries: { retry: (count, error) => toAppError(error).retryable && count < 2 } },
  queryCache: new QueryCache({
    onError: (error, query) => {
      const e = toAppError(error)
      if (query.state.data !== undefined) toast.error(messageFor(e))   // background refetch failed: soft notice
      if (isUnexpected(e)) reportError(e)                              // monitoring
    },
  }),
  mutationCache: new MutationCache({
    onError: (error, _v, _c, mutation) => {
      if (mutation.options.onError) return                            // the component already handled it
      handleUnhandled(toAppError(error))
    },
  }),
})
```

### 3. Components: handle what's *expected for this screen*

The component knows its context, so it owns the user-facing handling of expected errors:

- **Loading/empty/error states** for its own data ([loading and error states](../12-server-state/02-loading-and-error-states.md)).
- **Field errors** for forms: map `validation` errors onto fields ([form errors](../11-api-integration/05-api-error-handling.md#validation-errors--form-fields)).
- **Specific recovery**: a `conflict` offers "reload latest", a `not_found` shows a tailored empty state.

Anything it *doesn't* specifically handle should **propagate** (via Suspense/`throwOnError`, or rethrow), not be swallowed with a generic message.

### 4. Boundaries: contain the unexpected

- **Root boundary**: always present; shows "something broke", with reload.
- **Route boundary** (`errorElement`): one failed page doesn't take down navigation.
- **Widget boundaries**: risky or independent widgets (charts, embeds, third-party) fail alone ([placement](../15-concurrent-and-modern-react/04-error-boundaries.md#where-to-put-boundaries)).

The fallback component can switch on `kind` to give better messages (a 404 route error shows "Not found", a 403 shows "No access", the rest a generic apology).

### 5. Monitoring: learn about the unexpected

Report **unexpected** errors (bugs, `contract`, `unknown`, unhandled 5xx) once, from a central point: the root error hooks, the query caches, and boundaries' `onError`. Don't report expected ones. See [monitoring](../19-production/06-error-monitoring-and-logging.md).

## User-facing messages

Translate error `kind`/`code` to user language **in one place**, not scattered across components:

```ts
// shared/lib/errors/messages.ts
export function messageFor(error: AppError): string {
  switch (error.kind) {
    case "network":         return "Can't reach the server. Check your connection and try again."
    case "unauthenticated": return "Your session expired. Please sign in again."
    case "forbidden":       return "You don't have permission to do that."
    case "not_found":       return "We couldn't find what you were looking for."
    case "conflict":        return "This was changed by someone else. Reload to see the latest."
    case "rate_limited":    return "Too many requests. Please wait a moment."
    case "validation":      return error.message
    default:                return "Something went wrong on our side. Please try again."
  }
}
```

- Prefer **specific `code`s** from the API for domain errors ("EMAIL_TAKEN" → "That email is already registered") via a lookup (and your i18n system, [internationalization](../16-advanced-react/03-internationalization.md)).
- **Never show raw server text** for `server`/`unknown` errors. It can leak internals.
- Keep tone consistent: say what happened, and what the user can do.

## Presentation: consistent building blocks

Provide a small set of **shared components**, so every screen handles errors the same way:

| Component | Use |
|---|---|
| `<ErrorState error onRetry />` | A section failed to load (inline, with retry) |
| `<FormError error />` / field error helpers | Form-level and field-level messages |
| `<PageError />` | Route-level fallback (switches on `kind`) |
| `<AppCrash />` | Root fallback |
| `toastError(error)` | Non-blocking notice for background failures ([toasts guidance](../09-ui-components/08-toasts-and-notifications.md#choosing-the-right-feedback)) |

Consistency matters more than any single design. Users learn where errors appear and what to do. Put these in `shared/ui` so features don't invent their own.

## Correlation IDs

When a user reports "it failed", you need to find *that* request across systems:

- Generate or accept a **request ID** (`X-Request-Id`) in the client and read it back from responses. Add it to `AppError.requestId`.
- Include it in **monitoring events** and in server logs.
- Optionally show it to users in the error UI ("Reference: 7f3a9c") so support can look it up.
- If you use distributed tracing, propagate the trace ID the same way ([performance monitoring](../19-production/07-performance-monitoring.md#tracing-and-api-performance)).

## Recovery strategies

Match the recovery to the kind:

| Kind | Recovery |
|---|---|
| `network`, `server`, `rate_limited` | **Automatic retry** (queries) and a manual "Try again" |
| `unauthenticated` | Redirect to login, preserving the destination ([route protection](../10-routing/04-route-protection.md)) |
| `forbidden` | Explain, offer navigation elsewhere; no retry |
| `not_found` | A tailored not-found state; link back |
| `conflict` | Offer to reload/merge; keep the user's input |
| `validation` | Show field errors; keep the form state |
| `contract` / `unknown` | Safe fallback + report; offer reload |
| Stale deployment chunk | Reload once ([chunk errors](../14-performance/03-code-splitting-and-lazy-loading.md#chunk-load-errors-and-deployments)) |

**Preserve user work** wherever possible: never clear a form because the request failed.

## Graceful degradation

Design features so a failure **degrades** instead of destroying the page:

- **Isolate non-critical data**: a failing "recommendations" widget shouldn't block checkout. Give it its own boundary and its own query.
- **Show stale data** with a warning when a refresh fails ([failed refetch](../12-server-state/02-loading-and-error-states.md#2-failed-refetch-with-data-on-screen)).
- **Fail soft on non-essentials** (analytics, feature-flag fetch) with a safe default. **Fail fast on essentials** (can't determine the user's identity).
- **Timeouts** so a hung dependency doesn't hang the UI forever.
- **Feature flags / kill switches** to disable a broken feature without a deploy.

## Domain code and errors

For **pure domain functions**, prefer to make impossible states unrepresentable (types, discriminated unions) and **throw on genuine programmer errors** (invariants) with clear messages. Some teams use a **`Result<T, E>`** return type for *expected* domain failures instead of exceptions, so callers must handle them. It can improve explicitness, but adds ceremony and friction with libraries built on exceptions (React Query, `await`). Pick one convention per layer and stay consistent.

```ts
function assertDefined<T>(value: T | null | undefined, message: string): T {
  if (value == null) throw new Error(`Invariant violated: ${message}`)
  return value
}
```

## Testing the error paths

Error handling that's never tested rarely works:

- **Unit-test `toAppError` and `messageFor`** with a table of inputs.
- **Integration-test failure flows** with MSW: 500 on load shows the error state and retry works; 409 shows the conflict UI; 401 triggers refresh ([MSW overrides](../18-testing-and-debugging/04-mocking-and-msw.md#per-test-overrides-errors-empty-states-slow-responses), [integration tests](../18-testing-and-debugging/05-integration-testing.md#testing-the-unhappy-paths)).
- **Test boundaries**: a throwing child shows the fallback and siblings survive.
- **In staging, deliberately break things** (kill the API, throttle, return 500s) and watch what users would see.

## Common mistakes

- **Every component inventing its own handling**, with different messages, toasts, and states.
- **Raw status-code checks scattered around** instead of one normalized error type.
- **Swallowing errors** (`catch {}`), or turning them into a generic message that hides the cause.
- **Reporting everything** (or nothing) to monitoring, instead of unexpected errors once.
- **Double handling**: a component toast *and* a global toast for the same failure.
- **Toasts for errors that need inline UI** (validation) or that are critical.
- **One root boundary only**, so any failure blanks the app.
- **No recovery path**: error states with no retry, back, or support option.
- **Showing raw server messages or stack traces** to users.
- **Clearing user input on failure.**
- **Treating cancellations as errors.**
- **Retrying non-retryable or non-idempotent failures.**
- **Untested failure paths** that only get exercised in production.
- **No correlation ID**, so reports can't be matched to server logs.

## Quick summary

- Design **one strategy**: classify → normalize → handle at the right level → contain → report.
- Separate **expected** errors (shown helpfully, not reported) from **unexpected** ones (generic fallback, reported once).
- Normalize everything to a single **`AppError`** with a `kind`, `code`, optional fields and `requestId`, and branch on those, never on message text.
- **Client** normalizes and handles auth refresh; **query layer** applies retry policy and global handling; **components** handle expected, screen-specific cases; **boundaries** (widget → route → root) contain the rest; **monitoring** captures the unexpected.
- Centralize **user-facing messages** and use a small set of **shared error components** for consistency.
- Include **correlation IDs**, preserve user input, degrade gracefully, and **test failure paths**.

## Next

[04 — Scaling large applications](./04-scaling-large-applications.md)
