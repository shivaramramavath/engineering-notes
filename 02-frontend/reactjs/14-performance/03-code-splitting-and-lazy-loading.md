# Code Splitting and Lazy Loading

By default, a bundler combines your whole app into one JavaScript file. Users must download and parse **all of it** (every route, every modal, every chart library) before seeing anything. **Code splitting** breaks that bundle into chunks that load on demand, so the first visit only pays for what the first screen needs.

```text
Without splitting:   [ app + dashboard + admin + charts + editor ] ──► 1.4 MB before anything renders

With splitting:      [ core + landing ] ──► 180 KB, renders fast
                     dashboard.chunk   ──► loaded when /dashboard opens
                     admin.chunk       ──► loaded when /admin opens
                     charts.chunk      ──► loaded when a chart first appears
```

## The mechanism: dynamic `import()`

A normal `import` is static: it's bundled up front. A **dynamic** `import()` returns a promise and tells the bundler "make this a separate chunk":

```ts
const module = await import("./heavy-feature")   // fetches heavy-feature.[hash].js on demand
module.run()
```

Vite (Rollup) and other modern bundlers create the chunk automatically. Everything else in this note builds on that.

## `React.lazy` and `Suspense`

`lazy` turns a dynamic import into a component that loads when first rendered; `Suspense` shows a fallback while it loads:

```tsx
import { lazy, Suspense } from "react"

const ReportsPage = lazy(() => import("./pages/reports"))     // module must `export default` a component

function App() {
  return (
    <Suspense fallback={<PageSkeleton />}>
      <ReportsPage />
    </Suspense>
  )
}
```

Details:

- The component's code isn't fetched until it **first renders**. After that it's cached.
- `lazy` needs a **default export**. For a named export, adapt it:

```tsx
const Chart = lazy(() => import("./chart").then((m) => ({ default: m.Chart })))
```

- Always render lazy components inside a `<Suspense>`; without one, React errors.
- The `fallback` should match the final layout to avoid layout shift ([CLS](./00-profiling-and-measuring.md#what-to-measure-core-web-vitals)).
- Wrap in an [error boundary](../15-concurrent-and-modern-react/04-error-boundaries.md): a failed chunk download (offline, bad deploy) throws, and without a boundary the whole tree crashes. See [chunk load errors](#chunk-load-errors-and-deployments).

## What to split, in order of value

### 1. Routes (always start here)

Users only need one route at a time, and routes are natural boundaries. With [React Router's data APIs](../10-routing/05-route-data-loading.md#lazy-routes-code-splitting), `lazy` on the route object loads the route's component **and loader** together:

```tsx
const router = createBrowserRouter([
  { path: "/", element: <RootLayout />, children: [
    { index: true, element: <HomePage /> },                              // keep the landing page in the main chunk
    {
      path: "reports",
      lazy: async () => {
        const { ReportsPage, reportsLoader } = await import("./pages/reports")
        return { Component: ReportsPage, loader: reportsLoader }
      },
    },
    {
      path: "admin",
      lazy: () => import("./pages/admin"),     // module exports Component / loader / etc. in the route-module shape
    },
  ]},
])
```

Route `lazy` also avoids a common waterfall: with `React.lazy` inside a component, the chunk download starts only after the router has matched and rendered, and *then* your component's data fetch starts. Router-level `lazy` can fetch the chunk and the loader data in parallel as navigation begins.

### 2. Heavy, rarely used UI

Things most visits never touch:

- Modals/dialogs with big dependencies (rich-text editors, date pickers, file uploaders)
- Charts and data-visualization libraries
- Code editors, maps, PDF viewers, 3D
- Admin-only or settings screens

```tsx
const RichTextEditor = lazy(() => import("./rich-text-editor"))

{isEditing && (
  <Suspense fallback={<EditorSkeleton />}>
    <RichTextEditor value={text} onChange={setText} />
  </Suspense>
)}
```

### 3. Heavy libraries on demand (not components)

```ts
async function exportToExcel(rows: Row[]) {
  const XLSX = await import("xlsx")                  // 400 KB only paid by users who click "Export"
  const sheet = XLSX.utils.json_to_sheet(rows)
  // …
}
```

Often the biggest win: a library used in one event handler shouldn't be in the initial bundle.

## What *not* to split

- **Above-the-fold, critical UI** (the landing page's main content). Splitting it adds a round trip *before* the first meaningful paint.
- **Tiny components.** Each chunk costs an HTTP request and some overhead; below a few KB it isn't worth it.
- **Everything, reflexively.** Many tiny chunks create request chains and loading flashes that feel worse than one slightly larger bundle.

## Avoiding the loading-flash waterfall

Lazy loading adds a delay between "user clicks" and "UI appears". Reduce it by **loading earlier than render**:

### Preload on intent

Start the download when the user shows intent (hover/focus), so it's likely cached by the time they click:

```tsx
const loadReports = () => import("./pages/reports")
const ReportsPage = lazy(loadReports)

<Link
  to="/reports"
  onMouseEnter={loadReports}        // fires the import; later `lazy` reuses the same promise
  onFocus={loadReports}
>
  Reports
</Link>
```

The module system caches the import, so calling it again is free. Combine with data prefetching ([TanStack Query prefetch](../12-server-state/04-caching-and-synchronization.md#prefetching)).

### Preload when idle

```ts
if ("requestIdleCallback" in window) {
  requestIdleCallback(() => import("./pages/dashboard"))
}
```

After the first screen is interactive, quietly fetch likely-next chunks. (Safari lacks `requestIdleCallback` in some versions, so fall back to `setTimeout`.)

### Avoid chunk-then-data waterfalls

If the lazy component then fetches its own data after mounting, the user waits for chunk **then** data:

```text
click ─► download chunk ─► render ─► fetch data ─► show content      ✗ sequential
click ─► download chunk ┐
         fetch data ────┴─► render with both                          ✓ parallel (loaders / prefetch)
```

Use route loaders or prefetch the query at the same time as the chunk.

### Don't flash the fallback when switching

If switching tabs or filters re-suspends a section, users see the skeleton replace content they were viewing. Wrap the state change in `startTransition` so React keeps the old UI until the new one is ready ([transitions](../15-concurrent-and-modern-react/01-transitions.md), [Suspense](../15-concurrent-and-modern-react/03-suspense.md)).

## Chunk load errors and deployments

Built files have content-hashed names (`reports.a1b2c3.js`). After you deploy, the **old chunks may no longer exist**. A user with the old `index.html` open who navigates to a lazy route requests a file that now 404s:

```text
TypeError: Failed to fetch dynamically imported module: …/reports.a1b2c3.js
```

Handle it:

```tsx
function lazyWithRetry<T extends React.ComponentType<any>>(factory: () => Promise<{ default: T }>) {
  return lazy(async () => {
    try {
      return await factory()
    } catch (error) {
      // Likely a stale deployment: reload once to get the new index.html and chunk names
      if (!sessionStorage.getItem("chunk-reloaded")) {
        sessionStorage.setItem("chunk-reloaded", "1")
        window.location.reload()
      }
      throw error
    }
  })
}
```

A simple reload-once strategy plus an error boundary with a "Reload" button covers most cases. Deployment strategies that **keep old assets available for a while** (rather than deleting them immediately) reduce the problem. See [deployment](../19-production/02-deployment.md). Clear the `chunk-reloaded` flag after a successful load so later deploys can retry.

Vite also emits `modulepreload` links for the chunks an entry statically depends on, so you don't need to hand-write those for the initial load.

## How Vite chunks things

- Each `import()` target becomes its own chunk; code shared between chunks is extracted into shared chunks automatically.
- Node-modules code is bundled where it's used, so a library imported only by a lazy route ends up in that route's chunk.
- You can influence grouping (for example, a stable "vendor" chunk for better long-term caching) through Rollup's output options. The configuration surface has differed across Vite major versions, so check the Vite docs for yours. **Start with the defaults and the analyzer** ([05](./05-bundle-optimization.md)), and only hand-tune chunks if you have a measured reason.

## Verifying it worked

1. `vite build`: the output lists each chunk and its size. You should see several JS files, not one.
2. Open the Network panel on first load: lazy chunks shouldn't appear until you navigate or interact.
3. Run the [bundle analyzer](./05-bundle-optimization.md#analyze-first) and confirm heavy libraries live in lazy chunks and not the entry chunk.
4. The Coverage panel shows how much loaded JS is unused on the first screen.

## Common mistakes

- **Splitting everything**, creating many tiny chunks and loading flashes.
- **Lazy-loading critical, above-the-fold UI.**
- **No `Suspense` boundary** (error), or one fallback so far up that it blanks the whole page.
- **No error boundary** around lazy parts, so a failed chunk crashes the app.
- **Chunk-then-data waterfalls** (lazy component that fetches after mounting).
- **Creating `lazy()` inside a component**: it makes a new component type every render, resetting state and refetching. Declare `lazy` at module level.
- **Importing a heavy library statically** in a file that is otherwise lazy, then also importing it elsewhere eagerly, so it never leaves the main chunk. Check the analyzer.
- **Forgetting stale-deployment chunk errors.**
- **Fallbacks that cause layout shift.**
- **Named exports with `lazy`** without the `.then(m => ({ default: ... }))` adapter.

## Quick summary

- Code splitting turns one big bundle into on-demand chunks via **dynamic `import()`**.
- `React.lazy` + `Suspense` for components; router `lazy` for routes (which also parallelizes chunk and data loading).
- Split **routes first**, then heavy rarely-used UI, then heavy libraries loaded inside handlers. Don't split critical or tiny code.
- Remove the wait with **preload on hover/focus/idle**, and avoid chunk-then-data waterfalls.
- Declare `lazy` at module scope, wrap in `Suspense` and an error boundary, and handle stale-chunk errors after deploys.
- Verify with `vite build` output, the Network panel, and the bundle analyzer.

## Next

[04 — Virtualization](./04-virtualization.md)
