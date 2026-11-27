# Text Overflow & Multi-Line Clamping

| Metadata | Details |
| :--- | :--- |
| **Topic** | Single-Line Ellipsis, Multi-Line Line Clamping (`-webkit-line-clamp` & standard `line-clamp`), Flex/Grid Truncation Defense, Accessible Disclosures |
| **Spec Level** | CSS Overflow Module Level 3 / CSS Overflow Module Level 4 |
| **Baseline Status** | Widely Available (Single-line `text-overflow: ellipsis` Baseline 2015; `-webkit-line-clamp` Baseline 2020) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | Chromium LayoutNG, Safari WebKit, Firefox Gecko, Screen Readers (NVDA/VoiceOver) |
| **Module** | [06-typography-and-text](README.md) |

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

In responsive UI architecture, text content frequently varies in length. A product title might be 12 characters ("Wool Sweater") or 140 characters ("Ultra-Fine Merino Wool Men's Crewneck Sweater - Limited Edition Midnight Navy Blue"). If cards or table rows have strict layout constraints, overflowing text will either break out of its container, blow out the table columns, or cause layout shifts.

CSS provides two primary mechanisms for handling overflowing text:
1. **Single-Line Truncation**: Enforcing a single line and clipping excess characters with an ellipsis (`...`) via `text-overflow: ellipsis`.
2. **Multi-Line Clamping**: Permitting text to wrap up to a fixed number of lines (e.g., 2 or 3 lines), and clipping with an ellipsis on the final line via `-webkit-line-clamp` (and standard `line-clamp`).

While straightforward in concept, implementing truncation in real-world layouts involves tricky layout engine mechanics:
- Inside flexbox and CSS grid layouts, elements default to `min-width: auto`, preventing truncation from taking effect unless explicitly overridden with `min-width: 0`.
- Truncating text visually can destroy screen reader accessibility if the full string is not preserved in the accessibility tree.

---

## 2. Core Concepts

### Single-Line Truncation Trinity
For `text-overflow: ellipsis` to function on a single line, **three mandatory CSS properties** must be declared together:
```css
.single-line-truncate {
  white-space: nowrap;        /* 1. Prevent line wrapping */
  overflow: hidden;           /* 2. Clip overflowing content */
  text-overflow: ellipsis;   /* 3. Render '...' at boundary */
}
```
If any of these three properties is missing, single-line truncation fails completely.

### The Flex/Grid `min-width: 0` Defense
By CSS specification, flex items and grid items default to `min-width: auto` (or `min-inline-size: auto`). This means the item refuses to shrink smaller than its content's intrinsic `min-content` width. 
- If a flex child contains a long string, the flex item expands horizontally, blowing out the parent container, and `overflow: hidden` never triggers.
- **The Golden Rule**: Any flex item or grid child whose text must truncate **must have `min-width: 0;`** (or `min-inline-size: 0;`).

### Multi-Line Clamping
To clamp text to $N$ lines, browsers universally support the WebKit flexbox legacy pattern (Baseline 2020):
```css
.multi-line-truncate {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 3;      /* Clamp to 3 lines */
  overflow: hidden;
}
```
In the latest CSS Overflow Level 4 standard, this is being standardized as:
```css
.modern-clamp {
  line-clamp: 3;
}
```
Modern browser engines alias `line-clamp` while maintaining the `-webkit-` syntax for universal compatibility.

---

## 3. How It Works Internally

```
Anatomy of Flex Child Truncation Failure vs Fix

1. Default (min-width: auto):
+------------------------------------+
| .flex-container (width: 300px)     |
| +----------------------------------+-----------------------------+
| | .flex-child (min-width: auto) expands to fit 600px text!       |
| +----------------------------------+-----------------------------+
+------------------------------------+
  Result: Horizontal container blowout!

2. With `min-width: 0`:
+------------------------------------+
| .flex-container (width: 300px)     |
| +--------------------------------+ |
| | .flex-child (min-width: 0)     | |
| | Clips text with ellipsis...    | |
| +--------------------------------+ |
+------------------------------------+
  Result: Clean truncation inside parent bounds!
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **Single-Line `text-overflow: ellipsis`**: Baseline Widely Available (supported across all modern browsers).
- **Multi-Line `-webkit-line-clamp`**: Baseline Widely Available (Chrome 1+, Safari 5+, Firefox 68+, Edge 79+).
- **Standard `line-clamp`**: Baseline 2024 / Emerging.

```css
/* Standard Single-Line Ellipsis */
.truncate-single {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  min-width: 0; /* Defends against flex/grid expansion */
}

/* Standard 3-Line Clamping */
.truncate-lines-3 {
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 3;
  line-clamp: 3;
  overflow: hidden;
}
```

---

## 5. Practical Examples

### Example 1: Truncation Inside a Flexbox User List Item
```html
<div class="user-row">
  <img src="avatar.jpg" alt="" class="avatar">
  <div class="user-info">
    <h4 class="name" title="Dr. Jonathan Bartholomew-Livingston III">
      Dr. Jonathan Bartholomew-Livingston III
    </h4>
    <p class="role" title="Senior Vice President of Distributed Systems Architecture">
      Senior Vice President of Distributed Systems Architecture
    </p>
  </div>
  <button type="button" class="action-btn">Connect</button>
</div>
```

```css
.user-row {
  display: flex;
  align-items: center;
  gap: 1rem;
  max-width: 360px;
  padding: 0.75rem 1rem;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  background-color: #ffffff;
}

.avatar {
  width: 44px;
  height: 44px;
  border-radius: 50%;
  flex-shrink: 0;
}

/* Critical: min-width: 0 allows the flex child to shrink below intrinsic content width */
.user-info {
  flex: 1;
  min-width: 0;
}

.name {
  margin: 0;
  font-size: 0.9375rem;
  font-weight: 600;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.role {
  margin: 0.25rem 0 0 0;
  font-size: 0.8125rem;
  color: #64748b;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.action-btn {
  flex-shrink: 0;
  padding: 0.375rem 0.75rem;
  font-size: 0.8125rem;
  font-weight: 600;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
}

.action-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: Card Excerpt Multi-Line Clamping (2 Lines)
```html
<article class="news-card">
  <span class="tag">Security Advisory</span>
  <h3 class="news-title">Zero-Day Vulnerability Patched in Core SSH Daemons Worldwide</h3>
  <p class="news-excerpt">
    A high-severity cryptographic flaw discovered in legacy SSH daemon implementations
    has been patched today across all major Linux enterprise distributions, including
    RHEL, Debian, Alpine, and Ubuntu server LTS releases.
  </p>
</article>
```

```css
.news-card {
  max-width: 320px;
  padding: 1.25rem;
  border: 1px solid #cbd5e1;
  border-radius: 10px;
  background-color: #ffffff;
}

.news-title {
  font-size: 1.125rem;
  font-weight: 700;
  line-height: 1.3;
  margin-block: 0.5rem 0.75rem;
  
  /* Clamps headline to 2 lines */
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
  line-clamp: 2;
  overflow: hidden;
}

.news-excerpt {
  font-size: 0.875rem;
  line-height: 1.5;
  color: #475569;
  
  /* Clamps body excerpt to 3 lines */
  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 3;
  line-clamp: 3;
  overflow: hidden;
}
```

### Example 3: Gradient Fade-Out Text Masking (Alternative to Ellipsis)
```css
/* Creates an editorial fade-out gradient at the bottom of long text blocks */
.fade-out-block {
  max-height: 120px;
  overflow: hidden;
  position: relative;
  line-height: 1.6;
}

.fade-out-block::after {
  content: "";
  position: absolute;
  inset-inline: 0;
  inset-block-end: 0;
  block-size: 48px;
  background: linear-gradient(to bottom, rgba(255, 255, 255, 0), #ffffff);
  pointer-events: none;
}
```

### Example 4: Expandable Text Disclosure via Pure CSS `<details>`
```html
<details class="accessible-read-more">
  <summary class="read-more-trigger">
    <span class="label-closed">Read full biography</span>
    <span class="label-open">Show less</span>
  </summary>
  <p class="full-bio">
    Born in 1982, Sarah Connor spearheaded the human resistance, leading autonomous
    drone counter-operations and architecting resilient offline communications networks.
  </p>
</details>
```

```css
.accessible-read-more summary {
  cursor: pointer;
  color: #2563eb;
  font-weight: 600;
  font-size: 0.875rem;
  margin-block-start: 0.5rem;
  list-style: none; /* Hides default disclosure triangle */
}

.accessible-read-more summary::-webkit-details-marker {
  display: none;
}

.accessible-read-more summary:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.accessible-read-more .label-open { display: none; }
.accessible-read-more[open] .label-closed { display: none; }
.accessible-read-more[open] .label-open { display: inline; }
```

### Example 5: Grid Cell Truncation with `minmax(0, 1fr)`
```css
/* In CSS Grid, tracks default to minmax(auto, 1fr).
   Setting minmax(0, 1fr) permits columns to shrink so ellipsis works! */
.data-grid {
  display: grid;
  grid-template-columns: 120px minmax(0, 1fr) 100px;
  gap: 1rem;
  align-items: center;
}

.data-grid .filename {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```

---

## 6. Best Practices

1. **Always Add `min-width: 0` to Truncated Flex and Grid Items**: Without `min-width: 0;` (or `min-inline-size: 0;`), flex items default to `auto` and expand past their container instead of truncating.
2. **Provide Full Text in HTML `title` or `aria-label`**: Visually truncating critical information (like filenames or error messages) leaves mouse and screen reader users unable to read the full string unless an accessible tooltip or accessible name is provided.
3. **Use Multi-Line Clamping for Card Descriptions**: Limit card descriptions to 2 or 3 lines to maintain uniform card heights across responsive grid rows.
4. **Avoid Fixed Heights When Using `line-clamp`**: Do not combine `height: 60px;` with `-webkit-line-clamp: 3;`. Let line clamping naturally dictate the element's height.
5. **Always Set `line-height` Explicitly on Clamped Containers**: Because line clamping clips based on line boxes, an undeclared line height can cause partial lines or chopped ascenders to bleed into view.
6. **Prefer Semantic `<details>` Over CSS Hack Toggles**: Use native `<details>` and `<summary>` for expandable text; it provides built-in keyboard accessibility and screen reader support without JavaScript.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`text-overflow: ellipsis` without `overflow: hidden`** | The text simply overflows the container boundaries; ellipsis never renders. | Must pair with `overflow: hidden` and `white-space: nowrap`. |
| **`text-overflow: ellipsis` failing inside flex item** | Flex child defaults to `min-width: auto`, expanding parent instead of clipping. | Add `min-width: 0;` to the flex child. |
| **Using `height` with `-webkit-line-clamp`** | Fixed height conflicts with the line box clamp count, cutting off lines awkwardly. | Remove `height`; let `-webkit-line-clamp` control box bounds. |
| **Truncating text without providing accessible full text** | Users on touchscreen devices or screen readers cannot access clipped information. | Add `title="..."` or accessible modal / `<details>`. |
| **Single-line truncation on inline `<span>`** | `text-overflow` only applies to block containers. | Change `span` to `display: block;` or `display: inline-block;`. |
| **Applying `line-clamp` to table cells directly** | Table layout engines do not treat `<td>` as standard block containers. | Wrap table cell content in a `<div class="clamped">`. |

---

## 8. Performance, Accessibility & Browser Support

### Accessibility Compliance (WCAG 2.2 AA)
- **Criterion 1.4.4 (Resize Text)**: When a user zooms the browser to 200%, truncated text components must not break the overall layout or trap focus.
- **Criterion 4.1.2 (Name, Role, Value)**: Truncated links and buttons must still convey their full accessible purpose.

### Layout Engine Performance
Single-line truncation and multi-line line clamping are computed directly by the layout engine (e.g., Chromium LayoutNG) during the inline layout pass. They do not trigger JavaScript layout thrashing or recalculations.

---

## 9. Comparison / When to Use What

| Technique | Max Lines | Visual Indicator | Best Used For |
| :--- | :--- | :--- | :--- |
| **Single-Line Ellipsis** | Strictly 1 | `...` at right edge | Filenames, email addresses, table cells |
| **`-webkit-line-clamp`** | Configurable (2–6) | `...` at end of last line | Blog excerpts, product card titles, reviews |
| **CSS Mask Gradient** | Variable | Smooth vertical fade-out | Editorial paywalls, long quote blocks |
| **`<details>` Disclosure** | Dynamic | Expands to 100% full text | Biographies, FAQ items, terms and conditions |

---

## 10. Interview Questions

<details>
<summary>1. Why does <code>text-overflow: ellipsis</code> frequently fail to work when applied to a child inside a Flexbox container?</summary>

By default according to the CSS Flexible Box Layout specification, all flex items have an initial value of `min-width: auto` (instead of `0`). This means a flex child's minimum width is dictated by its content's intrinsic `min-content` size. 

When a long text string is placed inside a flex item, the item refuses to shrink below the full width of that text string, causing the flex item (and often its parent container) to stretch horizontally. Because the element's width expands to fit the text, overflow never occurs, and `overflow: hidden` is never triggered. Adding `min-width: 0;` (or `min-inline-size: 0;`) resets the minimum size constraint to zero, allowing the item to shrink and triggering the ellipsis.
</details>

<details>
<summary>2. What are the three mandatory CSS declarations required for single-line text truncation to work?</summary>

The three mandatory properties are:
1. `white-space: nowrap;` (Forces the text to remain on a single unbroken line).
2. `overflow: hidden;` (Clips the content when it exceeds the element's bounding box).
3. `text-overflow: ellipsis;` (Renders the `...` symbol at the clipping boundary).

Additionally, the element must establish a block formatting context (`display: block` or `inline-block`) with a constrained width.
</details>

<details>
<summary>3. Why is the prefix <code>-webkit-</code> still required for multi-line clamping in modern browsers?</summary>

The multi-line clamping technique was originally an undocumented proprietary WebKit extension (`display: -webkit-box; -webkit-box-orient: vertical; -webkit-line-clamp: N;`). Over a decade, millions of websites adopted this exact syntax. 

When the W3C CSS Working Group designed the standard `line-clamp` property in CSS Overflow Level 4, browser vendors (including Firefox and Chromium) agreed to standardize and support the `-webkit-` syntax permanently as part of the Web Platform compatibility standard so legacy code wouldn't break.
</details>

<details>
<summary>4. What accessibility problem arises when text is truncated with CSS ellipsis?</summary>

CSS ellipsis purely alters visual rendering; the underlying DOM text remains intact. However:
1. Touchscreen users have no native way to trigger `:hover` tooltips to read clipped text.
2. If text is truncated mid-word without an accessible alternative, users with cognitive disabilities or low vision may miss critical data (e.g., distinguishing between `invoice_final_v1.pdf` and `invoice_final_v2.pdf`).
Developers should provide the full string via the HTML `title` attribute, an accessible tooltip, or a toggleable disclosure.
</details>

<details>
<summary>5. How does <code>minmax(0, 1fr)</code> solve truncation issues in CSS Grid?</summary>

Similar to Flexbox, grid tracks defined with `1fr` compute to `minmax(auto, 1fr)`. If a grid cell contains an unbreakable string or single-line truncated text, the track locks to the `min-content` width of that string and expands the entire grid column. Explicitly declaring `grid-template-columns: minmax(0, 1fr);` overrides the `auto` floor, allowing the column to shrink below content width and enabling ellipsis clipping.
</details>

<details>
<summary>6. Can you apply <code>text-overflow: ellipsis</code> directly to a <code>&lt;span&gt;</code> element?</summary>

No, not with default styles. `text-overflow` only applies to block containers. Inline elements (`display: inline`) do not generate independent bounding boxes or support `overflow: hidden`. To truncate a `<span>`, you must set `display: inline-block;` or `display: block;` and define a width constraint.
</details>

<details>
<summary>7. What is the difference between <code>text-overflow: ellipsis</code> and <code>text-overflow: clip</code>?</summary>

- `text-overflow: clip` (default): Abruptly slices glyphs at the exact pixel boundary of the overflow box. A letter can be chopped vertically in half.
- `text-overflow: ellipsis`: Replaces the last visible characters before the boundary with a typographical ellipsis (`...` or character $U+2026$) without chopping individual letterforms.
</details>

<details>
<summary>8. How does `text-overflow` behave in Right-to-Left (RTL) text flows?</summary>

`text-overflow: ellipsis` is direction-aware. In an LTR container, the ellipsis renders at the right inline-end edge. In an RTL container (`dir="rtl"`), the ellipsis renders at the left inline-end edge, where the text overflows.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Truncated File Explorer List Item
**Goal**: Build a flex item that truncates long filenames while keeping file size and date badges fully visible.

<details>
<summary>View Solution</summary>

```html
<div class="file-item">
  <span class="file-icon">📄</span>
  <span class="file-name" title="financial_q4_audit_report_consolidated_final_signed.xlsx">
    financial_q4_audit_report_consolidated_final_signed.xlsx
  </span>
  <span class="file-size">4.2 MB</span>
</div>
```

```css
.file-item {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  max-width: 350px;
  padding: 0.5rem 0.75rem;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  background-color: #ffffff;
}

.file-icon {
  flex-shrink: 0;
}

.file-name {
  flex: 1;
  min-width: 0; /* Crucial for flex child truncation */
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  font-family: ui-monospace, monospace;
  font-size: 0.8125rem;
}

.file-size {
  flex-shrink: 0;
  font-size: 0.75rem;
  color: #64748b;
}
```
</details>

### Exercise 2: Implement 2-Line Article Clamping
**Goal**: Clamp article card headlines to strictly 2 lines with an ellipsis.

<details>
<summary>View Solution</summary>

```css
.card-headline {
  font-family: system-ui, sans-serif;
  font-size: 1.125rem;
  font-weight: 700;
  line-height: 1.35;
  margin: 0;

  display: -webkit-box;
  -webkit-box-orient: vertical;
  -webkit-line-clamp: 2;
  line-clamp: 2;
  overflow: hidden;
}
```
</details>

### Exercise 3: Construct a Pure CSS Bottom Fade-Out Mask
**Goal**: Fade the bottom 40px of a long quotation block using CSS linear gradients.

<details>
<summary>View Solution</summary>

```css
.quote-preview {
  position: relative;
  max-height: 100px;
  overflow: hidden;
  line-height: 1.5;
  color: #334155;
}

.quote-preview::after {
  content: "";
  position: absolute;
  inset-inline: 0;
  inset-block-end: 0;
  height: 40px;
  background: linear-gradient(to bottom, rgba(255, 255, 255, 0), #ffffff);
  pointer-events: none;
}
```
</details>

### Exercise 4: Truncate Table Cells with `table-layout: fixed`
**Goal**: Enforce single-line ellipsis truncation inside a data table cell.

<details>
<summary>View Solution</summary>

```css
.fixed-data-table {
  width: 100%;
  table-layout: fixed;
  border-collapse: collapse;
}

.fixed-data-table td.truncate-cell {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

### Exercise 5: Accessible Expandable Text with `<details>`
**Goal**: Style a `<details>` element where clicking toggles clamped vs full text.

<details>
<summary>View Solution</summary>

```html
<details class="accordion-text">
  <summary>Toggle description</summary>
  <p>
    This full paragraph contains detailed operational documentation that expands and
    collapses cleanly without requiring any JavaScript libraries.
  </p>
</details>
```

```css
.accordion-text summary {
  color: #2563eb;
  font-weight: 600;
  cursor: pointer;
}

.accordion-text summary:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.accordion-text p {
  margin-block-start: 0.5rem;
  line-height: 1.6;
}
```
</details>

### Exercise 6: Grid Column Truncation Verification
**Goal**: Construct a 3-column grid where the middle column truncates cleanly when the container shrinks.

<details>
<summary>View Solution</summary>

```css
.grid-container {
  display: grid;
  grid-template-columns: 80px minmax(0, 1fr) 80px;
  gap: 1rem;
  max-width: 400px;
}

.grid-middle {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Technique | Core Syntax | Key Requirements |
| :--- | :--- | :--- |
| **Single-Line Ellipsis** | `white-space: nowrap; overflow: hidden; text-overflow: ellipsis;` | Must have block formatting and width constraint |
| **Flex Item Truncation**| `min-width: 0;` (or `min-inline-size: 0;`) | Required on flex/grid children to override `min-width: auto` |
| **Multi-Line Clamping** | `display: -webkit-box; -webkit-box-orient: vertical; -webkit-line-clamp: N; overflow: hidden;` | Supported across all modern browsers |
| **Grid Track Truncation**| `grid-template-columns: minmax(0, 1fr);` | Overrides the default `minmax(auto, 1fr)` track sizing |

---

## 13. Further Reading
- [W3C CSS Overflow Module Level 3](https://www.w3.org/TR/css-overflow-3/)
- [W3C CSS Overflow Module Level 4 - line-clamp](https://www.w3.org/TR/css-overflow-4/#line-clamp)
- [MDN Web Docs: text-overflow](https://developer.mozilla.org/en-US/docs/Web/CSS/text-overflow)
- [MDN Web Docs: -webkit-line-clamp](https://developer.mozilla.org/en-US/docs/Web/CSS/-webkit-line-clamp)
- [CSS-Tricks: Flexbox and Truncated Text](https://css-tricks.com/flexbox-truncated-text/)

---

[Previous: 07-writing-modes-and-internationalisation.md](07-writing-modes-and-internationalisation.md) | [Module README](README.md) | [Next: 09-icon-fonts-vs-svg.md](09-icon-fonts-vs-svg.md)
