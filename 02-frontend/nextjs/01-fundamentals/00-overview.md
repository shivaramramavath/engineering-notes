# Next.js Overview

Next.js is a full-stack React framework. React gives you components; Next.js adds everything else a real application needs: routing, server rendering, data fetching, caching, bundling, image/font optimization, and a place to run server code. You still write React; Next.js decides *where* and *when* it runs.

## What problem it solves

A plain React app (Vite, Create React App style) ships an empty HTML page plus a JavaScript bundle. The browser downloads the bundle, runs it, fetches data, and only then shows content. That means slower first paint, poor default SEO, and you must assemble routing, bundling and data loading yourself.

Next.js moves work to the server and gives you conventions for the rest:

| Concern | Plain React | Next.js |
|---|---|---|
| Routing | Add a router library | Built in, based on folders |
| First HTML | Empty shell | Rendered on the server |
| Data fetching | `useEffect` + loading states | `async` components on the server |
| Backend endpoints | Separate server | Route Handlers and Server Actions in the same project |
| Bundling, code splitting | Configure it | Automatic, per route |
| Images, fonts, metadata | Manual | Optimized components and APIs |
| Caching | DIY | Framework-level, with explicit controls |

## The big ideas

**1. File-system routing.** Folders under `app/` are URL segments. A `page.tsx` inside makes the segment publicly reachable. No route table to maintain.

**2. Server Components by default.** Components run on the server unless you opt in to the browser with `"use client"`. Server code can read a database and keep secrets; it ships no JavaScript to the client.

**3. Multiple rendering strategies, per route.** Static (HTML built ahead of time), dynamic (built per request), and streamed (sent in pieces as it becomes ready). You do not pick one globally.

**4. Caching and revalidation.** Next.js can cache data and rendered output, and you control when it refreshes. This is powerful and the single most common source of confusion; it has its own chapter.

**5. Server Functions for mutations.** Server Actions let a form or event handler call server code directly, without writing an API endpoint.

## How a request flows

```text
Browser requests /blog/hello
        │
        ▼
 proxy.ts (optional)  ── redirect / rewrite / headers
        │
        ▼
 Route matched in app/ ── blog/[slug]/page.tsx
        │
        ▼
 Server renders layouts + page (Server Components)
   • fetches data
   • produces React Server Component payload
        │
        ▼
 HTML + RSC payload streamed to browser
        │
        ▼
 Browser shows HTML immediately
        │
        ▼
 Client Components hydrate (become interactive)
        │
        ▼
 Later navigations fetch only the RSC payload for changed segments
```

First load gives HTML for fast display. Subsequent link clicks do not reload the page; Next.js requests only what changed and updates in place.

## What runs where

```text
Server                              Browser
──────                              ───────
Server Components                   Client Components ("use client")
Route Handlers, Server Actions      Event handlers, state, effects
Database, secrets, file system      window, localStorage, DOM APIs
```

Client Components are also pre-rendered to HTML on the server for the first load, then hydrated in the browser. "Client Component" means "also ships JavaScript and can use interactivity", not "never runs on the server". See [Server Components](../03-components/00-server-components.md).

## Two routers, one framework

Next.js has two routing systems:

- **App Router** (`app/`): the current model, built on React Server Components. Use it for all new work.
- **Pages Router** (`pages/`): the original model, still supported. You will meet it in older code and tutorials. See [Pages Router](./04-pages-router.md).

They can coexist in one project, which allows gradual migration.

## Version milestones worth knowing

| Version | What changed |
|---|---|
| 13 | App Router introduced (stable in 13.4) |
| 15 | React 19 support; `params` / `searchParams` became Promises; fetch is no longer cached by default |
| 16 | Turbopack default; Cache Components (`use cache`), enabled by default in new projects per the 16.4 docs; `middleware.ts` replaced by `proxy.ts`; `next lint` removed |

Verify against the release notes for your installed version; behavior in tutorials depends heavily on which version they were written for.

## Common misconceptions

| Belief | Reality |
|---|---|
| "Next.js is only for SSR" | It supports static, dynamic and streamed rendering, mixed per route |
| "Everything runs in the browser, like React" | Components are server-side by default in the App Router |
| "`use client` makes a component client-only" | It still pre-renders on the server; it adds JavaScript and interactivity |
| "Next.js replaces a backend" | It covers many backend needs (endpoints, actions), but it is not a full backend framework |
| "Static means no data" | Static pages can fetch data at build time and revalidate later |

## Quick Summary

- Next.js = React + routing + server rendering + data + caching + bundling + deployment targets.
- Folders define routes; Server Components are the default; `"use client"` opts in to the browser.
- Rendering strategy is chosen per route, not per app.
- The App Router is the modern model; the Pages Router is legacy but supported.
- Many tutorials are version-specific, so check which Next.js they target.

## Next

- [App Router](./01-app-router.md)
- [Rendering Overview](../04-rendering/00-rendering-overview.md)
- [Caching Overview](../06-caching/00-caching-overview.md)