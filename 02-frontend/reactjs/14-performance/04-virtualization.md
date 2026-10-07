# Virtualization

Rendering a list of 10,000 rows creates 10,000 sets of DOM nodes. The browser has to lay out, paint, and keep them all in memory, and React has to reconcile them. Scrolling and updating get sluggish no matter how efficient each row is.

**Virtualization (windowing)** renders only the rows **currently visible** (plus a small buffer) and fakes the rest with empty space. A 10,000-row list becomes ~20 DOM rows that get recycled as you scroll.

```text
Full list (10,000 items)             What's actually in the DOM

┌───────────────┐  ← spacer           ┌───────────────┐  ← empty space (top)
│   (not        │                     ├───────────────┤
│   rendered)   │                     │ row 4,203     │ ┐
├───────────────┤                     │ row 4,204     │ │ visible + overscan
│ ▓ visible ▓   │  ← viewport         │ …             │ │ (≈ 20 rows)
├───────────────┤                     │ row 4,222     │ ┘
│   (not        │                     ├───────────────┤
│   rendered)   │                     │ empty space   │  ← (bottom)
└───────────────┘                     └───────────────┘
```

## Do you need it?

Virtualization has real costs (complexity, accessibility trade-offs, find-in-page), so use it when the data justifies it.

| Situation | Approach |
|---|---|
| Up to a few hundred simple rows | **Just render them.** Browsers handle this fine |
| Hundreds to a few thousand, light rows | Probably fine; **measure** first |
| Thousands of rows, or heavy rows (images, many cells, interactive controls) | **Virtualize** |
| Users mostly want to find a *specific* item | **Search/filter + pagination** is often a better UX |
| Table with sorting/totals/jump-to-page | [Server-side pagination](../12-server-state/07-pagination-and-infinite-queries.md) |
| A feed that grows as you scroll | Infinite query **+** virtualization once it gets long |

Before reaching for it, check the cheaper options: pagination, `content-visibility: auto` ([01](./01-rendering-performance.md#fix-5-do-less-work-per-render)), and making rows cheaper to render.

## TanStack Virtual

[`@tanstack/react-virtual`](https://tanstack.com/virtual) is a headless virtualizer: it computes *which* items are visible and *where* they sit; you render the markup. (Alternatives: `react-window`, `react-virtuoso`, which is more batteries-included.)

```bash
npm install @tanstack/react-virtual
```

### Fixed-height rows

```tsx
import { useRef } from "react"
import { useVirtualizer } from "@tanstack/react-virtual"

export function VirtualList({ items }: { items: Item[] }) {
  const parentRef = useRef<HTMLDivElement>(null)

  const virtualizer = useVirtualizer({
    count: items.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 48,          // row height in px
    overscan: 5,                     // extra rows rendered above/below the viewport
  })

  return (
    <div ref={parentRef} className="h-[600px] overflow-auto">          {/* scroll container: needs a fixed height */}
      <div style={{ height: virtualizer.getTotalSize(), position: "relative" }}>   {/* full-height spacer */}
        {virtualizer.getVirtualItems().map((row) => (
          <div
            key={row.key}
            style={{
              position: "absolute",
              top: 0,
              left: 0,
              width: "100%",
              height: row.size,
              transform: `translateY(${row.start}px)`,
            }}
          >
            <ItemRow item={items[row.index]} />
          </div>
        ))}
      </div>
    </div>
  )
}
```

How it fits together:

1. **The scroll container** has a bounded height and `overflow: auto`. Without a fixed height, there's no "viewport" and everything renders.
2. **The inner spacer** is as tall as the *whole* list (`getTotalSize()`), so the scrollbar behaves as if all rows existed.
3. **Only `getVirtualItems()`** (visible + overscan) are rendered, each **absolutely positioned** at its computed offset with `translateY`.
4. **`overscan`** renders a few extra rows so fast scrolling doesn't show blank gaps. More overscan means smoother scrolling but more DOM.
5. **`key={row.key}`**, not the index: it keeps item identity stable as the window moves.

### Dynamic-height rows

When row heights vary (wrapped text, expandable content), let the virtualizer **measure** the rendered rows:

```tsx
{virtualizer.getVirtualItems().map((row) => (
  <div
    key={row.key}
    data-index={row.index}                       // required so it can map the element to the item
    ref={virtualizer.measureElement}             // measures the real height after render
    style={{ position: "absolute", top: 0, left: 0, width: "100%", transform: `translateY(${row.start}px)` }}
  >
    <MessageRow message={messages[row.index]} />
  </div>
))}
```

`estimateSize` is only the *initial guess* (make it a decent average to limit scrollbar jumping). Heights are replaced with measured values once rows render. Don't set a fixed `height` on measured rows.

### Window scrolling, horizontal, and grids

- A list that scrolls with the **page** (no inner scroll container): `useWindowVirtualizer`.
- **Horizontal** lists: `horizontal: true` (and use `row.start` on the x-axis).
- **Grids**: use two virtualizers (rows and columns) or a lanes option, depending on the layout. See the library docs.

## Virtualized tables

Virtualize the **rows** of a [TanStack Table](../09-ui-components/07-data-tables.md). The table computes `table.getRowModel().rows`; the virtualizer decides which to render:

```tsx
const { rows } = table.getRowModel()
const virtualizer = useVirtualizer({
  count: rows.length,
  getScrollElement: () => parentRef.current,
  estimateSize: () => 44,
  overscan: 10,
})

{virtualizer.getVirtualItems().map((vr) => {
  const row = rows[vr.index]
  return (/* render <tr> for `row`, positioned at vr.start */)
})}
```

Native `<table>` layout and absolute positioning don't mix easily (table rows can't simply be `position: absolute` without breaking column widths). Common approaches are fixed column widths with `display: grid`/`flex` rows, or top/bottom **padding rows** (spacers) around the visible rows. Check the library's table example for the approach that matches your markup, and keep ARIA roles (`role="table"`, `row`, `cell`) if you leave native table elements.

## With infinite scroll

Combine with [`useInfiniteQuery`](../12-server-state/07-pagination-and-infinite-queries.md#infinite-queries): flatten the pages, virtualize them, and fetch more when the **last virtual item** nears the end:

```tsx
const rows = data.pages.flatMap((p) => p.items)

const virtualizer = useVirtualizer({
  count: hasNextPage ? rows.length + 1 : rows.length,     // +1 row for a "loading more…" placeholder
  getScrollElement: () => parentRef.current,
  estimateSize: () => 64,
  overscan: 5,
})

const items = virtualizer.getVirtualItems()
const last = items[items.length - 1]

useEffect(() => {
  if (last && last.index >= rows.length - 1 && hasNextPage && !isFetchingNextPage) {
    fetchNextPage()
  }
}, [last?.index, rows.length, hasNextPage, isFetchingNextPage, fetchNextPage])
```

This replaces the `IntersectionObserver` sentinel: the virtualizer already knows what's near the end. Gate on `hasNextPage && !isFetchingNextPage` to avoid duplicate fetches.

## Accessibility and usability costs

Virtualization removes content from the DOM, and that has consequences:

- **Browser find (Ctrl/Cmd+F)** can't find text in rows that aren't rendered. Provide your own search/filter.
- **Screen readers** only know about rendered rows. Announce the total size and each item's position so users have context: `aria-rowcount` / `aria-rowindex` for tables and grids, `aria-setsize` / `aria-posinset` on list items.
- **Keyboard focus**: if a focused row scrolls out and unmounts, focus is lost. Manage focus deliberately (keep the focused index rendered, or use roving tabindex with `scrollToIndex`).
- **Jump navigation**: `virtualizer.scrollToIndex(i)` supports "go to row" and anchor features.
- **Print and "select all / copy"** only include rendered rows.
- **Fast-scroll blank flashes** if rows are expensive to render and overscan is too low.

If these costs matter for your product (documents, long articles, accessibility-critical lists), prefer pagination or `content-visibility: auto`.

## Keep rows cheap

Virtualization limits how many rows exist at once, but each visible row still has to render **fast**, especially while scrolling creates and destroys rows constantly:

- Avoid heavy per-row effects, subscriptions, and big images without fixed sizes.
- Memoize the row component if the parent re-renders often ([02](./02-memoization.md)).
- Don't create new state or fetch per row on mount; load data in bulk.
- Reserve image dimensions to avoid measured-height jumps.

## Common mistakes

- **No fixed height on the scroll container**, so nothing virtualizes (everything renders).
- **Using the index as the React key** instead of `row.key`.
- **Forgetting `data-index`** (and `ref={measureElement}`) with dynamic heights.
- **Setting a fixed `height` on rows you also measure.**
- **A poor `estimateSize`**, causing scrollbar jumping.
- **`overscan` of 0**, producing blank flashes during fast scrolls.
- **Virtualizing small lists**, adding complexity for no gain.
- **Putting a virtualized list inside another scroll container or a parent with unconstrained height.**
- **Ignoring accessibility** (no total count, lost focus, no search).
- **Heavy rows** that make scrolling janky even with virtualization.
- **Position changes that animate layout** (`top`/`height`) instead of `transform`.

## Quick summary

- Virtualization renders only visible rows plus overscan, and fakes the rest with a spacer, so DOM size stays constant.
- Use it for **thousands of rows or heavy rows**; otherwise prefer plain rendering, pagination, or `content-visibility`.
- TanStack Virtual: a bounded scroll container, a full-height spacer, absolutely positioned visible items keyed by `row.key`, and `measureElement` + `data-index` for dynamic heights.
- Works with tables (virtualize rows) and infinite queries (fetch when the last virtual item nears the end).
- Mind the costs: find-in-page, screen reader context, focus, and print/copy.
- Keep each row cheap to render.

## Next

[05 — Bundle optimization](./05-bundle-optimization.md)
