# Server State vs Client State

"State" in React usually means `useState`. But there are really two different kinds of state with different problems, and treating them the same is the root of most data-fetching mess.

## The distinction

**Client state** is owned by your app. It exists only in the browser and you are the source of truth: is the sidebar open, what's typed in a field, which tab is selected, the current theme.

**Server state** is owned by someone else. It lives on a server; the browser holds a **snapshot** that might already be out of date.

| | Client state | Server state |
|---|---|---|
| Source of truth | Your component/app | The server |
| Access | Synchronous | **Asynchronous** (loading, error) |
| Can change without you | No | **Yes**: other users, other tabs, background jobs |
| Can be stale | No | **Yes**, always, by definition |
| Shared across components | Only if you wire it up | Constantly (same user in header and profile page) |
| Needs | `useState`, context, a store | Fetching, caching, deduping, refetching, invalidating |

Server state brings problems client state never has:

- Loading and error states for every read
- **Staleness**: when to refetch, and how often
- **Duplicate requests** when three components want the same data
- **Race conditions** when responses arrive out of order
- **Invalidation**: after a write, which cached reads are now wrong?
- Pagination, retries, background updates, and memory cleanup of data nobody shows anymore

You can solve each by hand ([01](./01-fetching-data.md) shows what that looks like), but you'd be building a cache library, badly.

## Where each kind of state belongs

| Kind | Example | Where it lives |
|---|---|---|
| **Server state** | Projects list, current user profile, comments | A server-state cache (TanStack Query) |
| **URL state** | Search text, filters, page, selected tab | The URL: [search params](../10-routing/06-search-filter-and-url-state.md) |
| **Form state** | Values and errors of a form being edited | A form library ([React Hook Form](../06-forms/02-react-hook-form.md)) or local state |
| **Local UI state** | Is this menu open? Hovered? Expanded? | `useState` in the component |
| **Shared client state** | Theme, sidebar collapsed, multi-step wizard progress | Context or a small store ([state management](../13-state-management/README.md)) |

Once server state moves into a dedicated cache, what's left for "global state" is surprisingly small. Many apps find they no longer need Redux at all.

## The cardinal rule: don't copy server state

The most common mistake:

```tsx
// ✗ A copy of server data in local state
const [projects, setProjects] = useState<Project[]>([])

useEffect(() => {
  projectsApi.list().then(setProjects)
}, [])
```

Now *you* are responsible for keeping that copy fresh. After you create a project, the copy is wrong until you patch it. Another component showing the same list holds its own, possibly different, copy. Navigating away and back refetches from scratch. The same mistake happens when server data is pushed into Redux or Zustand: you've built a cache without invalidation rules.

```tsx
// ✓ Read server state from the cache; it owns freshness
const { data: projects } = useQuery({
  queryKey: ["projects"],
  queryFn: () => projectsApi.list(),
})
```

The component **reads**; the cache **owns** the data. Every component asking for `["projects"]` shares one entry and one request.

### Related trap: copying into state to edit

```tsx
// ✗ Seeding state from props/query data and expecting it to follow updates
const { data: user } = useUser()
const [name, setName] = useState(user?.name ?? "")   // initial value only; never updates
```

`useState`'s initial value is used once. If `user` arrives later or refreshes, `name` doesn't change. For editable forms, either render the form **after** the data exists and pass `defaultValues`, or reset it when the record changes ([state preservation and reset](../02-state-and-rendering/05-state-preservation-and-reset.md)). Treat the draft as **form state** that starts from server state, not as a copy that tracks it.

## Derive, don't store

If it can be computed from server data, compute it:

```tsx
const { data: projects = [] } = useProjectsQuery()
const openCount = projects.filter((p) => p.status === "open").length   // derived, always consistent
```

Storing `openCount` in state creates a second source of truth that you must keep in sync. TanStack Query's `select` option does this transformation with memoization ([03](./03-tanstack-query.md#select-derive-from-the-cache)).

## "Fresh enough"

Server state is never *correct*, only *fresh enough*. The design question isn't "how do I load this?" but "how stale can this be before it matters?":

| Data | Tolerable staleness |
|---|---|
| Country list, feature flags | Hours or days |
| A user's profile | Minutes |
| Dashboard metrics | Seconds to a minute |
| Stock price, chat | Seconds, or push ([realtime](../11-api-integration/06-realtime-communication.md)) |

That answer becomes your `staleTime` ([04](./04-caching-and-synchronization.md)).

## Common mistakes

- **Mirroring server data in `useState`/Redux/Zustand** and hand-maintaining it.
- **Seeding `useState` from query data** and expecting it to update.
- **Storing derived values** that could be computed.
- **Treating loaded data as permanent truth**: it's a snapshot that ages.
- **Putting UI state in the server cache**, or putting server data in the UI store.
- **One giant "global store" for everything**, so UI toggles and API data share rules that suit neither.
- **Ignoring staleness**: users see yesterday's data after someone else edits it.

## Quick summary

- Client state: you own it, synchronous, always current. Server state: someone else owns it, async, always potentially stale.
- Server state needs caching, deduping, refetching, and invalidation; use a purpose-built cache.
- Components **read** from the cache; don't copy server data into local or global state.
- Derive values instead of storing them; treat editable drafts as form state.
- Choose how stale is acceptable per kind of data.

## Next

[01 — Fetching data](./01-fetching-data.md)
