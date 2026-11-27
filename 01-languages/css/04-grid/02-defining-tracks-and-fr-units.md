# Defining Tracks and fr Units

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `04-grid/01-grid-mental-model.md` |
| **Estimated Reading Time** | 24 minutes |
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

In traditional web layout, dividing a container into flexible columns was fraught with mathematical rounding errors. Setting columns to `width: 33.333%` when gaps were present required awkward `calc()` statements like `width: calc((100% - 32px) / 3);`. Any alteration to container padding or borders broke the layout.

CSS Grid introduced two revolutionary features to solve this:
1. **The Fractional Unit (`fr`)**: Represents a fraction of the **free, unallocated space** remaining in the grid container *after* all fixed, percentage, and gap values have been subtracted.
2. **The `repeat()` notation**: A compact syntactic expression to define dozens of identical or patterned tracks without repetitive declarations.

Mastering track definitions and the fractional unit allows you to build resilient, proportional layout structures with zero brittle percentage calculations.

---

## 2 Core Concepts

### 1. `grid-template-columns` and `grid-template-rows`

These properties declare the **Explicit Grid**—the exact list of tracks created along the inline (column) and block (row) axes.

Every space-separated value represents one track width:
```css
/* Creates 3 columns: 200px fixed, 1 share fluid, 2 shares fluid */
.container {
  grid-template-columns: 200px 1fr 2fr;
}
```

### 2. The Fractional Unit (`fr`)

- The `fr` unit is **not a length** (like `px` or `rem`) and **not a percentage** (like `%`).
- It is a flexible sizing factor that represents a share of **positive free space**.
- The browser calculates all fixed dimensions (`px`, `rem`, `auto`, `gap`) first. Whatever empty space remains is divided among the `fr` tracks according to their ratios.

### 3. The `repeat()` Function

Instead of writing `1fr 1fr 1fr 1fr 1fr 1fr`, CSS Grid provides the `repeat()` notation:

```css
/* Syntax: repeat(<number-of-repetitions>, <track-size-list>) */
grid-template-columns: repeat(6, 1fr);

/* Alternating repeat pattern: 100px 1fr 100px 1fr */
grid-template-columns: repeat(2, 100px 1fr);
```

### 4. Sizing Keywords: `auto`, `min-content`, `max-content`

- **`auto`**: Sizes the track based on the content of the items inside it, but expands to fill free space if available (unlike `min-content`).
- **`min-content`**: The smallest possible track size that prevents internal text from overflowing (sizes to the longest word).
- **`max-content`**: The size the track needs to display all content on a single line without wrapping text.

---

## 3 How It Works Internally

### The Track Sizing Algorithm for `fr` Units

```
Container Width = 1000px, Gap = 20px (2 gaps = 40px)
Track Definition: grid-template-columns: 200px 1fr 3fr;

Step 1: Subtract Non-Flexible Sizing (Fixed tracks and Gaps)
Non-flexible space = 200px (Fixed Col 1) + 40px (Gaps) = 240px

Step 2: Calculate Positive Free Space
Free Space = 1000px - 240px = 760px

Step 3: Calculate the Value of 1fr
Total fr shares = 1fr + 3fr = 4fr
1fr = 760px / 4 = 190px

Step 4: Distribute to Tracks
- Column 1: 200px (Fixed)
- Column 2: 1fr = 190px
- Column 3: 3fr = 190px * 3 = 570px

Sum Verification: 200 + 190 + 570 + 40 = 1000px (Exact fit!)
```

```
+-------------------------------------------------------------------------+
| Grid Container: 1000px                                                  |
|  +--------------+  +--------------+  +-------------------------------+  |
|  | Fixed: 200px |  | 1fr: 190px   |  | 3fr: 570px                    |  |
|  +--------------+  +--------------+  +-------------------------------+  |
|          ▲                 ▲                         ▲                  |
|          │                 │                         │                  |
|       Column 1          Column 2                  Column 3              |
|                     (Gap = 20px)              (Gap = 20px)              |
+-------------------------------------------------------------------------+
```

---

## 4 Syntax & Usage

```css
.container {
  display: grid;

  /* Basic Columns & Rows */
  grid-template-columns: 250px 1fr;
  grid-template-rows: 60px 1fr 60px;

  /* Using repeat() */
  grid-template-columns: repeat(12, 1fr);             /* 12-column grid */
  grid-template-columns: repeat(4, [col-start] 1fr);  /* Named repeat lines */

  /* Mixed Units */
  grid-template-columns: 100px min-content 1fr max-content;

  /* Track Gutters */
  gap: 20px;
}
```

---

## 5 Practical Examples

### Example 1: Standard 12-Column Responsive Layout Grid

```css
.grid-system-12 {
  display: grid;
  grid-template-columns: repeat(12, 1fr);
  gap: 16px;
  max-width: 1200px;
  margin: 0 auto;
}

.col-span-8 {
  grid-column: span 8; /* Consumes 8 of the 12 fractional columns */
}

.col-span-4 {
  grid-column: span 4; /* Consumes remaining 4 columns */
}
```

### Example 2: Fixed Sidebar with Fluid Dual Content Panes

```css
.dashboard-shell {
  display: grid;
  grid-template-columns: 260px 2fr 1fr;
  min-height: 100vh;
  gap: 24px;
  padding: 24px;
  background-color: #f8fafc;
}

.sidebar-nav {
  /* Fits cleanly into the 260px fixed track */
  background: #0f172a;
  border-radius: 8px;
}

.primary-feed {
  /* Gets 2/3 of remaining screen width */
  background: #ffffff;
  border-radius: 8px;
}

.activity-panel {
  /* Gets 1/3 of remaining screen width */
  background: #ffffff;
  border-radius: 8px;
}
```

### Example 3: Edge Case — Fractional Tracks Summing to Less Than 1

When the sum of `fr` units is less than 1, the browser does not expand them to fill 100% of the container:

```css
.partial-fraction-grid {
  display: grid;
  width: 1000px;
  /* Sum is 0.5fr + 0.2fr = 0.7fr!
     Free space is NOT divided by 0.7. Instead, 1fr is treated as the full width!
     Col 1 = 500px, Col 2 = 200px. Remaining 300px is left empty! */
  grid-template-columns: 0.5fr 0.2fr;
}
```

### Example 4: Failure Case — Using `1fr` without Accounting for `min-width`

```css
/* FAILURE PATTERN */
.code-preview-grid {
  display: grid;
  grid-template-columns: 1fr 1fr; /* Two 50/50 columns */
  gap: 16px;
}

.code-snippet {
  /* BUG: By default, 1fr implies minmax(auto, 1fr)!
     If a long code line exceeds 50% width, the track will blow out
     and force the entire grid to scroll horizontally! */
}

/* FIXED PATTERN */
.code-preview-grid-fixed {
  display: grid;
  /* minmax(0, 1fr) explicitly allows the track to shrink below content size! */
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 16px;
}
```

### Example 5: Intrinsic Sizing with `min-content` and `max-content`

```css
.table-like-grid {
  display: grid;
  /* Col 1 sizes to fit icon tightly (min-content)
     Col 2 takes all remaining space (1fr)
     Col 3 sizes to fit full action button text without wrapping (max-content) */
  grid-template-columns: min-content 1fr max-content;
  align-items: center;
  gap: 16px;
  padding: 12px;
  border-bottom: 1px solid #e2e8f0;
}
```

---

## 6 Best Practices

1. **Always prefer `fr` over `%` for grid tracks**: `fr` automatically accounts for `gap` spacing, eliminating tedious `calc()` deductions.
2. **Use `repeat()` notation for uniform tracks**: Writing `repeat(4, 1fr)` is cleaner and more maintainable than `1fr 1fr 1fr 1fr`.
3. **Use `minmax(0, 1fr)` whenever tracks contain text that might truncate**: Protects against unexpected track blowout caused by `min-width: auto`.
4. **Mix fixed and fluid tracks intentionally**: A common pattern is `grid-template-columns: 250px 1fr;` for sidebar + content.
5. **Use `max-content` for buttons and badges**: Guarantees that interactive text within the track will never wrap onto two lines.
6. **Remember that `repeat()` can repeat multi-track patterns**: `repeat(3, 20px 1fr)` creates an alternating 6-track grid.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Root Cause | Fix |
| :--- | :--- | :--- |
| Combining `%` with `gap` causes horizontal scroll | `%` does not subtract `gap` width before sizing | Switch from `%` to `fr` units |
| Track expands wider than `1fr` | `1fr` defaults to `minmax(auto, 1fr)`, respecting content minimums | Use `minmax(0, 1fr)` |
| Writing comma-separated track lists | CSS Grid syntax uses whitespace separation, not commas | Use `grid-template-columns: 1fr 1fr;` |
| Using `repeat()` with invalid track syntax | Forgetting parentheses or track count | Use `repeat(3, 1fr)` |
| Expecting `fr` to work outside CSS Grid | The `fr` unit is valid **only** in CSS Grid track definitions | Use `%` or `flex` in other contexts |
| Using huge `fr` numbers (`100fr 200fr`) | Ratios reduce identically (`1fr 2fr`); huge numbers cause math clutter | Simplify to lowest common integer ratios |

---

## 8 Performance, Accessibility & Browser Support

### Engine Track Computation

- Resolving `fr` track dimensions requires only basic arithmetic operations in the layout engine.
- When `min-content` or `max-content` keywords are included in track lists, the engine must perform an intrinsic sizing pass on child elements before distributing free space to `fr` tracks.
- For extremely large grids (e.g. 100+ columns), pure `repeat(N, 1fr)` without content-dependent keywords resolves significantly faster.

### Accessibility

- Track sizing changes how content is rendered visually, but has zero effect on the accessibility tree or keyboard focus sequence.
- Sighted users navigating zoomed displays (up to 400% zoom per WCAG 1.4.10) benefit immensely from `fr` units, as tracks scale down gracefully without triggering horizontal document scrollports.

---

## 9 Comparison / When to Use What

| Unit / Keyword | Type | Calculates Against | Best Use Case |
| :--- | :--- | :--- | :--- |
| `fr` | Fluid Flexible Factor | Remaining free space (after gaps) | Equal columns, flexible dashboard panes |
| `px` / `rem` | Fixed Absolute Length | Physical display pixels / root font | Sidebars, icon columns, fixed gutters |
| `%` | Percentage Length | Full container width (ignores gaps!) | Legacy layouts; avoid in modern Grid |
| `auto` | Dynamic Content | Intrinsic item size; expands if free | Header/footer rows, dynamic cards |
| `min-content` | Strict Minimum | Longest unbreakable word | Narrow icon rails, avatar columns |
| `max-content` | Strict Maximum | Total un-wrapped content length | Button columns, action links |

---

## 10 Interview Questions

<details>
<summary><strong>1. What exactly does the <code>fr</code> unit represent in CSS Grid?</strong></summary>

The `fr` (fractional) unit represents a share of the **positive free space** available inside the grid container after all non-flexible dimensions (fixed lengths like `px`, percentages, intrinsic tracks like `min-content`, and `gap` spacing) have been subtracted.
</details>

<details>
<summary><strong>2. Why is <code>grid-template-columns: 1fr 1fr 1fr</code> superior to <code>grid-template-columns: 33.33% 33.33% 33.33%</code>?</strong></summary>

Percentage values calculate against the container's raw width without factoring in `gap` spacing. If a `gap: 20px;` is added to a grid with three `33.33%` columns, the total width exceeds $100\%$, triggering an unwanted horizontal scrollbar. In contrast, `1fr` tracks automatically subtract the two 20px gaps first, dividing only the remaining space equally.
</details>

<details>
<summary><strong>3. What does <code>grid-template-columns: repeat(3, 1fr 2fr)</code> create?</strong></summary>

It creates a 6-column grid with an alternating pattern repeated 3 times: `1fr 2fr 1fr 2fr 1fr 2fr`.
</details>

<details>
<summary><strong>4. Why does a <code>1fr</code> column sometimes expand wider than its sibling <code>1fr</code> column?</strong></summary>

By specification, a solitary `1fr` declaration computes to `minmax(auto, 1fr)`. The `auto` minimum tracks the item's `min-content` size (e.g. an unhyphenated word, code string, or oversized image). If an item's content is wider than the calculated fraction, the track will expand to fit the content, breaking equal column widths. Using `minmax(0, 1fr)` eliminates this content lock.
</details>

<details>
<summary><strong>5. Can the <code>fr</code> unit be used in <code>grid-template-rows</code>?</strong></summary>

Yes, but for `fr` row tracks to function, the grid container **must have a definite, bounded height** (such as `height: 100vh` or `height: 600px`). If container height is indefinite (`height: auto`), there is no positive free space along the vertical axis, and `fr` row tracks compute identically to `auto`.
</details>

<details>
<summary><strong>6. What happens if the sum of all fractional tracks is less than 1 (e.g. <code>grid-template-columns: 0.3fr 0.3fr</code>)?</strong></summary>

The fractional tracks do not stretch to consume 100% of the container. Instead, each track claims its precise fractional percentage of the available free space (30% each), leaving the remaining 40% as empty, unallocated space on the right side of the container.
</details>

<details>
<summary><strong>7. How does the <code>max-content</code> keyword behave in track sizing?</strong></summary>

`max-content` forces the track to be exactly as wide as the largest item inside it when that item's text is laid out on a single line without wrapping.
</details>

<details>
<summary><strong>8. Is the <code>fr</code> unit valid in Flexbox or standard block layouts?</strong></summary>

No. The `fr` unit is defined exclusively within the CSS Grid specification and can only be used in grid track definition properties (`grid-template-columns`, `grid-template-rows`, `grid-auto-columns`, `grid-auto-rows`).
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic 4-Column Responsive Deck
- **Task**: Define an explicit grid of 4 equal columns with a 16px gap using the `repeat()` notation.

<details>
<summary>View Solution</summary>

```css
.card-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 16px;
}
```
</details>

### Exercise 2 (Easy): Classic 2-Pane Application Workspace
- **Task**: Create a layout with a fixed 300px sidebar on the left and a fluid workspace on the right.

<details>
<summary>View Solution</summary>

```css
.workspace-shell {
  display: grid;
  grid-template-columns: 300px 1fr;
  min-height: 100vh;
}
```
</details>

### Exercise 3 (Medium): 3:1 Editorial Split with Gap Accounting
- **Task**: Construct a 2-column layout where the main column receives 3 shares and the aside receives 1 share, with a 24px gap.

<details>
<summary>View Solution</summary>

```css
.editorial-container {
  display: grid;
  grid-template-columns: 3fr 1fr;
  gap: 24px;
}
```
</details>

### Exercise 4 (Medium): Defending `fr` Tracks from Text Blowout
- **Task**: Author a 2-column grid where long URLs in the left column will not force the track to expand past 50%.

<details>
<summary>View Solution</summary>

```css
.safe-grid {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  gap: 16px;
}

.url-text {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

### Exercise 5 (Hard): Holy Grail Track Definitions with Explicit Rows
- **Task**: Define a full-height page grid with a 60px header, dynamic middle row (fluid content + fixed aside), and 80px footer.

<details>
<summary>View Solution</summary>

```css
.holy-grail-grid {
  display: grid;
  grid-template-columns: 1fr 280px;
  grid-template-rows: 60px 1fr 80px;
  min-height: 100vh;
}

.header { grid-column: 1 / -1; }
.footer { grid-column: 1 / -1; }
```
</details>

### Exercise 6 (Hard): Intrinsic Action Table Row
- **Task**: Style a list row where the checkbox is `min-content`, title is `1fr`, and action buttons are `max-content`.

<details>
<summary>View Solution</summary>

```css
.list-row {
  display: grid;
  grid-template-columns: min-content 1fr max-content;
  align-items: center;
  gap: 16px;
  padding: 12px;
  border-bottom: 1px solid #e2e8f0;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Track Definition Basics */
grid-template-columns: 200px 1fr;              /* Fixed + Fluid */
grid-template-columns: repeat(4, 1fr);         /* 4 Equal Columns */
grid-template-columns: repeat(12, 1fr);        /* 12-Column System */

/* Intrinsic Keywords */
grid-template-columns: min-content 1fr max-content;

/* Defensive Sizing (Stops Content Blowout) */
grid-template-columns: repeat(2, minmax(0, 1fr));

/* Gaps */
gap: 1.5rem; /* row-gap and column-gap */
```

---

## 13 Further Reading

- [MDN Web Docs: An introduction to grid tracks](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Basic_concepts_of_grid_layout#grid_tracks)
- [W3C CSS Grid Layout Module Level 1: Track Sizing](https://www.w3.org/TR/css-grid-1/#track-sizing)
- [web.dev: Learn CSS - Grid Tracks and Sizing](https://web.dev/learn/css/grid/)
- [CSS-Tricks: Introduction to the CSS Grid fr Unit](https://css-tricks.com/introduction-fr-css-grid-unit/)

---

[Previous: 01-grid-mental-model.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/01-grid-mental-model.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Next: 03-placing-items-and-lines.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/03-placing-items-and-lines.md)
