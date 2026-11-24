# React useEffectEvent: Extracting Non-Reactive Logic from Effects

- **Topic**: `03-hooks`
- **React version**: React 19 Experimental / RFC (with Stable Ref-Based Shim)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Addresses the most frequent `react-hooks/exhaustive-deps` dilemma; eliminates unnecessary effect re-executions caused by unstable callbacks.

---

## 1. Concept Overview

A fundamental challenge in React's `useEffect` model is distinguishing between **reactive logic** (code that *should* cause the effect to re-synchronize when values change) and **non-reactive event logic** (code that needs to read the *latest* values when the effect fires, but should *not* trigger a re-synchronization when those values change).

Consider a chat room component that connects to a room and logs a page view with the user's current theme preference:
```tsx
useEffect(() => {
  const connection = createConnection(roomId);
  connection.connect();

  // We want to log the event with the latest theme:
  logAnalytics('visit_room', { roomId, theme });

  return () => connection.disconnect();
}, [roomId, theme]); // ⚠️ Problem: Changing `theme` disconnects & reconnects the chat socket!
```

In the example above, `theme` is non-reactive logic. When the user toggles dark mode, we do **not** want to sever and re-establish the WebSocket connection; we only want to read the current `theme` when `roomId` changes.

`useEffectEvent` is a specialized React hook that separates non-reactive logic from reactive effects. It creates a special function that always sees the latest props and state, but is **omitted from the effect's dependency array**.

```
Without useEffectEvent:
Theme changes ──► triggers useEffect ──► Socket disconnects & reconnects! (Bad UX)

With useEffectEvent:
Theme changes ──► Effect does NOT re-run! (Socket remains connected)
Room ID changes ──► Effect re-runs ──► calls onVisit() ──► reads latest theme! (Ideal)
```

### When to Use
- Logging analytics inside effects where event payloads depend on user state or props.
- Firing notification toasts or callbacks (`onSuccess`, `onComplete`) received via props when an effect finishes without causing re-synchronization.
- Reading mutable configuration flags inside long-lived interval or subscription effects.

### When NOT to Use
- **Do NOT pass `useEffectEvent` functions to child components as props**: They are strictly scoped to the enclosing component's effects.
- **Do NOT call `useEffectEvent` functions inside user event handlers (`onClick`, `onChange`)**: User event handlers can read state directly without special wrappers.
- **Do NOT use for values that SHOULD re-trigger the effect**: If changing a URL should trigger a new fetch, that URL must remain in the reactive dependency array.

---

## 2. Key Mechanics & Architecture

### API Signature
```ts
const onEvent = useEffectEvent<Args, Return>((...args: Args) => Return);
```

### Core Invariants
1. **Always Sees Latest Values**: When `onEvent` is invoked, it always reads the most up-to-date props and state from the component render.
2. **Omitted from Dependencies**: React's ESLint rules (`react-hooks/exhaustive-deps`) explicitly ignore functions created by `useEffectEvent`. They must **not** be included in dependency arrays.
3. **Execution Guard**: Can **only** be called from inside `useEffect` or `useLayoutEffect`. Calling it during render or from external event handlers throws a runtime error.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Context** | Strictly inside `useEffect` or `useLayoutEffect` |
| **Dependency Inclusion** | Must NOT be listed in `useEffect` dependency array |
| **Identity Stability** | Stable identity; guaranteed not to trigger re-renders |
| **State Freshness** | Always captures fresh render variables at invocation time |
| **Status in React 19** | Feature preview / RFC in Canary; standard user-land shim available |
| **React Compiler Impact** | Recognized by compiler; automatically treats body as non-reactive scope |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Preventing chat reconnection when notification settings change.

```tsx
import React, { useState, useEffect } from 'react';
// Note: In stable React 19, use the production ref-shim (shown in Example 3)
// or the experimental react export:
import { experimental_useEffectEvent as useEffectEvent } from 'react';

interface ChatConnection {
  connect: () => void;
  disconnect: () => void;
}

declare function createConnection(roomId: string): ChatConnection;
declare function showNotification(message: string, sound: boolean): void;

export function ChatRoom({ roomId }: { roomId: string }) {
  const [enableSound, setEnableSound] = useState(true);

  // Extract non-reactive notification logic into useEffectEvent:
  const onConnected = useEffectEvent(() => {
    showNotification(`Connected to room: ${roomId}`, enableSound);
  });

  useEffect(() => {
    const connection = createConnection(roomId);
    connection.connect();

    // Call event function safely:
    onConnected();

    return () => connection.disconnect();
  }, [roomId]); // Notice: `enableSound` is completely omitted from deps!

  return (
    <div>
      <h3>Room: {roomId}</h3>
      <label>
        <input
          type="checkbox"
          checked={enableSound}
          onChange={(e) => setEnableSound(e.target.checked)}
        />
        Enable Notification Sound
      </label>
    </div>
  );
}
```
Output:
```
// Toggling enableSound checkbox DOES NOT reconnect the chat room!
// Changing roomId cleanly disconnects old room and connects new room with latest sound preference.
```

### Example 2: Production / Real-World Integration
Analytics page tracking with reactive route changes and non-reactive cart count.

```tsx
import React, { useEffect } from 'react';
import { experimental_useEffectEvent as useEffectEvent } from 'react';

declare function logAnalyticsEvent(event: string, meta: Record<string, any>): void;

export function ProductPage({
  productId,
  cartCount,
}: {
  productId: string;
  cartCount: number;
}) {
  // We want to log product_view whenever productId changes,
  // capturing the current cartCount WITHOUT re-logging when cartCount changes!
  const onProductView = useEffectEvent((id: string) => {
    logAnalyticsEvent('product_view', {
      productId: id,
      itemsInCart: cartCount,
      timestamp: Date.now(),
    });
  });

  useEffect(() => {
    onProductView(productId);
  }, [productId]); // cartCount is excluded from deps!

  return <div className="p-4">Product Details for {productId}</div>;
}
```
Output:
```
// When user adds item to cart (cartCount increments), product_view is NOT re-sent.
// When user navigates to a new productId, product_view logs with fresh cartCount.
```

### Example 3: Edge Case / Production-Ready Ref-Based Shim
Since `useEffectEvent` is currently under experimental/RFC flag in standard React releases, production applications use the authoritative, battle-tested ref-based shim:

```tsx
import { useRef, useInsertionEffect, useCallback } from 'react';

/**
 * Production-ready shim for useEffectEvent.
 * Always reads latest callback without triggering effect re-execution.
 */
export function useEvent<T extends (...args: any[]) => any>(fn: T): T {
  const ref = useRef<T>(fn);

  // Synchronously update ref before any layout or passive effects run
  useInsertionEffect(() => {
    ref.current = fn;
  });

  return useCallback((...args: Parameters<T>) => {
    return ref.current(...args);
  }, []) as T;
}

// Usage in application components:
export function TimerWidget({ onTick }: { onTick: (time: number) => void }) {
  // Stable callback reference that never changes, but always calls the latest onTick prop!
  const handleTick = useEvent(onTick);

  useEffect(() => {
    const interval = setInterval(() => {
      handleTick(Date.now());
    }, 1000);

    return () => clearInterval(interval);
  }, [handleTick]); // Safe: handleTick identity is permanent!

  return <div className="text-xs text-zinc-500">Timer Active</div>;
}
```
Output:
```
// Parent can pass inline arrow functions for onTick without resetting the 1000ms interval!
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Calling `useEffectEvent` inside a regular click event handler or passing it as a prop.

```tsx
// ❌ WRONG: Passing useEffectEvent function as a prop or calling it in onClick!
export function BadComponent({ onClick }: { onClick: () => void }) {
  const handleClickEvent = useEffectEvent(() => {
    onClick();
  });

  return (
    // ⚠️ RUNTIME ERROR: A function created by useEffectEvent can only be called
    // from an Effect! Calling it from an event handler throws!
    <button onClick={() => handleClickEvent()}>Click Me</button>
  );
}

// ✅ REMEDIATION: Event handlers already have access to fresh props/state!
// useEffectEvent is ONLY for effects!
export function GoodComponent({ onClick }: { onClick: () => void }) {
  return <button onClick={onClick}>Click Me</button>;
}
```
Output:
```
Error: An effect event cannot be called outside of an effect.
```

### Example 5: Idiomatic Modern React 19 Pattern
Window resize tracker with throttle callback that reads latest theme state.

```tsx
import React, { useState, useEffect } from 'react';
import { useEvent } from './useEvent'; // Using our production shim

export function ResizeListener() {
  const [windowWidth, setWindowWidth] = useState(window.innerWidth);
  const [theme] = useState<'light' | 'dark'>('dark');

  const onWindowResize = useEvent((width: number) => {
    console.log(`Window resized to ${width}px under ${theme} theme`);
    setWindowWidth(width);
  });

  useEffect(() => {
    const handleResize = () => onWindowResize(window.innerWidth);
    window.addEventListener('resize', handleResize);

    return () => window.removeEventListener('resize', handleResize);
  }, [onWindowResize]); // Listener is attached ONCE and never re-bound on theme change!

  return <div className="text-sm">Current Width: {windowWidth}px</div>;
}
```
Output:
```
// Resize listener stays attached continuously without memory leaks or rebinding.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Passing `useEffectEvent` to child components** | Violates React invariant: effect events are strictly private to the component's effects. | Pass standard callbacks or use `useCallback` for child props. |
| **Calling `useEffectEvent` inside `onClick`** | Effect events are guarded to only execute during effect execution phases. | Call the callback directly in `onClick`. |
| **Including `useEffectEvent` in the dependency array** | Defeats the entire purpose; effect events are intentionally omitted from deps. | Remove the effect event from the `useEffect` dependency array. |
| **Using `useEffectEvent` to suppress reactive dependencies** | Hiding props that *should* trigger an effect re-run causes stale sync bugs. | Only extract values that represent non-reactive side-effects. |
| **Calling `useEffectEvent` conditionally** | It is still a React Hook and must obey top-level call rules. | Call `useEffectEvent` unconditionally at top level. |
| **Reading DOM refs inside `useEffectEvent` before mount** | Effect events should only be invoked inside `useEffect` when DOM is ready. | Only call inside `useEffect` execution body. |

---

## 5. React Internals: Beneath the Abstraction

Inside React:
1. `useEffectEvent` registers a special hook record on the fiber's memoizedState.
2. The returned wrapper maintains an internal closure:
   - During the render phase, React updates the internal reference pointer `inst.impl = callback`.
   - React sets an internal execution flag: `isExecutingEffect = false`.
3. When React executes `commitPassiveMountEffects` (running `useEffect`):
   - React sets `isExecutingEffect = true`.
   - If the component calls the effect event, React executes `inst.impl(...args)`.
   - Once all effects finish running, React sets `isExecutingEffect = false`.
4. If code attempts to invoke the effect event while `isExecutingEffect === false` (e.g. during a click handler or render), React throws an invariant violation error.

---

## 6. Performance, Bundling & React Compiler

- **Connection Thrashing Elimination**: Prevents teardown and re-establishment of expensive resources (WebSockets, WebRTC channels, Canvas animation loops).
- **ESLint Integration**: Officially supported by `@eslint/plugin-react-hooks` v6+ and `eslint-plugin-react-compiler`.
- **React Compiler**: The React Compiler recognizes the `useEffectEvent` semantic contract and prevents unnecessary effect invalidation automatically.

---

## 7. Accessibility (a11y) & Usability

When managing audio feedback, screen reader announcements, or focus restoration via effects, `useEffectEvent` ensures that user preference changes (e.g. toggling sound effects or high-contrast mode) do not cancel in-flight announcements or re-trigger intrusive focus shifts.

---

## 8. Testing & Verification Patterns

Testing effect event behavior with Vitest:

```tsx
import { render, act } from '@testing-library/react';
import { describe, it, expect, vi } from 'vitest';
import React, { useState } from 'react';
import { useEvent } from './useEvent';

describe('useEffectEvent pattern', () => {
  it('reads latest prop value without re-triggering effect', () => {
    const effectSpy = vi.fn();

    function TestComponent({ filter }: { filter: string }) {
      const onFilterApplied = useEvent((f: string) => {
        effectSpy(f);
      });

      React.useEffect(() => {
        onFilterApplied(filter);
      }, []); // Empty deps: runs only once on mount

      return <div>Filter: {filter}</div>;
    }

    const { rerender } = render(<TestComponent filter="alpha" />);
    expect(effectSpy).toHaveBeenCalledTimes(1);
    expect(effectSpy).toHaveBeenCalledWith('alpha');

    // Re-rendering with new prop does NOT re-trigger the effect!
    rerender(<TestComponent filter="beta" />);
    expect(effectSpy).toHaveBeenCalledTimes(1);
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | No native API. Developers relied on `useRef` + `useEffect` to create manual stable callbacks (`useLatest`). | Use `useEvent` shim for clean migration. |
| **React Canary** | Introduced `experimental_useEffectEvent`. | Evaluated in large production codebases. |
| **React 19** | Standardized mental model for separating reactive values from non-reactive logic. | Use `experimental_useEffectEvent` or `useEvent` shim. |

---

## 10. Security & Hardening

1. **Information Leakage in Event Logs**: Ensure analytics functions called from effect events do not inadvertently capture sensitive session tokens or unmasked passwords stored in adjacent component state.
2. **Execution Timing**: Because effect events run asynchronously during `useEffect`, avoid using them for security-critical validation checks that must intercept user clicks synchronously.

---

## 11. Real-World Case Study

### High-Volume WebSocket Trading Ticker

A real-time crypto trading interface connects to an exchange order book socket. The socket connection is keyed by `symbol` (e.g. `BTC-USD`). The user can customize notification thresholds, price alerts, and audio alerts.

Using standard `useEffect`, every alert threshold change severed the WebSocket connection, causing missed trade ticks. Introducing `useEffectEvent` kept the connection alive continuously while still dispatching notifications against the latest user alert thresholds.

```tsx
import React, { useEffect } from 'react';
import { useEvent } from './useEvent';

interface AlertConfig {
  minPrice: number;
  maxPrice: number;
}

export function CryptoTicker({
  symbol,
  alerts,
}: {
  symbol: string;
  alerts: AlertConfig;
}) {
  // Non-reactive alert evaluation:
  const handlePriceUpdate = useEvent((currentPrice: number) => {
    if (currentPrice > alerts.maxPrice) {
      console.warn(`ALERT: ${symbol} exceeded max threshold at $${currentPrice}!`);
    } else if (currentPrice < alerts.minPrice) {
      console.warn(`ALERT: ${symbol} fell below min threshold at $${currentPrice}!`);
    }
  });

  useEffect(() => {
    const ws = new WebSocket(`wss://market.crypto.io/ticks/${symbol}`);

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      handlePriceUpdate(data.price);
    };

    return () => ws.close();
  }, [symbol]); // Socket ONLY reconnects when `symbol` changes!

  return <div className="font-mono text-sm">Monitoring {symbol}...</div>;
}
```

---

## 12. Interview Q&A

<details>
<summary>1. What is the fundamental problem `useEffectEvent` solves in React's effect system?</summary>

In React, `useEffect` synchronizes with an external system and re-runs whenever any value in its dependency array changes. Often, an effect needs to read a current value (like a theme, callback, or analytics setting) that should *not* cause the effect to re-synchronize when it changes.
Prior to `useEffectEvent`, developers either had to add the value to dependencies (causing unnecessary re-runs) or use ESLint disable comments (risking stale closures). `useEffectEvent` provides an official primitive to read latest values without registering them as reactive dependencies.
</details>

<details>
<summary>2. Why are functions created by `useEffectEvent` omitted from the `useEffect` dependency array?</summary>

`useEffectEvent` is specifically designed for non-reactive logic. By contract, it guarantees that changes to the values read inside the event function will not cause the effect to re-run. Therefore, React's linter and runtime strictly prohibit adding it to the dependency array.
</details>

<details>
<summary>3. Why does React forbid calling `useEffectEvent` functions inside `onClick` handlers?</summary>

User event handlers (`onClick`, `onChange`, `onSubmit`) run during browser user interactions and already have direct access to the latest props and state without causing effect re-runs. Allowing effect events to be passed around as general callbacks encourages anti-patterns and breaks React's mental model separating render, effects, and events.
</details>

<details>
<summary>4. How does `useEffectEvent` differ from `useCallback`?</summary>

`useCallback` returns a memoized function that changes identity whenever its dependencies change, and it *must* be included in dependency arrays.
`useEffectEvent` returns a function with a stable identity that is *never* listed in dependency arrays and always reads the latest values at the moment it is called.
</details>

<details>
<summary>5. How can you implement a safe user-land shim for `useEffectEvent` in production React today?</summary>

You can store the callback in a mutable `useRef` and update it synchronously inside `useInsertionEffect` (or `useLayoutEffect`). Then, return a stable `useCallback` that invokes `ref.current(...args)`. This guarantees fresh closure access without changing function identity.
</details>

<details>
<summary>6. Can you return a value from a `useEffectEvent` function?</summary>

Yes. A `useEffectEvent` function can return values to the enclosing effect synchronously just like any standard JavaScript function.
</details>

<details>
<summary>7. What is the difference between a "reactive" value and a "non-reactive" value in React?</summary>

A **reactive** value is any variable calculated during render (props, state, or variables derived from them). When a reactive value changes, the component re-renders and effects depending on it should re-synchronize.
A **non-reactive** value is a piece of logic or data that is read as a snapshot in response to a specific event without driving the synchronization lifecycle itself.
</details>

<details>
<summary>8. How does the React Compiler handle `useEffectEvent`?</summary>

The React Compiler recognizes `useEffectEvent` semantics. It does not attempt to memoize the call site or extract dependencies for the event body, treating it as an explicit non-reactive escape hatch.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Auto-Save Draft with Latest User Tags</summary>

**Specification**: Effect saves a document draft on a 5-second interval, reading the latest `tags` array without resetting the interval timer on every tag edit.

```tsx
import React, { useEffect, useState } from 'react';
import { useEvent } from './useEvent';

export function DraftAutoSaver({ docId, tags }: { docId: string; tags: string[] }) {
  const onSave = useEvent(() => {
    console.log(`Auto-saving document ${docId} with tags:`, tags);
  });

  useEffect(() => {
    const timer = setInterval(() => {
      onSave();
    }, 5000);

    return () => clearInterval(timer);
  }, [docId]); // tags is excluded! Interval is never interrupted!

  return <div className="text-xs text-zinc-400">Auto-save active</div>;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Sound-Enabled Page View Logger</summary>

**Specification**: Play an audio chime and log page view when route changes, respecting a `soundMuted` toggle without re-logging on mute toggle.

```tsx
import React, { useEffect } from 'react';
import { useEvent } from './useEvent';

export function RouteAuditor({ path, soundMuted }: { path: string; soundMuted: boolean }) {
  const onVisit = useEvent((currentPath: string) => {
    if (!soundMuted) {
      console.log('Ding! Audio chime played');
    }
    console.log('Logged visit to:', currentPath);
  });

  useEffect(() => {
    onVisit(path);
  }, [path]);

  return <div className="text-xs">Auditing {path}</div>;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Scroll Spy with Dynamic Section Callbacks</summary>

**Specification**: Track scroll position and invoke an `onSectionEnter` prop with current scroll speed without re-binding the window scroll listener.

```tsx
import React, { useEffect } from 'react';
import { useEvent } from './useEvent';

export function ScrollSpy({ onSectionEnter, activeSection }: { onSectionEnter: (sec: string) => void; activeSection: string }) {
  const handleScroll = useEvent(() => {
    onSectionEnter(activeSection);
  });

  useEffect(() => {
    window.addEventListener('scroll', handleScroll);
    return () => window.removeEventListener('scroll', handleScroll);
  }, [handleScroll]);

  return null;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Resilient Event-Driven Poller</summary>

**Specification**: Build an HTTP polling effect that respects dynamic query headers without restarting the polling timer when headers mutate.

```tsx
import React, { useEffect } from 'react';
import { useEvent } from './useEvent';

export function PollingClient({ endpoint, headers }: { endpoint: string; headers: Record<string, string> }) {
  const fetchTick = useEvent(async () => {
    console.log(`Polling ${endpoint} with auth:`, headers['Authorization']);
  });

  useEffect(() => {
    const interval = setInterval(fetchTick, 3000);
    return () => clearInterval(interval);
  }, [endpoint]); // Poller only restarts if endpoint changes!

  return <div className="text-xs text-zinc-500">Polling {endpoint}...</div>;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): High-Frequency Telemetry Buffer Flusher</summary>

**Specification**: Component flushes a telemetry queue on unmount, reading the latest application session ID and auth token without registering them in the cleanup effect deps.

```tsx
import React, { useEffect } from 'react';
import { useEvent } from './useEvent';

export function TelemetryFlusher({ sessionId, token }: { sessionId: string; token: string }) {
  const flushTelemetry = useEvent(() => {
    console.log(`Flushing buffered telemetry for session ${sessionId} with token ${token.slice(0, 4)}...`);
  });

  useEffect(() => {
    // Flush on unmount only
    return () => {
      flushTelemetry();
    };
  }, []);

  return null;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Intersection Observer Visibility Tracker</summary>

**Specification**: Reusable hook using `useEffectEvent` pattern to monitor DOM element visibility and report view durations, capturing dynamic analytics tags without resetting the observer.

```tsx
import React, { useEffect, useRef } from 'react';
import { useEvent } from './useEvent';

export function useVisibilityTracker(
  targetRef: React.RefObject<HTMLElement | null>,
  metadata: Record<string, any>
) {
  const onVisibilityChange = useEvent((isIntersecting: boolean) => {
    if (isIntersecting) {
      console.log('Element visible with metadata:', metadata);
    }
  });

  useEffect(() => {
    const element = targetRef.current;
    if (!element) return;

    const observer = new IntersectionObserver(([entry]) => {
      onVisibilityChange(entry.isIntersecting);
    });

    observer.observe(element);
    return () => observer.disconnect();
  }, [targetRef]);
}
```
</details>
