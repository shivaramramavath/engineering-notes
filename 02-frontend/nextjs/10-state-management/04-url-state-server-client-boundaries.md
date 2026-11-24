# URL State, SSR Hydration & Zustand (`url-state-server-client-boundaries`)

> State management boundaries in Next.js App Router: URL query parameters as single source of truth, SSR-safe Zustand store initialization, and `useOptimistic` state coordination.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **State** | URL SearchParams + Zustand v5 + `useOptimistic` |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The State Triangle in the App Router
State in a modern Next.js application is divided into three distinct tiers:
1. **URL State (Global & Shareable)**: Filters, pagination, active tabs, search terms. Should live in the URL (`?page=2&sort=desc`) so that links can be shared, bookmarked, and restored across browser refreshes.
2. **Server State (Ground Truth Cache)**: Data from the database or external APIs. Managed via Server Components and `"use cache"` or TanStack Query.
3. **Ephemeral Client State (Local UI)**: Modals open/closed, transient drag-and-drop state, text input focus. Managed via `useState` or SSR-safe Zustand stores.

```
       [ URL SearchParams (?q=nextjs) ] ──(Shareable & Refresh-Safe)
                      ▲
                      │
   [ Server State (RSC Cache) ] <───> [ Ephemeral Client State (Zustand / UI) ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Leaking Global Store Across Server Requests
```ts
// store/useStore.ts (Naive - SINGLETON LEAK BUG IN SSR!)
import { create } from 'zustand';

// Dangerous: In Node.js server runtime, this singleton is SHARED across different user requests!
export const useUserStore = create((set) => ({
  user: null,
  setUser: (user: any) => set({ user }),
}));
```

### Idiomatic: Per-Request Store Factory with Context
```tsx
// store/UserStoreProvider.tsx (Idiomatic SSR-Safe Zustand)
'use client';

import { createContext, useContext, useRef } from 'react';
import { createStore, useStore } from 'zustand';

interface UserState {
  user: { id: string; name: string } | null;
}

const createUserStore = (init: UserState) =>
  createStore<UserState>(() => init);

const UserStoreContext = createContext<ReturnType<typeof createUserStore> | null>(null);

export function UserStoreProvider({ init, children }: { init: UserState; children: React.ReactNode }) {
  const storeRef = useRef<ReturnType<typeof createUserStore>>(null);
  if (!storeRef.current) {
    storeRef.current = createUserStore(init); // Fresh store instance per client tree!
  }
  return <UserStoreContext.Provider value={storeRef.current}>{children}</UserStoreContext.Provider>;
}

export function useUserStore<T>(selector: (state: UserState) => T): T {
  const store = useContext(UserStoreContext);
  if (!store) throw new Error('Missing UserStoreProvider');
  return useStore(store, selector);
}
```

---

## 3. Five Progressive Examples

### Example 1: Reading and Setting URL State with `nuqs`
```tsx
'use client';

import { useQueryState, parseAsString } from 'nuqs';

export function SearchFilter() {
  const [search, setSearch] = useQueryState('search', parseAsString.withDefault(''));

  return (
    <input
      type="search"
      value={search}
      onChange={(e) => setSearch(e.target.value)}
      placeholder="Filter items..."
    />
  );
}
```

### Example 2: Server Component Consuming URL State
```tsx
// app/catalog/page.tsx
interface PageProps {
  searchParams: Promise<{ search?: string; page?: string }>;
}

export default async function CatalogPage({ searchParams }: PageProps) {
  const { search = '', page = '1' } = await searchParams;
  const items = await queryCatalog(search, Number(page));

  return <ItemGrid items={items} />;
}
```

### Example 3: Optimistic Mutations with React 19 `useOptimistic`
```tsx
'use client';

import { useOptimistic, startTransition } from 'react';
import { toggleFavoriteAction } from '@/actions/favorites';

export function FavoriteButton({ id, isFav }: { id: string; isFav: boolean }) {
  const [optimisticFav, setOptimisticFav] = useOptimistic(
    isFav,
    (current, update: boolean) => update
  );

  const handleToggle = () => {
    startTransition(async () => {
      setOptimisticFav(!optimisticFav); // Instant paint
      await toggleFavoriteAction(id, !optimisticFav);
    });
  };

  return (
    <button onClick={handleToggle}>
      {optimisticFav ? '★ Favorited' : '☆ Favorite'}
    </button>
  );
}
```

### Example 4: Storing UI Preferences in Cookies
```tsx
// actions/theme.ts
'use server';

import { cookies } from 'next/headers';

export async function setThemePreference(theme: 'light' | 'dark') {
  const cookieJar = await cookies();
  cookieJar.set('theme', theme, { maxAge: 60 * 60 * 24 * 365, path: '/' });
}
```

### Example 5: Hydrating Server Data into Client Store
```tsx
// app/dashboard/layout.tsx
import { UserStoreProvider } from '@/store/UserStoreProvider';
import { verifySession } from '@/dal/session';

export default async function DashboardLayout({ children }: { children: React.ReactNode }) {
  const session = await verifySession();

  return (
    <UserStoreProvider init={{ user: session?.user ?? null }}>
      {children}
    </UserStoreProvider>
  );
}
```

---

## 4. Under the Hood & Internals
In traditional single-page apps, a global JavaScript singleton `create()` store is safe because JavaScript execution is confined to a single user's browser. In Next.js, Server Components execute in a shared Node.js server process. If a global singleton store is written to on the server, **User A's state can leak into User B's render!** Creating stores via a React Context factory ensures store lifetimes are bound to the component tree.

---

## 5. Security & Multi-Tenant State Hygiene
- Never persist authentication tokens or session objects in global client state stores without verifying them on every server mutation.
- Use cookies with `httpOnly: true` and `SameSite: Lax` for authentication, rather than storing JWTs in localStorage or global Zustand stores.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Global Zustand store on server | Shared Node.js memory | Cross-user data leakage in SSR | Use Context-scoped store factory |
| Duplicate URL state in `useState` | State desynchronization | Browser Back button does not update UI | Use URL searchParams as single source of truth |
| Hydration mismatch in theme | Client reads localStorage on mount | React hydration mismatch warning | Read theme from cookie on server |
| Client-side fetch waterfall | Fetching in `useEffect` in client store | Slow page load, layout shifts | Fetch in Server Component and pass to provider |

---

## 7. Performance & Optimization Guide
Prefer URL query parameters for search and filters. When search parameters change, Next.js performs a lightweight partial navigation, re-rendering only the affected server components and streaming the updated RSC payload without resetting surrounding UI layouts.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export type StoreFactory<T> = (initialState: T) => ReturnType<typeof createStore<T>>;
```

---

## 9. Testing & Quality Verification

```tsx
import { render, screen } from '@testing-library/react';
import { UserStoreProvider } from './UserStoreProvider';
import { describe, it, expect } from 'vitest';

describe('UserStoreProvider', () => {
  it('initializes store with server state', () => {
    render(
      <UserStoreProvider init={{ user: { id: 'u1', name: 'Alice' } }}>
        <div>Test Child</div>
      </UserStoreProvider>
    );
    expect(screen.getByText('Test Child')).toBeInTheDocument();
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not serialize massive binary objects or complex state graphs into URL search parameters. Keep URLs concise and readable.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. Why can global singleton stores like Zustand leak data in Next.js SSR?</summary>
In a Node.js server environment, module-level variables are singletons shared across all concurrent HTTP requests. If a module-level store is mutated during SSR, one user's data can leak to another user.
</details>

<details>
<summary>2. How do you create an SSR-safe Zustand store in Next.js?</summary>
By creating a store factory function inside a React Context provider, instantiating the store once per component tree using `useRef`.
</details>

<details>
<summary>3. Why should filter and search state be stored in URL parameters rather than component state?</summary>
URL state is shareable, bookmarkable, preserves browser history (Back/Forward navigation works), and allows the server to prerender the exact filtered view.
</details>

<details>
<summary>4. What is the role of React 19 `useOptimistic`?</summary>
It enables instant UI updates during asynchronous Server Action transitions, automatically reverting to the server ground truth if the action fails.
</details>

<details>
<summary>5. How does `nuqs` differ from native `useSearchParams`?</summary>
`nuqs` provides type-safe parsers, batching, and synchronous local updates with automatic URL synchronization, eliminating manual router push boilerplate.
</details>

<details>
<summary>6. How do you avoid hydration mismatch errors with user theme preferences?</summary>
Store the theme in a cookie (`await cookies()`), read it in `app/layout.tsx` on the server, and inject the theme class directly into the `<html>` tag before sending HTML.
</details>

<details>
<summary>7. What is the difference between Server State and Client State?</summary>
Server State is persisted data owned by the database or external APIs. Client State is ephemeral UI state (e.g. dropdown toggles, modal visibility).
</details>

<details>
<summary>8. Can Server Components access Zustand stores?</summary>
No. Zustand is a client-side state library. Server Components should pass data down via props or React Context providers.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Sync input with URL search parameters</summary>
Create a search bar that updates `?q=` in the browser URL using `nuqs`.
</details>

<details>
<summary>Exercise 2 (Medium): Build an optimistic bookmark toggle</summary>
Implement a bookmark button with `useOptimistic` that toggles state instantly while awaiting a Server Action.
</details>

<details>
<summary>Exercise 3 (Hard): Implement an SSR-safe cart store</summary>
Build an SSR-safe Zustand cart store using Context, pre-populated with cart items fetched on the server.
</details>

---

## 13. Cross-References & Further Reading
- [Components: Server & Client Composition](file:///E:/Learning/engineering-notes/02-frontend/nextjs/03-components/README.md)
- [Official Next.js State Management Guide](https://nextjs.org/docs/app/building-your-application/data-fetching/server-actions-and-mutations#optimistic-updates)
