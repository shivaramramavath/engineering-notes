# Debounce

Debouncing delays a function until the caller has been **quiet for a given time**. Every new call resets the timer, so a burst of calls produces one execution, after the burst ends.

Typical use: don't fire a search request on every keystroke; fire it once the user pauses typing.

## Prerequisites

- [Closures](../06_closures/02_closure-use-cases.md): the timer lives in a closure
- [Timers](../12_event-loop/03_timers.md)

---

## The Idea

```text
calls:     x x x x x            x x
time:    ──┴─┴─┴─┴─┴──────────────┴─┴──────────►
                      │wait│            │wait│
runs:                   ▲                   ▲
                  once, after quiet    once again
```

Each call cancels the pending timer and starts a new one. Only the last call's timer survives to fire.

---

## Minimal Implementation

```js
function debounce(fn, wait) {
  let timer;
  return function (...args) {
    clearTimeout(timer);
    timer = setTimeout(() => fn.apply(this, args), wait);
  };
}

const onInput = debounce((e) => search(e.target.value), 300);
input.addEventListener('input', onInput);
```

Why this shape:

- `timer` persists between calls through the closure.
- A **regular function** (not an arrow) as the returned wrapper keeps `this`, so debounced methods and event handlers still see the right receiver. The inner arrow function forwards it.
- `...args` forwards the **latest** arguments. Earlier calls are discarded.

---

## A More Complete Version

Real code usually needs `cancel` (component unmount, navigation), `flush` (run now, e.g. before leaving a page), and optionally a **leading** edge (fire immediately on the first call, ignore the rest of the burst).

```js
function debounce(fn, wait = 0, { leading = false, trailing = true } = {}) {
  let timer = null;
  let lastArgs, lastThis, result;

  function invoke() {
    const args = lastArgs, ctx = lastThis;
    lastArgs = lastThis = undefined;
    result = fn.apply(ctx, args);
    return result;
  }

  function debounced(...args) {
    lastArgs = args;
    lastThis = this;
    const callNow = leading && timer === null;

    clearTimeout(timer);
    timer = setTimeout(() => {
      timer = null;
      if (trailing && lastArgs) invoke();   // lastArgs is cleared if leading already ran it
    }, wait);

    if (callNow) invoke();
    return result;                           // value of the most recent execution, if any
  }

  debounced.cancel = () => {
    clearTimeout(timer);
    timer = null;
    lastArgs = lastThis = undefined;
  };

  debounced.flush = () => {
    if (timer === null) return result;
    clearTimeout(timer);
    timer = null;
    return lastArgs ? invoke() : result;
  };

  return debounced;
}
```

| Option | Behavior |
|---|---|
| `trailing` (default) | Run after the quiet period ends |
| `leading` | Run on the first call of a burst |
| both | Run at the start and again at the end if more calls arrived |

Note that a debounced function **cannot return the real result** of a later execution to the caller, since the work happens after the call returns. Treat it as fire-and-forget; use callbacks or state for results.

---

## When to Use It

| Scenario | Why debounce fits |
|---|---|
| Search-as-you-type | Only the final query matters |
| Autosave | Save after the user stops editing |
| Window `resize` handling | Recompute layout once the resize settles |
| Validating a field | Don't flash errors mid-typing |

Use **throttle** instead when you need regular updates *during* continuous activity (scrolling, dragging). See [Throttle](./02_throttle.md).

| | Debounce | Throttle |
|---|---|---|
| Fires | After activity stops | At most once per interval, during activity |
| Continuous input for 10s | 1 call (at the end) | ~10s ÷ interval calls |
| Good for | Final value matters | Ongoing feedback matters |

---

## Async Gotchas

Debouncing limits **when you start** requests, not their completion order. Responses can arrive out of order, so an old slow response can overwrite a newer one.

```js
let controller;

const search = debounce(async (query) => {
  controller?.abort();                         // cancel the in-flight request
  controller = new AbortController();
  try {
    const res = await fetch(`/api/search?q=${encodeURIComponent(query)}`, { signal: controller.signal });
    render(await res.json());
  } catch (err) {
    if (err.name !== 'AbortError') throw err;  // aborts are expected
  }
}, 300);
```

See [Cancellation and abort](../11_asynchronous-javascript/08_cancellation-and-abort.md).

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Creating the debounced function inside the handler/render on every call → each call gets its own timer, nothing is debounced | Create it **once** (module scope, constructor, or memoized in your framework) |
| Using an arrow function for the wrapper and losing `this` | Use `function` for the outer wrapper |
| Forgetting `cancel()` on teardown → callback fires after unmount | Call `cancel()` in cleanup |
| Expecting a return value | Debounce is fire-and-forget |
| Too long a delay → UI feels laggy | ~150–400ms is common for typing; tune with real users |
| Debouncing something that must run for every event (analytics clicks) | Don't debounce; batch instead |

---

## Testing

Use fake timers; no real waiting ([Mocking](../21_testing/05_mocking.md), [Testing patterns](../21_testing/06_testing-patterns.md)).

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('fires once with the latest args after the quiet period', () => {
  const fn = vi.fn();
  const d = debounce(fn, 300);

  d('a'); d('b'); d('c');
  vi.advanceTimersByTime(299);
  expect(fn).not.toHaveBeenCalled();

  vi.advanceTimersByTime(1);
  expect(fn).toHaveBeenCalledOnce();
  expect(fn).toHaveBeenCalledWith('c');
});

it('does not fire after cancel', () => {
  const fn = vi.fn();
  const d = debounce(fn, 300);
  d(); d.cancel();
  vi.advanceTimersByTime(1000);
  expect(fn).not.toHaveBeenCalled();
});
```

---

## Quick Summary

- Debounce = **run after calls stop for `wait` ms**; each call resets the timer.
- Built from a closure holding a timer; preserve `this` and the latest arguments.
- Provide `cancel` and `flush` for real-world lifecycle needs; add `leading` when you need immediate response.
- Pair with `AbortController` to avoid stale async responses.
- Create the debounced function once.

**Next:** [Throttle](./02_throttle.md)
