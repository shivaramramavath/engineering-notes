# Throttle Pattern in JavaScript

> A production-grade implementation and architectural guide to the throttle pattern in JavaScript, enforcing execution rate limits over high-frequency event streams with leading/trailing guarantees and requestAnimationFrame synchronization.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate / Production-Grade |
| **Prerequisites** | [01_debounce.md](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns/01_debounce.md), [14_dom-and-browser/04_animation-and-timing.md](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser/04_animation-and-timing.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
| **Status** | Production Ready |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In modern web applications, continuous user interactions such as window scrolling, mouse gestures, touch drags, and window resizing can emit hundreds of events per second. If an application performs expensive calculations, DOM queries, or network requests on every individual event:
- The main JavaScript thread becomes saturated, dropping frame rates below 60 FPS (causing noticeable UI stutter and jank).
- Layout thrashing occurs as scroll-position queries (`scrollTop`, `getBoundingClientRect`) repeatedly force the browser to recalculate styles and reflow geometries.
- Network infrastructure and third-party APIs become overwhelmed by unnecessary burst traffic.

The **Throttle** pattern limits the execution of a function to at most **once per specified time window** ($N$ milliseconds), regardless of how many times the caller triggers it. Unlike debounce, which waits for a period of inactivity, throttle guarantees regular, periodic execution throughout continuous activity.

---

## 2. Core Concepts

### Throttle vs. Debounce
```
Events:      |--A-B-C-D-E-F-G-H-I-J-K------------------------> Time
Throttle:    |--[A]---------|--[E]---------|--[I]--------|---> Regular intervals
Debounce:    |--------------------------------------[K]------> Executes after quiet
```

### Leading and Trailing Edges
- **Leading Edge (`leading: true`)**: The function executes immediately on the first event invocation. Subsequent events within the time window are accumulated or ignored until the window closes.
- **Trailing Edge (`trailing: true`)**: When calls occur within the quiet interval, the *final* call's arguments are retained and executed automatically as soon as the throttle window expires.
- **Both Edges Active (Production Standard)**: Executes instantly on the first trigger, then executes once more at the end of the window if additional events arrived during the cooldown.

---

## 3. How It Works Internally

A robust throttle combines two tracking mechanisms:
1. **Timestamp Comparison (`Date.now()` or `performance.now()`)**: Calculates time elapsed since the last execution ($t_{\text{remaining}} = \text{wait} - (\text{now} - \text{lastCallTime})$).
2. **Trailing Timer (`setTimeout`)**: If a call arrives while $t_{\text{remaining}} > 0$, a timer is scheduled for $t_{\text{remaining}}$ to ensure the trailing edge executes with the freshest arguments.

```
                  throttledFn(args)
                          │
          Calculate remaining = wait - (now - lastExec)
                          │
         ┌────────────────┴────────────────┐
         ▼                                 ▼
   remaining <= 0                    remaining > 0
(Window has expired)              (Cooldown is active)
         │                                 │
Cancel pending timer                       │
Execute targetFn(args)                     │
Update lastExec = now                      ▼
                              Schedule trailing timer
                              for `remaining` ms if trailing=true
```

---

## 4. Syntax & API Reference

### Full-Featured Production Implementation
```javascript
/**
 * Creates a throttled function that only invokes `fn` at most once per
 * every `wait` milliseconds.
 *
 * @param {Function} fn - Target function to throttle.
 * @param {number} wait - Cooldown period in milliseconds.
 * @param {Object} [options={}] - Throttle configuration.
 * @param {boolean} [options.leading=true] - Invoke on leading edge.
 * @param {boolean} [options.trailing=true] - Invoke on trailing edge.
 * @returns {Function} Throttled wrapper with cancel and flush methods.
 */
export function throttle(fn, wait = 0, options = {}) {
  let timerId = null;
  let lastArgs = null;
  let lastThis = null;
  let result = undefined;
  let previous = 0;

  const leading = 'leading' in options ? Boolean(options.leading) : true;
  const trailing = 'trailing' in options ? Boolean(options.trailing) : true;

  function invoke(time) {
    const args = lastArgs;
    const thisArg = lastThis;
    lastArgs = null;
    lastThis = null;
    previous = leading ? time : 0;
    result = fn.apply(thisArg, args);
    return result;
  }

  function throttled(...args) {
    const now = Date.now();
    if (!previous && !leading) {
      previous = now;
    }

    const remaining = wait - (now - previous);
    lastArgs = args;
    lastThis = this;

    if (remaining <= 0 || remaining > wait) {
      if (timerId !== null) {
        clearTimeout(timerId);
        timerId = null;
      }
      previous = now;
      return invoke(now);
    }

    if (timerId === null && trailing) {
      timerId = setTimeout(() => {
        timerId = null;
        previous = leading ? Date.now() : 0;
        if (lastArgs) {
          invoke(Date.now());
        }
      }, remaining);
    }

    return result;
  }

  throttled.cancel = function () {
    if (timerId !== null) {
      clearTimeout(timerId);
      timerId = null;
    }
    previous = 0;
    lastArgs = null;
    lastThis = null;
  };

  throttled.flush = function () {
    if (timerId !== null && lastArgs !== null) {
      clearTimeout(timerId);
      timerId = null;
      return invoke(Date.now());
    }
    return result;
  };

  return throttled;
}
```

---

## 5. Practical Examples

### Example 1: Infinite Scroll Listener with Dynamic Page Fetching
```javascript
import { throttle } from './throttle.js';

class InfiniteFeed {
  constructor(container, loadMoreCallback) {
    this.container = container;
    this.loadMore = loadMoreCallback;
    this.isLoading = false;

    // Throttle scroll checks to at most once every 150ms
    this.onScroll = throttle(this.checkScrollPosition.bind(this), 150);
    window.addEventListener('scroll', this.onScroll, { passive: true });
  }

  async checkScrollPosition() {
    if (this.isLoading) return;

    const scrollHeight = document.documentElement.scrollHeight;
    const scrollTop = window.scrollY || document.documentElement.scrollTop;
    const clientHeight = window.innerHeight;

    // Trigger load when scrolled within 300px of bottom
    if (scrollTop + clientHeight >= scrollHeight - 300) {
      this.isLoading = true;
      try {
        await this.loadMore();
      } finally {
        this.isLoading = false;
      }
    }
  }

  destroy() {
    this.onScroll.cancel();
    window.removeEventListener('scroll', this.onScroll);
  }
}
```

### Example 2: High-Performance Canvas Freehand Drawing
```javascript
import { throttle } from './throttle.js';

export function setupDrawingCanvas(canvas) {
  const ctx = canvas.getContext('2d');
  let isDrawing = false;
  let lastX = 0;
  let lastY = 0;

  function drawSegment(x, y) {
    if (!isDrawing) return;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    ctx.lineTo(x, y);
    ctx.stroke();
    lastX = x;
    lastY = y;
  }

  // Throttle mouse moves to ~60 FPS (16ms)
  const throttledDraw = throttle(drawSegment, 16);

  canvas.addEventListener('mousedown', (e) => {
    isDrawing = true;
    lastX = e.offsetX;
    lastY = e.offsetY;
  });

  canvas.addEventListener('mousemove', (e) => {
    if (isDrawing) throttledDraw(e.offsetX, e.offsetY);
  });

  window.addEventListener('mouseup', () => {
    isDrawing = false;
    throttledDraw.cancel();
  });
}
```

### Example 3: Browser UI Animation Sync via `requestAnimationFrame` Throttle
```javascript
/**
 * Throttles an update function to the browser's native monitor refresh rate (60Hz / 120Hz).
 * Ideal for visual style/transform mutations.
 */
export function throttleRaf(fn) {
  let rafId = null;
  let lastArgs = null;
  let lastThis = null;

  function throttled(...args) {
    lastArgs = args;
    lastThis = this;

    if (rafId === null) {
      rafId = requestAnimationFrame(() => {
        rafId = null;
        fn.apply(lastThis, lastArgs);
      });
    }
  }

  throttled.cancel = () => {
    if (rafId !== null) {
      cancelAnimationFrame(rafId);
      rafId = null;
    }
    lastArgs = null;
    lastThis = null;
  };

  return throttled;
}

// Usage in parallax scroll effect
const updateParallax = throttleRaf((scrollY) => {
  const hero = document.querySelector('.hero-bg');
  hero.style.transform = `translate3d(0, ${scrollY * 0.4}px, 0)`;
});

window.addEventListener('scroll', () => updateParallax(window.scrollY), { passive: true });
```

### Example 4: Telemetry Heartbeat & Metrics Throttle
```javascript
import { throttle } from './throttle.js';

export class UserActivityTracker {
  constructor(endpoint) {
    this.endpoint = endpoint;

    // Send activity metrics at most once every 5000ms
    this.sendPing = throttle(this.dispatchBeacon.bind(this), 5000, {
      leading: true,
      trailing: true,
    });

    ['keydown', 'click', 'touchstart'].forEach((eventType) => {
      window.addEventListener(eventType, () => this.sendPing(eventType), { passive: true });
    });
  }

  dispatchBeacon(triggerType) {
    const payload = JSON.stringify({
      timestamp: Date.now(),
      lastAction: triggerType,
      url: window.location.href,
    });

    navigator.sendBeacon(this.endpoint, payload);
  }

  flushAndUnmount() {
    this.sendPing.flush();
    this.sendPing.cancel();
  }
}
```

### Example 5: Window Resize Grid Re-computation
```javascript
import { throttle } from './throttle.js';

export function setupResponsiveGrid(container) {
  function recalculateColumns() {
    const width = container.clientWidth;
    const colCount = Math.max(1, Math.floor(width / 250));
    container.style.setProperty('--grid-columns', String(colCount));
    console.log(`Grid columns recalculated: ${colCount} for width: ${width}px`);
  }

  // Throttle resize events to at most once every 100ms
  const onResize = throttle(recalculateColumns, 100);
  window.addEventListener('resize', onResize);

  return () => {
    onResize.cancel();
    window.removeEventListener('resize', onResize);
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **System Clock Drift & Adjustments**:
   If the operating system clock adjusts backwards (NTP synchronization or user manually setting system time), `now - previous` can produce negative values or exceed `wait`. Robust implementations check `remaining <= 0 || remaining > wait` to recover gracefully.
2. **Missing Trailing Edge**:
   If a user scrolls aggressively and suddenly stops, a naive throttle without a trailing edge ignores the final scroll position. In infinite scroll components, this can result in failing to load content because the final position within the trigger boundary was discarded.
3. **Passive Event Listeners Requirement**:
   When attaching throttled listeners to `scroll` or `touchmove`, always specify `{ passive: true }`. This prevents the browser from blocking the compositor thread while waiting to see if `event.preventDefault()` will be called.
4. **`requestAnimationFrame` in Background Tabs**:
   Browsers throttle `requestAnimationFrame` callbacks to 1–2 executions per second (or suspend them completely) when a browser tab is inactive or hidden. Do not use rAF-based throttling for critical background timers or telemetry dispatches.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Defect | Production Remediation |
| :--- | :--- | :--- |
| Disabling both `leading` and `trailing` | The function will never execute under any circumstances. | Enforce that at least one edge is enabled in configuration validation. |
| Non-passive scroll event listeners | Causes frame drops and jank during touch gestures and scrolling. | Always pass `{ passive: true }` to `addEventListener`. |
| Re-instantiating throttle on each React render | Creates a brand-new timer closure every frame; rate limit is completely lost. | Store the throttled instance in `useCallback` or `useRef`. |
| Using throttle for search autocomplete | Incomplete queries execute every 300ms while user is mid-word. | Use `debounce` for input/typeahead search inputs. |
| Forgetting `.cancel()` on unmount | Trailing timers fire against destroyed components, leading to memory leaks. | Execute `.cancel()` during unmount/cleanup cycles. |
| Synchronous return value dependency | Throttled wrapper returns previous cached value when rate-limited. | Do not design synchronous return-value dependencies around throttled methods. |

---

## 8. Performance Characteristics

- **Compositor Thread Decoupling**: Combining `{ passive: true }` with a 16ms or 50ms throttle allows the browser's GPU compositor to scroll smoothly at 60–120 FPS while offloading heavy business logic to predictable intervals.
- **CPU Cycle Reductions**: In typical scrolling sessions, unthrottled listeners trigger 60–150 times per second. A 100ms throttle caps invocations to 10 per second—an immediate 85% to 93% reduction in JavaScript execution time.
- **Allocation Efficiency**: A persistent throttle wrapper maintains fixed closure references, avoiding allocations inside event loop hot paths.

---

## 9. Engine Internals

### V8 Hidden Classes & Timer Queue Mechanics
- **Context Slot Optimization**: When `throttled()` executes, V8 accesses `previous`, `timerId`, and `lastArgs` via internal context registers (`LdaContextSlot`). Keeping the number of closure variables minimal prevents V8 from creating secondary scope lookup chains.
- **Macrotask Queue vs. Compositor Phase**:
  - `setTimeout(..., wait)` enqueues its callback into the Macrotask queue. The browser executes macrotasks after layout and rendering cycles.
  - `requestAnimationFrame` callbacks execute *immediately before* the browser calculates styles, layouts, and paints. Throttling visual DOM updates via `requestAnimationFrame` guarantees that style modifications are batched directly into the next render frame, eliminating redundant intermediary reflows.

---

## 10. Best Practices & Design Guidelines

1. **Use `throttle` for Ongoing Progress**: Choose throttle when user feedback must be continuous and progressive (e.g. scroll indicators, audio/video playback bars, canvas drawing).
2. **Choose `debounce` for Terminal Actions**: Choose debounce when the execution should only occur after user interaction has finalized (e.g. search suggestions, form field auto-save).
3. **Always Clean Up Resources**: Provide explicit `.cancel()` hooks to clear timer handles and remove event listeners during component unmounts.
4. **Sync Visual DOM Writes with rAF**: Use `requestAnimationFrame` throttling when modifying visual CSS properties or canvas pixels.

---

## 11. Real-World Case Study

### High-Volume Map Viewport Tile Fetcher
**Problem**: An interactive mapping application allowed users to pan and zoom smoothly across global vector maps. On every `pan` event, the application calculated visible geo-coordinates and initiated HTTP requests to fetch map vector tiles. Unconstrained panning generated up to 180 tile requests per second, overloading the backend CDN, saturating browser connection limits (HTTP/1.1 6 connections per domain), and causing severe UI freeze.

**Solution Architecture**:
- Applied a dual-strategy throttle:
  1. Pan visual translation is updated at 60 FPS using CSS `transform: translate3d()` driven by `throttleRaf`.
  2. Data fetching of vector tiles is wrapped with `throttle(fetchTiles, 350, { leading: false, trailing: true })`.
- During fast panning, map graphics transform smoothly on the GPU without making network calls.
- When panning slows down or pauses, the trailing edge triggers a single coordinated bounding-box tile request.

**Impact**:
- Network requests dropped by 88%.
- Tile cache hit ratio on CDN increased from 42% to 94%.
- Panning frame rates remained locked at 60 FPS across low-end mobile devices.

---

## 12. Interview Questions

<details>
<summary>1. How does Throttling differ from Debouncing in execution pattern and use cases?</summary>

Throttling enforces a maximum execution frequency, guaranteeing that the target function executes periodically at most once every $N$ milliseconds throughout continuous activity (e.g. scroll tracking, window resize, game loop updates). Debouncing groups bursty calls together, executing only after a quiet period with no calls has elapsed (e.g. search autocomplete, form input validation).
</details>

<details>
<summary>2. Why does a standard throttle implementation require both a timestamp check and a <code>setTimeout</code> timer?</summary>

A timestamp check alone (`Date.now() - previous > wait`) handles the leading edge and periodic execution during active calls, but it cannot handle the **trailing edge**. If calls stop while the cooldown window is still active, a timestamp-only implementation would discard the final call. The `setTimeout` timer guarantees that the final call's arguments are executed as soon as the remaining cooldown window expires.
</details>

<details>
<summary>3. What is the benefit of throttling with <code>requestAnimationFrame</code> instead of <code>setTimeout(fn, 16)</code>?</summary>

`setTimeout(fn, 16)` runs as a generic macrotask and has no awareness of the browser's display refresh cycle. It may execute right after a frame has rendered, forcing a redundant style calculation or missing the current vsync frame. In contrast, `requestAnimationFrame` is synchronized directly with the display hardware refresh rate (60Hz, 120Hz, ProMotion) and executes immediately before the browser renders the next frame, eliminating screen tearing and redundant layout recalculations.
</details>

<details>
<summary>4. Why should scroll and touch event listeners always be registered with <code>{ passive: true }</code>?</summary>

By default, browsers assume event listeners might call `event.preventDefault()`, which requires the browser to wait for the JavaScript thread to finish executing before scrolling the page. Specifying `{ passive: true }` signals to the browser that the listener will never cancel the event, allowing the compositor thread to scroll the page immediately without waiting for JavaScript execution.
</details>

<details>
<summary>5. What edge case occurs if the user or system clock changes while a throttle is active?</summary>

If the system clock adjusts backwards (e.g. NTP sync), `now - previous` becomes negative, making `remaining` greater than `wait`. Without defensive handling (`remaining > wait`), the function would be blocked from executing for an arbitrary duration until the clock caught up. A defensive throttle checks `if (remaining <= 0 || remaining > wait)` and executes immediately.
</details>

<details>
<summary>6. How do you handle <code>this</code> context and parameter forwarding in a throttle wrapper?</summary>

The wrapper function is declared using a standard `function (...args)` expression (not an arrow function) so that it captures dynamic `this`. It stores `lastThis = this` and `lastArgs = args`, then invokes the target function using `fn.apply(lastThis, lastArgs)`.
</details>

<details>
<summary>7. What happens if both <code>leading: false</code> and <code>trailing: false</code> are passed to a throttle function?</summary>

Neither the immediate first call nor the delayed final call will ever execute. In a production implementation, configuration options should be validated, and attempting to disable both edges should either throw a `TypeError` or default `trailing` back to `true`.
</details>

<details>
<summary>8. How do you implement a <code>flush()</code> method on a throttled function?</summary>

The `flush()` method checks if an active trailing timer is pending (`timerId !== null` and `lastArgs !== null`). If so, it cancels the pending timer, clears `timerId`, immediately invokes the target function with `lastArgs` and `lastThis`, updates `previous = Date.now()`, and returns the invocation result.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Basic Timestamp-Based Throttle
**Specification**: Write a lightweight `throttle(fn, wait)` function that executes immediately on the first call and at most once every `wait` ms thereafter using `Date.now()`.

<details>
<summary>Solution</summary>

```javascript
export function simpleThrottle(fn, wait) {
  let lastTime = 0;

  return function (...args) {
    const now = Date.now();
    if (now - lastTime >= wait) {
      lastTime = now;
      return fn.apply(this, args);
    }
  };
}
```
</details>

### Exercise 2: Build a RAF-Based Scroll Progress Bar
**Specification**: Implement a scroll progress bar updater that tracks percentage scrolled down the page and updates a progress bar width using `requestAnimationFrame` throttling.

<details>
<summary>Solution</summary>

```javascript
export function initScrollProgressBar(progressBarElement) {
  let ticking = false;

  function update() {
    const totalHeight = document.documentElement.scrollHeight - window.innerHeight;
    const progress = totalHeight > 0 ? (window.scrollY / totalHeight) * 100 : 0;
    progressBarElement.style.width = `${Math.min(100, Math.max(0, progress))}%`;
    ticking = false;
  }

  function onScroll() {
    if (!ticking) {
      ticking = true;
      requestAnimationFrame(update);
    }
  }

  window.addEventListener('scroll', onScroll, { passive: true });

  return () => window.removeEventListener('scroll', onScroll);
}
```
</details>
