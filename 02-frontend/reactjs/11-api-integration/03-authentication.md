# Authentication

Authentication answers **"who is this user?"** (*authorization* answers "what may they do?"). On the frontend, your job is narrower than people expect: collect credentials, hold onto proof of identity safely, attach it to requests, and keep UI state in sync. **The server does the real work** (verifying passwords, issuing tokens, enforcing access on every request).

> Everything in the browser can be inspected and modified by the user. Frontend auth state decides what to *display*. It is never what protects data.

## The two models

### 1. Cookie sessions

The server sets a cookie after login; the browser sends it automatically on every request.

```text
POST /auth/login ──────────────►  server verifies credentials
◄────── Set-Cookie: sid=…; HttpOnly; Secure; SameSite=Lax
GET /api/me  (cookie sent automatically) ──►
```

The cookie is `HttpOnly`, so JavaScript can't read it (good against token theft via XSS). The frontend never touches a token; it just calls `fetch` with `credentials: "include"` when cross-origin.

### 2. Bearer tokens (typically JWTs)

The server returns a token in the response body; the client stores it and sends `Authorization: Bearer <token>`.

Works across domains and non-browser clients, but **you** now decide where to store the token, and that's the hard part.

Most real systems blend them: a short-lived **access token** (bearer, used for API calls) plus a longer-lived **refresh token** (in an `HttpOnly` cookie). That's the pattern in [04](./04-refresh-token-flow.md).

## Where to store a token

| Storage | XSS can steal it? | CSRF risk? | Survives refresh? |
|---|---|---|---|
| `localStorage` / `sessionStorage` | **Yes** | No | Yes (`session`: per tab) |
| JS memory (variable/store) | Only while the page runs, harder to exfiltrate persistently | No | **No**, must re-obtain |
| `HttpOnly` cookie | No (JS can't read it) | **Yes**, needs mitigations | Yes |

There's no storage that's safe against everything:

- **XSS** (an attacker running script on your page) can read `localStorage` and make authenticated requests regardless of storage. It can't read an `HttpOnly` cookie, though it can still *use* the session while the page is open.
- **CSRF** (another site triggering a request to yours) targets cookies, since browsers attach them automatically. Mitigate with `SameSite=Lax/Strict`, CSRF tokens or custom headers, and checking `Origin`.

A widely recommended compromise for SPAs:

- **Access token in memory** (short-lived, e.g. 5–15 min)
- **Refresh token in an `HttpOnly; Secure; SameSite` cookie**, used only against the refresh endpoint
- On page load, **silently refresh** to get a new access token

Avoid `localStorage` for long-lived tokens unless you accept the XSS exposure. If you do use it, a strict Content Security Policy and rigorous output escaping become mandatory. See [security](../19-production/05-security.md).

## Auth state in React

Keep two things separate:

1. **The token** → the [token store](./02-api-client.md#the-token-store) (plain module, readable by the API client).
2. **The user + status** → React state, so components re-render.

```tsx
// features/auth/auth-provider.tsx
type AuthState =
  | { status: "loading" }
  | { status: "unauthenticated" }
  | { status: "authenticated"; user: User }

type AuthContextValue = {
  state: AuthState
  login: (credentials: Credentials) => Promise<void>
  logout: () => Promise<void>
}

const AuthContext = createContext<AuthContextValue | null>(null)

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [state, setState] = useState<AuthState>({ status: "loading" })
  const queryClient = useQueryClient()

  // Bootstrap the session on page load
  useEffect(() => {
    let cancelled = false
    ;(async () => {
      try {
        const { accessToken, user } = await authApi.refresh()      // uses the refresh cookie
        tokenStore.set(accessToken)
        if (!cancelled) setState({ status: "authenticated", user })
      } catch {
        if (!cancelled) setState({ status: "unauthenticated" })
      }
    })()
    return () => { cancelled = true }
  }, [])

  const login = useCallback(async (credentials: Credentials) => {
    const { accessToken, user } = await authApi.login(credentials)
    tokenStore.set(accessToken)
    setState({ status: "authenticated", user })
  }, [])

  const logout = useCallback(async () => {
    try { await authApi.logout() } finally {
      tokenStore.set(null)
      queryClient.clear()                          // drop cached private data
      setState({ status: "unauthenticated" })
    }
  }, [queryClient])

  const value = useMemo(() => ({ state, login, logout }), [state, login, logout])
  return <AuthContext value={value}>{children}</AuthContext>
}

export function useAuth() {
  const ctx = use(AuthContext)
  if (!ctx) throw new Error("useAuth must be used inside <AuthProvider>")
  return ctx
}
```

(React 19 allows `<AuthContext value=…>` directly and reading context with `use()`. On React 18, use `<AuthContext.Provider>` and `useContext`.)

What this gets right:

- **A tri-state `status`**, not `user | null`. On refresh you don't *know* yet whether the user is logged in. Collapsing "loading" into "logged out" makes every page refresh bounce users to `/login`. [Route guards](../10-routing/04-route-protection.md) read `status`.
- **Session bootstrap** on load, since an in-memory token vanishes on refresh.
- **Logout clears the query cache.** Otherwise the next user on a shared machine briefly sees the previous user's cached data.
- **The `cancelled` flag** guards against `setState` after unmount (and StrictMode's double effect).

For session-cookie apps, bootstrap with `GET /auth/me` instead of a refresh call; the cookie is already there.

## Login

```ts
// features/auth/api.ts
export const authApi = {
  login:   (c: Credentials) => api.post<{ accessToken: string; user: User }>("/auth/login", c),
  refresh: () => api.post<{ accessToken: string; user: User }>("/auth/refresh", undefined, { credentials: "include" }),
  logout:  () => api.post("/auth/logout", undefined, { credentials: "include" }),
}
```

The login form is a regular form ([React Hook Form](../06-forms/02-react-hook-form.md)). Show server errors (wrong password) as an inline form message, not just a toast ([05](./05-api-error-handling.md)). Don't reveal *which* part was wrong ("invalid email or password").

## Cross-origin cookies checklist

If the API is on a different origin than the SPA and you use cookies:

- Client: `credentials: "include"` on every request (or axios `withCredentials: true`).
- Server: `Access-Control-Allow-Credentials: true` and a **specific** `Access-Control-Allow-Origin` (no `*`).
- Cookie attributes: `Secure`, and `SameSite=None` if the sites are truly cross-site (which also requires `Secure`).
- The easiest path: serve the SPA and API from the **same site** (or proxy `/api`), where `SameSite=Lax` is enough and CORS disappears.

## Things not to do

- **Don't decode a JWT to make security decisions.** Decoding is fine for UX (showing the user's name or scheduling a refresh), but only the server can verify the signature.
- **Don't put secrets in the frontend.** Client secrets, API keys, and signing keys in `VITE_*` variables are public the moment you deploy.
- **Don't roll your own crypto or password handling.**
- **Don't build OAuth flows by hand.** For "Sign in with Google/GitHub", use Authorization Code flow with **PKCE** through a well-maintained library or your provider's SDK; avoid the deprecated implicit flow.
- **Don't log tokens** to the console, analytics, or error reports ([error monitoring](../19-production/06-error-monitoring-and-logging.md)).

## Authorization on the frontend

Roles or permissions arrive with the user object. Use them to hide buttons and routes, as a convenience. A hidden "Delete" button isn't protection, so the API must still return `403` for unauthorized requests, and the UI should handle that response gracefully.

## Common mistakes

- **Treating "unknown" as "logged out"**, causing a redirect to login on every refresh.
- **Trusting client-side checks** as access control.
- **Long-lived tokens in `localStorage`** with no XSS defenses.
- **Forgetting `credentials: "include"`**, so the cookie is never sent or stored cross-origin.
- **`Access-Control-Allow-Origin: *` with credentials**, which the browser rejects.
- **Not clearing the query cache on logout**, leaking previous user data.
- **Storing the token only in React state**, so the API client can't see it.
- **Storing the entire user/token object in a global store that is persisted** without thinking about what ends up in `localStorage`.
- **Putting tokens in URLs** (query strings leak via history, logs, and referrers).
- **Redirecting to an unvalidated `next` URL** after login (open redirect).

## Quick summary

- The server authenticates; the frontend holds identity proof and reflects state.
- Cookie sessions are simplest and keep tokens away from JS; bearer tokens need careful storage.
- Common SPA pattern: short-lived access token in memory + refresh token in an `HttpOnly` cookie, with a silent refresh on load.
- Model auth as `loading | unauthenticated | authenticated`.
- Clear tokens **and** the query cache on logout.
- Never make security decisions from client-side state or decoded JWTs.

## Next

[04 — Refresh token flow](./04-refresh-token-flow.md)
