# API Error Handling

Requests fail constantly: the Wi-Fi drops, a token expires, validation rejects a field, a deploy returns 502s. Good error handling means three things: **classify** what went wrong, **decide** what to do (retry? log out? tell the user?), and **show** it in the right place.

## Kinds of failure

| Kind | Example | Typical response |
|---|---|---|
| **Network** | Offline, DNS, CORS block | Retry; "check your connection" |
| **Timeout / abort** | Slow server; user navigated away | Timeout: retry. Abort: **ignore** |
| **Auth (401)** | Token expired | Refresh and retry ([04](./04-refresh-token-flow.md)); else log in |
| **Forbidden (403)** | Logged in, not allowed | Show "no permission". Don't retry |
| **Not found (404)** | Record deleted | "Not found" UI. Don't retry |
| **Validation (400/422)** | Field rules failed | Show **per-field** errors. Don't retry |
| **Conflict (409)** | Edited elsewhere | Explain; offer reload/merge |
| **Rate limited (429)** | Too many requests | Retry after `Retry-After` |
| **Server (5xx)** | Bug, outage, bad gateway | Retry (idempotent); generic message; log |
| **Contract** | Response doesn't match expected shape | Log loudly; generic message |

The mistake is treating all of these as "an error occurred". The right action differs for each.

## One error type

The [API client](./02-api-client.md) throws `ApiError` with `status`, `code`, and `details`. Add small helpers so the rest of the app never inspects raw statuses ad hoc:

```ts
// lib/api/errors.ts
export const isApiError = (e: unknown): e is ApiError => e instanceof ApiError
// A cancellation is expected, not a failure. A timeout (TimeoutError) IS worth reporting.
export const isAbort = (e: unknown) => e instanceof DOMException && e.name === "AbortError"

export const isNetworkError = (e: unknown) => isApiError(e) && e.status === 0
export const isUnauthorized = (e: unknown) => isApiError(e) && e.status === 401
export const isForbidden    = (e: unknown) => isApiError(e) && e.status === 403
export const isNotFound     = (e: unknown) => isApiError(e) && e.status === 404
export const isValidation   = (e: unknown) => isApiError(e) && (e.status === 400 || e.status === 422)
export const isServerError  = (e: unknown) => isApiError(e) && e.status >= 500

export function getErrorMessage(error: unknown): string {
  if (isNetworkError(error)) return "Can't reach the server. Check your connection and try again."
  if (isForbidden(error))    return "You don't have permission to do that."
  if (isNotFound(error))     return "We couldn't find what you were looking for."
  if (isServerError(error))  return "Something went wrong on our side. Please try again."
  if (isApiError(error))     return error.message
  return "Something unexpected happened."
}
```

**Show user-facing messages, not raw server text**, for 5xx and unknown errors (stack traces and SQL fragments leak). Log the detail separately.

## Server error format

Agree on a consistent error body with your backend. A standard exists: **Problem Details for HTTP APIs (RFC 9457, which replaced RFC 7807)**, with fields like `type`, `title`, `status`, and `detail`. A common practical shape:

```json
{
  "code": "VALIDATION_FAILED",
  "message": "Some fields are invalid",
  "errors": { "email": ["Already in use"], "name": ["Too short"] }
}
```

Branch on the stable machine-readable `code`, never on the human `message` text, which can change or be translated.

## Validation errors → form fields

A `422` with field errors belongs **next to the inputs**, not in a toast:

```tsx
const { setError } = useForm<FormValues>()

const mutation = useMutation({
  mutationFn: projectsApi.create,
  onError: (error) => {
    if (isValidation(error) && isFieldErrors(error.details)) {
      for (const [field, messages] of Object.entries(error.details)) {
        setError(field as keyof FormValues, { type: "server", message: messages[0] })
      }
      return
    }
    toast.error(getErrorMessage(error))      // everything else
  },
})
```

Use `setError("root", ...)` for form-level messages like "Invalid email or password". See [form submission and errors](../06-forms/05-form-submission-and-errors.md). Don't trust field names from the server blindly; ignore unknown keys.

## Retries

Retrying fixes transient failures, and makes permanent ones slower and noisier. Rules:

- **Retry:** network errors, timeouts, `502/503/504`, `429` (honor `Retry-After`).
- **Don't retry:** `4xx` (except `408/429`), validation, auth failures. The result won't change.
- **Only retry idempotent requests** (`GET`, `PUT`, `DELETE`) automatically. Retrying a `POST` can create duplicates unless the API supports **idempotency keys**.
- Use **exponential backoff with jitter** (1s, 2s, 4s, plus randomness) so clients don't retry in lockstep and hammer a recovering server.
- Cap attempts (2–3).

TanStack Query does this for you; teach it what's retryable:

```ts
new QueryClient({
  defaultOptions: {
    queries: {
      retry: (failureCount, error) => {
        if (isApiError(error) && error.status >= 400 && error.status < 500) return false
        return failureCount < 2
      },
    },
    mutations: { retry: false },          // mutations are unsafe to retry by default
  },
})
```

TanStack Query applies exponential backoff between retries by default, and you can supply `retryDelay` to change it.

## Where to show errors

Match the surface to the error ([toasts vs alerts](../09-ui-components/08-toasts-and-notifications.md#choosing-the-right-feedback)):

| Error | Surface |
|---|---|
| Field validation | Inline, under the field |
| Form-level failure (bad credentials) | Inline alert at the top of the form |
| Failed **load** of a page/section | Inline error state **with a Retry button** in that area |
| Failed background action ("Couldn't save") | Toast (ideally with Retry) |
| Route can't render (404 record, crash) | Route [`errorElement`](../10-routing/05-route-data-loading.md#errors) |
| Unexpected render error | [Error boundary](../15-concurrent-and-modern-react/04-error-boundaries.md) |
| Session expired | Redirect to login, preserving location |

Every error state should give the user a **next step**: retry, go back, contact support. A dead-end "Error" is a bug.

An inline load error with retry:

```tsx
const { data, error, isPending, refetch } = useQuery(...)

if (isPending) return <Skeleton />
if (error) return (
  <Alert variant="destructive">
    <AlertDescription>{getErrorMessage(error)}</AlertDescription>
    <Button onClick={() => refetch()}>Try again</Button>
  </Alert>
)
```

## Global handlers

For policies that apply to *every* request, centralize rather than repeating `onError`:

```ts
const queryClient = new QueryClient({
  queryCache: new QueryCache({
    onError: (error, query) => {
      // Only toast for background refetches where stale data is already on screen;
      // initial-load errors are shown inline by the component.
      if (query.state.data !== undefined) toast.error(getErrorMessage(error))
    },
  }),
  mutationCache: new MutationCache({
    onError: (error, _vars, _ctx, mutation) => {
      if (mutation.options.onError) return             // component handled it
      if (isAbort(error) || isUnauthorized(error)) return
      toast.error(getErrorMessage(error))
    },
  }),
})
```

Typical global rules: send unexpected errors to monitoring ([error monitoring](../19-production/06-error-monitoring-and-logging.md)), toast for unhandled mutation failures, and **skip** aborts and 401s (handled by the refresh flow). Watch for the classic bug: a component toasts *and* the global handler toasts the same failure.

## Ignoring what shouldn't be errors

- **`AbortError`** from navigation or superseded requests: not a failure; never show it.
- **`401` handled by refresh**: invisible to the user when refresh succeeds.
- **Optimistic update rollbacks**: tell the user once, not once per layer.

## Logging and monitoring

Report **unexpected** errors (5xx, contract mismatches, JavaScript exceptions) with context: endpoint, method, status, a request ID if the server returns one (`X-Request-Id`), and the route. Don't report expected ones (validation, 404s, aborts), which only add noise. **Never include tokens, passwords, or full request bodies** in logs.

## Offline

`navigator.onLine` only tells you whether the device has *a network interface*, not whether the internet works, so treat `false` as reliable and `true` as unreliable. A better signal is real request failures (`status: 0`). TanStack Query also pauses queries while offline by default and resumes on reconnect (its `networkMode` option controls this).

## Common mistakes

- **One generic "Something went wrong"** for every failure.
- **Matching on error message strings** instead of status/`code`.
- **Retrying `4xx` and non-idempotent `POST`s.**
- **Showing raw server messages** for unexpected errors.
- **Toasting validation errors** instead of marking fields.
- **Double reporting** (component handler + global handler).
- **Treating aborts as errors**, flashing error toasts on navigation.
- **Empty `catch {}` blocks** that swallow failures silently.
- **Error states with no retry or next step.**
- **Logging secrets** along with error context.
- **Trusting `navigator.onLine === true`.**
- **Assuming the error shape**: a proxy's HTML 502 page isn't your JSON error.

## Quick summary

- Classify failures: network, timeout/abort, auth, validation, not found, rate limit, server, contract. Each calls for a different action.
- Use one `ApiError` plus tiny predicate helpers and a `getErrorMessage` mapping; branch on `status`/`code`, not message text.
- Field errors go to the form; load errors go inline with Retry; background failures get a toast; route-level failures use `errorElement`/boundaries.
- Retry only transient failures, only idempotent requests, with backoff and a cap.
- Centralize cross-cutting policy (logging, unhandled toasts) in the query/mutation caches without double-reporting.
- Never swallow errors silently or leak sensitive data into messages or logs.

## Next

[06 — Realtime communication](./06-realtime-communication.md)
