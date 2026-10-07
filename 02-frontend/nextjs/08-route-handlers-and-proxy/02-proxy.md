# Proxy

`proxy.ts` runs code **before** a request reaches a page or Route Handler. You can redirect, rewrite, set headers or cookies, or answer directly. In Next.js 16 it replaced `middleware.ts`.

> Verified against the Next.js 16.4 docs. Next.js recommends treating Proxy as a **last resort**, for cases other features cannot cover.

## Renamed from `middleware`

| | Before (≤ 15) | Now (16+) |
|---|---|---|
| File | `middleware.ts` | `proxy.ts` |
| Export | `middleware` | `proxy` (or default export) |
| Runtime | Edge by default | **Node.js** (the `runtime` option is not allowed in `proxy`; setting it throws) |

Migrate with the codemod:

```bash
npx @next/codemod@canary middleware-to-proxy .
```

It renames the file and the function. Old tutorials and some libraries still say "middleware"; the behavior is the same idea.

## The file

Place `proxy.ts` at the project root (or inside `src/`), next to `app/`. **One file per project.**

```ts
// proxy.ts
import { NextResponse, type NextRequest } from "next/server";

export function proxy(request: NextRequest) {
  if (request.nextUrl.pathname === "/old-pricing") {
    return NextResponse.redirect(new URL("/pricing", request.url));
  }
  // returning nothing continues to the route
}

export const config = {
  matcher: ["/old-pricing", "/dashboard/:path*"],
};
```

The function may be `async`. It can return:

| Return | Effect |
|---|---|
| `NextResponse.next()` or nothing | Continue to the route |
| `NextResponse.redirect(url)` | Send the browser elsewhere |
| `NextResponse.rewrite(url)` | Serve a different route, URL stays the same in the browser |
| `Response.json(...)` / `NextResponse.json(...)` | Answer directly (for example `401`) |

A second argument `event` (a `NextFetchEvent`) offers `waitUntil(promise)` to finish background work, such as logging, after the response is sent:

```ts
import type { NextFetchEvent, NextRequest } from "next/server";

export function proxy(request: NextRequest, event: NextFetchEvent) {
  event.waitUntil(logRequest(request.nextUrl.pathname));
}
```

The `NextProxy` type infers both parameter types if you prefer a typed constant.

## Matching: choose paths on purpose

**Without a `matcher`, proxy runs on every request**, including `_next/static`, `_next/image` and files in `public/`. An auth redirect there can break CSS, JavaScript and images.

```ts
export const config = {
  matcher: [
    // everything except API routes, static assets, image optimization and metadata files
    "/((?!api|_next/static|_next/image|favicon.ico|sitemap.xml|robots.txt).*)",
  ],
};
```

Path pattern rules:

- Must start with `/`.
- `:name` matches one segment; `:path*` zero or more; `:path+` one or more; `:path?` zero or one.
- Regex goes in parentheses: `/about/(.*)` equals `/about/:path*`.
- Patterns are anchored at the start: `/about` matches `/about` and `/about/team`, not `/blog/about`.
- `matcher` values must be **constants**; variables are ignored because they are analyzed at build time.

Object form, with conditions:

```ts
export const config = {
  matcher: [
    {
      source: "/dashboard/:path*",
      has: [{ type: "cookie", key: "session" }],            // only when present
      missing: [{ type: "header", key: "next-router-prefetch" }], // and not a prefetch
    },
  ],
};
```

Or branch inside the function:

```ts
export function proxy(request: NextRequest) {
  const { pathname } = request.nextUrl;
  if (pathname.startsWith("/about")) return NextResponse.rewrite(new URL("/about-2", request.url));
  if (pathname.startsWith("/dashboard")) return NextResponse.rewrite(new URL("/dashboard/user", request.url));
}
```

> Even if you exclude `_next/data` in a negative matcher, proxy still runs for those routes. This is deliberate, so protecting a page cannot leave its data route open.

## Execution order

Proxy runs early in the request lifecycle:

1. `headers` from `next.config`
2. `redirects` from `next.config`
3. **Proxy**
4. `beforeFiles` rewrites
5. Filesystem routes (`public/`, `app/`)
6. `afterFiles` rewrites
7. Dynamic routes
8. `fallback` rewrites

Prefer `redirects`, `rewrites` and `headers` in `next.config.ts` when the rule is static. Use proxy only when the decision depends on the request (cookies, headers, geography).

## Common uses

### Optimistic auth redirect

Check only that a session cookie **exists**, and send visitors to login early:

```ts
import { NextResponse, type NextRequest } from "next/server";

export function proxy(request: NextRequest) {
  const hasSession = request.cookies.has("session");

  if (!hasSession) {
    const login = new URL("/login", request.url);
    login.searchParams.set("from", request.nextUrl.pathname);
    return NextResponse.redirect(login);
  }
}

export const config = { matcher: ["/dashboard/:path*", "/settings/:path*"] };
```

This is a **UX shortcut, not security**. See the warning below.

### Passing data to the app (request headers)

```ts
export function proxy(request: NextRequest) {
  const requestHeaders = new Headers(request.headers);
  requestHeaders.set("x-request-id", crypto.randomUUID());

  return NextResponse.next({ request: { headers: requestHeaders } });
}
```

`NextResponse.next({ request: { headers } })` sets headers your **server** receives. `NextResponse.next({ headers })` sends headers to the **client**, which is rarely what you want and can break Server Actions or streaming by overriding `Content-Type`. Do not copy all incoming headers; allow-list the ones you forward, so `authorization` and cookies do not leak.

Proxy is separate from your render code and can be deployed to a CDN. **Do not rely on shared modules or globals** between proxy and the app. Pass information through headers, cookies, rewrites, redirects or the URL.

### Response headers and cookies

```ts
const response = NextResponse.next();
response.headers.set("x-frame-options", "DENY");
response.cookies.set("visited", "1", { httpOnly: true, secure: true, sameSite: "lax" });
return response;
```

### Redirect or rewrite by request data

```ts
// A/B test: serve a variant without changing the URL
const variant = request.cookies.get("variant")?.value === "b" ? "/home-b" : "/home";
return NextResponse.rewrite(new URL(variant, request.url));
```

### CORS for many API routes

```ts
const allowedOrigins = ["https://acme.com", "https://my-app.org"];
const corsOptions = {
  "Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
};

export function proxy(request: NextRequest) {
  const origin = request.headers.get("origin") ?? "";
  const isAllowed = allowedOrigins.includes(origin);

  if (request.method === "OPTIONS") {
    return NextResponse.json({}, { headers: { ...(isAllowed && { "Access-Control-Allow-Origin": origin }), ...corsOptions } });
  }

  const response = NextResponse.next();
  if (isAllowed) response.headers.set("Access-Control-Allow-Origin", origin);
  Object.entries(corsOptions).forEach(([k, v]) => response.headers.set(k, v));
  return response;
}

export const config = { matcher: "/api/:path*" };
```

### Block unauthenticated API calls

```ts
export function proxy(request: NextRequest) {
  if (!request.headers.get("authorization")) {
    return Response.json({ success: false, message: "authentication failed" }, { status: 401 });
  }
}
export const config = { matcher: "/api/:path*" };
```

## The security warning

**Proxy alone is not an auth system.** Three reasons:

1. **Server Actions are not separate routes.** They are `POST` requests to the page that uses them. If your matcher excludes that path, or a refactor moves an action to another route, proxy coverage disappears silently.
2. Matchers are easy to get wrong (a new route outside the pattern is unprotected).
3. A cookie-exists check does not prove the session is valid.

So:

- Use proxy for **fast, optimistic** checks (is there a session cookie?) and redirects.
- Verify the session and permissions **inside** every Server Action, Route Handler, and data-access function. See [Auth Architecture](../11-authentication/00-auth-architecture.md).

## Keep it fast and small

Proxy sits in front of every matched request.

- No slow database queries or long network calls.
- Narrow the `matcher` so it does not run on assets or routes that do not need it.
- Do heavy or full-session work in the route, not here.
- Use `event.waitUntil` for work that should not delay the response.

## Platform support

| Deployment | Proxy supported |
|---|---|
| Node.js server | Yes |
| Docker container | Yes |
| Static export (`output: "export"`) | **No** |
| Adapters | Depends on the platform |

## Testing (experimental)

`next/experimental/testing/server` provides helpers such as `unstable_doesProxyMatch` to assert which URLs a matcher covers. The API is experimental; check the docs for your version.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Styles or images missing | No matcher; proxy redirected asset requests | Exclude `_next/static`, `_next/image`, files |
| Redirect loop | Redirect target is also matched | Exclude `/login` or check the path first |
| Proxy never runs | Matcher does not match, file in the wrong place | File beside `app/`; test the pattern |
| `runtime` error | `export const config = { runtime: ... }` | Remove it (Node.js only) |
| Headers not visible in the app | Used `next({ headers })` | Use `next({ request: { headers } })` |
| Two proxies conflict | Only one file allowed | Combine logic in one function |
| Server Action reachable without auth | Relied on proxy | Check auth inside the action |
| Variable in `matcher` ignored | Matchers must be static | Use literals |

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating proxy as the only auth check | Verify inside actions, handlers and data functions |
| No matcher | Add one, and exclude static assets |
| Querying the database on every request | Cheap cookie check here, real check in the route |
| Using `middleware.ts` on Next.js 16 | Rename to `proxy.ts` (codemod) |
| Reaching for proxy when `next.config` redirects work | Use config for static rules |
| Sharing globals between proxy and the app | Pass data by header, cookie or URL |
| Forwarding all request headers | Allow-list safe headers |

## Quick Summary

- `proxy.ts` (formerly `middleware.ts`) runs before routes; one file at the project root; Node.js runtime.
- Return nothing to continue, or redirect, rewrite, or respond directly.
- Always set a `matcher`; static, constant patterns only.
- Use `next({ request: { headers } })` to pass data to the app; do not rely on shared globals.
- It is a convenience layer, not a security boundary; verify auth in every action and handler.
- Keep it fast; prefer `next.config` redirects, rewrites and headers when the rule is static.

## Next

- [Webhooks](./03-webhooks.md)
- [Redirects and Rewrites](../02-routing/04-redirects-and-rewrites.md)
- [Server Actions](../07-server-actions/00-server-actions.md)
