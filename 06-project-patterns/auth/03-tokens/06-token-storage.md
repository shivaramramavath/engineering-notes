# Token Storage

Where the browser keeps credentials decides which attacks are possible. There is no perfect option; the architecture picks the combination with the best trade-off: **access token in JS memory, refresh token in an HttpOnly cookie.**

**Prerequisites:** [02-threat-model.md](../01-fundamentals/02-threat-model.md), [02-jwt-access-tokens.md](./02-jwt-access-tokens.md)

---

## The options compared

| Storage | Readable by JS (XSS)? | Auto-sent by browser (CSRF)? | Survives reload? | Survives tab close? |
|---|---|---|---|---|
| JS memory (variable/state) | Only by script running in that page | No | No | No |
| `localStorage` | **Yes**, trivially | No | Yes | Yes |
| `sessionStorage` | **Yes** | No | Yes (same tab) | No |
| IndexedDB | **Yes** | No | Yes | Yes |
| Cookie, not HttpOnly | **Yes** | **Yes** | Configurable | Configurable |
| Cookie, `HttpOnly` | **No** | **Yes** | Configurable | Configurable |

Two separate threats are in play:

- **XSS** wants to *read* the token. HttpOnly blocks reading; memory limits the exposure window.
- **CSRF** wants the browser to *send* the token for it. Only cookies are auto-sent, so they need `SameSite`/`Path`/CSRF defenses; header-based tokens aren't affected.

---

## The chosen design

```text
Access token  → JS memory           → sent as Authorization header (no CSRF)
Refresh token → HttpOnly cookie     → sent only to /api/auth/refresh (Path), SameSite=Strict
```

Why each choice:

- **Access in memory:** nothing persisted for an attacker to harvest later; the XSS payload would need to run while the page is open. Reload loses it, which is fine because the refresh cookie restores it.
- **Refresh in HttpOnly cookie:** the long-lived, high-value secret is invisible to JS. Scoped by `Path` so it isn't attached to normal API calls, and `SameSite=Strict` blocks cross-site sending.

Why not put the refresh token in `localStorage`: a single XSS bug would let an attacker exfiltrate a 30-day credential and use it from their own machine.

**Honest limit:** if an attacker has XSS on your page, they can still make authenticated requests *from within the page* while the user is there, or call `/auth/refresh` (the browser attaches the cookie). HttpOnly prevents theft of the token, not abuse in place. Fix XSS at the source (output encoding, CSP), keep dependencies clean, and treat this storage as damage reduction.

---

## Cookie flags

```js
res.cookie("refreshToken", refreshToken, {
  httpOnly: true,                                   // no document.cookie access
  secure: process.env.NODE_ENV === "production",    // HTTPS only
  sameSite: "strict",                               // not sent on cross-site requests
  path: "/api/auth/refresh",                        // only attached to this endpoint
  maxAge: 30 * 24 * 60 * 60 * 1000,                 // ms
});
```

Gotchas:

- To **clear** a cookie, `clearCookie` must use the same `path` (and `domain`) as when set, or the browser keeps it.
- If the frontend and API are on different **sites** (not just origins), `SameSite=Strict` cookies won't be sent. You'd need `SameSite=None; Secure` plus CSRF protection ([csrf](../04-sessions/05-csrf.md)). Putting the SPA and API on the same site (e.g. `app.example.com` and `api.example.com`) keeps `Strict` workable.
- `Secure` cookies aren't set over plain HTTP in most browsers, though `localhost` is typically treated as secure for development.

---

## Frontend: the access token holder

```js
// auth/tokenStore.js: module-level, never persisted
let accessToken = null;
export const getAccessToken = () => accessToken;
export const setAccessToken = (t) => { accessToken = t; };
```

### Restoring the session on page load

```js
// AuthProvider.jsx
useEffect(() => {
  refreshOnce()                         // POST /api/auth/refresh with credentials
    .then((t) => setAccessToken(t))
    .catch(() => setAccessToken(null))  // not logged in
    .finally(() => setReady(true));
}, []);
```

Render protected routes only after `ready`, or users flash to the login page on reload.

### Automatic refresh on 401 (single-flight)

```js
import axios from "axios";

const api = axios.create({ baseURL: "/api", withCredentials: true });

api.interceptors.request.use((cfg) => {
  const t = getAccessToken();
  if (t) cfg.headers.Authorization = `Bearer ${t}`;
  return cfg;
});

let refreshing = null;
const refreshOnce = () =>
  (refreshing ??= axios
    .post("/api/auth/refresh", null, { withCredentials: true })
    .then((r) => { setAccessToken(r.data.accessToken); return r.data.accessToken; })
    .finally(() => { refreshing = null; }));

api.interceptors.response.use(undefined, async (error) => {
  const original = error.config;
  if (error.response?.status === 401 && !original._retried && !original.url.includes("/auth/refresh")) {
    original._retried = true;
    try {
      await refreshOnce();
      return api(original);
    } catch {
      setAccessToken(null);   // refresh failed: log out in the UI
    }
  }
  return Promise.reject(error);
});
```

Three details that prevent real bugs: the `_retried` flag stops infinite loops, the `/auth/refresh` exclusion stops refresh-calls-refresh, and the shared `refreshing` promise ensures N parallel 401s trigger **one** refresh (important with rotation; see [05-atomic-rotation-lua.md](./05-atomic-rotation-lua.md)). Only retry on `401`, not `403`.

### Multiple tabs

Memory isn't shared across tabs, so each tab refreshes on its own and tabs can race each other's rotation. Options: accept occasional re-login, or coordinate with `BroadcastChannel` / the Web Locks API so one tab refreshes and shares the result. Pair with a server-side grace window if races are frequent.

---

## Alternatives worth knowing

- **BFF (backend-for-frontend):** a small server holds the tokens and gives the browser only a session cookie. The browser never sees any token. Strongest against token theft; more infrastructure.
- **Pure cookie sessions** (no JWT): simplest model for same-site apps; see [06-jwt-vs-sessions.md](../04-sessions/06-jwt-vs-sessions.md).
- **Mobile/native apps:** use the OS secure store (Keychain/Keystore) for refresh tokens, not cookies.

---

## Common mistakes

- Persisting the access token in `localStorage` "so reload works".
- Forgetting `withCredentials` / `credentials: "include"`, so cookies never travel.
- Multiple simultaneous refreshes (no single-flight).
- Retrying forever on 401.
- Clearing the cookie with a different `path` than it was set with.
- Using `SameSite=None` without `Secure` and CSRF defenses.
- Believing HttpOnly "makes XSS harmless".

## Debugging

| Symptom | Check |
|---|---|
| Cookie missing in DevTools | Response `Set-Cookie` blocked: CORS `credentials`, `Secure` on HTTP, `SameSite` mismatch (Application → Cookies, Network → response headers show the reason) |
| Cookie present but not sent to `/refresh` | Request `Path` differs from cookie `Path`; missing `credentials` |
| Logout "works" but session returns | `clearCookie` path/domain mismatch |
| Flash of login screen on reload | Rendering before initial refresh completes |

---

## Quick summary

- Access token: JS memory, `Authorization` header. Refresh token: HttpOnly, `Secure`, `SameSite=Strict`, path-scoped cookie.
- Memory/HttpOnly reduce XSS damage but don't replace preventing XSS.
- Restore sessions on load via refresh; handle 401 with a single-flight interceptor.
- Match cookie options exactly when clearing.

**Next:** [07-jwt-algorithms-and-keys.md](./07-jwt-algorithms-and-keys.md)
