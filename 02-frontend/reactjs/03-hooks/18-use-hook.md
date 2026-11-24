# React use: Reading Promises & Context Conditionally in React 19

- **Topic**: `03-hooks`
- **React version**: React 19+
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Core React 19 API; breaks traditional hook constraints by permitting execution inside conditional blocks (`if`) and loops.

---

## 1. Concept Overview

Historically, all React hooks (`useState`, `useEffect`, `useContext`) have obeyed the strict **Rules of Hooks**: they must be called unconditionally at the top level of a component and never inside `if` statements, loops, or nested functions.

In React 19, the new `use` API is introduced. While technically an API rather than a traditional hook, it allows components to read the value of a resource—specifically **Promises** and **React Contexts**.

Crucially, **`use` can be called conditionally** inside `if` branches and loops!

```
Traditional useContext vs React 19 use():
┌──────────────────────────────────────┐     ┌──────────────────────────────────────┐
│ useContext(ThemeContext)             │     │ use(ThemeContext)                    │
│ ❌ Cannot be inside `if` statement   │     │ ✅ CAN be called inside `if` block   │
│ ❌ Must be top-level unconditional   │     │ ✅ Evaluates only when condition met │
└──────────────────────────────────────┘     └──────────────────────────────────────┘

Reading Promises:
┌──────────────────────────────────────┐
│ const data = use(fetchPromise);      │
│ ──► If Promise is pending: Suspends! │ ──► Nearest <Suspense fallback={...}>
│ ──► If Promise resolves: Returns data│
│ ──► If Promise rejects: Throws error │ ──► Nearest <ErrorBoundary>
└──────────────────────────────────────┘
```

### When to Use
- **Reading Context Conditionally**: When a component only needs a Context under certain props or conditional paths.
- **Unwrapping Promises with `<Suspense>`**: Reading async data passed from Server Components or cached data loaders directly during render without `useEffect` or `useState`.
- **Server-to-Client Promise Streaming**: Passing an unresolved Promise from a Server Component to a Client Component as a prop, unwrapping it with `use(promise)`.

### When NOT to Use
- **Do NOT create Promises directly inside the render phase**: Calling `use(fetch(...))` on every render creates a new un-cached Promise, causing an infinite suspension loop. Promises passed to `use` must be cached (via a cache library, module scope, or Server Component).
- **Do NOT use in arbitrary non-React functions**: `use` must still be called inside a React Component or a Custom Hook.

---

## 2. Key Mechanics & Architecture

### API Signature
```ts
const value = use<T>(resource: Promise<T> | React.Context<T>): T;
```

1. **When passed a Promise**:
   - If the Promise is **pending**, React suspends rendering and renders the nearest `<Suspense>` fallback.
   - If the Promise **resolves**, React resumes rendering and returns the resolved value `T`.
   - If the Promise **rejects**, React throws the rejection error to the nearest `<ErrorBoundary>`.
2. **When passed a Context**:
   - Behaves identically to `useContext(Context)`, returning the current context value.
   - Unlike `useContext`, it can be invoked conditionally after early returns or inside `if` statements.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | Render phase (synchronous unwrapping) |
| **Conditional Support** | Allowed inside `if` statements and loops |
| **Suspense Integration** | Suspends component automatically when promise is pending |
| **Error Handling** | Bubbles promise rejections to the nearest Error Boundary |
| **Promise Caching Requirement** | The Promise MUST be cached across renders to prevent infinite suspension loops |
| **React Compiler Impact** | Recognized as a suspension boundary; compiler preserves caching semantics |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Reading React Context conditionally inside an `if` block.

```tsx
import React, { createContext, use } from 'react';

const AdminSettingsContext = createContext<{ debugMode: boolean } | null>(null);

export function FeaturePanel({ isAdmin }: { isAdmin: boolean }) {
  // Traditional useContext cannot be placed inside this if-block!
  // React 19 use() handles conditional context cleanly:
  if (isAdmin) {
    const adminSettings = use(AdminSettingsContext);
    return (
      <div className="p-3 bg-red-50 border border-red-200 rounded text-xs text-red-800">
        Admin Mode Active: Debug = {adminSettings?.debugMode ? 'TRUE' : 'FALSE'}
      </div>
    );
  }

  return <div className="text-sm text-zinc-500">Standard User View</div>;
}
```
Output:
```
// If isAdmin is false: renders "Standard User View" without reading AdminSettingsContext.
// If isAdmin is true: reads context and renders debug status.
```

### Example 2: Production / Real-World Integration
Unwrapping a cached data-fetching Promise wrapped in `<Suspense>` and `<ErrorBoundary>`.

```tsx
import React, { use, Suspense } from 'react';

interface UserData {
  id: string;
  name: string;
  role: string;
}

// Simulated module-level promise cache
const userPromiseCache = new Map<string, Promise<UserData>>();

function getUserPromise(userId: string): Promise<UserData> {
  if (!userPromiseCache.has(userId)) {
    const promise = fetch(`/api/users/${userId}`).then((res) => {
      if (!res.ok) throw new Error(`User fetch failed: ${res.statusText}`);
      return res.json();
    });
    userPromiseCache.set(userId, promise);
  }
  return userPromiseCache.get(userId)!;
}

function UserCard({ userPromise }: { userPromise: Promise<UserData> }) {
  // Unwraps promise directly during render!
  const user = use(userPromise);

  return (
    <div className="p-4 border rounded-xl shadow-sm bg-white">
      <h3 className="font-bold text-zinc-900">{user.name}</h3>
      <p className="text-xs text-zinc-500">{user.role}</p>
    </div>
  );
}

export function UserProfileScreen({ userId }: { userId: string }) {
  const promise = getUserPromise(userId);

  return (
    <Suspense
      fallback={
        <div className="p-4 border rounded-xl animate-pulse bg-zinc-50 text-xs text-zinc-400">
          Loading profile...
        </div>
      }
    >
      <UserCard userPromise={promise} />
    </Suspense>
  );
}
```
Output:
```
// Displays pulse skeleton while promise is pending; renders UserCard as soon as promise resolves.
```

### Example 3: Edge Case / Server-to-Client Promise Streaming
Passing an unresolved promise from a Server Component to an interactive Client Component.

```tsx
// Client Component ('use client')
'use client';

import React, { use, Suspense, useState } from 'react';

interface AnalyticsData {
  views: number;
  conversion: number;
}

export function AnalyticsViewer({
  analyticsPromise,
}: {
  analyticsPromise: Promise<AnalyticsData>;
}) {
  return (
    <Suspense fallback={<div className="text-xs text-zinc-400">Streaming analytics metrics...</div>}>
      <AnalyticsDetails promise={analyticsPromise} />
    </Suspense>
  );
}

function AnalyticsDetails({ promise }: { promise: Promise<AnalyticsData> }) {
  const data = use(promise);
  const [multiplier, setMultiplier] = useState(1);

  return (
    <div className="p-4 border rounded-lg bg-zinc-950 text-white font-mono text-xs">
      <div>PAGE VIEWS: {data.views * multiplier}</div>
      <div>CONVERSION: {(data.conversion * 100).toFixed(2)}%</div>
      <button
        type="button"
        onClick={() => setMultiplier((m) => m + 1)}
        className="mt-3 px-2 py-1 bg-zinc-800 text-zinc-200 rounded text-[11px]"
      >
        Simulate 2x Traffic
      </button>
    </div>
  );
}
```
Output:
```
// Client suspends until Server Component streaming promise finishes;
// then hydrates as a fully interactive stateful widget.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Creating the Promise inline inside the render phase.

```tsx
// ❌ CRITICAL BUG: Creating an uncached Promise inside the render phase!
export function InfiniteSuspension({ id }: { id: string }) {
  // ⚠️ BUG: On every render pass, fetch() creates a BRAND NEW Promise instance!
  // React suspends, the promise resolves, React re-renders, creates ANOTHER promise,
  // suspends again -> INFINITE SUSPENSION LOOP!
  const data = use(fetch(`/api/items/${id}`).then((r) => r.json()));

  return <div>{data.title}</div>;
}

// ✅ REMEDIATION: Cache the promise outside the render phase or pass it as a prop!
const promiseCache = new Map<string, Promise<any>>();
function getCachedItem(id: string) {
  if (!promiseCache.has(id)) {
    promiseCache.set(id, fetch(`/api/items/${id}`).then((r) => r.json()));
  }
  return promiseCache.get(id)!;
}

export function WorkingSuspension({ id }: { id: string }) {
  const data = use(getCachedItem(id));
  return <div>{data.title}</div>;
}
```
Output:
```
// Component renders cleanly with single network fetch and stable cache.
```

### Example 5: Idiomatic Modern React 19 Pattern
Combining conditional Context reading and conditional Promise unwrapping based on user configuration.

```tsx
import React, { createContext, use, Suspense } from 'react';

const ConfigContext = createContext<{ enableLiveMetrics: boolean }>({ enableLiveMetrics: false });

async function fetchLiveMetrics(): Promise<{ status: string; uptime: number }> {
  await new Promise((r) => setTimeout(r, 800));
  return { status: 'OPTIMAL', uptime: 99.98 };
}

// Cached promise
const metricsPromise = fetchLiveMetrics();

function MetricsDisplay() {
  const config = use(ConfigContext);

  // Conditional Promise unwrapping based on Context value!
  if (!config.enableLiveMetrics) {
    return <div className="text-xs text-zinc-400">Metrics are disabled in settings.</div>;
  }

  const metrics = use(metricsPromise);

  return (
    <div className="p-2 bg-emerald-50 text-emerald-800 rounded text-xs font-mono">
      STATUS: {metrics.status} | UPTIME: {metrics.uptime}%
    </div>
  );
}

export function DashboardWidget() {
  return (
    <Suspense fallback={<div className="text-xs text-zinc-400">Loading metrics...</div>}>
      <MetricsDisplay />
    </Suspense>
  );
}
```
Output:
```
// If metrics enabled: suspends, loads, and displays green status pill.
// If metrics disabled: renders disabled note immediately without reading the promise.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Creating Promises inline inside render (`use(fetch())`)** | New promise instance created every render pass, triggering an infinite suspension loop. | Cache promises in a Map, pass from Server Components, or use TanStack Query. |
| **Using `use(promise)` without a `<Suspense>` boundary** | Unhandled suspension throws a fatal runtime exception in React. | Always wrap components that call `use(promise)` with `<Suspense>`. |
| **Using `use(promise)` without an `<ErrorBoundary>`** | If the promise rejects, the entire application tree crashes unhandled. | Always pair `<Suspense>` with an `<ErrorBoundary>`. |
| **Calling `use` in a regular JavaScript function** | `use` requires a React Component or Hook execution context. | Only invoke `use` inside components or custom hooks. |
| **Replacing all `useState` with `use(promise)` for client state** | `use` is for reading static or server-streamed resources, not mutable local state. | Use `useState` or `useReducer` for interactive local state. |
| **Calling `use` inside `try/catch` blocks** | React relies on throwing Promises to trigger Suspense; catching it aborts Suspense. | Do not wrap `use(promise)` in a `try/catch`; let Suspense handle suspension. |

---

## 5. React Internals: Beneath the Abstraction

Inside `ReactFiberHooks.js`:
1. When `use(resource)` is called with a Context:
   - React reads the Fiber's current dependency list and registers the context dependency directly, identical to `readContext`.
2. When `use(resource)` is called with a Promise:
   - React checks if the Promise has already been instrumented with status fields (`promise.status`, `promise.value`, `promise.reason`).
   - **If status is `'fulfilled'`**: React returns `promise.value` immediately (synchronous fast path).
   - **If status is `'rejected'`**: React throws `promise.reason` immediately.
   - **If status is `'pending'` (or uninstrumented)**: React attaches `.then()` handlers to mutate the promise status upon settlement and then **throws the Promise itself**.
3. React's work loop catches the thrown Promise, pauses execution on the current fiber, unwinds to the nearest ancestor `<Suspense>` boundary, and renders its fallback.
4. When the thrown Promise resolves, React re-schedules the suspended fiber and renders it again.

---

## 6. Performance, Bundling & React Compiler

- **Zero Client Overhead**: Replaces hundreds of lines of `useEffect` + `useState` + cancellation token boilerplates.
- **Concurrent Streaming**: Enables streaming HTML responses from the server directly into Client Component Suspense boundaries.
- **React Compiler**: The React Compiler recognizes `use()` as an unwrapping boundary and avoids re-running expensive computations until the resource resolves.

---

## 7. Accessibility (a11y) & Usability

- **Suspense Fallback A11y**: Ensure `<Suspense fallback={...}>` components include `aria-busy="true"` and an accessible loading announcement:
  ```html
  <div role="status" aria-busy="true" class="sr-only">Loading dynamic content...</div>
  ```
- **Focus Preservation**: When a component transitions from a suspended fallback to resolved content, ensure keyboard focus is not lost or reset to `document.body`.

---

## 8. Testing & Verification Patterns

Testing `use(promise)` with Vitest and `@testing-library/react`:

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React, { Suspense } from 'react';
import { UserProfileScreen } from './UserProfileScreen';

describe('use(promise) - UserProfileScreen', () => {
  it('displays loading fallback then resolves user details', async () => {
    render(<UserProfileScreen userId="u100" />);

    // Fallback is rendered initially
    expect(screen.getByText(/loading profile.../i)).toBeInTheDocument();

    // After mock resolution, user name appears
    expect(await screen.findByText('Alex Mercer')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Hooks could never be called conditionally. Promises required `useEffect` or external suspense libraries. | Refactor conditional context reads to `use(Context)`. |
| **React Canary** | Previewed as `use` RFC. | Finalized in React 19 stable. |
| **React 19** | Standardized `use()` for both Promises and Contexts. | Import `{ use } from 'react'`. |

---

## 10. Security & Hardening

1. **Server-Client Promise Serialization**: When passing promises from Server Components to Client Components, ensure the resolved payload does not serialize sensitive server secrets, unhashed passwords, or internal API tokens.
2. **Error Boundary Information Leakage**: Sanitize rejection messages in Error Boundaries to avoid exposing database connection strings or stack traces to end users.

---

## 11. Real-World Case Study

### Multi-Tenant Feature Flag & Settings Hydration

An enterprise dashboard reads tenant configuration from context and then lazily fetches localized regulatory policies only if the tenant resides in the EU or UK.

```tsx
import React, { createContext, use, Suspense } from 'react';

interface TenantConfig {
  region: 'US' | 'EU' | 'APAC';
}

const TenantContext = createContext<TenantConfig>({ region: 'US' });

const policyPromiseCache = new Map<string, Promise<string[]>>();
function getRegionPolicies(region: string): Promise<string[]> {
  if (!policyPromiseCache.has(region)) {
    policyPromiseCache.set(
      region,
      new Promise((res) => setTimeout(() => res(['GDPR-2016', 'EU-AI-Act']), 500))
    );
  }
  return policyPromiseCache.get(region)!;
}

function ComplianceRules() {
  const tenant = use(TenantContext);

  // Conditional early return: Non-EU tenants skip fetching entirely!
  if (tenant.region !== 'EU') {
    return <p className="text-xs text-zinc-500">Standard global policies apply.</p>;
  }

  // Unwraps promise conditionally only for EU tenants
  const policies = use(getRegionPolicies('EU'));

  return (
    <div className="p-3 bg-blue-50 text-blue-900 border rounded text-xs">
      <h4 className="font-bold mb-1">Mandatory Compliance Frameworks:</h4>
      <ul className="list-disc pl-4">
        {policies.map((p) => (
          <li key={p}>{p}</li>
        ))}
      </ul>
    </div>
  );
}

export function TenantComplianceWidget() {
  return (
    <Suspense fallback={<div className="text-xs text-zinc-400">Evaluating regional mandates...</div>}>
      <ComplianceRules />
    </Suspense>
  );
}
```

---

## 12. Interview Q&A

<details>
<summary>1. Why can `use()` be called conditionally inside `if` statements, while `useState` and `useEffect` cannot?</summary>

Traditional hooks like `useState` and `useEffect` rely on a linked list attached to the Fiber node. React traverses this linked list in the exact order the hooks were called during the previous render. If a hook call is skipped inside an `if` statement, all subsequent hook pointers misalign, corrupting component state.
`use()` does not store state in the hook linked list. When passed a Context, it queries React's context tree directly. When passed a Promise, it inspects the Promise's status and throws if pending. Neither operation depends on fixed linked-list ordering.
</details>

<details>
<summary>2. What happens if you call `use(promise)` with an uncached Promise created inside the component body?</summary>

It causes an infinite re-render / suspension loop. Every time the component renders, a new Promise instance is created. `use()` suspends. When the promise resolves, React re-renders the component. On re-render, a *new* promise is created, causing React to suspend again, looping indefinitely until memory or rate limits are exhausted. Promises passed to `use()` must be cached outside the render pass.
</details>

<details>
<summary>3. Can `use()` be called in regular helper functions?</summary>

No. `use()` must still be called inside a React Component or a Custom Hook. React needs the active component's Fiber context to register Context subscriptions or find the nearest `<Suspense>` boundary when suspending.
</details>

<details>
<summary>4. What is the difference between `use(Context)` and `useContext(Context)`?</summary>

Functionally they return the same context value. However:
- `useContext(Context)` must be called unconditionally at the top level of the component.
- `use(Context)` can be called conditionally inside `if` statements, after early returns, or inside loops.
</details>

<details>
<summary>5. How does `use(promise)` interact with Error Boundaries?</summary>

If the Promise passed to `use(promise)` rejects, React treats the rejection reason as an unhandled error during the render phase and bubbles it up to the nearest `<ErrorBoundary>`, invoking `componentDidCatch` or `getDerivedStateFromError`.
</details>

<details>
<summary>6. Why shouldn't you wrap `use(promise)` inside a JavaScript `try/catch` block?</summary>

React implements `<Suspense>` by having `use()` throw the pending Promise. If you wrap `use(promise)` in a `try/catch` block, your `catch` handler will intercept the thrown Promise and prevent React from discovering the suspension, breaking the `<Suspense>` fallback mechanism.
</details>

<details>
<summary>7. How does `use()` support Server-to-Client streaming in React 19?</summary>

A React Server Component can initiate an asynchronous operation (e.g. database query) and pass the resulting pending Promise as a prop to a Client Component. The server streams the initial HTML immediately with a Suspense fallback. When the Promise resolves on the server, React streams the resolved data over the wire, and the Client Component's `use(promise)` unwraps it seamlessly.
</details>

<details>
<summary>8. Does `use()` replace data fetching libraries like TanStack Query?</summary>

No. `use()` is a low-level primitive for unwrapping promises within Suspense. It does not provide caching strategies, background refetching on window focus, garbage collection, query invalidation, or retry logic. Libraries like TanStack Query v5 use `use()` internally for their `useSuspenseQuery` implementations.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Conditional Theme Reader</summary>

**Specification**: Component that reads `ThemeContext` only if `overrideTheme` prop is `true`, otherwise uses `'system'`.

```tsx
import React, { createContext, use } from 'react';

const ThemeContext = createContext('dark');

export function DynamicThemedBox({ overrideTheme }: { overrideTheme: boolean }) {
  let theme = 'system';

  if (overrideTheme) {
    theme = use(ThemeContext);
  }

  return (
    <div className={`p-4 border rounded ${theme === 'dark' ? 'bg-zinc-900 text-white' : 'bg-white text-black'}`}>
      Active mode: {theme}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Cached String Message Loader</summary>

**Specification**: Render a message fetched from a cached Promise with Suspense fallback.

```tsx
import React, { use, Suspense } from 'react';

const messagePromise = new Promise<string>((res) => setTimeout(() => res('Welcome to React 19!'), 500));

function MessageText() {
  const msg = use(messagePromise);
  return <p className="font-semibold text-sm">{msg}</p>;
}

export function MessageWidget() {
  return (
    <Suspense fallback={<p className="text-xs text-zinc-400">Loading announcement...</p>}>
      <MessageText />
    </Suspense>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): User Role Permission Gate</summary>

**Specification**: Read a `PermissionsContext` conditionally only if user is logged in, avoiding context overhead for anonymous guests.

```tsx
import React, { createContext, use } from 'react';

const PermissionsContext = createContext<string[]>([]);

export function PermissionGate({
  isLoggedIn,
  requiredPermission,
  children,
}: {
  isLoggedIn: boolean;
  requiredPermission: string;
  children: React.ReactNode;
}) {
  if (!isLoggedIn) {
    return <div className="text-xs text-zinc-400">Please sign in to view.</div>;
  }

  const permissions = use(PermissionsContext);
  if (!permissions.includes(requiredPermission)) {
    return <div className="text-xs text-red-500">Access Denied: Missing {requiredPermission}</div>;
  }

  return <>{children}</>;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Simple In-Memory Promise Cache Helper</summary>

**Specification**: Implement a generic `createResourceCache` utility that guarantees promises passed to `use` are strictly cached per key.

```tsx
import { use } from 'react';

export function createResourceCache<K, V>(fetcher: (key: K) => Promise<V>) {
  const cache = new Map<K, Promise<V>>();

  return {
    read: (key: K): V => {
      if (!cache.has(key)) {
        cache.set(key, fetcher(key));
      }
      return use(cache.get(key)!);
    },
    clear: () => cache.clear(),
  };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Multi-Resource Parallel Suspense Coordinator</summary>

**Specification**: Component that reads multiple independent cached promises conditionally using `use()` within a single Suspense boundary.

```tsx
import React, { use, Suspense } from 'react';

const profilePromise = new Promise<{ name: string }>((r) => setTimeout(() => r({ name: 'Jordan' }), 400));
const notificationsPromise = new Promise<string[]>((r) => setTimeout(() => r(['New message', 'Invoice ready']), 600));

function UserOverview({ showNotifications }: { showNotifications: boolean }) {
  const profile = use(profilePromise);

  let notifications: string[] = [];
  if (showNotifications) {
    notifications = use(notificationsPromise);
  }

  return (
    <div className="p-4 border rounded max-w-sm">
      <h3 className="font-bold">Welcome, {profile.name}</h3>
      {showNotifications && (
        <ul className="text-xs mt-2 list-disc pl-4 text-zinc-600">
          {notifications.map((n, i) => (
            <li key={i}>{n}</li>
          ))}
        </ul>
      )}
    </div>
  );
}

export function OverviewDashboard() {
  return (
    <Suspense fallback={<div className="text-xs text-zinc-400">Gathering user data...</div>}>
      <UserOverview showNotifications={true} />
    </Suspense>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Resilient Fallback Boundary with Promise Retry</summary>

**Specification**: Build an interactive component that catches promise rejections from `use()` with a custom error boundary and provides a "Retry" button that invalidates the cached promise.

```tsx
import React, { Component, use, Suspense, useState, type ReactNode } from 'react';

class CustomErrorBoundary extends Component<{ fallback: (retry: () => void) => ReactNode; children: ReactNode }, { hasError: boolean }> {
  state = { hasError: false };

  static getDerivedStateFromError() {
    return { hasError: true };
  }

  retry = () => {
    this.setState({ hasError: false });
  };

  render() {
    if (this.state.hasError) {
      return this.props.fallback(this.retry);
    }
    return this.props.children;
  }
}

let attempt = 0;
let promiseRef: Promise<string>;

function getFailablePromise(): Promise<string> {
  attempt++;
  if (attempt === 1) {
    return Promise.reject(new Error('Initial connection failed'));
  }
  return Promise.resolve('Connected successfully on attempt ' + attempt);
}

function ResilientData({ version }: { version: number }) {
  const data = use(promiseRef);
  return <div className="text-xs text-green-700 font-bold">{data}</div>;
}

export function ResilientContainer() {
  const [version, setVersion] = useState(0);

  if (!promiseRef || version > 0) {
    promiseRef = getFailablePromise();
  }

  return (
    <CustomErrorBoundary
      fallback={(retry) => (
        <div className="p-3 border border-red-200 bg-red-50 rounded text-xs text-red-700">
          <p>Failed to load data.</p>
          <button
            type="button"
            onClick={() => {
              setVersion((v) => v + 1);
              retry();
            }}
            className="mt-2 bg-red-600 text-white px-2 py-1 rounded text-[11px]"
          >
            Retry Connection
          </button>
        </div>
      )}
    >
      <Suspense fallback={<div className="text-xs text-zinc-400">Connecting...</div>}>
        <ResilientData version={version} />
      </Suspense>
    </CustomErrorBoundary>
  );
}
```
</details>
