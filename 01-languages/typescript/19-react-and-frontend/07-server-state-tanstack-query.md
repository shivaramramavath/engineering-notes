# Server State Management with TanStack Query v5 in TypeScript

> Typing TanStack Query v5 in TypeScript: useQuery and useMutation generic inference, Query Key Factories, queryOptions helper, infinite queries, and optimistic updates.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [12-async-and-iteration/01-promises.md](../12-async-and-iteration/01-promises.md), [16-type-safe-apis/00-api-contracts.md](../16-type-safe-apis/00-api-contracts.md) |
| **TypeScript Baseline** | TS 5.0+, TanStack Query v5 |
| **Tested tsconfig** | `"strict": true, "jsx": "react-jsx", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In client applications, **Server State** (data residing on a remote server that must be fetched, cached, and synchronized) behaves fundamentally differently from local client state (UI state like open modals or selected tabs). Trying to manage server state inside Redux or standard `useState` leads to massive boilerplate: manual loading spinners, duplicate network requests, stale data bugs, and missing retry logic.

**TanStack Query (React Query v5)** handles fetching, caching, synchronizing, and updating server state. In TypeScript, TanStack Query v5 represents a massive leap forward in type safety:
- **Automatic Type Inference**: The return type of `useQuery` is inferred directly from the `queryFn` Promise return type.
- **`queryOptions` Factory Helper**: Allows defining query configuration once and reusing it across `useQuery`, `prefetchQuery`, and route loaders with 100% type preservation.
- **Strict Error Typing**: Configurable `TError` defaulting to `Error` rather than `unknown`.

```
+-----------------------------------------------------------+
|              TanStack Query v5 Type Inference             |
|                                                           |
|  const userQuery = queryOptions({                         |
|    queryKey: ['users', id] as const,                      |
|    queryFn: () => fetchUser(id) // Promise<UserDTO>       |
|  });                                                      |
|                                                           |
|  const { data } = useQuery(userQuery);                    |
|  // Inferred data: UserDTO | undefined                    |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. The `queryOptions` Helper (v5 Feature)
In TanStack Query v5, `queryOptions` bundles the `queryKey` and `queryFn` together into a typed, reusable query definition:
```typescript
import { queryOptions, useQuery } from '@tanstack/react-query';

export const userQueryOptions = (id: string) =>
  queryOptions({
    queryKey: ['users', id] as const,
    queryFn: async () => {
      const res = await fetch(`/api/users/${id}`);
      return res.json() as Promise<UserDTO>;
    }
  });

// Consuming in component:
const { data, isPending, error } = useQuery(userQueryOptions('123'));
```

### 2. Query Key Factories
Query keys must be tuples. A Query Key Factory centralizes key generation to ensure safe cache invalidation across the app:
```typescript
export const usersKeys = {
  all: ['users'] as const,
  lists: () => [...usersKeys.all, 'list'] as const,
  detail: (id: string) => [...usersKeys.all, 'detail', id] as const
};
```

### 3. Typing `useMutation`
Mutations require typing: `<TData, TError, TVariables, TContext>`:
```typescript
const mutation = useMutation<UserDTO, Error, UpdateUserInput>({
  mutationFn: (input) => api.updateUser(input)
});
```

---

## 3. How It Works Internally

Inside TanStack Query's type definitions:
```typescript
function useQuery<TQueryFnData = unknown, TError = DefaultError, TData = TQueryFnData, TQueryKey extends QueryKey = QueryKey>(
  options: UseQueryOptions<TQueryFnData, TError, TData, TQueryKey>
): UseQueryResult<TData, TError>;
```
When `queryOptions` is passed, TypeScript infers `TQueryFnData` from `ReturnType<typeof queryFn>`. If you supply a `select: (data) => data.name` transformation function, `TData` is inferred from the return type of `select`.

---

## 4. Syntax & API Reference

### Complete Mutation with Optimistic Updates Typing
```typescript
import { useMutation, useQueryClient } from '@tanstack/react-query';

interface MutationContext {
  previousTodos?: Todo[];
}

const mutation = useMutation<Todo, Error, NewTodo, MutationContext>({
  mutationFn: (newTodo) => api.createTodo(newTodo),
  onMutate: async (newTodo) => {
    await queryClient.cancelQueries({ queryKey: ['todos'] });
    const previousTodos = queryClient.getQueryData<Todo[]>(['todos']);
    queryClient.setQueryData<Todo[]>(['todos'], (old = []) => [...old, { id: 'temp', ...newTodo }]);
    return { previousTodos };
  },
  onError: (err, newTodo, context) => {
    if (context?.previousTodos) {
      queryClient.setQueryData(['todos'], context.previousTodos);
    }
  },
  onSettled: () => {
    queryClient.invalidateQueries({ queryKey: ['todos'] });
  }
});
```

---

## 5. Practical Examples

### Example 1: Basic Inferred `useQuery` with Typed Fetch
```typescript
import { useQuery } from '@tanstack/react-query';

interface Post { id: number; title: string; body: string; }

async function fetchPosts(): Promise<Post[]> {
  const res = await fetch('/api/posts');
  if (!res.ok) throw new Error('Network error');
  return res.json();
}

export function PostList() {
  const { data: posts, isPending, error } = useQuery({
    queryKey: ['posts'],
    queryFn: fetchPosts
  });

  if (isPending) return <div>Loading posts...</div>;
  if (error) return <div>Error: {error.message}</div>;

  return <ul>{posts.map((p) => <li key={p.id}>{p.title}</li>)}</ul>;
}
```

### Example 2: Real-World Query Key Factory with Invalidation
```typescript
import { useQueryClient } from '@tanstack/react-query';

export const articleKeys = {
  all: ['articles'] as const,
  list: (filters: { tag?: string }) => [...articleKeys.all, 'list', filters] as const,
  detail: (slug: string) => [...articleKeys.all, 'detail', slug] as const
};

export function useInvalidateArticles() {
  const queryClient = useQueryClient();
  return {
    invalidateAll: () => queryClient.invalidateQueries({ queryKey: articleKeys.all }),
    invalidateDetail: (slug: string) =>
      queryClient.invalidateQueries({ queryKey: articleKeys.detail(slug) })
  };
}
```

### Example 3: Edge Case - `select` Option Type Transformation
```typescript
export function useUserNames() {
  return useQuery({
    queryKey: ['users'],
    queryFn: async (): Promise<{ id: string; name: string }[]> => {
      const res = await fetch('/api/users');
      return res.json();
    },
    // select transforms { id, name }[] into string[]
    select: (users) => users.map((u) => u.name)
  });
  // data is inferred as string[] | undefined, NOT { id, name }[]!
}
```

### Example 4: Failure Case - Missing Variables on Mutation Trigger
```typescript
interface UpdateUserPayload { id: string; name: string; }

declare const updateMutation: ReturnType<typeof useMutation<void, Error, UpdateUserPayload>>;

// @ts-expect-error TS2554: Expected 1 arguments, but got 0. Missing 'UpdateUserPayload'.
updateMutation.mutate();
```

### Example 5: Idiomatic Infinite Query Typing
```typescript
import { useInfiniteQuery } from '@tanstack/react-query';

interface PageResult { items: string[]; nextCursor?: number; }

export function useInfiniteList() {
  return useInfiniteQuery<PageResult, Error>({
    queryKey: ['infinite-list'],
    queryFn: async ({ pageParam = 0 }) => {
      const res = await fetch(`/api/items?cursor=${pageParam}`);
      return res.json();
    },
    initialPageParam: 0,
    getNextPageParam: (lastPage) => lastPage.nextCursor ?? undefined
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`data` is `undefined` When Loading**:
   Until the query succeeds, `data` is typed as `TData | undefined`. Always guard with `if (isPending)` or `if (!data)` before accessing properties, or supply `placeholderData` / `initialData`.
2. **Missing `as const` on Query Keys**:
   Writing `queryKey: ['users', id]` infers `string[]`. This allows any string array to match, which weakens type safety in key factories. Always append `as const`: `['users', id] as const`.
3. **`TError` in React Query v5**:
   In v5, `TError` defaults to `Error` (in v4 it defaulted to `unknown`). If your API returns custom error objects (e.g. `ApiProblem`), explicitly type `useQuery<Data, ApiProblem>`.
4. **Mutating Cache Data Directly**:
   Never mutate objects retrieved from `queryClient.getQueryData()`. Always treat cached objects as immutable and return new object references when calling `setQueryData`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Omitting `as const` on Keys** | `queryKey: ['user', id]` | `queryKey: ['user', id] as const` | Loses exact tuple positioning |
| **Manual `useQuery<User>` Casting** | `useQuery<User>({ queryFn: () => ... })` | Let `queryFn` return type infer `User` | Overrides inference, hides mismatch |
| **Missing `initialPageParam` (v5)** | Omitting initialPageParam in v5 | Explicit `initialPageParam: 0` | Compiler error in TanStack Query v5 |
| **Ignoring Error Typing** | Assuming `error.status` exists on `Error` | Type custom error or check `instanceof` | Runtime `undefined` access |
| **Direct Cache Mutation** | `data.items.push(newItem)` | `setQueryData(key, old => [...old, newItem])` | Cache fails to notify subscribers |
| **Duplicate Queries in Monorepos** | Declaring options in 5 places | Centralized `queryOptions()` file | Cache key drift across components |

---

## 8. Performance & Type-Checker Complexity

- **Query Key Matching**: Key comparisons are performed by structural equality on tuple elements.
- **Type Checking**: `queryOptions` caches inference signatures, keeping compilation instant even in large apps with 200+ queries.

---

## 9. Compiler Internals

When calling `useQuery(options)`:
TypeScript matches `options` against `UseQueryOptions<TQueryFnData, TError, TData, TQueryKey>`. If `select` is present, the compiler substitutes `TData` with the return type of `select(data: TQueryFnData)`. The return type of `useQuery` sets `data: TData | undefined`.

---

## 10. Best Practices & Design Guidelines

1. **Adopt `queryOptions`**: The single best pattern in TanStack Query v5 for reusability.
2. **Use Query Key Factories**: Centralize cache keys to eliminate invalidation typos.
3. **Keep `queryFn` Pure**: Let TanStack Query manage lifecycle, retries, and cancellation.
4. **Always Return Snapshots in `onMutate`**: Ensure safe rollbacks on mutation failure.

---

## 11. Real-World Case Study

### High-Volume Financial Transaction Dashboard
Managing real-time balance queries and instant optimistic ledger transactions:

```typescript
import { queryOptions, useQuery, useMutation, useQueryClient } from '@tanstack/react-query';

export interface AccountBalance { accountId: string; balanceCents: bigint; }

export const balanceOptions = (accountId: string) =>
  queryOptions({
    queryKey: ['account', accountId, 'balance'] as const,
    queryFn: async (): Promise<AccountBalance> => {
      const res = await fetch(`/api/accounts/${accountId}/balance`);
      const data = await res.json();
      return { accountId: data.accountId, balanceCents: BigInt(data.balanceCents) };
    },
    staleTime: 10000
  });

export function useAccountBalance(accountId: string) {
  return useQuery(balanceOptions(accountId));
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the benefit of the `queryOptions` helper introduced in TanStack Query v5?</summary>

`queryOptions` defines `queryKey` and `queryFn` together in a single type-safe configuration object. It allows this exact query definition to be shared across `useQuery`, `prefetchQuery`, router loaders, and cache utilities while preserving 100% of the inferred data and error types without code duplication.
</details>

<details>
<summary>2. Why should query keys always be defined with `as const`?</summary>

`as const` preserves the exact tuple structure and string literal types (e.g. `readonly ['users', string]`). Without `as const`, TypeScript widens the array to `string[]`, which permits passing arbitrary string arrays and weakens cache invalidation precision.
</details>

<details>
<summary>3. How does the `select` option in `useQuery` alter the inferred `data` type?</summary>

The `select` option transforms the raw data returned by `queryFn`. TypeScript automatically changes the returned `data` property of `useQuery` from `TQueryFnData | undefined` to `ReturnType<typeof select> | undefined`.
</details>

<details>
<summary>4. What are the four generic parameters of `useMutation`?</summary>

`useMutation<TData, TError, TVariables, TContext>`:
- `TData`: Return type of the mutation function.
- `TError`: Type of the error thrown on failure.
- `TVariables`: The arguments passed to `.mutate(variables)`.
- `TContext`: The context object returned by `onMutate` and passed to `onError` and `onSettled` (used for optimistic rollbacks).
</details>

<details>
<summary>5. How does optimistic update rollback work in TanStack Query?</summary>

In `onMutate`, the current cache state is captured via `queryClient.getQueryData()` and returned as context. The optimistic value is set via `setQueryData`. If the mutation fails, `onError(err, vars, context)` receives that context and restores the original cache state.
</details>

<details>
<summary>6. What changed regarding `TError` in TanStack Query v5 compared to v4?</summary>

In v4, `TError` defaulted to `unknown`, requiring manual type narrowing on `error`. In v5, `TError` defaults to `Error`, allowing developers to immediately access `error.message`.
</details>

<details>
<summary>7. How do you handle pagination with `useQuery` without flickering the UI?</summary>

Use `placeholderData: keepPreviousData`. This keeps the data from the previous page visible while the next page is being fetched.
</details>

<details>
<summary>8. How do you abort an in-flight fetch request in `queryFn`?</summary>

TanStack Query provides an `AbortSignal` in the query function context:
`queryFn: ({ signal }) => fetch(url, { signal })`.
If the component unmounts or query becomes stale, TanStack Query automatically calls `signal.abort()`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Typed Query Function</summary>

**Problem:** Type a query function that fetches an array of strings from `/api/tags`.

**Solution:**
```typescript
export async function fetchTags(): Promise<string[]> {
  const res = await fetch('/api/tags');
  if (!res.ok) throw new Error('Failed to fetch tags');
  return res.json();
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Typed Mutation Hook</summary>

**Problem:** Create a `useDeletePost` mutation hook that accepts a `postId: string`.

**Solution:**
```typescript
import { useMutation } from '@tanstack/react-query';

export function useDeletePost() {
  return useMutation({
    mutationFn: async (postId: string) => {
      await fetch(`/api/posts/${postId}`, { method: 'DELETE' });
    }
  });
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Query Key Factory Implementation</summary>

**Problem:** Build a typed query key factory for an `orders` entity supporting lists with status filters and individual order IDs.

**Solution:**
```typescript
export const ordersKeys = {
  all: ['orders'] as const,
  lists: () => [...ordersKeys.all, 'list'] as const,
  list: (status: 'PENDING' | 'SHIPPED') => [...ordersKeys.lists(), { status }] as const,
  detail: (id: string) => [...ordersKeys.all, 'detail', id] as const
};
```
</details>

<details>
<summary>Exercise 4 (Medium): Select Transform Query Options</summary>

**Problem:** Write a `queryOptions` helper that fetches `{ id: string; count: number }[]` and uses `select` to compute total count.

**Solution:**
```typescript
import { queryOptions } from '@tanstack/react-query';

export const totalMetricsQuery = queryOptions({
  queryKey: ['metrics'] as const,
  queryFn: async (): Promise<{ id: string; count: number }[]> => {
    return [{ id: '1', count: 10 }, { id: '2', count: 20 }];
  },
  select: (items) => items.reduce((sum, item) => sum + item.count, 0)
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Optimistic Todo Creator</summary>

**Problem:** Implement an optimistic mutation that adds a todo to the cached `['todos']` array and rolls back on failure.

**Solution:**
```typescript
import { useMutation, useQueryClient } from '@tanstack/react-query';

interface Todo { id: string; title: string; }

export function useOptimisticCreateTodo() {
  const queryClient = useQueryClient();

  return useMutation<Todo, Error, { title: string }, { prev?: Todo[] }>({
    mutationFn: async (vars) => {
      const res = await fetch('/api/todos', { method: 'POST', body: JSON.stringify(vars) });
      return res.json();
    },
    onMutate: async (newTodo) => {
      await queryClient.cancelQueries({ queryKey: ['todos'] });
      const prev = queryClient.getQueryData<Todo[]>(['todos']);
      queryClient.setQueryData<Todo[]>(['todos'], (old = []) => [
        ...old,
        { id: `temp-${Date.now()}`, title: newTodo.title }
      ]);
      return { prev };
    },
    onError: (_, __, ctx) => {
      if (ctx?.prev) queryClient.setQueryData(['todos'], ctx.prev);
    },
    onSettled: () => {
      queryClient.invalidateQueries({ queryKey: ['todos'] });
    }
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Generic Filtered Query Hook</summary>

**Problem:** Write a generic hook `useFilteredQuery<TItem, TFilter>` that binds query keys and options with type safety.

**Solution:**
```typescript
import { useQuery } from '@tanstack/react-query';

export function useFilteredQuery<TItem, TFilter extends object>(
  resource: string,
  filter: TFilter,
  fetcher: (f: TFilter) => Promise<TItem[]>
) {
  return useQuery({
    queryKey: [resource, filter] as const,
    queryFn: () => fetcher(filter)
  });
}
```
</details>
