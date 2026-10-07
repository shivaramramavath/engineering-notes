# Client Components

A Client Component is a component that can use state, effects, event handlers and browser APIs. You mark one by putting the `"use client"` directive at the top of its file. Client Components are still pre-rendered to HTML on the server, then **hydrated** in the browser to become interactive.

## The directive

```tsx
// components/counter.tsx
"use client";

import { useState } from "react";

export function Counter() {
  const [count, setCount] = useState(0);

  return <button onClick={() => setCount(count + 1)}>Clicked {count} times</button>;
}
```

Rules for the directive:

- It must be the **first statement** in the file, above imports. A comment may precede it.
- It marks a **module boundary**: this file, plus everything it imports, joins the client bundle.
- You do not need it in files that are already imported by a Client Component, since they are client code by inheritance.
- It applies to the whole file, not to one component.

## When you need one

| You need | Example |
|---|---|
| State | `useState`, `useReducer` |
| Effects / lifecycle | `useEffect`, subscriptions, timers |
| Event handlers | `onClick`, `onChange`, `onSubmit` |
| Browser-only APIs | `window`, `localStorage`, `IntersectionObserver`, geolocation |
| Context consumers and providers | `useContext`, `createContext` |
| Custom hooks built on the above | `useDebounce`, `useMediaQuery` |
| Client-side libraries | Charts, rich-text editors, drag-and-drop, maps |

If none of these apply, keep it a Server Component.

## What "client" really means

`"use client"` does **not** mean "renders only in the browser". Lifecycle on first load:

```text
1. Server pre-renders the component to HTML (using initial state)
2. Browser shows the HTML (not yet interactive)
3. Browser downloads the component's JavaScript
4. React hydrates: attaches event handlers and state
5. After hydration, updates happen entirely in the browser
```

On later client-side navigations, new Client Components render in the browser directly.

Consequence: code that touches `window` at render time breaks step 1, because the server has no `window`.

## Browser APIs and hydration

Do browser-only work in an effect or event handler, which run only in the browser:

```tsx
"use client";

import { useEffect, useState } from "react";

export function Theme() {
  const [theme, setTheme] = useState<string | null>(null);

  useEffect(() => {
    setTheme(localStorage.getItem("theme") ?? "light");
  }, []);

  if (theme === null) return null; // or a stable placeholder
  return <p>Theme: {theme}</p>;
}
```

Reading `localStorage` directly during render yields a server error, or a **hydration mismatch** when the server HTML differs from the first client render. See [Hydration Errors](../19-debugging/01-hydration-errors.md).

## Props crossing the boundary

A Server Component can pass props into a Client Component, but they are serialized, so they must be plain data:

```tsx
// Server Component
import { Counter } from "@/components/counter";

export default async function Page() {
  const user = await getUser();
  return <Counter initialCount={user.count} />; // number: fine
}
```

Functions (except Server Actions), class instances and DOM nodes cannot be passed. Details in [Server vs Client](./02-server-vs-client.md).

## Keep the boundary small

Everything imported by a Client Component file ships to the browser. Putting `"use client"` on a whole page turns the entire page and its imports into JavaScript:

```tsx
// Heavy: whole page is client
"use client";
export default function Page() { /* header, article body, a like button */ }
```

Better: make only the interactive leaf a Client Component:

```tsx
// page.tsx (Server Component)
import { LikeButton } from "./like-button"; // "use client" lives here

export default async function Page() {
  const post = await getPost();
  return (
    <article>
      <h1>{post.title}</h1>
      <p>{post.body}</p>
      <LikeButton postId={post.id} />
    </article>
  );
}
```

Heading and body render on the server with no client JS; only `LikeButton` is hydrated. See [Composition Patterns](./03-composition-patterns.md) and [Bundle Optimization](../20-performance/02-bundle-optimization.md).

## Context providers

Context requires a Client Component. Wrap it once and render it in a layout:

```tsx
// app/providers.tsx
"use client";

import { createContext, useState } from "react";

export const ThemeContext = createContext<{ dark: boolean; toggle: () => void } | null>(null);

export function ThemeProvider({ children }: { children: React.ReactNode }) {
  const [dark, setDark] = useState(false);
  return (
    <ThemeContext.Provider value={{ dark, toggle: () => setDark((d) => !d) }}>
      {children}
    </ThemeContext.Provider>
  );
}
```

```tsx
// app/layout.tsx  (Server Component)
import { ThemeProvider } from "./providers";

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        <ThemeProvider>{children}</ThemeProvider>
      </body>
    </html>
  );
}
```

`children` passed through the provider are still Server Components. Wrapping them in a Client Component does not make them client code. More in [Context](../10-state-management/01-context.md).

## Browser-only components

Some libraries cannot render on the server at all. Load them without SSR from inside a Client Component:

```tsx
"use client";

import dynamic from "next/dynamic";

const Map = dynamic(() => import("./map"), {
  ssr: false,
  loading: () => <p>Loading map…</p>,
});
```

`ssr: false` is not allowed in a Server Component, so the `dynamic` call itself must live in a Client Component.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `"use client"` on a page or layout "just in case" | Large bundle, lost server benefits | Move the directive to the interactive leaf |
| Directive not at the top of the file | Ignored or build error | Put it first, before imports |
| `window`/`localStorage` in render | `window is not defined` or hydration mismatch | Use `useEffect` or an event handler |
| Passing a function prop from a Server Component | Serialization error | Define the handler inside the Client Component, or use a Server Action |
| Expecting `"use client"` to prevent server rendering | Component still pre-renders on the server | Use `dynamic(..., { ssr: false })` for browser-only code |
| Placing `"use client"` in many tiny files unnecessarily | Noise | Only boundary files need it; children inherit |
| Importing a server-only module (database client) | Build error or leaked code | Keep data access in Server Components or Server Actions |

## Quick Summary

- `"use client"` marks a module boundary: that file and its imports are client code.
- Client Components are pre-rendered on the server, then hydrated.
- Use them for state, effects, handlers, browser APIs, context and client libraries.
- Props crossing the boundary must be serializable.
- Keep the boundary at the leaves; wrap providers once, in the layout.
- Browser-only code goes in effects/handlers or `dynamic(..., { ssr: false })` inside a Client Component.

## Next

- [Server vs Client](./02-server-vs-client.md)
- [Composition Patterns](./03-composition-patterns.md)
- [Hydration Errors](../19-debugging/01-hydration-errors.md)
