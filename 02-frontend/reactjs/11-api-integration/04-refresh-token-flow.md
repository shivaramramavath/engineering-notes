# Refresh Token Flow

Access tokens should be **short-lived** (minutes), so a stolen one is useful only briefly. But nobody wants to log in every 10 minutes. The refresh flow solves this: a longer-lived **refresh token** silently obtains new access tokens, and the user never notices.

This note assumes the setup from [03](./03-authentication.md): access token in memory, refresh token in an `HttpOnly` cookie, and the [API client](./02-api-client.md).

## The flow

```text
1. Request with access token ───────────────► API
2.                              ◄──── 401 Unauthorized (token expired)
3. POST /auth/refresh (refresh cookie) ─────► Auth server
4.                              ◄──── new access token (+ rotated refresh cookie)
5. Retry the original request ──────────────► API
6.                              ◄──── 200 OK
```

From the caller's perspective, step 2–5 never happened. They just see a slightly slower response.

Two strategies for *when* to refresh:

| | Reactive | Proactive |
|---|---|---|
| Trigger | A request fails with `401` | A timer fires shortly before expiry |
| Pros | Simple, no clock logic, always correct | Fewer failed requests |
| Cons | One extra round trip when it happens | Needs expiry info, clock skew, timers that sleep in background tabs |

**Do reactive.** It's simpler and robust. Proactive refresh is an optimization you can add later.

## The three hard parts

A naive "on 401, refresh and retry" breaks in three ways:

1. **Stampede**: ten requests fail at once and trigger ten refresh calls.
2. **Infinite loop**: the retry also gets a 401, or the *refresh* request itself 401s and tries to refresh.
3. **Failed refresh**: the refresh token is expired or revoked and the app is stuck in limbo.

## Single-flight refresh

Make concurrent callers **share one in-flight refresh promise**:

```ts
// lib/api/refresh.ts
import { BASE_URL } from "./config"        // shared with client.ts (avoids a circular import)
import { tokenStore } from "./token-store"

let refreshPromise: Promise<string> | null = null

export function refreshAccessToken(): Promise<string> {
  refreshPromise ??= (async () => {
    const res = await fetch(`${BASE_URL}/auth/refresh`, {
      method: "POST",
      credentials: "include",                 // sends the refresh cookie
    })
    if (!res.ok) throw new RefreshFailedError()
    const { accessToken } = (await res.json()) as { accessToken: string }
    tokenStore.set(accessToken)
    return accessToken
  })().finally(() => { refreshPromise = null })

  return refreshPromise
}

export class RefreshFailedError extends Error {}
```

The first caller starts the request; everyone who arrives while it's pending gets **the same promise**. When it settles, the variable resets, so the next expiry starts a new refresh. This is the key idea of the whole topic.

Note the refresh call uses raw `fetch`, not the API client. If it went through the client, a 401 from the refresh endpoint would trigger another refresh, which is the infinite loop.

## Plugging it into the client

In [02's `request`](./02-api-client.md#the-client), split sending from error handling and retry **once** after a refresh:

```ts
type InternalOptions = RequestOptions & { skipAuthRefresh?: boolean }

async function request<T>(method: string, path: string, options: InternalOptions = {}): Promise<T> {
  const { skipAuthRefresh, ...rest } = options

  let result = await send(method, path, rest)             // returns { res, data }

  if (result.res.status === 401 && !skipAuthRefresh) {
    try {
      await refreshAccessToken()
    } catch {
      authEvents.emit()                                     // see below
      throw toApiError(result)                            // surface the original 401
    }
    result = await send(method, path, rest)               // ONE retry with the new token
  }

  if (!result.res.ok) throw toApiError(result)
  return result.data as T
}
```

Guards against the loop:

- **Retry exactly once.** If the retried request is still `401`, it falls through to the error branch; there's no recursion.
- **`skipAuthRefresh`** on login/logout/refresh-style endpoints, where a 401 means "wrong credentials", not "expired token".
- The refresh itself bypasses the client.

`send` is the first half of the earlier `request` (build URL, headers, token, `fetch`, parse); `toApiError` is the second half (turn a non-OK response into an `ApiError`). Splitting them is a mechanical refactor of the client from 02. Because `send` reads the token from `tokenStore` each time, the retry automatically uses the new one.

A caveat for retries: **a request with a stream or one-shot body can't always be re-sent.** JSON and `FormData` bodies are safe; they're rebuilt from your options each call.

## When the refresh fails

If the refresh token is expired, revoked, or the user logged out elsewhere, the session is over. The API client can't navigate or set React state itself, so it announces the event and the auth layer reacts:

```ts
// lib/api/auth-events.ts
type Listener = () => void
const listeners = new Set<Listener>()
export const authEvents = {
  on: (l: Listener) => { listeners.add(l); return () => listeners.delete(l) },
  emit: () => listeners.forEach((l) => l()),
}
```

```tsx
// in AuthProvider
useEffect(() => authEvents.on(() => {
  tokenStore.set(null)
  queryClient.clear()
  setState({ status: "unauthenticated" })
}), [queryClient])
```

[Route guards](../10-routing/04-route-protection.md) see `unauthenticated` and redirect to login (remembering the current location). Don't `window.location.href = "/login"`; a hard reload destroys all in-memory state and unsaved form data.

## With axios

The same logic as an interceptor, using the same single-flight function:

```ts
http.interceptors.response.use(undefined, async (error) => {
  const original = error.config
  if (axios.isAxiosError(error) && error.response?.status === 401 && !original._retried && !original.skipAuthRefresh) {
    original._retried = true                           // loop guard
    try {
      const token = await refreshAccessToken()         // shared promise
      original.headers.Authorization = `Bearer ${token}`
      return http(original)                            // replay
    } catch {
      authEvents.emit()
    }
  }
  return Promise.reject(error)
})
```

`_retried` is a custom flag you add to the request config (extend the axios types to declare it in TypeScript). The refresh call should use a **separate axios instance or `fetch`** with no response interceptor.

## Refresh token rotation

Best practice on the server: every refresh returns a **new** refresh token and invalidates the old one. If an old token is ever reused, the server knows it was stolen and revokes the whole family. Frontend implications:

- You don't manage the refresh token (it's an `HttpOnly` cookie); you only call `/auth/refresh`.
- **Races now matter more.** Two refresh calls with the same cookie can make the second one look like token reuse. Single-flight within a tab is mandatory.

## Multiple tabs

Each tab has its own in-memory access token and its own `refreshPromise`, but they **share the refresh cookie**. With rotation, two tabs refreshing at the same instant can collide: one succeeds, and the other presents an already-rotated token.

Mitigations:

- **Server-side grace window**: briefly accept the previous refresh token after rotation (many auth servers support this).
- **Coordinate across tabs** with the Web Locks API so only one tab refreshes at a time:

```ts
const token = await navigator.locks.request("auth-refresh", () => refreshAccessToken())
```

  (After acquiring the lock, a robust implementation checks whether another tab already refreshed, for instance by sharing the new token through `BroadcastChannel`.)
- **Logout everywhere**: broadcast on a `BroadcastChannel("auth")` so other tabs clear their state when one logs out.

For most apps, a server grace window plus `BroadcastChannel` logout sync is enough.

## Page load: the first refresh

On a fresh page load the in-memory token is gone, so the bootstrap in [03](./03-authentication.md#auth-state-in-react) calls refresh once. While it's pending the auth status is `loading`, and guards show a spinner. If you also have loaders or queries firing at that moment, they'll hit `401`s and join the same single-flight refresh, which is exactly why the shared promise matters. A cleaner approach: don't render the authenticated app (or start its queries) until the bootstrap settles.

## Debugging

- **Refresh storm in the network tab** → no single-flight, or the refresh request goes through the interceptor.
- **Endless redirect to login** → bootstrap treated as logged out too early, or the guard ignores `loading`.
- **Works for 15 minutes, then everything 401s** → refresh isn't wired to the right status code (some APIs use `403` or a custom code for expiry), or cookies aren't sent (`credentials: "include"`, `SameSite`, CORS).
- **Cookie not stored** → `Secure` on plain HTTP, `SameSite=None` without `Secure`, or a CORS wildcard with credentials.
- **Intermittent logouts with multiple tabs** → rotation races; see above.
- **Laptop wakes from sleep and requests fail** → expected; the reactive flow recovers on the first `401`.

## Common mistakes

- **No single-flight**, so N concurrent 401s trigger N refreshes (and rotation reuse errors).
- **Refresh request through the same interceptor/client**, causing a loop.
- **Retrying more than once**, or recursing.
- **Refreshing on every non-2xx**, rather than specifically on expiry-related `401`s.
- **Hard redirects on session expiry**, wiping app state.
- **Forgetting to clear the query cache** when the refresh fails.
- **Reading the refresh token from JavaScript.** If you can, it isn't `HttpOnly`.
- **Retrying non-idempotent requests blindly** if the first attempt might have partially succeeded. (A `401` means the server rejected it before processing, so a single retry is safe.)
- **Putting the refresh token in `localStorage`** for convenience, which undoes the point of the pattern.

## Quick summary

- Short-lived access token + long-lived refresh token = security without constant logins.
- Refresh **reactively**: on `401`, refresh once, retry once.
- **Single-flight** the refresh with a shared promise; the refresh call bypasses the client's own 401 handling.
- On refresh failure, clear tokens and cache and let the auth layer redirect (no hard reload).
- Rotation and multiple tabs need a server grace window and/or cross-tab coordination.
- Gate the app on the initial session bootstrap.

## Next

[05 — API error handling](./05-api-error-handling.md)
