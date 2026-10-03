# Browser Performance

In the browser, performance comes down to three questions: how fast does the page **load**, how quickly does it **respond** to input, and how **smoothly** does it render and animate? This file covers Core Web Vitals, the loading path, the rendering pipeline, and the JavaScript habits that keep the main thread free.

Related: [DOM](../14_dom-and-browser/01_dom.md), [Observers](../14_dom-and-browser/07_observers.md), [Web Workers](../17_concurrency-and-parallelism/02_web-workers.md), [Bundlers and Tree Shaking](../13_modules/04_bundlers-and-tree-shaking.md).

## Core Web Vitals

Google's user-centric metrics. Thresholds apply to the **75th percentile** of real page loads, measured separately for mobile and desktop.

| Metric | Measures | Good | Needs improvement | Poor |
|--------|----------|------|-------------------|------|
| **LCP** (Largest Contentful Paint) | Loading: when the main content appears | ≤ 2.5 s | ≤ 4 s | > 4 s |
| **INP** (Interaction to Next Paint) | Responsiveness: delay from input to the next paint, across the page's life | ≤ 200 ms | ≤ 500 ms | > 500 ms |
| **CLS** (Cumulative Layout Shift) | Visual stability: unexpected layout movement | ≤ 0.1 | ≤ 0.25 | > 0.25 |

Other useful metrics:

| Metric | Meaning |
|--------|---------|
| **TTFB** (Time to First Byte) | Server and network start-up time |
| **FCP** (First Contentful Paint) | First text or image appears |
| **TBT** (Total Blocking Time, lab) | Sum of long-task time between FCP and interactivity: a lab proxy for responsiveness |
| **Long tasks** | Main-thread tasks over 50 ms |

Thresholds and metrics evolve (INP replaced First Input Delay in 2024), so check https://web.dev/vitals for current definitions.

### Measuring in the field

```js
import { onLCP, onINP, onCLS } from 'web-vitals';

function send(metric) {
  navigator.sendBeacon('/analytics', JSON.stringify({
    name: metric.name, value: metric.value, id: metric.id, rating: metric.rating,
  }));
}

onLCP(send);
onINP(send);
onCLS(send);
```

Or use the browser APIs directly:

```js
new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) console.log('LCP candidate:', entry.startTime, entry.element);
}).observe({ type: 'largest-contentful-paint', buffered: true });

new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) console.warn('Long task:', entry.duration, 'ms');
}).observe({ type: 'longtask', buffered: true });
```

## Loading performance

### The critical path

```
DNS → TCP → TLS → request → TTFB → HTML parse → (CSS, blocking JS) → render → LCP element loads → paint
```

Everything that blocks rendering delays the user. Shorten the path and shrink what is on it.

### Network

| Technique | Effect |
|-----------|--------|
| **HTTP/2 or HTTP/3** | Multiplexing, header compression, faster connection setup |
| **CDN** | Serve static assets near users |
| **Compression** (Brotli, gzip) | 60 to 80% smaller text assets |
| **Caching headers** | `Cache-Control: public, max-age=31536000, immutable` for fingerprinted assets (`app.3f9a1c.js`) |
| **`ETag` / `Last-Modified`** | Cheap revalidation for HTML and API responses |
| **Fewer requests and fewer origins** | Less connection overhead |
| **Preconnect** to needed origins | `<link rel="preconnect" href="https://cdn.example.com" crossorigin>` |
| **Modern image/font formats** | AVIF/WebP, WOFF2 |

### Resource hints

```html
<link rel="preload" href="/hero.avif" as="image" fetchpriority="high">
<link rel="preload" href="/fonts/inter.woff2" as="font" type="font/woff2" crossorigin>
<link rel="preconnect" href="https://api.example.com">
<link rel="dns-prefetch" href="https://analytics.example.com">
<link rel="prefetch" href="/next-page.js">           <!-- low priority, for likely next navigation -->
<link rel="modulepreload" href="/app.js">
```

Preload only what the **current** page needs immediately; overusing preload delays other resources.

### Scripts: do not block the parser

```html
<script src="analytics.js" async></script>      <!-- download in parallel; run as soon as ready, any order -->
<script src="app.js" defer></script>            <!-- download in parallel; run after parsing, in order -->
<script type="module" src="main.js"></script>   <!-- deferred by default -->
```

A plain `<script src>` in the `<head>` blocks HTML parsing. Prefer `defer` or modules.

### Reduce JavaScript

JavaScript is the most expensive byte on the web: it must be downloaded, parsed, compiled, and executed.

| Tactic | How |
|--------|-----|
| **Code splitting** | `import()` per route or feature so users download only what they need |
| **Tree shaking** | ES modules plus a bundler remove unused exports (see [Bundlers and Tree Shaking](../13_modules/04_bundlers-and-tree-shaking.md)) |
| **Minify and compress** | Terser/esbuild plus Brotli |
| **Audit dependencies** | Replace heavy libraries (for example Moment.js) with lighter ones; use bundle analyzers (`source-map-explorer`, `rollup-plugin-visualizer`, `webpack-bundle-analyzer`) |
| **Lazy-load below-the-fold or rarely used code** | Load on interaction, visibility, or idle |
| **Defer third-party scripts** | Tag managers, chat widgets, and ads often dominate main-thread time |
| **Target modern browsers** | Ship less transpiled and polyfilled code |

```js
// Load a heavy module only when needed
button.addEventListener('click', async () => {
  const { openEditor } = await import('./editor.js');
  openEditor();
});
```

### Images, video, fonts

```html
<img
  src="photo-800.avif"
  srcset="photo-400.avif 400w, photo-800.avif 800w, photo-1600.avif 1600w"
  sizes="(max-width: 600px) 100vw, 800px"
  width="800" height="600"
  loading="lazy" decoding="async"
  alt="Description">

<!-- The LCP image: do NOT lazy-load; raise its priority -->
<img src="hero.avif" width="1200" height="600" fetchpriority="high" alt="Hero">
```

| Tip | Why |
|-----|-----|
| Always set `width` and `height` (or `aspect-ratio`) | Reserves space; prevents layout shift |
| `loading="lazy"` for offscreen images and iframes | Saves bandwidth and speeds up the initial load |
| Responsive `srcset` and `sizes` | Do not send desktop images to phones |
| Use AVIF/WebP and compress | Often 30 to 70% smaller than JPEG/PNG |
| Fonts: `font-display: swap`, subset, preload critical files | Avoids invisible text and layout shift |
| Prefer video over animated GIF | Much smaller |

### CSS

- Inline **critical CSS** for the first screen; load the rest asynchronously
- Remove unused CSS (coverage panel, PurgeCSS)
- Avoid very deep or complex selectors in huge stylesheets only when profiles show style recalculation cost

### Rendering strategy

| Approach | Strength | Weakness |
|----------|----------|----------|
| **Client-side rendering (CSR)** | Simple hosting, rich interactivity | Blank until JS runs; slower LCP |
| **Server-side rendering (SSR)** | Fast first paint, SEO | Server cost; hydration cost |
| **Static generation (SSG)** | Fastest, cacheable | Rebuilds for changes |
| **Streaming SSR / partial hydration / islands** | Early content; less JS to hydrate | More architecture |

Pick based on the page type; content pages benefit most from server-rendered HTML.

### Caching with a service worker

A service worker can serve repeat visits from cache and make the app load offline. Use stale-while-revalidate for assets and API data where staleness is acceptable (see [Web Workers](../17_concurrency-and-parallelism/02_web-workers.md)).

## The rendering pipeline

To show a frame the browser runs these steps, ideally within **16.7 ms** (60 fps; less on 120 Hz screens):

```
JavaScript → Style → Layout → Paint → Composite
              │         │        │         │
              │         │        │         └─ GPU combines layers (cheap)
              │         │        └─ draw pixels into layers
              │         └─ compute size and position of every box (expensive)
              └─ which CSS rules apply to which elements
```

| Changing | Triggers |
|----------|----------|
| `width`, `height`, `margin`, `top`, `left`, `font-size`, adding or removing nodes | **Layout**, paint, composite |
| `color`, `background`, `box-shadow`, `visibility` | Paint, composite |
| `transform`, `opacity` | Composite only (cheapest; can run on the GPU/compositor thread) |

Animate **`transform` and `opacity`**, not `top/left/width/height`:

```css
/* Cheap: composited */
.card { transition: transform 200ms, opacity 200ms; }
.card:hover { transform: translateY(-4px); }

/* Expensive: triggers layout each frame */
.card:hover { top: -4px; }
```

`will-change: transform` hints that an element will animate, but each promoted layer uses GPU memory; use it sparingly and remove it when done.

### Layout thrashing

Reading layout values (`offsetHeight`, `getBoundingClientRect()`, `scrollTop`, `getComputedStyle`) after writing styles forces the browser to recalculate layout **synchronously** ("forced reflow"). Alternating read and write in a loop makes it happen on every iteration.

```js
// Thrashing: write, read, write, read...
for (const el of items) {
  el.style.width = el.parentNode.offsetWidth / 2 + 'px';   // read forces layout after the previous write
}

// Batch: all reads, then all writes
const widths = items.map((el) => el.parentNode.offsetWidth);   // reads
items.forEach((el, i) => { el.style.width = widths[i] / 2 + 'px'; });   // writes
```

### Schedule visual work with `requestAnimationFrame`

```js
function animate(time) {
  update(time);
  draw();
  requestAnimationFrame(animate);       // aligned with the browser's paint
}
requestAnimationFrame(animate);

// Batch DOM writes for the next frame
let scheduled = false;
function scheduleUpdate() {
  if (scheduled) return;
  scheduled = true;
  requestAnimationFrame(() => { scheduled = false; applyUpdates(); });
}
```

Do not use `setInterval` for animation.

### DOM efficiency

```js
// Slow: many separate DOM updates, each possibly triggering layout
for (const text of items) {
  const li = document.createElement('li');
  li.textContent = text;
  list.appendChild(li);
}

// Better: build off-DOM and attach once
const frag = document.createDocumentFragment();
for (const text of items) {
  const li = document.createElement('li');
  li.textContent = text;
  frag.appendChild(li);
}
list.appendChild(frag);

// Or replace children in one call
list.replaceChildren(...items.map((t) => Object.assign(document.createElement('li'), { textContent: t })));
```

| Tip | Why |
|-----|-----|
| Keep the DOM small (aim for under a few thousand nodes) | Layout and style cost scale with node count |
| **Virtualize** long lists (render only visible rows) | Constant DOM size for huge lists |
| Use **event delegation** (see [Event Delegation](../14_dom-and-browser/05_event-delegation.md)) | Fewer listeners |
| Use `textContent` over `innerHTML` for plain text | Faster and safer |
| Use CSS `content-visibility: auto` for offscreen sections | Skips rendering work until near the viewport |
| Use `contain: layout paint` on independent widgets | Limits the scope of layout recalculation |
| Avoid `display: none` toggling of large subtrees in loops | Triggers layout; batch it |

### Layout shift (CLS) fixes

- Reserve space for images, ads, embeds, and iframes (`width`/`height`, `aspect-ratio`, `min-height`)
- Do not insert content above existing content unless triggered by user input
- Use `font-display: optional` or `swap` with size-adjusted fallback fonts
- Animate with `transform` instead of properties that move other content

## Responsiveness: keep the main thread free

Input handlers run on the main thread. Anything long delays the next paint, hurting **INP**.

### Break up long tasks

```js
// One long task blocks input for hundreds of ms
function processAll(items) {
  for (const item of items) heavy(item);
}

// Yield periodically so the browser can handle input and paint
async function processChunked(items, chunk = 50) {
  for (let i = 0; i < items.length; i++) {
    heavy(items[i]);
    if (i % chunk === 0) await yieldToMain();
  }
}

function yieldToMain() {
  if (globalThis.scheduler?.yield) return scheduler.yield();   // prioritized continuation (where supported)
  return new Promise((r) => setTimeout(r, 0));
}
```

Other scheduling tools: `requestIdleCallback` for low-priority work, `scheduler.postTask()` with priorities, and `isInputPending()` in some browsers.

### Move heavy work off the main thread

Use a **Web Worker** for parsing, compression, search indexing, image processing, and large computations ([Web Workers](../17_concurrency-and-parallelism/02_web-workers.md)). Use `OffscreenCanvas` for canvas rendering in a worker.

### Handlers

```js
// Debounce: wait until the user stops (search boxes, resize end)
function debounce(fn, ms) {
  let t;
  return (...args) => { clearTimeout(t); t = setTimeout(() => fn(...args), ms); };
}

// Throttle: at most once per interval (scroll, mousemove)
function throttle(fn, ms) {
  let last = 0;
  return (...args) => {
    const now = performance.now();
    if (now - last >= ms) { last = now; fn(...args); }
  };
}

input.addEventListener('input', debounce(search, 250));
```

See [Debounce](../23_real-world-patterns/01_debounce.md) and [Throttle](../23_real-world-patterns/02_throttle.md).

Mark scroll and touch listeners **passive** when they do not call `preventDefault()`:

```js
window.addEventListener('scroll', onScroll, { passive: true });
```

Prefer **IntersectionObserver** over scroll listeners for visibility checks (lazy loading, infinite scroll, analytics) and **ResizeObserver** over polling sizes ([Observers](../14_dom-and-browser/07_observers.md)).

### Give immediate feedback

Show a pressed state, spinner, or optimistic update right away, then do the heavy work after the next paint:

```js
button.addEventListener('click', () => {
  button.classList.add('loading');                  // visible immediately
  requestAnimationFrame(() => setTimeout(doHeavyWork, 0));
});
```

## Framework-level habits (React, Vue, and others)

| Habit | Why |
|-------|-----|
| Avoid unnecessary re-renders (memoization, stable props, state close to where it is used) | Less work per update |
| Virtualize long lists (react-window, TanStack Virtual) | Constant rendering cost |
| Code-split routes (`React.lazy`, dynamic imports) | Smaller initial bundle |
| Keep expensive computation out of render (memo, workers) | Smooth interactions |
| Use keys properly in lists | Correct, minimal DOM updates |
| Measure with framework devtools profilers before adding memoization everywhere | Memoization has its own cost |

## Memory in the browser

Leaks and large heaps slow pages and crash tabs, especially on phones. Clean up listeners, observers, and timers; revoke object URLs; release large buffers. See [Memory Leaks](../18_memory-and-garbage-collection/03_memory-leaks.md).

## Storage and network calls

- Cache API responses (HTTP caching, service worker, in-memory with TTL)
- Batch requests, avoid waterfalls (do independent requests in parallel with `Promise.all`)
- Use `fetch` with `keepalive` or `navigator.sendBeacon` for analytics on unload
- Prefer `IndexedDB` (async) over `localStorage` (synchronous, blocks the main thread) for large or frequent writes
- Compress and paginate API payloads; request only needed fields
- Use `AbortController` to cancel stale requests (type-ahead search)

```js
// Avoid request waterfalls
const user = await getUser();
const posts = await getPosts();          // waits for user first, though independent

const [user2, posts2] = await Promise.all([getUser(), getPosts()]);
```

## Testing on realistic conditions

- Throttle **CPU** (4x to 6x) and **network** (Fast/Slow 4G) in DevTools
- Test on real mid-range Android devices, not just a fast laptop
- Run **Lighthouse** and **WebPageTest** for lab audits; track **CrUX**/RUM for field data
- Check with a cold cache and a warm cache
- Add performance budgets in CI (bundle size, Lighthouse CI)

## Quick wins checklist

| Area | Check |
|------|-------|
| Server | Fast TTFB, HTTP/2+, CDN, Brotli |
| HTML | Server-rendered or static for content pages; no render-blocking scripts |
| JS | Code split, tree shaken, `defer`/modules, third-party scripts audited |
| CSS | Critical CSS inline; unused CSS removed |
| Images | AVIF/WebP, `srcset`, `width`/`height`, lazy loading except the LCP image |
| Fonts | WOFF2, subset, `font-display`, preload critical fonts |
| Caching | Long-lived immutable caching for fingerprinted assets |
| Rendering | Animate `transform`/`opacity`; batch DOM reads and writes |
| Responsiveness | No tasks over 50 ms; heavy work in workers; passive listeners |
| Stability | Reserved space for media, ads, and embeds |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Lazy-loading the LCP image | Delays the main content | `fetchpriority="high"`, no lazy |
| Images without dimensions | Layout shift | `width`/`height` or `aspect-ratio` |
| Large JS bundles | Slow parse and execute on phones | Split, shake, audit |
| Render-blocking third-party scripts | Delays everything | `async`/`defer`, load late |
| Animating layout properties | Layout and paint every frame | `transform` and `opacity` |
| Reading layout in loops after writes | Forced reflow (thrashing) | Batch reads then writes |
| `setInterval` animations | Not synced with paint | `requestAnimationFrame` or CSS |
| Heavy work in input handlers | Poor INP | Yield, workers, debounce |
| Synchronous `localStorage` in hot paths | Blocks the main thread | IndexedDB, or batch writes |
| Preloading everything | Competes with critical resources | Preload only the critical few |
| Testing only on fast hardware | Misses real-user pain | Throttle; use real devices and field data |
| Memoizing everything in a framework | Overhead and complexity | Profile first |

## Key takeaways

- Track Core Web Vitals (LCP, INP, CLS) with field data, and debug with lab tools
- Loading: shrink and prioritize the critical path (compression, CDN, caching, `defer`, code splitting, optimized images and fonts)
- Rendering: animate `transform` and `opacity`, avoid layout thrashing, keep the DOM small, virtualize long lists
- Responsiveness: keep main-thread tasks under 50 ms; yield, defer, or move work to Web Workers
- Reserve space for anything that loads late to prevent layout shift
- Test under throttled CPU and network, on real devices, and monitor real users

**Next:** [Node Performance](./04_node-performance.md)