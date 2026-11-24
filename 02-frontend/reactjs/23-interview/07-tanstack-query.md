# TanStack Query — Senior Interview Questions & Deep Dives

> Server-state management, cache invalidation strategies, `staleTime` vs `gcTime`, optimistic updates, and React 19 Suspense integration.

---

## Question 1: What is the exact difference between `staleTime` and `gcTime` in TanStack Query v5?

### The Core Definitions
- **`staleTime` (Freshness)**: The duration (in ms) that fetched data is considered "fresh". While fresh, subsequent component mounts or window focuses return data **instantly from cache without background refetching**. Default: `0` (immediately stale).
- **`gcTime` (Garbage Collection / Retention)**: The duration that unused or unmounted query data remains preserved in memory. If no active observers (components) are listening to a query for longer than `gcTime`, the cache entry is deleted to free memory. Default: `5 minutes` (`1000 * 60 * 5`).

### Scenario Example
If `staleTime = 10_000` (10s) and `gcTime = 60_000` (60s):
- At $t=5s$, a component remounts: cache is fresh, returns data instantly, no network call.
- At $t=15s$, a component remounts: cache is stale, returns cached data immediately (instant UI) AND triggers a background refetch.
- At $t=30s$, component unmounts. Timer begins.
- At $t=95s$ (unmounted for 65s > 60s): Garbage collector purges data from memory.

---

## Question 2: How do you implement safe Optimistic Updates with automatic rollback using TanStack Query v5?

```tsx
const queryClient = useQueryClient();

const mutation = useMutation({
  mutationFn: updateTodoOnServer,
  onMutate: async (newTodo) => {
    // 1. Cancel ongoing refetches to avoid overwriting optimistic state
    await queryClient.cancelQueries({ queryKey: ['todos'] });

    // 2. Snapshot the previous cache state
    const previousTodos = queryClient.getQueryData<Todo[]>(['todos']);

    // 3. Optimistically update the cache
    queryClient.setQueryData<Todo[]>(['todos'], (old = []) => [
      ...old,
      { ...newTodo, id: 'temp-id', isPending: true },
    ]);

    // 4. Return context object with snapshot
    return { previousTodos };
  },
  onError: (err, newTodo, context) => {
    // 5. Rollback on failure!
    if (context?.previousTodos) {
      queryClient.setQueryData(['todos'], context.previousTodos);
    }
  },
  onSettled: () => {
    // 6. Invalidate to sync server ground truth
    queryClient.invalidateQueries({ queryKey: ['todos'] });
  },
});
```
