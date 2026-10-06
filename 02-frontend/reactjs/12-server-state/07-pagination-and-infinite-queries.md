# Pagination and Infinite Queries

You rarely want to fetch ten thousand rows at once. Large lists arrive in **pages**, and the UI either lets users step through them (**pagination**) or keeps loading more as they scroll or click "Load more" (**infinite**). TanStack Query supports both, and the choice of API starts with the *server's* pagination scheme.

## Offset vs cursor pagination

| | Offset / page-based | Cursor-based |
|---|---|---|
| Request | `?page=3&pageSize=20` or `?offset=40&limit=20` | `?cursor=abc123&limit=20` |
| Response | Items + `total` | Items + `nextCursor` (and maybe `prevCursor`) |
| Jump to page N | **Yes** | No (sequential only) |
| Stable when data changes | **No**: inserts/deletes shift items, causing duplicates or gaps between pages | **Yes**: the cursor points at a position |
| Performance on huge tables | Degrades with large offsets | Consistent |
| Typical UI | Numbered pagination, tables | Feeds, infinite scroll, "Load more" |

Use **page-based** for tables where users jump around and need totals; **cursor-based** for feeds and anything that changes while being browsed. You can still do infinite scroll over offset pagination, but you'll live with drift.

## Page-based pagination

Make the page part of the **query key**: each page is its own cache entry.

```tsx
import { keepPreviousData, useQuery } from "@tanstack/react-query"

function ProjectsTable() {
  const [page, setPage] = useState(1)

  const { data, isPending, isError, isPlaceholderData } = useQuery({
    queryKey: ["projects", "list", { page }],
    queryFn: ({ signal }) => projectsApi.list({ page, pageSize: 20 }, signal),
    placeholderData: keepPreviousData,
  })

  if (isPending) return <TableSkeleton />
  if (isError) return <ErrorState />

  return (
    <>
      <div className={isPlaceholderData ? "opacity-60" : undefined}>
        <ProjectTable rows={data.items} />
      </div>

      <Pagination
        page={page}
        pageCount={Math.ceil(data.total / 20)}
        onPageChange={setPage}
        disabled={isPlaceholderData}
      />
    </>
  )
}
```

The key piece is **`placeholderData: keepPreviousData`**. Without it, changing `page` creates a new key with no data, so `isPending` flips to true and the table flashes to a skeleton on every click. With it, the previous page stays on screen (flagged `isPlaceholderData`) until the next arrives. Dim it and disable the controls meanwhile. (v4's `keepPreviousData: true` option is now this function, imported from the library.)

### Prefetch the next page

```tsx
const queryClient = useQueryClient()

useEffect(() => {
  if (!isPlaceholderData && data && page * 20 < data.total) {
    queryClient.prefetchQuery({
      queryKey: ["projects", "list", { page: page + 1 }],
      queryFn: ({ signal }) => projectsApi.list({ page: page + 1, pageSize: 20 }, signal),
    })
  }
}, [data, isPlaceholderData, page, queryClient])
```

Clicking "Next" is now instant because the cache already holds that page. Build the key with the same factory in both places so they match ([key factories](./03-tanstack-query.md#key-factories)).

### Keep the page in the URL

Page, filters, and sort are [URL state](../10-routing/06-search-filter-and-url-state.md), so refresh, sharing, and Back all work:

```tsx
const [searchParams, setSearchParams] = useSearchParams()
const page = Math.max(1, Number(searchParams.get("page")) || 1)
```

**Reset to page 1 when filters change**, or users land on "page 7 of 2". Because filters are in the key, the new filter set simply loads a fresh entry.

Also guard against out-of-range pages (a deleted last item can make the current page empty): if `data.items.length === 0 && page > 1`, step back.

### With data tables

For a [TanStack Table](../09-ui-components/07-data-tables.md#client-side-vs-server-side) driven by the server, hold `pagination` and `sorting` state, put them in the query key, set `manualPagination` and `manualSorting`, and pass `rowCount` from the response.

## Infinite queries

For feeds and "Load more", use `useInfiniteQuery`: one cache entry that holds **an array of pages**.

```tsx
import { useInfiniteQuery } from "@tanstack/react-query"

type Page = { items: Post[]; nextCursor: string | null }

function Feed() {
  const {
    data,
    fetchNextPage,
    hasNextPage,
    isFetchingNextPage,
    isPending,
    isError,
  } = useInfiniteQuery({
    queryKey: ["posts", "feed"],
    queryFn: ({ pageParam, signal }) => postsApi.feed({ cursor: pageParam }, signal),
    initialPageParam: null as string | null,
    getNextPageParam: (lastPage: Page) => lastPage.nextCursor ?? undefined,
  })

  if (isPending) return <FeedSkeleton />
  if (isError) return <ErrorState />

  const posts = data.pages.flatMap((page) => page.items)

  return (
    <>
      {posts.map((post) => <PostCard key={post.id} post={post} />)}

      <button onClick={() => fetchNextPage()} disabled={!hasNextPage || isFetchingNextPage}>
        {isFetchingNextPage ? "Loading…" : hasNextPage ? "Load more" : "You're all caught up"}
      </button>
    </>
  )
}
```

The three required pieces:

- **`initialPageParam`** is the param for the first page (required in v5). It's `pageParam` in the first `queryFn` call.
- **`getNextPageParam(lastPage, allPages, lastPageParam)`** returns the param for the next page, or **`undefined` to signal "no more"**. That's what makes `hasNextPage` false. Returning `null` does *not* stop it in v5 (return `undefined`; `null`/`undefined` handling has changed across versions, so be explicit).
- **`queryFn`** receives `pageParam` and fetches that page.

What you get: `data.pages` (array of page responses) and `data.pageParams`, `fetchNextPage`, `hasNextPage`, `isFetchingNextPage`, plus `fetchPreviousPage`/`getPreviousPageParam` for bidirectional lists (chat history).

Flatten with `data.pages.flatMap(...)`, or use `select` to do it once and keep components simple:

```ts
select: (data) => ({ ...data, posts: data.pages.flatMap((p) => p.items) })
```

### Offset-based infinite

If the server only offers pages/offsets, compute the next param yourself:

```ts
initialPageParam: 1,
getNextPageParam: (lastPage, allPages, lastPageParam) =>
  lastPage.items.length === PAGE_SIZE ? lastPageParam + 1 : undefined,
```

(Or compare against `total` if the server returns it.)

## Infinite scroll (auto-load)

Trigger `fetchNextPage` when a **sentinel element** scrolls into view, using `IntersectionObserver`, not scroll listeners:

```tsx
function useIntersection(onIntersect: () => void, enabled: boolean) {
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const node = ref.current
    if (!node || !enabled) return
    const observer = new IntersectionObserver(
      ([entry]) => { if (entry.isIntersecting) onIntersect() },
      { rootMargin: "400px" }           // start loading before the user hits the bottom
    )
    observer.observe(node)
    return () => observer.disconnect()
  }, [onIntersect, enabled])

  return ref
}

// In Feed:
const sentinel = useIntersection(fetchNextPage, hasNextPage && !isFetchingNextPage)
// …
<div ref={sentinel} aria-hidden />
```

- Gate on `hasNextPage && !isFetchingNextPage`, or the observer fires repeatedly and queues duplicate fetches.
- Keep a visible **"Load more" button fallback**. Infinite scroll has real accessibility and usability costs: keyboard users can't reach the footer, screen reader users get unannounced content changes, and Back often loses scroll position. Consider a button, or announce loaded content in a polite live region.

## How refetching works with pages

When an infinite query goes stale and refetches, it **refetches every loaded page, sequentially**, starting from the first, to keep the cursors consistent. Ten loaded pages means ten requests. Mitigations:

- Use a sensible `staleTime` so it doesn't happen on every focus/mount.
- Set **`maxPages`** to cap how many pages stay in the cache (older ones are dropped; this requires `getPreviousPageParam` so they can be re-fetched when scrolling back).
- Prefer cursor pagination so refetched pages stay consistent.

## Filters and reset

Filters and sort belong in the **key**. A new filter set is a new infinite query, starting from the first page:

```ts
queryKey: ["posts", "feed", { tag, sort }]
```

Don't try to reuse pages across filters. When filters change, scroll to the top (users will otherwise stay at the old scroll offset of a new, shorter list).

## Mutations and infinite data

Because data is nested (`pages[i].items[j]`), cache updates are more verbose:

```ts
queryClient.setQueryData<InfiniteData<Page>>(feedKey, (old) =>
  old && {
    ...old,
    pages: old.pages.map((page) => ({
      ...page,
      items: page.items.map((p) => (p.id === updated.id ? updated : p)),
    })),
  }
)
```

Where possible **invalidate instead** ([05](./05-mutations.md)), but note that invalidation refetches *all loaded pages*. For high-traffic feeds, patching the one item (or optimistic UI, [06](./06-optimistic-updates.md)) is often worth the extra code. Creating a new item usually belongs at the top of the first page.

## Duplicates and ordering

Items can appear twice across pages (offset drift, or new items inserted at the top while someone scrolls). Defend against it when rendering:

```ts
const posts = useMemo(() => {
  const seen = new Set<string>()
  return data.pages.flatMap((p) => p.items).filter((x) => !seen.has(x.id) && seen.add(x.id))
}, [data])
```

Stable sort keys on the server (for example sort by `createdAt` with an ID tiebreaker) prevent most of it.

## Long lists need virtualization

Rendering 2,000 loaded rows is slow even if each fetch is fast. When a feed grows large, [virtualize the list](../14-performance/04-virtualization.md) (render only visible rows), and trigger `fetchNextPage` from the virtualizer when the last rows come into range.

## Page-based or infinite?

| Need | Choose |
|---|---|
| Table with sorting, totals, jump to page | **Page-based** |
| Shareable position ("page 5") | **Page-based** (URL param) |
| Social feed, activity stream | **Infinite** (cursor) |
| Chat history | Infinite with `fetchPreviousPage` |
| Users must find *a specific item* by position, or reach the footer | **Page-based** or "Load more" button |
| Mobile browsing of long content | Infinite (with a button fallback) |

## Common mistakes

- **Forgetting `placeholderData: keepPreviousData`** on page-based queries, causing a skeleton flash on every page change.
- **Page missing from the query key**, so every page shares one entry.
- **Filters missing from the key**, or not resetting to page 1 when they change.
- **Returning `null`/the wrong value from `getNextPageParam`** and never stopping ("Load more" forever, or never loads).
- **Missing `initialPageParam`** (required in v5).
- **Triggering `fetchNextPage` without gating on `hasNextPage` / `isFetchingNextPage`**, causing duplicate requests.
- **Scroll listeners** instead of `IntersectionObserver`.
- **Infinite scroll with no keyboard/screen-reader fallback**, and no way to reach the footer.
- **Invalidating a long infinite query repeatedly**, refetching every page each time.
- **Offset pagination on rapidly-changing data**, leading to duplicates and gaps.
- **Rendering thousands of rows** without virtualization.
- **Mutating the nested `pages` structure in place.**
- **Not handling an empty last page** after deletions.

## Quick summary

- Choose **offset/page** pagination for jumpable, countable tables; **cursor** for feeds and changing data.
- Page-based: put `page` (and filters) in the key, use `placeholderData: keepPreviousData`, prefetch the next page, sync with the URL, and reset to page 1 on filter changes.
- Infinite: `useInfiniteQuery` with `initialPageParam`, `getNextPageParam` (return `undefined` at the end), and `data.pages.flatMap`.
- Auto-load with an `IntersectionObserver` sentinel, gated on `hasNextPage && !isFetchingNextPage`, and keep a "Load more" fallback.
- Infinite queries refetch **all** pages; control that with `staleTime` and `maxPages`.
- Update nested pages immutably (or invalidate), de-duplicate items, and virtualize big lists.

## Next

Continue to [13 — State management](../13-state-management/README.md).