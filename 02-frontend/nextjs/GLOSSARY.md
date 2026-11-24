# Next.js Architecture Glossary

> Authoritative reference of terminology, build symbols, protocols, and directives used throughout the Next.js curriculum.

---

## 1. Directives & Markers

- **`'use client'`**: Marks a component module boundary as a Client Component. It does NOT mean "runs only in the browser"; Client Components pre-render on the server during SSR and hydrate in the browser.
- **`'use server'`**: Declares a function or file as a Server Action (callable RPC endpoint from client or server forms).
- **`"use cache"`**: File, component, or function-level directive opting into Next.js fine-grained cache component caching.
- **`import 'server-only'`**: Build-time assertion package throwing a compilation error if the marked module is ever imported by a Client Component.
- **`import 'client-only'`**: Build-time assertion package throwing a compilation error if imported by a Server Component.

---

## 2. Route Table Symbols (`next build`)

- **`○  (Static)`**: Prerendered as static content. Rendered at build time and cached as static HTML/RSC payload.
- **`ƒ  (Dynamic)`**: Rendered on demand for each incoming user request using server runtime.
- **`◐  (Partial Prerender / PPR)`**: Prerendered with static shell and dynamic streaming holes filled at runtime via Suspense.

---

## 3. Caching & Request APIs

- **`cookies()`**: Asynchronous function (`await cookies()`) returning a `ReadonlyRequestCookies` jar.
- **`headers()`**: Asynchronous function (`await headers()`) returning request HTTP headers.
- **`params`**: Promise passed to Pages, Layouts, and Route Handlers containing dynamic route parameters.
- **`searchParams`**: Promise passed to Page components containing URL query parameters.
- **`cacheLife(profile)`**: Sets the freshness lifetime (`seconds`, `minutes`, `hours`, `days`, `max`) within a `"use cache"` block.
- **`cacheTag(tag)`**: Attaches invalidation tags to cached functions.
- **`updateTag(tag)`**: Immediately expires a cache tag inside a Server Action for read-your-own-writes consistency.
- **`revalidateTag(tag, profile)`**: Two-argument function triggering background Stale-While-Revalidate re-fetching.
- **`after()`**: Schedules non-blocking background work to run after the response stream has closed.
- **`unauthorized()`**: Halts rendering and triggers `unauthorized.tsx` with a 401 status.
- **`forbidden()`**: Halts rendering and triggers `forbidden.tsx` with a 403 status.

---

## 4. Networking & Protocols

- **`proxy.ts`**: The modern Next.js 16 network interceptor (renamed from `middleware.ts`).
- **React Flight Protocol**: The internal streaming serialization format used by React Server Components to transmit UI virtual DOM representations, client component references, and props across HTTP.
- **Data Access Layer (DAL)**: Architectural pattern encapsulating database queries, user authorization checks, and DTO transformations safely behind a `server-only` boundary.
