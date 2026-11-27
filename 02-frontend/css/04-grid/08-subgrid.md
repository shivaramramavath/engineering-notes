# CSS Subgrid Deep Dive

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Modern Standard |
| **Prerequisites** | `04-grid/01-grid-mental-model.md` through `07-alignment-in-grid.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2023) |
| **Browsers Tested** | Chrome 117+, Firefox 71+, Safari 16.0+, Edge 117+ |
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

In CSS Grid Level 1, a grid container's tracks **only apply to its direct children**.
If a grid item contains child elements (grandchildren of the grid container), those grandchildren are completely isolated inside their parent's box.

This limitation caused a notorious, ubiquitous web design flaw: **The Unequal Card Alignment Bug**.
Imagine 3 cards in a row. Each card contains:
1. A header/title
2. An image
3. A variable-length description paragraph
4. A footer with a "Buy Now" button

Because each card is an independent element, Card 1's title might be 1 line, while Card 2's title is 3 lines. Consequently, the images, descriptions, and buttons across the 3 cards were vertically misaligned and jagged!

**CSS Subgrid** (CSS Grid Level 2, Baseline 2023) solves this problem permanently. It allows a nested grid item to **inherit the track definitions of its parent grid**, enabling grandchildren across multiple sibling cards to align perfectly to the exact same global horizontal or vertical tracks!

---

## 2 Core Concepts

### What is a Subgrid?

A subgrid is an element that is both:
1. A **Grid Item** (participating in its parent's grid tracks).
2. A **Grid Container** for its own children, where one or both of its track definitions are declared as `subgrid`:

```css
.card {
  /* Step 1: Span across 3 rows of the parent grid */
  grid-row: span 3;

  /* Step 2: Establish a grid container */
  display: grid;

  /* Step 3: INHERIT parent row tracks instead of defining new ones! */
  grid-template-rows: subgrid;
}
```

### Single-Axis vs. Dual-Axis Subgrid

Subgrid can be adopted selectively on **either or both axes**:
- **Row Subgrid**: `grid-template-rows: subgrid;` (inherits row tracks; defines custom column tracks).
- **Column Subgrid**: `grid-template-columns: subgrid;` (inherits column tracks; defines custom row tracks).
- **Dual-Axis Subgrid**: Both rows and columns inherit parent tracks.

### Track and Gap Inheritance

- **Track Sizes**: The subgrid does not define track sizes. It adopts the exact pixel, `fr`, or `auto` sizes computed by the parent grid.
- **Content Contribution**: Children of the subgrid participate in the parent's track sizing! If Card 2's title requires $80\text{px}$, the parent grid's title row expands to $80\text{px}$ for *all* sibling cards!
- **Gap Inheritance**: By default, the subgrid inherits the parent's `gap`. However, the subgrid can override its gap locally (e.g. `row-gap: 0px`).

---

## 3 How It Works Internally

```
Without Subgrid (Each Card is an Isolated Box)
Card 1                       Card 2 (Long Title)          Card 3
+-----------------------+    +-----------------------+    +-----------------------+
| Title (Short)         |    | Title (Wraps onto     |    | Title (Short)         |
+-----------------------+    | three whole lines)    |    +-----------------------+
| Image                 |    +-----------------------+    | Image                 |
+-----------------------+    | Image                 |    +-----------------------+
| Text...               |    +-----------------------+    | Text...               |
+-----------------------+    | Text...               |    +-----------------------+
| Button                |    +-----------------------+    | Button                |
+-----------------------+    | Button                |    +-----------------------+
                             +-----------------------+
Result: Images and buttons are jagged and misaligned across cards!

With CSS Subgrid (grid-template-rows: subgrid)
Parent Grid Row 1 (Title):  [Title (Short)]    [Title (3 Lines)]    [Title (Short)]  <-- Sized to tallest!
--------------------------------------------------------------------------------------
Parent Grid Row 2 (Image):  [Image        ]    [Image          ]    [Image        ]  <-- Aligned!
--------------------------------------------------------------------------------------
Parent Grid Row 3 (Body):   [Text...      ]    [Text...        ]    [Text...      ]  <-- Aligned!
--------------------------------------------------------------------------------------
Parent Grid Row 4 (Button): [Button       ]    [Button         ]    [Button       ]  <-- Aligned!
Result: Flawless, synchronized alignment across every single card component!
```

---

## 4 Syntax & Usage

```css
/* Parent Grid */
.parent-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  /* Automatically generate 4 rows per card: Title, Image, Body, Button */
  grid-auto-rows: auto;
  gap: 24px;
}

/* Child Card: Becomes Subgrid */
.card-item {
  /* 1. Span the number of tracks children will occupy */
  grid-row: span 4;

  /* 2. Declare grid display */
  display: grid;

  /* 3. Adopt parent rows */
  grid-template-rows: subgrid;

  /* Optional: Override gap */
  row-gap: 12px;
}
```

> [!NOTE]
> **Baseline Status: Widely Available (Baseline 2023)**
> CSS Subgrid is supported across all major rendering engines: Chrome 117+ (Sept 2023), Safari 16.0+ (Sept 2022), Firefox 71+ (Dec 2019), and Edge 117+.

---

## 5 Practical Examples

### Example 1: The Aligned Pricing Table

```css
.pricing-deck {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 24px;
  align-items: start;
}

.pricing-card {
  /* Spans 4 parent row tracks: Tier, Price, Features, CTA */
  grid-row: span 4;
  display: grid;
  grid-template-rows: subgrid;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 24px;
}

.tier-title    { font-size: 1.25rem; font-weight: 700; }
.tier-price    { font-size: 2.5rem; font-weight: 800; color: #2563eb; }
.tier-features { list-style: none; padding: 0; margin: 0; }
.tier-button   { padding: 12px; background: #0f172a; color: #ffffff; border-radius: 6px; }
```

### Example 2: Column Subgrid in a Form Layout

Aligning input labels and text fields inside multiple fieldsets to the same column tracks:

```css
.form-shell {
  display: grid;
  /* Col 1: Label (auto), Col 2: Input (1fr) */
  grid-template-columns: auto 1fr;
  gap: 16px 24px;
  max-width: 600px;
}

.form-fieldset {
  /* Span both columns */
  grid-column: span 2;
  display: grid;
  grid-template-columns: subgrid; /* Inherits label + input widths! */
  border: none;
  padding: 0;
  margin: 0;
}

.form-label {
  grid-column: 1;
  font-weight: 600;
  text-align: right;
}

.form-input {
  grid-column: 2;
  padding: 8px 12px;
  border: 1px solid #cbd5e1;
  border-radius: 4px;
}
```

### Example 3: Edge Case — Named Lines in Subgrid

A subgrid can inherit or redefine named lines from its parent grid:

```css
.parent-container {
  display: grid;
  grid-template-columns: [full-start] 1fr [content-start] 3fr [content-end] 1fr [full-end];
}

.nested-component {
  grid-column: full-start / full-end;
  display: grid;
  grid-template-columns: subgrid;
}

.nested-inner-text {
  /* Can target parent's named lines directly! */
  grid-column: content-start / content-end;
}
```

### Example 4: Failure Case — Forgetting to Span Parent Tracks

```css
/* FAILURE PATTERN */
.broken-card {
  display: grid;
  grid-template-rows: subgrid;
  /* BUG: Forgot to declare grid-row: span N!
     Defaults to span 1 track. All 4 child elements will be forced
     into a single 1-track cell and overlap or cause overflow! */
}

/* FIXED PATTERN */
.clean-card {
  grid-row: span 4; /* Reserves 4 tracks for 4 child elements */
  display: grid;
  grid-template-rows: subgrid;
}
```

### Example 5: Progressive Enhancement Fallback for Legacy Browsers

```css
/* Baseline Fallback for pre-2023 engines */
.card {
  display: flex;
  flex-direction: column;
}

.card-button {
  margin-top: auto; /* Fallback button alignment */
}

/* Modern Subgrid Enhancement */
@supports (grid-template-rows: subgrid) {
  .card-deck {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  }
  
  .card {
    grid-row: span 3;
    display: grid;
    grid-template-rows: subgrid;
  }
  
  .card-button {
    margin-top: 0;
  }
}
```

---

## 6 Best Practices

1. **Always specify `span N` when creating a subgrid**: Ensure the span count equals the number of child elements participating in the subgrid.
2. **Use Subgrid for synchronized card rows**: It is the single most effective tool to align titles, descriptions, and buttons across sibling cards.
3. **Use Column Subgrids for accessible form grids**: Ensures field labels across separate `<fieldset>` elements share identical widths without hardcoded pixel measurements.
4. **Provide a clean `@supports (grid-template-rows: subgrid)` fallback**: Use Flexbox with `margin-top: auto` for older browsers.
5. **Adjust `gap` locally if needed**: If parent grid gap is 24px, but internal card elements should be 8px apart, declare `row-gap: 8px` on the subgrid.
6. **Keep subgrid depth manageable**: Avoid nesting subgrids 5+ levels deep, which can complicate stylesheet debugging.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Solution |
| :--- | :--- | :--- |
| Forgetting `grid-row: span N` | Subgrid defaults to 1 track span; all children compress into 1 cell | Declare `grid-row: span [number of children]` |
| Subgrid children overlapping | Children assigned to same subgrid track without explicit row placement | Place children sequentially or let auto-placement work |
| Trying to declare track sizes on a subgrid | `grid-template-rows: subgrid 100px 1fr` is invalid syntax | Subgrid inherits tracks; cannot declare sizes |
| Assuming subgrid works without `display: grid` | Element must be a grid container (`display: grid`) to use subgrid | Declare `display: grid` on the subgrid item |
| Overwriting parent column tracks accidentally | Declaring `grid-template-columns: subgrid` when only row subgrid was needed | Only apply `subgrid` to the specific axis you want to inherit |
| Grandchild not participating | Missing `display: grid; grid-template-*: subgrid;` on the intermediary parent | Ensure intermediate parent declares subgrid |

---

## 8 Performance, Accessibility & Browser Support

### Rendering Engine Performance

- Subgrid layout involves a **two-way constraint solving pass**:
  1. Children in the subgrid report their intrinsic sizing requirements upward to the parent grid tracks.
  2. The parent grid resolves the maximum track sizes and distributes them downward to the subgrids.
- In Blink, Gecko, and WebKit, this calculation is hardware-optimized and resolves in linear time.
- Highly performant for production design systems; however, avoid creating hundreds of deeply nested subgrids inside high-frequency scroll animations.

### Accessibility Standards

- Subgrid allows complex, visually aligned layouts without forcing authors to dismantle semantic HTML trees.
- Without Subgrid, developers often resorted to "flattening" HTML into a single unsemantic `<div>` list so all elements could share a root grid.
- With Subgrid, you maintain clean, accessible semantic HTML (e.g. `<article class="card"><h3>Title</h3><p>Text</p><button>Buy</button></article>`) while achieving perfect visual track alignment across siblings!

---

## 9 Comparison / When to Use What

| Layout Approach | HTML Structure | Multi-Card Title Alignment? | Button Alignment? | Subgrid Required? |
| :--- | :--- | :--- | :--- | :--- |
| **Traditional Flexbox** | Nested Semantic Cards | No (Jagged heights) | Yes (via `margin-top: auto`) | No |
| **Flat CSS Grid** | Flattened Non-Semantic Tags | Yes (Shared tracks) | Yes (Shared tracks) | No (Destroys DOM semantics!) |
| **CSS Subgrid** | **Nested Semantic Cards** | **Yes (Synchronized tracks)** | **Yes (Synchronized tracks)** | **Yes (Best Architecture)** |

---

## 10 Interview Questions

<details>
<summary><strong>1. What exact problem does CSS Subgrid solve that CSS Grid Level 1 could not?</strong></summary>

In CSS Grid Level 1, grid tracks only apply to direct children. Grandchildren cannot participate in the parent grid's tracks. This made it impossible to align elements inside sibling cards (such as titles, images, and buttons) to the same horizontal lines while keeping the cards wrapped in semantic `<article>` tags. Authors had to either accept misaligned, jagged cards or flatten their HTML into an inaccessible markup soup. Subgrid allows nested items to inherit parent tracks directly.
</details>

<details>
<summary><strong>2. Can an element be a subgrid along only one axis?</strong></summary>

Yes. Subgrid is axis-independent. An element can inherit parent row tracks (`grid-template-rows: subgrid;`) while defining completely independent custom column tracks (`grid-template-columns: 1fr 1fr;`), or vice versa.
</details>

<details>
<summary><strong>3. What happens if you declare <code>grid-template-rows: subgrid</code> without declaring <code>grid-row: span N</code>?</strong></summary>

The subgrid item defaults to spanning a single track (`span 1`). Consequently, the subgrid only has 1 parent track available to it. All children placed inside the subgrid will be forced into that single cell, either overlapping each other or creating unexpected implicit tracks.
</details>

<details>
<summary><strong>4. How does track sizing work when subgrid children have differing content heights?</strong></summary>

Sizing is bi-directional. Subgrid children contribute their intrinsic sizes to the parent grid's tracks. The parent grid track expands to fit the largest child across all participating subgrids, ensuring that every sibling card shares the exact same expanded row height.
</details>

<details>
<summary><strong>5. Can a subgrid override the parent grid's <code>gap</code>?</strong></summary>

Yes. By default, a subgrid inherits the parent's `gap` on that axis. However, the subgrid can declare its own `row-gap` or `column-gap` (e.g. `row-gap: 8px;`), which overrides the spacing between its internal tracks without affecting the parent grid.
</details>

<details>
<summary><strong>6. What is the browser support baseline for CSS Subgrid?</strong></summary>

CSS Subgrid is part of **Web Platform Baseline 2023** (Widely Available). It is supported in Chrome 117+ (Sept 2023), Safari 16.0+ (Sept 2022), Firefox 71+ (Dec 2019), and Edge 117+.
</details>

<details>
<summary><strong>7. How do named lines behave inside a subgrid?</strong></summary>

A subgrid inherits all named lines from the parent tracks it spans. Furthermore, a subgrid can define its own local named lines inside square brackets within the subgrid declaration: `grid-template-columns: subgrid [card-start] [card-end];`.
</details>

<details>
<summary><strong>8. How should you progressively enhance for subgrid in production?</strong></summary>

By using the `@supports (grid-template-rows: subgrid)` feature query. Provide a standard Flexbox layout with `margin-top: auto` as the baseline fallback, and enhance to Subgrid inside the `@supports` block.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic 3-Row Card Subgrid
- **Task**: Configure a card component to span 3 parent row tracks and inherit them via `subgrid`.

<details>
<summary>View Solution</summary>

```css
.card {
  grid-row: span 3;
  display: grid;
  grid-template-rows: subgrid;
}
```
</details>

### Exercise 2 (Easy): Overriding Subgrid Gap
- **Task**: Inherit parent row tracks in a subgrid card, but set the internal row gap to 8px instead of parent's 24px.

<details>
<summary>View Solution</summary>

```css
.card {
  grid-row: span 3;
  display: grid;
  grid-template-rows: subgrid;
  row-gap: 8px; /* Local gap override */
}
```
</details>

### Exercise 3 (Medium): Synchronized 3-Card Feature Deck
- **Task**: Build a 3-column deck of feature cards where titles, descriptions, and buttons across all cards align horizontally.

<details>
<summary>View Solution</summary>

```css
.feature-deck {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 24px;
}

.feature-card {
  grid-row: span 3; /* Title, Description, Button */
  display: grid;
  grid-template-rows: subgrid;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  padding: 20px;
  border-radius: 8px;
}
```
</details>

### Exercise 4 (Medium): Aligned Form Fieldsets with Column Subgrid
- **Task**: Create a form with multiple fieldsets where labels and inputs share identical column widths via `grid-template-columns: subgrid`.

<details>
<summary>View Solution</summary>

```css
.form-container {
  display: grid;
  grid-template-columns: 140px 1fr;
  gap: 16px;
  max-width: 500px;
}

.form-row {
  grid-column: span 2;
  display: grid;
  grid-template-columns: subgrid;
  align-items: center;
}
```
</details>

### Exercise 5 (Hard): Dual-Axis Subgrid Card
- **Task**: Create an internal grid component that spans 2 columns and 3 rows, inheriting both axes as subgrids.

<details>
<summary>View Solution</summary>

```css
.dual-subgrid-card {
  grid-column: span 2;
  grid-row: span 3;
  display: grid;
  grid-template-columns: subgrid;
  grid-template-rows: subgrid;
}
```
</details>

### Exercise 6 (Hard): Progressive Enhancement Subgrid Fallback
- **Task**: Style a product card using flexbox by default and progressively enhance with subgrid when supported.

<details>
<summary>View Solution</summary>

```css
/* Fallback: Flexbox */
.product-card {
  display: flex;
  flex-direction: column;
  padding: 16px;
  border: 1px solid #e2e8f0;
}

.product-cta {
  margin-top: auto;
}

/* Progressive Enhancement */
@supports (grid-template-rows: subgrid) {
  .product-deck {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
    gap: 20px;
  }

  .product-card {
    grid-row: span 4;
    display: grid;
    grid-template-rows: subgrid;
  }

  .product-cta {
    margin-top: 0;
  }
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Subgrid Essentials */
.parent {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 24px;
}

.child-subgrid {
  grid-row: span 3;             /* REQUIRED: span count */
  display: grid;                /* REQUIRED: grid container */
  grid-template-rows: subgrid;  /* Adopt parent row tracks */
  row-gap: 8px;                 /* Optional local gap */
}
```

---

## 13 Further Reading

- [MDN Web Docs: CSS Subgrid](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_grid_layout/Subgrid)
- [W3C CSS Grid Layout Module Level 2: Subgrids](https://www.w3.org/TR/css-grid-2/#subgrids)
- [web.dev: Learn CSS Subgrid](https://web.dev/css-subgrid/)
- [Smashing Magazine: Digging Into The Display Property: Subgrid](https://www.smashingmagazine.com/2022/09/subgrid-css-grid-level-2/)

---

[Previous: 07-alignment-in-grid.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/07-alignment-in-grid.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/README.md) | [Next: 09-grid-vs-flexbox.md](file:///e:/Learning/engineering-notes/01-languages/css/04-grid/09-grid-vs-flexbox.md)
