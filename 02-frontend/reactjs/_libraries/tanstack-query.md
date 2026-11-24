# TanStack Query v5 — Basic to Advanced

A comprehensive reference card for `@tanstack/react-query` v5 in React 19 applications.

> **Target Version:** TanStack Query v5.60+  
> **React Version:** 19.0.0+  
> **TypeScript:** 5.5+  

---

## Level 0 — Concepts

- **Server State Caching**: Remote asynchronous data that is owned by external backends, shared, and potentially stale.
- **Query Keys**: Serialized arrays that uniquely identify and index query entries in cache (e.g. `['todos', 'detail', id]`).
- **`staleTime` vs `gcTime`**:
  - `staleTime`: Duration data is considered fresh. Stale queries refetch automatically on window focus or remount.
  - `gcTime`: Duration inactive data remains in memory before being garbage collected (default 5 minutes).
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `QueryClientProvider` | Component | Context provider holding the centralized `QueryClient` cache |
| `useQuery()` | Hook | Standard query fetching with status flags (`isPending`, `data`, `error`) |
| `useSuspenseQuery()` | Hook | Suspense-enabled query with guaranteed non-null `data` |
| `useMutation()` | Hook | Executes server side-effects (POST/PUT/DELETE) with callbacks |
| `useQueryClient()` | Hook | Imperatively invalidates, prefetches, or updates cached data |
| `useInfiniteQuery()` | Hook | Manages cursor or offset-based paginated lists |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install @tanstack/react-query
npm install -D @tanstack/react-query-devtools
```

### 1.2 Provider Configuration
```tsx
// src/providers/QueryProvider.tsx
import React, { useState } from 'react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ReactQueryDevtools } from '@tanstack/react-query-devtools';

export function QueryProvider({ children }: { children: React.ReactNode }) {
  const [queryClient] = useState(
    () =>
      new QueryClient({
        defaultOptions: {
          queries: {
            staleTime: 1000 * 60 * 2, // 2 minutes fresh
            retry: 1,
            refetchOnWindowFocus: false,
          },
        },
      })
  );

  return (
    <QueryClientProvider client={queryClient}>
      {children}
      <ReactQueryDevtools initialIsOpen={false} />
    </QueryClientProvider>
  );
}
```

### 1.3 Basic Query
```tsx
import { useQuery } from '@tanstack/react-query';

export function UserList() {
  const { data, isPending, isError, error } = useQuery({
    queryKey: ['users'],
    queryFn: async () => {
      const res = await fetch('/api/users');
      if (!res.ok) throw new Error('Network error');
      return res.json() as Promise<Array<{ id: string; name: string }>>;
    },
  });

  if (isPending) return <p>Loading users...</p>;
  if (isError) return <p>Error: {error.message}</p>;

  return (
    <ul>
      {data.map((u) => (
        <li key={u.id}>{u.name}</li>
      ))}
    </ul>
  );
}
```

---

## Level 2 — Intermediate: Mutations & Invalidation

### 2.1 Executing Mutations with Cache Invalidation
```tsx
import { useMutation, useQueryClient } from '@tanstack/react-query';

export function CreateUserButton() {
  const queryClient = useQueryClient();

  const mutation = useMutation({
    mutationFn: async (newUser: { name: string; email: string }) => {
      const res = await fetch('/api/users', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(newUser),
      });
      if (!res.ok) throw new Error('Create failed');
      return res.json();
    },
    onSuccess: () => {
      // Invalidate users list to trigger fresh background refetch
      queryClient.invalidateQueries({ queryKey: ['users'] });
    },
  });

  return (
    <button
      onClick={() => mutation.mutate({ name: 'Sam', email: 'sam@example.com' })}
      disabled={mutation.isPending}
    >
      {mutation.isPending ? 'Creating...' : 'Create User'}
    </button>
  );
}
```

---

## Level 3 — Advanced: Suspense & Optimistic Updates

### 3.1 Optimistic Rollback Pattern
```tsx
const mutation = useMutation({
  mutationFn: updateTodoOnServer,
  onMutate: async (newTodo) => {
    // 1. Cancel in-flight queries
    await queryClient.cancelQueries({ queryKey: ['todos'] });
    // 2. Snapshot previous value
    const previousTodos = queryClient.getQueryData(['todos']);
    // 3. Optimistically set new cache state
    queryClient.setQueryData(['todos'], (old: any) => [...old, newTodo]);
    return { previousTodos };
  },
  onError: (_err, _newTodo, context) => {
    // Rollback on network failure
    if (context?.previousTodos) {
      queryClient.setQueryData(['todos'], context.previousTodos);
    }
  },
  onSettled: () => {
    // Always refetch to sync true server state
    queryClient.invalidateQueries({ queryKey: ['todos'] });
  },
});
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Fails | Recommended Practice |
| :--- | :--- | :--- |
| **`new QueryClient()` in Component Body** | Resets cache on every parent re-render. | Wrap in `useState(() => new QueryClient())`. |
| **Hardcoded Inline Keys (`['todos', id]`)** | Missed invalidations due to typos. | Use Query Key Factories (`todoKeys.detail(id)`). |
| **Default `staleTime: 0` on SSR Hydration** | Refetches immediately on client boot. | Set `staleTime: 60_000` to prevent double-fetches. |
| **Global `QueryClient` Singleton on Server** | Data leaks between concurrent visitor sessions. | Create request-scoped client per SSR request. |
| **Using `useEffect` for Fetching** | Race conditions and missing deduplication. | Replace with `useQuery` / `useSuspenseQuery`. |
