# Server Components and SSR

Everything in the earlier notes assumed a **client-side rendered (CSR)** SPA: the server sends an empty HTML shell plus JavaScript, and the browser builds the UI. That's simple and cheap to host, but it has costs: users see a blank page until the JavaScript loads, search engines get little content, and every byte of component code ships to the browser.

Server rendering moves some or all of that work to the server. This note covers the options, how **SSR and hydration** work, and what **React Server Components (RSC)** add.

> Server rendering needs a server or a framework. This note explains the *concepts* and *React's role*. Framework specifics (Next.js, React Router framework mode, and others) change quickly, so check their docs for current setup and RSC support rather than relying on any snapshot here.

## The rendering strategies

| Strategy | HTML produced | Where | When |
|---|---|---|---|
| **CSR** (client-side rendering) | Empty shell; UI built in the browser | Browser | At runtime |
| **SSR** (server-side rendering) | Full HTML per request | Server | At request time |
| **SSG** (static site generation) | Full HTML as files | Build | At build time |
| **Streaming SSR** | HTML in chunks as parts become ready | Server | At request time |
| **RSC** (Server Components) | A serialized component tree; part of the UI never ships JS | Server (+ client) | Request or build time |

They aren't exclusive. Modern frameworks mix them **per route or per component**.

```text
CSR:    request ─► [empty HTML + JS] ─► download JS ─► render ─► fetch data ─► content
SSR:    request ─► server renders data+HTML ─► [full HTML visible] ─► download JS ─► hydrate ─► interactive
```

## SSR and hydration

With SSR the server runs your React components to **produce HTML**, which the browser can show immediately (good for first paint, LCP, and SEO). That HTML is **inert**: buttons don't respond. Then the client JavaScript loads and **hydrates** it: React attaches event handlers and state to the existing DOM, rather than rebuilding it.

```tsx
// server (simplified; frameworks handle this for you)
const stream = renderToPipeableStream(<App />, { onShellReady() { stream.pipe(res) } })

// client
import { hydrateRoot } from "react-dom/client"
hydrateRoot(document.getElementById("root")!, <App />)
```

What SSR does and doesn't buy you:

- **Faster first content**: users see something before the JS arrives.
- **Better SEO and link previews**, since crawlers get real HTML.
- **Not faster interactivity**: the page can *look* ready but ignore clicks until hydration finishes (the "uncanny valley"). Streaming and selective hydration reduce this.
- **A server to run and pay for**, plus more complexity (two environments).
- You still ship all the JavaScript. SSR alone doesn't reduce bundle size.

### Hydration mismatches

Hydration expects the **client's first render to produce exactly the HTML the server sent**. If not, React warns (React 19 shows a diff) and may recover by re-rendering client-side, which wastes the benefit. Common causes:

```tsx
// ✗ Different on server and client
<p>{new Date().toLocaleTimeString()}</p>          // time differs
<p>{Math.random()}</p>                            // random differs
<p>{typeof window !== "undefined" ? "client" : "server"}</p>   // environment branch
<p>{localStorage.getItem("name")}</p>             // doesn't exist on the server (crashes)
```

Fixes:

- Render the same thing initially, then update after mount with an effect or state for client-only values.
- Use `useId` for stable IDs (never `Math.random()` or counters).
- For external stores, provide `getServerSnapshot` to `useSyncExternalStore` ([external stores](../16-advanced-react/02-external-stores.md)).
- Don't produce invalid HTML nesting (`<p><div>`), which browsers rewrite before hydration.
- Beware browser extensions and tools that modify the DOM. For a deliberate, small difference (like a timestamp), `suppressHydrationWarning` on that one element is the escape hatch.

## Streaming

Classic SSR waited for **all** data before sending **any** HTML. **Streaming SSR** uses [Suspense](./03-suspense.md) to send the page in pieces:

```text
1. Server sends the shell immediately (header, layout, <Suspense> fallbacks as placeholders)
2. Slow section's data resolves  ─► server streams that section's HTML + a tiny script to swap it in
3. React hydrates each boundary independently (selective hydration), prioritizing what the user interacts with
```

```tsx
<Layout>
  <Suspense fallback={<FeedSkeleton />}>
    <Feed />                  {/* slow: streams in when ready */}
  </Suspense>
  <Suspense fallback={<SidebarSkeleton />}>
    <Sidebar />               {/* independent: doesn't wait for Feed */}
  </Suspense>
</Layout>
```

One slow query no longer holds up the whole page, and users can interact with parts that are already hydrated. Placement of Suspense boundaries is now also a **server performance decision**.

## React Server Components

RSC introduce a different kind of component: one that runs **only on the server**.

```tsx
// ProjectList.tsx: a Server Component (the default in RSC-enabled frameworks)
export default async function ProjectList() {
  const projects = await db.project.findMany()      // direct data access: no API layer, no secrets exposed
  return (
    <ul>
      {projects.map((p) => <li key={p.id}>{p.name}</li>)}
    </ul>
  )
}
```

What's different:

- It can be **`async`** and read databases, files, and secrets directly.
- Its **code never ships to the browser**, with zero JavaScript from it or the heavy libraries it imports (a Markdown parser, a date library, a syntax highlighter).
- It renders to a special **serialized format** (the RSC payload) that React merges into the client tree, not to HTML-that-must-be-hydrated.
- It has **no state, effects, or event handlers**. No `useState`, `useEffect`, `onClick`, or browser APIs.

### `"use client"`: the boundary

Interactivity lives in **Client Components**, opted in with a directive at the top of the file:

```tsx
"use client"

import { useState } from "react"

export function LikeButton({ initial }: { initial: number }) {
  const [likes, setLikes] = useState(initial)
  return <button onClick={() => setLikes(likes + 1)}>♥ {likes}</button>
}
```

`"use client"` marks the **boundary**: this file and everything it imports run on the client (they're in the JS bundle). Client Components are still server-rendered for the initial HTML (SSR) and then hydrated; the difference from Server Components is that their code **also** ships to the browser.

```tsx
// Server Component composing a Client Component
export default async function Post({ id }: { id: string }) {
  const post = await getPost(id)                 // server-only work
  return (
    <article>
      <h1>{post.title}</h1>
      <Markdown source={post.body} />            {/* heavy lib stays on the server */}
      <LikeButton initial={post.likes} />        {/* interactive island */}
    </article>
  )
}
```

Rules at the boundary:

- **Props from server to client must be serializable** (strings, numbers, plain objects/arrays, dates, promises, and so on). You can't pass functions, class instances, or arbitrary objects.
- A **Client Component can't import a Server Component**. But it can *receive one as `children` or a prop*, composed by a server parent:

```tsx
// Server
<ClientSidebar>
  <ServerWidget />       {/* rendered on the server, passed through as children */}
</ClientSidebar>
```

- Put `"use client"` as **deep in the tree as possible** (on the small interactive leaf), so most of the tree stays server-only.
- Server-only modules (database clients, secrets) should never be imported into client code. Some tooling provides a `server-only` import to turn accidents into build errors.

### Server Functions (`"use server"`)

A **Server Function** is an async function that runs on the server but can be **called from client code**, the server counterpart of [Actions](./05-react-19-features.md#actions):

```tsx
// actions.ts
"use server"

export async function createProject(formData: FormData) {
  const session = await requireSession()            // ← authenticate AND authorize here
  const name = projectSchema.parse(String(formData.get("name")))   // ← validate here
  await db.project.create({ data: { name, ownerId: session.userId } })
}
```

```tsx
<form action={createProject}>…</form>
```

**Security is crucial here.** A Server Function is a **public HTTP endpoint** under the hood. Anyone can call it directly with any arguments, whether or not your UI exposes it. So every function must independently **authenticate, authorize, and validate its input**. Never assume the caller is your own form. This is the same principle as [route protection](../10-routing/04-route-protection.md): client-side checks are UX, the server is the real gate.

### What RSC give you

- **Smaller bundles**: heavy dependencies used only in Server Components never reach the client ([bundle optimization](../14-performance/05-bundle-optimization.md)).
- **Data fetching next to the component**, with no waterfalls of client-side fetch-after-render: the server is close to the data, and can fetch in parallel.
- **No client-side API layer** for server-rendered data, and secrets stay on the server.
- **Streaming and Suspense** integrated end to end.

### What they cost

- **A server runtime** and a framework/bundler that implements the RSC protocol. You can't use RSC in a plain Vite SPA without such support.
- A **new mental model**: two kinds of components with different capabilities, a serialization boundary, and caching rules that vary by framework.
- **Library compatibility**: libraries relying on context, state, or browser APIs must be Client Components (they need `"use client"`), which can make integration awkward.
- **Not a replacement for client state**: interactivity, optimistic UI, and local state still live in Client Components. Tools like [TanStack Query](../12-server-state/README.md) remain relevant for client-driven data.

## SSR vs RSC: not the same thing

| | SSR | Server Components |
|---|---|---|
| What it does | Renders components to **HTML** on the server | Runs components **only** on the server and sends their output |
| Component code in the browser bundle | **Yes**, all of it (needed to hydrate) | **No**, for Server Components |
| Hydrated | Yes | Server Components aren't; Client Components are |
| Runs on the server | At initial request | At request, and on subsequent navigations/refreshes |
| Can be combined | | **Yes**, and usually is (RSC plus SSR of the Client Components) |

SSR answers "how do I show HTML before JS loads?" RSC answers "how do I avoid shipping JS for components that don't need to be interactive?" They're complementary.

## Do you need any of this?

Many apps don't. Match the tool to the problem.

| Situation | Reasonable choice |
|---|---|
| Authenticated dashboard/admin app, no SEO needs | **CSR SPA** (Vite + React Router + TanStack Query). Simple, cheap, fast enough |
| Marketing or content site, blog, docs | **SSG or SSR**, ideally with a framework |
| E-commerce, public pages where SEO and first-load speed matter | **SSR/streaming** (and RSC if your framework supports it) |
| Large app where bundle size and data fetching close to the DB are pain points | Framework with **RSC** |
| Mixed: public marketing pages + private app | Framework with per-route strategies |

Questions to ask:

- Does **SEO or link preview** matter? (If yes, you want real HTML.)
- Is **first-load performance** (LCP) the bottleneck, and is it due to client-side fetch waterfalls or bundle size?
- Can your team operate a **server runtime**, and is the added complexity justified?
- How much of the UI is genuinely **interactive** versus mostly display?

Remember the SPA you've been building throughout this repo is a legitimate, fully supported way to build React apps. React Router also offers a framework mode with SSR on the same routing concepts ([routing](../10-routing/README.md)), which can be a gentler step than a full RSC framework.

## Common mistakes

- **Reaching for SSR/RSC by default** when a CSR SPA fits (authenticated app, no SEO needs).
- **Hydration mismatches** from dates, randomness, `window`/`localStorage` checks, or invalid HTML.
- **Adding `"use client"` at the top of large trees**, losing RSC benefits, instead of on small interactive leaves.
- **Passing non-serializable props** (functions, class instances) from server to client.
- **Importing a Server Component into a Client Component** (pass it as `children` instead).
- **Using state, effects, or browser APIs in a Server Component.**
- **Server Functions without authentication, authorization, or input validation**, treating them as private.
- **Leaking secrets** into client code by importing server-only modules.
- **Assuming SSR means "interactive immediately."** Hydration still has to finish.
- **Confusing SSR with RSC**, or expecting SSR to shrink the bundle.
- **Blocking the whole page on the slowest query** instead of using Suspense boundaries for streaming.
- **Assuming a feature or API is stable across frameworks**, when RSC support and caching semantics differ and keep evolving.

## Quick summary

- **CSR** is simplest; **SSR** sends HTML first (better first paint and SEO) but still hydrates the same JavaScript; **streaming** with Suspense sends the page in pieces.
- **Hydration** attaches interactivity to server HTML, and requires the client's first render to match, so avoid time, randomness, and environment checks in the initial render.
- **Server Components** run only on the server, ship no JavaScript, can be `async`, and access data directly; **`"use client"`** marks interactive Client Components, pushed as deep as possible.
- Server → client props must be serializable; Client Components receive Server Components via `children`.
- **Server Functions are public endpoints**: always authenticate, authorize, and validate.
- SSR and RSC solve different problems and combine. Both need a server and usually a framework.
- Choose by actual needs (SEO, first load, bundle size, team capacity), not by trend. A CSR SPA is often the right answer.

## Next

Continue to [16 — Advanced React](../16-advanced-react/README.md).