# Hook Recipes

A catalog of small, reusable custom hooks for problems that come up in almost every app. Each recipe shows the hook, a usage example, and the design decisions worth knowing. Copy them, or treat them as worked examples of the principles in [`09-custom-hooks.md`](./09-custom-hooks.md).

## Prerequisites

[`09-custom-hooks.md`](./09-custom-hooks.md), [`02-useEffect.md`](./02-useEffect.md), and [`04-useRef.md`](./04-useRef.md)

All recipes are plain JavaScript. For typed versions see [`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md). Many mature libraries (for example `usehooks-ts`, `react-use`) provide tested versions; writing them yourself once is the best way to understand them.

---

## `useToggle`

A boolean with a stable toggle function.

```jsx
import { useState, useCallback } from "react";

export function useToggle(initial = false) {
  const [value, setValue] = useState(initial);
  const toggle = useCallback(() => setValue((v) => !v), []);
  return [value, toggle, setValue];
}
```

```jsx
const [isOpen, toggleOpen] = useToggle();
<button onClick={toggleOpen}>{isOpen ? "Hide" : "Show"}</button>
```

Uses the **updater form** so `toggle` never reads a stale value and needs no dependencies.

---

## `useDebounce`

Returns a value that only updates after it has stopped changing for `delay` ms. Ideal for search-as-you-type.

```jsx
import { useState, useEffect } from "react";

export function useDebounce(value, delay = 300) {
  const [debounced, setDebounced] = useState(value);

  useEffect(() => {
    const id = setTimeout(() => setDebounced(value), delay);
    return () => clearTimeout(id);   // cancel if value changes before the delay
  }, [value, delay]);

  return debounced;
}
```

```jsx
function Search() {
  const [query, setQuery] = useState("");
  const debouncedQuery = useDebounce(query, 400);

  useEffect(() => {
    if (!debouncedQuery) return;
    const controller = new AbortController();
    fetch(`/api/search?q=${encodeURIComponent(debouncedQuery)}`, {
      signal: controller.signal,
    })
      .then((r) => r.json())
      .then(setResults)
      .catch((e) => { if (e.name !== "AbortError") console.error(e); });
    return () => controller.abort();
  }, [debouncedQuery]);

  return <input value={query} onChange={(e) => setQuery(e.target.value)} />;
}
```

The input stays responsive (it uses `query`), while the request uses the debounced value. With a data library, put `debouncedQuery` in the query key ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md)). For lowering the *priority* of rendering instead of delaying, see `useDeferredValue` ([`../15-concurrent-and-modern-react/02-useDeferredValue.md`](../15-concurrent-and-modern-react/02-useDeferredValue.md)).

---

## `useLocalStorage`

State that persists in `localStorage`.

```jsx
import { useState, useEffect } from "react";

export function useLocalStorage(key, initialValue) {
  const [value, setValue] = useState(() => {
    try {
      const stored = window.localStorage.getItem(key);
      return stored !== null ? JSON.parse(stored) : initialValue;
    } catch {
      return initialValue;   // unavailable storage or invalid JSON
    }
  });

  useEffect(() => {
    try {
      window.localStorage.setItem(key, JSON.stringify(value));
    } catch {
      // storage full or blocked: ignore
    }
  }, [key, value]);

  return [value, setValue];
}
```

```jsx
const [theme, setTheme] = useLocalStorage("theme", "light");
```

Design notes:

- **Lazy initializer** reads storage once, not on every render ([`01-useState.md`](./01-useState.md)).
- **`try/catch`** because storage can throw (private mode, quota, corrupt JSON).
- **Server rendering:** `window` doesn't exist there. Guard with `typeof window !== "undefined"`, or render a default first and read storage after mount, to avoid hydration mismatches ([`../15-concurrent-and-modern-react/07-server-components-and-ssr.md`](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)).
- **Don't store secrets** — `localStorage` is readable by any script on the page ([`../19-production/05-security.md`](../19-production/05-security.md)).
- Changes in *other tabs* aren't picked up; subscribe to the `storage` event (or `useSyncExternalStore`) if you need that.

---

## `usePrevious`

The value from the previous render.

```jsx
import { useRef, useEffect } from "react";

export function usePrevious(value) {
  const ref = useRef();

  useEffect(() => {
    ref.current = value;   // runs AFTER render, so the render saw the old value
  }, [value]);

  return ref.current;
}
```

```jsx
const [count, setCount] = useState(0);
const prevCount = usePrevious(count);
// "Now: {count}, before: {prevCount}"
```

It works because the effect runs after rendering: during the render, `ref.current` still holds the earlier value. It returns `undefined` on the first render.

If you only need the previous value to compare and react to a change, first consider whether you can restructure state instead ([`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md)).

---

## `useMediaQuery`

Tracks whether a CSS media query matches.

```jsx
import { useState, useEffect } from "react";

export function useMediaQuery(query) {
  const [matches, setMatches] = useState(() =>
    typeof window !== "undefined" ? window.matchMedia(query).matches : false
  );

  useEffect(() => {
    const mql = window.matchMedia(query);
    const onChange = (e) => setMatches(e.matches);

    setMatches(mql.matches);                  // sync if query changed
    mql.addEventListener("change", onChange);
    return () => mql.removeEventListener("change", onChange);
  }, [query]);

  return matches;
}
```

```jsx
const isDesktop = useMediaQuery("(min-width: 768px)");
const prefersDark = useMediaQuery("(prefers-color-scheme: dark)");
const reducedMotion = useMediaQuery("(prefers-reduced-motion: reduce)");
```

Prefer **CSS media queries** for purely visual responsiveness ([`../07-styling/03-responsive-design.md`](../07-styling/03-responsive-design.md)). Use this hook when the *component tree itself* must differ (render a drawer instead of a sidebar) or to respect `prefers-reduced-motion` in JavaScript animations ([`../21-specializations/animation/04-animation-performance-and-accessibility.md`](../21-specializations/animation/04-animation-performance-and-accessibility.md)). `useSyncExternalStore` is the more robust implementation for server rendering ([`../16-advanced-react/02-external-stores.md`](../16-advanced-react/02-external-stores.md)).

---

## `useOnClickOutside`

Calls a handler when the user clicks or taps outside an element — dropdowns, popovers, modals.

```jsx
import { useEffect } from "react";

export function useOnClickOutside(ref, handler) {
  useEffect(() => {
    function listener(event) {
      if (!ref.current || ref.current.contains(event.target)) return;
      handler(event);
    }
    document.addEventListener("mousedown", listener);
    document.addEventListener("touchstart", listener);
    return () => {
      document.removeEventListener("mousedown", listener);
      document.removeEventListener("touchstart", listener);
    };
  }, [ref, handler]);
}
```

```jsx
function Dropdown() {
  const ref = useRef(null);
  const [open, setOpen] = useState(false);
  useOnClickOutside(ref, () => setOpen(false));

  return (
    <div ref={ref}>
      <button onClick={() => setOpen(!open)}>Menu</button>
      {open && <Menu />}
    </div>
  );
}
```

Because `handler` is usually an inline arrow (new each render), the effect re-subscribes each time. That's correct but wasteful; store the latest handler in a ref (as in `useInterval` below) or wrap it in `useCallback`.

Accessibility: pair this with **Escape-key handling and focus management**; a click-outside alone isn't enough for keyboard users ([`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md)). Prefer a library primitive (Radix, via shadcn/ui) for real dropdowns and dialogs ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)).

---

## `useInterval`

`setInterval` that always calls the **latest** callback, and can be paused by passing `null`.

```jsx
import { useEffect, useRef } from "react";

export function useInterval(callback, delay) {
  const savedCallback = useRef(callback);

  // Always remember the latest callback
  useEffect(() => {
    savedCallback.current = callback;
  }, [callback]);

  useEffect(() => {
    if (delay === null) return;   // paused
    const id = setInterval(() => savedCallback.current(), delay);
    return () => clearInterval(id);
  }, [delay]);
}
```

```jsx
function Clock() {
  const [seconds, setSeconds] = useState(0);
  const [running, setRunning] = useState(true);

  useInterval(() => setSeconds((s) => s + 1), running ? 1000 : null);

  return (
    <>
      <p>{seconds}s</p>
      <button onClick={() => setRunning(!running)}>
        {running ? "Pause" : "Resume"}
      </button>
    </>
  );
}
```

**Why the ref?** A naive `setInterval(callback, delay)` inside an effect either captures a **stale** callback (empty deps) or **resets the timer** every render (callback in deps). Keeping the latest callback in a ref lets the interval stay put while always calling fresh logic. (Recent React versions provide `useEffectEvent` for this pattern — see [`02-useEffect.md`](./02-useEffect.md).)

---

## `useFetch` (a minimal version)

A tiny fetching hook that handles loading, errors, and cancellation. Good for learning and small projects; for real apps use a data library.

```jsx
import { useState, useEffect } from "react";

export function useFetch(url) {
  const [state, setState] = useState({ status: "idle", data: null, error: null });

  useEffect(() => {
    if (!url) return;
    const controller = new AbortController();
    setState({ status: "loading", data: null, error: null });

    fetch(url, { signal: controller.signal })
      .then((res) => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then((data) => setState({ status: "success", data, error: null }))
      .catch((error) => {
        if (error.name === "AbortError") return;
        setState({ status: "error", data: null, error });
      });

    return () => controller.abort();
  }, [url]);

  return state;
}
```

```jsx
const { status, data, error } = useFetch(`/api/users/${id}`);
if (status === "loading") return <Spinner />;
if (status === "error") return <p>{error.message}</p>;
```

Uses a single **`status`** value rather than several booleans ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)), checks `res.ok` ([`../00-setup/00-javascript-for-react.md`](../00-setup/00-javascript-for-react.md)), and aborts stale requests. It still lacks caching, deduplication, retries, and refetching, which is why [`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md) exists.

---

## Choosing between a hook, a library, and a platform feature

| Need | Consider |
|------|----------|
| Debounce input | `useDebounce`, or `useDeferredValue` for render priority |
| Persisted UI preference | `useLocalStorage` (or a store with persistence middleware, [`../13-state-management/03-zustand.md`](../13-state-management/03-zustand.md)) |
| Data fetching | A data library, not `useFetch` |
| Responsive layout | CSS first; `useMediaQuery` when the tree must change |
| Dropdowns, dialogs, tooltips | An accessible primitive library |

---

## Testing recipes

Hooks can be tested in isolation with `renderHook` and fake timers (`vi.useFakeTimers()`) for `useDebounce` and `useInterval`; see [`../18-testing-and-debugging/03-hook-testing.md`](../18-testing-and-debugging/03-hook-testing.md).

---

## Common mistakes

- **Missing cleanup** — leaked timers and listeners (`useDebounce`, `useInterval`, `useOnClickOutside` all clean up).
- **Capturing a stale callback in an interval or listener** — keep the latest in a ref.
- **Accessing `window` or `localStorage` during render on the server** — guard it, or read after mount.
- **Passing unstable callbacks or options as dependencies** — causes needless re-subscribing.
- **Treating `useFetch` as production-ready** — add a data library once you need caching or retries.
- **Click-outside without keyboard support** — also handle Escape and focus.
- **Storing sensitive data in `localStorage`** — it's accessible to any script on the page.

## Quick summary

- Recipes: `useToggle`, `useDebounce`, `useLocalStorage`, `usePrevious`, `useMediaQuery`, `useOnClickOutside`, `useInterval`, `useFetch`
- Prefer the updater form and lazy initializers inside hooks
- Always clean up timers, listeners, and requests
- Use a ref to hold the latest callback when a timer or listener must not re-subscribe
- Guard browser-only APIs for server rendering
- Reach for a library when a recipe grows into caching, accessibility, or cross-tab concerns

## Next

You've finished the hooks chapter. Continue to **[`../04-typescript-with-react/README.md`](../04-typescript-with-react/README.md)** to add types to components, props, events, and hooks.
