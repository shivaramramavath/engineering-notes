# Sibling Index and Count Functions

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Values and Units Module Level 5 (W3C Working Draft) |
| **Baseline Status** | Emerging / In Development (First Public Working Draft, CSSWG Resolution 2024–2026) |
| **Browsers Tested** | Chromium Canary (Prototype builds), W3C Reference Test Suite |
| **Tested With** | W3C CSSWG Specification Drafts, Web Platform Tests, CSSWG Issues Tracker |

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

One of the most persistent hurdles in CSS animation and layout engineering has been the inability to access an element's index position or total sibling count programmatically.

Consider creating a staggered entrance animation for a list of items:
- Traditionally, developers had to write repetitive, brittle `:nth-child` chains:
  ```css
  li:nth-child(1) { animation-delay: 50ms; }
  li:nth-child(2) { animation-delay: 100ms; }
  li:nth-child(3) { animation-delay: 150ms; }
  /* ...what if there are 50 items? */
  ```
- Alternatively, frontend teams injected inline custom property styles via JavaScript or template engines: `<li style="--i: 3; --total: 10">`. While functional, this clutters HTML markup, prevents clean CSS caching, and breaks when items are dynamically filtered or re-ordered.

The **`sibling-index()`** and **`sibling-count()`** functional notations in **CSS Values and Units Module Level 5** solve this natively. They expose an element's 1-based index and the parent's total child count directly as mathematical integers in CSS, unlocking procedural animations, circular layouts, and progress bars without inline styles or JavaScript.

---

## 2. Core Concepts

### `sibling-index()`
Returns an integer representing the 1-based position of the element among its siblings (analogous to `:nth-child(n)`):
- The first child returns `1`.
- The fifth child returns `5`.
- Can be used anywhere an `<integer>` or `<number>` is accepted, especially inside `calc()`.

```css
.card {
  /* Stagger animation delay by 75ms per card! */
  animation-delay: calc(sibling-index() * 75ms);
}
```

### `sibling-count()`
Returns an integer representing the **total number of siblings** in the parent container:
- If a list has 12 items, `sibling-count()` returns `12` for all 12 items.
- Enables computing percentage progress or equal distribution angles:

```css
.progress-bar {
  /* Calculate progress fraction (e.g. 3 / 10 = 30%) */
  width: calc(sibling-index() / sibling-count() * 100%);
}
```

### Filtering Siblings with Selector Syntax
The specification allows an optional selector argument to restrict counting to specific types of siblings (matching `:nth-of-type` or `:nth-child(n of <selector>)` logic):
```css
/* Only count items matching .featured */
.featured {
  z-index: sibling-index(of .featured);
}
```

---

## 3. How It Works Internally

```
Tree-Position Layout Lifecycle:

  <ul class="nav-menu">            <── Parent Node: Tracks total child count (4)
    <li>Item 1</li>                  ├── sibling-index() = 1, sibling-count() = 4
    <li>Item 2</li>                  ├── sibling-index() = 2, sibling-count() = 4
    <li>Item 3</li>                  ├── sibling-index() = 3, sibling-count() = 4
    <li>Item 4</li>                  └── sibling-index() = 4, sibling-count() = 4
  </ul>

Dynamic Recalculation:
  - If JavaScript removes 'Item 2':
  - Engine updates total child count: sibling-count() -> 3
  - Item 3 automatically re-evaluates: sibling-index() -> 2
  - Item 4 automatically re-evaluates: sibling-index() -> 3
  - All calc() formulas update synchronously in the CSSOM!
```

```
Manual :nth-child Chains vs. Native sibling-index():
┌──────────────────────────────┬──────────────────────────────┬──────────────────────────────┐
│ Criteria                     │ Manual :nth-child Chains     │ Native `sibling-index()`     │
├──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
│ Code Length                  │ 50 lines for 50 items        │ **1 line of pure CSS**       │
│ Scalability                  │ Breaks if item count $> N$   │ **Infinite scale**           │
│ DOM Dynamic Updates          │ Fails if DOM nodes reordered │ **Self-healing & dynamic**   │
│ Inline HTML Injection        │ Often requires `style="--i"` │ **Zero inline HTML styles**  │
└──────────────────────────────┴──────────────────────────────┴──────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Specification & Browser Status:
├── CSSWG Status: First Public Working Draft (CSS Values & Units Level 5)
├── Chrome / Blink: Under active prototype implementation
├── Safari / WebKit: Strongly supported in CSSWG discussions
└── Firefox / Gecko: Tracking issue open
```

### Syntax Definition
```css
/* 1. Basic index and count */
property: calc(sibling-index() * unit);
property: calc(sibling-index() / sibling-count());

/* 2. Filtered by selector list */
property: sibling-index(of .class-name);
property: sibling-count(of .class-name);
```

---

## 5. Practical Examples

### Example 1: Procedural Staggered Card Entrance
A list of cards where each card automatically waits an additional 60ms before animating in.

```html
<ul class="card-list">
  <li class="card">Card #1</li>
  <li class="card">Card #2</li>
  <li class="card">Card #3</li>
  <li class="card">Card #4</li>
</ul>
```

```css
.card-list {
  list-style: none;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.card {
  padding: 1.25rem;
  background: white;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  opacity: 0;
  transform: translateY(20px);
  animation: cardFadeIn 0.4s ease-out forwards;

  /* Native procedural stagger delay! */
  animation-delay: calc(sibling-index() * 60ms);
}

@keyframes cardFadeIn {
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
```

### Example 2: Circular Radial Menu Layout
Evenly distribute buttons around a full 360-degree circle based on total sibling count.

```html
<nav class="radial-menu">
  <button class="menu-item">A</button>
  <button class="menu-item">B</button>
  <button class="menu-item">C</button>
  <button class="menu-item">D</button>
  <button class="menu-item">E</button>
  <button class="menu-item">F</button>
</nav>
```

```css
.radial-menu {
  position: relative;
  width: 16rem;
  height: 16rem;
  border-radius: 50%;
  margin: 4rem auto;
}

.menu-item {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 3rem;
  height: 3rem;
  margin: -1.5rem;
  border-radius: 50%;
  border: none;
  background-color: #2563eb;
  color: white;
  cursor: pointer;

  /* Calculate angle step: 360deg / total items * item index */
  --angle: calc((360deg / sibling-count()) * sibling-index());

  /* Position along unit circle with trigonometry */
  transform: rotate(var(--angle)) translate(7rem) rotate(calc(var(--angle) * -1));
}
```

### Example 3: Dynamic Multi-Step Progress Indicator
A breadcrumb or wizard bar where progress fills dynamically based on sibling position.

```html
<ol class="step-wizard">
  <li class="step is-complete">Cart</li>
  <li class="step is-complete">Shipping</li>
  <li class="step is-active">Payment</li>
  <li class="step">Review</li>
</ol>
```

```css
.step-wizard {
  display: flex;
  list-style: none;
  padding: 0;
  counter-reset: step-counter;

  .step {
    flex: 1;
    text-align: center;
    position: relative;

    /* Display step numbers automatically */
    &::before {
      content: sibling-index() ' of ' sibling-count();
      display: block;
      font-size: 0.75rem;
      color: #64748b;
    }
  }
}
```

### Example 4: Automatic Stacked Card Layering (`z-index`)
Invert stacking order so the first card sits on top of subsequent cards without writing manual `z-index` rules.

```css
.stacked-deck .deck-card {
  position: absolute;
  top: calc(sibling-index() * 1rem);
  left: calc(sibling-index() * 1rem);

  /* Inverted z-index: first card gets highest layer */
  z-index: calc(sibling-count() - sibling-index());
}
```

### Example 5: Color Hue Shift along Spectrum
Spread items evenly across the OKLCH color wheel based on index position.

```css
.palette-row .color-swatch {
  /* Spread 360 degrees across all siblings */
  --hue: calc((360 / sibling-count()) * sibling-index());
  background-color: oklch(0.65 0.22 var(--hue));
}
```

---

## 6. Best Practices

1. **Always use inside `calc()` for units**: `sibling-index()` outputs a raw unitless `<integer>`. When using it for durations, lengths, or angles, always multiply inside `calc()`: `calc(sibling-index() * 100ms)`.
2. **Handle empty states gracefully**: If a container has no matching elements, `sibling-count()` evaluates to `0`. Avoid naked division by `sibling-count()` without a fallback to prevent division-by-zero errors.
3. **Use the `of <selector>` filter for mixed DOM lists**: If an unordered list contains decorative headers or dividers, filter indexing using `sibling-index(of .interactive-item)`.
4. **Provide progressive enhancement fallback via inline variables**: While `sibling-index()` is in draft status, provide a fallback using custom properties:
   ```css
   animation-delay: calc(var(--i, sibling-index()) * 50ms);
   ```
5. **Always respect `prefers-reduced-motion`**: When using `sibling-index()` for staggered animations, collapse the duration and delay for users who prefer reduced motion:
   ```css
   @media (prefers-reduced-motion: reduce) {
     .card { animation: none; opacity: 1; transform: none; }
   }
   ```
6. **Limit total sibling counts for expensive animations**: Staggering 1,000 items by 50ms means the 1,000th item will not appear for 50 seconds. Cap delays using `min(calc(sibling-index() * 50ms), 1s)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting unit multiplication in `calc()` | Passing a raw number where a length or time is required drops the declaration. | Multiply by unit: `calc(sibling-index() * 1s)`. |
| Zero-based indexing assumption | Believing index starts at `0`. | `sibling-index()` is 1-based (first child is `1`). |
| Division by zero in dynamic lists | If `sibling-count()` is zero, `calc(100% / sibling-count())` fails. | Use `max(sibling-count(), 1)` or guard with `:not(:empty)`. |
| Unbounded animation delays on long lists | The 100th item takes 5 seconds to appear, frustrating users. | Clamp delay: `min(calc(sibling-index() * 50ms), 600ms)`. |
| Relying on native browser support in production today | Fails in current stable engines (currently in draft status). | Use `style="--i: 1"` or PostCSS polyfills as fallback. |
| Expecting `sibling-index()` to count across Shadow DOM boundaries | Only counts siblings within the same local tree. | Structure styling within the appropriate shadow tree. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero Inline Style Serialization**: Replaces thousands of inline `style="--i: ..."` string attributes in HTML responses, dramatically reducing HTML payload size over the network.
- **Fast C++ Child Index Queries**: Browser engines already track child node index positions in memory to support `:nth-child()`; exposing this to CSS values introduces zero architectural overhead.

### Accessibility (A11y)
- **Vestibular Safety**: Staggered animations can induce vestibular disorientation if too many items cascade across the screen simultaneously. Always provide immediate static appearances under `prefers-reduced-motion: reduce`.

### Browser Compatibility
- **Status**: Official W3C Working Draft in CSS Values and Units Module Level 5.
- **Experimental Implementation**: Prototype development underway in Chromium.
- **Feature Detection**:
```css
@supports (animation-delay: calc(sibling-index() * 1ms)) {
  /* Supported */
}
```

---

## 9. Comparison / When to Use What

| Technique | Scale Limit | HTML Clutter | Dynamic DOM Reaction | Production Ready? |
| :--- | :--- | :--- | :--- | :--- |
| **`sibling-index()`** | **Infinite** | **Zero (Pure CSS)** | **Immediate & Automatic** | Working Draft |
| **Inline CSS Variable (`--i`)**| Infinite | High (injected on every tag)| Requires JS DOM update | **Yes (Today)** |
| **`:nth-child` Delay Chains** | Finite (10–20 max)| Zero | Brittle | **Yes (Today)** |
| **JavaScript Animation Loops** | Infinite | Zero | High runtime CPU cost | **Yes (Today)** |

---

## 10. Interview Questions

### Q1: What problem do `sibling-index()` and `sibling-count()` solve in modern CSS?
<details>
<summary>View Answer</summary>
They eliminate the need for manual, repetitive `:nth-child` CSS delay chains and the need for JavaScript or template engines to inject inline style custom properties (`style="--i: 3; --total: 10"`). They allow CSS property values, animation delays, layout angles, and progress widths to be calculated directly from an element's structural position in the DOM tree.
</details>

### Q2: Is `sibling-index()` 0-based or 1-based?
<details>
<summary>View Answer</summary>
It is **1-based**. The first child element returns `1`, the second returns `2`, consistent with existing CSS conventions like `:nth-child(1)`.
</details>

### Q3: How can `sibling-count()` be used to create circular navigation menus?
<details>
<summary>View Answer</summary>
By dividing $360^\circ$ by `sibling-count()`, you obtain the exact angle step between adjacent items. Multiplying that step by `sibling-index()` yields each button's unique radial angle:
`--angle: calc((360deg / sibling-count()) * sibling-index());`
Combined with `transform: rotate(var(--angle)) translate(radius)`, items automatically position themselves in a perfect circle regardless of how many buttons exist.
</details>

### Q4: What does the `of <selector>` argument do inside `sibling-index(of .item)`?
<details>
<summary>View Answer</summary>
It filters the counting scope so that only siblings matching the specified selector are included in the index calculation. Non-matching siblings (such as divider lines or section headers) are ignored, mirroring the behavior of the `:nth-child(n of <selector>)` syntax.
</details>

### Q5: How do you prevent division-by-zero errors when calculating percentage fractions with `sibling-count()`?
<details>
<summary>View Answer</summary>
If a container has no matching elements, `sibling-count()` can evaluate to `0`. Dividing by zero in CSS calculations results in an invalid value. Developers can prevent this by using `max()` inside the calculation: `calc(sibling-index() / max(sibling-count(), 1) * 100%)`.
</details>

### Q6: What is the current production-safe fallback pattern for `sibling-index()` today?
<details>
<summary>View Answer</summary>
The standard fallback pattern is to use a CSS Custom Property with `sibling-index()` as the second fallback argument:
```css
.card {
  animation-delay: calc(var(--i, sibling-index()) * 50ms);
}
```
In today's browsers, the inline `--i` variable takes precedence. When browsers natively implement `sibling-index()`, the HTML inline attributes can be removed without breaking the stylesheet.
</details>

### Q7: Can `sibling-index()` be used inside `content: ""` for pseudo-elements?
<details>
<summary>View Answer</summary>
Yes. Because `sibling-index()` returns an `<integer>`, it can be concatenated into generated content strings or paired with CSS counters:
```css
.list-item::before {
  content: sibling-index() '. ';
}
```
</details>

### Q8: Does `sibling-index()` update automatically if DOM nodes are re-ordered via JavaScript?
<details>
<summary>View Answer</summary>
Yes. Because it is computed dynamically by the browser's style engine during style resolution, any DOM manipulation (like dragging to re-order, deleting items, or appending nodes) will cause the affected nodes to immediately re-evaluate their index and re-calculate all dependent CSS expressions synchronously.
</details>

---

## 11. Practice Exercises

### Exercise 1: Procedural Stagger Delay
**Objective**: Apply a staggered 40ms animation delay to list items using `sibling-index()`.
<details>
<summary>View Solution</summary>

```css
.stagger-item {
  opacity: 0;
  animation: fadeIn 0.3s ease forwards;
  animation-delay: calc(sibling-index() * 40ms);
}
```
</details>

### Exercise 2: Radial Equal-Angle Distribution
**Objective**: Distribute icon buttons evenly around a full circle using `sibling-index()` and `sibling-count()`.
<details>
<summary>View Solution</summary>

```css
.circle-btn {
  position: absolute;
  top: 50%;
  left: 50%;
  --item-angle: calc((360deg / sibling-count()) * sibling-index());
  transform: rotate(var(--item-angle)) translate(6rem) rotate(calc(var(--item-angle) * -1));
}
```
</details>

### Exercise 3: Dynamic Multi-Column Color Gradients
**Objective**: Shift background hue incrementally across all cards in a grid based on total siblings.
<details>
<summary>View Solution</summary>

```css
.spectrum-card {
  --hue: calc((360 / sibling-count()) * sibling-index());
  background-color: oklch(0.7 0.2 var(--hue));
  padding: 1.5rem;
  border-radius: 0.5rem;
}
```
</details>

### Exercise 4: Capped Maximum Stagger Delay
**Objective**: Stagger entrance delays by 50ms, but cap the maximum delay at 800ms so large lists don't take forever to display.
<details>
<summary>View Solution</summary>

```css
.feed-post {
  animation: slideUp 0.4s ease forwards;
  animation-delay: min(calc(sibling-index() * 50ms), 800ms);
}
```
</details>

### Exercise 5: Filtered Indexing on Tag Badges
**Objective**: Calculate the index of badges matching `.active-badge` only, ignoring inactive badges.
<details>
<summary>View Solution</summary>

```css
.active-badge {
  order: sibling-index(of .active-badge);
  font-weight: 700;
}
```
</details>

### Exercise 6: Progressive Fallback Wrapper
**Objective**: Write a resilient animation rule that falls back to `var(--index)` in legacy browsers and enhances with `sibling-index()`.
<details>
<summary>View Solution</summary>

```css
.resilient-item {
  animation: popIn 0.35s cubic-bezier(0.16, 1, 0.3, 1) forwards;
  animation-delay: calc(var(--index, sibling-index()) * 60ms);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Staggered Delay */
.item {
  animation-delay: calc(sibling-index() * 50ms);
}

/* 2. Percentage Share */
.bar {
  width: calc(sibling-index() / sibling-count() * 100%);
}

/* 3. Radial Distribution */
.dial-node {
  --rot: calc((360deg / sibling-count()) * sibling-index());
  transform: rotate(var(--rot)) translate(8rem);
}

/* 4. Filtered Selection */
.selected {
  z-index: sibling-index(of .selected);
}
```

- `sibling-index()` = 1-based index position of element among siblings.
- `sibling-count()` = Total count of siblings in parent container.
- Operates dynamically in C++ without JavaScript or inline HTML styles.
- Emerging W3C specification under active implementation.

---

## 13. Further Reading

- [W3C CSS Values and Units Module Level 5 (sibling-index)](https://drafts.csswg.org/css-values-5/#tree-counting)
- [CSSWG GitHub Issue: Sibling index and count functional notations](https://github.com/w3c/csswg-drafts/issues/4559)
- [Bramus: Proposed CSS sibling-index() and sibling-count()](https://www.bram.us/2023/11/06/css-sibling-index-and-sibling-count/)

---

### Navigation
- **Previous**: [16-supports-and-progressive-enhancement.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/16-supports-and-progressive-enhancement.md)
- **Next**: [../10-architecture-and-methodologies/README.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/README.md)
