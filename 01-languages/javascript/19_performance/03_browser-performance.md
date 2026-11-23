# Browser Performance and Core Web Vitals

> The Critical Rendering Path, Layout Thrashing, GPU compositor layers, Core Web Vitals (LCP, INP, CLS), and Long Animation Frames.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [DOM and Browser APIs](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser/01_dom.md), [Performance Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/19_performance/01_performance-fundamentals.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | W3C Web Performance APIs, Core Web Vitals 2024+, Evergreen Browsers |

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

In client-side web applications, performance is experienced visually. Users do not perceive whether JavaScript executed in 2 microseconds or 10 microseconds; they perceive dropped frames, sluggish scrolling, layout jitter, and delays when clicking buttons.

Modern browsers strive to maintain a smooth 60 FPS (16.6ms per frame) or 120 FPS (8.3ms per frame) rendering budget. The main browser thread coordinates both JavaScript execution and visual pipeline tasks (style calculation, layout, and painting). Poorly written DOM mutations trigger **Layout Thrashing** (Forced Synchronous Layout), starving the compositor and violating Google's **Core Web Vitals**—the standardized metrics that govern user experience and search engine ranking.

---

## 2. Core Concepts

### The Critical Rendering Path
The sequence of steps a browser takes to convert HTML, CSS, and JavaScript into physical screen pixels:
1. **DOM Construction**: HTML bytes $\to$ tokens $\to$ nodes $\to$ DOM Tree.
2. **CSSOM Construction**: CSS rules $\to$ style sheet model.
3. **Render Tree**: Combines visible DOM nodes with matched CSSOM rules (elements with `display: none` are excluded).
4. **Layout (Reflow)**: Computes exact geometric coordinates and sizes for every render box.
5. **Paint**: Fills pixels with colors, borders, shadows, and text onto bitmap surfaces.
6. **Composite**: Uploads painted layer bitmaps to the GPU, which arranges them into the final screen buffer.

### Layout Thrashing (Forced Synchronous Layout)
Normally, browsers batch style mutations and calculate layout once per animation frame right before painting. However, if JavaScript **writes** a style property and then immediately **reads** a geometric layout property (such as `offsetWidth` or `scrollTop`) in the same turn, the browser is forced to flush pending styles and execute an immediate synchronous layout calculation. Doing this in a loop causes catastrophic frame drops.

### Core Web Vitals (2024 Standard)
- **LCP (Largest Contentful Paint)**: Measures perceived loading speed. Marks when the largest visual element (hero image, heading) is rendered. Target: $\le 2.5\text{s}$.
- **INP (Interaction to Next Paint)**: Measures page responsiveness to user input (clicks, taps, keypresses), replacing the legacy First Input Delay (FID). Target: $\le 200\text{ms}$.
- **CLS (Cumulative Layout Shift)**: Measures visual stability. Quantifies unexpected layout shifts of elements during viewing. Target: $\le 0.1$.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| MAIN BROWSER THREAD PIPELINE                                             |
|                                                                          |
|  [ JavaScript Task ] ---> [ Style Recalc ] ---> [ Layout ] ---> [ Paint ]|
|          |                                                          |    |
|          | Forced Synchronous Layout (Thrashing)                    |    |
|          +--- read offsetHeight <--- force instant reflow ----------+    |
+--------------------------------------------------------------------------+
                                    |
                                    | Layer Bitmaps Uploaded
                                    v
+--------------------------------------------------------------------------+
| COMPOSITOR THREAD (Runs on GPU independent of Main Thread)               |
|                                                                          |
|  - Manages Composited Layers                                             |
|  - Fast operations: transform, opacity, filter                           |
|  - Smooth 60/120 FPS scrolling and GPU hardware transforms               |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Observing Core Web Vitals via `PerformanceObserver`
```javascript
// 1. Observe Largest Contentful Paint (LCP)
const lcpObserver = new PerformanceObserver((entryList) => {
  const entries = entryList.getEntries();
  const lastEntry = entries[entries.length - 1];
  console.log(`LCP: ${lastEntry.startTime.toFixed(2)}ms`, lastEntry.element);
});
lcpObserver.observe({ type: 'largest-contentful-paint', buffered: true });

// 2. Observe Cumulative Layout Shift (CLS)
let clsScore = 0;
const clsObserver = new PerformanceObserver((entryList) => {
  for (const entry of entryList.getEntries()) {
    if (!entry.hadRecentInput) {
      clsScore += entry.value;
      console.log(`Current CLS: ${clsScore.toFixed(3)}`);
    }
  }
});
clsObserver.observe({ type: 'layout-shift', buffered: true });
```

### FastDOM Batching Pattern
```javascript
// Batching reads and writes to avoid Layout Thrashing
requestAnimationFrame(() => {
  // 1. Read Phase
  const width = element.offsetWidth;

  // 2. Write Phase
  requestAnimationFrame(() => {
    element.style.width = `${width + 10}px`;
  });
});
```

---

## 5. Practical Examples

### Example 1: Demonstrating and Fixing Layout Thrashing
```javascript
const boxes = document.querySelectorAll('.box');

// BAD: Layout Thrashing (Forced Synchronous Layout inside loop)
// Causes N style recalculations and N layout reflows!
function badResize() {
  for (let i = 0; i < boxes.length; i++) {
    // READ triggers immediate reflow because previous iteration modified style!
    const currentWidth = boxes[i].offsetWidth;
    // WRITE invalidates layout
    boxes[i].style.width = `${currentWidth + 10}px`;
  }
}

// GOOD: Batched Reads followed by Batched Writes
// Exactly ONE style recalculation and ONE layout pass!
function goodResize() {
  // Phase 1: Batch all reads
  const widths = new Array(boxes.length);
  for (let i = 0; i < boxes.length; i++) {
    widths[i] = boxes[i].offsetWidth;
  }

  // Phase 2: Batch all writes
  for (let i = 0; i < boxes.length; i++) {
    boxes[i].style.width = `${widths[i] + 10}px`;
  }
}
```

### Example 2: Tracking Long Animation Frames (LoAF API)
```javascript
// Long Animation Frames API (Baseline 2024): Pinpoints scripts causing dropped frames
const loafObserver = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    if (entry.duration > 50) { // Frame blocked for > 50ms
      console.warn(`[LoAF DETECTED] Frame duration: ${entry.duration.toFixed(2)}ms`);
      for (const script of entry.scripts) {
        console.log(`  Source: ${script.sourceURL}:${script.sourceCharPosition}`);
        console.log(`  Execution duration: ${script.duration.toFixed(2)}ms`);
      }
    }
  }
});

loafObserver.observe({ type: 'long-animation-frame', buffered: true });
```

### Example 3: Eliminating Cumulative Layout Shift (CLS) for Dynamic Media
```html
<!-- BAD: Missing dimensions causes massive layout jump when image loads -->
<img src="hero.jpg" alt="Hero banner" />

<!-- GOOD: Explicit aspect-ratio or width/height attributes reserve space upfront -->
<style>
  .hero-img {
    width: 100%;
    aspect-ratio: 16 / 9;
    object-fit: cover;
  }
</style>
<img class="hero-img" src="hero.jpg" width="1600" height="900" alt="Hero banner" />
```

### Example 4: GPU Composited Animations with `transform`
```javascript
const box = document.getElementById('animatedBox');

// POOR: Animating `left` or `top` triggers Layout and Paint on every frame!
function animateSlow() {
  let pos = 0;
  function step() {
    box.style.left = `${pos++}px`; // Triggers layout!
    requestAnimationFrame(step);
  }
  requestAnimationFrame(step);
}

// EXCELLENT: Animating `transform` runs entirely on the GPU Compositor thread!
function animateFast() {
  let pos = 0;
  function step() {
    box.style.transform = `translate3d(${pos++}px, 0, 0)`; // Zero layout, zero paint!
    requestAnimationFrame(step);
  }
  requestAnimationFrame(step);
}
```

### Example 5: Yielding to Maintain Low INP (Interaction to Next Paint)
```javascript
// Breaking a heavy user-click task to keep INP < 200ms
async function handleClick(event) {
  // Step 1: Provide immediate visual feedback (e.g. show loading spinner)
  button.classList.add('loading');

  // Step 2: Yield control to the browser so the UI update can paint to the screen
  if (globalThis.scheduler?.yield) {
    await scheduler.yield();
  } else {
    await new Promise(resolve => setTimeout(resolve, 0));
  }

  // Step 3: Run the heavy data calculation
  processHeavyData();
  button.classList.remove('loading');
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Geometry-Reading Layout Triggers**: Any of the following properties force a synchronous layout reflow if styles are dirty: `offsetWidth`, `offsetHeight`, `offsetTop`, `offsetLeft`, `clientWidth`, `clientHeight`, `scrollTop`, `scrollHeight`, `getBoundingClientRect()`, and `getComputedStyle()`.
2. **Overusing `will-change: transform`**: Declaring `will-change` on hundreds of elements forces the browser to create excessive GPU compositor layers, exhausting GPU VRAM and degrading mobile performance. Apply it sparingly and remove it when animations complete.
3. **`display: none` vs `visibility: hidden`**: Modifying an element with `display: none` removes it from the Render Tree completely, triggering layout for siblings. `visibility: hidden` keeps its geometry intact, skipping layout and triggering only repaint.
4. **Third-Party Script Layout Shifts**: Dynamic ad injections and cookie consent banners that inject content at the top of the page without reserved placeholder space cause severe CLS penalties.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Reading `offsetWidth` inside a DOM write loop | Forces repeated synchronous layout reflows (thrashing) | Batch all reads before writing styles |
| Animating `top`, `left`, `width`, or `height` | Triggers expensive Layout and Paint cycles every frame | Animate `transform` and `opacity` only |
| Inserting images without explicit dimensions | Causes page content to jump when image loads (high CLS) | Set explicit `width`/`height` or CSS `aspect-ratio` |
| Monopolizing the main thread after a user click | Blocks the browser paint cycle, blowing INP targets | Yield execution via `scheduler.yield()` |
| Injecting dynamic ad banners without placeholder space | Shifts page content downward, destroying CLS score | Reserve fixed-height skeleton placeholders |
| Putting `will-change: transform` on everything | Consumes vast amounts of GPU VRAM | Apply only to actively animating elements |

---

## 8. Performance Characteristics

- **Composited GPU Operations**: Transforming `translate3d()` or `opacity` bypasses Layout and Paint entirely, running at $< 1$ms per frame on the GPU compositor thread.
- **Reflow Cost**: Modifying the geometry of an element high in the DOM tree (e.g., `<body>` or root container) triggers a recursive layout recalculation of all descendant and sibling nodes, taking 20–100ms.

---

## 9. Engine Internals

1. **Dirty Bit Propagation in Layout Trees**:
   - The browser's layout engine assigns a "needs-layout" dirty bit to nodes whose styles or content change.
   - Reading geometry flags requires traversing up the tree to the nearest clean ancestor and executing a partial layout reflow before returning the coordinate value.
2. **Compositor Layers (GraphicsLayers)**:
   - Elements with 3D transforms, `<video>`, `<canvas>`, or `will-change` are promoted to independent Compositor Layers.
   - The GPU rasterizes these layers into textures. During scrolling or animation, the GPU simply adjusts texture offsets without asking the main CPU thread to repaint.

---

## 10. Best Practices & Design Guidelines

- **Adhere to the RAIL Model**: Response ($\le 100$ms), Animation (frame budget $\le 16$ms), Idle (utilize idle periods for deferred work), Load (interactive $\le 2.5$s).
- **Reserve Layout Space**: Always define aspect-ratio containers for dynamic images, embeds, and ads.
- **Audit via Chrome Lighthouse**: Regularly audit production pages using synthetic Lighthouse and real-user monitoring (RUM) data.

---

## 11. Real-World Case Study

### Optimizing an Infinite Scroll Table from 15 FPS to 60 FPS
```javascript
// PROBLEM: An enterprise data grid of 500 rows experienced severe jank (12-15 FPS) during scroll.
// Profiling revealed Layout Thrashing: each row component queried `row.offsetTop`
// to calculate its sticky highlight position, followed by updating `row.style.background`.

// SOLUTION: Decouple geometry calculations using IntersectionObserver and requestAnimationFrame
class SmoothDataGrid {
  constructor(container) {
    this.container = container;
    this.observer = new IntersectionObserver(this.onIntersect.bind(this), {
      root: container,
      threshold: 0.1,
    });
  }

  observeRows(rows) {
    rows.forEach(row => this.observer.observe(row));
  }

  onIntersect(entries) {
    // Batch DOM class updates inside single requestAnimationFrame turn
    requestAnimationFrame(() => {
      for (const entry of entries) {
        if (entry.isIntersecting) {
          entry.target.classList.add('visible');
        } else {
          entry.target.classList.remove('visible');
        }
      }
    });
  }
}

// Result: Zero layout reflows during scroll; rendering locked at fluid 60 FPS.
```

---

## 12. Interview Questions

<details>
<summary>1. What are the 6 stages of the Critical Rendering Path?</summary>
1. DOM construction (parsing HTML tokens into the DOM tree).
2. CSSOM construction (parsing CSS rules into the CSSOM tree).
3. Render Tree generation (combining DOM and visible CSSOM nodes).
4. Layout / Reflow (calculating geometric coordinates and dimensions).
5. Paint (filling pixels with colors, borders, text, and shadows).
6. Composite (uploading layer bitmaps to the GPU to produce the final screen display).
</details>

<details>
<summary>2. What is Layout Thrashing (Forced Synchronous Layout) and how do you prevent it?</summary>
Layout thrashing occurs when JavaScript writes to the DOM (invalidating layout styles) and then immediately reads a geometric property (like <code>offsetHeight</code> or <code>getBoundingClientRect</code>) within the same turn. The browser is forced to flush pending styles and synchronously compute layout immediately. It is prevented by batching all geometric reads before performing style writes.
</details>

<details>
<summary>3. What is INP (Interaction to Next Paint) and how does it differ from FID?</summary>
FID (First Input Delay) only measured the delay before the browser began processing the <i>first</i> interaction on a page. INP evaluates all user interactions (clicks, taps, keypresses) across the entire page lifecycle and reports the 98th percentile duration from user input to the moment the next frame is painted to the screen.
</details>

<details>
<summary>4. Why are animations on transform and opacity faster than animating left and top?</summary>
Modifying <code>left</code> and <code>top</code> triggers Layout, Paint, and Composite on the main browser thread. Modifying <code>transform</code> and <code>opacity</code> bypasses both Layout and Paint, allowing the animation to run directly on the GPU Compositor thread, which runs independently of main-thread JavaScript execution.
</details>

<details>
<summary>5. How does Cumulative Layout Shift (CLS) measure visual instability?</summary>
CLS measures the fraction of the viewport that shifts unexpectedly and the distance moved by unstable elements. An unexpected layout shift occurs when an element changes its starting position between frames without being prompted by direct user interaction (such as an image loading without defined dimensions).
</details>

<details>
<summary>6. What is the Long Animation Frames (LoAF) API?</summary>
LoAF is a modern browser performance API that captures animation frames exceeding 50ms in duration. It identifies the exact scripts, execution durations, and source code character positions responsible for blocking the rendering pipeline.
</details>

<details>
<summary>7. What is the difference between requestAnimationFrame and requestIdleCallback?</summary>
<code>requestAnimationFrame</code> schedules a callback to execute immediately before the next screen repaint, guaranteeing synchronization with the display refresh rate (e.g. 60Hz/120Hz). <code>requestIdleCallback</code> schedules low-priority background tasks to execute only when the browser has idle time between frame budgets.
</details>

<details>
<summary>8. Why can overusing will-change: transform cause performance degradation?</summary>
Every element with <code>will-change: transform</code> is promoted to an independent GPU compositor layer. Each layer allocates dedicated texture memory in GPU VRAM. Creating too many layers exhausts GPU memory, causing cache thrashing and slowing down page compositing.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a FastDOM Read/Write Batcher
Implement a mini-library `BatchDOM` providing `read(fn)` and `write(fn)` methods that schedule callbacks into separate arrays and flushes them inside a single `requestAnimationFrame`, ensuring all reads complete before any writes run.

<details>
<summary>View Solution</summary>

```javascript
export class BatchDOM {
  constructor() {
    this.reads = [];
    this.writes = [];
    this.scheduled = false;
  }

  read(task) {
    this.reads.push(task);
    this._schedule();
  }

  write(task) {
    this.writes.push(task);
    this._schedule();
  }

  _schedule() {
    if (this.scheduled) return;
    this.scheduled = true;

    requestAnimationFrame(() => {
      // Phase 1: Flush all reads
      const readsToRun = this.reads.splice(0);
      readsToRun.forEach(fn => fn());

      // Phase 2: Flush all writes
      const writesToRun = this.writes.splice(0);
      writesToRun.forEach(fn => fn());

      this.scheduled = false;
    });
  }
}
```
</details>

### Exercise 2: Real-Time INP Monitor
Write a script using `PerformanceObserver` that observes `event` entry types, captures the longest interaction duration on the page, and logs an alert if any user interaction takes longer than 200ms before paint.

<details>
<summary>View Solution</summary>

```javascript
class INPMonitor {
  constructor() {
    this.maxDuration = 0;

    const observer = new PerformanceObserver((list) => {
      for (const entry of list.getEntries()) {
        if (entry.interactionId) {
          const duration = entry.duration;
          if (duration > this.maxDuration) {
            this.maxDuration = duration;
          }

          if (duration > 200) {
            console.warn(`[POOR INP] Interaction on <${entry.target?.tagName}> took ${duration.toFixed(2)}ms! Target: <= 200ms`);
          }
        }
      }
    });

    observer.observe({ type: 'event', durationThreshold: 16, buffered: true });
  }

  getWorstInteraction() {
    return this.maxDuration;
  }
}

const monitor = new INPMonitor();
```
</details>
