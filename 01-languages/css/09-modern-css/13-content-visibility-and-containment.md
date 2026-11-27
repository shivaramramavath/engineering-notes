# Content Visibility and CSS Containment

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Containment Module Level 2 (W3C Candidate Recommendation) |
| **Baseline Status** | Baseline 2024 (Widely Available across Chrome 85+, Firefox 125+, Safari 18+, Edge 85+) |
| **Browsers Tested** | Chrome 85+, Firefox 125+, Safari 18+, Edge 85+ |
| **Tested With** | Chromium Layout Performance Profiler, Lighthouse Performance Audits, Playwright |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

On large web applications featuring thousands of DOM nodes (such as endless social media feeds, extensive documentation portals, e-commerce catalog pages, or analytics tables), browser rendering performance degrades drastically.

By default, the browser engine calculates layout and paints **every single element on the page**, even if an element is thousands of pixels below the fold and will never be seen by the user. This results in:
- High Total Blocking Time (TBT) and slow First Contentful Paint (FCP).
- Laggy initial page loading and DOM tree evaluation.
- High memory (RAM) usage on mobile devices.

Historically, engineering teams turned to **DOM Virtualization** (e.g. `react-virtualized` or `react-window`), which mounts and unmounts DOM nodes dynamically during scroll. While effective, DOM virtualization breaks in-page search (`Ctrl+F`), introduces scroll jumping, breaks accessibility screen readers, and adds heavy JavaScript bundle overhead.

**`content-visibility: auto`** and **CSS Containment** solve this natively in the browser's layout engine. By adding a single CSS declaration, the browser **skips layout, style, and painting for off-screen elements**, boosting rendering speeds by up to 700% while keeping DOM nodes intact for `Ctrl+F` in-page search and accessibility trees.

---

## 2. Core Concepts

### `content-visibility: auto`
Instructs the browser to evaluate the element against the viewport:
- **On-Screen**: Renders normally with full layout and painting.
- **Off-Screen**: Browser skips rendering the element's child subtree entirely. The element acts as if it has layout, style, and paint containment enabled.
- **In-Page Search Friendly**: If a user invokes `Ctrl+F` or an anchor link targets an off-screen section, the browser automatically mounts and renders the target instantly.

```css
.long-article-section {
  content-visibility: auto;
  contain-intrinsic-size: auto 500px;
}
```

### The `contain-intrinsic-size` Guardrail
When an off-screen element's rendering is skipped, its height collapses to `0px` by default. As the user scrolls down, elements suddenly pop into existence and expand, causing massive **Cumulative Layout Shift (CLS)** and a wildly erratic, bouncing scrollbar thumb.

`contain-intrinsic-size` acts as a placeholder height/width:
- `contain-intrinsic-size: 500px`: Treats the unrendered element as 500px tall.
- **`contain-intrinsic-size: auto 500px`** (Modern Standard): Uses `500px` as an initial estimate. Once the element enters the viewport and renders, the browser **caches its real rendered height**. If it scrolls off-screen again, the browser uses the cached height, guaranteeing **zero scrollbar jumps**!

### The `contain` Property
The foundational mechanism powering `content-visibility`. It isolates a DOM subtree from the rest of the document:
- `contain: layout`: Guarantees that internal layout changes inside the element never affect elements outside it.
- `contain: paint`: Guarantees that internal children never paint outside the element's bounding box.
- `contain: size`: The element's size is calculated without examining its children (requires explicit width/height).
- `contain: style`: Ensures counter increments or quote nesting do not leak outside.
- `contain: content`: Shorthand for `contain: layout paint`.
- `contain: strict`: Shorthand for `contain: size layout paint`.

---

## 3. How It Works Internally

```
Without content-visibility (Default Browser Behavior):
  [DOM Tree with 5,000 Nodes]
     │
     ▼ (Calculates layout for all 5,000 nodes)
  [Massive Style Recalculation -> 600ms Main Thread Lock]
     │
     ▼ (Paints all 5,000 nodes into GPU memory)
  [High Memory Footprint, Delayed FCP / TBT]

With content-visibility: auto:
  [DOM Tree with 5,000 Nodes]
     │
     ├── Viewport Check (Intersection Observer in C++)
     │     ├─ Nodes 1 to 50: In Viewport ──> Full Style, Layout, & Paint
     │     └─ Nodes 51 to 5,000: Off-Screen ──> SKIP SUBTREE RENDERING!
     │                                           (Use contain-intrinsic-size)
     ▼
  [Only 50 nodes evaluated -> 15ms Main Thread Task!]
```

```
Containment Value Capabilities:
┌──────────────────┬─────────────────┬─────────────────┬─────────────────┐
│ Value            │ Layout Isolated │ Paint Clipped   │ Size Detached   │
├──────────────────┼─────────────────┼─────────────────┼─────────────────┤
│ `layout`         │ YES             │ NO              │ NO              │
│ `paint`          │ NO              │ YES             │ NO              │
│ `size`           │ NO              │ NO              │ YES             │
│ `content`        │ YES             │ YES             │ NO              │
│ `strict`         │ YES             │ YES             │ YES             │
│ `auto` (c-v)     │ Dynamic         │ Dynamic         │ Dynamic         │
└──────────────────┴─────────────────┴─────────────────┴─────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2024 Support Box:
├── Chrome / Edge: 85+ (August 2020)
├── Firefox: 125+ (April 2024)
└── Safari: 18+ (September 2024 - Complete cross-browser Baseline)
```

### Basic Syntax
```css
/* 1. Automatic Viewport Containment */
.feed-card {
  content-visibility: auto;
  contain-intrinsic-size: auto 350px;
}

/* 2. Hidden State (Hides subtree without losing internal state) */
.modal-panel.hidden {
  content-visibility: hidden;
}

/* 3. Explicit Containment on Isolated Component */
.widget-island {
  contain: content;
}
```

---

## 5. Practical Examples

### Example 1: Infinite Social Feed or Catalog Cards
Boost scrolling and page load performance across thousands of feed items.

```html
<main class="feed-container">
  <article class="feed-item">Post #1</article>
  <article class="feed-item">Post #2</article>
  <!-- 500 more feed items... -->
</main>
```

```css
.feed-container {
  max-width: 42rem;
  margin: 0 auto;
  padding: 1.5rem;
}

.feed-item {
  /* Skips off-screen layout and paint */
  content-visibility: auto;

  /* Use estimated 200px until rendered, then remember true height */
  contain-intrinsic-size: auto 200px;

  margin-bottom: 1.5rem;
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
}
```

### Example 2: Long Technical Documentation Page
Improve First Contentful Paint on a 20,000-word documentation page with dozens of code samples.

```html
<article class="doc-body">
  <section class="doc-chapter">
    <h2>Chapter 1: Architecture</h2>
    <p>Extensive prose...</p>
    <pre><code>// Code block</code></pre>
  </section>
  <section class="doc-chapter">
    <h2>Chapter 2: Optimization</h2>
    <p>Extensive prose...</p>
    <pre><code>// Code block</code></pre>
  </section>
</article>
```

```css
.doc-chapter {
  content-visibility: auto;
  contain-intrinsic-size: auto 600px;
  margin-block-end: 4rem;
}
```

### Example 3: Zero-Overhead Off-Screen Tab Panels (`content-visibility: hidden`)
Keep inactive tab panels mounted in the DOM (preserving form state and scroll positions) without incurring ongoing paint or layout penalties.

```html
<div class="tabs-wrapper">
  <div class="tab-panel is-active">Active Tab Content</div>
  <div class="tab-panel is-inactive">Inactive Tab Content</div>
</div>
```

```css
.tab-panel.is-inactive {
  /* Skips rendering completely, but retains DOM state! */
  content-visibility: hidden;
}

.tab-panel.is-active {
  content-visibility: visible;
}
```

### Example 4: Third-Party Widget Isolation with `contain: content`
Prevent a third-party chat widget or advertisement from triggering layout reflows on the host page.

```css
.chat-widget-island {
  position: fixed;
  bottom: 2rem;
  right: 2rem;
  width: 22rem;
  height: 30rem;

  /* Strict layout and paint boundary */
  contain: content;

  background: white;
  border-radius: 0.75rem;
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.2);
}
```

### Example 5: Virtualized Data Grid Row Optimization
Accelerate massive 1,000-row data tables without third-party virtualization frameworks.

```html
<table class="perf-table">
  <tbody class="table-body">
    <tr class="table-row"><td>Data Item</td></tr>
  </tbody>
</table>
```

```css
.table-row {
  content-visibility: auto;
  contain-intrinsic-size: auto 3rem;
}
```

---

## 6. Best Practices

1. **Always pair `content-visibility: auto` with `contain-intrinsic-size`**: Omitting `contain-intrinsic-size` causes off-screen elements to collapse to $0\text{px}$ height, resulting in massive Cumulative Layout Shift (CLS) penalties and erratic scrollbars.
2. **Use the `auto <length>` keyword pattern**: Writing `contain-intrinsic-size: auto 400px` instructs the browser to remember the element's actual rendered height once it enters the viewport, completely eliminating scrollbar jitter.
3. **Apply `content-visibility: auto` to chunked sections, not tiny individual elements**: Apply it to large articles, cards, or sections (`<section>`, `<article>`), rather than hundreds of tiny `<li>` or `<span>` tags. Micro-containment introduces bookkeeping overhead.
4. **Use `content-visibility: hidden` instead of `display: none` when preserving state**: Unlike `display: none`, `content-visibility: hidden` retains the element's internal scroll positions, focused elements, and video playback state.
5. **Never apply `content-visibility: auto` to the top fold of a webpage**: Applying it to elements that are guaranteed to be visible upon initial load adds unnecessary intersection observer bookkeeping. Only apply it to elements expected to render below the fold.
6. **Ensure accessible headings are indexed**: Modern engines maintain accessibility trees for unrendered `content-visibility: auto` subtrees so screen reader users can still navigate document landmarks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `contain-intrinsic-size` | Severe Cumulative Layout Shift (CLS); bouncing, jumping scrollbars. | Always specify `contain-intrinsic-size: auto <estimate>`. |
| Applying `content-visibility: auto` to elements with sticky children | Sticky positioning fails inside contained elements because layout containment isolates the scroll parent. | Keep sticky elements outside contained blocks. |
| Over-fragmenting containment onto thousands of tiny nodes | Intersection bookkeeping in the browser engine out-costs the rendering savings. | Scope containment to logical chunked cards or sections. |
| Using `contain: size` without setting explicit width and height | The element collapses to `0px` because the engine ignores child sizes. | Specify explicit dimensions whenever using `size` containment. |
| Expecting `content-visibility: auto` to prevent asset downloads | Images and media inside off-screen elements may still preload over the network. | Use `loading="lazy"` on `<img>` tags in tandem. |
| Using `content-visibility: hidden` and expecting it to be accessible | `content-visibility: hidden` removes the subtree from the accessibility tree, just like `display: none`. | Only use `hidden` for intentionally inactive tabs or drawers. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **700% Faster Initial Load**: On pages with 10,000+ words or hundreds of cards, First Contentful Paint (FCP) and Time to Interactive (TTI) improve by up to $70\%$.
- **Reduced Memory Usage**: Off-screen textures are not held in GPU VRAM until they approach the viewport margin.

### Accessibility (A11y)
- **`content-visibility: auto`**: Browsers preserve heading hierarchies, landmarks, and find-in-page (`Ctrl+F`) functionality. When a user searches for text in an unrendered section, the browser immediately evaluates the node and scrolls it into view.
- **`content-visibility: hidden`**: Hides content from visual users and the Accessibility Tree, matching the semantics of `aria-hidden="true"`.

### Browser Compatibility
- **Baseline 2024**: Full cross-browser availability across Chrome (85+), Firefox (125+), and Safari (18+).
- **Feature Detection**:
```css
@supports (content-visibility: auto) {
  /* Supported */
}
```

---

## 9. Comparison / When to Use What

| Technique | In-Page Search (`Ctrl+F`)? | Screen Reader Accessible? | Memory Usage | Implementation Effort |
| :--- | :--- | :--- | :--- | :--- |
| **`content-visibility: auto`** | **Yes (Automatic)** | **Yes** | **Very Low** | **1 line of CSS** |
| **JavaScript Virtualization** | No (DOM is unmounted) | No (requires ARIA hacks) | Very Low | Complex (React lib) |
| **`display: none`** | No | No | Zero | Toggled via JS |
| **Native Un-contained DOM** | Yes | Yes | High | Default |

---

## 10. Interview Questions

### Q1: How does `content-visibility: auto` fundamentally differ from JavaScript DOM virtualization?
<details>
<summary>View Answer</summary>
DOM virtualization libraries (like `react-window`) unmount DOM nodes when they leave the viewport, removing them completely from the DOM tree. This breaks native browser in-page search (`Ctrl+F`), breaks accessibility landmarks, and introduces scroll position bugs. `content-visibility: auto` keeps all nodes mounted in the DOM, but instructs the browser's C++ rendering engine to skip the layout and painting phases for off-screen subtrees. If a user presses `Ctrl+F` or an anchor link targets an off-screen node, the browser mounts and paints it instantly.
</details>

### Q2: What is the purpose of the `auto` keyword in `contain-intrinsic-size: auto 500px`?
<details>
<summary>View Answer</summary>
The `auto` keyword instructs the browser to cache the element's actual rendered size once it enters the viewport. When the element is initially off-screen, the engine uses the estimated fallback (`500px`). Once the element is scrolled into view and rendered, the browser records its real computed height. If the element scrolls off-screen again, the browser uses the cached real height rather than the estimate, eliminating scrollbar jumping and layout shifts.
</details>

### Q3: What are the four individual containment types combined under `contain: strict`?
<details>
<summary>View Answer</summary>
`contain: strict` applies three core containment primitives: `contain: size layout paint`.
1. `size`: The element's dimensions are calculated without checking its children.
2. `layout`: Descendants cannot affect the layout of elements outside the container.
3. `paint`: Descendants cannot visually paint outside the container's bounding box.
</details>

### Q4: Why shouldn't `content-visibility: auto` be applied to the topmost hero section of a landing page?
<details>
<summary>View Answer</summary>
`content-visibility: auto` is designed to defer off-screen rendering. The hero section is guaranteed to be in the initial viewport upon page load. Applying `content-visibility: auto` to above-the-fold content forces the browser to run intersection observer calculations without any rendering savings, delaying First Contentful Paint.
</details>

### Q5: How does `content-visibility: hidden` differ from `display: none`?
<details>
<summary>View Answer</summary>
Both hide the element and remove its subtree from the accessibility tree. However, `display: none` destroys the element's computed layout box, resetting internal scroll positions, discarding cached canvas/WebGL contexts, and resetting focus. `content-visibility: hidden` stops rendering while caching the element's internal state in memory. When changed back to `visible`, the element restores instantly with its exact prior scroll offset.
</details>

### Q6: What happens if you apply `contain: size` to an element without specifying an explicit `width` or `height`?
<details>
<summary>View Answer</summary>
Because `size` containment instructs the browser to calculate the container's box size without examining its children, an element without an explicit width or height will collapse to $0\text{px} \times 0\text{px}$.
</details>

### Q7: Why do CSS sticky elements fail to stick when placed inside a container with `contain: layout`?
<details>
<summary>View Answer</summary>
`contain: layout` establishes an independent layout boundary. The container treats itself as a new containing block for all descendants, which isolates internal children from the outer scrolling viewport, preventing `position: sticky` from tracking the document scroll.
</details>

### Q8: Does `content-visibility: auto` prevent images inside the unrendered section from downloading?
<details>
<summary>View Answer</summary>
Not necessarily. While layout and painting are skipped, the HTML parser may still preload network resources. To ensure images in unrendered sections do not consume network bandwidth, developers should always combine `content-visibility: auto` with native HTML `loading="lazy"`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Optimize an Infinite Article Feed
**Objective**: Apply `content-visibility: auto` with an estimated 400px height to an article feed to eliminate scroll lag.
<details>
<summary>View Solution</summary>

```css
.article-stream {
  max-width: 48rem;
  margin: 0 auto;
}

.article-stream > article {
  content-visibility: auto;
  contain-intrinsic-size: auto 400px;
  margin-bottom: 2rem;
  padding: 1.5rem;
  background: white;
  border-radius: 0.5rem;
  border: 1px solid #cbd5e1;
}
```
</details>

### Exercise 2: State-Preserving Hidden Tab Panel
**Objective**: Hide an inactive tab panel using `content-visibility: hidden` so that user-typed form inputs are retained.
<details>
<summary>View Solution</summary>

```css
.settings-panel {
  display: block;
}

.settings-panel[hidden] {
  display: block; /* Override UA stylesheet */
  content-visibility: hidden;
}
```
</details>

### Exercise 3: Isolate an Ad Banner with `contain: content`
**Objective**: Ensure that dynamic banner ads cannot trigger layout reflows on the parent article.
<details>
<summary>View Solution</summary>

```css
.ad-slot {
  width: 100%;
  min-height: 15rem;
  contain: content;
  background-color: #f8fafc;
  border: 1px solid #e2e8f0;
}
```
</details>

### Exercise 4: Optimize a 1,000-Row Data Grid
**Objective**: Accelerate a large data table body by chunking rows into auto-contained blocks.
<details>
<summary>View Solution</summary>

```css
.data-grid-row {
  content-visibility: auto;
  contain-intrinsic-size: auto 2.5rem;
}
```
</details>

### Exercise 5: Off-Screen Documentation Sectioning
**Objective**: Configure multi-chapter technical documentation so that each sub-chapter renders only when approached by the viewport.
<details>
<summary>View Solution</summary>

```css
.doc-section {
  content-visibility: auto;
  contain-intrinsic-size: auto 50rem;
  border-bottom: 1px solid #e2e8f0;
  padding-bottom: 3rem;
}
```
</details>

### Exercise 6: Progressive Enhancement Check for Containment
**Objective**: Write a stylesheet rule ensuring that unsupported browsers render standard margins and layouts without layout jumps.
<details>
<summary>View Solution</summary>

```css
.feed-card {
  margin-bottom: 1.5rem;
  min-height: 12rem;
}

@supports (content-visibility: auto) {
  .feed-card {
    min-height: auto;
    content-visibility: auto;
    contain-intrinsic-size: auto 12rem;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Skip Off-Screen Rendering */
.card {
  content-visibility: auto;
  contain-intrinsic-size: auto 300px; /* Crucial for zero CLS */
}

/* 2. State-Preserving Hidden Component */
.tab.inactive {
  content-visibility: hidden;
}

/* 3. Layout & Paint Containment */
.widget {
  contain: content; /* Shorthand for contain: layout paint */
}
```

- `content-visibility: auto` skips layout and paint for off-screen elements.
- `contain-intrinsic-size: auto <length>` caches real rendered sizes, preventing scrollbar jumps.
- Retains in-page search (`Ctrl+F`) and accessibility trees.
- Baseline 2024 (Supported in Chrome 85+, Firefox 125+, Safari 18+).

---

## 13. Further Reading

- [MDN Web Docs: content-visibility](https://developer.mozilla.org/en-US/docs/Web/CSS/content-visibility)
- [MDN Web Docs: contain-intrinsic-size](https://developer.mozilla.org/en-US/docs/Web/CSS/contain-intrinsic-size)
- [W3C CSS Containment Module Level 2](https://www.w3.org/TR/css-contain-2/)
- [web.dev: content-visibility: the new CSS property that boosts your rendering performance](https://web.dev/content-visibility/)

---

### Navigation
- **Previous**: [12-scroll-snap.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/12-scroll-snap.md)
- **Next**: [14-css-if-and-custom-functions.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/14-css-if-and-custom-functions.md)
