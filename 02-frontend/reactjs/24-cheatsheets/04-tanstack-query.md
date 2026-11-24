# TanStack Query v5 Cheatsheet

> Quick-reference guide for TanStack Query v5 queries, mutations, query key factories, cache invalidation, and suspense hooks.

---

## 1. Basic Query & Suspense

```tsx
import { useQuery, useSuspenseQuery } from '@tanstack/react-query';

// Standard Query
const { data, isLoading, isError, error, refetch } = useQuery({
  queryKey: ['todos', { status: 'active' }],
  queryFn: fetchActiveTodos,
  staleTime: 1000 * 60 * 5, // 5 min fresh
  gcTime: 1000 * 60 * 30,   // 30 min garbage collection
});

// React 19 Suspense Query (data is guaranteed non-null!)
const { data: user } = useSuspenseQuery({
  queryKey: ['user', userId],
  queryFn: () => fetchUser(userId),
});
```

---

## 2. Mutations & Invalidation

```tsx
import { useMutation, useQueryClient } from '@tanstack/react-query';

const queryClient = useQueryClient();

const createTodoMutation = useMutation({
  mutationFn: (newTodoText: string) => api.post('/todos', { text: newTodoText }),
  onSuccess: () => {
    // Invalidate and refetch todos
    queryClient.invalidateQueries({ queryKey: ['todos'] });
  },
  onError: (error) => {
    console.error('Failed to create:', error);
  },
});

// Trigger
createTodoMutation.mutate('Buy milk');
```

---

## 3. Query Key Factory Pattern

```ts
export const todoKeys = {
  all: ['todos'] as const,
  lists: () => [...todoKeys.all, 'list'] as const,
  list: (filters: string) => [...todoKeys.lists(), { filters }] as const,
  details: () => [...todoKeys.all, 'detail'] as const,
  detail: (id: string) => [...todoKeys.details(), id] as const,
};

// Usage:
// queryKey: todoKeys.detail(id)
// queryClient.invalidateQueries({ queryKey: todoKeys.lists() })
```
