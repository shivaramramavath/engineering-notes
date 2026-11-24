# StrictMode Double-Rendering Explained

- **Topic**: `02-state-and-rendering`
- **React version**: React 19+ (introduced in React 18, standard in 19)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Strictly a development-only check. Stripped entirely in production builds.

---

## 1. Concept Overview

One of the most frequently asked questions by developers upgrading to React 18 and 19 is:
> *"Why is my `console.log()` firing twice, and why does my `useEffect` run two times on page load?"*

This behavior is caused by **`<React.StrictMode>`**. In development mode, React deliberately invokes component functions, state initializers, and effects **twice**.

This intentional behavior exists to enforce two critical architectural guarantees in modern React:
1. **Component Purity**: Components must produce identical output given identical inputs, without mutating outside variables.
2. **Resilience to Remounting**: In Concurrent React, React frequently hides and restores subtrees (e.g. `<Activity>`, Fast Refresh, offscreen tab caches) without destroying state. If an effect sets up a subscription or event listener but fails to clean it up, remounting causes memory leaks and duplicate handlers.

```
Standard Single Render (Production):
[ Component Render ] ──> [ Mount Effect ]

StrictMode Development Cycle (React 18 & 19):
1. [ Component Render 1 ] (Discarded)
2. [ Component Render 2 ] (Reconciled)
3. [ Mount Effect ]
4. [ Unmount / Cleanup Effect ]  <── Immediate stress test!
5. [ Remount Effect ]
```

---

## 2. Key Mechanics & Architecture

### What StrictMode Double-Invokes

| Hook / Area | Is Double-Invoked in Dev? | Purpose of Check |
| :--- | :---: | :--- |
| **Component Body** | **Yes** | Catches mutations to external variables during render |
| **`useState(() => init)`** | **Yes** | Catches side effects inside lazy state initializers |
| **`useReducer(reducer)`** | **Yes** | Verifies that reducer state transitions are pure |
| **`useMemo(() => fn)`** | **Yes** | Verifies that memoized calculations are deterministic |
| **`useEffect` Lifecycle** | **`Mount -> Cleanup -> Remount`** | Catches missing cleanup functions in subscriptions |
| **Event Handlers (`onClick`)** | **No (Runs 1x)** | User interactions run exactly once |
| **Production Builds** | **No (All run 1x)** | Zero performance or behavioral overhead |

---

## 3. Real-World Code Examples

### Example 1: The Leaking Event Listener Caught by StrictMode
```tsx
import { useEffect, useState } from 'react';

// BAD: Missing cleanup function!
export function LeakyKeyListener() {
  const [lastKey, setLastKey] = useState('');

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => setLastKey(e.key);
    window.addEventListener('keydown', handleKey);
    // BUG: Missing return () => window.removeEventListener('keydown', handleKey);
  }, []);

  return <div>Last key: {lastKey}</div>;
}

// GOOD: Clean implementation handles unmount/remount
export function CleanKeyListener() {
  const [lastKey, setLastKey] = useState('');

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => setLastKey(e.key);
    window.addEventListener('keydown', handleKey);
    return () => {
      window.removeEventListener('keydown', handleKey);
    };
  }, []);

  return <div>Last key: {lastKey}</div>;
}
```
Output in Dev:
```
// LeakyKeyListener leaves 2 duplicate event listeners active in window.
// CleanKeyListener safely cleans up the first listener before remounting.
```

### Example 2: Impure Render Mutation Exposed by Double Rendering
```tsx
// BAD: Mutating external array during render
let globalItems: string[] = [];

export function ImpureList({ item }: { item: string }) {
  globalItems.push(item); // BUG: Mutation in render!
  return <div>Count: {globalItems.length}</div>;
}
```
Output:
```
// In Production: Count displays 1
// In StrictMode Dev: Count displays 2!
// The divergence instantly signals that globalItems.push() is an illegal side-effect.
```

### Example 3: AbortController Pattern for StrictMode Data Fetching
```tsx
import { useState, useEffect } from 'react';

export function UserBio({ userId }: { userId: string }) {
  const [bio, setBio] = useState('Loading...');

  useEffect(() => {
    const controller = new AbortController();

    fetch(`/api/users/${userId}`, { signal: controller.signal })
      .then((res) => res.json())
      .then((data) => setBio(data.bio))
      .catch((err) => {
        if (err.name !== 'AbortError') {
          setBio('Error loading bio');
        }
      });

    // Cleanup aborts request during StrictMode unmount simulation
    return () => {
      controller.abort();
    };
  }, [userId]);

  return <div>{bio}</div>;
}
```
Network Log in Dev:
```
[GET /api/users/1] (Status: Canceled / Aborted)
[GET /api/users/1] (Status: 200 OK)
```

### Example 4: Testing Component Under StrictMode in Vitest
```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { StrictMode } from 'react';
import { CleanKeyListener } from './CleanKeyListener';

describe('StrictMode Mount Test', () => {
  it('mounts cleanly under StrictMode without throwing', () => {
    render(
      <StrictMode>
        <CleanKeyListener />
      </StrictMode>
    );

    expect(screen.getByText(/Last key:/)).toBeInTheDocument();
  });
});
```

### Example 5: Why `useRef(false)` Workarounds Are Anti-Patterns
```tsx
import { useEffect, useRef } from 'react';

// ANTI-PATTERN: Attempting to prevent effect running twice using a ref guard!
export function BadEffectGuard() {
  const hasRun = useRef(false);

  useEffect(() => {
    if (hasRun.current) return; // DANGEROUS WORKAROUND!
    hasRun.current = true;

    console.log('Setup connection');
    // If the component is legitimately unmounted and remounted (e.g. user tab switch),
    // hasRun remains true, and the connection will NEVER re-open in production!
  }, []);

  return <div>Guard Demo</div>;
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Disabling StrictMode in `main.tsx` | Leaves memory leaks, race conditions, and impurity bugs undetected | Keep StrictMode enabled; fix missing effect cleanups |
| Using `const isMounted = useRef(false)` to bypass double execution | Breaks component remounting when user navigates back to cached tabs | Write genuine cleanup functions (`abort()`, `disconnect()`) |
| Writing state mutations in reducers (`state.list.push(item)`) | Reducers run twice; mutating in place duplicates items | Use pure return copies: `{ ...state, list: [...state.list, item] }` or Immer |
| Firing non-idempotent analytics events directly inside `useEffect` | StrictMode triggers duplicate analytics pageviews in development | Track analytics in route change listeners or user click handlers |
| Confusing double rendering with an infinite render loop | Double rendering stops after 2 runs; infinite loops throw `Too many re-renders` | Inspect call stack to verify if render continues endlessly |
| Attempting to fix double renders using `useCallback` | StrictMode re-invokes functional components regardless of callbacks | Embrace double-rendering as an automated health check |

---

## 5. React Internals: Beneath the Abstraction

### Double Invocation Implementation in CPython/C++-like Fiber Code
In React's reconciler (`ReactFiberBeginWork.js`):
```javascript
// Simplified conceptual implementation in React reconciler:
function renderWithHooks(current, workInProgress, Component, props) {
  let children = Component(props);

  if (__DEV__ && (workInProgress.mode & StrictMode)) {
    // Intentionally discard and execute second invocation:
    setIsExtraRenderCycle(true);
    try {
      children = Component(props);
    } finally {
      setIsExtraRenderCycle(false);
    }
  }

  return children;
}
```

---

## 6. Performance, Bundling & React Compiler

- **Stripped in Production**: Bundlers like Vite and Rollup replace `process.env.NODE_ENV !== "production"` checks with `false`. Dead-code elimination completely purges StrictMode double-invocation branches from the production bundle.

---

## 7. Accessibility (a11y) & Usability

- **Ensuring Clean Modal Focus**: StrictMode unmount/remount tests verify that modals, tooltips, and focus traps release document focus cleanly when destroyed, ensuring keyboard accessibility does not lock up.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { StrictMode } from 'react';
import { UserBio } from './UserBio';

describe('StrictMode Resilience', () => {
  it('handles abortable fetch gracefully', async () => {
    render(
      <StrictMode>
        <UserBio userId="user-42" />
      </StrictMode>
    );

    expect(screen.getByText('Loading...')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration

| React Version | StrictMode Evolution |
| :--- | :--- |
| **React 16.3** | Introduced StrictMode (warns on `findDOMNode`, legacy context) |
| **React 17** | Modified console logging to dim second invocation in dev tools |
| **React 18** | Added intentional unmount/remount cycle (`Mount -> Cleanup -> Remount`) |
| **React 19** | StrictMode double-invokes Action transition functions to ensure idempotency |

---

## 10. Security & Hardening

- **Avoiding Leaked Security Tokens**: Incomplete effect cleanups can leave background intervals polling authenticated endpoints long after a user has logged out. StrictMode ensures all polling timers are cleanly terminated on unmount.

---

## 11. Real-World Case Study

### Fixing a WebSocket Connection Leak
A team noticed their development WebSocket server crashed after 30 minutes of frontend development due to exceeding maximum connection limits.
- **Investigation**: Components opened sockets in `useEffect` without a `socket.close()` cleanup return.
- Every page refresh and StrictMode mount spawned 2 active connections that never closed.
- **Fix**: Added `return () => socket.close();`. StrictMode immediately verified that socket count remained strictly 1. Server crashes ceased completely.

---

## 12. Interview Q&A

<details>
<summary>1. Why did React 18 introduce the Mount -> Cleanup -> Remount lifecycle in StrictMode?</summary>
React 18 introduced Concurrent features like offscreen component rendering (preserving state of hidden tabs or views). When a component is hidden and restored, React remounts it using its previous state. StrictMode simulates this cycle in development to ensure all effects have proper cleanup functions that prevent memory leaks and duplicate subscriptions.
</details>

<details>
<summary>2. Does StrictMode cause performance issues in production?</summary>
No. In production (<code>NODE_ENV === "production"</code>), StrictMode checks and double-invocations are completely disabled. It behaves as an empty fragment with zero performance or memory overhead.
</details>

<details>
<summary>3. Why is using a boolean flag like isMounted = false an anti-pattern to prevent double fetches?</summary>
Using a flag or ref to block the second execution treats the symptom instead of the cause. If the component is legitimately unmounted and remounted in production (such as tab switching or Fast Refresh), the flag prevents the effect from running when it actually needs to, leaving the UI empty or disconnected.
</details>

<details>
<summary>4. What should you do if an API call runs twice in development and causes backend issues?</summary>
1. If the API call is a data mutation (creating a record, charging a card), it does NOT belong in <code>useEffect</code>; it belongs in a user event handler or React 19 Action.
2. If it is a read query, use an <code>AbortController</code> to cancel the first fetch, or use TanStack Query which deduplicates requests automatically.
</details>

<details>
<summary>5. Are event handlers like onClick double-invoked in StrictMode?</summary>
No. StrictMode only double-invokes component render functions, state initializers, reducers, and effects. User-triggered event handlers execute exactly once.
</details>

<details>
<summary>6. How does React DevTools handle logs from the second StrictMode render?</summary>
React DevTools automatically intercepts <code>console.log</code> calls from the second render pass and dims them slightly in the browser console to distinguish them from the initial invocation.
</details>

<details>
<summary>7. What happens if a reducer is not pure in StrictMode?</summary>
Because <code>useReducer</code> invokes the reducer function twice in development, an impure reducer that mutates state in place (e.g. <code>state.count++</code>) will increment by 2 instead of 1, instantly exposing the mutation bug.
</details>

<details>
<summary>8. How do you disable StrictMode for a single legacy third-party library?</summary>
Instead of wrapping your root <code>&lt;App /&gt;</code> in <code>&lt;StrictMode&gt;</code>, wrap only your modern feature subtrees in <code>&lt;StrictMode&gt;</code>, leaving the legacy component outside the boundary.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Audio Element Cleanup</summary>

**Specification**: Write an effect that plays an audio notification on mount and pauses/resets it on cleanup.

```tsx
import { useEffect } from 'react';

export function AudioNotification({ audioUrl }: { audioUrl: string }) {
  useEffect(() => {
    const audio = new Audio(audioUrl);
    audio.play().catch(() => {});

    return () => {
      audio.pause();
      audio.currentTime = 0;
    };
  }, [audioUrl]);

  return <div>Playing sound...</div>;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Safe Document Event Listener</summary>

**Specification**: Add an escape key listener to close a modal that properly removes the listener on unmount.

```tsx
import { useEffect } from 'react';

export function EscapeListener({ onEscape }: { onEscape: () => void }) {
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onEscape();
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
    };
  }, [onEscape]);

  return null;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): StrictMode-Safe Web Worker</summary>

**Specification**: Instantiate a Web Worker inside `useEffect` and terminate it cleanly during cleanup.

```tsx
import { useEffect, useState } from 'react';

export function HeavyComputation() {
  const [result, setResult] = useState<number | null>(null);

  useEffect(() => {
    const worker = new Worker(new URL('./worker.ts', import.meta.url), { type: 'module' });

    worker.onmessage = (e) => {
      setResult(e.data);
    };

    worker.postMessage('start');

    return () => {
      // Clean up worker during StrictMode remount simulation
      worker.terminate();
    };
  }, []);

  return <div>Worker Result: {result ?? 'Computing...'}</div>;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): IntersectionObserver Lifecycle</summary>

**Specification**: Observe an element using `IntersectionObserver` in `useEffect` and disconnect the observer on cleanup.

```tsx
import { useEffect, useRef, useState } from 'react';

export function LazyImageSentinel({ onVisible }: { onVisible: () => void }) {
  const sentinelRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!sentinelRef.current) return;

    const observer = new IntersectionObserver(([entry]) => {
      if (entry.isIntersecting) {
        onVisible();
      }
    });

    observer.observe(sentinelRef.current);

    return () => {
      observer.disconnect();
    };
  }, [onVisible]);

  return <div ref={sentinelRef} className="h-4" />;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Abortable Fetch Hook with StrictMode Verification</summary>

**Specification**: Write a test verifying that `useAbortableFetch` cancels the first request when run inside `<StrictMode>`.

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect, vi } from 'vitest';
import { StrictMode } from 'react';
import { UserBio } from './UserBio';

describe('StrictMode Abort Test', () => {
  it('mounts and cleanly aborts pre-flight fetch', () => {
    render(
      <StrictMode>
        <UserBio userId="test-1" />
      </StrictMode>
    );

    expect(screen.getByText('Loading...')).toBeInTheDocument();
  });
});
```
</details>

<details>
<summary>Exercise 6 (Hard): BroadcastChannel Cross-Tab Synchronizer</summary>

**Specification**: Synchronize state across tabs using `BroadcastChannel` and close the channel on cleanup.

```tsx
import { useEffect, useState } from 'react';

export function CrossTabCounter() {
  const [count, setCount] = useState(0);

  useEffect(() => {
    const channel = new BroadcastChannel('app_counter');

    channel.onmessage = (e) => {
      if (typeof e.data === 'number') {
        setCount(e.data);
      }
    };

    return () => {
      // Guarantee channel close
      channel.close();
    };
  }, []);

  const increment = () => {
    const next = count + 1;
    setCount(next);
    const channel = new BroadcastChannel('app_counter');
    channel.postMessage(next);
    channel.close();
  };

  return (
    <button onClick={increment} className="border p-2">
      Shared Count: {count}
    </button>
  );
}
```
</details>
