# Redirects and Rewrites

A **redirect** tells the browser "go to a different URL" and the address bar changes. A **rewrite** serves different content for the same URL and the address bar stays put. Next.js offers several places to do each; the skill is picking the right one.

## Redirect vs rewrite

```text
Redirect:   browser → /old ──► server says "go to /new" ──► browser → /new   (URL changes)
Rewrite:    browser → /old ──► server serves /new's content               (URL stays /old)
```

## Four ways to redirect

| Tool | Where it runs | Use it for |
|---|---|---|
| `redirect()` / `permanentRedirect()` | Server Components, Server Actions, Route Handlers | Logic-based redirects in your code (not logged in, resource moved) |
| `next.config` `redirects()` | Config, before rendering | Static, known URL changes |
| `proxy.ts` | Before the request reaches routes | Request-based redirects (auth, locale, A/B) |
| `router.push()` | Client Components, event handlers | User-triggered navigation |

### `redirect()` in code

```tsx
// app/dashboard/page.tsx
import { redirect } from "next/navigation";

export default async function Dashboard() {
  const user = await getUser();
  if (!user) redirect("/login");

  return <h1>Hello {user.name}</h1>;
}
```

- Default status is **307** (temporary). In a Server Action it responds with 303.
- `permanentRedirect(url)` uses **308** for URLs that moved for good.
- It works by throwing a special error, so **never wrap it in `try/catch`**, or catch it and rethrow. Call it after the try block or in the `finally`-free path.

```tsx
// Wrong: the catch swallows the redirect
try {
  await save();
  redirect("/done");
} catch (e) {
  /* ... */
}

// Right
try {
  await save();
} catch (e) {
  return { error: "Could not save" };
}
redirect("/done");
```

You can use `redirect` while rendering a Client Component, but in event handlers use `router.push` instead.

### Config redirects

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async redirects() {
    return [
      { source: "/blog/:slug", destination: "/posts/:slug", permanent: true },
      { source: "/docs/:path*", destination: "/guides/:path*", permanent: false },
    ];
  },
};

export default nextConfig;
```

- `permanent: true` → 308, `false` → 307.
- Supports path matching (`:slug`, `:path*`) and conditions via `has` / `missing` (headers, cookies, query).
- The list is static and read at startup. Very long lists may hit platform limits; use `proxy.ts` or a data lookup for thousands of rules.

### Redirects in `proxy.ts`

```ts
// proxy.ts
import { NextResponse, type NextRequest } from "next/server";

export function proxy(request: NextRequest) {
  const token = request.cookies.get("session")?.value;

  if (!token && request.nextUrl.pathname.startsWith("/dashboard")) {
    return NextResponse.redirect(new URL("/login", request.url));
  }
  return NextResponse.next();
}

export const config = {
  matcher: ["/dashboard/:path*"],
};
```

Good for fast, request-level decisions. `proxy.ts` is the Next.js 16 name; earlier versions use `middleware.ts` with an exported `middleware` function. See [Proxy](../08-route-handlers-and-proxy/02-proxy.md). Treat it as a first filter, not your only security check.

## Rewrites

Config rewrites:

```ts
async rewrites() {
  return [
    // /api/legacy/x is served by another origin; the browser URL does not change
    { source: "/api/legacy/:path*", destination: "https://legacy.example.com/:path*" },
    // friendly URL → internal route
    { source: "/shop/:slug", destination: "/products/:slug" },
  ];
},
```

Rewrites can also run in `proxy.ts`:

```ts
return NextResponse.rewrite(new URL("/maintenance", request.url));
```

Typical uses:

- Proxying to an external API (also avoids CORS for the browser)
- Pretty URLs that map to different internal routes
- Per-request variants (A/B tests, locale or tenant routing: see [i18n](../15-advanced-features/00-i18n.md))
- Gradual migration (route some paths to a legacy app)

Config rewrites can be returned as an object with `beforeFiles`, `afterFiles` and `fallback` arrays to control ordering relative to filesystem routes. The plain array form is equivalent to `afterFiles`.

## Choosing

| You need | Use |
|---|---|
| "Old URL → new URL" that never changes | Config `redirects()` with `permanent: true` |
| Redirect based on login state | `redirect()` in the page/action, or `proxy.ts` |
| Redirect after a form submit | `redirect()` in the Server Action |
| Redirect on a button click | `router.push()` |
| Hide an external service behind your domain | Rewrite |
| Decide per request from cookies or headers | `proxy.ts` |

## Status codes

| Code | Name | Method kept? | Cached by browsers/search engines |
|---|---|---|---|
| 307 | Temporary | Yes | No |
| 308 | Permanent | Yes | Yes, aggressively |
| 301 / 302 | Legacy permanent / temporary | May change to GET | 301 yes |

Next.js uses 307/308 so a redirected POST stays a POST. Only use permanent redirects when you are sure; browsers can cache them for a long time, which makes mistakes hard to undo.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `redirect()` inside `try/catch` | Redirect never happens | Move it outside |
| Redirect loop | Browser error "too many redirects" | Check conditions and patterns for overlap (`/a → /b → /a`) |
| Permanent redirect set by mistake | Browser keeps redirecting even after the fix | Clear browser cache; use temporary until sure |
| Expecting a rewrite to change the address bar | It does not | Use a redirect |
| Changing `next.config` redirects and nothing happens | Config is read at startup | Restart the dev server |
| Relying only on `proxy.ts` for authorization | Direct data access still exposed | Verify access where the data is read |
| Calling `redirect()` from an event handler | Unexpected behavior | Use `router.push()` |

## Quick Summary

- Redirect changes the URL; rewrite does not.
- `redirect()` for logic in server code, config for static maps, `proxy.ts` for request-based rules, `router.push` for clicks.
- `redirect()` throws; keep it out of `try/catch`.
- 307/308 preserve the HTTP method; permanent redirects are cached hard.
- Rewrites are ideal for proxying external services and pretty URLs.

## Next

- [Error and Not Found](./05-error-and-not-found.md)
- [Proxy](../08-route-handlers-and-proxy/02-proxy.md)
- [Protecting Routes](../11-authentication/04-protecting-routes.md)
