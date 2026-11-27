# Sticky Positioning (`position: sticky`)

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | `02-box-model-and-layout-basics/08-positioning-static-relative-absolute-fixed.md` |
| **Estimated Reading Time** | 20 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (`position: sticky` Baseline 2017) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+ |
| **Tested With** | Automated CSS Validation Runner (`css_runner.js`) |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

`position: sticky` represents a hybrid of relative and fixed positioning. An element styled with `position: sticky` behaves as `position: relative` within its parent container until the scrolling viewport reaches a designated offset threshold (such as `top: 0`). At that moment, it seamlessly switches to behaving like `position: fixed`, "sticking" in place on the screen.

Crucially, unlike `position: fixed`, a sticky element is strictly **confined to the boundaries of its immediate parent container**. When the bottom edge of the parent container scrolls past the sticky element, the element unsticks and travels upward with its parent out of the viewport.

Common applications include:
- **Alphabetical contact list headers** (iOS-style contacts where header 'A' sticks until 'B' pushes it away).
- **Persistent table headers (`<thead>`)** during deep financial or analytics scrolling.
- **Side navigation bars and table of contents** anchored alongside long technical articles.

---

## 2 Core Concepts

### The Two Critical Rules of Sticky Positioning

1. **Threshold Offset Requirement**:
   - For an element to stick, you **must** define at least one threshold offset: `top`, `bottom`, `left`, `right`, `inset-block-start`, or `inset-inline-start`.
   - Without an offset declaration (e.g., just `position: sticky;`), the element simply behaves as `position: relative` with zero offset—it will never stick!

2. **The Parent Container Boundary (Sticky Range)**:
   - A sticky element can **never travel outside its parent element**.
   - If the parent element has the exact same height as the sticky element (e.g., wrapped in an unstyled `<div>`), the sticky element has $0\text{px}$ of travel range and will appear not to stick.
   - For sticky scrolling to work, the parent container must be taller than the sticky element.

3. **The Overflow Containment Trap**:
   - If ANY ancestor between the sticky element and the scroll container has `overflow: hidden`, `overflow: auto`, `overflow: clip`, or `overflow: scroll`, sticky positioning will frequently break or anchor to that intermediate overflow box rather than the main document scroll!

---

## 3 How It Works Internally

The browser engine calculates sticky positioning relative to two coordinate spaces: the **Nearest Scrollable Ancestor** and the **Parent Box Layout Geometry**.

```
Document Viewport (Scrolling Down)
+-------------------------------------------------------------+
| Browser Window Viewport                                     |
|                                                             |
|  +-------------------------------------------------------+  |
|  | Section Parent (e.g., height: 800px)                  |  |
|  |                                                       |  |
|  |  +-------------------------------------------------+  |  |
|  |  | Header (position: sticky; top: 0px;)            |  |  |
|  |  | [STICKING]: Anchored at top: 0 inside viewport  |  |  |
|  |  +-------------------------------------------------+  |  |
|  |                                                       |  |
|  |  Body paragraph text scrolling upward...              |  |
|  |  More text...                                         |  |
|  |  Bottom edge of section parent approaching!           |  |
|  +-------------------------------------------------------+  |
|                                                             |
|  +-------------------------------------------------------+  |
|  | Next Section Parent (Pushes previous header away)     |  |
|  |  +-------------------------------------------------+  |  |
|  |  | Next Sticky Header (top: 0)                     |  |  |
|  |  +-------------------------------------------------+  |  |
+-------------------------------------------------------------+
```

### Sticking Mechanics Lifecycle

1. **Normal Flow Phase**: When the parent is entering the viewport and scroll position has not reached the sticky threshold, the element is rendered in normal flow (offset is zero).
2. **Stuck Phase**: As the scroll position passes the threshold (`scrollOffset >= elementInitialOffset - stickyTop`), the element is pinned to `stickyTop` relative to the scrolling viewport.
3. **Pushed / Unstuck Phase**: When the scroll continues until `elementBottom >= parentBottom`, the bottom boundary of the parent box pushes the sticky element along with it.

---

## 4 Syntax & Usage

```css
.sticky-header {
  /* Declare sticky mode */
  position: sticky;

  /* MANDATORY: At least one scroll threshold */
  top: 0;

  /* Optional: Layer above normal-flow siblings */
  z-index: 10;
}

/* Horizontal Sticky in Horizontally Scrolling Containers */
.sticky-column {
  position: sticky;
  left: 0;
  background-color: #ffffff;
  z-index: 2;
}
```

> [!NOTE]
> **Baseline Status: Widely Available**
> `position: sticky` is Baseline Widely Available across all major desktop and mobile browsers since 2017. Historically required `-webkit-sticky` in older iOS WebKit; standard unprefixed `position: sticky` is universally supported today.

---

## 5 Practical Examples

### Example 1: Sticky Navigation Bar with Shadow on Scroll

```css
.site-nav {
  position: sticky;
  top: 0;
  z-index: 100;
  height: 60px;
  background-color: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(8px);
  border-bottom: 1px solid #e2e8f0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 1.5rem;
}
```

### Example 2: Alphabetical Categorized List (iOS-Style Contact Book)

```css
.contact-section {
  position: relative;
  /* Parent must be tall enough to allow sticky travel */
  margin-bottom: 2rem;
}

.contact-letter-heading {
  position: sticky;
  top: 0;
  background-color: #f1f5f9;
  color: #475569;
  padding: 0.5rem 1rem;
  font-weight: 700;
  font-size: 0.875rem;
  border-bottom: 1px solid #cbd5e1;
  z-index: 10;
}

.contact-item {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #f1f5f9;
}
```

### Example 3: Edge Case — Sticky `<th>` in Scrollable HTML Table

When designing data grids with sticky table headers, `position: sticky` must be placed directly on the `<th>` cells, not the `<tr>`:

```css
.table-scroll-container {
  max-height: 400px;
  overflow-y: auto;
  border: 1px solid #e2e8f0;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
}

/* Apply sticky to <th>, NOT to <tr> or <thead> in older rendering engines */
.data-table th {
  position: sticky;
  top: 0;
  background-color: #0f172a;
  color: #f8fafc;
  padding: 12px 16px;
  text-align: left;
  z-index: 20;
}

.data-table td {
  padding: 10px 16px;
  border-bottom: 1px solid #e2e8f0;
}
```

### Example 4: Failure Case — Broken Sticky due to Ancestor `overflow: hidden`

```css
/* BUGGY CODE: Sticky will NEVER activate */
.page-wrapper {
  /* BUG: Any overflow property on an ancestor traps or disables sticky */
  overflow: hidden;
}

.sidebar {
  position: sticky;
  top: 20px;
  /* Never sticks! The scroll occurs on document, but page-wrapper clips it */
}

/* FIXED CODE */
.page-wrapper-fixed {
  /* Replace overflow: hidden with modern display: flow-root if BFC containment is desired */
  display: flow-root;
}

.sidebar-fixed {
  position: sticky;
  top: 20px;
  /* Works as intended */
}
```

### Example 5: Production Two-Column Article Layout with Sticky Sidebar

```css
.article-grid {
  display: grid;
  grid-template-columns: 1fr 280px;
  gap: 32px;
  max-width: 1200px;
  margin: 0 auto;
  padding: 32px 16px;
  /* Crucial: Prevent default stretch if sidebar is shorter than article */
  align-items: start;
}

.article-body {
  min-width: 0; /* Prevent grid breakout */
}

.sticky-table-of-contents {
  position: sticky;
  top: 24px;
  max-height: calc(100vh - 48px);
  overflow-y: auto;
  padding: 16px;
  background-color: #f8fafc;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}
```

---

## 6 Best Practices

1. **Always supply an explicit offset**: Never omit `top`, `bottom`, `left`, or `right` when using `position: sticky`.
2. **Apply `align-items: start` on Flex or Grid parents**: By default, flex and grid stretch items to equal height. If the sticky item stretches to match the parent height, it has $0\text{px}$ travel room and cannot stick!
3. **Never apply `overflow: hidden` to intermediate ancestor wrappers**: Use `overflow: clip` or `display: flow-root` if layout containment is needed.
4. **Always give sticky elements an opaque background**: Sticky elements scroll over in-flow content. Without a background (e.g. `background-color: #ffffff`), underlying text shows through illegibly.
5. **Set an appropriate `z-index`**: Normal flow content scrolling under sticky elements can bleed through if stacking contexts are improperly ordered.
6. **Apply sticky to `<th>` cells directly in tables**: Do not apply sticky solely to `<tr>`, as table row layout algorithms handle position properties inconsistently across engines.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Root Cause | Fix |
| :--- | :--- | :--- |
| Sticky element refuses to stick | Omitted offset threshold (e.g., missing `top: 0`) | Declare `top: 0` (or desired offset) |
| Ancestor has `overflow: hidden` | Overflow creates an isolated scroll port, breaking viewport tracking | Remove `overflow: hidden` or switch to `overflow: clip` |
| Flex/Grid child has no travel distance | Default `align-items: stretch` makes item as tall as parent | Add `align-self: start` to the sticky item |
| Sticky header is translucent | Transparent default background shows scrolled text underneath | Set explicit `background-color` |
| Sticky header gets covered by cards | Sibling elements create new stacking contexts with higher z-index | Add explicit `z-index` to the sticky element |
| Applying sticky to `<tr>` | HTML table rendering engine specs treat table rows uniquely | Target `.table th` instead of `.table tr` |

---

## 8 Performance, Accessibility & Browser Support

### Compositor & Scrolling Performance

- Unlike JavaScript scroll listeners (`window.addEventListener('scroll', ...)`), native `position: sticky` runs on the browser's asynchronous **compositor thread**. It achieves smooth 60fps / 120fps scrolling without main-thread jank or frame drops.
- Avoid attaching expensive heavy CSS filters (such as large `backdrop-filter: blur(20px)`) to tall sticky containers on low-powered mobile devices, which can increase GPU power consumption.

### Accessibility Considerations

- **Reduced Motion**: Sticky headers that animate size during scrolling should respect `@media (prefers-reduced-motion: reduce)`.
- **Keyboard Tab Navigation**: When keyboard users tab through links in an article, the browser automatically scrolls the page. Ensure sticky headers do not obscure the focused element by utilizing `scroll-margin-top` on headings and target anchors:

```css
/* Prevent sticky header from concealing focus targets */
h2, h3, :target {
  scroll-margin-top: 80px; /* Header height + buffer */
}
```

---

## 9 Comparison / When to Use What

| Feature | `position: relative` | `position: sticky` | `position: fixed` |
| :--- | :--- | :--- | :--- |
| **In Normal Flow?** | Yes | Yes (until threshold) | No (Completely removed) |
| **Bound to Parent Box?** | Yes | Yes (Unsticks at parent bottom) | No (Bound to Viewport) |
| **Scroll Triggered?** | No | Yes (Activates at offset) | Always pinned |
| **Requires Offset?** | Optional | **Mandatory** | Recommended |
| **Best Used For** | Micro-shifts & containing blocks | Headers, sidebars, TOCs | Modals, global toasts, root nav |

---

## 10 Interview Questions

<details>
<summary><strong>1. What causes a <code>position: sticky</code> element to fail silently and refuse to stick?</strong></summary>

The three most common causes are:
1. Missing offset threshold (omitting `top: 0` or similar).
2. An ancestor having `overflow: hidden`, `overflow: auto`, or `overflow: scroll`, which traps the scroll context.
3. The parent container having the exact same height as the sticky element (commonly caused by flex or grid `align-items: stretch`).
</details>

<details>
<summary><strong>2. How does <code>position: sticky</code> differ from <code>position: fixed</code>?</strong></summary>

`position: fixed` removes the element from normal flow and pins it permanently to the viewport, regardless of where its parent scrolls. In contrast, `position: sticky` retains normal flow layout space and is strictly bounded by its immediate parent's bounding box: once the parent scrolls out of view, the sticky element scrolls away with it.
</details>

<details>
<summary><strong>3. Why does <code>align-self: start</code> often fix broken sticky elements inside flex containers?</strong></summary>

Flexbox defaults to `align-items: stretch`, causing child items to expand to match the height of the tallest sibling. If the sticky item stretches to 100% of the parent's height, there is zero remaining distance for it to scroll within the parent. Setting `align-self: start` shrinks the sticky element to its intrinsic content height, restoring its travel range.
</details>

<details>
<summary><strong>4. How does <code>scroll-margin-top</code> solve sticky header clipping issues?</strong></summary>

When a user clicks an in-page anchor link (`#section-2`), the browser scrolls the page until that heading touches the very top of the viewport. If a sticky header is present, it covers the heading. Declaring `scroll-margin-top: [header-height]` informs the browser's scroll snapping algorithm to stop scrolling before the heading is obscured.
</details>

<details>
<summary><strong>5. Can an element stick to the bottom of the screen instead of the top?</strong></summary>

Yes. By specifying `position: sticky; bottom: 0;`, an element will stick to the bottom of the viewport as the user scrolls down, until its parent container scrolls into full view or leaves the bottom viewport boundary.
</details>

<details>
<summary><strong>6. Does <code>position: sticky</code> create a new stacking context?</strong></summary>

Not automatically by itself. However, when paired with an explicit `z-index` other than `auto`, a sticky element establishes a stacking context.
</details>

<details>
<summary><strong>7. How can you have multiple sticky headers that push each other away in sequence?</strong></summary>

Wrap each sticky section in its own parent container. When section A scrolls, its header sticks to `top: 0`. As section B enters, section A's bottom boundary pushes header A up and out of view, while header B reaches `top: 0` and sticks.
</details>

<details>
<summary><strong>8. Is JavaScript required to detect when a sticky element becomes "stuck"?</strong></summary>

CSS natively does not provide a `:stuck` pseudo-class (though proposed). Currently, detecting the exact moment an element transitions from flow to stuck requires an `IntersectionObserver` in JavaScript observing a sentinel element right above the sticky element.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic Sticky Header
- **Task**: Make a `<header>` stick to the top of the browser window as the user scrolls.

<details>
<summary>View Solution</summary>

```css
header {
  position: sticky;
  top: 0;
  background-color: #ffffff;
  border-bottom: 1px solid #e5e7eb;
  padding: 1rem;
  z-index: 50;
}
```
</details>

### Exercise 2 (Easy): Sticky Footer Action Bar in Mobile View
- **Task**: Pin a checkout action bar to the bottom of the screen inside a container.

<details>
<summary>View Solution</summary>

```css
.checkout-bar {
  position: sticky;
  bottom: 0;
  background-color: #0f172a;
  color: #ffffff;
  padding: 16px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
```
</details>

### Exercise 3 (Medium): Table with Sticky First Column and Sticky Header
- **Task**: Construct a two-dimensional sticky data grid where headers stick to top and row titles stick to left.

<details>
<summary>View Solution</summary>

```css
.table-wrapper {
  max-width: 600px;
  max-height: 400px;
  overflow: auto;
}

table {
  border-collapse: separate;
  border-spacing: 0;
}

/* Top header row */
thead th {
  position: sticky;
  top: 0;
  background-color: #1e293b;
  color: #ffffff;
  z-index: 2;
}

/* Left pinned column */
tbody th {
  position: sticky;
  left: 0;
  background-color: #334155;
  color: #ffffff;
  z-index: 1;
}

/* Intersection corner */
thead th:first-child {
  left: 0;
  z-index: 3;
}
```
</details>

### Exercise 4 (Medium): Flexbox Sticky Sidebar Fix
- **Task**: Fix a two-column flexbox layout where the sidebar refuses to stick.

<details>
<summary>View Solution</summary>

```css
.flex-layout {
  display: flex;
  gap: 24px;
}

.main-content {
  flex: 1;
}

.sidebar {
  width: 260px;
  position: sticky;
  top: 16px;
  /* Crucial fix: overrides default stretch */
  align-self: start;
}
```
</details>

### Exercise 5 (Hard): Stacking Card Timeline Effect
- **Task**: Create a series of cards where each successive card sticks slightly below the previous one (e.g. `top: 20px`, `top: 40px`, etc.).

<details>
<summary>View Solution</summary>

```css
.card-stack {
  display: flex;
  flex-direction: column;
  gap: 40px;
  padding-bottom: 200px;
}

.card {
  position: sticky;
  background: #ffffff;
  border-radius: 12px;
  padding: 32px;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
}

.card:nth-child(1) { top: 20px; }
.card:nth-child(2) { top: 40px; }
.card:nth-child(3) { top: 60px; }
.card:nth-child(4) { top: 80px; }
```
</details>

### Exercise 6 (Hard): Horizontal Scroll with Sticky Category Badges
- **Task**: Build an inline horizontally scrolling chip bar where the "All Categories" chip stays pinned to the left edge while others scroll.

<details>
<summary>View Solution</summary>

```css
.chip-scroller {
  display: flex;
  overflow-x: auto;
  gap: 12px;
  padding: 12px;
  white-space: nowrap;
}

.chip-scroller .all-chip {
  position: sticky;
  left: 12px;
  background-color: #2563eb;
  color: #ffffff;
  z-index: 5;
  box-shadow: 4px 0 6px -2px rgba(0, 0, 0, 0.1);
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Standard Sticky Declaration */
.element {
  position: sticky;
  top: 0;             /* REQUIRED: threshold */
  z-index: 10;        /* Keep above scrolled siblings */
}

/* Key Rules */
/* 1. Parent container MUST be taller than the sticky element. */
/* 2. In Flex/Grid layouts, add 'align-self: start' to prevent height stretching. */
/* 3. NO ancestor can have 'overflow: hidden / auto / scroll'. */
/* 4. Use 'scroll-margin-top' on headings to prevent sticky header occlusion. */
```

---

## 13 Further Reading

- [MDN Web Docs: position sticky](https://developer.mozilla.org/en-US/docs/Web/CSS/position#sticky)
- [CSSWG Drafts: Positioned Layout Module - Sticky Positioning](https://drafts.csswg.org/css-position/#sticky-pos)
- [web.dev: An introduction to position: sticky](https://web.dev/)
- [CSS-Tricks: Position Sticky Guide](https://css-tricks.com/position-sticky-2/)

---

[Previous: 08-positioning-static-relative-absolute-fixed.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/08-positioning-static-relative-absolute-fixed.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/README.md) | [Next: 10-stacking-contexts-and-z-index.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/10-stacking-contexts-and-z-index.md)
