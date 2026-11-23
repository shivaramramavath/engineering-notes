# Modern DOM Observers

> The browser Observer APIs—`IntersectionObserver`, `ResizeObserver`, and `MutationObserver`—replace expensive polling and scroll listeners with asynchronous, compositor-driven lifecycle notifications.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [The DOM](./01_dom.md), [Event Loop](../12_event-loop/01_event-loop.md), [Microtasks](../12_event-loop/02_microtasks-and-macrotasks.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | W3C Intersection Observer / Resize Observer / WHATWG MutationObserver / Baseline Widely Available |

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

Historically, detecting when an element scrolled into view or changed dimensions required attaching synchronous event listeners to `window.addEventListener('scroll')` or `window.addEventListener('resize')`. Within these high-frequency handlers, developers invoked geometry queries like `element.getBoundingClientRect()` or `element.offsetWidth`. Because these queries read layout geometry while scripts were executing, they caused **Forced Synchronous Reflows (Layout Thrashing)**, starving the main thread and resulting in severe scroll stutter and high battery drain.

Similarly, monitoring DOM tree mutations historically relied on legacy **DOM Mutation Events** (`DOMNodeInserted`, `DOMSubtreeModified`), which were synchronous and caused quadratic ($O(N^2)$) performance degradation on large DOM trees.

Modern web standards solve these challenges through three dedicated asynchronous **Observer APIs**:
- **`IntersectionObserver`**: Asynchronously monitors when an element enters or exits the viewport or a parent container.
- **`ResizeObserver`**: Asynchronously monitors changes to an element's box model dimensions.
- **`MutationObserver`**: Asynchronously batches notifications of DOM tree mutations at the microtask checkpoint.

---

## 2. Core Concepts

### Architectural Overview of the Three Observers

| Observer | Purpose | Trigger Timing | Execution Phase |
| :--- | :--- | :--- | :--- |
| **`IntersectionObserver`** | Element visibility / scroll position | Intersecting threshold crossings | Post-layout / pre-paint task |
| **`ResizeObserver`** | Element dimension / box model changes | Box size changes after layout | Post-layout / pre-paint loop |
| **`MutationObserver`** | DOM tree, attribute, or text mutations | Immediate on DOM mutation | **Microtask Checkpoint** |

```text
                  +-------------------------------------------------+
                  |                 DOM Mutations                   |
                  +-------------------------------------------------+
                                           │
                                           ▼
                                 [ MutationObserver ]
                             (Drains in Microtask Queue!)
                                           │
                                           ▼
                  +-------------------------------------------------+
                  |              Browser Render Phase               |
                  |                                                 |
                  |   1. Recalculate Styles & Compute Layout        |
                  |   2. Check Box Geometry ──> [ ResizeObserver ]  |
                  |   3. Compute Visibility ──> [ IntersectionObs ] |
                  |   4. Paint Composited Layers to Screen          |
                  +-------------------------------------------------+
```

---

## 3. How It Works Internally

### `IntersectionObserver` Geometry Calculation
Unlike scroll listeners that execute on the main JavaScript thread, `IntersectionObserver` geometry checks are calculated inside the browser's **Compositor / Layout Thread** after the layout phase completes. The browser computes the intersection between the target element's bounding box and the root container's clip rectangle. Callbacks are delivered asynchronously as a macrotask before visual frame rendering.

### `ResizeObserver` Depth-Loop Guard
If a `ResizeObserver` callback mutates styles that alter the target's size again (creating an infinite layout loop), the browser detects the recursion. It processes elements in order of DOM tree depth. If an element shallower than the current depth mutates, the browser delivers the warning: `"ResizeObserver loop completed with undelivered notifications"` and defers remaining notifications to the next frame to prevent locking the thread.

---

## 4. Syntax & API Reference

### 1. `IntersectionObserver`

```javascript
const observer = new IntersectionObserver((entries, obs) => {
  for (const entry of entries) {
    if (entry.isIntersecting) {
      console.log('Element visible:', entry.target);
      // entry.intersectionRatio: 0.0 to 1.0
      // entry.boundingClientRect, entry.intersectionRect, entry.rootBounds
    }
  }
}, {
  root: null,          // null = browser viewport; or parent Element
  rootMargin: '100px', // Expand/contract root bounding box
  threshold: [0, 0.5, 1.0] // Trigger at 0%, 50%, and 100% visibility
});

observer.observe(targetElement);
observer.unobserve(targetElement);
observer.disconnect(); // Detach all
```

### 2. `ResizeObserver`

```javascript
const resizeObserver = new ResizeObserver((entries, obs) => {
  for (const entry of entries) {
    // Modern Box Dimensions
    const contentBox = entry.contentBoxSize[0]; // { inlineSize, blockSize }
    const borderBox = entry.borderBoxSize[0];
    console.log(`Width: ${contentBox.inlineSize}px, Height: ${contentBox.blockSize}px`);
  }
});

resizeObserver.observe(targetElement, { box: 'content-box' }); // or 'border-box'
```

### 3. `MutationObserver`

```javascript
const mutationObserver = new MutationObserver((mutationsList, obs) => {
  for (const mutation of mutationsList) {
    if (mutation.type === 'childList') {
      console.log('Nodes added:', mutation.addedNodes);
      console.log('Nodes removed:', mutation.removedNodes);
    } else if (mutation.type === 'attributes') {
      console.log(`Attribute ${mutation.attributeName} changed!`);
    }
  }
});

mutationObserver.observe(targetElement, {
  childList: true,        // Direct children additions/removals
  subtree: true,          // Monitor entire descendant subtree
  attributes: true,       // Attribute modifications
  attributeFilter: ['class', 'hidden'], // Restrict to specific attributes
  attributeOldValue: true,// Record prior attribute value
  characterData: true,    // Text node content changes
});
```

---

## 5. Practical Examples

### Example 1: High-Performance Image Lazy-Loading
```javascript
function initImageLazyLoader() {
  const imageObserver = new IntersectionObserver((entries, observer) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        const img = entry.target;
        // Swap data-src to real src
        img.src = img.dataset.src;
        img.classList.remove('lazy-blur');

        // Stop observing this element once loaded
        observer.unobserve(img);
      }
    }
  }, {
    rootMargin: '200px 0px', // Pre-load images 200px before they enter viewport
    threshold: 0.01,
  });

  document.querySelectorAll('img[data-src]').forEach(img => imageObserver.observe(img));
}
```

### Example 2: Infinite Scroll Sentinel Trigger
```javascript
function setupInfiniteScroll(sentinelElement, fetchNextPage) {
  const scrollObserver = new IntersectionObserver(async (entries) => {
    const [entry] = entries;
    if (entry.isIntersecting) {
      // Temporarily disable while fetching
      scrollObserver.unobserve(sentinelElement);

      await fetchNextPage();

      // Re-observe sentinel once new content is appended
      scrollObserver.observe(sentinelElement);
    }
  }, {
    rootMargin: '400px', // Fetch 400px before reaching the exact bottom
    threshold: 0,
  });

  scrollObserver.observe(sentinelElement);
}
```

### Example 3: Responsive Component Design with `ResizeObserver`
```javascript
// Container Queries in JavaScript: Adapt layout based on element width, NOT screen width!
function createResponsiveWidget(widgetElement) {
  const ro = new ResizeObserver((entries) => {
    for (const entry of entries) {
      const width = entry.contentBoxSize[0].inlineSize;

      // Apply container styling classes directly
      widgetElement.classList.toggle('widget-compact', width < 300);
      widgetElement.classList.toggle('widget-medium', width >= 300 && width < 600);
      widgetElement.classList.toggle('widget-wide', width >= 600);
    }
  });

  ro.observe(widgetElement);
}
```

### Example 4: Sanitizing Injected Markup with `MutationObserver`
```javascript
function watchForUnauthorizedScripts(rootElement) {
  const observer = new MutationObserver((mutations) => {
    for (const mutation of mutations) {
      for (const node of mutation.addedNodes) {
        if (node.nodeType === Node.ELEMENT_NODE) {
          // Detect and destroy unauthorized script tags immediately in microtask
          if (node.tagName === 'SCRIPT') {
            console.error('Blocked injected script tag:', node);
            node.remove();
          } else {
            // Check nested scripts
            node.querySelectorAll('script').forEach(s => s.remove());
          }
        }
      }
    }
  });

  observer.observe(rootElement, { childList: true, subtree: true });
}
```

### Example 5: Active Table of Contents Spy via `IntersectionObserver`
```javascript
function setupScrollSpy(headings, navLinks) {
  const linkMap = new Map();
  navLinks.forEach(link => {
    const id = link.getAttribute('href').slice(1);
    linkMap.set(id, link);
  });

  const observer = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        navLinks.forEach(l => l.classList.remove('active'));
        const activeLink = linkMap.get(entry.target.id);
        activeLink?.classList.add('active');
      }
    }
  }, {
    rootMargin: '-20% 0px -70% 0px', // Active zone in upper-middle of viewport
    threshold: 0,
  });

  headings.forEach(h => observer.observe(h));
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Initial Invocation on Observe**: Both `IntersectionObserver` and `ResizeObserver` invoke their callback **immediately** upon calling `.observe()`, reporting the initial state. Code must expect this synchronous initial run.
2. **`MutationObserver` Microtask Timing**: `MutationObserver` callbacks run as **microtasks** immediately after the current call stack clears, before browser rendering. If a callback triggers another mutation, it can cause microtask loops.
3. **`ResizeObserver` Loop Warnings**: Modifying an element's size inside its own `ResizeObserver` callback triggers the browser warning: `"ResizeObserver loop completed with undelivered notifications"`. Decouple mutations using `requestAnimationFrame`.
4. **Zero-Height Root Elements**: If `IntersectionObserver.root` has CSS `height: 0` or `display: none`, intersection calculations will never trigger `isIntersecting: true`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using Scroll Listeners for Lazy Load**| `window.addEventListener('scroll', checkBounds)` | Use `IntersectionObserver` | Forces layout recalculation on every scroll tick, causing jank. |
| **Infinite Loop in `ResizeObserver`** | `ro = new ResizeObserver(() => el.style.width = ...)` | Wrap size mutation in `requestAnimationFrame` | Triggers infinite feedback loops and browser warnings. |
| **Forgetting to `disconnect()`** | Abandoning observer on component unmount | Call `observer.disconnect()` in teardown | Retains DOM elements and closures in memory (memory leak). |
| **Assuming Synchronous MutationObserver**| Expecting callback to run synchronously on `appendChild`| Wait for microtask tick to drain | `MutationObserver` is asynchronous (microtask-based). |
| **Broad `subtree: true` without filter**| Observing `document.body` with `attributes: true` | Use `attributeFilter: ['specific-attr']` | Overwhelms the engine with thousands of irrelevant mutation records. |
| **Reading Obsolete Properties in RO** | Reading `entry.contentRect` | Use `entry.contentBoxSize[0].inlineSize` | `contentRect` is legacy and doesn't handle fractional subpixels properly. |

---

## 8. Performance Characteristics

- **Zero Scroll Thread Blocking**: `IntersectionObserver` offloads bounding box intersection checks to the browser's background compositor pipeline, executing with $0\text{ms}$ main thread scroll blocking.
- **Microtask Batching in `MutationObserver`**: If 1,000 DOM elements are appended in a tight loop, `MutationObserver` does not fire 1,000 times; it coalesces all modifications into a single callback with a list of 1,000 records.
- **Box Size Subpixels**: Modern `contentBoxSize` returns exact floating-point subpixel measurements directly from layout without rounding penalties.

---

## 9. Engine Internals

1. **Chromium `IntersectionObservation`**: Blink coordinates visibility checks during the `LocalFrameView::UpdateLifecyclePhasesInternal` lifecycle step.
2. **`MutationObserverRegistration` Ring Buffer**: Blink associates a linked list of `MutationObserverRegistration` objects with each observed node. Mutations append raw `MutationRecord` instances into an isolate-wide queue drained at the end of the microtask checkpoint.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `rootMargin` for Pre-fetching**: Set `rootMargin: '200px'` on image and infinite scroll observers so assets load before the user reaches them.
2. **Unobserve Single-Use Elements Immediately**: Once a lazy-loaded image or animation trigger fires, invoke `observer.unobserve(entry.target)`.
3. **Always Call `disconnect()` on Component Unmount**: Clean up active observers in Single Page Application (SPA) component teardown hooks.
4. **Use `attributeFilter` in `MutationObserver`**: Explicitly restrict observed attributes to prevent processing noise.

---

## 11. Real-World Case Study

### High-Performance Analytics Viewability Tracker (IAB 50/1 Standard)

An ad-tech SDK must record an impression only when an advertisement unit is at least 50% visible in the viewport continuously for at least 1 consecutive second (IAB Viewability Standard).

```javascript
class AdViewabilityTracker {
  #element;
  #timer = null;
  #observer;
  #hasLoggedImpression = false;

  constructor(adElement, onImpression) {
    this.#element = adElement;
    this.#initObserver(onImpression);
  }

  #initObserver(onImpression) {
    this.#observer = new IntersectionObserver((entries) => {
      const [entry] = entries;

      if (entry.isIntersecting && entry.intersectionRatio >= 0.5) {
        // Ad is >= 50% visible. Start 1-second continuous timer
        if (!this.#timer && !this.#hasLoggedImpression) {
          this.#timer = setTimeout(() => {
            this.#hasLoggedImpression = true;
            onImpression(this.#element.dataset.adId);
            this.destroy(); // Done tracking this ad
          }, 1000);
        }
      } else {
        // User scrolled away before 1 second elapsed! Cancel timer!
        if (this.#timer) {
          clearTimeout(this.#timer);
          this.#timer = null;
        }
      }
    }, {
      threshold: [0.5], // Fire when crossing the 50% threshold
    });

    this.#observer.observe(this.#element);
  }

  destroy() {
    if (this.#timer) clearTimeout(this.#timer);
    this.#observer?.disconnect();
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is <code>IntersectionObserver</code> vastly superior to listening to the <code>scroll</code> event?</strong></summary>

Scroll event listeners run on every scroll tick on the main thread and typically require calling `getBoundingClientRect()`, which forces synchronous layout recalculation (layout thrashing). `IntersectionObserver` computes visibility asynchronously inside the browser's compositor thread after layout without blocking user interaction or scrolling.
</details>

<details>
<summary><strong>2. In what event loop queue do <code>MutationObserver</code> callbacks execute?</strong></summary>

They execute in the **Microtask Queue**. When DOM mutations occur, records are batched and executed at the next microtask checkpoint (after the current call stack clears, before rendering and macrotasks).
</details>

<details>
<summary><strong>3. What causes the "ResizeObserver loop completed with undelivered notifications" warning?</strong></summary>

It occurs when a `ResizeObserver` callback resizes an element whose depth in the DOM tree is shallower than or equal to an element already processed in the current frame. The browser logs the warning and defers undelivered notifications to the next frame to prevent infinite layout loops.
</details>

<details>
<summary><strong>4. What is the purpose of the <code>rootMargin</code> option in <code>IntersectionObserver</code>?</strong></summary>

`rootMargin` expands or shrinks the bounding box of the root viewport before computing intersections (using CSS margin syntax, e.g. `'100px 0px'`). It allows pre-fetching resources (like images or infinite scroll pages) before they physically appear on screen.
</details>

<details>
<summary><strong>5. What is the difference between <code>contentBoxSize</code> and <code>borderBoxSize</code> in `ResizeObserver`?</strong></summary>

`contentBoxSize` measures the dimension of the element's content area (excluding padding and borders). `borderBoxSize` measures the complete box dimension including padding and borders.
</details>

<details>
<summary><strong>6. What does <code>mutationObserver.takeRecords()</code> do?</strong></summary>

It immediately drains and returns all pending unhandled `MutationRecord` instances from the observer's queue, clearing the queue so the callback will not fire for those records.
</details>

<details>
<summary><strong>7. Do <code>IntersectionObserver</code> and <code>ResizeObserver</code> fire immediately upon calling <code>.observe()</code>?</strong></summary>

Yes. Both observers dispatch an initial notification immediately after observation begins to report the element's current baseline state.
</details>

<details>
<summary><strong>8. Why should you avoid using <code>subtree: true</code> with <code>attributes: true</code> on the document body?</strong></summary>

Because observing the entire document subtree for all attribute changes forces the browser to generate and batch mutation records for every single element attribute change across the entire application, causing significant memory and CPU overhead.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Element Sticky Pin Detector
Using `IntersectionObserver`, implement a class `StickyWatcher` that detects when a `position: sticky` navigation header sticks to the top of the viewport and toggles a `.is-stuck` class on it.

<details>
<summary>View Solution</summary>

```javascript
class StickyWatcher {
  constructor(stickyElement) {
    // Insert a 1px zero-opacity sentinel directly before sticky element
    const sentinel = document.createElement('div');
    sentinel.className = 'sticky-sentinel';
    sentinel.style.height = '1px';
    sentinel.style.marginBottom = '-1px';
    stickyElement.before(sentinel);

    const observer = new IntersectionObserver(([entry]) => {
      // If sentinel is NOT intersecting, the sticky element has scrolled past top!
      const isStuck = !entry.isIntersecting;
      stickyElement.classList.toggle('is-stuck', isStuck);
    }, {
      threshold: [0],
    });

    observer.observe(sentinel);
  }
}
```
</details>

### Challenge 2: Implement a DOM Change Debouncer with `MutationObserver`
Write a utility function `onDomStable(container, callback, timeoutMs = 300)` that monitors DOM mutations in `container` and fires `callback()` only after the DOM has stopped mutating for at least `timeoutMs`.

<details>
<summary>View Solution</summary>

```javascript
function onDomStable(container, callback, timeoutMs = 300) {
  let timer = null;

  const observer = new MutationObserver(() => {
    clearTimeout(timer);
    timer = setTimeout(() => {
      observer.disconnect();
      callback();
    }, timeoutMs);
  });

  observer.observe(container, {
    childList: true,
    subtree: true,
    attributes: true,
    characterData: true,
  });

  // Initial timer in case no mutations occur
  timer = setTimeout(() => {
    observer.disconnect();
    callback();
  }, timeoutMs);

  return {
    cancel: () => {
      clearTimeout(timer);
      observer.disconnect();
    },
  };
}
```
</details>
