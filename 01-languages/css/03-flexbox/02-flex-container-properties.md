# Flex Container Properties

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `03-flexbox/01-flexbox-mental-model.md` |
| **Estimated Reading Time** | 20 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2015) |
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

In Flexbox, the parent element holds the primary architectural authority over its children. When you set `display: flex`, the container properties determine:
- **Direction**: Whether items flow in a horizontal row or a vertical column.
- **Wrapping**: Whether items stay compressed on a single line or wrap onto new lines when space runs out.
- **Flow Shorthand**: Combining direction and wrapping in a single concise declaration.
- **Internationalization**: How the container responds when translated into Right-to-Left (RTL) or vertical writing modes.

Mastering flex container properties allows you to configure layout behavior at the macro level before fine-tuning individual items.

---

## 2 Core Concepts

### 1. `flex-direction`

The `flex-direction` property defines the orientation of the **Main Axis**:

- **`row` (Default)**: Main axis runs in the inline direction (left-to-right in English LTR, right-to-left in Arabic RTL). Cross axis runs vertically downward.
- **`row-reverse`**: Main axis runs in opposite inline direction (right-to-left in LTR). Items start on the right and advance toward the left.
- **`column`**: Main axis runs in the block direction (top-to-bottom). Cross axis runs horizontally.
- **`column-reverse`**: Main axis runs bottom-to-top. Items start at the bottom and advance upward.

### 2. `flex-wrap`

By default, Flexbox tries to squeeze every item onto a single line:

- **`nowrap` (Default)**: All items are forced onto a single line. If items exceed the container's width, they shrink according to `flex-shrink` or overflow the container boundaries.
- **`wrap`**: Items break onto multiple flex lines along the cross axis when they exceed container main-axis dimensions.
- **`wrap-reverse`**: Items wrap onto multiple lines in reverse cross-axis direction (e.g. In a row layout, new lines are added *above* rather than below).

### 3. `flex-flow` Shorthand

The `flex-flow` shorthand combines `flex-direction` and `flex-wrap` into one declaration:

```css
/* Syntax: flex-flow: <'flex-direction'> || <'flex-wrap'> */
.container {
  flex-flow: row wrap;
}
```

---

## 3 How It Works Internally

When the browser evaluates container properties, it dynamically binds layout calculations to the active writing mode and text direction:

```
+-------------------------------------------------------------+
| flex-direction: row; direction: ltr                         |
| [1] ───► [2] ───► [3]                                       |
+-------------------------------------------------------------+
| flex-direction: row; direction: rtl (e.g. Arabic)           |
|                                       [3] ◄─── [2] ◄─── [1] |
+-------------------------------------------------------------+
| flex-direction: row-reverse; direction: ltr                 |
|                                       [3] ◄─── [2] ◄─── [1] |
+-------------------------------------------------------------+
| flex-direction: column                                      |
| [1]                                                         |
|  │                                                          |
|  ▼                                                          |
| [2]                                                         |
|  │                                                          |
|  ▼                                                          |
| [3]                                                         |
+-------------------------------------------------------------+
```

### The Cross-Axis Line Generation Algorithm

When `flex-wrap: wrap` is enabled:
1. The engine calculates the **hypothetical main size** of each item.
2. It packs items sequentially onto the current flex line.
3. If an item cannot fit on the current line without overflowing, the browser breaks to a **new flex line**.
4. Each flex line operates independently: the height (in row mode) of a line is determined solely by the tallest item in *that specific line*, not the whole container!

---

## 4 Syntax & Usage

```css
.container {
  display: flex;

  /* Explicit Directions */
  flex-direction: row | row-reverse | column | column-reverse;

  /* Wrapping Modes */
  flex-wrap: nowrap | wrap | wrap-reverse;

  /* Concise Shorthand */
  flex-flow: row wrap;
  flex-flow: column nowrap;
}
```

> [!NOTE]
> **Baseline Status: Widely Available**
> `flex-direction`, `flex-wrap`, and `flex-flow` are Baseline Widely Available since 2015 across all browser engines.

---

## 5 Practical Examples

### Example 1: Responsive Navigation Menu (Row on Desktop, Column on Mobile)

```css
.site-nav {
  display: flex;
  flex-direction: column; /* Default: mobile-first vertical stack */
  gap: 16px;
  padding: 16px;
  background-color: #0f172a;
}

@media (min-width: 768px) {
  .site-nav {
    flex-direction: row; /* Switch to horizontal bar on desktop */
    align-items: center;
  }
}
```

### Example 2: Wrapping Tag / Filter Chip Cloud

```css
.chip-container {
  display: flex;
  flex-wrap: wrap; /* Allows chips to wrap smoothly onto multiple lines */
  gap: 8px;
  max-width: 400px;
  padding: 16px;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.chip {
  padding: 6px 12px;
  background-color: #f1f5f9;
  border-radius: 9999px;
  font-size: 0.875rem;
  color: #334155;
  white-space: nowrap;
}
```

### Example 3: Edge Case — Inverted Stacking with `wrap-reverse`

In a chat interface or terminal feed, you may want new lines of status chips to populate from the bottom upward:

```css
.terminal-badges {
  display: flex;
  flex-flow: row wrap-reverse; /* Lines wrap UPWARDS */
  gap: 8px;
  padding: 12px;
  background-color: #1e1e1e;
  border-radius: 6px;
}
```

### Example 4: Failure Case — `nowrap` Causing Container Blowout

```css
/* FAILURE PATTERN */
.product-showcase {
  display: flex;
  /* BUG: Default is flex-wrap: nowrap. Fixed-width cards will force
     the container to blow out horizontally and create page scrollbars! */
  gap: 16px;
}

.product-card {
  flex: 0 0 300px; /* 5 cards = 1500px, overflows on mobile! */
}

/* FIXED PATTERN */
.product-showcase-fixed {
  display: flex;
  flex-wrap: wrap; /* Items wrap when screen is smaller than card widths */
  gap: 16px;
}

.product-card-fixed {
  flex: 1 1 280px; /* Fluid cards that wrap at 280px */
}
```

### Example 5: Clean Master-Detail Sidebar Layout

```css
.master-detail-layout {
  display: flex;
  flex-flow: row nowrap;
  min-height: 100vh;
}

.sidebar {
  flex: 0 0 260px;
  background-color: #f8fafc;
  border-right: 1px solid #e2e8f0;
}

.main-workspace {
  flex: 1 1 0%;
  display: flex;
  flex-direction: column;
  padding: 24px;
}
```

---

## 6 Best Practices

1. **Use `flex-flow` shorthand for cleaner stylesheets**: Instead of specifying `flex-direction: row; flex-wrap: wrap;`, write `flex-flow: row wrap;`.
2. **Combine `flex-wrap: wrap` with `gap` for responsive chip groups**: Avoids tedious horizontal and vertical margin hacks.
3. **Be cautious with `*-reverse` values**: They change visual order without changing the DOM or keyboard tab sequence.
4. **Use `flex-direction: column` for card layouts**: Pushing a footer button to the bottom using `margin-top: auto` requires column orientation.
5. **Set `flex-direction` mobile-first**: Start with `column` for small screens, and switch to `row` inside `@media (min-width: ...)` queries.
6. **Remember that flex lines are independent**: Items on line 2 will not align with items on line 1 unless explicit widths or CSS Grid are used.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Fix |
| :--- | :--- | :--- |
| Forgetting `flex-wrap: wrap` on multi-item rows | Items refuse to wrap and overflow container on mobile | Add `flex-wrap: wrap` |
| Using `column-reverse` to put newest messages at bottom | Tab navigation moves from bottom to top, breaking accessibility | Reorder DOM elements in HTML |
| Expecting grid-like cell alignment with `flex-wrap: wrap` | Each flex line calculates sizing independently | Use CSS Grid if multi-line grid alignment is required |
| Confusing `flex-direction: column` axis rules | `justify-content` now aligns vertically, `align-items` aligns horizontally | Remember alignment properties follow the main axis |
| Overwriting `flex-wrap` by declaring `flex-flow` improperly | `flex-flow: row` resets `flex-wrap` back to `nowrap` | Specify both values: `flex-flow: row wrap` |
| Setting `height: 100%` on child of `column` flex container | Percentage height needs explicit parent height or `flex: 1` | Use `flex-grow: 1` instead of `height: 100%` |

---

## 8 Performance, Accessibility & Browser Support

### Rendering Engine Considerations

- Changing `flex-direction` between `row` and `column` at responsive breakpoints triggers a complete reflow and layout recomputation for all children. This is normal and expected for media queries, but avoid animating `flex-direction` with CSS transitions (which is unsupported anyway).
- `flex-wrap: wrap` requires the engine to calculate line breaks iteratively, which has slightly higher computational cost than `nowrap`, but remains imperceptible in standard component hierarchies.

### Accessibility Standards (WCAG 2.1 Success Criterion 1.3.2)

- Using `flex-direction: row-reverse` or `column-reverse` causes a disconnect between visual presentation and DOM reading order.
- Screen readers will announce the elements in original DOM order (1, 2, 3), while visual users see (3, 2, 1).
- Keyboard tab order will traverse backwards visually, which disorients users with motor or cognitive impairments. Only use reverse directions for purely aesthetic, non-interactive decoration.

---

## 9 Comparison / When to Use What

| Property Value | Main Axis Direction | Wrap Behavior | Typical Use Case |
| :--- | :--- | :--- | :--- |
| `row nowrap` (Default) | Horizontal LTR | Single line, shrinks | Headers, toolbars, navbars |
| `row wrap` | Horizontal LTR | Wraps onto new lines below | Tag clouds, photo galleries, form fields |
| `row wrap-reverse` | Horizontal LTR | Wraps onto new lines above | Reverse logs, bottom-up badges |
| `column nowrap` | Vertical Top-to-Bottom | Single vertical stack | Page layouts, cards, vertical sidebars |
| `column wrap` | Vertical Top-to-Bottom | Spills into new columns to right | Multi-column article index (rare) |

---

## 10 Interview Questions

<details>
<summary><strong>1. What happens to the alignment properties when <code>flex-direction</code> changes from <code>row</code> to <code>column</code>?</strong></summary>

The alignment properties follow the axes rather than physical screen directions. When switching to `column`, the Main Axis becomes vertical, meaning `justify-content` now controls vertical spacing. The Cross Axis becomes horizontal, meaning `align-items` and `align-self` now control horizontal alignment.
</details>

<details>
<summary><strong>2. What is the default value of <code>flex-wrap</code>, and why can it cause responsive bugs?</strong></summary>

The default value is `nowrap`. When multiple items with fixed or intrinsic widths sit inside a flex container on a small screen, they will shrink down to their min-content widths and then overflow the container, causing horizontal scrollbars on mobile devices if wrapping is not explicitly enabled.
</details>

<details>
<summary><strong>3. How does <code>flex-flow</code> work, and what are its constituent properties?</strong></summary>

`flex-flow` is a shorthand property that accepts `flex-direction` and `flex-wrap` in either order (e.g., `flex-flow: row wrap;` or `flex-flow: column nowrap;`). If one value is omitted, it resets that omitted property to its initial specification default.
</details>

<details>
<summary><strong>4. Why does each flex line in a multi-line flex container calculate its cross-size independently?</strong></summary>

Because Flexbox is a one-dimensional layout model. Unlike CSS Grid (which enforces a shared two-dimensional track grid across all rows), each wrapped flex line only knows about the items within that specific line. The cross-size (height in row layout) of line 2 is determined by line 2's tallest item, completely independent of line 1.
</details>

<details>
<summary><strong>5. Explain the accessibility risk of using <code>flex-direction: row-reverse</code>.</strong></summary>

`row-reverse` visually mirrors the items on the screen, but preserves their original DOM source order. Sighted keyboard users pressing `Tab` will experience focus moving in the opposite direction of visual reading, causing disorientation and violating WCAG 1.3.2 (Meaningful Sequence).
</details>

<details>
<summary><strong>6. How does <code>flex-direction: row</code> interact with an Arabic or Hebrew (RTL) document?</strong></summary>

In an RTL document (`<html dir="rtl">`), the inline start edge is on the physical right. Therefore, `flex-direction: row` automatically places the first child on the right and flows leftward, matching the natural reading order without requiring any custom CSS overrides.
</details>

<details>
<summary><strong>7. Can a flex container wrap items vertically into multiple columns?</strong></summary>

Yes, using `flex-direction: column; flex-wrap: wrap;`. However, for vertical wrapping to occur, the container **must have an explicit constrained height** (or `max-height`), otherwise the container will just expand downward indefinitely without ever triggering a line break.
</details>

<details>
<summary><strong>8. What is the difference between <code>flex-wrap: wrap</code> and <code>flex-wrap: wrap-reverse</code>?</strong></summary>

Under `wrap`, new flex lines are added along the normal cross-axis direction (downward in horizontal row layout). Under `wrap-reverse`, new flex lines are added in the opposite cross-axis direction (upward in horizontal row layout).
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Responsive Direction Switch
- **Task**: Stack two buttons vertically on screens under 600px, and place them side-by-side on screens 600px and wider.

<details>
<summary>View Solution</summary>

```css
.button-group {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

@media (min-width: 600px) {
  .button-group {
    flex-direction: row;
  }
}
```
</details>

### Exercise 2 (Easy): Wrapping Badge List
- **Task**: Create a badge container that neatly wraps badges into multiple rows with 8px horizontal and vertical spacing.

<details>
<summary>View Solution</summary>

```css
.badge-list {
  display: flex;
  flex-flow: row wrap;
  gap: 8px;
  padding: 12px;
}
```
</details>

### Exercise 3 (Medium): Full-Height Vertical Column Layout
- **Task**: Create a sidebar container with a top header, middle scrollable nav list, and bottom user profile card.

<details>
<summary>View Solution</summary>

```css
.sidebar-container {
  display: flex;
  flex-direction: column;
  height: 100vh;
  width: 250px;
  background-color: #0f172a;
  color: #ffffff;
}

.sidebar-header {
  padding: 16px;
}

.sidebar-nav {
  flex-grow: 1;
  overflow-y: auto;
  padding: 16px;
}

.sidebar-footer {
  padding: 16px;
  border-top: 1px solid #1e293b;
}
```
</details>

### Exercise 4 (Medium): Vertical Wrapping Masonry List
- **Task**: Wrap a list of bookmarks into multiple vertical columns inside a fixed 300px-tall container.

<details>
<summary>View Solution</summary>

```css
.bookmark-columns {
  display: flex;
  flex-flow: column wrap;
  height: 300px;
  gap: 12px;
  overflow-x: auto;
  padding: 16px;
}

.bookmark-item {
  width: 180px;
  padding: 8px 12px;
  background: #f1f5f9;
  border-radius: 4px;
}
```
</details>

### Exercise 5 (Hard): Dynamic Multi-Row Card Grid with Flexbox
- **Task**: Build a 3-column wrapping card grid where each card takes up 1/3 of the row minus gaps, and wraps smoothly.

<details>
<summary>View Solution</summary>

```css
.card-grid {
  display: flex;
  flex-flow: row wrap;
  gap: 16px;
}

.card-item {
  /* 3 items per line: calc(100% / 3 - gap adjustment) */
  flex: 0 0 calc((100% - 32px) / 3);
  box-sizing: border-box;
  background: #ffffff;
  padding: 20px;
  border: 1px solid #e2e8f0;
}

@media (max-width: 768px) {
  .card-item {
    flex: 0 0 100%;
  }
}
```
</details>

### Exercise 6 (Hard): Accessible Reverse Layout Fix
- **Task**: Given a mockup requiring an image on the right and text on the left in desktop view, author accessible CSS without using `row-reverse`.

<details>
<summary>View Solution</summary>

```html
<!-- DOM order matches logical reading order: Text first, then Image -->
<section class="feature-section">
  <div class="feature-text">...</div>
  <div class="feature-image">...</div>
</section>
```

```css
.feature-section {
  display: flex;
  flex-direction: column;
  gap: 24px;
}

@media (min-width: 768px) {
  .feature-section {
    flex-direction: row; /* Normal row preserves DOM order */
    align-items: center;
  }
  
  .feature-text {
    flex: 1;
  }
  
  .feature-image {
    flex: 1;
  }
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Container Property Quick Reference */
flex-direction: row;             /* Default. Horizontal LTR */
flex-direction: column;          /* Vertical Top-to-Bottom */
flex-direction: row-reverse;     /* Horizontal RTL visually */
flex-direction: column-reverse;  /* Vertical Bottom-to-Top visually */

flex-wrap: nowrap;               /* Default. Squeezes onto 1 line */
flex-wrap: wrap;                 /* Wraps onto multiple lines */
flex-wrap: wrap-reverse;         /* Wraps lines upwards */

flex-flow: row wrap;             /* Shorthand */
```

---

## 13 Further Reading

- [MDN Web Docs: flex-direction](https://developer.mozilla.org/en-US/docs/Web/CSS/flex-direction)
- [MDN Web Docs: flex-wrap](https://developer.mozilla.org/en-US/docs/Web/CSS/flex-wrap)
- [MDN Web Docs: flex-flow](https://developer.mozilla.org/en-US/docs/Web/CSS/flex-flow)
- [W3C CSS Flexible Box Layout Module Level 1: Flex Container Properties](https://www.w3.org/TR/css-flexbox-1/#flex-container-properties)

---

[Previous: 01-flexbox-mental-model.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/01-flexbox-mental-model.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md) | [Next: 03-flex-item-properties.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/03-flex-item-properties.md)
