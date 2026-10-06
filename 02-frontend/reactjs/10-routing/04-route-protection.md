# Route Protection

Some pages should only be reachable by logged-in users, or by users with a specific role. **Route protection** redirects people who don't qualify, and sends them back where they were headed once they do.

> **Client-side route protection is UX, not security.** Everything in your JavaScript bundle can be read or bypassed by the user. A guard decides what to *show*; the **server must still authorize every API request**. Treat the guards below as navigation convenience, never as the access control. See [authentication](../11-api-integration/03-authentication.md) and [security](../19-production/05-security.md).

## The standard pattern: a guard layout

A pathless layout route ([01](./01-nested-routes-and-layouts.md)) wraps every route that needs protection:

```tsx
// guards/require-auth.tsx
import { Navigate, Outlet, useLocation } from "react-router"
import { useAuth } from "@/features/auth/use-auth"

export function RequireAuth() {
  const { user, isLoading } = useAuth()
  const location = useLocation()

  if (isLoading) return <FullPageSpinner />          // don't guess yet

  if (!user) {
    return <Navigate to="/login" replace state={{ from: location }} />
  }

  return <Outlet />
}
```

```tsx
const router = createBrowserRouter([
  { path: "/login", element: <LoginPage /> },
  {
    element: <RequireAuth />,           // pathless: guards all children
    children: [
      { path: "dashboard", element: <Dashboard /> },
      { path: "settings", element: <Settings /> },
    ],
  },
])
```

Three details that matter:

1. **The loading state.** On a page refresh, you may not know yet if the user is logged in (the token is being validated, or the session fetched). If you treat "unknown" as "logged out", every refresh bounces users to `/login`. Render a spinner until auth resolves.
2. **`replace`.** Otherwise Back returns to the protected URL, which redirects again, trapping the user in a loop.
3. **`state={{ from: location }}`.** Remembers where they were going.

## Redirect back after login

```tsx
function LoginPage() {
  const navigate = useNavigate()
  const location = useLocation()
  const { login } = useAuth()
  const from = (location.state as { from?: Location })?.from?.pathname ?? "/dashboard"

  async function onSubmit(values: Credentials) {
    await login(values)
    navigate(from, { replace: true })
  }
  // …
}
```

Only redirect to **internal** paths. If you accept a redirect target from a query string (`/login?next=...`), validate that it begins with a single `/` and not `//` or a full URL, or you've created an open-redirect vulnerability.

## Guest-only routes

The inverse guard keeps signed-in users away from login/signup:

```tsx
export function GuestOnly() {
  const { user, isLoading } = useAuth()
  if (isLoading) return <FullPageSpinner />
  return user ? <Navigate to="/dashboard" replace /> : <Outlet />
}
```

## Role-based guards

Parameterize the guard:

```tsx
export function RequireRole({ allow }: { allow: Role[] }) {
  const { user } = useAuth()
  if (!user) return <Navigate to="/login" replace />
  if (!allow.includes(user.role)) return <ForbiddenPage />   // 403, not a redirect
  return <Outlet />
}

{
  element: <RequireAuth />,
  children: [
    { path: "dashboard", element: <Dashboard /> },
    {
      element: <RequireRole allow={["admin"]} />,
      children: [{ path: "admin", element: <AdminPanel /> }],
    },
  ],
}
```

Show a **403 page** for "logged in but not allowed". Redirecting to `/login` would confuse someone who is already signed in. Hide navigation links the user can't use too, but remember that hiding a link isn't protection.

Prefer checking **permissions** (`can("invoice:delete")`) over raw roles as the app grows; roles tend to multiply.

## Loader-based protection (no flash)

The component guard above renders first and then redirects. With a data router you can decide **before rendering anything**:

```tsx
import { redirect } from "react-router"

export async function requireAuthLoader({ request }: LoaderFunctionArgs) {
  const user = await getCurrentUser()           // must work outside React
  if (!user) {
    const url = new URL(request.url)
    throw redirect(`/login?next=${encodeURIComponent(url.pathname + url.search)}`)
  }
  return user
}

{ path: "dashboard", loader: requireAuthLoader, element: <Dashboard /> }
```

Loaders run outside React, so they can't call hooks like `useAuth()`. They need auth state from somewhere non-React: a token in memory or storage, a module-level store, or the `queryClient` (`queryClient.ensureQueryData(currentUserQuery)`). That's the real cost of loader-based guards.

Nested loaders run in **parallel**, so a child's loader may start before the parent's guard finishes. Never let a child loader fetch protected data on the assumption that the parent guard blocked it. The API must reject unauthenticated requests anyway.

| | Component guard (`<RequireAuth>`) | Loader guard |
|---|---|---|
| Works with | Any router; hooks/context | Data routers only |
| Auth source | React context/hooks | Non-React store, token, or query cache |
| Flash of content | Possible (guard until auth resolves) | None; redirect happens pre-render |
| Simplicity | Simple | More wiring |

A reasonable default: component guard backed by an auth context for most SPAs; loader guards when you already use loaders/query cache for auth state and want no flash.

## Handling expired sessions

A user can be "logged in" when the page loads and expired five minutes later. Don't rely on route guards to notice. Handle `401` responses centrally in the API layer (refresh the token or clear auth state), and let the guard react to the cleared state by redirecting. See [refresh token flow](../11-api-integration/04-refresh-token-flow.md).

## Where to keep the guard in your structure

```text
src/
├── routes/
│   ├── router.tsx          # route tree, guards applied here
│   └── guards/
│       ├── require-auth.tsx
│       ├── require-role.tsx
│       └── guest-only.tsx
└── features/auth/          # auth state, useAuth, login/logout
```

Compose guards at the **route tree level** (visible in one file) rather than sprinkling auth checks inside individual pages.

## Common mistakes

- **Treating the guard as security.** The API must enforce access.
- **No loading state**, redirecting everyone to login on refresh.
- **Missing `replace`**, creating a Back-button redirect loop.
- **Open redirect** via an unvalidated `next` parameter.
- **Redirecting unauthorized (but logged-in) users to login** instead of showing 403.
- **Calling hooks in loaders** (`useAuth` inside a loader isn't possible).
- **Assuming a parent loader blocks child loaders.** They run in parallel.
- **Storing the "is admin" flag in client state only** and trusting it for authorization.
- **Guard checks inside every page** instead of one layout route.

## Quick summary

- Wrap protected routes in a pathless guard layout that renders `<Outlet />` or redirects.
- Handle the unknown/loading auth state; use `replace`; pass `from` to return after login.
- Role guards show a 403, not a login redirect; prefer permission checks.
- Loader guards redirect before render but need auth state outside React.
- All of this is UX. The server is the source of truth for authorization.

## Next

[05 — Route data loading](./05-route-data-loading.md)
