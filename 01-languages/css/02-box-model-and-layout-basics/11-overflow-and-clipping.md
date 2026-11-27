# Overflow and Clipping

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | `02-box-model-and-layout-basics/01-the-box-model.md`, `02-box-model-and-layout-basics/06-inline-vs-block-formatting-contexts.md` |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available (`overflow` CSS2.1, `overflow: clip` Baseline 2021) |
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

In CSS, content does not always fit neatly inside the geometric box allocated to it. When an image is wider than the viewport, when a long unbroken URL exceeds a card's width, or when dynamic server data overflows a fixed-height card, the layout engine encounters **Overflow**.

Handling overflow cleanly is critical:
- **Uncontrolled overflow** creates unwanted horizontal page scrolling (the #1 mobile web layout bug).
- **Misused `overflow: hidden`** silently clips important text, trims focus outlines, or breaks sticky headers and dropdown menus.
- **Modern CSS** introduces explicit two-axis overflow control (`overflow-x`, `overflow-y`), modern non-scrolling clipping (`overflow: clip`), custom scrollbar controls (`scrollbar-gutter`, `scrollbar-width`), and non-rectangular clipping (`clip-path`).

---

## 2 Core Concepts

### The Core Overflow Values

1. **`visible` (Default)**:
   - Content that spills outside the box is rendered visibly outside the padding/border edge.
   - It does not generate scrollbars and does not establish a Block Formatting Context (BFC).
   - Can collide visually with neighboring components.

2. **`hidden`**:
   - Content spilling outside the box's padding edge is clipped and invisible.
   - **Important**: No scrollbars are displayed, but the container **is still programmatically scrollable** via JavaScript (`element.scrollTop += 50`) or mouse wheel events in certain configurations.
   - Establishes a Block Formatting Context (BFC).

3. **`clip` (Modern Baseline 2021)**:
   - Like `hidden`, content outside the box is clipped.
   - **Crucial Difference**: It **completely disables all scrolling**, including programmatic JavaScript scrolling.
   - Does **not** establish a scroll container, meaning it does not break `position: sticky` on ancestor elements!
   - Can be paired with `overflow-clip-margin` to allow overflow to spill slightly before clipping.

4. **`scroll`**:
   - Always reserves and renders scrollbar tracks, regardless of whether the content is actually overflowing or not (preventing layout jump when content expands).

5. **`auto`**:
   - The engine automatically renders scrollbars only when the content size actually exceeds the container's inner dimensions.

### Two-Axis Separation & Dependency Rules

CSS allows setting `overflow-x` (horizontal/inline axis) and `overflow-y` (vertical/block axis) independently:

```css
.card {
  overflow-x: hidden;
  overflow-y: auto;
}
```

> **The CSS Specification Dependency Rule**: If you set one axis to `visible` and the other axis to `hidden`, `auto`, or `scroll`, the specification dictates that the `visible` axis **automatically computes to `auto`**! You cannot have truly visible overflow on one axis while scrolling on the other.

---

## 3 How It Works Internally

When an element has an overflow value other than `visible`, the browser establishes a **Scroll Container** with its own scrollport and clipping boundary:

```
+-------------------------------------------------------------+
| Container Border Edge                                       |
|  +-------------------------------------------------------+  |
|  | Padding Edge (Clipping Boundary for overflow: hidden) |  |
|  |  +-------------------------------------------------+  |  |
|  |  | Content Area (Scrollport)                       |  |  |
|  |  |                                                 |  |  |
|  |  | [In-flow content...]                            |  |  |
|  |  |                                                 |  |  |
|  |  | === OVERFLOW CONTENT SPILL BOUNDARY ===         |  |  |
|  |  | [Hidden / Clipped outside padding edge]         |  |  |
|  |  +-------------------------------------------------+  |  |
|  |                                                       |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

### The Layout Shift Problem: `scrollbar-gutter`

When content grows dynamically and triggers an `auto` scrollbar, the physical scrollbar (typically $15\text{px} - 17\text{px}$ wide on desktop Windows/Linux) encroaches on the content box, causing an abrupt horizontal layout shift.

The modern CSS property `scrollbar-gutter: stable;` (Baseline 2022) permanently reserves space for the scrollbar track even when content does not overflow, eliminating layout shifts completely.

---

## 4 Syntax & Usage

```css
.scroll-container {
  /* Shorthand: sets both axes */
  overflow: auto;

  /* Explicit per-axis */
  overflow-x: hidden;
  overflow-y: auto;

  /* Modern strict clipping (Baseline 2021) */
  overflow: clip;
  overflow-clip-margin: 16px; /* Allow 16px bleed before clipping */

  /* Scrollbar stability (Baseline 2022) */
  scrollbar-gutter: stable both-edges;

  /* Modern scrollbar styling (Baseline 2024) */
  scrollbar-width: thin;
  scrollbar-color: #94a3b8 #f1f5f9;
}
```

---

## 5 Practical Examples

### Example 1: Resilient Responsive Table with Horizontal Scroll

```css
.table-responsive-container {
  width: 100%;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch; /* Momentum scrolling on iOS WebKit */
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.table-responsive-container table {
  width: 100%;
  min-width: 640px; /* Forces horizontal scroll on mobile screens */
  border-collapse: collapse;
}

.table-responsive-container th,
.table-responsive-container td {
  padding: 12px 16px;
  border-bottom: 1px solid #e2e8f0;
  white-space: nowrap;
}
```

### Example 2: Non-breaking Layout with `overflow: clip` and `overflow-clip-margin`

```css
/* Card with decorative elements that can bleed slightly into margins */
.promo-card {
  position: relative;
  border-radius: 12px;
  background-color: #ffffff;
  padding: 24px;
  /* clip does NOT break position: sticky on ancestor headers! */
  overflow: clip;
  overflow-clip-margin: 8px; /* Permits 8px decorative spill */
}

.promo-badge {
  position: absolute;
  top: -6px;
  right: -6px;
  background: #ef4444;
  color: #ffffff;
  padding: 4px 8px;
  border-radius: 4px;
}
```

### Example 3: Edge Case — Truncating Long Unbroken Text with Ellipsis

```css
/* Single-line text truncation */
.truncate-single-line {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 100%;
}

/* Multi-line text truncation (Baseline Widely Available) */
.truncate-multi-line {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 3;
  overflow: hidden;
}
```

### Example 4: Failure Case — `overflow: hidden` Clipping Accessible Focus Rings

```css
/* FAILURE PATTERN */
.card-wrapper {
  border-radius: 8px;
  overflow: hidden; /* Clips children to rounded corners */
}

.card-link {
  display: block;
  padding: 16px;
}

.card-link:focus-visible {
  /* BUG: The 3px outline gets completely clipped by .card-wrapper! */
  outline: 3px solid #2563eb;
}

/* FIXED PATTERN */
.card-link-fixed:focus-visible {
  /* Use an inset outline or internal box-shadow so it stays inside clipping boundaries */
  outline: 2px solid #2563eb;
  outline-offset: -2px;
}
```

### Example 5: High-Performance Custom Scrollable Panel with Smooth Scrolling

```css
.chat-messages-panel {
  height: 480px;
  overflow-y: auto;
  overscroll-behavior-y: contain; /* Prevents scroll chaining to the outer page */
  scrollbar-gutter: stable;       /* Prevents jump when new messages arrive */
  scrollbar-width: thin;
  scrollbar-color: #cbd5e1 transparent;
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
```

---

## 6 Best Practices

1. **Use `overflow: clip` instead of `overflow: hidden` for non-scrollable clipping**: `clip` has zero programmatic scrolling overhead and will not break `position: sticky` on ancestor elements.
2. **Prevent mobile page blowouts with `min-width: 0` on flex/grid items**: Grid and flex items default to `min-width: auto`, which prevents them from shrinking below their content and causes horizontal overflow.
3. **Use `scrollbar-gutter: stable` to eradicate layout shift**: Always reserve scrollbar track space on dynamic pages where content length fluctuates.
4. **Use `overscroll-behavior: contain` on custom scroll areas**: This prevents the annoying "scroll chaining" effect where scrolling inside a modal or dropdown suddenly starts scrolling the background body page.
5. **Always provide keyboard access for scrollable containers**: If an element has `overflow: auto`, add `tabindex="0"` in the HTML so keyboard-only users can scroll it with arrow keys.
6. **Mind your focus outlines**: Never let `overflow: hidden` cut off accessibility focus rings; use negative `outline-offset` or `box-shadow` if clipping is necessary.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
| :--- | :--- | :--- |
| Unexpected horizontal scrollbar on mobile | An absolute element, image, or fixed-width child exceeds 100vw | Audit elements extending past viewport; set `max-width: 100%` |
| `overflow-x: visible` auto-converts to `auto` | CSS spec dictates if one axis is non-visible, the other cannot be `visible` | Restructure layout so clipping happens on a separate container |
| Focus rings sliced off at borders | `overflow: hidden` clips everything past the box boundary | Use `outline-offset: -2px` or `overflow-clip-margin` |
| Scrollbar appearing and disappearing causes jitter | Content height fluctuates around container threshold | Add `scrollbar-gutter: stable` |
| Sticky header stops sticking | An ancestor has `overflow: hidden`, capturing the scrollport | Replace ancestor's `overflow: hidden` with `overflow: clip` |
| Non-scrollable scrollport for keyboard users | `overflow: auto` container without `tabindex="0"` is inaccessible to keyboard | Add `tabindex="0"` to the scrollable container in HTML |

---

## 8 Performance, Accessibility & Browser Support

### Scroll Container Performance

- When an element is given `overflow: auto` or `scroll`, browser engines instantiate a separate scrolling layer.
- Ensure that smooth-scrolling containers use `overscroll-behavior: contain` to prevent unnecessary main-thread event propagation to the document root.

### Accessibility Standards (WCAG 2.1.1 & 1.4.10)

- **WCAG 1.4.10 Reflow (Level AA)**: Content must reflow without loss of information or functionality, and without requiring scrolling in two dimensions (horizontal and vertical simultaneously), down to a width of $320\text{px}$.
- **Keyboard Scrollability**: Screen-reader and keyboard users navigating without a mouse cannot scroll an `overflow: auto` `<div>` unless that container is keyboard-focusable (`tabindex="0"`). Always include an `aria-label` describing the scrollable region (e.g., `aria-label="Transaction History"`).

---

## 9 Comparison / When to Use What

| Value | Clips Content? | Shows Scrollbars? | Programmatic Scroll? | Breaks `position: sticky`? |
| :--- | :--- | :--- | :--- | :--- |
| `visible` | No | No | No | No |
| `hidden` | Yes | No | Yes (`element.scrollTop`) | **Yes** |
| `clip` | Yes | No | **No** (Strict clip) | **No** |
| `auto` | Yes | Only when needed | Yes | Yes |
| `scroll` | Yes | Always visible | Yes | Yes |

---

## 10 Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>overflow: hidden</code> and <code>overflow: clip</code>?</strong></summary>

While both clip content to the padding box without displaying scrollbars, `overflow: hidden` establishes a scroll container that allows programmatic scrolling via JavaScript (`scrollTop`), which can interfere with ancestor sticky positioning. In contrast, `overflow: clip` completely forbids all scrolling (both user and programmatic), does not establish a scroll container, and does not break ancestor sticky positioning.
</details>

<details>
<summary><strong>2. Why does setting <code>overflow-x: visible</code> and <code>overflow-y: auto</code> not work as intuitively expected?</strong></summary>

Per the W3C CSS Overflow specification, if either `overflow-x` or `overflow-y` is set to anything other than `visible` (`auto`, `scroll`, or `hidden`), the other axis automatically computes from `visible` to `auto`. Thus, `overflow-x: visible` becomes `overflow-x: auto`.
</details>

<details>
<summary><strong>3. What does <code>scrollbar-gutter: stable</code> do, and what problem does it solve?</strong></summary>

It reserves space for the scrollbar track within the layout even when the content does not overflow. This eliminates the abrupt layout shift (jank) that occurs when dynamic content loads and causes a scrollbar to appear suddenly.
</details>

<details>
<summary><strong>4. How does <code>overscroll-behavior: contain</code> improve user experience in nested scroll areas?</strong></summary>

When a user scrolls to the bottom of a nested scroll container (such as a modal or dropdown), standard browser behavior chains the scroll to the parent page, causing the main webpage to scroll. Setting `overscroll-behavior: contain` isolates scrolling to that specific container.
</details>

<details>
<summary><strong>5. What is the #1 cause of accidental horizontal scrolling on mobile websites, and how do you diagnose it?</strong></summary>

Elements with fixed pixel widths larger than 320px, unconstrained media (`img`, `video`), or flex/grid items with default `min-width: auto`. You can diagnose it by running `document.querySelectorAll('*').forEach(el => el.offsetWidth > window.innerWidth && console.log(el))` in the DevTools console.
</details>

<details>
<summary><strong>6. What is <code>overflow-clip-margin</code>?</strong></summary>

A companion property to `overflow: clip` that specifies how far outside the element's box content can expand before being clipped. For example, `overflow-clip-margin: 10px;` allows decorative borders or shadows to bleed 10px into the surrounding space.
</details>

<details>
<summary><strong>7. How do you implement multi-line text truncation in modern CSS?</strong></summary>

Using the standard web baseline combination:
```css
display: -webkit-box;
-webkit-box-orient: vertical;
-webkit-line-clamp: 3;
overflow: hidden;
```
This truncates text after 3 lines and renders an ellipsis (`...`).
</details>

<details>
<summary><strong>8. Why is adding <code>tabindex="0"</code> essential on custom <code>overflow: auto</code> elements?</strong></summary>

Because without `tabindex="0"`, keyboard-only users cannot focus the container, making it impossible for them to scroll through the hidden content using keyboard arrow or Page Down keys.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic Card Overflow Clipping
- **Task**: Prevent an oversized image inside a card with rounded corners from poking outside the border.

<details>
<summary>View Solution</summary>

```css
.card {
  width: 320px;
  border-radius: 12px;
  overflow: clip; /* Clean clipping without scroll side effects */
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}

.card img {
  width: 100%;
  height: 200px;
  object-fit: cover;
  display: block;
}
```
</details>

### Exercise 2 (Easy): Reserving Scrollbar Gutter
- **Task**: Prevent layout shifting in a live feed container when new comments make the container exceed its fixed height.

<details>
<summary>View Solution</summary>

```css
.comments-feed {
  height: 350px;
  overflow-y: auto;
  scrollbar-gutter: stable; /* Permanently reserves track space */
  border: 1px solid #cbd5e1;
  padding: 16px;
}
```
</details>

### Exercise 3 (Medium): Accessible Horizontal Code Block
- **Task**: Style a `<pre><code>` block that scrolls horizontally on small screens without breaking page width, with accessible keyboard support.

<details>
<summary>View Solution</summary>

```css
pre {
  max-width: 100%;
  overflow-x: auto;
  background-color: #0f172a;
  color: #f8fafc;
  padding: 16px;
  border-radius: 8px;
  scrollbar-width: thin;
  scrollbar-color: #475569 transparent;
}

pre:focus-visible {
  outline: 2px solid #38bdf8;
  outline-offset: 2px;
}
```
</details>

### Exercise 4 (Medium): Truncate Article Teaser with 2-Line Ellipsis
- **Task**: Limit an article summary paragraph to exactly 2 lines followed by an ellipsis.

<details>
<summary>View Solution</summary>

```css
.article-summary {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
  overflow: hidden;
  text-overflow: ellipsis;
  line-height: 1.5;
  color: #475569;
}
```
</details>

### Exercise 5 (Hard): Modal Drawer with Isolated Scroll Chains
- **Task**: Build a slide-out drawer that scrolls its own content smoothly without ever scrolling the background webpage when it hits the top or bottom edge.

<details>
<summary>View Solution</summary>

```css
.slideout-drawer {
  position: fixed;
  top: 0;
  right: 0;
  width: 380px;
  height: 100vh;
  background-color: #ffffff;
  overflow-y: auto;
  overscroll-behavior-y: contain; /* Prevents background page scroll chaining */
  box-shadow: -4px 0 15px rgba(0, 0, 0, 0.1);
  padding: 24px;
}
```
</details>

### Exercise 6 (Hard): Circular Avatar with Decorative Badge Bleeding Out
- **Task**: Create a circular avatar that clips its internal image but allows a status badge to sit partially outside the circle using `overflow: clip` and `overflow-clip-margin`.

<details>
<summary>View Solution</summary>

```css
.avatar-container {
  position: relative;
  width: 64px;
  height: 64px;
}

.avatar-image-mask {
  width: 100%;
  height: 100%;
  border-radius: 50%;
  overflow: clip;
}

.avatar-image-mask img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.status-indicator {
  position: absolute;
  bottom: 2px;
  right: 2px;
  width: 14px;
  height: 14px;
  border-radius: 50%;
  background-color: #22c55e;
  border: 2px solid #ffffff;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Core Overflow Values */
overflow: visible;  /* Default. Overflows freely. */
overflow: hidden;   /* Clips. Enables programmatic scroll. */
overflow: clip;     /* Clips strictly. No programmatic scroll. Doesn't break sticky. */
overflow: auto;     /* Scrollbars only when content exceeds bounds. */
overflow: scroll;   /* Always reserves scrollbar tracks. */

/* Modern Stability & Control */
scrollbar-gutter: stable;          /* Stops layout shift */
overscroll-behavior: contain;      /* Stops scroll chaining to body */
overflow-clip-margin: 12px;        /* Bleed margin for overflow: clip */
scrollbar-width: thin;             /* Compact modern scrollbar */
```

---

## 13 Further Reading

- [MDN Web Docs: overflow](https://developer.mozilla.org/en-US/docs/Web/CSS/overflow)
- [MDN Web Docs: overflow: clip](https://developer.mozilla.org/en-US/docs/Web/CSS/overflow#clip)
- [W3C CSS Overflow Module Level 3](https://www.w3.org/TR/css-overflow-3/)
- [web.dev: Unlocking new capabilities with scrollbar-gutter](https://web.dev/scrollbar-gutter/)

---

[Previous: 10-stacking-contexts-and-z-index.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/10-stacking-contexts-and-z-index.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/README.md) | [Next: 12-logical-properties-and-writing-modes.md](file:///e:/Learning/engineering-notes/01-languages/css/02-box-model-and-layout-basics/12-logical-properties-and-writing-modes.md)
