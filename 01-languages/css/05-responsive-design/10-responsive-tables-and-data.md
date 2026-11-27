# Responsive Tables and Data

| Metadata | Details |
| :--- | :--- |
| **Topic** | Responsive Data Presentation, Horizontal Scrolling, Mobile Card Transformation, Sticky Table Headers |
| **Spec Level** | CSS Table Module Level 3 / CSS Overflow Module Level 3 / CSS Box Alignment Module Level 3 |
| **Baseline Status** | Widely Available (CSS Tables & `overflow-x: auto` Baseline 2015; `position: sticky` Baseline 2017; Container Queries Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 122+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA, VoiceOver, Playwright, Chrome DevTools Responsive Mode |
| **Module** | [05-responsive-design](README.md) |

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

HTML tabular data (`<table>`) is inherently two-dimensional, rigid, and relational. By default, tables expand horizontally to accommodate their cell contents, obeying complex internal table layout algorithms (`table-layout: auto`). On narrow screens (smartphones with 320px–390px viewport widths), wide relational datasets with 6 to 12 columns inevitably cause viewport horizontal blowout, clipping data, or destroying readable typography if cells collapse too tightly.

Responsive table engineering solves this fundamental conflict through three primary strategies:
1. **Contained Horizontal Overflow**: Enclosing the table in an accessible scroll container (`overflow-x: auto; tabindex="0"`) equipped with scroll shadows and fixed primary headers/columns (`position: sticky`).
2. **Structural Transformation (Card Layout)**: Converting relational rows into standalone self-describing cards on small screens (`display: block` or `display: grid`) while injecting column header context via pseudo-elements (`::before`) and the HTML `data-label` attribute.
3. **Adaptive Column Prioritization**: Selectively hiding secondary or tertiary columns on narrow screens while keeping mission-critical summary metrics visible, often toggled through CSS Container Queries.

Understanding how to build responsive data tables without destroying accessibility semantics (such as screen reader table navigation keys) is a critical skill for senior frontend engineers.

---

## 2. Core Concepts

### The Native Table Layout Engine (`auto` vs `fixed`)
- `table-layout: auto`: Default browser algorithm. Column widths depend on cell contents across the entire table. The browser reads all cell contents before calculating column widths, causing layout reflows on streaming or large datasets.
- `table-layout: fixed`: High-performance algorithm. The table width and column widths are determined solely by the width of the table and the explicit widths of the `<col>` or first row cells (`<th>`/`<td>`). Content inside cells does not alter column widths; excess content truncates, clips, or wraps.

### The Accessible Scroll Container
Tables should never overflow the main viewport document `<html>` or `<body>`. Instead, wrap the table in a container:
```html
<div class="table-container" tabindex="0" role="region" aria-label="Monthly Sales Report">
  <table>...</table>
</div>
```
The wrapper requires `tabindex="0"` so keyboard-only users can focus the scrollable container and use arrow keys to pan horizontally.

### CSS Sticky Positioning in Tables
`position: sticky` on `<th>` elements allows column headers to remain pinned to the top of the viewport or scroll container while scrolling vertically, and row headers (`<th scope="row">`) to remain pinned to the left while scrolling horizontally.
> Note: Sticky table headers require an explicit `background-color`; otherwise, scrolling table body cells render visibly underneath the transparent header cells.

### Card Transformation via `data-label` and `attr()`
On mobile viewports, rows can be transformed into card-like blocks:
- Set `table, thead, tbody, th, td, tr { display: block; }`.
- Visually hide `thead` (preserving screen reader access).
- Populate table cells with `data-label="Column Name"`.
- Generate header labels dynamically via `td::before { content: attr(data-label); font-weight: bold; }`.

```
Desktop (Tabular Grid):
+----------+----------+----------+
| ID       | Customer | Status   |
+----------+----------+----------+
| #1001    | Alice    | Paid     |
| #1002    | Bob      | Pending  |
+----------+----------+----------+

Mobile Card Transformation:
+-------------------------------+
| ID:       #1001               |
| Customer: Alice               |
| Status:   Paid                |
+-------------------------------+
+-------------------------------+
| ID:       #1002               |
| Customer: Bob                 |
| Status:   Pending             |
+-------------------------------+
```

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------+
|  .table-container (overflow-x: auto, position: relative)              |
|                                                                       |
|  +-----------------------------------------------------------------+  |
|  | table (width: 100%, min-width: 640px)                           |  |
|  |                                                                 |  |
|  | [thead: sticky top: 0, z-index: 2]                              |  |
|  | +-----------+---------------+-----------------+---------------+ |  |
|  | | th (col1) | th (col2)     | th (col3)       | th (col4)     | |  |
|  | +-----------+---------------+-----------------+---------------+ |  |
|  | [tbody: vertical scroll]                                        |  |
|  | | td        | td            | td              | td            | |  |
|  | | td        | td            | td              | td            | |  |
|  +-----------------------------------------------------------------+  |
|                                                                       |
+-----------------------------------------------------------------------+
                <--- Scrollable along inline X-axis --->
```

### The Accessibility Trap of `display: block` on Tables
Historically, when WebKit (Safari) and Blink (Chrome) encountered `display: block`, `display: flex`, or `display: grid` applied to `<table>`, `<tr>`, or `<td>` elements, the browser engine stripped the implicit table ARIA semantics from the accessibility tree. A screen reader reading a transformed table would announce "group" or "generic container" instead of "Table, 3 columns, 4 rows".

To preserve accessibility when applying card transformations:
- Modern CSS Container Queries allow selective column styling without breaking layout.
- If `display: block` is required, explicitly restore ARIA semantics:
```html
<table role="table">
  <thead role="rowgroup"><tr role="row"><th role="columnheader">...</th></tr></thead>
  <tbody role="rowgroup"><tr role="row"><td role="cell">...</td></tr></tbody>
</table>
```

---

## 4. Syntax & Usage

### Baseline & Feature Support
- **`overflow-x: auto` & CSS Table Module**: Baseline Widely Available (since all major browser releases).
- **`position: sticky` on `<th>` / `<td>`**: Baseline Widely Available (Safari 13+, Chrome 56+, Firefox 59+).
- **CSS Container Queries for Tables**: Baseline 2023 (Safari 16+, Chrome 105+, Firefox 110+).

```css
/* Baseline Horizontal Scroll Container */
.data-table-wrapper {
  width: 100%;
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
}

.data-table-wrapper:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  text-align: left;
  font-size: 0.875rem;
}

.data-table th,
.data-table td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e2e8f0;
  white-space: nowrap;
}

.data-table th {
  background-color: #f8fafc;
  font-weight: 600;
  color: #1e293b;
  position: sticky;
  top: 0;
  z-index: 10;
}
```

---

## 5. Practical Examples

### Example 1: Contained Horizontal Scroll with Accessible Keyboard Pan
```html
<div class="table-scroll-container" tabindex="0" role="region" aria-label="System Metrics Table">
  <table class="system-metrics">
    <thead>
      <tr>
        <th scope="col">Node ID</th>
        <th scope="col">IP Address</th>
        <th scope="col">CPU Usage (%)</th>
        <th scope="col">Memory (GB)</th>
        <th scope="col">Disk I/O (MB/s)</th>
        <th scope="col">Status</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <th scope="row">worker-prod-01</th>
        <td>192.168.1.101</td>
        <td>42.5</td>
        <td>14.2 / 32.0</td>
        <td>128.4</td>
        <td><span class="status status-healthy">Online</span></td>
      </tr>
      <tr>
        <th scope="row">worker-prod-02</th>
        <td>192.168.1.102</td>
        <td>88.1</td>
        <td>30.1 / 32.0</td>
        <td>412.0</td>
        <td><span class="status status-warning">High Load</span></td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.table-scroll-container {
  width: 100%;
  overflow-x: auto;
  border: 1px solid #d1d5db;
  border-radius: 6px;
  background-color: #ffffff;
}

.table-scroll-container:focus-visible {
  outline: 2px solid #3b82f6;
  outline-offset: 2px;
}

.system-metrics {
  width: 100%;
  min-width: 600px;
  border-collapse: collapse;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  font-size: 0.8125rem;
}

.system-metrics th,
.system-metrics td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e5e7eb;
  text-align: left;
}

.system-metrics thead th {
  background-color: #f9fafb;
  color: #374151;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  font-size: 0.75rem;
}

.system-metrics tbody tr:hover {
  background-color: #f3f4f6;
}

.status {
  display: inline-block;
  padding: 0.25rem 0.5rem;
  border-radius: 9999px;
  font-weight: 600;
  font-size: 0.75rem;
}

.status-healthy {
  background-color: #dcfce7;
  color: #166534;
}

.status-warning {
  background-color: #fef9c3;
  color: #854d0e;
}
```

### Example 2: Two-Axis Sticky Headers (Sticky Row & Column)
```css
.matrix-container {
  width: 100%;
  max-height: 400px;
  overflow: auto;
  border: 1px solid #cbd5e1;
}

.matrix-table {
  border-collapse: separate;
  border-spacing: 0;
  width: 100%;
}

/* Sticky Column Header (top) */
.matrix-table th {
  position: sticky;
  top: 0;
  background-color: #f1f5f9;
  z-index: 2;
  border-bottom: 2px solid #cbd5e1;
  padding: 0.75rem;
}

/* Sticky Row Header (left) */
.matrix-table tbody th {
  position: sticky;
  left: 0;
  top: auto;
  background-color: #f8fafc;
  z-index: 1;
  border-right: 2px solid #cbd5e1;
  text-align: left;
}

/* Intersecting Top-Left Corner Cell */
.matrix-table thead th:first-child {
  position: sticky;
  top: 0;
  left: 0;
  z-index: 3;
  background-color: #e2e8f0;
}
```

### Example 3: Mobile Card Transformation with `data-label`
```html
<table class="card-responsive-table" role="table">
  <caption class="sr-only">Q3 Customer Invoices</caption>
  <thead role="rowgroup">
    <tr role="row">
      <th scope="col" role="columnheader">Invoice</th>
      <th scope="col" role="columnheader">Client</th>
      <th scope="col" role="columnheader">Amount</th>
      <th scope="col" role="columnheader">Due Date</th>
    </tr>
  </thead>
  <tbody role="rowgroup">
    <tr role="row">
      <td role="cell" data-label="Invoice">#INV-2026-081</td>
      <td role="cell" data-label="Client">Acme Corp</td>
      <td role="cell" data-label="Amount">$14,250.00</td>
      <td role="cell" data-label="Due Date">2026-12-15</td>
    </tr>
    <tr role="row">
      <td role="cell" data-label="Invoice">#INV-2026-082</td>
      <td role="cell" data-label="Client">Starlight Media</td>
      <td role="cell" data-label="Amount">$3,800.00</td>
      <td role="cell" data-label="Due Date">2026-12-18</td>
    </tr>
  </tbody>
</table>
```

```css
.card-responsive-table {
  width: 100%;
  border-collapse: collapse;
}

.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  border: 0;
}

@media (max-width: 640px) {
  /* Collapse table structure into block cards */
  .card-responsive-table,
  .card-responsive-table thead,
  .card-responsive-table tbody,
  .card-responsive-table th,
  .card-responsive-table td,
  .card-responsive-table tr {
    display: block;
  }

  /* Visually hide original table headers */
  .card-responsive-table thead tr {
    position: absolute;
    top: -9999px;
    left: -9999px;
  }

  .card-responsive-table tr {
    margin-bottom: 1rem;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    background-color: #ffffff;
    padding: 0.5rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
  }

  .card-responsive-table td {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem 0.75rem;
    border-bottom: 1px solid #f1f5f9;
    text-align: right;
  }

  .card-responsive-table td:last-child {
    border-bottom: none;
  }

  /* Generate mobile label from HTML attribute */
  .card-responsive-table td::before {
    content: attr(data-label);
    font-weight: 600;
    color: #475569;
    text-align: left;
    margin-right: 1rem;
  }
}
```

### Example 4: CSS Scroll Indicator Shadows (Pure CSS Scroll Hint)
```css
/* Creates gradient shadows on edges indicating horizontal scrollability */
.scroll-shadow-wrapper {
  position: relative;
  width: 100%;
  overflow-x: auto;
  
  /* Pure CSS linear gradient trick for scroll shadows */
  background:
    /* Shadow covers */
    linear-gradient(to right, white 30%, rgba(255, 255, 255, 0)),
    linear-gradient(to right, rgba(255, 255, 255, 0), white 70%) 100% 0,
    /* Shadows */
    radial-gradient(farthest-side at 0 50%, rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0)),
    radial-gradient(farthest-side at 100% 50%, rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0)) 100% 0;
  background-repeat: no-repeat;
  background-color: white;
  background-size: 40px 100%, 40px 100%, 14px 100%, 14px 100%;
  background-attachment: local, local, scroll, scroll;
}
```

### Example 5: Container Query Priority Columns
```html
<div class="table-card-widget">
  <table class="adaptive-table">
    <thead>
      <tr>
        <th class="col-primary">Project</th>
        <th class="col-secondary">Lead</th>
        <th class="col-tertiary">Budget</th>
        <th class="col-primary">Status</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td class="col-primary">Apollo Gateway</td>
        <td class="col-secondary">Sarah Connor</td>
        <td class="col-tertiary">$450,000</td>
        <td class="col-primary"><span class="badge">In Progress</span></td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.table-card-widget {
  container-type: inline-size;
  container-name: table-widget;
  width: 100%;
}

.adaptive-table {
  width: 100%;
  border-collapse: collapse;
}

.adaptive-table th,
.adaptive-table td {
  padding: 0.625rem 0.875rem;
  border-bottom: 1px solid #e2e8f0;
}

/* When the widget container is narrow, progressively hide lower-priority columns */
@container table-widget (max-width: 500px) {
  .adaptive-table .col-tertiary {
    display: none;
  }
}

@container table-widget (max-width: 380px) {
  .adaptive-table .col-secondary {
    display: none;
  }
}
```

---

## 6. Best Practices

1. **Always Add `tabindex="0"` to Horizontal Scroll Wrappers**: Screen reader and keyboard users cannot pan an overflowing `<div>` unless it receives keyboard focus.
2. **Assign Explicit Accessible Names via `aria-label` or `aria-labelledby`**: A scroll container with `tabindex="0"` creates a focus stop. Without an accessible label, screen readers announce "group", leaving the user disoriented.
3. **Use `border-collapse: separate; border-spacing: 0;` for Sticky Table Cells**: In many browser engines, `position: sticky` on `<th>` elements glitches or produces visual border artifacts when combined with `border-collapse: collapse;`.
4. **Always Define Explicit Background Colors on Sticky Headers**: If sticky `<th>` elements have a transparent background, scrolled row content will bleed through the header text.
5. **Always Set `scope="col"` and `scope="row"`**: Screen readers depend on `scope` attributes to associate data cells with their respective header cells.
6. **Preserve Semantic Roles if Converting to Block**: If converting table structures with `display: block` on mobile, add ARIA table attributes (`role="table"`, `role="row"`, `role="cell"`) to maintain accessibility tree relationships in WebKit browsers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
| :--- | :--- | :--- |
| **`overflow-x: auto` directly on `<table>`** | Browsers do not treat `<table>` as a block formatting context; table width expands past its parent. | Wrap the `<table>` in a `<div class="table-wrapper">` with `overflow-x: auto`. |
| **Unlabeled Keyboard Scroll Containers** | Keyboard users tab into an unnamed `div`, hearing "section" or "group" without context. | Add `role="region" aria-label="Descriptive Table Title" tabindex="0"`. |
| **`border-collapse: collapse` with Sticky Headers** | Sticky cells leave border gaps or cause erratic rendering during scroll in Firefox and Safari. | Use `border-collapse: separate; border-spacing: 0;` and borders on `td`/`th`. |
| **Omitting Background on Sticky `<th>`** | Scrolled table body text renders directly behind the transparent header cells. | Set explicit `background-color: #ffffff;` (or theme variable) on `th`. |
| **`display: block` stripping ARIA table roles** | Safari/WebKit removes table semantics from the a11y tree when `display: block` is applied. | Add explicit `role="table"`, `role="rowgroup"`, `role="row"`, `role="cell"`. |
| **Hardcoding `font-size: 10px` to fit 8 columns on mobile** | Destroys readability, violates WCAG 1.4.4 (Resize Text), and frustrates users. | Use horizontal scrolling or transform rows into stacked card blocks. |

---

## 8. Performance, Accessibility & Browser Support

### Performance: `table-layout: fixed`
For large tables (1,000+ rows), `table-layout: auto` triggers heavy layout calculations on every DOM insert or resize. Specifying:
```css
.large-table {
  table-layout: fixed;
  width: 100%;
}
```
allows the browser layout engine to render rows incrementally without waiting to calculate the content width of every cell in every row.

### Accessibility Checklist (WCAG 2.2 AA)
- [ ] Every table has a `<caption>` or `aria-label` describing its content.
- [ ] Header cells use `<th>` with appropriate `scope="col"` or `scope="row"`.
- [ ] Scrollable containers are reachable via keyboard (`tabindex="0"`).
- [ ] Scrollable containers have an accessible name (`aria-label="..."` or `aria-labelledby="..."`).
- [ ] Touch scroll areas have `-webkit-overflow-scrolling: touch;` for smooth inertia on iOS.

---

## 9. Comparison / When to Use What

| Pattern | Best Used For | Mobile UX | Accessibility Overhead | Implementation Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **Contained Scroll Container** | High-density financial data, multi-column analytics, comparison tables | Good; preserves row/column correlation | Low; requires `tabindex="0"` + `aria-label` | Low |
| **Card Transformation (`data-label`)** | Directories, e-commerce orders, user lists (3–6 columns) | Excellent; natural vertical reading flow | Medium; requires explicit ARIA roles | Medium |
| **Priority Column Hiding** | Dashboards, ticket tracking where 2–3 columns are primary | High; clean uncluttered view | Low; hidden columns must not contain vital errors | Low |
| **Accordion / Collapsible Rows** | Detail-heavy records with primary status + hidden metadata | Excellent; taps reveal deep row details | Medium; requires `aria-expanded` buttons | High |

---

## 10. Interview Questions

<details>
<summary>1. Why does applying <code>overflow-x: auto</code> directly to an HTML <code>&lt;table&gt;</code> element fail to create a scroll container?</summary>

According to the CSS Table specification, an element with `display: table` generates a table wrapper box and table grid box. The table layout engine does not calculate overflow boundaries like standard block containers (`display: block`). When a table's contents exceed the available width, the table element simply expands rather than creating a scrollable viewport. Wrapping the table inside a block element (`<div style="overflow-x: auto;">`) creates a standard block formatting context with a defined bounding box that reliably clips and scrolls overflowing table elements.
</details>

<details>
<summary>2. How does the browser layout engine handle <code>table-layout: auto</code> versus <code>table-layout: fixed</code>?</summary>

Under `table-layout: auto`, the browser must parse every cell in the entire table before computing column widths. The column width is calculated based on the maximum intrinsic `min-content` and `max-content` widths of all cells in that column. This causes high CPU layout overhead and delays initial rendering on large datasets.

Under `table-layout: fixed`, the browser only reads the table's explicit width and the first row of cells (or `<col>` elements). Subsequent rows do not influence column widths. The browser renders rows immediately as they stream across the network, reducing layout time from $O(N \times M)$ to $O(M)$.
</details>

<details>
<summary>3. Why does Safari/WebKit strip table semantics from screen readers when applying <code>display: block</code> on table elements?</summary>

WebKit maintains a heuristic to avoid announcing non-semantic CSS tables as tabular data (such as legacy layouts using `<table>` for positioning). When an author overrides `display: table` with `display: block` or `display: flex`, WebKit infers that the author intended to discard the table presentation semantics. Consequently, VoiceOver ceases announcing row and column coordinates. To retain table accessibility when transforming to cards on mobile, developers must add explicit ARIA roles: `role="table"`, `role="rowgroup"`, `role="row"`, `role="columnheader"`, and `role="cell"`.
</details>

<details>
<summary>4. Why do sticky headers (<code>position: sticky; top: 0;</code>) often fail or misalign in tables using <code>border-collapse: collapse</code>?</summary>

In the collapsed border model (`border-collapse: collapse`), borders are shared between adjacent cells and the table grid itself; individual cells do not own independent borders. When a cell with `position: sticky` scrolls, the shared borders become detached or render erratically in Firefox and Safari because the border coordinate space is bound to the static table grid. The solution is switching to `border-collapse: separate; border-spacing: 0;` and styling cell borders individually on `<th>` and `<td>`.
</details>

<details>
<summary>5. How does the pure-CSS scroll shadow technique work using CSS multiple backgrounds?</summary>

The technique layers four background images using `background-attachment`:
1. Two local gradient covers (`background-attachment: local`) that scroll with the content and mask the shadows when the content is at the extreme left or right.
2. Two radial or linear gradients (`background-attachment: scroll`) that remain fixed to the scroll container's viewport, providing the subtle shadow.
When the user scrolls away from the edge, the local covers scroll out of view, revealing the underlying fixed drop shadows.
</details>

<details>
<summary>6. How can Container Queries improve responsive data table design compared to global Media Queries?</summary>

Media queries only evaluate the global browser viewport. If a table component is placed inside a 300px sidebar or inside an expandable dashboard split-pane, a media query will still evaluate the desktop 1440px viewport and attempt to render 8 columns, overflowing the container. With `@container (max-width: 500px)`, the table component adapts based on its actual available container width, rendering full columns when placed in the main pane and transforming into stacked cards when placed in a sidebar.
</details>

<details>
<summary>7. What accessibility requirement is mandatory when a table scroll container has <code>overflow-x: auto</code>?</summary>

The container must have `tabindex="0"`, a semantic landmark or role (`role="region"`), and an accessible label (`aria-label="..."` or `aria-labelledby="..."`). Without `tabindex="0"`, keyboard-only users cannot focus the container to scroll horizontally using arrow keys. Without an accessible label, screen readers will announce an ambiguous "region" or "group" when the user tabs into it.
</details>

<details>
<summary>8. How do you implement dynamic cell labels in CSS without duplicating text across HTML files?</summary>

By adding custom attributes like `data-label="Column Name"` on every `<td>` element in the HTML markup, CSS can query that attribute dynamically using the `attr()` function inside a pseudo-element:
```css
td::before {
  content: attr(data-label);
  font-weight: 600;
}
```
This avoids creating extra DOM nodes while rendering clear label-value pairings in mobile card layouts.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Accessible Horizontal Scroll Table
**Goal**: Create a 6-column pricing comparison table inside a scrollable container with keyboard focus, focus rings, and proper ARIA labeling.

<details>
<summary>View Solution</summary>

```html
<div class="pricing-scroll-wrapper" tabindex="0" role="region" aria-label="Feature Comparison Matrix">
  <table class="pricing-table">
    <thead>
      <tr>
        <th scope="col">Feature</th>
        <th scope="col">Free</th>
        <th scope="col">Starter</th>
        <th scope="col">Pro</th>
        <th scope="col">Enterprise</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <th scope="row">API Rate Limit</th>
        <td>1,000 / day</td>
        <td>50,000 / day</td>
        <td>500,000 / day</td>
        <td>Unlimited</td>
      </tr>
      <tr>
        <th scope="row">SSO / SAML</th>
        <td>No</td>
        <td>No</td>
        <td>Yes</td>
        <td>Yes</td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.pricing-scroll-wrapper {
  width: 100%;
  overflow-x: auto;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  background-color: #ffffff;
}

.pricing-scroll-wrapper:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.pricing-table {
  width: 100%;
  min-width: 650px;
  border-collapse: separate;
  border-spacing: 0;
  font-size: 0.875rem;
}

.pricing-table th,
.pricing-table td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #f1f5f9;
  text-align: left;
}

.pricing-table thead th {
  background-color: #f8fafc;
  color: #0f172a;
  font-weight: 600;
  border-bottom: 2px solid #e2e8f0;
}
```
</details>

### Exercise 2: Implement Mobile Card Transformation with `data-label`
**Goal**: Take a 4-column user list table and convert it into isolated cards below 600px viewport width using `data-label` and `::before`.

<details>
<summary>View Solution</summary>

```html
<table class="user-table" role="table">
  <thead role="rowgroup">
    <tr role="row">
      <th scope="col" role="columnheader">Name</th>
      <th scope="col" role="columnheader">Email</th>
      <th scope="col" role="columnheader">Role</th>
      <th scope="col" role="columnheader">Action</th>
    </tr>
  </thead>
  <tbody role="rowgroup">
    <tr role="row">
      <td role="cell" data-label="Name">Jane Doe</td>
      <td role="cell" data-label="Email">jane@example.com</td>
      <td role="cell" data-label="Role">Admin</td>
      <td role="cell" data-label="Action"><button type="button">Edit</button></td>
    </tr>
  </tbody>
</table>
```

```css
.user-table {
  width: 100%;
  border-collapse: collapse;
}

@media (max-width: 600px) {
  .user-table,
  .user-table thead,
  .user-table tbody,
  .user-table th,
  .user-table td,
  .user-table tr {
    display: block;
  }

  .user-table thead {
    position: absolute;
    width: 1px;
    height: 1px;
    overflow: hidden;
    clip: rect(0 0 0 0);
  }

  .user-table tr {
    border: 1px solid #e2e8f0;
    border-radius: 8px;
    padding: 0.75rem;
    margin-bottom: 0.75rem;
    background: #ffffff;
  }

  .user-table td {
    display: flex;
    justify-content: space-between;
    padding: 0.375rem 0;
    border-bottom: 1px solid #f8fafc;
  }

  .user-table td::before {
    content: attr(data-label);
    font-weight: 600;
    color: #64748b;
  }
}
```
</details>

### Exercise 3: Two-Axis Sticky Header Matrix Table
**Goal**: Construct a financial quarterly matrix table where both the top header row and left ID column stay pinned during horizontal and vertical scrolling.

<details>
<summary>View Solution</summary>

```css
.matrix-wrapper {
  max-width: 100%;
  max-height: 350px;
  overflow: auto;
  border: 1px solid #cbd5e1;
}

.matrix-table {
  border-collapse: separate;
  border-spacing: 0;
  width: 100%;
}

.matrix-table th,
.matrix-table td {
  padding: 0.625rem 0.875rem;
  border-bottom: 1px solid #e2e8f0;
  border-right: 1px solid #e2e8f0;
  white-space: nowrap;
}

/* Sticky Column Header */
.matrix-table thead th {
  position: sticky;
  top: 0;
  background-color: #f1f5f9;
  z-index: 2;
}

/* Sticky Row Header */
.matrix-table tbody th {
  position: sticky;
  left: 0;
  background-color: #f8fafc;
  z-index: 1;
}

/* Intersection Cell */
.matrix-table thead th:first-child {
  position: sticky;
  top: 0;
  left: 0;
  z-index: 3;
  background-color: #e2e8f0;
}
```
</details>

### Exercise 4: Implement High-Performance `table-layout: fixed` with Truncation
**Goal**: Prevent text blowout and speed up layout rendering on a 500-row log table by enforcing fixed layout with ellipsis clipping.

<details>
<summary>View Solution</summary>

```css
.fixed-log-table {
  table-layout: fixed;
  width: 100%;
  border-collapse: collapse;
}

.fixed-log-table col.col-timestamp { width: 180px; }
.fixed-log-table col.col-level { width: 100px; }
.fixed-log-table col.col-message { width: auto; }

.fixed-log-table th,
.fixed-log-table td {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid #e5e7eb;
}

.fixed-log-table td.col-message {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

### Exercise 5: Container Query Adaptive Column Visibility
**Goal**: Build a data table where columns hide progressively based on the parent component's inline size.

<details>
<summary>View Solution</summary>

```css
.data-widget {
  container-type: inline-size;
  container-name: table-container;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
}

@container table-container (max-width: 480px) {
  .data-table .col-metadata,
  .data-table .col-created-at {
    display: none;
  }
}

@container table-container (max-width: 320px) {
  .data-table .col-author {
    display: none;
  }
}
```
</details>

### Exercise 6: Windows High Contrast Mode Table Verification
**Goal**: Ensure sticky table headers, borders, and active scroll indicators remain distinctly visible under `@media (forced-colors: active)`.

<details>
<summary>View Solution</summary>

```css
@media (forced-colors: active) {
  .pricing-scroll-wrapper {
    border: 2px solid CanvasText;
  }

  .pricing-table th,
  .pricing-table td {
    border-bottom: 1px solid CanvasText;
    border-right: 1px solid CanvasText;
  }

  .pricing-table thead th {
    background-color: Canvas;
    color: CanvasText;
  }

  .pricing-scroll-wrapper:focus-visible {
    outline: 3px solid Highlight;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Technique | Core CSS Snippet | Key Benefit |
| :--- | :--- | :--- |
| **Accessible Scroll Wrapper** | `.wrapper { overflow-x: auto; tabindex="0"; }` | Prevents page viewport blowout; keyboard accessible |
| **Sticky Column Headers** | `th { position: sticky; top: 0; background: #fff; }` | Header stays visible during vertical scroll |
| **Sticky Row Headers** | `tbody th { position: sticky; left: 0; background: #fff; }` | Primary column stays visible during horizontal scroll |
| **Separate Borders for Sticky** | `table { border-collapse: separate; border-spacing: 0; }` | Eliminates border artifacts on sticky cells |
| **Card Transformation** | `td { display: flex; } td::before { content: attr(data-label); }` | Converts tabular rows to readable mobile cards |
| **Fixed Table Layout** | `table { table-layout: fixed; width: 100%; }` | Instant rendering; enforces column width constraints |
| **Container Query Table** | `@container widget (max-width: 450px) { .col-sub { display: none; } }` | Adapts columns to component container width |

---

## 13. Further Reading
- [W3C CSS Table Module Level 3](https://www.w3.org/TR/css-tables-3/)
- [MDN Web Docs: Styling Tables](https://developer.mozilla.org/en-US/docs/Learn/CSS/Building_blocks/Styling_tables)
- [Adrian Roselli: A Responsive Accessible Table](https://adrianroselli.com/2017/11/a-responsive-accessible-table.html)
- [CSS-Tricks: Responsive Data Tables](https://css-tricks.com/responsive-data-tables/)
- [web.dev: Learn CSS Tables](https://web.dev/learn/css/)

---

[Previous: 09-responsive-navigation-patterns.md](09-responsive-navigation-patterns.md) | [Module README](README.md) | [Next Module: 06-typography-and-text](file:///e:/Learning/engineering-notes/01-languages/css/06-typography-and-text/README.md)
