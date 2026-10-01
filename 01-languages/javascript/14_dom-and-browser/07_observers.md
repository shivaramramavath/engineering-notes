# Observers

Observers let the browser **notify you** about changes (visibility, DOM mutations, size changes, performance entries) **without polling** or expensive scroll/resize handlers. Callbacks are batched and run efficiently.

| Observer | Watches | Typical use |
|----------|---------|-------------|
| `IntersectionObserver` | element visibility in a viewport/ancestor | lazy loading, infinite scroll, analytics, animations on scroll |
| `MutationObserver` | DOM tree changes | reacting to third-party DOM changes, waiting for elements |
| `ResizeObserver` | element size changes | responsive components, charts, text fitting |
| `PerformanceObserver` | performance entries | long tasks, LCP, CLS, resource timing |

All follow the same shape:

```js
const observer = new XObserver(callback, options);
observer.observe(target);
observer.unobserve(target);
observer.disconnect();            // stop everything: do this in cleanup
```

## IntersectionObserver

```js
const observer = new IntersectionObserver((entries, obs) => {
  for (const entry of entries) {
    if (entry.isIntersecting) {
      entry.target.classList.add("visible");
      obs.unobserve(entry.target);                // run once per element
    }
  }
}, {
  root: null,                      // viewport (or an ancestor scroll container element)
  rootMargin: "200px 0px",         // grow/shrink the root box (preload before entering view)
  threshold: [0, 0.25, 1],         // visibility ratios that trigger the callback
});

document.querySelectorAll(".reveal").forEach((el) => observer.observe(el));
```

### Entry fields

| Field | Meaning |
|-------|---------|
| `isIntersecting` | currently crossing the threshold |
| `intersectionRatio` | 0 to 1 visible fraction |
| `boundingClientRect`, `intersectionRect`, `rootBounds` | geometry |
| `target` | the element |
| `time` | timestamp |

### Lazy-loading images

```html
<img data-src="/photos/1.jpg" alt="..." width="800" height="600">
```

```js
const lazy = new IntersectionObserver((entries, obs) => {
  entries.filter((e) => e.isIntersecting).forEach(({ target }) => {
    target.src = target.dataset.src;
    obs.unobserve(target);
  });
}, { rootMargin: "300px" });
document.querySelectorAll("img[data-src]").forEach((img) => lazy.observe(img));
```

Native alternative: `<img loading="lazy">` and `<iframe loading="lazy">`: use this first.

### Infinite scroll

```js
const sentinel = document.querySelector("#sentinel");     // empty element after the list
new IntersectionObserver(async ([entry]) => {
  if (entry.isIntersecting) await loadNextPage();
}, { rootMargin: "400px" }).observe(sentinel);
```

### Sticky header detection

```js
new IntersectionObserver(([e]) => header.classList.toggle("stuck", e.intersectionRatio < 1), { threshold: [1] })
  .observe(topSentinel);
```

### Notes

- Callbacks run asynchronously (not per scroll event), so they are cheap
- `isIntersecting` is `true` for zero-area elements touching the root
- Thresholds are checked as visibility **crosses** them
- Newer options: `IntersectionObserver` v2 (`trackVisibility`) for occlusion detection (limited use)

## MutationObserver

```js
const observer = new MutationObserver((mutations) => {
  for (const m of mutations) {
    if (m.type === "childList") {
      m.addedNodes.forEach((n) => n.nodeType === 1 && enhance(n));
      m.removedNodes.forEach(cleanup);
    } else if (m.type === "attributes") {
      console.log(m.attributeName, m.oldValue, m.target.getAttribute(m.attributeName));
    } else if (m.type === "characterData") {
      console.log("text changed", m.target.data);
    }
  }
});

observer.observe(document.body, {
  childList: true,          // added/removed children
  subtree: true,            // include all descendants
  attributes: true,         // attribute changes
  attributeFilter: ["class", "data-state"],   // limit which attributes
  attributeOldValue: true,
  characterData: true,      // text node changes
});

observer.takeRecords();     // fetch pending mutations synchronously
observer.disconnect();
```

Facts:

- Callbacks are delivered as **microtasks**, after the current script, batching all mutations
- Changing the DOM **inside** the callback can retrigger it: avoid loops (disconnect/reconnect, or guard)
- Observing `document.body` with `subtree` plus `attributes` on a busy page is expensive: **narrow** the target and filters
- Use for third-party widgets, enhancing dynamically added elements, "wait for element" helpers, undo/redo recording

```js
function onElementAdded(selector, callback, root = document) {
  root.querySelectorAll(selector).forEach(callback);        // existing
  const mo = new MutationObserver((records) => {
    for (const r of records) for (const n of r.addedNodes) {
      if (n.nodeType !== 1) continue;
      if (n.matches(selector)) callback(n);
      n.querySelectorAll?.(selector).forEach(callback);
    }
  });
  mo.observe(root, { childList: true, subtree: true });
  return () => mo.disconnect();
}
```

If you control the code that changes the DOM, use events or direct calls instead of observing it.

## ResizeObserver

```js
const ro = new ResizeObserver((entries) => {
  for (const entry of entries) {
    const { width, height } = entry.contentRect;
    entry.target.classList.toggle("compact", width < 400);
    // entry.borderBoxSize[0].inlineSize, contentBoxSize, devicePixelContentBoxSize
  }
});
ro.observe(container);                         // options: { box: "border-box" }
```

Use for:

- Component-level responsive logic where media queries (viewport-based) are not enough (**CSS container queries** are the better first choice)
- Resizing canvases/charts when their container changes
- Virtual lists and text-fitting

`ResizeObserver loop completed with undelivered notifications` warns when your callback changes sizes that retrigger the observer in the same frame: defer changes with `requestAnimationFrame` or avoid resizing the observed element in the callback.

## PerformanceObserver

```js
new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) console.log(entry.entryType, entry.name, entry.startTime, entry.duration);
}).observe({ type: "longtask", buffered: true });

// Core Web Vitals building blocks
new PerformanceObserver((l) => console.log("LCP", l.getEntries().at(-1).startTime)).observe({ type: "largest-contentful-paint", buffered: true });
new PerformanceObserver((l) => l.getEntries().forEach((e) => !e.hadRecentInput && (cls += e.value))).observe({ type: "layout-shift", buffered: true });
new PerformanceObserver((l) => console.log(l.getEntries())).observe({ type: "event", durationThreshold: 40, buffered: true });   // INP candidates
```

Entry types: `navigation`, `resource`, `paint`, `largest-contentful-paint`, `layout-shift`, `longtask`, `event`, `first-input`, `mark`, `measure`, `element`. Use the `web-vitals` library for production metrics.

## Other observer-like APIs

| API | Purpose |
|-----|---------|
| `ReportingObserver` | deprecations, interventions, CSP violations |
| `MediaQueryList` (`matchMedia(...).addEventListener("change")`) | viewport/preference changes |
| `document.addEventListener("visibilitychange")` | tab visibility |
| `BroadcastChannel`, `storage` event | cross-tab changes |
| `ElementInternals`, `slotchange` event | slot content changes in web components |
| `Navigation` / `popstate` | URL changes |

```js
const mq = matchMedia("(prefers-color-scheme: dark)");
mq.addEventListener("change", (e) => applyTheme(e.matches ? "dark" : "light"));
```

## Cleanup

```js
class Widget {
  #io = new IntersectionObserver(this.#onVisible.bind(this));
  #ro = new ResizeObserver(this.#onResize.bind(this));

  mount(el) { this.el = el; this.#io.observe(el); this.#ro.observe(el); }
  unmount() { this.#io.disconnect(); this.#ro.disconnect(); }   // prevent leaks and ghost callbacks
  #onVisible([entry]) {}
  #onResize([entry]) {}
}
```

Observers hold references to targets and callbacks until disconnected.

## Choosing the right tool

| Need | Use |
|------|-----|
| "Is this element on screen?" | `IntersectionObserver` |
| "Did the DOM change?" | `MutationObserver` (or an event/callback from the code making the change) |
| "Did this box change size?" | `ResizeObserver` or CSS container queries |
| "How slow is my page?" | `PerformanceObserver` |
| "Is the window scrolled?" | `scroll` event (passive) + rAF, or `IntersectionObserver` sentinel |
| Layout depends on viewport width only | CSS media queries |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `scroll` + `getBoundingClientRect` for visibility | Jank | `IntersectionObserver` |
| Not disconnecting observers | Leaks and callbacks on removed elements | `disconnect()` in cleanup |
| Observing `document` with broad `MutationObserver` options | Heavy overhead | Narrow target, `attributeFilter` |
| Mutating observed DOM inside the callback | Infinite loops | Guard, disconnect/reconnect |
| Changing the observed element's size in `ResizeObserver` | Loop warnings, layout churn | Defer with rAF or change something else |
| Missing initial state in `IntersectionObserver` | Callback fires once on observe with current state, then on changes | Handle the first callback |
| Thresholds as a single float for fine tracking | Misses intermediate states | Use an array of thresholds |
| Polyfills not needed today | Extra bytes | Native support is universal in modern browsers |
| Replacing native lazy loading with custom code needlessly | Extra JS | `loading="lazy"` |

## Key takeaways

- Observers replace polling and expensive scroll/resize handlers with batched, async callbacks
- `IntersectionObserver` for visibility, `MutationObserver` for DOM changes, `ResizeObserver` for size, `PerformanceObserver` for metrics
- Narrow what you observe and always `disconnect()` in cleanup
- Prefer native features (`loading="lazy"`, container queries) when they solve the problem

**Next:** [Browser Storage](./08_browser-storage.md)
