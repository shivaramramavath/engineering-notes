# Flexbox Wrapping and Ordering

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | `03-flexbox/02-flex-container-properties.md`, `03-flexbox/03-flex-item-properties.md` |
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

In responsive web design, viewports range from $320\text{px}$ mobile screens to $4\text{K}$ desktop monitors. Forcing all components onto a rigid single line inevitably causes container overflow, truncated text, or compressed layouts.

Flexbox solves this through **multi-line wrapping** (`flex-wrap: wrap`) and **visual reordering** (`order`).

However, multi-line Flexbox behavior is frequently misunderstood:
- Unlike CSS Grid, Flexbox does **not** create a coordinated two-dimensional grid; each wrapped line is an independent one-dimensional world.
- The `order` property decouples visual presentation from the underlying HTML DOM tree, which can introduce severe accessibility violations for keyboard and screen-reader users if misused.

---

## 2 Core Concepts

### Multi-Line Flex Containers

When `flex-wrap: wrap` is declared, the flex container becomes a **multi-line flex container**:
- Items fill the current line until the next item's hypothetical main size exceeds the remaining main-axis space.
- The engine creates a **new flex line** and places the item at the start of that new line.
- **Cross-Axis Independence**: The height of row 1 depends solely on the tallest item in row 1. The height of row 2 depends solely on the tallest item in row 2. Items on line 2 will not automatically match heights with items on line 1 unless forced by CSS Grid.

### Visual Reordering with `order`

- All flex items default to `order: 0`.
- The browser groups items into "order buckets" sorted in ascending numerical order (`-1`, `0`, `1`, `2`, ...).
- Items sharing the same `order` number are rendered according to their original source order in HTML.
- Modifying `order` alters **only the visual paint order**. It does not affect:
  - Keyboard tab sequence.
  - Screen reader reading order.
  - Text selection order.

---

## 3 How It Works Internally

```
+-------------------------------------------------------------+
| Multi-Line Flex Container (flex-wrap: wrap)                 |
|                                                             |
| Line 1 (Height determined by tallest item in Line 1)        |
| +-------------------+  +-------------------+                |
| | Item 1            |  | Item 2 (Tallest)  |                |
| | (flex: 1 1 200px) |  |                   |                |
| +-------------------+  +-------------------+                |
|                                                             |
| Line 2 (Height determined by tallest item in Line 2)        |
| +---------------------------------------------------------+ |
| | Item 3 (flex: 1 1 200px - expands to fill whole line!)  | |
| +---------------------------------------------------------+ |
+-------------------------------------------------------------+
```

> **The Orphan Item Phenomenon**: In the diagram above, `Item 3` wrapped onto Line 2. Because `flex-grow: 1` is active, `Item 3` expands across the *entire width* of Line 2. This is the defining difference between multi-line Flexbox and CSS Grid! In CSS Grid, an item stays locked in its grid column; in Flexbox, wrapped items expand across their independent line.

---

## 4 Syntax & Usage

```css
/* Container Wrapping Modes */
.container {
  display: flex;
  flex-wrap: nowrap;       /* Single line (default) */
  flex-wrap: wrap;         /* Multi-line, wraps downward */
  flex-wrap: wrap-reverse; /* Multi-line, wraps upward */
}

/* Item Order */
.item-first {
  order: -1; /* Jumps to visual start */
}

.item-last {
  order: 99; /* Jumps to visual end */
}
```

---

## 5 Practical Examples

### Example 1: Fluid Responsive Form Row

```css
.form-row {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
}

.form-group-half {
  /* Takes 50% width on wide screens, but wraps at 240px */
  flex: 1 1 calc(50% - 8px);
  min-width: 240px;
}

.form-group-full {
  flex: 1 1 100%;
}
```

### Example 2: Reordering Sidebar and Content for Mobile

```css
.app-container {
  display: flex;
  flex-direction: column;
}

/* In DOM: <aside class="sidebar"> comes first, <main> comes second */
.sidebar {
  order: 2; /* Visually push sidebar below main content on mobile */
  padding: 16px;
  background-color: #f1f5f9;
}

.main-content {
  order: 1; /* Visually pull main content to the top */
  padding: 16px;
}

@media (min-width: 768px) {
  .app-container {
    flex-direction: row;
  }
  .sidebar { order: 0; flex: 0 0 240px; }
  .main-content { order: 0; flex: 1; }
}
```

### Example 3: Edge Case — Wrapping with Fixed vs Flexible Basis

```css
.gallery {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
}

/* Rigid Basis: last item does NOT stretch */
.gallery-item-rigid {
  flex: 0 0 200px; /* Always precisely 200px */
}

/* Flexible Basis: last orphaned item stretches to 100% width! */
.gallery-item-fluid {
  flex: 1 1 200px; /* If 1 item is left on the last row, it fills the row */
}
```

### Example 4: Failure Case — Broken Tab Sequence with `order`

```html
<!-- DANGEROUS PATTERN FOR ACCESSIBILITY -->
<form class="checkout-form">
  <button type="submit" class="submit-btn" style="order: 2;">Place Order</button>
  <input type="text" class="card-input" style="order: 1;" placeholder="Card Number" />
</form>
```
*Problem*: A keyboard user presses `Tab` into the form. Focus lands immediately on "Place Order" (DOM order), then jumps to "Card Number"!
*Correction*: Align the HTML source order with the visual sequence.

### Example 5: Inverted Chat Bubble Thread with `wrap-reverse`

```css
.chat-container {
  display: flex;
  flex-flow: column wrap-reverse;
  height: 400px;
  gap: 8px;
  padding: 16px;
  background-color: #0f172a;
}
```

---

## 6 Best Practices

1. **Decide if you want orphan items to stretch**: If wrapped items should remain a fixed size on the last line, use `flex: 0 0 [size]`; if they should fill remaining space, use `flex: 1 1 [size]`.
2. **Use CSS Grid instead of Flexbox for strict 2D column alignment**: If items in row 2 must align vertically with items in row 1, Grid is the right tool.
3. **Never use `order` to repair poor HTML markup**: Always structure the DOM in logical semantic order first.
4. **Combine `flex-wrap: wrap` with `min-width` on form inputs**: Allows inputs to sit side-by-side on wide screens and stack automatically on mobile.
5. **Use `gap` instead of margin resets on wrapped items**: Margins on wrapped flex items can leak into container edges or create double spacing.
6. **Limit `order` usage to non-interactive decorative elements**: Such as reordering hero artwork or decorative badges.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Root Cause | Solution |
| :--- | :--- | :--- |
| Last card in a grid stretches to fill entire line | `flex-grow: 1` expands to fill free space on that independent line | Use `flex: 0 0 [size]` or switch to CSS Grid |
| Items refuse to wrap onto new lines | Default `flex-wrap` is `nowrap` | Add `flex-wrap: wrap` |
| Keyboard focus jumps backwards across the screen | Manipulating `order` decouples visual order from DOM tab order | Match DOM structure to visual hierarchy |
| Rows have different heights in multi-line flex | Flex lines compute cross-size independently | Set explicit card heights or use CSS Grid |
| `align-content` ignored | Container has `flex-wrap: nowrap` (only 1 line) | `align-content` requires `flex-wrap: wrap` |
| Negative margin blowout on wrapped lines | Legacy float hacks cause horizontal scrolling | Use native `gap` |

---

## 8 Performance, Accessibility & Browser Support

### Layout Engine Cost

- Multi-line flexbox calculation is slightly more computationally intensive than single-line flexbox, as the engine must determine line breaks iteratively.
- For large data tables or thousands of elements, CSS Grid or virtualized lists are significantly more performant than a single massive wrapping flex container.

### Accessibility Violations (WCAG 1.3.2 Meaningful Sequence)

- **The WCAG Compliance Mandate**: When the visual reading sequence of a page differs from its programmatic DOM sequence, it fails WCAG 2.1 Success Criterion 1.3.2.
- Screen readers read the accessibility tree, which is generated directly from DOM hierarchy. Blind users will hear content in a completely different order than sighted users see. Keep `order` modifications strictly confined to non-critical aesthetic elements.

---

## 9 Comparison / When to Use What

| Requirement | Multi-Line Flexbox (`flex-wrap: wrap`) | CSS Grid (`display: grid`) |
| :--- | :--- | :--- |
| **Grid Column Alignment** | No (Lines are independent) | **Yes (Strict 2D columns)** |
| **Orphan Item Behavior** | Expands to fill available line width | Remains locked in its allocated grid cell |
| **Dynamic Content Widths** | Ideal (Tag clouds, variable buttons) | Less natural (requires auto-fit / minmax) |
| **Responsive Form Rows** | Simple, fluid | Requires template column queries |

---

## 10 Interview Questions

<details>
<summary><strong>1. Why does an orphaned item on the last line of a wrapped flex container stretch across the whole row?</strong></summary>

Because in Flexbox, every wrapped line acts as an independent one-dimensional flex container. When an item wraps to line 2, it is the sole occupant of line 2. If it has `flex-grow: 1`, it claims 100% of the positive free space on that line, expanding to full width. In CSS Grid, an item would remain constrained to its single column.
</details>

<details>
<summary><strong>2. How does the <code>order</code> property interact with keyboard tab navigation?</strong></summary>

The `order` property modifies only the visual rendering tree; it has zero effect on the DOM tree or keyboard focus sequence. When a keyboard user tabs through interactive elements, focus travels according to original HTML source order, resulting in jarring visual jumps if `order` differs from DOM sequence.
</details>

<details>
<summary><strong>3. What happens if multiple flex items share the exact same <code>order</code> value?</strong></summary>

Items with identical `order` integers are rendered in their natural source order as defined in the HTML document.
</details>

<details>
<summary><strong>4. What is the difference between <code>flex-wrap: wrap</code> and <code>flex-wrap: wrap-reverse</code>?</strong></summary>

In `flex-wrap: wrap`, new flex lines are added in the standard cross-axis direction (downward in horizontal row mode). In `wrap-reverse`, new flex lines are added in the opposite cross-axis direction (upward in horizontal row mode).
</details>

<details>
<summary><strong>5. When should you choose CSS Grid over multi-line Flexbox?</strong></summary>

Choose CSS Grid when you require strict two-dimensional alignment where columns and rows must align across multiple lines (e.g., photo galleries, dashboards, data tables). Choose multi-line Flexbox when items have varying intrinsic widths and should wrap naturally like words in a sentence (e.g., tag clouds, button bars, filter chips).
</details>

<details>
<summary><strong>6. What role does <code>align-content</code> play in a wrapped flex container?</strong></summary>

`align-content` distributes extra space along the cross axis across the multiple flex lines. For example, `align-content: flex-start` packs all wrapped rows tightly at the top rather than spreading them across the container height.
</details>

<details>
<summary><strong>7. How can you prevent the last card in a wrapped flex container from stretching?</strong></summary>

By setting `flex-grow: 0` (or `flex: 0 0 250px`). Without a growth factor, the orphaned card will retain its exact 250px basis rather than consuming the remaining line width.
</details>

<details>
<summary><strong>8. Does <code>order</code> affect CSS pseudo-elements like <code>::before</code> and <code>::after</code>?</strong></summary>

Yes. Generated pseudo-elements become flex items inside a flex container, and they can be targeted with `order` to position them before, between, or after real DOM children.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Natural Wrapping Tag Cloud
- **Task**: Create a container that neatly wraps tags into rows with 8px horizontal and vertical gaps.

<details>
<summary>View Solution</summary>

```css
.tag-cloud {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.tag {
  padding: 6px 12px;
  background-color: #e2e8f0;
  border-radius: 9999px;
  font-size: 0.875rem;
}
```
</details>

### Exercise 2 (Easy): Pin Banner to Visual Top with Order
- **Task**: Visually promote an announcement banner to the top of a page without moving its DOM node.

<details>
<summary>View Solution</summary>

```css
.site-wrapper {
  display: flex;
  flex-direction: column;
}

.announcement-banner {
  order: -1; /* Jumps to top */
  background-color: #fef08a;
  padding: 12px;
  text-align: center;
}
```
</details>

### Exercise 3 (Medium): Responsive Form Row with Clean Breakpoints
- **Task**: Create two form inputs that sit side-by-side at 50% width on desktop, but wrap to 100% width on mobile screens under 500px.

<details>
<summary>View Solution</summary>

```css
.form-row {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
}

.form-input-half {
  flex: 1 1 calc(50% - 8px);
  min-width: 220px; /* Triggers wrap when container is below ~460px */
}
```
</details>

### Exercise 4 (Medium): Fixed-Width Multi-Line Card Grid
- **Task**: Construct a flex container where cards wrap onto new lines but never stretch to fill the last row.

<details>
<summary>View Solution</summary>

```css
.card-container {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
}

.card {
  flex: 0 0 280px; /* Inflexible basis: never expands or contracts */
  height: 200px;
  background: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
}
```
</details>

### Exercise 5 (Hard): Dynamic Multi-Column Form with Action Buttons at Bottom
- **Task**: Build a form where input fields wrap in pairs, and the button row always wraps to a full-width line at the bottom.

<details>
<summary>View Solution</summary>

```css
.order-form {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  max-width: 600px;
}

.form-field {
  flex: 1 1 calc(50% - 8px);
  min-width: 250px;
}

.form-actions {
  flex: 1 1 100%; /* Guarantees its own full line */
  display: flex;
  justify-content: flex-end;
  gap: 12px;
  margin-top: 16px;
}
```
</details>

### Exercise 6 (Hard): Accessible Responsive Feature Section
- **Task**: Reorder an image and text block on mobile versus desktop while ensuring screen readers and keyboard users encounter the heading first.

<details>
<summary>View Solution</summary>

```html
<!-- DOM Order: Text heading first, then image -->
<article class="feature-card">
  <div class="feature-text">
    <h2>Supercharged Performance</h2>
    <p>Details about the feature...</p>
  </div>
  <div class="feature-media">
    <img src="chart.png" alt="Performance graph showing 2x speedup" />
  </div>
</article>
```

```css
.feature-card {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

@media (min-width: 768px) {
  .feature-card {
    flex-direction: row;
    align-items: center;
  }
  
  .feature-text { flex: 1; }
  .feature-media { flex: 1; }
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Container Wrapping */
flex-wrap: nowrap;         /* Default. Single line */
flex-wrap: wrap;           /* Multi-line */
flex-wrap: wrap-reverse;   /* Multi-line in reverse cross direction */

/* Item Visual Ordering */
order: 0;                  /* Default integer */
order: -1;                 /* Moves toward start */
order: 1;                  /* Moves toward end */

/* Key Takeaways */
/* 1. Flex lines calculate dimensions independently. */
/* 2. Orphaned items with flex-grow: 1 will stretch across the entire last row. */
/* 3. 'order' alters visual rendering ONLY; keyboard tab sequence is unaffected. */
```

---

## 13 Further Reading

- [MDN Web Docs: Wrapping flex items](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_flexible_box_layout/Mastering_wrapping_of_flex_items)
- [MDN Web Docs: Ordering flex items](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_flexible_box_layout/Ordering_flex_items)
- [W3C CSS Flexible Box Layout Module Level 1: Flex Lines](https://www.w3.org/TR/css-flexbox-1/#flex-lines)
- [web.dev: Responsive layout with Flexbox](https://web.dev/)

---

[Previous: 04-alignment-and-gap.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/04-alignment-and-gap.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md) | [Next: 06-flexbox-sizing-algorithm-deep-dive.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/06-flexbox-sizing-algorithm-deep-dive.md)
