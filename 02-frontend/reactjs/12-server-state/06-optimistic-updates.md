# Optimistic Updates

A normal mutation waits for the server: click "like", see a spinner, then the heart fills. An **optimistic update** assumes the write will succeed and updates the UI **immediately**, then reconciles when the server answers. If the request fails, the UI rolls back.

```text
click ─► UI updates NOW ─► request in flight ─┬─► success: confirm (refetch/replace)
                                              └─► failure: roll back + tell the user
```

Done well, the app feels instant. Done carelessly, it shows lies, loses data, and leaves the cache corrupted. The technique is simple; the discipline is knowing when it's appropriate.

## When to use it

Good fits are actions that are **frequent, low-stakes, and very likely to succeed**:

- Toggle a like, favorite, or checkbox
- Reorder or drag items
- Rename inline, mark a todo done
- Delete with undo available
- Sending a chat message (shown immediately as "sending")

Avoid it for **payments, irreversible actions, or anything with complex server-side rules** that frequently reject the request (permissions, inventory, uniqueness). If failure is common, rollbacks become the normal experience, which is worse than a spinner. When in doubt, make users wait and show clear progress.

## Approach 1: UI-only (simplest)

While a mutation is pending, its input is available as `variables`. Render the optimistic item from that, **without touching the cache**:

```tsx
function TodoList() {
  const { data: todos = [] } = useQuery(todosQuery())
  const addTodo = useMutation({
    mutationFn: (text: string) => todosApi.create({ text }),
    onSettled: () => queryClient.invalidateQueries({ queryKey: todoKeys.all }),
  })

  return (
    <ul>
      {todos.map((t) => <li key={t.id}>{t.text}</li>)}

      {addTodo.isPending && (
        <li className="opacity-50">{addTodo.variables}</li>      {/* the optimistic row */}
      )}
      {addTodo.isError && (
        <li className="text-destructive">
          {addTodo.variables} <button onClick={() => addTodo.mutate(addTodo.variables)}>Retry</button>
        </li>
      )}
    </ul>
  )
}
```

- **No rollback code**: when the mutation fails, the pending row just becomes an error row, and the cache never changed.
- Great when the optimistic result appears in **one place**.
- Limitation: other components reading the same query won't see the optimistic item. For that, use the cache approach.

You can also gather in-flight mutations app-wide with `useMutationState` ([05](./05-mutations.md#mutation-state-elsewhere)).

## Approach 2: update the cache (shared, with rollback)

Write the optimistic value into the cache so **every** component reflects it, and restore a snapshot on failure:

```tsx
const toggleTodo = useMutation({
  mutationFn: (todo: Todo) => todosApi.update(todo.id, { done: !todo.done }),

  onMutate: async (todo) => {
    // 1. Stop in-flight refetches from overwriting our optimistic value
    await queryClient.cancelQueries({ queryKey: todoKeys.lists() })

    // 2. Snapshot the current value for rollback
    const previous = queryClient.getQueryData<Todo[]>(todoKeys.list(filters))

    // 3. Optimistically update (immutably!)
    queryClient.setQueryData<Todo[]>(todoKeys.list(filters), (old) =>
      old?.map((t) => (t.id === todo.id ? { ...t, done: !t.done } : t))
    )

    // 4. Return the snapshot as context for onError
    return { previous }
  },

  onError: (_error, _todo, context) => {
    // Roll back to the snapshot
    queryClient.setQueryData(todoKeys.list(filters), context?.previous)
    toast.error("Couldn't update the todo")
  },

  onSettled: () => {
    // Success or failure: re-sync with the server's truth
    return queryClient.invalidateQueries({ queryKey: todoKeys.lists() })
  },
})
```

> Recent v5 releases rename and extend the callback arguments (the value returned from `onMutate` and a context object). The four-step pattern is unchanged, but check your version's signature for how the returned snapshot reaches `onError`.

The four steps are the pattern. Each is there for a reason:

| Step | Why |
|---|---|
| **`cancelQueries`** | A refetch already in flight would land *after* your optimistic write and overwrite it with the old server data, so the item flickers back. |
| **Snapshot** | You need the exact previous state to restore on failure. |
| **`setQueryData` immutably** | Mutating the cached array in place breaks change detection and corrupts your snapshot (it would be the same object). |
| **Invalidate in `onSettled`** | After either outcome, the server is the source of truth. This also corrects any drift between your guess and reality (server-assigned fields, ordering). |

## Optimistic create: temporary IDs

A new item doesn't have a server ID yet. Give it a temporary one and make it visibly pending:

```ts
onMutate: async (input: NewTodo) => {
  await queryClient.cancelQueries({ queryKey: todoKeys.lists() })
  const previous = queryClient.getQueryData<Todo[]>(todoKeys.list(filters))

  const optimistic: Todo = { id: `temp-${crypto.randomUUID()}`, ...input, done: false, pending: true }
  queryClient.setQueryData<Todo[]>(todoKeys.list(filters), (old = []) => [...old, optimistic])
  return { previous }
},
```

- Disable actions on temp rows (you can't edit or delete an ID the server doesn't know yet).
- Use a **stable React key**. If the key changes from `temp-…` to the real ID when the server responds, React remounts the row (flicker, lost focus). Keep a separate client-side key if that matters.
- `onSettled` invalidation replaces the temp item with the real one.

## Optimistic delete

```ts
onMutate: async (id: string) => {
  await queryClient.cancelQueries({ queryKey: todoKeys.lists() })
  const previous = queryClient.getQueryData<Todo[]>(todoKeys.list(filters))
  queryClient.setQueryData<Todo[]>(todoKeys.list(filters), (old) => old?.filter((t) => t.id !== id))
  return { previous }
},
```

Pair with an **Undo toast** and delay the actual request, or make the server reversible, so a mistake isn't costly.

## Updating several caches

A single change can appear in a detail view, several filtered lists, and counts. Options:

1. **Update the one on screen, invalidate the rest** (`onSettled`). Simple and usually enough.
2. **Update all affected keys** by iterating:

```ts
queryClient.setQueriesData<Todo[]>({ queryKey: todoKeys.lists() }, (old) =>
  old?.map((t) => (t.id === id ? { ...t, done: true } : t))
)
```

`setQueriesData` applies the updater to **every** matching entry, though you must snapshot each one for rollback (use `getQueriesData` for that). The more caches you patch, the more you reimplement server logic, such as whether the item still matches a filter. Prefer invalidation for anything beyond trivial cases.

## Concurrency: the hard part

Several optimistic mutations in flight at once can corrupt each other:

1. You toggle A → snapshot S0, optimistic S1.
2. You toggle B (before A finishes) → snapshot **S1**, optimistic S2.
3. A fails → rolls back to **S0**, wiping B's optimistic change too.
4. B succeeds, and `onSettled` refetches, so it eventually corrects, but the UI jumps.

Mitigations:

- **Only invalidate when the last mutation settles**, so intermediate refetches don't clobber newer optimistic state:

```ts
onSettled: () => {
  if (queryClient.isMutating({ mutationKey: ["todos"] }) === 1) {
    return queryClient.invalidateQueries({ queryKey: todoKeys.lists() })
  }
},
```

  (Set `mutationKey: ["todos"]` on these mutations; `1` is "just this one".)
- **Serialize conflicting writes**: disable the control while its mutation is pending, or queue per item.
- Rely on the final **invalidate** to converge to the server's truth, and accept brief jumps for rare failures.

If correctness under concurrency really matters, that's a sign to avoid optimism, or to adopt a sync engine built for it.

## Rolling back well

A rollback is a UX event, not just a cache operation:

- **Tell the user** what failed (toast or inline), ideally with a retry. A silent revert looks like a bug.
- Don't roll back *over* newer changes the user made meanwhile (see concurrency).
- Keep the failing input around where practical (the UI-only approach does this naturally).
- Report the error ([API error handling](../11-api-integration/05-api-error-handling.md)).

## Other optimistic tools

- **React Router fetchers** expose `fetcher.formData` for simple optimistic UI in loader/action apps ([route data loading](../10-routing/05-route-data-loading.md#fetchers-mutations-without-navigation)).
- **React 19 `useOptimistic`** is a built-in hook for showing a temporary value while an async action runs, then reverting to the real state when it completes. It fits form actions and `useTransition`. With TanStack Query, the `variables` approach above or cache updates usually fit better, because the server data lives in the query cache.

## Testing and debugging

- Slow the network in DevTools (and force failures) to see the pending and rollback paths. The happy path with a fast server hides every bug.
- Use the Query devtools to watch the cache entry flip to optimistic data, then settle.
- Test the failure path explicitly: mock a 500 and assert the UI rolls back and shows feedback.
- "Item flickers back for a moment" → missing `cancelQueries`.
- "Rollback restores the wrong thing" → snapshot taken after the optimistic write, or mutated in place.
- "UI never corrects after failure" → missing `onSettled` invalidation.

## Common mistakes

- **Optimism for actions that often fail** or can't be undone.
- **Skipping `cancelQueries`**, so in-flight refetches overwrite the optimistic state.
- **Mutating cache data in place**, so snapshots alias the live value and rollback does nothing.
- **No `onSettled` invalidation**, leaving client guesses uncorrected.
- **Silent rollbacks** with no message to the user.
- **Ignoring concurrent mutations**, so one failure wipes out another's optimistic change.
- **Changing React keys** when a temp ID becomes a real ID.
- **Allowing actions on temp-ID items** (edit/delete of something the server hasn't created).
- **Patching many caches by hand** instead of invalidating.
- **Only testing the fast, successful path.**

## Quick summary

- Optimistic update = change the UI first, reconcile after, roll back on failure. Use it for frequent, low-risk, usually-successful actions.
- **Simplest:** render from `mutation.variables` while pending, and leave the cache alone.
- **Shared:** `onMutate` → `cancelQueries`, snapshot, `setQueryData` (immutably), return snapshot; `onError` → restore; `onSettled` → invalidate.
- Create uses temp IDs with stable keys; delete pairs well with undo.
- Concurrent optimistic writes need care: invalidate only after the last one settles, or serialize.
- Always tell the user when something rolls back, and test the failure path.

## Next

[07 — Pagination and infinite queries](./07-pagination-and-infinite-queries.md)