# Cheatsheet 05: CSS Grid

| Spec Level | CSS Grid Layout Module Level 2 | Baseline Status | Widely Available / Subgrid Baseline 2023 |
|---|---|---|---|
| Browsers Tested | Chrome 120+, Firefox 125+, Safari 17.4+ | Primary Usage | 2D matrix layouts, magazine templates, complex responsive components |

---

## 1. Grid Container Syntax Reference

```css
.grid-container {
  display: grid; /* or inline-grid */

  /* Explicit Track Definition */
  grid-template-columns: 250px 1fr 2fr;
  grid-template-rows: auto 1fr 60px;

  /* Track Gaps */
  gap: 1.5rem; /* shorthand: row-gap column-gap */

  /* Named Grid Lines */
  grid-template-columns: [full-start] 1fr [content-start] 800px [content-end] 1fr [full-end];

  /* Implicit Auto Tracks (Generated when items overflow explicit grid) */
  grid-auto-flow: row;              /* default: fill rows sequentially */
  grid-auto-flow: column;           /* fill columns sequentially */
  grid-auto-flow: row dense;        /* backtrack and backfill gaps with smaller items */
  grid-auto-rows: minmax(100px, auto);
  grid-auto-columns: 200px;

  /* Whole-Grid Content Alignment (When total grid size < container size) */
  justify-content: start | end | center | space-between | space-around | space-evenly;
  align-content: start | end | center | space-between | space-around | space-evenly;

  /* Items Alignment within their Grid Cells */
  justify-items: stretch | start | end | center;
  align-items: stretch | start | end | center;
  place-items: center; /* shorthand: align-items justify-items */
}
```

---

## 2. Track Functions Cheat Sheet

| Function | Syntax Example | Meaning |
|---|---|---|
| `fr` (Fraction) | `1fr 2fr` | Distributes remaining free space in a 1:2 ratio |
| `minmax(min, max)` | `minmax(200px, 1fr)` | Track never drops below 200px, expands with free space |
| `repeat(count, val)`| `repeat(12, 1fr)` | Repeats 1fr track 12 times |
| `auto-fill` | `repeat(auto-fill, 200px)` | Creates as many 200px tracks as fit, even if empty |
| `auto-fit` | `repeat(auto-fit, 200px)` | Creates tracks and collapses empty tracks to 0px |
| `fit-content(limit)`| `fit-content(300px)` | Uses content size up to a hard maximum boundary of 300px |

---

## 3. The Responsive RAM Pattern (Repeat, Auto, Minmax)

A complete responsive auto-wrapping grid that requires **zero media queries** and handles viewports from 320px to 4K displays:

```css
.responsive-cards {
  display: grid;
  /* min(100%, 280px) prevents overflow on screens narrower than 280px */
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr));
  gap: 1.5rem;
}
```

---

## 4. Grid Template Areas

```css
.app-layout {
  display: grid;
  grid-template-columns: 260px 1fr;
  grid-template-rows: 70px 1fr 50px;
  grid-template-areas:
    "sidebar header"
    "sidebar main"
    "sidebar footer";
  min-height: 100dvh;
}

/* Item Assignment */
.header  { grid-area: header; }
.sidebar { grid-area: sidebar; }
.main    { grid-area: main; }
.footer  { grid-area: footer; }
```

---

## 5. Grid Item Coordinate Placement

```css
.grid-item {
  /* Line Numbers (1-based index) */
  grid-column-start: 1;
  grid-column-end: 3;
  /* Shorthand: start / end */
  grid-column: 1 / 3;

  /* Span Syntax */
  grid-column: span 2;        /* span 2 columns from current auto position */
  grid-row: 2 / span 3;       /* start at row line 2, span 3 rows down */

  /* Full Width Breakout */
  grid-column: 1 / -1;        /* spans from first explicit line to last explicit line */

  /* Grid Area Shorthand (row-start / col-start / row-end / col-end) */
  grid-area: 1 / 2 / 3 / 4;

  /* Self Alignment */
  justify-self: center;
  align-self: center;
  place-self: center; /* align-self justify-self */
}
```

---

## 6. CSS Subgrid (Baseline 2023)

Allows a nested child grid to adopt and inherit the track dimensions of its parent grid, aligning card elements (headers, descriptions, buttons) across cards.

```css
.card-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 1.5rem;
}

.card {
  display: grid;
  grid-row: span 3; /* Spans 3 tracks in parent row */
  grid-template-rows: subgrid; /* Header, body, footer align across cards! */
}
```

---

[← Previous: Cheatsheet 04 - Flexbox](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/04-flexbox.md) | [Next: Cheatsheet 06 - Responsive and Media Queries →](file:///e:/Learning/engineering-notes/01-languages/css/16-cheatsheets/06-responsive-and-media-queries.md)
