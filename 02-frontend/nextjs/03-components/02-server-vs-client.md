# Server vs Client

This note is the decision guide: which kind of component to write, where the boundary between server and client sits, and what is allowed to cross it. Read it after the two previous notes and refer back to it whenever you hit a `"use client"` error.

## Decision guide

Start with a Server Component. Move to a Client Component only for a concrete reason.

| I need to... | Use |
|---|---|
| Fetch data, query a database | Server |
| Use secrets (API keys, tokens) | Server |
| Render static or mostly-static content | Server |
| Keep heavy dependencies out of the bundle | Server |
| Respond to clicks, input, form events | Client |
| Hold UI state (`useState`, `useReducer`) | Client |
| Use effects, timers, subscriptions | Client |
| Use browser APIs (`window`, `localStorage`, geolocation) | Client |
| Use React context (provider or consumer) | Client |
| Use hooks like `useRouter`, `usePathname`, `useSearchParams` | Client |
| Use a library that needs any of the above | Client (wrap it) |

```text
Does it need state, effects, event handlers, browser APIs or context?
   │
   ├── No  ──► Server Component
   │
   └── Yes ──► Can only a small part of it do that?
                 ├── Yes ──► Extract that part into a Client Component
                 └── No  ──► Client Component
```

## The boundary is about modules

The unit is the **file**, not the individual component. `"use client"` at the top of a file declares: this module and everything it imports belong to the client bundle.

```text
page.tsx (server)
 ├── header.tsx (server)
 ├── article.tsx (server)
 └── like-button.tsx  ← "use client"  ── boundary
      └── icon.tsx                       now client code (imported by a client module)
```

Things to keep straight:

- Modules imported **from** a Client Component become client code, even without their own directive.
- A Server Component **can** import and render a Client Component.
- A Client Component **cannot** import a Server Component and keep it server-side. The import makes it client code. Pass Server Components as `children` or props instead; see [Composition Patterns](./03-composition-patterns.md).

## Do not confuse `"use client"` with `"use server"`

| Directive | Meaning |
|---|---|
| `"use client"` | Marks the client boundary |
| `"use server"` | Marks **Server Functions** (Server Actions) callable from the client |

`"use server"` does **not** make a component a Server Component. Components are server by default with no directive at all. See [Server Actions](../07-server-actions/00-server-actions.md).

## What can cross the boundary

Props passed from a Server Component to a Client Component are serialized and sent to the browser, so only serializable values work.

| Allowed | Not allowed |
|---|---|
| strings, numbers, booleans, `null`, `undefined`, `bigint` | functions (except Server Functions) |
| plain objects and arrays | class instances |
| `Date`, `Map`, `Set`, typed arrays | DOM nodes |
| `FormData` | symbols (other than registered ones) |
| JSX elements (including Server Components as `children`) | objects with methods or circular, non-plain structure |
| Promises (consumable with `use()`) | |
| Server Functions (`"use server"`) | |

```tsx
// Server Component
<ClientChart
  data={rows}                     // fine: array of plain objects
  generatedAt={new Date()}        // fine: Date
  onSelect={(id) => doThing(id)}  // error: plain function cannot be serialized
/>
```

Fix the last one by defining the handler inside the Client Component, or by passing a Server Action if the work must happen on the server.

Also note that serialized props become visible to the client. Do not pass whole database rows that contain private fields; select what the UI needs.

## Where to fetch data

| Option | When |
|---|---|
| Server Component | Default. Data needed to render the page |
| Server Action | Mutations and form submissions |
| Route Handler | Endpoints for external clients or webhooks |
| Client Component with SWR / TanStack Query | Client-driven data: polling, search-as-you-type, infinite scroll |

Fetching on the server and passing props avoids an extra round trip and keeps secrets safe. See [Server Fetching](../05-data-fetching/00-server-fetching.md) and [Client Fetching](../05-data-fetching/01-client-fetching.md).

## Protecting code from the wrong side

Poisoning guards make boundary mistakes fail at build time:

```ts
// lib/db.ts
import "server-only"; // build fails if a client module imports this
```

```ts
// lib/analytics.ts
import "client-only"; // build fails if a Server Component imports this
```

Both are small npm packages (`server-only`, `client-only`). Use `server-only` for anything involving secrets or direct data access.

Environment variables follow the same split: only `NEXT_PUBLIC_*` reach the browser. See [Environment Variables](../00-setup/03-environment-variables.md).

## Trade-offs at a glance

| | Server Component | Client Component |
|---|---|---|
| JS sent to browser | None for the component | Component + dependencies |
| Data access | Direct | Via API or props |
| Interactivity | None | Full |
| Re-render in browser | No | Yes |
| State | None | Yes |
| Secrets | Safe | Never |
| Hydration cost | None | Yes |

Practical heuristic: most pages are a server-rendered shell with a handful of small client islands.

## Worked example: a product page

```text
ProductPage                (Server)  fetch product, reviews
├── ProductGallery         (Server)  images, no interaction
├── ProductInfo            (Server)  title, price, description
├── AddToCartButton        (Client)  click handler, pending state
├── ReviewsList            (Server)  rendered from data
└── ReviewForm             (Client)  controlled inputs
```

Only two small pieces ship JavaScript. Everything else, including data fetching and formatting libraries, stays on the server.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `"use client"` high in the tree | Everything below becomes client code | Move it to the interactive leaf |
| Importing a Server Component inside a Client file | It gets bundled to the client; server-only code fails | Pass as `children` / prop |
| Confusing `"use server"` with Server Components | Components marked `"use server"` fail | Use `"use server"` only for Server Functions |
| Passing a function or class instance as a prop | Serialization error | Pass plain data; define handlers in the client file |
| Passing a full DB record to a client component | Private fields leak to the browser | Select only required fields |
| Importing a DB client into a client module | Build error / security risk | Add `import "server-only"` to catch it early |
| Fetching in `useEffect` data the server already has | Waterfall, flash of loading | Fetch on the server, pass props |

## Quick Summary

- Default to Server Components; add `"use client"` only for interactivity, state, effects, browser APIs or context.
- The boundary is per module: a client file and its imports are client code.
- Server Components can render Client Components; the reverse needs `children` or props.
- `"use client"` ≠ `"use server"`: the latter marks Server Functions.
- Only serializable props cross the boundary; select just the fields you need.
- Use `server-only` to turn boundary mistakes into build errors.

## Next

- [Composition Patterns](./03-composition-patterns.md)
- [Rendering Overview](../04-rendering/00-rendering-overview.md)
- [Server Actions](../07-server-actions/00-server-actions.md)
