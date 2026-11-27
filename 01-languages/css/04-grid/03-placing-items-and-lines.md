# Placing Items and Grid Lines

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `04-grid/01-grid-mental-model.md`, `04-grid/02-defining-tracks-and-fr-units.md` |
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

Once you have defined a coordinate grid of tracks on a parent container, the next step is placing items onto that skeleton.

In traditional CSS, placing an element required pushing it with floats, margins, or declaring `position: absolute` with coordinate offsets (`top: 100px; left: 250px;`). Absolute positioning removes the element from normal document flow, meaning containers cannot compute their height dynamically, causing brittle layout collapses.

**CSS Grid Line-Based Placement** allows you to attach any child item directly to specific horizontal and vertical grid lines:
- Using **positive integers** (`1`, `2`, `3`) counting from the start edge.
- Using **negative integers** (`-1`, `-2`) counting backwards from the end edge.
- Spanning across multiple tracks (`span 2`).
- Referencing **custom named lines** (such as `[content-start]`).

Line-based placement gives you absolute-like precision while retaining the responsive resilience of normal document flow.

---

## 2 Core Concepts

### Grid Line Coordinate System

Grid lines are the lines separating tracks. In an explicit grid with 3 columns, there are 4 vertical lines:

```
Positive Lines:   1               2               3               4
                  │               │               │               │
                  ▼               ▼               ▼               ▼
                  +---------------+---------------+---------------+
                  | Column Track 1| Column Track 2| Column Track 3|
                  +---------------+---------------+---------------+
                  ▲               ▲               ▲               ▲
                  │               │               │               │
Negative Lines:  -4              -3              -2              -1
```

- **Positive Indices**: Count from the inline-start edge (`1` is the leftmost line in LTR).
- **Negative Indices**: Count backwards from the inline-end edge (`-1` is the rightmost line in LTR). This is invaluable for spanning an item to the very edge without knowing the total track count!

### The Placement Properties

1. **`grid-column-start` / `grid-column-end`**: The starting and ending vertical lines between which the item is slotted.
2. **`grid-row-start` / `grid-row-end`**: The starting and ending horizontal lines.
3. **The Shorthands**:
   - `grid-column: <start> / <end>;`
   - `grid-row: <start> / <end>;`
   - `grid-area: <row-start> / <col-start> / <row-end> / <col-end>;`

### Spanning Tracks (`span N`)

Instead of specifying an explicit ending line, you can declare a span factor:
```css
/* Start at column line 2 and span across 3 column tracks */
.card {
  grid-column: 2 / span 3;
}
```

---

## 3 How It Works Internally

```
+-------------------------------------------------------------+
| Line 1            Line 2            Line 3            Line 4|
|                                                             |
| Line 1 ──► +---------------------------------------------+  |
|            | Header: grid-column: 1 / -1;                |  |
| Line 2 ──► +---------------------+-----------------------+  |
|            | Sidebar:            | Main:                 |  |
|            | grid-column: 1 / 2; | grid-column: 2 / 4;   |  |
|            | (spans 1 col)       | (spans 2 cols)        |  |
| Line 3 ──► +---------------------+-----------------------+  |
|            | Footer: grid-column: 1 / -1;                |  |
| Line 4 ──► +---------------------------------------------+  |
+-------------------------------------------------------------+
```

### Named Grid Lines

Tracks can be named directly inside the `grid-template-columns` definition using square brackets `[...]`:

```css
.container {
  grid-template-columns: [sidebar-start] 260px [sidebar-end content-start] 1fr [content-end];
}

.main-content {
  /* Place by semantic line name instead of arbitrary integers! */
  grid-column: content-start / content-end;
}
```

When you name lines with the suffixes `-start` and `-end` (e.g. `[main-start]` and `[main-end]`), the browser automatically creates an implicit named grid area called `main`!

---

## 4 Syntax & Usage

```css
.item {
  /* Explicit Lines */
  grid-column-start: 1;
  grid-column-end: 3;

  /* Shorthand: <start> / <end> */
  grid-column: 1 / 3;
  grid-row: 2 / 4;

  /* Negative Index Spanning (Full Width) */
  grid-column: 1 / -1;

  /* Spanning Keyword */
  grid-column: 2 / span 2; /* Starts at 2, spans 2 tracks wide */
  grid-row: span 3;        /* Automatically placed, spans 3 rows tall */

  /* The 4-Value Shorthand */
  grid-area: 1 / 2 / 3 / 4; /* row-start / col-start / row-end / col-end */
}
```

---

## 5 Practical Examples

### Example 1: Full-Bleed Header and Footer Layout

```css
.layout-grid {
  display: grid;
  grid-template-columns: 240px 1fr;
  grid-template-rows: auto 1fr auto;
  min-height: 100vh;
}

.header {
  /* Spans across all columns from start to end */
  grid-column: 1 / -1;
  grid-row: 1;
  background-color: #0f172a;
  color: #ffffff;
  padding: 16px;
}

.sidebar {
  grid-column: 1;
  grid-row: 2;
  background-color: #f8fafc;
  padding: 16px;
}

.content {
  grid-column: 2;
  grid-row: 2;
  padding: 24px;
}

.footer {
  grid-column: 1 / -1;
  grid-row: 3;
  background-color: #1e293b;
  color: #ffffff;
  padding: 16px;
}
```

### Example 2: Magazine Feature Card with Multi-Row Span

```css
.magazine-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  grid-auto-rows: 180px;
  gap: 16px;
}

/* Featured lead story occupies 2 columns and 2 rows */
.lead-story {
  grid-column: span 2;
  grid-row: span 2;
  background-color: #0284c7;
  color: #ffffff;
  padding: 24px;
  border-radius: 8px;
}

.regular-story {
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  padding: 16px;
  border-radius: 8px;
}
```

### Example 3: Edge Case — Named Lines with Implicit Area Generation

```css
.editorial-shell {
  display: grid;
  grid-template-columns: 
    [site-start] 1fr 
    [content-start] min(100%, 720px) [content-end] 
    1fr [site-end];
  gap: 20px;
}

/* Sits within readable centered content track */
.article-prose {
  grid-column: content; /* Automatically maps to content-start / content-end! */
}

/* Breaks out to full screen width */
.full-bleed-figure {
  grid-column: site;    /* Automatically maps to site-start / site-end! */
}
```

### Example 4: Failure Case — Inverted Start and End Line Numbers

```css
/* FAILURE PATTERN */
.broken-item {
  /* BUG: Start line is 4 and end line is 2.
     The browser reverses them automatically, but the mental model is broken! */
  grid-column: 4 / 2;
}

/* FIXED PATTERN */
.clean-item {
  grid-column: 2 / 4; /* Clear, left-to-right progression */
}
```

### Example 5: Overlapping Hero Banner with Z-Index

```css
.hero-grid {
  display: grid;
  grid-template-columns: repeat(12, 1fr);
  grid-template-rows: 100px 300px 100px;
  align-items: center;
}

.hero-image {
  grid-column: 1 / 10;
  grid-row: 1 / 4;
  width: 100%;
  height: 100%;
  object-fit: cover;
  border-radius: 8px;
}

/* Floating card overlaps right side of image */
.hero-floating-card {
  grid-column: 7 / 13;
  grid-row: 2;
  z-index: 2;
  background: #ffffff;
  padding: 32px;
  border-radius: 8px;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.15);
}
```

---

## 6 Best Practices

1. **Use `grid-column: 1 / -1` for full-width elements**: Never hardcode the total column count (like `1 / 13`); negative indexing works regardless of column adjustments.
2. **Use the `grid-column: <start> / <end>` shorthand**: Avoid verbose `grid-column-start` and `grid-column-end` longhands.
3. **Use `span N` for components that need relative sizing**: `grid-column: span 2` allows items to auto-place smoothly without hardcoded column numbers.
4. **Use named lines for large design systems**: Declaring `[content-start] 1fr [content-end]` is far more readable than counting line numbers.
5. **Remember that grid lines start at 1, not 0**: CSS Grid is 1-indexed, unlike JavaScript arrays.
6. **Ensure DOM order remains logical**: Do not use line placement to jump around the document in a way that breaks keyboard navigation.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Fails | Solution |
| :--- | :--- | :--- |
| Starting grid line at `0` | CSS Grid lines are 1-indexed; `0` is invalid syntax | Start at line `1` (or `-1` from end) |
| Assuming `grid-column: 3` means 3 columns wide | Single number sets start line only (spans 1 column) | Use `grid-column: span 3` or `1 / 4` |
| Negative line numbers on implicit grid | Negative numbers only work on the **explicit** grid | Define explicit tracks via `grid-template-*` |
| Comma-separated shorthand (`grid-column: 1, 3`) | CSS Grid uses slash `/` syntax | Use `grid-column: 1 / 3` |
| `grid-area` parameter order confusion | `grid-area` follows `row-start / col-start / row-end / col-end` | Memorize: Top / Left / Bottom / Right (Clockwise-ish) |
| Items colliding unintentionally | Unplaced items flow around explicitly placed items | Audit placement or use `grid-auto-flow: dense` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Engine Resolution

- Line-based placement maps directly to native integer matrix coordinates in the browser's C++ layout engine.
- Resolving explicitly placed items takes constant time $\mathcal{O}(1)$ per item.
- Explicit placements are placed *first*, before the auto-placement algorithm runs to fill empty cells.

### Accessibility (WCAG 1.3.2)

- **The Visual Reordering Trap**: Placing an item into `grid-column: 1; grid-row: 1;` when it is the 15th element in your HTML document moves it visually to the top left, but **keyboard tab order remains at step 15**.
- Sighted keyboard users will tab through 14 other elements before the focus ring jumps to the top left!
- Always ensure that DOM source order matches visual reading sequence.

---

## 9 Comparison / When to Use What

| Placement Technique | Syntax | Best Use Case |
| :--- | :--- | :--- |
| **Numeric Lines** | `grid-column: 1 / 3;` | Standard layout regions, simple grids |
| **Negative Lines** | `grid-column: 1 / -1;` | Full-bleed banners, headers, footers |
| **Relative Spans** | `grid-column: span 2;` | Auto-placed cards, fluid gallery items |
| **Named Lines** | `grid-column: content-start / content-end;` | Large systems, editorial layouts |
| **4-Value Area** | `grid-area: 1 / 1 / 3 / 3;` | Overlapping collage patterns |

---

## 10 Interview Questions

<details>
<summary><strong>1. What does the slash (<code>/</code>) syntax mean in <code>grid-column: 1 / 3</code>?</strong></summary>

The slash separates the **start line** from the **end line**. `grid-column: 1 / 3` is shorthand for `grid-column-start: 1; grid-column-end: 3;`, meaning the item occupies the tracks between vertical lines 1 and 3 (spanning 2 columns).
</details>

<details>
<summary><strong>2. How do negative grid line numbers work in CSS Grid?</strong></summary>

Negative integers count backwards from the end of the explicit grid. Line `-1` represents the trailing edge of the explicit grid, line `-2` is the penultimate line, and so forth. `grid-column: 1 / -1` spans the full width of all explicit columns regardless of how many columns are declared.
</details>

<details>
<summary><strong>3. What is the difference between <code>grid-column: 3</code> and <code>grid-column: span 3</code>?</strong></summary>

`grid-column: 3` places the item starting at vertical line `3` and spans 1 default track (ending at line 4). `grid-column: span 3` tells the item to span across 3 columns starting from wherever the auto-placement algorithm slots it.
</details>

<details>
<summary><strong>4. In what order are coordinates specified in the <code>grid-area</code> shorthand?</strong></summary>

The order is:
`grid-area: <row-start> / <column-start> / <row-end> / <column-end>;`
An easy mnemonic is to think of starting top-left, then ending bottom-right.
</details>

<details>
<summary><strong>5. Can negative line numbers be used on tracks created by the implicit grid?</strong></summary>

No. Negative line numbers only reference the boundaries of the **explicit grid** (tracks declared via `grid-template-columns` or `grid-template-rows`). They cannot reference lines in the implicit grid created dynamically by auto-placement.
</details>

<details>
<summary><strong>6. How does naming lines with <code>-start</code> and <code>-end</code> create named grid areas?</strong></summary>

Per the CSS Grid specification, if you name lines with corresponding prefixes and `-start` / `-end` suffixes (e.g. `[main-start]` and `[main-end]` for columns and rows), the browser automatically constructs a named grid area called `main`. You can then assign items using `grid-area: main;`.
</details>

<details>
<summary><strong>7. How do you create an overlapping layer between two grid items?</strong></summary>

By assigning them overlapping grid coordinates (e.g. Item A has `grid-column: 1 / 4; grid-row: 1 / 3;` and Item B has `grid-column: 3 / 6; grid-row: 2 / 4;`). Their stacking order can then be adjusted using `z-index`.
</details>

<details>
<summary><strong>8. What is the default span of a grid item if no end line is specified?</strong></summary>

The default span is `span 1`. If you declare `grid-column-start: 2;` without an end line, it automatically stops at line `3`.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Full-Width Header
- **Task**: In a 4-column grid, span the header across all columns using negative line indexing.

<details>
<summary>View Solution</summary>

```css
.header {
  grid-column: 1 / -1;
}
```
</details>

### Exercise 2 (Easy): Spanning 2 Rows
- **Task**: Make a featured card in a grid span 2 rows tall without hardcoding the row start line.

<details>
<summary>View Solution</summary>

```css
.featured-card {
  grid-row: span 2;
}
```
</details>

### Exercise 3 (Medium): Asymmetric 3-Column Layout
- **Task**: In a 3-column grid, place the sidebar in column 1, and make the main article span columns 2 and 3.

<details>
<summary>View Solution</summary>

```css
.sidebar {
  grid-column: 1;
}

.article {
  grid-column: 2 / 4; /* or 2 / span 2 */
}
```
</details>

### Exercise 4 (Medium): Overlapping Image Card with Text
- **Task**: Position an image in columns 1 to 3, and position a card in columns 2 to 4 in the same row so they overlap by 1 column.

<details>
<summary>View Solution</summary>

```css
.container {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  align-items: center;
}

.image {
  grid-column: 1 / 4;
  grid-row: 1;
}

.text-card {
  grid-column: 2 / 5;
  grid-row: 1;
  z-index: 2;
  background: #ffffff;
  padding: 24px;
}
```
</details>

### Exercise 5 (Hard): Named Editorial Layout Lines
- **Task**: Define an explicit grid with named lines `[full-start]`, `[content-start]`, `[content-end]`, `[full-end]` and place elements accordingly.

<details>
<summary>View Solution</summary>

```css
.editorial-grid {
  display: grid;
  grid-template-columns: 
    [full-start] 1fr 
    [content-start] min(100%, 800px) [content-end] 
    1fr [full-end];
}

.prose {
  grid-column: content-start / content-end;
}

.hero-breakout {
  grid-column: full-start / full-end;
}
```
</details>

### Exercise 6 (Hard): Four-Value Grid Area Coordinate
- **Task**: Place an element into rows 2 to 4 and columns 3 to 6 using the single `grid-area` property shorthand.

<details>
<summary>View Solution</summary>

```css
.placed-component {
  /* Syntax: row-start / col-start / row-end / col-end */
  grid-area: 2 / 3 / 4 / 6;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Line Placement Shorthands */
grid-column: 1 / 3;            /* Column start 1, end 3 */
grid-column: 1 / -1;           /* Full width of explicit grid */
grid-column: 2 / span 2;       /* Start at 2, span 2 columns */
grid-row: span 3;              /* Span 3 rows from auto slot */

/* Grid Area Shorthand */
grid-area: 1 / 2 / 3 / 4;      /* row-start / col-start / row-end / col-end */

/* Named Lines */
grid-template-columns: [main-start] 1fr [main-end];
grid-column: main-start / main-end;
```

---

## 13 Further Reading

- [MDN Web Docs: Line-based placement with CSS Grid](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Grid_layout_using_line_based_placement)
- [W3C CSS Grid Layout Module Level 1: Line-based Placement](https://www.w3.org/TR/css-grid-1/#line-placement)
- [web.dev: Learn CSS - Placement](https://web.dev/learn/css/grid/#placement)
- [Smashing Magazine: Naming Things In CSS Grid](https://www.smashingmagazine.com/2017/10/naming-css-grid-lines/)

---

[Previous: 02-defining-tracks-and-fr-units.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/02-defining-tracks-and-fr-units.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Next: 04-grid-areas-and-template-areas.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/04-grid-areas-and-template-areas.md)
