# Grid Areas and Template Areas

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `04-grid/01-grid-mental-model.md`, `04-grid/03-placing-items-and-lines.md` |
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

While line-based placement (`grid-column: 1 / 3`) provides precision, it suffers from a significant drawback in complex application layouts: **readability**. Examining a stylesheet with dozens of numeric line coordinates makes it nearly impossible to visualize the page structure without mentally mapping out line numbers. Furthermore, reorganizing the layout at responsive breakpoints requires modifying individual column and row numbers on every single child element.

**CSS Grid Template Areas** (`grid-template-areas`) revolutionizes page layout by introducing visual "ASCII art" declarations:
- You draw the entire layout as a string matrix right in the CSS stylesheet.
- Child items are assigned simple semantic names (`grid-area: header;`).
- Restructuring an entire application layout at responsive breakpoints requires altering **only a single property on the container**, without touching a single child selector!

---

## 2 Core Concepts

### The Two Components of Named Area Layout

1. **On the Parent Container (`grid-template-areas`)**:
   - Defines a visual grid map.
   - Each quoted string represents a **row**.
   - Each word within that string represents a **cell** in a column.
   - Repeating a word across adjacent cells fuses them into a contiguous **named grid area**.

2. **On the Child Items (`grid-area`)**:
   - Assigns the item to the named area created on the parent:
   ```css
   .site-header { grid-area: header; }
   ```

### The Null Cell Keyword (`.`)

If a cell in your layout should remain empty or unassigned, use one or more consecutive period characters (`.` or `...`):

```css
grid-template-areas:
  "header  header"
  "sidebar content"
  ".       footer"; /* First cell in row 3 is blank */
```

### The Strict Rectangular Rule

A named area **must always form a single contiguous rectangle** (e.g. $1 \times 2$, $2 \times 2$, $3 \times 1$).
You **cannot** create "L-shaped", "T-shaped", or disjoint non-adjacent areas. Declaring an invalid non-rectangular shape causes the entire `grid-template-areas` declaration to be ignored by the browser parser!

---

## 3 How It Works Internally

```
+-------------------------------------------------------------+
| grid-template-areas:                                        |
|   "header  header  header"                                  |
|   "sidebar main    main"                                    |
|   "sidebar footer  footer";                                 |
|                                                             |
| Generated Coordinates:                                      |
| +---------------------------------------------------------+ |
| | header: row 1/2, col 1/4 (Spans 3 cols)                 | |
| +-------------------------+-------------------------------+ |
| | sidebar:                | main:                         | |
| | row 2/4, col 1/2        | row 2/3, col 2/4              | |
| | (Spans 2 rows)          +-------------------------------+ |
| |                         | footer:                       | |
| |                         | row 3/4, col 2/4              | |
| +-------------------------+-------------------------------+ |
+-------------------------------------------------------------+
```

When the browser parses `grid-template-areas`:
1. It validates that every string has the **exact same number of column tokens**.
2. It validates that every unique identifier forms a **strict rectangle**.
3. It automatically generates implicit named grid lines for every area:
   - For area `main`, it creates `main-start` and `main-end` lines on both row and column axes!

---

## 4 Syntax & Usage

```css
.app-container {
  display: grid;
  grid-template-columns: 240px 1fr 200px;
  grid-template-rows: 60px 1fr 60px;
  grid-template-areas:
    "head head  head"
    "nav  main  ads"
    "foot foot  foot";
  gap: 16px;
}

/* Assigning children to named areas */
.header  { grid-area: head; }
.nav     { grid-area: nav; }
.main    { grid-area: main; }
.ads     { grid-area: ads; }
.footer  { grid-area: foot; }
```

---

## 5 Practical Examples

### Example 1: Classic Holy Grail Page Layout

```css
.site-layout {
  display: grid;
  grid-template-columns: 220px 1fr;
  grid-template-rows: auto 1fr auto;
  grid-template-areas:
    "header  header"
    "sidebar main"
    "footer  footer";
  min-height: 100vh;
}

.site-header {
  grid-area: header;
  background-color: #0f172a;
  color: #ffffff;
  padding: 16px;
}

.site-sidebar {
  grid-area: sidebar;
  background-color: #f8fafc;
  border-right: 1px solid #e2e8f0;
  padding: 20px;
}

.site-main {
  grid-area: main;
  padding: 24px;
}

.site-footer {
  grid-area: footer;
  background-color: #1e293b;
  color: #ffffff;
  padding: 16px;
}
```

### Example 2: Radical Responsive Reorganization in 10 Lines of CSS

Notice how the entire layout shifts from a single vertical stack on mobile to a 3-column dashboard on desktop without altering child selectors:

```css
/* Mobile-First: Simple vertical stack */
.dashboard-grid {
  display: grid;
  grid-template-columns: 1fr;
  grid-template-areas:
    "header"
    "nav"
    "content"
    "aside"
    "footer";
  gap: 16px;
}

/* Desktop: 3-column dashboard */
@media (min-width: 992px) {
  .dashboard-grid {
    grid-template-columns: 220px 1fr 280px;
    grid-template-areas:
      "header  header  header"
      "nav     content aside"
      "footer  footer  footer";
  }
}
```

### Example 3: Edge Case — Null Cells for Inset Forms

```css
.centered-form-layout {
  display: grid;
  grid-template-columns: 1fr min(100%, 480px) 1fr;
  grid-template-areas:
    ". form .";
  min-height: 100vh;
  align-items: center;
}

.login-form-card {
  grid-area: form;
  background: #ffffff;
  padding: 32px;
  border-radius: 8px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}
```

### Example 4: Failure Case — Non-Rectangular Area ("L-Shape" Bug)

```css
/* FAILURE PATTERN: Invalid CSS */
.invalid-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  /* BUG: Area 'sidebar' forms an L-shape (row 1 col 1, row 2 col 1, row 2 col 2).
     CSS Grid requires all named areas to be RECTANGLES! */
  grid-template-areas:
    "sidebar content"
    "sidebar sidebar"; /* PARSER ERROR: Declaration is discarded! */
}

/* FIXED PATTERN */
.valid-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  grid-template-areas:
    "sidebar content"
    "footer  footer"; /* Both are clean rectangles */
}
```

### Example 5: Magazine Feature Article Layout

```css
.magazine-article {
  display: grid;
  grid-template-columns: 3fr 1fr;
  grid-template-areas:
    "title   title"
    "lead    lead"
    "prose   author"
    "prose   related";
  gap: 24px;
  max-width: 900px;
  margin: 0 auto;
}

.article-title   { grid-area: title; }
.article-lead    { grid-area: lead; font-size: 1.25rem; font-weight: 500; }
.article-prose   { grid-area: prose; }
.article-author  { grid-area: author; border-left: 2px solid #e2e8f0; padding-left: 16px; }
.article-related { grid-area: related; }
```

---

## 6 Best Practices

1. **Align string whitespace visually in your code editor**: Formatting `grid-template-areas` strings with aligned column spacing makes your CSS self-documenting.
2. **Always ensure equal column tokens per row**: If row 1 has 3 tokens (`"a b c"`), row 2 must also have exactly 3 tokens (`"d e f"`).
3. **Use the period (`.`) for empty tracks**: Cleanly reserves space without requiring artificial placeholder HTML elements.
4. **Leverage template areas for responsive breakpoints**: Restructuring a media query via `grid-template-areas` is 10x cleaner than rewriting `grid-column` numbers on 6 different selectors.
5. **Remember that areas must be rectangular**: Never attempt L-shaped or disconnected T-shaped areas.
6. **Ensure DOM order remains logical**: Do not visually move forms or articles to the top using template areas if they appear at the bottom of the DOM.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
| :--- | :--- | :--- |
| Mismatched column token count | Row 1 has 3 tokens, row 2 has 2 tokens; syntax error | Ensure every row string has identical token counts |
| Creating an L-shaped or disconnected area | The CSS specification mandates strict rectangles | Redesign area to form a rectangle or use line placement |
| Quoting the `grid-area` value on the child | `grid-area: "header"` is invalid; must be an unquoted identifier | Write `grid-area: header;` |
| Forgetting to define matching track sizes | Template areas with 3 columns needs 3 track sizes or `1fr` defaults | Define `grid-template-columns` explicitly |
| Using punctuation inside area names | Identifiers cannot start with digits or contain special characters | Use simple alphanumeric kebab-case names |
| Expecting unassigned elements to disappear | Children not assigned a `grid-area` auto-place into vacant cells | Assign all children or let them flow into remaining tracks |

---

## 8 Performance, Accessibility & Browser Support

### Engine Compilation

- During stylesheet parsing, the browser validates area names and compiles them into a two-dimensional integer lookup matrix.
- `grid-template-areas` has **zero runtime performance difference** compared to explicit numeric line-based placement (`grid-column: 1 / 3`). The resulting layout geometry is mathematically identical.

### Accessibility Standards (WCAG 1.3.2)

- Because `grid-template-areas` makes it effortless to shift a footer to the top or a sidebar to the bottom, the danger of **visual/DOM disconnection** is elevated.
- Always inspect your rendered layout with a keyboard (pressing `Tab`) and a screen reader to verify that the focus and reading progression match the visual flow.

---

## 9 Comparison / When to Use What

| Technique | Readability | Responsive Refactoring | Complex Overlaps | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **`grid-template-areas`** | **Exceptional (Visual ASCII)** | **Trivial (1-property change)** | Limited (No overlapping) | Macro page shells, dashboards, articles |
| **Line-based (`grid-column`)** | Moderate (Numeric counting) | Complex (Rewriting every child) | **Full Support (Overlapping cells)** | Micro components, photo grids, collages |
| **Flexbox (`display: flex`)** | Standard (Axis-based) | Easy | None (Flow only) | Toolbars, navigation menus, chip clouds |

---

## 10 Interview Questions

<details>
<summary><strong>1. What are the strict geometric rules for named areas in <code>grid-template-areas</code>?</strong></summary>

A named grid area must always form a single contiguous, solid rectangle. L-shaped, T-shaped, cross-shaped, or disjoint areas are strictly forbidden. If an invalid non-rectangular shape is detected, the browser engine discards the entire `grid-template-areas` declaration as a syntax error.
</details>

<details>
<summary><strong>2. How does the null cell keyword (<code>.</code>) work in <code>grid-template-areas</code>?</strong></summary>

One or more consecutive period characters (e.g. `.` or `...`) represent an unassigned, empty grid cell within the matrix. It reserves track space in the layout without associating it with any named area.
</details>

<details>
<summary><strong>3. Why is <code>grid-area: header;</code> correct while <code>grid-area: "header";</code> is invalid?</strong></summary>

In CSS, named grid areas are custom identifiers (like custom property names or animation names), not string literals. Quoting the identifier causes a parser error, and the rule will be ignored.
</details>

<details>
<summary><strong>4. What implicit grid lines are generated when you define an area called <code>sidebar</code>?</strong></summary>

The browser engine automatically generates four named grid lines corresponding to the area's boundaries:
- `sidebar-start` and `sidebar-end` for columns.
- `sidebar-start` and `sidebar-end` for rows.
</details>

<details>
<summary><strong>5. How does <code>grid-template-areas</code> streamline responsive design?</strong></summary>

Instead of overriding numeric line placements (`grid-column` and `grid-row`) across multiple child selectors inside media queries, you only need to redefine the single `grid-template-areas` property on the parent container. The child selectors remain completely untouched.
</details>

<details>
<summary><strong>6. Can you overlap two elements using <code>grid-template-areas</code> alone?</strong></summary>

No. Within a `grid-template-areas` definition, each cell can contain only one area identifier, making it impossible to assign two named areas to the exact same cell directly. To overlap items, you must use line-based placement (`grid-column` / `grid-row`) or place items onto the implicit lines generated by the areas.
</details>

<details>
<summary><strong>7. What happens if a row in <code>grid-template-areas</code> has a different number of column tokens than other rows?</strong></summary>

The entire declaration is invalid and is completely dropped by the CSS parser, causing the grid to fall back to auto-placement without template areas.
</details>

<details>
<summary><strong>8. What happens to grid children that are not assigned a <code>grid-area</code>?</strong></summary>

Any child element without a matching `grid-area` will be placed by the browser's auto-placement algorithm into any remaining empty cells or into newly generated implicit tracks below the template area.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic Card Header-Body-Footer
- **Task**: Define a card layout with named areas `header`, `body`, and `footer` stacking vertically.

<details>
<summary>View Solution</summary>

```css
.card {
  display: grid;
  grid-template-rows: auto 1fr auto;
  grid-template-areas:
    "header"
    "body"
    "footer";
}

.card-header { grid-area: header; }
.card-body   { grid-area: body; }
.card-footer { grid-area: footer; }
```
</details>

### Exercise 2 (Easy): Sidebar Layout with Empty Top-Right Cell
- **Task**: Create a 2x2 grid where the top row contains a sidebar on the left and an empty cell on the right using the period keyword.

<details>
<summary>View Solution</summary>

```css
.grid-layout {
  display: grid;
  grid-template-columns: 200px 1fr;
  grid-template-areas:
    "sidebar ."
    "sidebar main";
}

.sidebar { grid-area: sidebar; }
.main    { grid-area: main; }
```
</details>

### Exercise 3 (Medium): Responsive 3-Row Holy Grail
- **Task**: Create a layout that stacks vertically on mobile and switches to a 3-column Holy Grail layout on screens 768px and wider.

<details>
<summary>View Solution</summary>

```css
.page-shell {
  display: grid;
  grid-template-areas:
    "header"
    "nav"
    "main"
    "footer";
}

@media (min-width: 768px) {
  .page-shell {
    grid-template-columns: 200px 1fr;
    grid-template-areas:
      "header header"
      "nav    main"
      "footer footer";
  }
}

.page-header { grid-area: header; }
.page-nav    { grid-area: nav; }
.page-main   { grid-area: main; }
.page-footer { grid-area: footer; }
```
</details>

### Exercise 4 (Medium): Centered Form Dialog with Flanking Space
- **Task**: Use null cells to center a 400px login form within a full-height screen.

<details>
<summary>View Solution</summary>

```css
.login-screen {
  display: grid;
  grid-template-columns: 1fr 400px 1fr;
  grid-template-rows: 1fr auto 1fr;
  grid-template-areas:
    ". .    ."
    ". card ."
    ". .    .";
  min-height: 100vh;
}

.login-card {
  grid-area: card;
  background: #ffffff;
  padding: 24px;
  border-radius: 8px;
}
```
</details>

### Exercise 5 (Hard): Complex Editorial Layout with Pullquote
- **Task**: Build an article layout featuring a title spanning 3 columns, a 2-column prose body, and a sidebar pullquote.

<details>
<summary>View Solution</summary>

```css
.editorial-grid {
  display: grid;
  grid-template-columns: 2fr 1fr;
  grid-template-areas:
    "title title"
    "lead  lead"
    "prose quote"
    "prose author";
  gap: 20px;
}

.art-title  { grid-area: title; }
.art-lead   { grid-area: lead; }
.art-prose  { grid-area: prose; }
.art-quote  { grid-area: quote; font-style: italic; }
.art-author { grid-area: author; }
```
</details>

### Exercise 6 (Hard): Dashboard Layout with Inset Topbar
- **Task**: Build a dashboard where a full-height sidebar spans the left edge, and a topbar + main content occupy the right.

<details>
<summary>View Solution</summary>

```css
.admin-shell {
  display: grid;
  grid-template-columns: 260px 1fr;
  grid-template-rows: 64px 1fr;
  grid-template-areas:
    "sidebar topbar"
    "sidebar content";
  min-height: 100vh;
}

.admin-sidebar { grid-area: sidebar; background: #0f172a; }
.admin-topbar  { grid-area: topbar;  background: #ffffff; border-bottom: 1px solid #e2e8f0; }
.admin-content { grid-area: content; padding: 24px; }
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Container Template Areas */
.container {
  display: grid;
  grid-template-columns: 200px 1fr;
  grid-template-areas:
    "header  header"
    "sidebar main"
    "footer  footer";
}

/* Item Assignment (Unquoted!) */
.header  { grid-area: header; }
.sidebar { grid-area: sidebar; }
.main    { grid-area: main; }
.footer  { grid-area: footer; }

/* Rules: */
/* 1. Areas must form single solid RECTANGLES. */
/* 2. Every row string must have identical token counts. */
/* 3. The period '.' denotes an empty null cell. */
```

---

## 13 Further Reading

- [MDN Web Docs: Grid template areas](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Grid_template_areas)
- [W3C CSS Grid Layout Module Level 1: Named Areas](https://www.w3.org/TR/css-grid-1/#grid-template-areas-property)
- [web.dev: Learn CSS - Grid Areas](https://web.dev/learn/css/grid/)
- [CSS-Tricks: CSS Grid Template Areas](https://css-tricks.com/snippets/css/complete-guide-grid/#aa-grid-template-areas)

---

[Previous: 03-placing-items-and-lines.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/03-placing-items-and-lines.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Next: 05-auto-placement-and-implicit-grid.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/05-auto-placement-and-implicit-grid.md)
