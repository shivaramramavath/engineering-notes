# Protecting Routes

Protecting a route means making sure unauthenticated users cannot reach the page **or the data behind it**. In the App Router this takes several layers: Proxy for a fast filter, a Data Access Layer for the real check, and re-verification in every Server Action and Route Handler.

> Verified against the Next.js 16.4 Authentication, Authentication with Cache Components and Data Security guides, and the `unauthorized` / `forbidden` references (both **experimental**).

## The layers, in order

| Layer | Runs | Reads | Use for |
|---|---|---|---|
| 1. **Proxy** | Before every matched request, including prefetches | The cookie only | Redirect early; protect static pages shared across users |
| 2. **DAL** (`verifySession`, `getUser`) | In the render or action | Cookie, plus the database when needed | The actual check |
| 3. **Page / leaf component** | During render | Result of the DAL | Role-specific UI |
| 4. **Server Action** | On each POST | DAL | Re-verify before every mutation |
| 5. **Route Handler** | On each request | DAL | Return 401 / 403 |

Rule: **the check lives next to the data.** Layers 1 and 3 are convenience.

## 1. The Data Access Layer

```ts
// app/lib/dal.ts
import "server-only";
import { cache } from "react";
import { cookies } from "next/headers";
import { redirect } from "next/navigation";
import { readSession } from "@/app/lib/session";
import { db } from "@/app/lib/db";

export const verifySession = cache(async () => {
  const token = (await cookies()).get("session")?.value;
  const session = await readSession(token);

  if (!session?.userId) {
    redirect("/login");
  }
  return { isAuth: true as const, userId: String(session.userId), role: session.role as "user" | "admin" };
});

export type UserDTO = { id: string; name: string; email: string };

export const getUser = cache(async (): Promise<UserDTO | null> => {
  const session = await verifySession();
  try {
    const user = await db.users.findById(session.userId);
    if (!user) return null;
    return { id: user.id, name: user.name, email: user.email };   // DTO: no password hash
  } catch {
    console.log("Failed to fetch user");
    return null;
  }
});
```

Points:

- `import "server-only"` keeps it off the client.
- `React.cache` **deduplicates within one render pass**, so ten components calling `verifySession()` run it once. Server Actions and Route Handlers are not part of a render pass, so the wrapper does nothing there: every call runs.
- `redirect()` throws, so code after it does not run, and callers get a non-null session.
- Return **DTOs**: pick the fields, never the whole row.
- Client Components cannot import it. Call it in a Server Component and pass props or a context.

## 2. Proxy (optimistic check)

```ts
// proxy.ts  (project root, or src/)
import { NextRequest, NextResponse } from "next/server";
import { readSession } from "@/app/lib/session";

const protectedPrefixes = ["/dashboard", "/settings"];
const authPages = ["/login", "/signup"];

export default async function proxy(req: NextRequest) {
  const path = req.nextUrl.pathname;
  const isProtected = protectedPrefixes.some((p) => path === p || path.startsWith(p + "/"));
  const isAuthPage = authPages.includes(path);

  const session = await readSession(req.cookies.get("session")?.value);   // signature check, no DB

  if (isProtected && !session?.userId) {
    const url = new URL("/login", req.nextUrl);
    url.searchParams.set("next", path + req.nextUrl.search);
    return NextResponse.redirect(url);
  }
  if (isAuthPage && session?.userId) {
    return NextResponse.redirect(new URL("/dashboard", req.nextUrl));
  }
  return NextResponse.next();
}

export const config = {
  matcher: ["/((?!api|_next/static|_next/image|.*\\.png$).*)"],
};
```

Docs guidance:

- Proxy runs on **every matched route, including prefetches**, so **read the cookie only**; do not query the database.
- Proxy uses the **Node.js runtime** in v16; confirm your auth and session libraries are compatible.
- For auth it is recommended that Proxy runs on all routes, then branch inside.
- It can protect **static routes that share data between users** (a paywalled article), because a DAL check does not run for build-time data.
- The matcher above excludes `api`, so **Route Handlers under `/api` are not covered**. They must check themselves.
- It is **not your only defense**. See [Proxy](../08-route-handlers-and-proxy/02-proxy.md).

### Safe `next` redirects

```ts
// app/lib/redirect.ts
export function safeNext(value: FormDataEntryValue | string | null | undefined, fallback = "/dashboard") {
  const v = typeof value === "string" ? value : "";
  // allow only same-site relative paths: "/x" but not "//evil.com" or "/\evil.com"
  if (v.startsWith("/") && !v.startsWith("//") && !v.startsWith("/\\")) return v;
  return fallback;
}
```

Use `redirect(safeNext(formData.get("next")))` after login. An unchecked `next` is an open redirect.

## 3. Pages and leaf components

```tsx
// app/dashboard/page.tsx
import { verifySession, getUser } from "@/app/lib/dal";

export default async function DashboardPage() {
  const session = await verifySession();          // redirects if signed out
  const user = await getUser();
  const projects = await getProjectsFor(session.userId);   // scoped query, see Authorization

  return (
    <>
      <h1>Welcome, {user?.name}</h1>
      <ProjectList projects={projects} />
    </>
  );
}
```

Role-aware leaf:

```tsx
// app/ui/admin-actions.tsx
import { verifySession } from "@/app/lib/dal";

export async function AdminActions() {
  const { role } = await verifySession();
  if (role !== "admin") return null;
  return <button>Delete user</button>;      // hiding is cosmetic; the action must still check
}
```

### Layouts are not a security boundary

Because of **partial rendering**, a layout does not re-render on every navigation, so its check does not run on each route change. A layout also does not control whether child segments or parallel route slots render: they are produced by the router and can appear in the RSC payload even if the layout hides them. And `return null` in a layout (a common SPA pattern) does not stop nested segments or Server Actions from being reached.

Do this instead: a layout may **call** `getUser()` to show a name; the **check** lives in the DAL function, so wherever data is read, the check runs.

## 4. Server Actions

```ts
// app/actions/profile.ts
"use server";
import { verifySession } from "@/app/lib/dal";

export async function updateName(formData: FormData) {
  const session = await verifySession();            // every action, every time
  const name = String(formData.get("name") ?? "").trim();
  if (name.length < 2) return { message: "Name too short" };
  await db.users.update(session.userId, { name });  // update the SESSION's user, not an id from the form
}
```

Server Actions are reachable by direct POST even if no UI shows them, so treat them like public API endpoints: authenticate, authorize, validate, return only what the UI needs. A page-level redirect does not protect an action defined in that page: re-verify inside it. See [Server Actions](../07-server-actions/00-server-actions.md) and [Validation](../07-server-actions/02-validation.md).

## 5. Route Handlers

```ts
// app/api/admin/stats/route.ts
import { verifySession } from "@/app/lib/dal";

export async function GET() {
  const session = await verifySession();   // NOTE: redirects; for APIs prefer returning 401, below
  ...
}
```

`verifySession()` redirects, which suits pages. APIs should answer with status codes, so give the DAL a non-redirecting variant:

```ts
// app/lib/dal.ts
export const getSession = cache(async () => {
  const session = await readSession((await cookies()).get("session")?.value);
  return session?.userId
    ? { userId: String(session.userId), role: session.role as "user" | "admin" }
    : null;
});
```

```ts
export async function GET() {
  const session = await getSession();
  if (!session) return new Response(null, { status: 401 });          // not authenticated
  if (session.role !== "admin") return new Response(null, { status: 403 }); // authenticated, not allowed
  return Response.json(await getStats());
}
```

| Status | Meaning |
|---|---|
| **401 Unauthorized** | Not authenticated (no or invalid session) |
| **403 Forbidden** | Authenticated but not allowed |
| **404 Not Found** | Also a good answer when revealing existence is itself a leak |

## `unauthorized()` and `forbidden()` (experimental)

Experimental in 16.4, "not recommended for production". Enable:

```ts
// next.config.ts
const nextConfig = { experimental: { authInterrupts: true } };
export default nextConfig;
```

```tsx
import { unauthorized, forbidden } from "next/navigation";

const session = await getSession();
if (!session) unauthorized();               // renders unauthorized.tsx (401)
if (session.role !== "admin") forbidden();  // renders forbidden.tsx (403)
```

```tsx
// app/unauthorized.tsx
export default function Unauthorized() {
  return <main><h1>Please sign in</h1><a href="/login">Sign in</a></main>;
}
```

Facts from the docs:

- Callable in Server Components, Server Functions and Route Handlers; **not in the root layout**.
- They **throw**; no `return` needed. A `try/catch` around them swallows the interrupt (use `unstable_rethrow` to let it through), and an un-awaited promise that calls them renders nothing.
- A call made **after streaming started** (inside `<Suspense>`) keeps a `200` status; before streaming it returns `401` or `403`. Next.js adds `noindex`.

## Cache Components

With `cacheComponents` on, reading the session is a request-time read, so:

- The component that reads it must be behind `<Suspense>`; `cookies()` outside a boundary is a build error.
- Do not `await` the session at the top of a **layout**: it holds `{children}` back. Push the read into a nested component under `<Suspense>`.
- `cookies()` cannot be called inside plain `use cache` or `use cache: remote`. `use cache: private` can read it and keeps the result in the browser only.
- To cache **per-user data on the server**, resolve the user first and pass the ID into a cached function, and keep that function unexported so callers cannot pass a different ID:

```ts
// lib/data.ts
import "server-only";
import { cacheLife, cacheTag } from "next/cache";
import { getCurrentUser } from "./auth";

export async function getNotes() {
  const user = await getCurrentUser();       // reads the session at request time
  return getNotesByUserId(user.id);
}

async function getNotesByUserId(userId: string) {   // NOT exported
  "use cache";
  cacheTag(`notes:${userId}`);
  cacheLife("minutes");
  return db.notes.findMany({ userId });
}
```

- Cache **keys and tags are stored in plain text**: use IDs, never emails or tokens.
- After a mutation call `updateTag` with the same tag (`notes:<userId>`) in the Server Action, after re-reading the session there.
- Keep `cacheLife` `stale` at 30 seconds or more or the scope drops out of prefetching.
- Migrating? `export const instant = false` on a page lets it keep blocking while you adopt the pattern route by route.

```tsx
// Streaming the signed-in part
<Suspense fallback={<p>Loading your dashboard…</p>}>
  <Dashboard />        {/* reads the session */}
</Suspense>
```

See [Cache Components](../06-caching/05-cache-components.md).

## Getting the user to Client Components

Client Components cannot import the DAL. Read in a Server Component, then pass props or an un-awaited promise through a context ([Context](../10-state-management/01-context.md#streaming-data-through-context-with-use)):

```tsx
function Dashboard() {
  const userPromise = getUser();                 // do not await
  return (
    <UserProvider userPromise={userPromise}>
      <Suspense fallback={<span>Loading…</span>}>
        <UserBadge />                            {/* Client Component: use(useUser()) */}
      </Suspense>
    </UserProvider>
  );
}
```

Pass a narrow DTO; use React's `taintUniqueValue` (needs `experimental.taint: true`) as an extra guard for sensitive fields. The client's copy is for display only: it is not authority.

## Logout

```tsx
<form action={logout}><button type="submit">Log out</button></form>
```

`logout` deletes the cookie (and the database row for database sessions), then `redirect("/login")` ([Sessions and Cookies](./01-sessions-and-cookies.md)). Do not log out in a render or via GET: Next.js blocks setting cookies during render, and GET triggers on prefetch.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Redirect loop between `/login` and `/dashboard` | Proxy says "signed in", DAL says "not" (or the reverse) | Make both read the same cookie with the same `readSession` |
| Signed-out user sees protected page | Only a layout checks | Add `verifySession()` in the page or DAL |
| Signed-out user can still call an API | Matcher excludes `/api`, handler has no check | Check in the handler |
| Static page behind login is public | Prerendered at build time; DAL never runs | Protect it in Proxy, or make it dynamic |
| Build error: `cookies()` outside Suspense | Cache Components on, session read in the shell | Wrap in `<Suspense>` |
| `cookies()` inside `use cache` throws | Not allowed there | Read outside and pass the value, or `use cache: private` |
| `forbidden is not a function` / no UI | `authInterrupts` not enabled | Enable the flag |
| `unauthorized()` caught silently | Wrapped in `try/catch` | Move out, or `unstable_rethrow` |
| Session read twice per request | `cache()` missing | Wrap the DAL function in `cache` |
| Slow navigation after adding auth | Awaiting the session at the top of a layout | Push down behind Suspense |

## Common mistakes

| Mistake | Fix |
|---|---|
| Proxy only | Add DAL checks |
| Check in a layout | Check in pages and the DAL |
| Database call inside Proxy | Cookie-only there |
| Trusting `userId` from props, form or URL | Take it from the session |
| Returning whole rows | DTOs |
| Redirect to an unvalidated `next` | `safeNext` |
| Forgetting `/api` is outside the matcher | Check in each handler |
| Hiding a button as "protection" | The action checks too |
| `redirect()` inside `try/catch` | Call it outside |

## Quick Summary

- Use layers: Proxy filters (cookie only), the DAL decides, actions and handlers re-verify.
- Put `verifySession` / `getUser` in a `server-only` DAL, wrap with `React.cache`, return DTOs.
- Do not protect by layout; check next to the data.
- 401 means not signed in, 403 means not allowed; `unauthorized()` and `forbidden()` are experimental.
- Under Cache Components, read the session behind `<Suspense>` and pass IDs, not secrets, into cached functions.

## Next

- [Authorization](./05-authorization.md)
- [Proxy](../08-route-handlers-and-proxy/02-proxy.md)
- [Cache Components](../06-caching/05-cache-components.md)
