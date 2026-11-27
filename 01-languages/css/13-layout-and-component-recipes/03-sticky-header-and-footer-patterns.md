# 03. Sticky Header and Footer Patterns

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | CSS Positioned Layout Level 3 / Box Alignment Level 3 / W3C Recommendation |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Modern DevTools, Mobile Safe-Area Inset Emulation |
| **Prerequisites** | `02-box-model-and-layout-basics/03-positioning.md`, `13-layout-and-component-recipes/02-holy-grail-and-sidebar-layouts.md` |

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
Two common layout flaws degrade the perceived quality of web applications:
1. **The Floating Footer Bug**: On pages with minimal content (such as a 404 page, a simple confirmation message, or a short login form), a standard footer floats in the middle of the screen, creating an awkward visual void beneath it.
2. **The Fragile Fixed Header**: Historically, keeping a navigation bar at the top of the screen required `position: fixed`. This took the header completely out of document flow, requiring hardcoded `padding-top: 80px` on `<body>`. If the header expanded (e.g. displaying an alert banner), body content was covered and obscured. Furthermore, clicking anchor links (`#section-2`) caused headings to scroll directly underneath the fixed header.

### The Solution
Modern CSS provides robust, zero-hack solutions:
- **`position: sticky; top: 0;`**: Keeps headers in the natural document flow until the user scrolls, seamlessly locking them to the top edge without requiring body padding offsets.
- **Scroll-Driven Elevation Shadows**: Automatically elevating the sticky header with a drop-shadow the instant scrolling begins, using modern CSS scroll-driven animations without JavaScript.
- **`scroll-padding-top`**: Instructs the browser to offset internal anchor navigation targets so focused content is never obscured by the header.
- **The Modern Sticky Footer (`100dvh` + `margin-top: auto`)**: Guarantees that on short pages, the footer is anchored to the bottom of the screen, while on long pages, it pushes down naturally.

---

## 2. Core Concepts

### 1. Pinned Footer Mechanics

```
SHORT PAGE (Minimal Content)              LONG PAGE (Extensive Content)
┌───────────────────────────────┐         ┌───────────────────────────────┐
│ HEADER                        │         │ HEADER                        │
├───────────────────────────────┤         ├───────────────────────────────┤
│ Short Content (2 lines)       │         │ Extensive Content...          │
│                               │         │ Multiple paragraphs...        │
│ [margin-top: auto consumes    │         │ Data tables and charts...     │
│  all remaining vertical space]│         │                               │
├───────────────────────────────┤         ├───────────────────────────────┤
│ FOOTER (Pinned to bottom!)    │         │ FOOTER (Pushed down naturally)│
└───────────────────────────────┘         └───────────────────────────────┘
```

1. **Flexbox Footer Pattern**: The body container is given `min-height: 100dvh; display: flex; flex-direction: column;`. The footer is given `margin-top: auto;`.
2. **CSS Grid Footer Pattern**: The body is given `min-height: 100dvh; display: grid; grid-template-rows: auto 1fr auto;`.

### 2. Sticky Header Mechanics
Unlike `position: fixed` (which is relative to the viewport and removed from flow), `position: sticky` is treated as **relative positioning** until its scroll threshold is reached within its scrolling container, at which point it acts as fixed **within its parent container's bounds**.

---

## 3. How It Works Internally

### The Sticking Container Constraint
A sticky element can **never travel outside its parent container**:

```
PARENT SECTION CONTAINER (Height: 800px)
┌───────────────────────────────────────────────┐
│ ┌───────────────────────────────────────────┐ │
│ │ Sticky Bar (top: 0)                       │ │ ──► Sticks while parent is in view!
│ └───────────────────────────────────────────┘ │
│ Content inside parent...                      │
│ More content...                               │
└───────────────────────────────────────────────┘
  ▲
  └── When this parent scrolls past the viewport, the sticky bar UNSTICKS
      and scrolls off-screen with its parent!
```
If you want a header to stick for the **entire page duration**, it must be a direct child of `<body>` or the top-level page wrapper, rather than wrapped inside a shallow container.

---

## 4. Syntax & Usage

### 1. The Pinned Footer Pattern (Flexbox)
```css
html {
  /* Prevent scroll jumps on mobile */
  scroll-behavior: smooth;
  scroll-padding-top: 5rem; /* Offsets anchor links beneath 4rem header */
}

body {
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
  margin: 0;
}

main {
  flex: 1 0 auto; /* Ensures main absorbs remaining vertical space */
}

footer {
  margin-top: auto; /* Guarantees footer is pinned to the bottom on short pages */
}
```

### 2. Sticky Header with Scroll-Driven Elevation Shadow
Modern CSS allows the header to remain flat when at the top of the page, and automatically transition a shadow once scrolling begins:

```css
.site-header {
  position: sticky;
  top: 0;
  z-index: 100;
  background-color: rgba(255, 255, 255, 0.9);
  backdrop-filter: blur(12px);
  border-bottom: 1px solid #e2e8f0;
  padding: 1rem 1.5rem;

  /* Modern Scroll-Driven Animation: triggers elevation on scroll */
  animation: header-elevate linear both;
  animation-timeline: scroll();
  animation-range: 0px 50px;
}

@keyframes header-elevate {
  to {
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);
    background-color: rgba(255, 255, 255, 0.98);
  }
}
```

---

## 5. Practical Examples

### Example 1: Full-Page Application with Sticky Header, Sticky Footer, and Scrollable Body
A complete responsive application layout:

```html
<header class="app-header">
  <div class="brand">CloudConsole</div>
  <nav class="nav-links">
    <a href="#servers">Servers</a>
    <a href="#billing">Billing</a>
  </nav>
</header>

<main class="app-content">
  <h1>Server Infrastructure</h1>
  <p>Overview of active cloud virtual machine instances.</p>
</main>

<footer class="app-footer">
  <p>&copy; 2026 CloudConsole Technologies Inc. All rights reserved.</p>
</footer>
```

```css
body {
  margin: 0;
  font-family: system-ui, -apple-system, sans-serif;
  color: #0f172a;
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
}

.app-header {
  position: sticky;
  top: 0;
  z-index: 100;
  height: 4rem;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding-inline: 2rem;
  background-color: #ffffff;
  border-bottom: 1px solid #e2e8f0;
}

.nav-links {
  display: flex;
  gap: 1.5rem;
}

.nav-links a {
  color: #475569;
  text-decoration: none;
  font-weight: 500;
}

.nav-links a:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
}

.app-content {
  flex: 1 0 auto;
  max-width: 1000px;
  width: 100%;
  margin: 0 auto;
  padding: 2.5rem 1.5rem;
}

.app-footer {
  margin-top: auto;
  padding: 1.5rem 2rem;
  background-color: #0f172a;
  color: #94a3b8;
  font-size: 0.875rem;
  text-align: center;
}
```

### Example 2: Sticky Table Headers and Sticky First Column
When navigating wide, tall data tables:

```html
<div class="table-container" tabindex="0" role="region" aria-label="Financial Metrics">
  <table class="sticky-table">
    <thead>
      <tr>
        <th class="sticky-col">Project</th>
        <th>Region</th>
        <th>Budget</th>
        <th>Spend</th>
        <th>Variance</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td class="sticky-col">Core Gateway</td>
        <td>us-east-1</td>
        <td>$120,000</td>
        <td>$98,400</td>
        <td>+$21,600</td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.table-container {
  max-height: 400px;
  max-width: 600px;
  overflow: auto;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
}

.sticky-table {
  border-collapse: separate; /* Required for sticky table cells */
  border-spacing: 0;
  width: 100%;
}

/* 1. STICKY THEAD: Header rows stick to the top when scrolling down */
.sticky-table thead th {
  position: sticky;
  top: 0;
  z-index: 10;
  background-color: #0f172a;
  color: #ffffff;
  padding: 0.75rem 1rem;
}

/* 2. STICKY FIRST COLUMN: Sticks to the left when scrolling right */
.sticky-table .sticky-col {
  position: sticky;
  left: 0;
  z-index: 5;
  background-color: #ffffff;
  border-right: 1px solid #e2e8f0;
}

/* 3. INTERSECTION CELL (Top-Left): Highest z-index */
.sticky-table thead th.sticky-col {
  z-index: 20;
  background-color: #0f172a;
}
```

### Example 3: Mobile Safe Area Header Padding
Ensure sticky headers accommodate the iPhone sensor notch and Android camera cutouts:

```css
.mobile-header {
  position: sticky;
  top: 0;
  z-index: 100;
  background-color: #ffffff;
  /* Top padding incorporates device hardware notch */
  padding-top: max(1rem, env(safe-area-inset-top));
  padding-inline: max(1rem, env(safe-area-inset-left)) max(1rem, env(safe-area-inset-right));
}
```

---

## 6. Best Practices

1. **Always Set `scroll-padding-top` on `html`**: Ensure internal in-page anchor navigation targets (`#faq-2`) stop beneath the sticky header with comfortable breathing room.
2. **Use `min-height: 100dvh` for Pinned Footers**: Never use fixed `height: 100vh`; `min-height: 100dvh` adapts dynamically to mobile browser address bars.
3. **Use `border-collapse: separate` for Sticky Tables**: Sticky positioning on `<th>` and `<td>` elements fails in several browsers if `border-collapse: collapse` is used.
4. **Elevate Z-Index on Sticky Headers**: Ensure sticky headers have `z-index: 50` or higher so relative positioned cards and text do not bleed over the navigation bar during scroll.
5. **Ensure the Sticky Header Parent is the Body/Viewport Container**: Remember that sticky elements cannot stick outside their immediate parent container.
6. **Incorporate Mobile Safe-Area Insets**: Use `env(safe-area-inset-top)` so sticky mobile headers do not overlap with device hardware status indicators.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Omitting `top: 0` on `position: sticky`** | The element never sticks because it lacks a sticking threshold coordinate. | Always define `top: 0;` (or other offset). |
| **Ancestor element has `overflow: hidden`** | Completely destroys sticky positioning on all descendant elements. | Use `overflow: clip` or remove `overflow` from ancestors. |
| **Using `height: 100vh` on the page container** | On long pages, the footer is pushed off-screen and content is trapped in nested scrollbars. | Use `min-height: 100dvh;`. |
| **Using `border-collapse: collapse` on sticky tables** | Sticky table headers flicker or fail to render borders during scroll in Blink. | Use `border-collapse: separate; border-spacing: 0;`. |
| **Forgetting `scroll-padding-top` on the root** | Clicking an anchor link scrolls the targeted heading directly underneath the sticky header. | Set `html { scroll-padding-top: 4.5rem; }`. |
| **Placing a sticky header inside a nested card** | Header only sticks while the card is in view, then scrolls off with the card. | Make sticky header a direct child of the top-level layout wrapper. |

---

## 8. Performance, Accessibility & Browser Support

### Universal Modern Support
- `position: sticky`: Supported in all modern browsers (Chrome 56+, Firefox 32+, Safari 13+).
- `scroll-padding-top`: Baseline supported across all modern browsers.
- CSS Scroll-Driven Animations: Supported in Chrome 115+, with graceful progressive degradation in Firefox and Safari.

---

## 9. Comparison / When to Use What

| Technique | Document Flow | Offsets Required | Overflow Parent Dependency | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **`position: sticky; top: 0`** | In Flow | Zero body padding | Broken by `overflow: hidden` | Modern website navigation headers |
| **`position: fixed; top: 0`** | Out of Flow | Requires `padding-top` on body | Ignores parent overflow | Persistent floating banners, overlays |
| **Flex `margin-top: auto`** | In Flow | None | None | Standard pinned footers |
| **Grid `1fr` Middle Row** | In Flow | None | None | Application shells, Holy Grail layouts |

---

## 10. Interview Questions

### Q1: What is the mechanical difference between `position: fixed` and `position: sticky`?
<details>
<summary>Answer</summary>
- **`position: fixed`**: Removes the element completely from standard document flow. It is positioned relative to the browser viewport (or a transform ancestor). It does not take up physical space in the layout, forcing developers to manually add margins or padding to surrounding elements to prevent visual overlap.
- **`position: sticky`**: Retains the element inside standard document flow. It acts as `position: relative` until the user scrolls past its defined threshold (e.g. `top: 0`), at which point it acts as fixed **strictly within the boundary of its parent container**. When the parent container's bottom edge reaches the sticky element, the sticky element stops sticking and scrolls away with its parent.
</details>

### Q2: Why does `position: sticky` fail to work when an ancestor element has `overflow: hidden`?
<details>
<summary>Answer</summary>
According to the CSS Positioned Layout specification, a sticky element sticks relative to its **nearest scrolling ancestor** (the closest ancestor with an overflow property other than `visible`).

When an ancestor element has `overflow: hidden`, `overflow: auto`, or `overflow: scroll`, that ancestor establishes a localized scroll container. The sticky element is now bounded to *that ancestor's clipping box* rather than the main browser viewport. Because the ancestor typically does not scroll, the sticky element never encounters a scrolling boundary and fails to stick.
Replacing `overflow: hidden` with modern `overflow: clip` fixes the issue because `overflow: clip` clips visual pixels without establishing a scroll container.
</details>

### Q3: What is the purpose of `scroll-padding-top` on the `html` element?
<details>
<summary>Answer</summary>
When a user navigates to an in-page fragment identifier (e.g. `<a href="#features">` or via keyboard focus), the browser scrolls the document until the target element aligns with the very top edge of the viewport (`top: 0`).
If the page features a sticky or fixed header of 64px height, the target heading scrolls directly behind the header, completely obscuring it from view.

Setting:
```css
html {
  scroll-padding-top: 5rem;
}
```
instructs the browser's scrolling engine to reserve 5rem of breathing room at the top of the viewport when resolving scroll targets, ensuring anchor destinations and keyboard-focused elements stop cleanly beneath the sticky header.
</details>

### Q4: How does `margin-top: auto` pin a footer to the bottom of the screen in CSS Flexbox?
<details>
<summary>Answer</summary>
In CSS Flexbox:
1. The parent container (`body`) is configured as a vertical column: `min-height: 100dvh; display: flex; flex-direction: column;`.
2. When the page has little content, the body has unused vertical free space between the content and the bottom of the viewport.
3. In Flexbox, an `auto` margin on an item **consumes all available free space** along that axis.
4. Setting `margin-top: auto` on `<footer>` directs the engine to allocate 100% of the unused vertical space directly above the footer, pushing the footer down to the very bottom of the viewport.
</details>

### Q5: Why is `border-collapse: separate` required when implementing sticky headers in HTML tables?
<details>
<summary>Answer</summary>
When `border-collapse: collapse` is applied to a `<table>`, the borders between adjacent cells are merged into a single shared border line owned by the table grid, rather than individual cell boxes.
When a table header (`<th>`) with `position: sticky` scrolls, the browser engine cannot detach the collapsed border from the static table rows below it, causing border rendering glitches, flickering, or the sticky header failing to scroll in Chromium and WebKit.
Setting:
```css
table {
  border-collapse: separate;
  border-spacing: 0;
}
```
keeps cell borders distinct and attached to their individual cell boxes, allowing sticky `<th>` cells to maintain their borders cleanly while scrolling.
</details>

### Q6: How does CSS Scroll-Driven Animations eliminate JavaScript for sticky header shadow reveals?
<details>
<summary>Answer</summary>
Historically, detecting when a user scrolled past the top of the page required adding a scroll event listener in JavaScript (`window.scrollY > 0`) to toggle an `.is-scrolled` class.
With modern **CSS Scroll-Driven Animations**:
```css
.site-header {
  animation: shadow-reveal linear both;
  animation-timeline: scroll();
  animation-range: 0px 50px;
}
@keyframes shadow-reveal {
  to { box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
}
```
The browser engine maps the animation timeline directly to the viewport's vertical scroll position. Over the first 50px of scroll, the drop shadow transitions smoothly on the compositor thread with zero JavaScript execution and zero main-thread overhead.
</details>

### Q7: Why should sticky headers include `backdrop-filter: blur()` alongside translucent backgrounds?
<details>
<summary>Answer</summary>
If a sticky header uses a simple translucent background color (`background: rgba(255, 255, 255, 0.85)`), text from the page scrolling underneath bleeds through the header. Sighted users see two overlapping layers of contrasting text glyphs simultaneously, creating visual noise and making navigation links unreadable.
Adding `backdrop-filter: blur(12px)` diffuses the high-frequency edges of the content passing underneath, turning it into soft, abstract colors while keeping the foreground navigation links crisp and legible.
</details>

### Q8: What happens to a sticky element if its parent container has a height equal to the sticky element's height?
<details>
<summary>Answer</summary>
Because a sticky element cannot travel outside the physical bounds of its parent container, if the parent's height is equal to the sticky element's height ($H_{\text{parent}} == H_{\text{sticky}}$), there is zero available travel distance.
The element will never stick; it will scroll away immediately with its parent container. For an element to stick, the parent container must be taller than the sticky element.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Infallible Pinned Footer Layout
**Objective**: Construct an HTML/CSS layout where the footer is guaranteed to sit at the bottom edge of the screen on short pages, while pushing down naturally on long pages.

<details>
<summary>Solution</summary>

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Pinned Footer</title>
</head>
<body>
  <header class="site-header">
    <strong>ApexEdge</strong>
  </header>
  <main class="site-main">
    <h1>Account Verification Complete</h1>
    <p>Your cluster token has been verified successfully.</p>
  </main>
  <footer class="site-footer">
    <p>&copy; 2026 ApexEdge Cloud. Pinned Footer Architecture.</p>
  </footer>
</body>
</html>
```

```css
*, *::before, *::after {
  box-sizing: border-box;
}

html, body {
  margin: 0;
  padding: 0;
}

body {
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
  font-family: system-ui, sans-serif;
  color: #0f172a;
}

.site-header {
  padding: 1rem 2rem;
  background-color: #0f172a;
  color: #ffffff;
}

.site-main {
  flex: 1 0 auto;
  max-width: 800px;
  width: 100%;
  margin: 0 auto;
  padding: 3rem 1.5rem;
}

.site-footer {
  margin-top: auto; /* Consumes free vertical space, pinning footer to bottom */
  padding: 1.5rem 2rem;
  background-color: #f1f5f9;
  border-top: 1px solid #e2e8f0;
  text-align: center;
  color: #64748b;
}
```
</details>

### Exercise 2: Responsive Sticky Table with Horizontal Scroll
**Objective**: Build a data table where column headers stick vertically and the left column sticks horizontally, with proper borders and background isolation.

<details>
<summary>Solution</summary>

```html
<div class="table-wrap">
  <table class="data-grid">
    <thead>
      <tr>
        <th class="cell-fixed">Node Identifier</th>
        <th>Status</th>
        <th>CPU Load</th>
        <th>Memory Usage</th>
        <th>Network I/O</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td class="cell-fixed">node-us-east-01</td>
        <td>Active</td>
        <td>42%</td>
        <td>18.4 GB</td>
        <td>1.2 Gbps</td>
      </tr>
      <tr>
        <td class="cell-fixed">node-us-east-02</td>
        <td>Active</td>
        <td>68%</td>
        <td>24.1 GB</td>
        <td>2.8 Gbps</td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.table-wrap {
  max-height: 250px;
  max-width: 500px;
  overflow: auto;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
}

.data-grid {
  border-collapse: separate;
  border-spacing: 0;
  width: 100%;
  font-family: system-ui, sans-serif;
  font-size: 0.875rem;
}

.data-grid th, .data-grid td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e2e8f0;
  white-space: nowrap;
}

/* Vertical Sticky Header */
.data-grid thead th {
  position: sticky;
  top: 0;
  background-color: #0f172a;
  color: #ffffff;
  z-index: 10;
}

/* Horizontal Sticky Column */
.data-grid .cell-fixed {
  position: sticky;
  left: 0;
  background-color: #f8fafc;
  border-right: 2px solid #cbd5e1;
  z-index: 5;
  font-weight: 600;
}

/* Top-Left Corner Cell: Must have highest z-index */
.data-grid thead th.cell-fixed {
  background-color: #0f172a;
  z-index: 20;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Sticky Header**: `position: sticky; top: 0; z-index: 100;`
- **Pinned Footer**: `body { min-height: 100dvh; display: flex; flex-direction: column; }` with `footer { margin-top: auto; }`.
- **Anchor Offset**: `html { scroll-padding-top: 5rem; }` prevents fixed/sticky headers from covering anchor targets.
- **Sticky Tables**: Must use `border-collapse: separate; border-spacing: 0;` on `<table>`.
- **Sticky Bounds**: Sticky elements cannot stick outside their immediate parent container.
- **No Overflow Ancestors**: Ancestors with `overflow: hidden` kill sticky positioning; use `overflow: clip`.
- **Mobile Safe Areas**: Use `padding-top: max(1rem, env(safe-area-inset-top));`.

---

## 13. Further Reading
- [W3C CSS Positioned Layout Module Level 3](https://www.w3.org/TR/css-position-3/)
- [MDN Web Docs: position sticky](https://developer.mozilla.org/en-US/docs/Web/CSS/position#sticky)
- [web.dev: Sticky Table Headers](https://web.dev/patterns/layout/sticky-table-header/)
- [CSS-Tricks: A Couple of Use Cases for Calc() in Sticky Layouts](https://css-tricks.com/a-couple-of-use-cases-for-calc/)
- [Ahmad Shadeed: Deep Dive into CSS Sticky Positioning](https://ishadeed.com/article/css-sticky/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [02. Holy Grail and Sidebar Layouts](02-holy-grail-and-sidebar-layouts.md) | [04. Card Grids and Galleries](04-card-grids-and-galleries.md) |
