# Server State with TanStack Query

**Server state** is data that lives on a server and that your UI borrows: users, orders, search results. It is different from UI state (an open modal, a form draft): it is asynchronous, can be stale, can be changed by others, and is shared across components. Hand-rolling it with `useState` and `useEffect` means re-implementing caching, deduplication, retries, cancellation, and race-condition handling. TanStack Query (formerly React Query) does that for you, and it has strong TypeScript support. This note uses the v5 API.

> **Version note.** v5 changed several APIs: only the object form (`useQuery({ queryKey, queryFn })`) exists, `isLoading` for the first load became `isPending`, `cacheTime` became `gcTime`, and `useInfiniteQuery` requires `initialPageParam`. Older tutorials show the v4 form. Check which major version you have.

**Prerequisites:**
- [Hooks](./02-hooks.md)
- [Typed fetch and API client](../16-type-safe-apis/05-typed-fetch-and-api-client.md)
- [Schema validation](../15-runtime-validation/01-schema-validation.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)

---

## Why not `useEffect` + `useState`

```tsx
const [user, setUser] = useState<User | null>(null);
const [loading, setLoading] = useState(true);
const [error, setError] = useState<Error | null>(null);

useEffect(() => {
  fetch(`/api/users/${id}`).then((r) => r.json()).then(setUser).catch(setError).finally(() => setLoading(false));
}, [id]);
```

Problems: no caching (every mount refetches), no deduplication (two components fetch twice), responses can arrive out of order when `id` changes, nothing is cancelled, there is no retry or refetch-on-focus, and the three states can disagree. A query library solves all of these, and the typed hook gives you one state object.

## Setup

```tsx
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";

const queryClient = new QueryClient();

function Root() {
  return (
    <QueryClientProvider client={queryClient}>
      <App />
    </QueryClientProvider>
  );
}
```

## `useQuery`

```tsx
import { useQuery } from "@tanstack/react-query";

function UserProfile({ id }: { id: string }) {
  const query = useQuery({
    queryKey: ["user", id],
    queryFn: ({ signal }) => api("GET /users/:id", { params: { id }, signal }),
  });

  if (query.isPending) return <Spinner />;
  if (query.isError) return <ErrorMessage error={query.error} />;

  return <h1>{query.data.name}</h1>;      // data is User here
}
```

- **`queryKey`** identifies the data. It is an array that includes everything the query depends on (`["user", id]`). When a key changes, the query refetches, and results are cached **per key**.
- **`queryFn`** returns a promise of the data. It receives a context with an `AbortSignal` (`signal`), which you can pass to `fetch` so superseded requests are cancelled ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)).
- The **data type is inferred** from `queryFn`'s return type, so with a typed client, `query.data` is `User | undefined` with no annotation.

### The result is a discriminated union

`useQuery` returns an object whose `status` is `"pending"`, `"error"`, or `"success"`, and TypeScript narrows on the boolean flags:

```tsx
if (query.isPending) { /* data: undefined */ }
if (query.isError)   { /* error: Error, data: undefined (or stale data) */ }
// here: status "success", query.data is defined
```

After handling `isPending` and `isError`, `query.data` is no longer `undefined`. This is far safer than three independent `useState` values ([state machines](../17-design-patterns/06-state-machines.md)). Other useful flags: `isFetching` (any fetch, including background refetches), `isRefetching`, and `isPlaceholderData`.

## Query keys and key factories

Keys are strings and objects in arrays. Keep them consistent by building them in one place:

```ts
export const userKeys = {
  all: ["users"] as const,
  list: (filters: UserFilters) => [...userKeys.all, "list", filters] as const,
  detail: (id: string) => [...userKeys.all, "detail", id] as const,
};

useQuery({ queryKey: userKeys.detail(id), queryFn: () => getUser(id) });
queryClient.invalidateQueries({ queryKey: userKeys.all });     // invalidates every users query
```

`as const` keeps the keys as precise tuples. Keys are matched by **prefix**, so invalidating `["users"]` also invalidates `["users", "detail", "1"]`.

## `queryOptions`: reusable, typed query definitions

`queryOptions` bundles the key and function so the same typed definition can be used with `useQuery`, `prefetchQuery`, `getQueryData`, and the like:

```ts
import { queryOptions } from "@tanstack/react-query";

export const userQuery = (id: string) =>
  queryOptions({
    queryKey: ["user", id],
    queryFn: () => getUser(id),
    staleTime: 60_000,
  });

const { data } = useQuery(userQuery(id));                  // User | undefined
const cached = queryClient.getQueryData(userQuery(id).queryKey);   // User | undefined, inferred from the key
```

The key carries the data type, so `getQueryData` and `setQueryData` are typed without you repeating `<User>`. This is the recommended way to share queries.

## Validate responses

A typed client is only as good as the data it parses. Validate in the `queryFn`, so the cache never holds data of the wrong shape ([validation recipes](../15-runtime-validation/04-validation-recipes.md)):

```ts
const userQuery = (id: string) =>
  queryOptions({
    queryKey: ["user", id],
    queryFn: async ({ signal }) => {
      const res = await fetch(`/api/users/${id}`, { signal });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return UserSchema.parse(await res.json());        // User
    },
  });
```

## Typing errors

In v5, the default error type is `Error`. If your API throws a custom error class, tell TypeScript once, globally:

```ts
declare module "@tanstack/react-query" {
  interface Register {
    defaultError: ApiClientError;
  }
}
```

Now `query.error` is `ApiClientError | null` everywhere, so you can read `query.error.error.code` ([global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md), [error response types](../16-type-safe-apis/04-error-response-types.md)). The thrown value is a **claim**: the library does not verify it, so throw only instances of that type from your query functions, and handle other failures (network errors) in the same class if you promise a single type. You can also pass the error type as a generic on a single query.

## Transforming data with `select`

`select` derives the value a component needs, and its return type becomes `data`:

```tsx
const { data: names } = useQuery({
  ...usersQuery(),
  select: (users) => users.map((u) => u.name),      // data: string[] | undefined
});
```

Components using different `select`s share the same cache entry, and each re-renders only when its selected result changes.

## Mutations

Mutations change data on the server. They are typed by their **variables** and **result**:

```tsx
import { useMutation, useQueryClient } from "@tanstack/react-query";

function useRenameUser() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (vars: { id: string; name: string }) =>
      api("PATCH /users/:id", { params: { id: vars.id }, body: { name: vars.name } }),
    onSuccess: (updated, vars) => {
      queryClient.setQueryData(userQuery(vars.id).queryKey, updated);          // update the cache
      void queryClient.invalidateQueries({ queryKey: userKeys.all });          // refresh related queries
    },
  });
}

const rename = useRenameUser();
rename.mutate({ id: "1", name: "Asha" });              // variables are checked
rename.mutate({ id: "1" });                            // error: name is missing
```

Use `mutate` (callback style) or `mutateAsync` (returns a promise to `await`, and you must catch its rejection). After a mutation, either **invalidate** affected queries so they refetch, or **write the result into the cache** with `setQueryData`.

### Optimistic updates

Update the UI immediately, and roll back on failure:

```tsx
useMutation({
  mutationFn: toggleTodo,
  onMutate: async (id: string) => {
    await queryClient.cancelQueries({ queryKey: todoKeys.all });
    const previous = queryClient.getQueryData<Todo[]>(todoKeys.list());
    queryClient.setQueryData<Todo[]>(todoKeys.list(), (old) =>
      old?.map((t) => (t.id === id ? { ...t, done: !t.done } : t)),
    );
    return { previous };                                // becomes `context` in onError
  },
  onError: (_err, _id, context) => {
    queryClient.setQueryData(todoKeys.list(), context?.previous);
  },
  onSettled: () => {
    void queryClient.invalidateQueries({ queryKey: todoKeys.all });
  },
});
```

The object returned from `onMutate` is typed and flows into `onError` as `context`. Optimistic updates add complexity, so use them where instant feedback matters.

## Pagination and infinite lists

```tsx
const query = useInfiniteQuery({
  queryKey: ["posts"],
  queryFn: ({ pageParam }) => api("GET /posts", { query: { cursor: pageParam, limit: 20 } }),
  initialPageParam: undefined as string | undefined,
  getNextPageParam: (lastPage) => lastPage.nextCursor ?? undefined,
});

const posts = query.data?.pages.flatMap((p) => p.items) ?? [];
// query.fetchNextPage(), query.hasNextPage
```

`initialPageParam` is required in v5, and its type determines `pageParam`. This pairs with cursor pagination from [pagination types](../16-type-safe-apis/03-pagination-types.md). For page-number UIs, use a normal `useQuery` with the page in the key and `placeholderData: keepPreviousData` so the previous page stays visible while the next loads.

## Suspense and server rendering

- **`useSuspenseQuery`** suspends while loading and throws on error, so `data` is always defined with no `isPending` check. Wrap in `<Suspense>` and an error boundary.
- **Prefetching and hydration** let a server render fetch data ahead of time and hand the cache to the client, so there is no loading flash. In Next.js, this involves creating a `QueryClient` per request and passing a dehydrated state to a client provider ([Next.js](./09-nextjs.md)).

## Staleness and caching

| Setting | Meaning | Default |
|---|---|---|
| `staleTime` | how long data counts as fresh, with no refetch on mount or focus | `0` (always stale) |
| `gcTime` | how long **unused** data stays in the cache | 5 minutes |
| `retry` | automatic retries on failure | 3 (for queries) |
| `refetchOnWindowFocus` | refetch when the tab regains focus | on |

Raise `staleTime` for data that does not change often, to avoid redundant requests. Disable retry for errors that will not fix themselves (`404`, `401`, validation errors) with a `retry` function that inspects the error.

## Do not copy server data into state

```tsx
// anti-pattern: copying creates two sources of truth that drift
const { data } = useQuery(userQuery(id));
const [name, setName] = useState(data?.name ?? "");     // stale after a refetch
```

Use the query data directly. For an editable draft, initialize local state **once** (for example in a form component that mounts after the data is loaded) and treat it as a separate draft ([forms](./05-forms.md)).

## Testing

Create a fresh `QueryClient` for every test (retries off, so failures surface immediately), wrap the component, and mock the network:

```tsx
function renderWithQuery(ui: ReactElement) {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  return render(<QueryClientProvider client={client}>{ui}</QueryClientProvider>);
}
```

Sharing a client between tests leaks cached data and makes tests order-dependent. Intercept HTTP with a network-level mock ([integration testing](../18-testing-and-debugging/01-integration-testing.md)).

## Alternatives

SWR is a similar, lighter library. Frameworks have their own data loading (Next.js server components, Remix loaders, React Router loaders). RTK Query integrates with Redux. The concepts (keys, staleness, invalidation) carry over.

## Common mistakes

- Putting server data in `useState` or a global store, and managing loading and errors by hand.
- Missing values in the `queryKey` that the query depends on, so stale data is shown for a new input.
- Mutating cached data in place instead of returning new objects in `setQueryData`.
- Forgetting to invalidate or update related queries after a mutation.
- Using `mutateAsync` without catching its rejection.
- Using v4 syntax (positional arguments, `isLoading`, `cacheTime`) with v5.
- Trusting the response type without validating it.
- Sharing one `QueryClient` across tests or, in SSR, across requests.
- Throwing non-`Error` values and assuming the error type is what you registered.
- Assuming `isLoading` equals "no data yet" (in v5, use `isPending`).

## Debugging

- Install the TanStack Query DevTools to inspect each query's key, status, data, and staleness.
- If a query refetches constantly, check for an unstable `queryKey` (a new object each render) or a `staleTime` of `0`.
- If data does not update after a mutation, check the key you invalidated against the key the query uses.
- If `data` is `undefined` unexpectedly, check `enabled` and the status flags.
- Log inside `queryFn` to see how often and with what arguments it runs.
- Hover `query` and `data` to see inferred types. If `data` is `unknown` or `any`, the `queryFn` is untyped.

## Quick summary

- Server state is cached, shared, and asynchronous. Use a query library instead of `useEffect` + `useState`.
- `useQuery({ queryKey, queryFn })` infers `data` from the function. The result narrows by status, so after `isPending` and `isError` checks `data` is defined.
- Build keys in one place, and share definitions with `queryOptions`. Pass the `signal` for cancellation, and validate responses in the `queryFn`.
- Register a default error type with module augmentation. Type mutations by variables and result, and invalidate or update the cache afterward.
- Use `useInfiniteQuery` (with `initialPageParam`) for cursor lists, and do not copy server data into local state.

**Next:** [Client state with Zustand](./08-client-state-zustand.md)
