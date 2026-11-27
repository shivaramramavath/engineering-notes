# Alignment in CSS Grid

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | `04-grid/01-grid-mental-model.md`, `02-box-model-and-layout-basics/13-box-alignment-basics.md` |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2017) |
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

In Flexbox, alignment is constrained along a primary main axis and a secondary cross axis. If you switch `flex-direction` to `column`, `justify-content` suddenly shifts to vertical alignment, which often confuses authors. Furthermore, Flexbox has no support for `justify-items` or `justify-self`.

**CSS Grid Layout implements the complete, uncompromised CSS Box Alignment Level 3 specification**:
- Full two-axis control across both the **Inline (Horizontal)** and **Block (Vertical)** axes.
- **`justify-*`** properties *always* operate horizontally along the inline axis.
- **`align-*`** properties *always* operate vertically along the block axis.
- Both item alignment within individual cells (`*-items`, `*-self`) and collective track distribution within the container (`*-content`) are fully supported.
- Absolute 2D centering in a single declaration: `place-items: center;`.

---

## 2 Core Concepts

### The Two Alignment Scopes in Grid

In CSS Grid, alignment operates at two distinct levels:

```
Scope 1: Track Distribution (*-content)
When the total size of all grid tracks is LESS than the container dimensions.
Controls where the whole grid sits inside the container.
- justify-content (horizontal)
- align-content (vertical)

Scope 2: Cell-Item Alignment (*-items, *-self)
When an individual item is SMALLER than its allocated cell or area.
Controls where the item sits inside its cell.
- justify-items / justify-self (horizontal)
- align-items / align-self (vertical)
```

```
+-------------------------------------------------------------------------+
| Grid Container (e.g. width: 800px; height: 500px)                       |
|                                                                         |
| justify-content: center; align-content: center;                         |
| (Centers the entire 400x300 track cluster inside the 800x500 container) |
|                                                                         |
|             +---------------------------------------------+             |
|             | Grid Track Cell (width: 200px; height: 150px) |             |
|             |                                             |             |
|             | justify-items: center; align-items: center; |             |
|             |      +-------------------------------+      |             |
|             |      | Item (width: 100px; h: 50px)  |      |             |
|             |      | (Centered inside its cell)    |      |             |
|             |      +-------------------------------+      |             |
|             |                                             |             |
|             +---------------------------------------------+             |
|                                                                         |
+-------------------------------------------------------------+
```

### The Shorthands: `place-*`

Modern CSS provides compact shorthands where the first argument sets the block (vertical) axis and the second sets the inline (horizontal) axis:
- `place-items: <align-items> <justify-items>;` (If 1 value, applies to both!).
- `place-content: <align-content> <justify-content>;`
- `place-self: <align-self> <justify-self>;`

---

## 3 How It Works Internally

### Alignment Matrix in CSS Grid

| Property | Axis | Targets | Default | Supported in Flexbox? |
| :--- | :--- | :--- | :--- | :--- |
| **`justify-items`** | Inline (Horizontal) | All items inside their cells | `stretch` | **No** |
| **`justify-self`** | Inline (Horizontal) | Single item inside its cell | `auto` | **No** |
| **`align-items`** | Block (Vertical) | All items inside their cells | `stretch` | Yes |
| **`align-self`** | Block (Vertical) | Single item inside its cell | `auto` | Yes |
| **`justify-content`** | Inline (Horizontal) | Grid tracks in container | `start` | Yes (Main axis) |
| **`align-content`** | Block (Vertical) | Grid tracks in container | `start` | Yes (Multi-line) |

### Default Stretch Behavior

By default, `justify-items` and `align-items` are both set to **`stretch`**:
- A grid item with no explicit `width` or `height` expands to fill $100\%$ of its allocated grid cell in both dimensions.
- Once you apply any alignment value other than `stretch` (e.g. `center`, `start`), the item contracts to its intrinsic content dimensions.

---

## 4 Syntax & Usage

```css
.grid-container {
  display: grid;
  grid-template-columns: repeat(3, 200px);
  grid-template-rows: repeat(2, 150px);
  min-height: 100vh;

  /* 1-Line Universal Centering (Both items and tracks!) */
  place-items: center;   /* align-items: center; justify-items: center; */
  place-content: center; /* align-content: center; justify-content: center; */

  /* Gutters */
  gap: 20px;
}

/* Individual Item Overrides */
.special-card {
  justify-self: end;    /* Hugs right cell edge */
  align-self: start;    /* Hugs top cell edge */
  place-self: start end; /* align-self justify-self */
}
```

---

## 5 Practical Examples

### Example 1: Perfect Absolute Centering in 2 Lines of CSS

```css
.modal-overlay {
  display: grid;
  place-items: center; /* Centers horizontally and vertically */
  min-height: 100vh;
  background-color: rgba(15, 23, 42, 0.7);
  padding: 16px;
}

.modal-dialog {
  width: min(100%, 480px);
  background: #ffffff;
  border-radius: 8px;
  padding: 24px;
}
```

### Example 2: Centered Fixed-Dimension Grid with `place-content`

```css
.catalog-canvas {
  display: grid;
  /* Fixed tracks: 3 columns of 240px = 720px width */
  grid-template-columns: repeat(3, 240px);
  grid-template-rows: repeat(2, 200px);
  gap: 16px;
  min-height: 100vh;
  /* Centers the entire 720px wide grid cluster in the middle of a 1920px screen */
  place-content: center;
  background-color: #f8fafc;
}

.catalog-tile {
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 6px;
}
```

### Example 3: Edge Case — Safe Alignment Preventing Top/Left Clipping

```css
.scrollable-grid-shell {
  display: grid;
  /* If content exceeds viewport, 'safe center' falls back to 'start'
     so user can scroll to view top-left content */
  justify-content: safe center;
  align-content: safe center;
  min-height: 100vh;
  overflow: auto;
  padding: 24px;
}
```

### Example 4: Failure Case — Using `justify-content` When `justify-items` Was Needed

```css
/* FAILURE PATTERN */
.card-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  /* BUG: Developer wants the text inside each card to center horizontally.
     They wrote justify-content: center.
     Because 1fr tracks already occupy 100% of container width,
     justify-content does NOTHING! */
  justify-content: center;
}

/* FIXED PATTERN */
.card-grid-fixed {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  /* Correct: Aligns the items INSIDE their individual 1fr cells! */
  justify-items: center;
}
```

### Example 5: Pinning Corner Badges with `place-self`

```css
.photo-card {
  display: grid;
  grid-template-columns: 1fr;
  grid-template-rows: 1fr;
  width: 280px;
  height: 200px;
}

.photo-img {
  grid-column: 1;
  grid-row: 1;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.status-badge {
  grid-column: 1;
  grid-row: 1;
  z-index: 2;
  /* Pinned to top-right corner of cell */
  place-self: start end;
  margin: 12px;
  background-color: #ef4444;
  color: #ffffff;
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 0.75rem;
  font-weight: 700;
}
```

---

## 6 Best Practices

1. **Use `place-items: center` for simple centering**: It is the modern standard, replacing legacy flexbox centering.
2. **Understand the difference between `-items` and `-content`**: Use `-items` to move elements inside cells; use `-content` to move the grid tracks within the container.
3. **Use `place-self` for badge positioning**: Placing an image and badge in the same cell and setting `place-self: start end` eliminates absolute positioning.
4. **Use `safe center` for scrollable modal dialogs**: Prevents the top or left half of content from clipping off-screen when viewports shrink.
5. **Remember that `justify-*` is always horizontal in Grid**: Unlike Flexbox, grid axes never swap when writing modes are horizontal.
6. **Beware of losing 100% width on non-stretched items**: Setting `justify-items: center` shrinks child elements to their content width.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Fails | Solution |
| :--- | :--- | :--- |
| `justify-content` has no effect on `1fr` tracks | `1fr` tracks consume all free space; zero free space left to distribute | Use `justify-items` to align items inside tracks |
| Cards shrink to text width unexpectedly | Setting `justify-items: center` overrides default `stretch` | Keep default `stretch` or set explicit child widths |
| `place-items: start center` parameter confusion | Order is `align-items` (vertical), then `justify-items` (horizontal) | Remember: Vertical first, Horizontal second |
| Centered modal cut off at top on mobile | Unsafe centering pushes top half outside scrollport | Use `place-content: safe center` |
| Trying to use `justify-items` in Flexbox | Flexbox specification does not support `justify-items` | Use Flexbox `justify-content` or switch to CSS Grid |
| Grid item ignores height | `align-items: stretch` is active; item stretches to match cell | Set `align-items: start` or `align-self: start` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Engine Cost

- Alignment properties calculate purely algebraically during the cell placement pass.
- Using `place-items: center` carries zero additional reflow cost compared to manual margin calculations.

### Accessibility

- Changing alignment values (`justify-items: end`, `place-self: center`) alters only the geometric position of elements within their cell boundaries.
- It does **not** alter DOM order, meaning screen readers and keyboard tab sequences remain completely linear and accessible.

---

## 9 Comparison / When to Use What

| Goal | CSS Grid Alignment | Flexbox Equivalent |
| :--- | :--- | :--- |
| **Perfect 2D Centering** | `place-items: center;` | `justify-content: center; align-items: center;` |
| **Align item to right of cell** | `justify-self: end;` | `margin-left: auto;` (No `justify-self` in Flex!) |
| **Center fixed-width tracks** | `justify-content: center;` | `justify-content: center;` |
| **Pin corner badge** | `place-self: start end;` | `position: absolute; top: 0; right: 0;` |
| **Baseline text alignment** | `align-items: baseline;` | `align-items: baseline;` |

---

## 10 Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>justify-items</code> and <code>justify-content</code> in CSS Grid?</strong></summary>

`justify-items` aligns individual items along the horizontal inline axis *inside their respective grid cells* (relevant when items are smaller than their cells). In contrast, `justify-content` aligns the *entire grid track cluster* along the inline axis within the container (relevant when the total width of all explicit tracks is smaller than the container's width).
</details>

<details>
<summary><strong>2. Why does <code>justify-content: center</code> have zero visible effect when tracks are defined with <code>1fr</code> units?</strong></summary>

Because `fr` tracks expand to consume all available positive free space inside the container. Since `1fr` tracks absorb 100% of the spare width, there is $0\text{px}$ of free space left over for `justify-content` to distribute.
</details>

<details>
<summary><strong>3. What does <code>place-items: center</code> expand to?</strong></summary>

It is shorthand for:
```css
align-items: center;   /* Vertical block axis */
justify-items: center; /* Horizontal inline axis */
```
</details>

<details>
<summary><strong>4. In what order are values parsed in the two-value <code>place-self</code> property?</strong></summary>

The first value targets the **Block Axis** (`align-self`), and the second value targets the **Inline Axis** (`justify-self`):
`place-self: <align-self> <justify-self>;` (e.g. `place-self: start end;` = top right in LTR).
</details>

<details>
<summary><strong>5. Why does setting <code>justify-items: center</code> cause cards to lose their full-width stretch?</strong></summary>

The default value of `justify-items` is `stretch`, which forces grid items without explicit widths to expand to 100% of the cell width. Setting `justify-items: center` cancels the stretch behavior, causing items to shrink-wrap to their intrinsic content width.
</details>

<details>
<summary><strong>6. What does the <code>safe</code> keyword do when paired with alignment properties?</strong></summary>

It prevents data loss from overflow. If an item or grid is larger than its container, normal `center` pushes content equally outside both edges, making the top/left unscrollable. `safe center` detects overflow and automatically falls back to `start` alignment so all content remains scrollable.
</details>

<details>
<summary><strong>7. Does CSS Grid support <code>justify-self</code> on individual children?</strong></summary>

Yes. Unlike Flexbox (which does not support `justify-self`), CSS Grid fully supports `justify-self` on individual grid items to override the container's `justify-items` along the inline axis.
</details>

<details>
<summary><strong>8. How does <code>align-items: baseline</code> work in CSS Grid?</strong></summary>

It aligns all items along the block axis so that the first text baseline of content in each cell in that row shares the exact same horizontal coordinate line.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Perfect 2D Centering
- **Task**: Center a box horizontally and vertically inside a full-height container using pure CSS Grid shorthands.

<details>
<summary>View Solution</summary>

```css
.center-viewport {
  display: grid;
  place-items: center;
  min-height: 100vh;
}

.box {
  width: 300px;
  padding: 24px;
  background: #ffffff;
  border-radius: 8px;
}
```
</details>

### Exercise 2 (Easy): Pin Badge to Bottom-Right of Cell
- **Task**: Place an element into a grid cell and position it in the bottom-right corner using `place-self`.

<details>
<summary>View Solution</summary>

```css
.badge {
  place-self: end end; /* align-self: end; justify-self: end; */
}
```
</details>

### Exercise 3 (Medium): Centering Fixed Tracks Inside Wide Viewport
- **Task**: Create a 3-column grid of 200px tracks that sits centered in the middle of a 1200px container.

<details>
<summary>View Solution</summary>

```css
.container {
  display: grid;
  grid-template-columns: repeat(3, 200px);
  justify-content: center; /* Centers the 600px total grid inside container */
  gap: 16px;
  width: 1200px;
}
```
</details>

### Exercise 4 (Medium): Overlapping Card with Self-Aligned Floating Button
- **Task**: In a 1-cell grid, place an image and a floating action button pinned to the bottom-left corner.

<details>
<summary>View Solution</summary>

```css
.card-wrapper {
  display: grid;
  grid-template-columns: 1fr;
  grid-template-rows: 240px;
}

.card-img {
  grid-area: 1 / 1;
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.card-fab-btn {
  grid-area: 1 / 1;
  z-index: 2;
  place-self: end start; /* Bottom-left */
  margin: 16px;
}
```
</details>

### Exercise 5 (Hard): Asymmetric Item Alignment Matrix
- **Task**: In a 2x2 grid, align Item 1 to top-left, Item 2 to top-right, Item 3 to bottom-left, Item 4 to bottom-right.

<details>
<summary>View Solution</summary>

```css
.matrix-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  grid-template-rows: 200px 200px;
  gap: 16px;
}

.item-1 { place-self: start start; }
.item-2 { place-self: start end; }
.item-3 { place-self: end start; }
.item-4 { place-self: end end; }
```
</details>

### Exercise 6 (Hard): Safe Scrollable Modal Shell
- **Task**: Author a modal backdrop that centers a dialog box using `place-content: safe center` to avoid mobile overflow clipping.

<details>
<summary>View Solution</summary>

```css
.modal-backdrop {
  display: grid;
  place-content: safe center;
  min-height: 100vh;
  overflow-y: auto;
  padding: 24px;
  background-color: rgba(0, 0, 0, 0.6);
}

.modal-content {
  width: min(90vw, 500px);
  background: #ffffff;
  border-radius: 8px;
  padding: 24px;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Cell Item Alignment (Inside Cell) */
justify-items: stretch | start | end | center; /* Horizontal */
align-items:   stretch | start | end | center; /* Vertical */
place-items:   center;                         /* Both axes */

/* Item Self-Overrides */
justify-self: start | end | center;
align-self:   start | end | center;
place-self:   start end; /* align-self justify-self */

/* Track Cluster Distribution (Inside Container) */
justify-content: start | end | center | space-between | space-around;
align-content:   start | end | center | space-between | space-around;
place-content:   center;
```

---

## 13 Further Reading

- [MDN Web Docs: Box alignment in CSS Grid Layout](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Box_alignment_in_grid_layout)
- [W3C CSS Box Alignment Module Level 3: Grid Alignment](https://www.w3.org/TR/css-align-3/)
- [web.dev: Learn CSS - Alignment in Grid](https://web.dev/learn/css/grid/)
- [CSS-Tricks: A Complete Guide to CSS Grid Alignment](https://css-tricks.com/snippets/css/complete-guide-grid/#aa-alignment)

---

[Previous: 06-minmax-auto-fit-and-auto-fill.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/06-minmax-auto-fit-and-auto-fill.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Next: 08-subgrid.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/08-subgrid.md)
