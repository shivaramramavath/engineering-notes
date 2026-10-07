# Parallel and Sequential Fetching

How you arrange multiple data requests decides how fast a page loads. **Sequential** fetches run one after another, so total time is the **sum**. **Parallel** fetches start together, so total time is roughly the **slowest one**. An unintended sequence of requests is called a **waterfall**, and it is one of the most common performance problems in server-rendered apps.

```text
Sequential (waterfall):  [── A 300ms ──][── B 400ms ──][── C 200ms ──]   = 900ms
Parallel:                [── A 300ms ──]
                         [── B 400ms ──]                                 = 400ms
                         [── C 200ms ──]
```

## When sequential is correct

Sometimes one request genuinely needs the result of another:

```tsx
export default async function Page({
  params,
}: {
  params: Promise<{ username: string }>;
}) {
  const { username } = await params;
  const user = await getUser(username);          // needed first
  const orders = await getOrders(user.id);       // depends on user.id
  return <OrderList user={user} orders={orders} />;
}
```

That is a real dependency. The goal is not to remove it but to avoid *accidental* sequencing, and to keep dependent work from blocking unrelated work.

## Accidental waterfalls

```tsx
// Slow: independent requests run one after another
const user = await getUser(id);
const posts = await getPosts(id);
const stats = await getStats(id);
```

None of these needs another's result, but `await` pauses before the next line starts.

### Fix 1: `Promise.all`

```tsx
const [user, posts, stats] = await Promise.all([
  getUser(id),
  getPosts(id),
  getStats(id),
]);
```

Start all requests, then wait for all of them. Caveats:

- `Promise.all` **rejects as soon as one rejects**. If partial results are acceptable, use `Promise.allSettled`.
- The page waits for the slowest request before rendering anything. To avoid that, use Suspense (below).

```tsx
const [userResult, postsResult] = await Promise.allSettled([getUser(id), getPosts(id)]);
if (postsResult.status === "fulfilled") { /* use postsResult.value */ }
```

### Fix 2: Suspense boundaries per component

Each async Server Component starts fetching when React renders it, so sibling components inside separate `<Suspense>` boundaries fetch in **parallel**, and each streams as soon as it resolves:

```tsx
import { Suspense } from "react";

export default async function Page({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  return (
    <>
      <Suspense fallback={<p>Loading user…</p>}>
        <UserCard id={id} />
      </Suspense>
      <Suspense fallback={<p>Loading posts…</p>}>
        <PostList id={id} />
      </Suspense>
    </>
  );
}
```

Neither component waits for the other, and neither blocks the page shell. See [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md).

Use `Promise.all` when the data must arrive together; use separate boundaries when sections can appear independently.

## Hidden waterfall: parent awaits, then renders child

```tsx
// Parent fetches, then renders a child that fetches
export default async function Page() {
  const user = await getUser();          // 300ms: nothing renders until done
  return <Posts userId={user.id} />;     // Posts then starts its own 400ms fetch
}
```

If `Posts` did not really need `user` (for example, it could use the id from the URL), render it earlier or fetch both at the same level. If it does need it, wrap the dependent part in Suspense so the rest of the page is not blocked:

```tsx
export default async function Page({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  return (
    <>
      <Header />                                   {/* renders immediately */}
      <Suspense fallback={<PostsSkeleton />}>
        <Posts userId={id} />                      {/* fetches inside */}
      </Suspense>
    </>
  );
}
```

Layouts and pages for the same URL are rendered in parallel on the server, so their fetches do not automatically wait for each other. Waterfalls come from a component awaiting data and *then* rendering a child that awaits more.

## Preloading

When a dependent request depends on a *different* slow request, you can start the independent one early and await later:

```ts
// lib/data/item.ts
import "server-only";
import { cache } from "react";

export const getItem = cache(async (id: string) => {
  return db.item.findUnique({ where: { id } });
});

export const preloadItem = (id: string) => {
  void getItem(id); // start the request, do not await
};
```

```tsx
export default async function Page({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  preloadItem(id);                          // kick off the request now

  const allowed = await checkAccess();      // slower check runs while item loads
  if (!allowed) return <Denied />;

  const item = await getItem(id);           // already in flight / resolved (deduplicated by cache())
  return <Item item={item} />;
}
```

Because `getItem` is wrapped in `cache()`, the second call reuses the first call's result within the same request. `server-only` keeps the module off the client.

## Loops: the classic trap

```tsx
// Sequential: N requests one by one
for (const id of ids) {
  results.push(await getItem(id));
}

// Parallel
const results = await Promise.all(ids.map((id) => getItem(id)));
```

Two further notes:

- **N+1 queries:** fetching a list, then one request per row, is slow even in parallel. Prefer a single batched query (`WHERE id IN (...)` or an ORM `include`). See [Database Architecture](../12-database/00-database-architecture.md).
- **Concurrency limits:** firing hundreds of simultaneous requests can overload an API or the database pool. Batch or limit concurrency for large lists.

## Client-side waterfalls

The same issue appears in the browser:

```tsx
// Chained effects: component mounts → fetch A → render child → fetch B
```

A child component's fetch does not start until the parent renders it, which does not happen until the parent's data arrives. Move the independent fetches to the server and run them in parallel, or use a library that lets you fetch several queries at once. See [Client Fetching](./01-client-fetching.md).

## Finding waterfalls

- Add timing logs around fetches in development: `console.time("posts")` / `console.timeEnd("posts")`. Overlapping timings mean parallel; end-to-start chains mean sequential.
- Look at the browser's Network tab for requests that start only after another finishes.
- Check whether a component awaits before rendering children that also fetch.

## Choosing

| Situation | Approach |
|---|---|
| B needs A's result | Sequential; isolate B in Suspense so the rest renders |
| Independent data, needed together | `Promise.all` |
| Independent data, sections can appear separately | Separate `<Suspense>` boundaries |
| Partial failure acceptable | `Promise.allSettled` |
| One slow check gates others | Preload the rest, then await |
| List of ids | Batched query, or `Promise.all` with a limit |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Sequential `await`s for independent data | Load time is the sum | `Promise.all` or separate Suspense boundaries |
| `await` above `<Suspense>` | Fallback never shows | Fetch inside the suspended component |
| `await` in a `for` loop | Slow lists | `Promise.all(ids.map(...))` or a batch query |
| `Promise.all` where one failure should not break the page | Whole page errors | `Promise.allSettled` |
| Parent awaits, child awaits | Hidden waterfall | Restructure or isolate with Suspense |
| N+1 queries | Many tiny DB calls | Batch with a single query |
| Unbounded parallelism | API rate limits, DB pool exhaustion | Limit concurrency |

## Quick Summary

- Sequential time adds up; parallel time is the slowest request.
- Only chain requests that truly depend on each other.
- Use `Promise.all` for independent data that is needed together, separate Suspense boundaries for independent sections.
- `await` placement matters: parents that await block children and Suspense fallbacks.
- Use preloading with `cache()` to start requests early.
- Avoid awaits in loops and N+1 queries.

## Next

- [06 · Caching](../06-caching/README.md)
- [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md)
- [Server Performance](../20-performance/03-server-performance.md)
