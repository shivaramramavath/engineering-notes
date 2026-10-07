# Client Fetching

Most data should be fetched on the server. But some data genuinely belongs in the browser: results that change as the user types, lists that load as they scroll, values that refresh on a timer, data that depends on browser-only state. This note covers when to fetch on the client and how to do it without the usual `useEffect` pitfalls.

## When client fetching is the right choice

| Situation | Why the server cannot do it alone |
|---|---|
| Search-as-you-type, autocomplete | Query changes with every keystroke |
| Infinite scroll, "load more" | Triggered by scroll position |
| Polling / live-ish data | Needs periodic refresh in the browser |
| Data tied to client-only state (selected tab, local filters) | State lives in the browser |
| After-load, user-specific widgets on an otherwise static page | Keeps the page static |
| Mutations with instant UI feedback | Needs client cache updates |

If the data is needed to render the initial page, fetch it on the server instead. See [Server Fetching](./00-server-fetching.md).

## The naive way: `useEffect`

```tsx
"use client";

import { useEffect, useState } from "react";

export function Results({ query }: { query: string }) {
  const [data, setData] = useState<string[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetch(`/api/search?q=${encodeURIComponent(query)}`)
      .then((r) => r.json())
      .then(setData)
      .catch((e) => setError(String(e)));
  }, [query]);

  if (error) return <p>{error}</p>;
  if (!data) return <p>Loading…</p>;
  return <ul>{data.map((d) => <li key={d}>{d}</li>)}</ul>;
}
```

It works for a demo, but has real problems:

- **Race conditions:** if `query` changes quickly, an older, slower response can arrive last and overwrite the newer one.
- **No caching:** revisiting the component refetches everything.
- **No deduplication:** two components fetching the same URL send two requests.
- **Boilerplate:** loading and error state every time.
- **Waterfalls:** the fetch starts only after JS loads and the component mounts.

If you must use it, at least cancel stale requests:

```tsx
useEffect(() => {
  const controller = new AbortController();
  fetch(`/api/search?q=${encodeURIComponent(query)}`, { signal: controller.signal })
    .then((r) => r.json())
    .then(setData)
    .catch((e) => {
      if (e.name !== "AbortError") setError(String(e));
    });
  return () => controller.abort();
}, [query]);
```

## Use a data-fetching library

Libraries handle caching, deduplication, revalidation, retries and race conditions.

### SWR

```bash
npm install swr
```

```tsx
"use client";

import useSWR from "swr";

const fetcher = (url: string) => fetch(url).then((r) => {
  if (!r.ok) throw new Error("Request failed");
  return r.json();
});

export function Results({ query }: { query: string }) {
  const { data, error, isLoading } = useSWR(
    query ? `/api/search?q=${encodeURIComponent(query)}` : null, // null = do not fetch
    fetcher,
  );

  if (error) return <p>Something went wrong</p>;
  if (isLoading) return <p>Loading…</p>;
  return <ul>{data.map((d: string) => <li key={d}>{d}</li>)}</ul>;
}
```

SWR shows cached data immediately, then revalidates in the background (on focus, reconnect, or interval).

### TanStack Query

```tsx
"use client";

import { useQuery } from "@tanstack/react-query";

export function Results({ query }: { query: string }) {
  const { data, error, isPending } = useQuery({
    queryKey: ["search", query],
    queryFn: async () => {
      const r = await fetch(`/api/search?q=${encodeURIComponent(query)}`);
      if (!r.ok) throw new Error("Request failed");
      return r.json() as Promise<string[]>;
    },
    enabled: query.length > 0,
  });

  if (error) return <p>Something went wrong</p>;
  if (isPending) return <p>Loading…</p>;
  return <ul>{data.map((d) => <li key={d}>{d}</li>)}</ul>;
}
```

TanStack Query needs a `QueryClientProvider` in a Client Component near the root of the tree where it is used. It is stronger for mutations, pagination, infinite queries and cache control. Full coverage: [TanStack Query](../10-state-management/03-tanstack-query.md).

Pick one library for the project; SWR is smaller and simpler, TanStack Query is more featureful.

## Debounce typed input

```tsx
"use client";

import { useEffect, useState } from "react";

function useDebounce<T>(value: T, ms = 300) {
  const [debounced, setDebounced] = useState(value);
  useEffect(() => {
    const id = setTimeout(() => setDebounced(value), ms);
    return () => clearTimeout(id);
  }, [value, ms]);
  return debounced;
}
```

Use `useDebounce(input)` as the key or query so a request is not sent on every keystroke.

## Combine: server for the first render, client for updates

You get fast first content and live updates by fetching initial data on the server and handing it to the client library:

```tsx
// app/items/page.tsx  (Server)
import { ItemList } from "./item-list";

export default async function Page() {
  const items = await getItems();
  return <ItemList initialItems={items} />;
}
```

```tsx
// app/items/item-list.tsx
"use client";

import useSWR from "swr";

export function ItemList({ initialItems }: { initialItems: Item[] }) {
  const { data } = useSWR("/api/items", fetcher, { fallbackData: initialItems });
  return <ul>{data.map((i) => <li key={i.id}>{i.name}</li>)}</ul>;
}
```

No loading spinner on first paint, and the list keeps itself fresh afterwards.

You can also pass a Promise from the server and read it with `use()`; see [Composition Patterns](../03-components/03-composition-patterns.md).

## What client fetching needs on the server

Client-side requests go to an HTTP endpoint, so you need one:

- **Route Handler** (`app/api/.../route.ts`): the usual choice. See [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md).
- An external API directly, subject to CORS and the exposure of any keys. Never put secret keys in client code; proxy through a Route Handler.

Same-origin requests include your cookies automatically, so session-based auth works without extra headers.

## Server Actions are not for reading data

Server Actions are designed for **mutations**. They are queued one at a time and are not a caching or query mechanism. For reads in the browser, use a Route Handler or data library. For mutations, see [Server Actions](../07-server-actions/00-server-actions.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `useEffect` fetching without cancellation | Wrong results appear after typing fast | Abort stale requests or use SWR / TanStack Query |
| Client-fetching data the server already knows | Spinner on first load, extra request | Fetch on the server, pass `initialData` / `fallbackData` |
| Fetching on every keystroke | Rate limits, flicker | Debounce, and use `enabled`/`null` keys |
| Secret API key in a client fetch | Key exposed in the browser | Proxy through a Route Handler |
| Not handling `!res.ok` | Errors rendered as data | Check status and throw |
| Using Server Actions to read lists | Serialized, slow, uncached | Route Handler + library |
| Missing provider (TanStack Query) | "No QueryClient set" error | Wrap with `QueryClientProvider` |
| Chained client fetches | Visible waterfall | Move one to the server or fetch in parallel |

## Quick Summary

- Fetch on the client only when data depends on browser state or user interaction after load.
- Raw `useEffect` fetching has race conditions and no caching; prefer SWR or TanStack Query.
- Combine server initial data with client revalidation for the best first load.
- Client fetches need an HTTP endpoint; use Route Handlers and keep secrets server-side.
- Server Actions are for mutations, not reads.

## Next

- [Parallel and Sequential](./02-parallel-and-sequential.md)
- [TanStack Query](../10-state-management/03-tanstack-query.md)
- [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md)
