# Throttle

Throttling guarantees a function runs **at most once per interval**, no matter how often it's called. Where debounce waits for silence, throttle gives you a steady, capped rate *during* continuous activity.

Typical use: scroll, `mousemove`, and drag handlers that would otherwise run hundreds of times per second.

## Prerequisites

- [Debounce](./01_debounce.md): the contrast is the main way to remember both
- [Closures](../06_closures/02_closure-use-cases.md), [Timers](../12_event-loop/03_timers.md)

---

## The Idea

```text
calls:    x x x x x x x x x x x x x x
time:   ──┴───────────┴───────────┴────►
runs:     ▲           ▲           ▲
          │← interval →│← interval →│
```

The first call runs immediately (leading edge). Calls within the interval are dropped, or the *latest* one is deferred to the end of the interval (trailing edge) so the final state isn't lost.

---

## Implementation (leading + trailing)

```js
function throttle(fn, wait) {
  let lastCall = -Infinity;
  let timer = null;
  let lastArgs, lastThis;

  return function throttled(...args) {
    const now = Date.now();
    lastArgs = args;
    lastThis = this;
    const remaining = wait - (now - lastCall);

    if (remaining <= 0) {
      // Interval has passed: run now
      clearTimeout(timer);
      timer = null;
      lastCall = now;
      fn.apply(this, args);
      lastArgs = lastThis = undefined;
    } else if (timer === null) {
      // Inside the interval: schedule one trailing run with the latest args
      timer = setTimeout(() => {
        timer = null;
        lastCall = Date.now();
        fn.apply(lastThis, lastArgs);
        lastArgs = lastThis = undefined;
      }, remaining);
    }
  };
}

window.addEventListener('scroll', throttle(updateScrollProgress, 100));
```

Behavior walkthrough with `wait = 100`:

1. `t=0` call → runs immediately.
2. `t=30`, `t=60` calls → only the timer is set once; args update to the latest.
3. `t=100` → trailing call fires with the `t=60` arguments.

The trailing run matters: without it, the last scroll position could be dropped and your UI would end up stale.

### Simplest version (leading only)

```js
function throttleLeading(fn, wait) {
  let last = 0;
  return function (...args) {
    const now = Date.now();
    if (now - last >= wait) {
      last = now;
      return fn.apply(this, args);
    }
  };
}
```

Fine when dropping the final event is acceptable (e.g. rate-limiting a button click).

---

## Choosing the Right Tool

| Need | Use |
|---|---|
| Run once the user *stops* (search box, autosave) | [Debounce](./01_debounce.md) |
| Regular updates *while* the user acts (scroll, drag, resize feedback) | Throttle |
| Update in sync with painting | `requestAnimationFrame` |
| React to visibility/intersection | `IntersectionObserver` instead of scroll handlers |
| Cap calls to an API with a limit | [Rate limiting](./09_rate-limiting.md) |

### `requestAnimationFrame` throttling

For visual work, throttle to the display's frame rate rather than a fixed ms:

```js
function rafThrottle(fn) {
  let scheduled = false;
  let lastArgs;
  return function (...args) {
    lastArgs = args;
    if (scheduled) return;
    scheduled = true;
    requestAnimationFrame(() => {
      scheduled = false;
      fn.apply(this, lastArgs);
    });
  };
}

window.addEventListener('mousemove', rafThrottle(drawCursorTrail));
```

This runs at most once per frame and pauses in background tabs. See [Observers](../14_dom-and-browser/07_observers.md) for `IntersectionObserver`/`ResizeObserver`, which often remove the need to throttle scroll/resize handlers at all.

---

## Mistakes to Avoid

| Mistake | Why it hurts | Fix |
|---|---|---|
| Creating the throttled function on each render/call | Every call has fresh state, so nothing is throttled | Create once |
| Dropping the trailing call | UI ends in a stale state | Use leading + trailing |
| Throttling work that must process every event | Events are lost | Batch/queue instead |
| Using throttle where debounce is right | You fire many useless intermediate requests | Pick by "final value vs ongoing feedback" |
| Heavy work inside the handler even when throttled | Still janky | Do less work; use `requestAnimationFrame` or `passive` listeners |
| `Date.now()` jumps (system clock changes) | Odd timing | Use `performance.now()` for monotonic time |

For scroll/touch listeners, also consider `{ passive: true }` so the browser doesn't wait on your handler before scrolling:

```js
window.addEventListener('scroll', handler, { passive: true });
```

---

## Testing

Fake timers control both `setTimeout` and `Date.now`.

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('runs immediately, then at most once per interval with the latest args', () => {
  const fn = vi.fn();
  const t = throttle(fn, 100);

  t(1);                                  // runs now
  t(2); t(3);                            // queued; latest wins
  expect(fn).toHaveBeenCalledTimes(1);
  expect(fn).toHaveBeenLastCalledWith(1);

  vi.advanceTimersByTime(100);           // trailing run
  expect(fn).toHaveBeenCalledTimes(2);
  expect(fn).toHaveBeenLastCalledWith(3);
});
```

---

## Quick Summary

- Throttle = **at most once per interval**; debounce = once after silence.
- Implement with leading + trailing edges so the last event isn't lost.
- Use `requestAnimationFrame` for visual updates and observers instead of scroll/resize polling when possible.
- Create the throttled function once; clean up listeners when done.
- Test with fake timers.

**Next:** [Retry](./03_retry.md)
