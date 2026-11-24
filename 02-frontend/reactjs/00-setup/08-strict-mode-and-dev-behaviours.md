# StrictMode and Development Behaviours

- **Topic**: `00-setup`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: StrictMode is purely a development-only tool. It compiles away with zero runtime or bundle impact in production builds.

---

## 1. Concept Overview

`<React.StrictMode>` is a development-only wrapper component that enables a suite of internal runtime checks, deprecation warnings, and behavioral stress tests to help developers write pure, resilient components compatible with Concurrent React.

The most widely discussed and misunderstood feature of StrictMode is **double invocation**:
1. **Components and state initializers render twice**.
2. **Effects (`useEffect`, `useLayoutEffect`) mount, unmount, and immediately re-mount (`Mount -> Cleanup -> Remount`)**.

This intentional behavior is **not a bug**; it is a sanity check designed to catch:
- Impure component renders (mutating variables or objects during render).
- Missing cleanup functions in effects (leaking event listeners, intervals, sockets).
- Incomplete state reset handling when components unmount and remount (essential for Fast Refresh, offscreen caching, and Activity components).

```
Standard Production Lifecycle:
[ Mount Component ] ──> [ Execute useEffect ]

StrictMode Development Lifecycle (React 18 & 19):
[ Mount Component ] ──> [ Execute useEffect ]
          │
          ▼  (React simulates immediate unmount & remount)
[ Run Cleanup ] ──────> [ Re-execute useEffect ]
```

---

## 2. Key Mechanics & Architecture

### What StrictMode Does Under the Hood

| Behavior | In Development (`<StrictMode>`) | In Production (`NODE_ENV === "production"`) |
| :--- | :--- | :--- |
| **Component Render Invocation** | **Called 2x** per update | **Called 1x** per update |
| **`useState(initializerFn)`** | Initializer called **2x** | Initializer called **1x** |
| **`useReducer(reducer, init)`** | Reducer called **2x** | Reducer called **1x** |
| **`useEffect` Execution** | `Mount -> Unmount/Cleanup -> Remount` | `Mount` only |
| **Console Logs** | Second render logs are slightly dimmed in DevTools | Normal logs |
| **Deprecated API Detection** | Warns on legacy context, findDOMNode, string refs | Ignored |

### Why StrictMode Simulates Remounting
In modern and future versions of React (such as the `<Activity>` component or tab switching), React preserves the state of hidden components and remounts them when visible again. If an effect does not implement a proper cleanup function, remounting leaks memory and creates duplicate subscriptions. StrictMode surfaces these bugs on day one.

---

## 3. Real-World Code Examples

### Example 1: Effect Cleanup Bug Caught by StrictMode
```tsx
import { useEffect, useState } from 'react';

// BAD: Missing cleanup creates duplicate listeners!
export function BuggyWindowListener() {
  const [width, setWidth] = useState(window.innerWidth);

  useEffect(() => {
    const handleResize = () => setWidth(window.innerWidth);
    window.addEventListener('resize', handleResize);
    // BUG: Missing return () => window.removeEventListener('resize', handleResize)!
  }, []);

  return <div>Width: {width}px</div>;
}

// GOOD: Clean implementation handles unmount/remount
export function CorrectWindowListener() {
  const [width, setWidth] = useState(window.innerWidth);

  useEffect(() => {
    const handleResize = () => setWidth(window.innerWidth);
    window.addEventListener('resize', handleResize);
    return () => {
      window.removeEventListener('resize', handleResize);
    };
  }, []);

  return <div>Width: {width}px</div>;
}
```
Output:
```
// StrictMode mounts, unmounts, and remounts.
// In BuggyWindowListener: 2 active listeners now exist in window.
// In CorrectWindowListener: 1 active listener cleanly maintained.
```

### Example 2: Data Fetching Race Condition and AbortController
```tsx
import { useEffect, useState } from 'react';

export function UserProfile({ userId }: { userId: string }) {
  const [name, setName] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();

    async function loadData() {
      try {
        const res = await fetch(`/api/users/${userId}`, { signal: controller.signal });
        const data = await res.json();
        setName(data.name);
      } catch (err: any) {
        if (err.name !== 'AbortError') {
          console.error('Fetch failed:', err);
        }
      }
    }

    loadData();

    // Cleanup aborts in-flight network request on unmount
    return () => {
      controller.abort();
    };
  }, [userId]);

  return <div>User: {name ?? 'Loading...'}</div>;
}
```
Output:
```
// In StrictMode: First request aborted instantly; second request completes cleanly.
```

### Example 3: Impure Render Mutation Caught by Double Invocation
```tsx
// BAD: Mutating external array during render
const globalLogs: string[] = [];

export function ImpureComponent({ message }: { message: string }) {
  globalLogs.push(message); // BUG: Side-effect in render!
  return <div>Logs Count: {globalLogs.length}</div>;
}
```
Output in Dev:
```
Logs Count: 2  <-- Double invocation exposes that render pushed twice!
```

### Example 4: Verifying Production Mode Bypasses StrictMode
```tsx
import React, { StrictMode } from 'react';

let renderCount = 0;

export function CountingComponent() {
  renderCount++;
  console.log(`Render count: ${renderCount}`);
  return <div>Rendered</div>;
}

export function App() {
  return (
    <StrictMode>
      <CountingComponent />
    </StrictMode>
  );
}
```
Console in Dev:
```
Render count: 1
Render count: 2
```
Console in Production (`npm run build && npm run preview`):
```
Render count: 1
```

### Example 5: StrictMode and Non-Idempotent Storage Updates
```tsx
import { useState } from 'react';

// BAD: Side effect inside useState initializer!
export function BadStorageInit() {
  const [token] = useState(() => {
    // Side effect: writing to localStorage in an initializer function!
    const t = Math.random().toString();
    localStorage.setItem('auth_token', t);
    return t;
  });
  return <div>Token: {token}</div>;
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Deleting `<StrictMode>` because "useEffect is running twice" | Hides latent subscription leaks, race conditions, and impurity bugs | Fix the missing cleanup function or adopt TanStack Query |
| Using `useRef(false)` to prevent useEffect running twice | Anti-pattern that actively breaks future remounting and offscreen tabs | Implement proper cleanup functions (`abort()`, `removeEventListener`) |
| Performing DOM mutations or API requests directly in render | Violates React component purity invariants | Move all side effects into `useEffect` or React 19 Actions |
| Mutating state objects directly in reducers: `state.items.push(x)` | StrictMode invokes reducers twice; in-place mutations result in duplicate items | Return new immutable copies (`[...state.items, x]`) or use Immer |
| Forgetting to clear timers: `setTimeout` without `clearTimeout` | Leaks timer callbacks after component unmounts | Return `() => clearTimeout(timer)` from effect |
| Assuming StrictMode slows down production performance | StrictMode is stripped completely in production builds | Keep StrictMode enabled across all development environments |

---

## 5. React Internals: Beneath the Abstraction

### Double Invocation Mechanism in the Fiber Reconciler
When `workInProgress.mode & ConcurrentMode` and `workInProgress.mode & StrictLegacyMode` (or `StrictExtendedMode`) are enabled:
1. In `beginWork()`, React executes the functional component body once to construct child fibers.
2. If in development, React discards the newly created child fibers and **immediately invokes the component function a second time**, reconciling from the second invocation.
3. This guarantees that if a component contains impure side effects (e.g. `state.count++`), the state divergence becomes immediately obvious.

---

## 6. Performance, Bundling & React Compiler

- **Zero Production Cost**: In production (`process.env.NODE_ENV === "production"`), `<StrictMode>` evaluates to a simple fragment (`return props.children`), incurring zero DOM nodes and zero overhead.
- **Compiler Compatibility**: The React Compiler relies on component purity. StrictMode serves as the first line of defense to verify your code follows the Rules of React before the compiler processes it.

---

## 7. Accessibility (a11y) & Usability

- **Focus Restoration**: StrictMode remount simulations verify that focus traps (in modals or dropdowns) properly release document focus when unmounted and re-acquire it when remounted.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { StrictMode } from 'react';
import { CorrectWindowListener } from './CorrectWindowListener';

describe('StrictMode Verification', () => {
  it('mounts cleanly under StrictMode without throwing', () => {
    render(
      <StrictMode>
        <CorrectWindowListener />
      </StrictMode>
    );

    expect(screen.getByText(/Width:/)).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration

| React Version | StrictMode Changes |
| :--- | :--- |
| **React 16.3** | StrictMode introduced (warns on legacy lifecycle methods, string refs) |
| **React 18** | Added intentional effect unmount/remount (`Mount -> Cleanup -> Remount`) |
| **React 19** | StrictMode double-invokes Action handlers and verifies Action idempotency |

---

## 10. Security & Hardening

- **Preventing Stale Auth Listeners**: Failing to clean up authentication listeners (`supabase.auth.onAuthStateChange`, `firebase.onAuthStateChanged`) leaves listeners attached to unmounted components, creating memory leaks and unauthorized state leaks.

---

## 11. Real-World Case Study

### Diagnosing a Duplicate Chat Message Bug
A developer reported that sending a message in a chat widget caused two identical messages to be sent to the backend during development.
- **Root Cause**: The developer had put `sendMessageApi(text)` directly inside a `useEffect` with an empty dependency array without an AbortController.
- **Resolution**: StrictMode caught the bug immediately. The developer realized that data mutations belong in user event handlers (or React 19 Actions), not `useEffect`. Moving the call to `onClick` resolved the issue permanently.

---

## 12. Interview Q&A

<details>
<summary>1. Why does React StrictMode run effects twice in development?</summary>
In modern React, components can be mounted, unmounted, and remounted without losing state (e.g. during Fast Refresh, tab switching with Activity, or offscreen caching). StrictMode simulates this exact cycle (Mount -> Cleanup -> Remount) in development to expose missing cleanup functions, dangling subscriptions, and unhandled race conditions early.
</details>

<details>
<summary>2. Does StrictMode affect production builds?</summary>
No. StrictMode is strictly a development-time tool. In production, it does not double-invoke components or effects, adds no extra DOM wrappers, and has zero impact on bundle size or runtime performance.
</details>

<details>
<summary>3. Why is using useRef to prevent an effect from running twice an anti-pattern?</summary>
Using <code>const isMounted = useRef(false)</code> to stop the second run defeats the entire purpose of StrictMode. It guarantees that when your component is legitimately remounted in production (such as navigating back to a cached tab), your effect will fail to run, breaking subscriptions and data loading.
</details>

<details>
<summary>4. What parts of a component are double-invoked in StrictMode?</summary>
1. Function component bodies.
2. <code>useState</code> initializer functions.
3. <code>useReducer</code> reducer functions.
4. <code>useMemo</code> calculation functions.
5. <code>useEffect</code> and <code>useLayoutEffect</code> lifecycles (mount, cleanup, remount).
</details>

<details>
<summary>5. How should data fetching inside useEffect handle StrictMode remounts?</summary>
Effects performing fetch operations should use an <code>AbortController</code> to cancel the in-flight network request during the cleanup phase, or use a boolean flag (<code>let ignore = false; ... return () => { ignore = true; }</code>) to prevent setting state on aborted fetches.
</details>

<details>
<summary>6. How does StrictMode help the React Compiler?</summary>
The React Compiler assumes that component render functions are strictly pure. By double-invoking components in development, StrictMode immediately highlights mutations and impurities that would otherwise cause the compiler to produce incorrect memoized code.
</details>

<details>
<summary>7. Why does console.log appear twice in the browser dev tools?</summary>
Because the component function runs twice in development under StrictMode. Modern React DevTools automatically dim the second set of logs in the console to indicate that they originated from a StrictMode re-invocation.
</details>

<details>
<summary>8. Can you apply StrictMode to only a small part of your app?</summary>
Yes. <code>&lt;StrictMode&gt;</code> can be wrapped around any component subtree (e.g. <code>&lt;StrictMode&gt;&lt;NewFeature /&gt;&lt;/StrictMode&gt;</code>) to audit legacy applications incrementally.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Event Listener Cleanup Guard</summary>

**Specification**: Write a custom hook `useDocumentTitle(title)` that safely updates `document.title` and restores the previous title on unmount.

```tsx
import { useEffect } from 'react';

export function useDocumentTitle(title: string) {
  useEffect(() => {
    const previousTitle = document.title;
    document.title = title;
    return () => {
      document.title = previousTitle;
    };
  }, [title]);
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Safe Interval Hook</summary>

**Specification**: Write a `useInterval(callback, delay)` hook that cleans up its `setInterval` timer on unmount.

```tsx
import { useEffect, useRef } from 'react';

export function useInterval(callback: () => void, delay: number | null) {
  const savedCallback = useRef(callback);

  useEffect(() => {
    savedCallback.current = callback;
  }, [callback]);

  useEffect(() => {
    if (delay === null) return;
    const id = setInterval(() => savedCallback.current(), delay);
    return () => clearInterval(id);
  }, [delay]);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): StrictMode-Proof Abortable Fetch</summary>

**Specification**: Build an effect hook that fetches data using `AbortController` and ensures state is not set if unmounted during flight.

```tsx
import { useState, useEffect } from 'react';

export function useAbortableFetch<T>(url: string) {
  const [data, setData] = useState<T | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();

    fetch(url, { signal: controller.signal })
      .then((res) => res.json())
      .then((json) => setData(json))
      .catch((err) => {
        if (err.name !== 'AbortError') {
          setError(err.message);
        }
      });

    return () => {
      controller.abort();
    };
  }, [url]);

  return { data, error };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): WebSocket Subscription Lifecycle</summary>

**Specification**: Write a hook `useMockSocket(url, onMessage)` that opens a mock connection on mount, closes it on cleanup, and logs mount/unmount events.

```tsx
import { useEffect } from 'react';

export function useMockSocket(url: string, onMessage: (msg: string) => void) {
  useEffect(() => {
    console.log(`Connecting to socket: ${url}`);
    let active = true;

    const timer = setInterval(() => {
      if (active) onMessage(`Ping from ${url}`);
    }, 1000);

    return () => {
      console.log(`Disconnecting socket: ${url}`);
      active = false;
      clearInterval(timer);
    };
  }, [url, onMessage]);
}
```
</details>

<details>
<summary>Exercise 5 (Hard): State Mutation Detector in Dev</summary>

**Specification**: Implement an experimental higher-order component `withImmutabilityCheck(Component)` that freezes props and state in dev mode using `Object.freeze()`.

```tsx
import React from 'react';

export function withImmutabilityCheck<P extends object>(WrappedComponent: React.ComponentType<P>) {
  return function GuardedComponent(props: P) {
    if (process.env.NODE_ENV !== 'production') {
      Object.freeze(props);
    }
    return <WrappedComponent {...props} />;
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Replay Hook Verifier</summary>

**Specification**: Build a test harness hook `useRenderAudit(componentName)` that counts render executions and asserts whether the component is running under StrictMode.

```tsx
import { useRef, useEffect } from 'react';

export function useRenderAudit(name: string) {
  const renders = useRef(0);
  renders.current++;

  useEffect(() => {
    // In StrictMode dev, renders.current will be 2 on first mount!
    console.log(`[${name}] Initial mount completed with ${renders.current} render passes.`);
  }, [name]);

  return renders.current;
}
```
</details>
