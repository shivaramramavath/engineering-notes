# 04. Containment and Content Visibility

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | CSS Containment Module Level 2 & 3 / W3C Candidate Recommendation |
| **Browsers Tested** | Chrome 85+, Edge 85+, Firefox 125+, Safari 18+ (Universal Modern Baseline) |
| **Tested With** | Chrome DevTools Performance Profiler, DOM Tree Virtualization Tests |
| **Prerequisites** | `05-responsive-design/03-container-queries.md`, `12-performance-and-debugging/03-layout-paint-and-composite-costs.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
In standard web page rendering, the DOM and layout trees are completely open and interdependent. When a small badge or text string changes inside a deeply nested card component:
- The browser layout engine cannot assume that the change is isolated.
- The engine must traverse upward to the document root and recalculate layout geometry for all ancestor nodes, sibling elements, and surrounding content across the entire page.
- Furthermore, on long content-heavy pages (such as social feeds, e-commerce catalog pages, documentation portals, or search results with thousands of items), the browser spends hundreds of milliseconds calculating layout geometry and rasterizing pixels for elements located **thousands of pixels below the fold** that the user may never scroll down to see.

Historically, solving this required heavy JavaScript virtual list libraries (like `react-window` or `react-virtualized`), which introduce complex scroll synchronization code, break browser native `Ctrl+F` find-in-page, and compromise keyboard focus navigation.

### The Solution
The **CSS Containment Module** provides native declarative boundaries to isolate subtrees from the rest of the document:
- **`contain`**: Explicitly informs the layout engine: *"Do not recalculate layout or paint outside this element's boundary when its internal children change."*
- **`content-visibility: auto`**: Native browser-level DOM virtualization. It completely skips layout and painting for off-screen elements until the user scrolls near them, reducing initial page render times by **up to 80%** with pure CSS.
- **`contain-intrinsic-size`**: Preserves placeholder dimensions for off-screen elements so that the browser's vertical scrollbar remains stable without visual jumping or Cumulative Layout Shift (CLS).

---

## 2. Core Concepts

### 1. The Taxonomy of `contain` Types

```
┌─────────────────────────────────────────────────────────────┐
│                 CSS CONTAINMENT SUBTYPES                    │
└─────────────────────────────────────────────────────────────┘
  1. contain: layout
     - Isolates internal layout from external page layout.
     - Creates an absolute positioning containing block and stacking context.
     - Changes to children NEVER trigger reflows in parent or siblings.

  2. contain: paint
     - Isolates painting. Descendants cannot paint outside element bounds.
     - Acts as an implicit overflow clip / stacking context.
     - If element is off-screen, browser skips painting children entirely.

  3. contain: size
     - Element dimensions are computed without examining its child contents.
     - Element must have explicit width/height; otherwise collapses to 0x0.

  4. contain: inline-size
     - Element's horizontal width is isolated (enables CSS Container Queries).

  5. contain: style
     - Prevents counters and quotes from leaking outside the element.

  COMPOSITE SHORTHANDS:
  • contain: content  ──► contain: layout paint (Safe for fluid children)
  • contain: strict   ──► contain: layout paint size (Maximum optimization)
```

### 2. `content-visibility` Modes

| Property Value | Off-Screen Behavior | On-Screen Behavior | Find-in-Page Indexed? |
| :--- | :--- | :--- | :--- |
| **`visible`** (Default) | Full Layout + Full Paint | Full Layout + Full Paint | Yes |
| **`hidden`** | Rendering completely skipped | Rendering completely skipped | No (Hidden from user) |
| **`auto`** | **Layout & Paint SKIPPED** | Full Layout + Full Paint | **YES! (Native Search works!)**|

- When an element with `content-visibility: auto` is off-screen, the browser skips layout and rasterization, acting as if `contain: strict` is applied.
- When the user scrolls near the element (typically within 1–2 viewports), the browser automatically triggers layout and paint so the content is seamlessly ready before it hits the visible screen.

---

## 3. How It Works Internally

### Rendering Pipeline Savings with `content-visibility: auto`

```
TRADITIONAL LONG PAGE RENDERING (1,000 DOM Cards):
  Parse HTML ──► Construct DOM (1,000 Nodes)
                       │
                       ▼
            [EXPENSIVE MAIN THREAD PASS]
  Calculate Style: 1,000 nodes  (85ms)
  Compute Layout:  1,000 nodes  (145ms)
  Rasterize Paint: 1,000 nodes  (210ms)
                       │
                       ▼ Total Main Thread Block: 440ms!

OPTIMIZED PAGE WITH content-visibility: auto:
  Parse HTML ──► Construct DOM (1,000 Nodes)
                       │
                       ▼
            [ISOLATED VIEWPORT-ONLY PASS]
  Calculate Style: 8 visible nodes  (2ms)
  Compute Layout:  8 visible nodes  (4ms)
  Rasterize Paint: 8 visible nodes  (6ms)
                       │
                       ▼ Total Main Thread Block: 12ms! (97% Faster Initial Render)
```

### The Role of `contain-intrinsic-size`
When the browser skips rendering an off-screen element, its height collapses to 0 pixels unless an intrinsic placeholder height is declared. Without placeholders:
1. The vertical scrollbar shrinks to tiny dimensions.
2. As the user scrolls, elements suddenly inflate to their real heights, causing violent scroll jumps and breaking scroll stability.

`contain-intrinsic-size: auto 350px` solves this by remembering the element's actual rendered height once it has been scrolled into view (`auto`), while using `350px` as an initial placeholder for unvisited elements.

---

## 4. Syntax & Usage

### 1. High-Performance Infinite List / Feed Pattern
```css
.feed-item {
  /* 1. Skip off-screen layout & paint */
  content-visibility: auto;

  /* 2. Set intrinsic placeholder: uses measured height after first render, falls back to 280px */
  contain-intrinsic-size: auto 280px;

  /* 3. Standard card styling */
  background: #ffffff;
  border-radius: 0.75rem;
  padding: 1.5rem;
  margin-block-end: 1rem;
  border: 1px solid #e2e8f0;
}
```

### 2. Widget Isolation with `contain: content`
Isolate complex dynamic third-party widgets (e.g. chat embeds, animated stock tickers) so their animations never trigger layout recalculations in your main page layout:

```css
.stock-ticker-widget {
  /* Isolate layout and paint within this box */
  contain: content;
  overflow: clip;
}
```

---

## 5. Practical Examples

### Example 1: 5,000-Item Long Documentation Portal
Large API reference pages often display hundreds of code blocks and tables on a single page, resulting in massive initial render delays:

```html
<main class="api-docs">
  <section class="api-endpoint">
    <h2>POST /api/v1/auth/tokens</h2>
    <p>Issues short-lived JWT access tokens for authenticated clients.</p>
    <pre><code>curl -X POST https://api.domain.com/v1/auth/tokens ...</code></pre>
  </section>
  <!-- 200+ repeated endpoint sections -->
</main>
```

```css
.api-docs {
  max-width: 900px;
  margin: 0 auto;
  padding: 2rem 1.5rem;
}

.api-endpoint {
  /* Skip rendering until scrolled near */
  content-visibility: auto;
  /* Estimated height of each endpoint section */
  contain-intrinsic-size: auto 420px;

  border-bottom: 1px solid #e2e8f0;
  padding-block: 2rem;
}

.api-endpoint pre {
  background-color: #0f172a;
  color: #f8fafc;
  padding: 1rem;
  border-radius: 0.5rem;
  overflow-x: auto;
}
```

### Example 2: Container Queries Powered by `contain: inline-size`
Modern container queries require inline containment to break circular layout dependency loops:

```css
.card-wrapper {
  /* Establishes container query context by isolating inline dimensions */
  container-type: inline-size;
  container-name: card;
}

@container card (min-width: 480px) {
  .card-layout {
    display: grid;
    grid-template-columns: 160px 1fr;
    align-items: center;
  }
}
```

### Example 3: Modal Isolation via `content-visibility: hidden`
Instead of using `display: none` (which destroys internal state or re-triggers video/iframe reloads when toggled), use `content-visibility: hidden` to pause rendering while keeping DOM state cached in memory:

```css
.cacheable-modal {
  /* Keeps DOM tree and internal form/video state in memory, but skips all layout and paint */
  content-visibility: hidden;
  position: fixed;
  inset: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.5);
}

.cacheable-modal.is-active {
  /* Instantaneous reveal without DOM recreation or iframe reloads */
  content-visibility: visible;
}
```

### Example 4: Preventing Scroll Jitter with `auto <length>`
Notice how `auto` remembers actual rendered heights after the first visit:

```css
/* BAD: Fixed estimate forces scroll jump if real height is 600px */
.article-section {
  content-visibility: auto;
  contain-intrinsic-size: 300px;
}

/* PRODUCTION PATTERN: Uses 300px initially, but caches exact rendered height permanently */
.article-section {
  content-visibility: auto;
  contain-intrinsic-size: auto 300px;
}
```

### Example 5: Benchmarking Style Recalculation Time
Measuring the tangible impact of `contain: layout` on an interactive live dashboard:

```css
/* Without containment: Updating live chart data triggers 12ms layout across entire page */
.dashboard-tile {
  background: #fff;
  padding: 1rem;
}

/* With containment: Updating chart data triggers 0.4ms layout constrained strictly to tile */
.dashboard-tile {
  contain: layout;
  background: #fff;
  padding: 1rem;
}
```

---

## 6. Best Practices

1. **Always Pair `content-visibility: auto` with `contain-intrinsic-size`**: Failing to declare intrinsic placeholder dimensions causes scrollbar jumping and severe Cumulative Layout Shift (CLS).
2. **Use `auto <length>` Syntax**: Always write `contain-intrinsic-size: auto 300px;` so the browser caches the real rendered geometry after elements are scrolled into view once.
3. **Apply `content-visibility: auto` to High-Level Repeating Blocks**: Target repeating article sections, catalog cards, or table chunks; do not apply it to hundreds of individual tiny list items or text spans.
4. **Use `contain: content` on Dynamic Widgets**: Isolate animated charts, video players, and third-party embeds to prevent internal mutations from invalidating the main page layout.
5. **Never Use `contain: size` Without Explicit Dimensions**: `contain: size` isolates both width and height from child content; without explicit `height: 400px;`, the element collapses to $0\times 0\text{px}$.
6. **Verify Native Find-in-Page (`Ctrl+F`)**: Confirm that searching for off-screen text still works smoothly. Modern browsers seamlessly expand `content-visibility: auto` subtrees when search matches occur.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Omitting `contain-intrinsic-size`** | Collapses unrendered elements to 0px, causing severe scrollbar jumping and CLS. | Always supply `contain-intrinsic-size: auto <length>`. |
| **Using `contain: size` on fluid auto-height cards** | Element collapses vertically to 0px height; child content overflows or clips. | Use `contain: layout` or `contain: inline-size`. |
| **Applying `content-visibility: auto` to above-the-fold content** | Unnecessary overhead for content that must be rendered immediately anyway. | Apply only to elements located below the fold. |
| **Using `contain: paint` on elements with dropdown menus** | Dropdown flyout or tooltip is clipped at the container's bounding box. | Use `contain: layout` instead, or position tooltips in top layer. |
| **Setting unrealistic intrinsic size estimates** | If estimate is 100px and real content is 800px, scrollbar thumb shifts noticeably. | Measure real component heights in DevTools to provide accurate estimates. |
| **Relying on `content-visibility: hidden` for secure data** | Data remains in the DOM and can be inspected via DevTools. | Remove sensitive data from DOM on server side; use for UI caching only. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Support Matrix
- **`contain: layout paint`**: Universally supported across all evergreen browsers (Chrome 52+, Firefox 69+, Safari 15.4+).
- **`content-visibility: auto`**: Supported in Chrome 85+, Edge 85+, Firefox 125+, Safari 18+.
- **`contain-intrinsic-size: auto <length>`**: Universal baseline across all modern browsers supporting `content-visibility`.

### Accessibility and Assistive Technologies
- **Screen Reader Support**: Modern browser engines (Blink and WebKit) include off-screen `content-visibility: auto` elements in the Accessibility Tree! Screen reader users can navigate through sections and listen to content even if visual layout has been skipped.
- **Find-in-Page Integration**: When a user presses `Cmd+F` / `Ctrl+F` and types a query matching off-screen text, the browser engine automatically triggers layout on the matched element and scrolls it into view.

---

## 9. Comparison / When to Use What

| Feature | Primary Goal | Layout Impact | Paint Impact | JS Dependency |
| :--- | :--- | :--- | :--- | :--- |
| **`contain: layout`** | Reflow isolation | Bounded to subtree | None | Zero (Pure CSS) |
| **`contain: paint`** | Clipping & stacking | None | Bounded to subtree | Zero (Pure CSS) |
| **`contain: strict`** | Complete isolation | Fixed geometry required | Bounded to subtree | Zero (Pure CSS) |
| **`content-visibility: auto`** | Skip off-screen work | Off-screen skipped | Off-screen skipped | Zero (Pure CSS) |
| **JS Virtualization (react-window)**| DOM node recycling | Replaces DOM nodes | Only renders visible | High JS overhead |

---

## 10. Interview Questions

### Q1: What is the primary difference between `contain: content` and `contain: strict`?
<details>
<summary>Answer</summary>
- **`contain: content`**: Shorthand for `contain: layout paint`. It isolates the element's internal layout and paint from the rest of the document. Crucially, the element's dimensions can still be calculated naturally based on its child content. It is safe for fluid-height cards and responsive containers.
- **`contain: strict`**: Shorthand for `contain: layout paint size`. It applies full layout, paint, and size containment. Under size containment, the browser calculates the element's size without examining its children at all. Therefore, the element must have explicit width and height declared in CSS (e.g. `width: 300px; height: 200px;`); otherwise, its height will collapse to zero.
</details>

### Q2: How does `content-visibility: auto` achieve near-instantaneous page loads on long content pages?
<details>
<summary>Answer</summary>
When a page contains hundreds of DOM nodes below the fold, a standard browser executes style recalculation, layout reflow, and paint rasterization for every single element during initial page load.

When `content-visibility: auto` is applied to below-the-fold sections, the browser engine checks whether each section intersects with the visible viewport (plus a margin). For all off-screen sections, the engine halts rendering: it skips calculating layout geometry and skips rasterizing pixels. The browser only renders the 5–10 items visible on screen, reducing initial style and layout times by 70% to 85%. When the user scrolls down, the engine lazily renders upcoming sections right before they enter the screen.
</details>

### Q3: Why is `contain-intrinsic-size` mandatory when implementing `content-visibility: auto`?
<details>
<summary>Answer</summary>
When an element's rendering is skipped via `content-visibility: auto`, the engine treats it as having size containment, meaning its height collapses to 0 pixels (or empty dimensions).
If 100 off-screen cards collapse to 0px, the total height of the webpage shrinks, forcing the browser's vertical scrollbar thumb to stretch across the entire scroll track. As the user begins scrolling, elements hit the viewport and suddenly inflate to their real heights (e.g. 400px each). This causes violent scrollbar jumping, disorients the user, and triggers severe Cumulative Layout Shift (CLS) penalties.

`contain-intrinsic-size` specifies placeholder dimensions (e.g. `auto 400px`) that reserve vertical scroll space before rendering, maintaining a completely stable scrollbar.
</details>

### Q4: Explain the significance of the `auto` keyword in `contain-intrinsic-size: auto 300px`.
<details>
<summary>Answer</summary>
The `auto` keyword instructs the browser to remember and cache the element's *actual rendered height* once it has been rendered on screen.
- On first page load (before scrolling), the browser uses the fallback estimate of `300px`.
- When the user scrolls to the element, the browser renders it and measures its real height (e.g. `342px`).
- When the user scrolls past and the element leaves the viewport again, the browser discards the rendering subtree, but **remembers the exact measured height of 342px** instead of reverting to the 300px estimate. This guarantees zero layout shift if the user scrolls back up.
</details>

### Q5: How does `contain: layout` optimize style recalculation performance on interactive web applications?
<details>
<summary>Answer</summary>
Normally, altering the DOM or text inside an element causes layout invalidation flags to propagate up to parent containers and across sibling branches in the Render Tree.

`contain: layout` creates an impassable boundary. It informs the browser layout engine that whatever happens inside this container will never change the container's external bounding box, margin, or layout positioning. Consequently, when child nodes inside the contained element change, the browser engine constrains its layout recalculation pass strictly to that element's internal subtree, skipping the rest of the document and dropping style recalculation times from 20ms to under 1ms.
</details>

### Q6: Does `content-visibility: auto` break browser native Find-in-Page (`Ctrl+F` / `Cmd+F`)?
<details>
<summary>Answer</summary>
No. Unlike JavaScript virtual lists (which physically remove off-screen DOM nodes from the document, breaking search completely), `content-visibility: auto` is an engine-level feature.
The underlying DOM nodes remain present in the DOM tree and are indexed by the browser's search and accessibility algorithms. When a user searches for text located inside an unrendered off-screen section, the browser engine detects the match, automatically triggers synchronous layout on that specific section, expands its geometry, and scrolls it smoothly into view.
</details>

### Q7: Why does `contain: paint` create an implicit stacking context and clipping boundary?
<details>
<summary>Answer</summary>
Paint containment dictates that no descendant of the element may draw visual pixels outside the element's bounding box.
To guarantee this invariant, the browser engine must:
1. Clip any overflowing child pixels at the container's edge (acting like `overflow: clip`).
2. Establish a new Stacking Context so that child elements cannot interleave their paint order with external elements on the page.
Because descendants cannot escape the box, if the contained element is completely off-screen, the engine can safely discard the entire element from the paint pass with 100% certainty that none of its children bleed onto the visible screen.
</details>

### Q8: What is `contain: inline-size`, and why was it introduced in CSS Containment Level 3?
<details>
<summary>Answer</summary>
`contain: size` isolates both horizontal (inline) and vertical (block) dimensions, requiring an explicit height to prevent vertical collapse.
However, in responsive web design, 99% of UI components need their width constrained while allowing their vertical height to grow dynamically based on text length.

`contain: inline-size` isolates *only* the horizontal inline dimension. It allows container queries (`@container (min-width: 400px)`) to measure parent widths without creating circular layout dependencies, while still allowing the component's vertical height to expand naturally to fit its content.
</details>

---

## 11. Practice Exercises

### Exercise 1: Zero-JS Virtualization of an E-Commerce Product Catalog
**Objective**: Apply `content-visibility: auto` and `contain-intrinsic-size` to an e-commerce grid displaying 1,000 product cards, ensuring scrollbar stability and zero CLS.

<details>
<summary>Solution</summary>

```html
<ul class="catalog-grid">
  <li class="product-item">
    <div class="product-item__thumb">
      <img src="/img/headphones.webp" alt="Studio Headphones" width="300" height="200" loading="lazy">
    </div>
    <div class="product-item__details">
      <h3>Studio Monitor Pro</h3>
      <p class="price">$199.00</p>
      <button type="button" class="btn-buy">Add to Cart</button>
    </div>
  </li>
  <!-- 1,000 items -->
</ul>
```

```css
.catalog-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 1.5rem;
  list-style: none;
  padding: 1.5rem;
  max-width: 1400px;
  margin: 0 auto;
}

.product-item {
  /* 1. Skip rendering when off-screen */
  content-visibility: auto;

  /* 2. Cache real measured height once viewed; fall back to 360px initially */
  contain-intrinsic-size: auto 360px;

  /* 3. Card aesthetics */
  background: #ffffff;
  border-radius: 0.75rem;
  border: 1px solid #e2e8f0;
  overflow: clip;
  display: flex;
  flex-direction: column;
}

.product-item__thumb img {
  width: 100%;
  height: auto;
  aspect-ratio: 4 / 3;
  object-fit: cover;
}

.product-item__details {
  padding: 1.25rem;
  display: flex;
  flex-direction: column;
  flex: 1;
}

.btn-buy {
  margin-top: auto;
  padding: 0.5rem 1rem;
  background: #2563eb;
  color: #fff;
  border: none;
  border-radius: 0.375rem;
  font-weight: 600;
  cursor: pointer;
}
```
</details>

### Exercise 2: Dynamic Live Chart Containment
**Objective**: Apply layout containment to a real-time updating cryptocurrency ticker card so high-frequency DOM text updates do not trigger page-wide reflow passes.

<details>
<summary>Solution</summary>

```html
<div class="ticker-card">
  <div class="ticker-header">
    <span class="symbol">BTC/USD</span>
    <span class="live-price" id="btc-price">$94,250.00</span>
  </div>
  <div class="sparkline-container">
    <!-- Real-time SVG chart -->
  </div>
</div>
```

```css
.ticker-card {
  /* Isolate layout and paint; internal price ticks will NEVER reflow external page layout */
  contain: content;
  
  width: 320px;
  background: #0f172a;
  color: #f8fafc;
  padding: 1.25rem;
  border-radius: 0.75rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
}

.ticker-header {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-bottom: 1rem;
}

.live-price {
  font-size: 1.25rem;
  font-weight: 700;
  font-variant-numeric: tabular-nums; /* Prevents text width shifting on number updates */
  color: #34d399;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`contain: layout`**: Prevents internal changes from triggering layout reflows in parent or sibling elements.
- **`contain: paint`**: Clips pixels to bounds and creates an isolated stacking context.
- **`contain: content`**: Shorthand for `contain: layout paint`. Safe for responsive fluid elements.
- **`contain: strict`**: Shorthand for `contain: layout paint size`. Requires fixed element dimensions.
- **`content-visibility: auto`**: Native browser-level DOM virtualization; skips layout and paint for off-screen subtrees.
- **`contain-intrinsic-size: auto <length>`**: Mandatory placeholder dimensions preventing scrollbar jumps and CLS.
- **Find-in-Page (`Ctrl+F`)**: Fully functional with `content-visibility: auto`; browser auto-renders matched nodes.

---

## 13. Further Reading
- [W3C CSS Containment Module Level 2](https://www.w3.org/TR/css-contain-2/)
- [W3C CSS Containment Module Level 3](https://www.w3.org/TR/css-contain-3/)
- [web.dev: content-visibility: the new CSS property that boosts your rendering performance](https://web.dev/content-visibility/)
- [MDN Web Docs: CSS Containment](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_containment)
- [Una Kravets: Container Queries & Containment Deep Dive](https://una.im/container-queries/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [03. Layout, Paint and Composite Costs](03-layout-paint-and-composite-costs.md) | [05. File Size, Minification and Delivery](05-file-size-and-delivery.md) |
