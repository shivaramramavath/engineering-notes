# Box Alignment Basics

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Modern Standard |
| **Prerequisites** | `02-box-model-and-layout-basics/01-the-box-model.md`, `02-box-model-and-layout-basics/05-display-property.md` |
| **Estimated Reading Time** | 20 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`justify-content`, `align-items` Baseline 2017; block alignment Baseline 2024) |
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

Historically, vertical centering in CSS was the subject of endless internet jokes and complex layout workarounds: `vertical-align: middle` (which only worked on table cells and inline elements), negative margin hacks (`margin-top: -50px`), or absolute positioning with transforms (`transform: translate(-50%, -50%)`).

The **CSS Box Alignment Module Level 3** unified alignment across all layout models—Block, Flexbox, Grid, and Multicol—under a single, consistent vocabulary.

With modern Box Alignment:
- Horizontal and vertical distribution is solved using uniform properties: `justify-*` and `align-*`.
- Single-line centering is as concise as `place-items: center` or `align-content: center` (now supported natively in block containers across modern browsers).
- Element spacing is standardized with `gap`, replacing cumbersome child margin resets.

---

## 2 Core Concepts

### The Two Fundamental Alignment Axes

Every alignment operation occurs along one of two primary axes:

1. **The Inline (Main / Justify) Axis**:
   - Governed by properties prefixed with `justify-`:
     - `justify-content`: Distributes extra space between and around items along the inline axis.
     - `justify-items`: Aligns all items along the inline axis within their allocated track/slot.
     - `justify-self`: Overrides inline alignment for an individual item.
2. **The Block (Cross / Align) Axis**:
   - Governed by properties prefixed with `align-`:
     - `align-content`: Distributes extra space along the block axis (when multiple rows/lines exist).
     - `align-items`: Aligns all items along the block axis within their allocated slot.
     - `align-self`: Overrides block alignment for an individual item.

### Alignment Terminology Matrix

| Scope | Inline Axis (Horizontal in LTR) | Block Axis (Vertical in LTR) | Shorthand |
| :--- | :--- | :--- | :--- |
| **All tracks/content** | `justify-content` | `align-content` | `place-content` |
| **All individual items** | `justify-items` | `align-items` | `place-items` |
| **Single item override** | `justify-self` | `align-self` | `place-self` |

### Modern Milestone: `align-content` in Block Layout (Baseline 2024)

Historically, `align-content` and `align-items` only functioned inside Flexbox and Grid containers. As of 2024 (Baseline 2024 across Chrome 123+, Safari 17.4+, Firefox 125+), **`align-content: center` works directly on standard block containers (`display: block`)**!

```css
/* Centering in 2024+ without changing display to flex or grid! */
.hero-block {
  display: block;
  min-height: 300px;
  align-content: center;
}
```

---

## 3 How It Works Internally

During layout computation, the engine checks for remaining unused space inside the container after sizing all in-flow items:

```
+-------------------------------------------------------------+
| Container with Free Space                                   |
|                                                             |
| [align-content: start]          [align-content: center]     |
| +-------------------------+     +-------------------------+ |
| | Item A                  |     |                         | |
| +-------------------------+     | +---------------------+ | |
| | Item B                  |     | | Item A              | | |
| +-------------------------+     | +---------------------+ | |
| | (Unused Space Below)    |     | | Item B              | | |
| |                         |     | +---------------------+ | |
| |                         |     |                         | |
| +-------------------------+     +-------------------------+ |
|                                                             |
| [justify-content: space-between]                            |
| +---------------------------------------------------------+ |
| | [Item 1]              [Item 2]                 [Item 3] | |
| +---------------------------------------------------------+ |
+-------------------------------------------------------------+
```

### The `safe` vs `unsafe` Alignment Keywords

A common issue with `center` alignment occurs when content overflows its container. Under standard (unsafe) centering, the content expands equally in both directions, pushing the top or left half outside the viewport where it **cannot be scrolled to**.

The `safe` keyword protects against this data-loss bug:

```css
/* If container is smaller than content, aligns to 'start' so it can be scrolled */
.scrollable-dialog {
  align-content: safe center;
}
```

---

## 4 Syntax & Usage

```css
.container {
  /* Content distribution (spacing out tracks/lines) */
  justify-content: start | end | center | space-between | space-around | space-evenly;
  align-content: start | end | center | space-between | space-around | space-evenly;
  place-content: center space-between; /* align-content justify-content */

  /* Item alignment within tracks/slots */
  justify-items: start | end | center | stretch;
  align-items: start | end | center | stretch | baseline;
  place-items: center; /* align-items justify-items */

  /* Gaps between items */
  gap: 1.5rem; /* row-gap and column-gap */
  row-gap: 1rem;
  column-gap: 2rem;
}

.item {
  /* Self overrides */
  justify-self: end;
  align-self: center;
  place-self: center end;
}
```

---

## 5 Practical Examples

### Example 1: Perfect Two-Axis Centering (The 1-Line Solution)

```css
.modal-viewport-wrapper {
  display: grid;
  place-items: center; /* Centers horizontally and vertically */
  min-height: 100vh;
  padding: 16px;
}

.modal-card {
  width: min(100%, 480px);
  background-color: #ffffff;
  border-radius: 8px;
  padding: 24px;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);
}
```

### Example 2: Baseline Text Alignment for Mixed Font Sizes

When aligning icons and text of varying font sizes, `align-items: baseline` aligns the typographic baseline of characters rather than their bounding boxes:

```css
.price-display {
  display: flex;
  align-items: baseline; /* Aligns bottoms of text characters */
  gap: 8px;
}

.price-currency {
  font-size: 1rem;
  font-weight: 500;
  color: #64748b;
}

.price-amount {
  font-size: 2.5rem;
  font-weight: 800;
  color: #0f172a;
}

.price-period {
  font-size: 0.875rem;
  color: #64748b;
}
```

### Example 3: Edge Case — Safe Alignment Preventing Top Clipped Content

```css
.scrollable-modal-overlay {
  display: flex;
  /* If the modal dialog is taller than the mobile screen,
     'safe center' falls back to 'flex-start' so the user can scroll to top! */
  align-items: safe center;
  justify-content: center;
  min-height: 100vh;
  overflow-y: auto;
  padding: 24px;
}

.tall-dialog-card {
  background: #ffffff;
  border-radius: 12px;
  padding: 32px;
  width: min(90vw, 540px);
}
```

### Example 4: Failure Case — `justify-items` Having Zero Effect in Flexbox

```css
/* FAILURE: justify-items is ignored in Flex containers! */
.flex-toolbar {
  display: flex;
  /* BUG: In standard 1D Flexbox, justify-items is NOT supported.
     Flex items do not have individual tracks along the main axis. */
  justify-items: center; /* Has NO effect */
}

/* FIXED RECOVERY */
.flex-toolbar-fixed {
  display: flex;
  /* Use justify-content to distribute items along the main axis */
  justify-content: space-between;
  /* Use align-items for cross-axis item alignment */
  align-items: center;
}
```

### Example 5: Responsive Card Grid with Unified `gap`

```css
.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 24px; /* Eliminates legacy margin-right / margin-bottom hacks */
  align-items: stretch; /* Cards stretch to equal height */
}

.feature-card {
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 24px;
  display: flex;
  flex-direction: column;
}

.feature-card-cta {
  margin-top: auto; /* Pushes button cleanly to bottom of equal-height card */
}
```

---

## 6 Best Practices

1. **Use `gap` instead of child margins**: `gap` automatically applies spacing strictly between items, completely eliminating `:last-child` margin resets.
2. **Use `place-items: center` in CSS Grid for full centering**: It is the cleanest, most performant way to center an unknown child box.
3. **Use `align-items: baseline` for text and inline badges**: Prevents awkward vertical visual misalignment between elements with differing font sizes or line heights.
4. **Use `safe center` for scrollable modal dialogs**: Prevents the top of tall content from being irrevocably clipped off-screen on small mobile viewports.
5. **Remember that `justify-items` does NOT work in Flexbox**: Use `justify-content` or `margin: auto` on flex items instead.
6. **Leverage modern `align-content: center` on block elements**: For single-column vertical centering without triggering a new formatting context.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
| :--- | :--- | :--- |
| `justify-items: center` in Flexbox | The CSS Flexbox spec does not support `justify-items` | Use `justify-content: center` |
| Centering modal with standard `center` cuts off top on mobile | Unsafe centering pushes top half outside viewport scrollport | Use `align-items: safe center` |
| `vertical-align: middle` on a `<div>` | `vertical-align` only works on inline boxes and table cells | Use `align-content: center` or Flex/Grid |
| Negative margin hacks on last child | Legacy workarounds are brittle when items wrap | Replace with standard `gap: 16px` |
| Confusing `place-items` with `place-content` | `place-items` targets items in tracks; `place-content` distributes tracks | Use `place-items` for single centered items |
| Forgetting `min-height` when vertical centering | Centering inside a box with $0\text{px}$ free space looks identical to start | Declare explicit `min-height` on container |

---

## 8 Performance, Accessibility & Browser Support

### Layout Computation Performance

- Box alignment properties are native engine primitives. Unlike JavaScript height-matching scripts (`matchHeight.js`), CSS box alignment resolves in a single layout pass.
- Using `gap` is significantly more performant than dynamically checking `:nth-child` or `:last-child` selectors for margin suppression.

### Accessibility

- Visual alignment does not alter document DOM order. Ensure that items aligned with `justify-content: flex-end` or `order` remain in logical DOM sequence for screen readers and keyboard tab navigation.

---

## 9 Comparison / When to Use What

| Goal | Technique | Why Use It |
| :--- | :--- | :--- |
| Absolute 2D Centering | `display: grid; place-items: center;` | Cleanest 2-line code |
| Vertical centering in Block | `display: block; align-content: center;` | Modern Baseline 2024, no display change |
| Distribute items with space between | `display: flex; justify-content: space-between;` | Classic header/navbar pattern |
| Baseline text & icon alignment | `display: flex; align-items: baseline;` | Perfect typographic baseline alignment |
| Safe scrollable modal dialog | `display: flex; align-items: safe center;` | Prevents top-edge clipping on mobile |

---

## 10 Interview Questions

<details>
<summary><strong>1. Why does <code>justify-items</code> not work in a standard Flexbox container?</strong></summary>

Flexbox is a one-dimensional layout model where items are packed along a single main axis. Because items in a flex line do not have separate predefined "tracks" along that main axis (unlike CSS Grid cells), there is no slot within which an individual item could be justified. Spacing along the main axis is controlled exclusively by `justify-content` across all items, or via `margin: auto` on individual flex items.
</details>

<details>
<summary><strong>2. What is the difference between <code>align-items</code> and <code>align-content</code>?</strong></summary>

`align-items` dictates how individual items are aligned along the cross axis within their specific line or grid track. In contrast, `align-content` dictates how the lines or grid tracks themselves are distributed within the container when there is extra free space along the cross axis (relevant in multi-line flex containers or multi-row grids).
</details>

<details>
<summary><strong>3. What does the <code>safe</code> keyword do in box alignment properties?</strong></summary>

The `safe` keyword (e.g. `align-items: safe center`) prevents data-loss caused by overflow. If an item is larger than its container, normal `center` pushes equal amounts outside the top and bottom boundaries, making the top inaccessible via scrolling. With `safe center`, if overflow occurs, the alignment automatically falls back to `start` so all content remains scrollable.
</details>

<details>
<summary><strong>4. Can <code>align-content: center</code> be used on a regular block container (<code>display: block</code>)?</strong></summary>

Yes. Under the modern CSS Box Alignment specification (Baseline 2024, supported in Chrome 123+, Safari 17.4+, and Firefox 125+), `align-content` functions directly on standard block containers, allowing vertical centering without needing to switch `display` to `flex` or `grid`.
</details>

<details>
<summary><strong>5. How does <code>place-items</code> differ from <code>place-content</code>?</strong></summary>

`place-items` is shorthand for `align-items` and `justify-items` (aligning items within their slots). `place-content` is shorthand for `align-content` and `justify-content` (distributing the overall content and tracks within the container).
</details>

<details>
<summary><strong>6. How does <code>margin: auto</code> interact with Box Alignment in Flexbox?</strong></summary>

In Flexbox, `margin: auto` absorbs all available free space along that axis before any `justify-content` or `align-items` properties are evaluated. An item with `margin-left: auto` will consume all remaining horizontal space, pushing itself all the way to the right edge.
</details>

<details>
<summary><strong>7. What values does <code>align-items</code> accept, and which is the default in Flexbox?</strong></summary>

Values include `stretch` (default in Flexbox), `flex-start` / `start`, `flex-end` / `end`, `center`, and `baseline`.
</details>

<details>
<summary><strong>8. Why is <code>gap</code> superior to applying margins on child elements?</strong></summary>

`gap` creates space exclusively between adjacent items, avoiding the need for `:last-child` or `:first-child` margin resets. Furthermore, `gap` functions seamlessly when flex items wrap across multiple lines.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Perfect 2D Centering
- **Task**: Center a login card horizontally and vertically inside a full-height container using pure CSS Grid shorthand.

<details>
<summary>View Solution</summary>

```css
.login-page {
  display: grid;
  place-items: center;
  min-height: 100vh;
}

.login-card {
  width: min(90vw, 400px);
  padding: 32px;
  background-color: #ffffff;
  border-radius: 8px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}
```
</details>

### Exercise 2 (Easy): Classic Navigation Bar
- **Task**: Space out a brand logo on the left and navigation links on the right using Flexbox alignment.

<details>
<summary>View Solution</summary>

```css
.navbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
  background-color: #0f172a;
  color: #ffffff;
}
```
</details>

### Exercise 3 (Medium): Baseline Alignment for Pricing Table
- **Task**: Vertically align a large price number ($99) and a small recurring suffix (/month) so their baseline font edges align.

<details>
<summary>View Solution</summary>

```css
.pricing-row {
  display: flex;
  align-items: baseline;
  gap: 6px;
}

.amount {
  font-size: 3rem;
  font-weight: 700;
  line-height: 1;
}

.period {
  font-size: 1rem;
  color: #64748b;
}
```
</details>

### Exercise 4 (Medium): Push Individual Action Button via Margin Auto
- **Task**: In a three-item flex toolbar, keep two items grouped on the left and push the third item to the right edge.

<details>
<summary>View Solution</summary>

```css
.toolbar {
  display: flex;
  align-items: center;
  gap: 12px;
}

.toolbar-logout {
  margin-left: auto; /* Absorbs free space and pushes to far right */
}
```
</details>

### Exercise 5 (Hard): Safe Scrollable Centered Modal
- **Task**: Center a modal dialog vertically and horizontally, ensuring that if the screen height is shorter than the modal, it can still be scrolled from the top.

<details>
<summary>View Solution</summary>

```css
.modal-overlay {
  position: fixed;
  inset: 0;
  background-color: rgba(0, 0, 0, 0.5);
  display: flex;
  justify-content: center;
  align-items: safe center; /* Prevents top clipping on small screens */
  overflow-y: auto;
  padding: 24px;
}

.modal-content {
  background: #ffffff;
  border-radius: 8px;
  padding: 24px;
  max-width: 500px;
}
```
</details>

### Exercise 6 (Hard): Modern Block Centering (Baseline 2024)
- **Task**: Center content vertically inside a hero banner container using `display: block` and `align-content`.

<details>
<summary>View Solution</summary>

```css
.hero-block-banner {
  display: block;
  min-height: 350px;
  align-content: center; /* Baseline 2024 on block containers */
  text-align: center;
  background-color: #f8fafc;
  padding-inline: 24px;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Container Box Alignment */
place-items: center;         /* align-items: center + justify-items: center */
place-content: center;       /* align-content: center + justify-content: center */

/* Spacing */
gap: 16px;                   /* Unified gutter between items */

/* Modern Block Vertical Centering (Baseline 2024) */
.block-element {
  display: block;
  align-content: center;
}

/* Safe Alignment */
align-items: safe center;    /* Avoids off-screen top clipping on overflow */
```

---

## 13 Further Reading

- [MDN Web Docs: CSS Box Alignment](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_box_alignment)
- [W3C CSS Box Alignment Module Level 3](https://www.w3.org/TR/css-align-3/)
- [web.dev: align-content in block layout](https://web.dev/blog/align-content-block)
- [Smashing Magazine: How To Center Things With CSS In 2024](https://www.smashingmagazine.com/2024/05/how-to-center-things-with-css-in-2024/)

---

[Previous: 12-logical-properties-and-writing-modes.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/12-logical-properties-and-writing-modes.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/README.md) | [Next: Module 03 - Flexbox Deep Dive](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md)
