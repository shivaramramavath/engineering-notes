# Responsive Tables Recipes in Modern CSS

| Topic | Module | Level | Baseline Status | Browsers Tested | Date | Prerequisites |
|---|---|---|---|---|---|---|
| Responsive Tables Recipes | 13: Layout & Component Recipes | Intermediate / Advanced | Widely Available | Chrome 120+, Firefox 120+, Safari 17.4+ | 2026-03-31 | Table Layout, Sticky Positioning, Pseudo-elements |

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
9. [Comparison: Table Responsiveness Patterns](#9-comparison-table-responsiveness-patterns)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Relational and financial data structures—transactions, telemetry feeds, user audits, and resource inventories—are fundamentally tabular. On high-resolution desktop viewports, HTML `<table>` elements render wide, multi-column datasets with optimal visual scanning. However, on compact mobile and tablet viewports, wide tables present one of the most stubborn layout challenges on the web:
- **Viewport Blowout**: Tables naturally expand to accommodate cell content (`min-content`). On small screens, this stretches the entire document horizontally, breaking page responsiveness and clipping navigation bars.
- **Lost Semantic Context**: Blindly setting `display: block` or `display: flex` on table elements strips native screen reader tabular semantics in several major browsers (WebKit/Safari).
- **The "Unsignaled Scroll" Trap**: Tables wrapped in `overflow-x: auto` often clip rightward columns seamlessly, leaving users unaware that additional data exists off-screen unless visual scroll cues are provided.

Modern CSS provides battle-tested architectures to conquer tabular data:
- **CSS Scroll-Driven Gradient Shadows**: Pure CSS affordance shadows that dynamically appear only when horizontal content overflows the viewport.
- **Card Reflow via Attribute Selectors**: Transforming tabular rows into discrete, accessible visual cards on mobile viewports using `data-label` and `attr()`.
- **Two-Dimensional Sticky Pinning**: Pinned `<thead>` headers and persistent freeze-paned primary ID columns (`position: sticky`).
- **Tabular Numerics**: `font-variant-numeric: tabular-nums` guaranteeing vertical alignment of decimal currency points and timestamps.

---

## 2. Core Concepts

### 1. The Scroll Container Wrapper Pattern
The most robust cross-platform table architecture wraps the native `<table>` inside an isolated, scrollable container:
```html
<div class="table-scroll-container" tabindex="0" role="region" aria-label="Transaction history">
  <table>...</table>
</div>
```
- `overflow-x: auto`: Prevents table width from corrupting document-level layouts.
- `tabindex="0"`: Ensures keyboard-only users can focus the container and scroll horizontally using arrow keys.
- `role="region"` + `aria-label`: Informs screen reader users that a scrollable landmark exists.

### 2. The Card Reflow Pattern (`data-label`)
On narrow screens (<640px), horizontal scrolling can become tedious. The Card Reflow pattern resets `tr`, `td`, and `tbody` to `display: block`, stacking each row into a self-contained card. Column headers from `data-label` are injected via pseudo-elements:
```html
<td data-label="Status">Active</td>
```
```css
@media (max-width: 640px) {
  td::before {
    content: attr(data-label);
    font-weight: 600;
  }
}
```

### 3. Fixed vs Auto Table Layout Performance
By default, browsers use `table-layout: auto`, requiring the layout engine to read every cell in every row before calculating column widths. For 1,000+ row tables, `table-layout: fixed` computes column widths strictly from the first row (`<colgroup>` or `<thead>`), accelerating rendering by up to 10x.

---

## 3. How It Works Internally

### The Pure CSS Scroll Shadow Mechanism
Using multiple background layers and `background-attachment`, CSS can reveal scroll shadows automatically without JavaScript:

```
[Left Edge] ────────────────────────────────────────── [Right Edge]
  ▲                                                      ▲
  │ Background Layer 1: Cover Scrim (scroll)             │ Background Layer 2: Cover Scrim (scroll)
  │ (Moves with content to hide shadow at start)         │ (Hides right shadow at end of scroll)
  │                                                      │
  ▼ Background Layer 3: Left Shadow (local)              ▼ Background Layer 4: Right Shadow (local)
  │ (Revealed as soon as user scrolls right)             │ (Visible whenever content overflows)
```

```css
.table-scroll-container {
  overflow-x: auto;
  background:
    /* Scrims to cover shadows when scrolled to edges */
    linear-gradient(to right, white 30%, rgba(255, 255, 255, 0)),
    linear-gradient(to left, white 30%, rgba(255, 255, 255, 0)) 100% 0,
    /* Dynamic drop shadows */
    radial-gradient(farthest-side at 0 50%, rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0)),
    radial-gradient(farthest-side at 100% 50%, rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0)) 100% 0;
  background-repeat: no-repeat;
  background-size: 40px 100%, 40px 100%, 14px 100%, 14px 100%;
  background-attachment: local, local, scroll, scroll;
}
```
When scrolled completely to the left, the local white scrim masks the left shadow. As soon as the user scrolls, the scrim shifts out of the clipping viewport, instantly revealing the underlying shadow to signal off-screen content.

---

## 4. Syntax & Usage

### Baseline Browser Support
| Feature | Baseline Status | Chrome / Edge | Firefox | Safari |
|---|---|---|---|---|
| `position: sticky` on `<th>` / `<td>` | Widely Available | 56+ | 59+ | 13.0+ |
| `table-layout: fixed` | Widely Available | 1+ | 1+ | 1+ |
| `font-variant-numeric: tabular-nums` | Widely Available | 52+ | 34+ | 9.1+ |
| `attr()` Content in Pseudo-Elements | Widely Available | 1+ | 1+ | 1+ |

### Core Boilerplate: High-Performance Data Table
```css
.data-table {
  width: 100%;
  border-collapse: collapse;
  table-layout: fixed; /* Immediate O(1) layout calculation */
  font-size: 0.875rem;
  font-variant-numeric: tabular-nums;
}

.data-table th,
.data-table td {
  padding: 0.75rem 1rem;
  text-align: left;
  border-bottom: 1px solid #e2e8f0;
  vertical-align: middle;
}

.data-table th {
  background-color: #f8fafc;
  font-weight: 600;
  color: #334155;
  position: sticky;
  top: 0;
  z-index: 2;
}

.data-table .col-numeric {
  text-align: right;
}
```

---

## 5. Practical Examples

### Example 1: Horizontal Scroll Table with Pure CSS Dynamic Scroll Shadows
```html
<div class="scrollable-table-wrapper" tabindex="0" role="region" aria-label="System cluster telemetry">
  <table class="telemetry-table">
    <caption>Global Cloud Compute Node Telemetry (Active Clusters)</caption>
    <thead>
      <tr>
        <th scope="col">Cluster ID</th>
        <th scope="col">Region</th>
        <th scope="col">Nodes</th>
        <th scope="col">CPU Load</th>
        <th scope="col">Memory Usage</th>
        <th scope="col">Network I/O</th>
        <th scope="col">Status</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>us-east-prod-01</td>
        <td>Virginia (us-east-1)</td>
        <td>128</td>
        <td>42.4%</td>
        <td>64.1%</td>
        <td>12.4 Gbps</td>
        <td><span class="badge success">Healthy</span></td>
      </tr>
      <tr>
        <td>eu-west-prod-04</td>
        <td>Frankfurt (eu-central-1)</td>
        <td>96</td>
        <td>88.1%</td>
        <td>91.2%</td>
        <td>8.1 Gbps</td>
        <td><span class="badge warning">High Load</span></td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.scrollable-table-wrapper {
  width: 100%;
  overflow-x: auto;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  outline: none;
  
  /* Pure CSS Scroll Affordance Shadows */
  background:
    linear-gradient(to right, #ffffff 30%, rgba(255, 255, 255, 0)),
    linear-gradient(to left, #ffffff 30%, rgba(255, 255, 255, 0)) 100% 0,
    radial-gradient(farthest-side at 0 50%, rgba(15, 23, 42, 0.15), transparent),
    radial-gradient(farthest-side at 100% 50%, rgba(15, 23, 42, 0.15), transparent) 100% 0;
  background-repeat: no-repeat;
  background-size: 40px 100%, 40px 100%, 12px 100%, 12px 100%;
  background-attachment: local, local, scroll, scroll;
}

.scrollable-table-wrapper:focus-visible {
  box-shadow: 0 0 0 2px #2563eb;
}

.telemetry-table {
  width: 100%;
  min-width: 720px; /* Preserve columns from crushing */
  border-collapse: collapse;
  font-family: system-ui, sans-serif;
  font-size: 0.875rem;
}

.telemetry-table caption {
  padding: 0.75rem 1rem;
  font-weight: 600;
  text-align: left;
  background: #ffffff;
  color: #0f172a;
  border-bottom: 1px solid #e2e8f0;
}

.telemetry-table th,
.telemetry-table td {
  padding: 0.75rem 1rem;
  text-align: left;
  border-bottom: 1px solid #f1f5f9;
}

.telemetry-table th {
  background: #f8fafc;
  color: #475569;
  font-weight: 600;
  white-space: nowrap;
}

.telemetry-table tr:hover {
  background-color: #f8fafc;
}

.badge {
  display: inline-block;
  padding: 0.2rem 0.5rem;
  border-radius: 4px;
  font-size: 0.75rem;
  font-weight: 600;
}

.badge.success { background: #dcfce7; color: #15803d; }
.badge.warning { background: #fef9c3; color: #a16207; }
```

---

### Example 2: Card Reflow Layout for Mobile Viewports
```html
<table class="card-reflow-table">
  <caption>Recent Customer Invoices</caption>
  <thead>
    <tr>
      <th scope="col">Invoice #</th>
      <th scope="col">Customer</th>
      <th scope="col">Issue Date</th>
      <th scope="col">Amount</th>
      <th scope="col">Status</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td data-label="Invoice #">INV-2026-081</td>
      <td data-label="Customer">Acme Corporation</td>
      <td data-label="Issue Date">Mar 28, 2026</td>
      <td data-label="Amount" class="col-numeric">$12,450.00</td>
      <td data-label="Status"><span class="status-pill paid">Paid</span></td>
    </tr>
    <tr>
      <td data-label="Invoice #">INV-2026-082</td>
      <td data-label="Customer">Cyberdyne Systems</td>
      <td data-label="Issue Date">Mar 29, 2026</td>
      <td data-label="Amount" class="col-numeric">$4,800.00</td>
      <td data-label="Status"><span class="status-pill pending">Pending</span></td>
    </tr>
  </tbody>
</table>
```

```css
.card-reflow-table {
  width: 100%;
  border-collapse: collapse;
  font-family: inherit;
  font-variant-numeric: tabular-nums;
}

.card-reflow-table caption {
  font-size: 1.125rem;
  font-weight: 700;
  text-align: left;
  margin-bottom: 1rem;
}

.card-reflow-table th,
.card-reflow-table td {
  padding: 0.75rem 1rem;
  text-align: left;
  border-bottom: 1px solid #e2e8f0;
}

.card-reflow-table th {
  background: #f1f5f9;
  font-weight: 600;
}

.col-numeric {
  text-align: right;
}

.status-pill {
  padding: 0.2rem 0.6rem;
  border-radius: 9999px;
  font-size: 0.75rem;
  font-weight: 600;
}
.status-pill.paid { background: #dcfce7; color: #166534; }
.status-pill.pending { background: #fef3c7; color: #92400e; }

/* Responsive Transformation to Cards */
@media (max-width: 640px) {
  .card-reflow-table thead {
    display: none; /* Hide visual header table row */
  }

  .card-reflow-table,
  .card-reflow-table tbody,
  .card-reflow-table tr,
  .card-reflow-table td {
    display: block;
    width: 100%;
  }

  .card-reflow-table tr {
    background: #ffffff;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    margin-bottom: 1rem;
    padding: 0.5rem 1rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
  }

  .card-reflow-table td {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.5rem 0;
    border-bottom: 1px solid #f1f5f9;
  }

  .card-reflow-table td:last-child {
    border-bottom: none;
  }

  /* Inject mobile label via data-label attribute */
  .card-reflow-table td::before {
    content: attr(data-label);
    font-weight: 600;
    color: #64748b;
    font-size: 0.8125rem;
    text-transform: uppercase;
    letter-spacing: 0.025em;
  }
}
```

---

### Example 3: Freeze Panes: Sticky Header + Sticky First Column
```html
<div class="freeze-pane-viewport" tabindex="0">
  <table class="matrix-table">
    <thead>
      <tr>
        <th class="sticky-freeze-corner" scope="col">Service Name</th>
        <th scope="col">Latency (p50)</th>
        <th scope="col">Latency (p95)</th>
        <th scope="col">Latency (p99)</th>
        <th scope="col">Throughput (RPS)</th>
        <th scope="col">Error %</th>
        <th scope="col">Uptime</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <th class="sticky-col-header" scope="row">Auth Gateway</th>
        <td>12ms</td>
        <td>28ms</td>
        <td>45ms</td>
        <td>14,500</td>
        <td>0.002%</td>
        <td>99.999%</td>
      </tr>
      <tr>
        <th class="sticky-col-header" scope="row">Payment API</th>
        <td>45ms</td>
        <td>110ms</td>
        <td>240ms</td>
        <td>3,200</td>
        <td>0.010%</td>
        <td>99.995%</td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.freeze-pane-viewport {
  max-height: 400px;
  max-width: 100%;
  overflow: auto;
  border: 1px solid #cbd5e1;
  position: relative;
}

.matrix-table {
  border-collapse: separate; /* Required for sticky borders */
  border-spacing: 0;
  width: 100%;
  min-width: 800px;
  font-size: 0.875rem;
}

.matrix-table th,
.matrix-table td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e2e8f0;
  border-right: 1px solid #e2e8f0;
  background-color: #ffffff;
}

/* 1. Freeze Header Row to Top */
.matrix-table thead th {
  position: sticky;
  top: 0;
  background-color: #f1f5f9;
  z-index: 2;
  font-weight: 600;
}

/* 2. Freeze First Column to Left */
.sticky-col-header {
  position: sticky;
  left: 0;
  background-color: #f8fafc !important;
  z-index: 1;
  text-align: left;
  font-weight: 600;
  border-right: 2px solid #cbd5e1 !important;
}

/* 3. Freeze Top-Left Corner Intersection */
.sticky-freeze-corner {
  position: sticky;
  top: 0;
  left: 0;
  background-color: #e2e8f0 !important;
  z-index: 3; /* Must be highest to render over row & col headers */
  border-right: 2px solid #94a3b8 !important;
}
```

---

### Example 4: Progressive Column Collapse via Container Queries
```html
<div class="adaptive-table-card">
  <table class="adaptive-table">
    <thead>
      <tr>
        <th scope="col">User</th>
        <th scope="col">Role</th>
        <th scope="col" class="col-priority-low">Last Login</th>
        <th scope="col" class="col-priority-low">MFA State</th>
        <th scope="col">Status</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td><strong>Jane Doe</strong><span class="subtext">jane@corp.com</span></td>
        <td>Admin</td>
        <td class="col-priority-low">10 mins ago</td>
        <td class="col-priority-low">Enabled</td>
        <td><span class="badge-active">Active</span></td>
      </tr>
    </tbody>
  </table>
</div>
```

```css
.adaptive-table-card {
  container-type: inline-size;
  container-name: tablebox;
  width: 100%;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  overflow: hidden;
}

.adaptive-table {
  width: 100%;
  border-collapse: collapse;
}

.adaptive-table th,
.adaptive-table td {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #f1f5f9;
  text-align: left;
}

.subtext {
  display: block;
  font-size: 0.75rem;
  color: #64748b;
}

/* Container query hides auxiliary columns when card width drops */
@container tablebox (max-width: 580px) {
  .col-priority-low {
    display: none;
  }
}
```

---

### Example 5: High-Density Financial Ledger with Tabular Figures
```html
<table class="ledger-table">
  <caption>Q1 Fiscal Operations Ledger</caption>
  <thead>
    <tr>
      <th scope="col">Tx Date</th>
      <th scope="col">Reference</th>
      <th scope="col" class="num">Debit</th>
      <th scope="col" class="num">Credit</th>
      <th scope="col" class="num">Balance</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>2026-03-01</td>
      <td>Payroll Processing #882</td>
      <td class="num debit">-$45,000.00</td>
      <td class="num credit">—</td>
      <td class="num">$1,240,500.00</td>
    </tr>
    <tr>
      <td>2026-03-03</td>
      <td>Client Wire Deposit #104</td>
      <td class="num debit">—</td>
      <td class="num credit">+$120,000.00</td>
      <td class="num">$1,360,500.00</td>
    </tr>
  </tbody>
</table>
```

```css
.ledger-table {
  width: 100%;
  border-collapse: collapse;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  font-variant-numeric: tabular-nums lining-nums;
  font-size: 0.8125rem;
}

.ledger-table caption {
  font-family: system-ui, sans-serif;
  font-weight: 700;
  text-align: left;
  margin-bottom: 0.5rem;
}

.ledger-table th,
.ledger-table td {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid #e2e8f0;
}

.ledger-table th {
  background: #f8fafc;
  color: #334155;
  font-weight: 600;
}

.ledger-table .num {
  text-align: right;
  white-space: nowrap;
}

.num.debit { color: #dc2626; }
.num.credit { color: #16a34a; }
```

---

## 6. Best Practices

1. **Always Use `table-layout: fixed` for Large Datasets**: Drastically reduces layout calculation overhead and eliminates unpredictable column width oscillations when data rows stream in dynamically.
2. **Make Scrollable Tables Keyboard Accessible**: Ensure any container with `overflow-x: auto` includes `tabindex="0"`, a `role="region"`, and an `aria-label` so keyboard-only and screen reader users can interact with the table.
3. **Align Numeric Data to the Right**: Numbers, currency amounts, and percentages should always align right (`text-align: right; font-variant-numeric: tabular-nums`) so decimal points line up vertically for rapid visual scanning.
4. **Use Semantic Table Landmarks (`<caption>`, `<thead>`, `<tbody>`, `<th>`)**: Never replace table markup with generic `<div>` grids unless ARIA table roles are manually attached. Provide `scope="col"` on header cells and `scope="row"` on row leaders.
5. **Freeze the Corner Cell When Using Dual Sticky Axes**: If both `<thead>` and the first column are sticky, the top-left intersection cell must declare a higher `z-index` (e.g. `z-index: 3`) to prevent visual overlap glitching.
6. **Provide Visual Scroll Cues**: Never allow a table to scroll horizontally without visual affordances (such as CSS gradient scroll shadows or custom edge fades) to signal clipped off-screen data.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom | Correct Architecture |
|---|---|---|---|
| **Whole Page Horizontal Scroll** | Table has fixed wide width without parent `overflow-x: auto` | Mobile phone user must scroll entire page horizontally | Wrap table in a container with `overflow-x: auto` |
| **Stripping Accessibility in Card Reflow** | Setting `display: block` on table tags in WebKit | Safari screen reader fails to recognize table headers and rows | Retain table semantics or add explicit `role="table"` |
| **Sticky Header Border Disappearing** | Using `border-collapse: collapse` with `position: sticky` | Header cell borders flicker, vanish, or fail to follow scrolling | Use `border-collapse: separate; border-spacing: 0;` |
| **Jagged Currency Alignment** | Standard proportional fonts on numeric columns | Digits `1` and `8` have different widths; decimals don't align | Apply `font-variant-numeric: tabular-nums; text-align: right;` |
| **Keyboard Trapped Container** | Overflow container lacks `tabindex="0"` | Keyboard users cannot arrow-scroll across clipped table columns | Add `tabindex="0"` and `role="region"` to the scroll wrapper |
| **Unanchored Sticky Corner** | Top-left cell lacks `z-index: 3` | Left column headers slide over top row headers during diagonal scroll | Assign `z-index: 3` to the corner cell |

---

## 8. Performance, Accessibility & Browser Support

### Layout Performance
- **`table-layout: fixed`**: In `auto` mode, the browser must read the content of all rows and cells before calculating track geometry. In `fixed` mode, track widths are determined solely by the width of the table and the explicit widths of the first row (`<colgroup>` or `<th>`), resulting in instantaneous $O(1)$ rendering.
- **Scroll Layer Promotion**: Applying `contain: paint` or `overflow-x: auto` isolates the table scroll container into a compositor layer, preventing internal row hover states from triggering full-page repaints.

### Accessibility Standards
- **ARIA Restoration**: When applying CSS card reflow (`display: block` on `<table>`, `<tr>`, `<td>`), some browser versions strip table accessibility semantics. Adding explicit fallback roles (`role="table"`, `role="rowgroup"`, `role="row"`, `role="cell"`) ensures universal compatibility:
  ```html
  <table role="table">
    <tbody role="rowgroup">
      <tr role="row">
        <td role="cell" data-label="Amount">$100</td>
      </tr>
    </tbody>
  </table>
  ```

---

## 9. Comparison: Table Responsiveness Patterns

| Feature | Scroll Container + Shadows | Card Reflow (`data-label`) | Collapsible Priority Columns | Native Div Grid |
|---|---|---|---|---|
| **Data Preservation** | 100% (All data retained) | 100% (All data retained) | Low-priority data hidden | 100% |
| **Mobile Ergonomics** | Moderate (Requires swipe) | High (Natural vertical flow) | High (Clean compact display) | Moderate |
| **Accessibility Semantics**| Native HTML Table | Requires ARIA role vigilance | Native HTML Table | Requires manual ARIA |
| **Engineering Complexity**| Very Low (Pure CSS) | Moderate (Requires `data-label`)| Low (Container Queries) | High |
| **Best For** | High-density matrices | Invoices, receipts, user cards | Status dashboards | Custom interactive cards |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does `border-collapse: collapse` cause issues with `position: sticky` on table headers, and how is it resolved?</strong></summary>

**Answer:**
Under `border-collapse: collapse`, neighboring cells share a single merged border. When `position: sticky` is applied to `<th>` cells, the browser cannot determine whether the bottom border belongs to the sticky header or the scrolling body cell beneath it. Consequently, sticky headers often suffer from missing, duplicated, or flickering borders during scroll.

To resolve this:
```css
.sticky-table {
  border-collapse: separate; /* Keep cell border models isolated */
  border-spacing: 0;         /* Eliminate gaps between cells */
}
.sticky-table th {
  position: sticky;
  top: 0;
  border-bottom: 2px solid #cbd5e1;
}
```
</details>

<details>
<summary><strong>2. Explain how the Pure CSS Scroll Shadow pattern works without JavaScript.</strong></summary>

**Answer:**
The scroll shadow pattern combines two distinct background attachment models:
1. `background-attachment: local`: The background image moves *with* the scrolling content.
2. `background-attachment: scroll`: The background image remains *stationary* relative to the container viewport.

By layering white gradient masks with `local` attachment over darker radial drop-shadows with `scroll` attachment:
- At scroll offset 0, the local white mask covers the left radial shadow.
- As the user scrolls right, the local mask scrolls away with the content, revealing the stationary left drop-shadow.
- At maximum scroll offset, the right local mask covers the right radial shadow, automatically extinguishing the right affordance cue.
</details>

<details>
<summary><strong>3. Why does setting `display: block` on table elements harm accessibility in some browsers?</strong></summary>

**Answer:**
Historically, web authors abused `<table>` elements for general page layout. To prevent screen readers from reading layout tables as data matrices, browser accessibility engines (notably Apple's WebKit in Safari) implement a heuristic: if an author overrides standard table styling with `display: block` or `display: flex`, the engine assumes the element is a layout hack and strips its table semantics (`role="table"`, `role="row"`, `role="cell"`).

To preserve accessibility when executing card reflow patterns, authors should explicitly reinforce semantic roles in HTML: `<table role="table">`, `<tr role="row">`, `<td role="cell">`.
</details>

<details>
<summary><strong>4. What is the operational difference between `table-layout: auto` and `table-layout: fixed`?</strong></summary>

**Answer:**
- `table-layout: auto`: The browser must download and evaluate the content of every single cell in every row across the entire table before computing column widths. For tables with 10,000 cells, this introduces substantial CPU latency and memory pressure during DOM parsing.
- `table-layout: fixed`: The browser calculates column widths strictly based on the container width and the declared widths of the first row of cells (or `<col>` elements). Cell content in subsequent rows cannot alter column widths. The browser renders rows immediately as they stream over the network, achieving massive performance gains.
</details>

<details>
<summary><strong>5. How does `font-variant-numeric: tabular-nums` enhance data readability?</strong></summary>

**Answer:**
Standard proportional typefaces assign varying horizontal widths to glyphs (e.g. `1` is much narrower than `8` or `0`). When rendering columns of numbers or monetary values, proportional numbers wiggle horizontally, making it difficult for the human eye to compare orders of magnitude.

`font-variant-numeric: tabular-nums` forces the font engine to render all numeric digits with identical, monospaced character bounding widths (tabular figures) while keeping standard proportional spacing for alphabetic characters.
</details>

<details>
<summary><strong>6. Why is `tabindex="0"` required on table scroll wrappers?</strong></summary>

**Answer:**
When a `<div>` with `overflow: auto` overflows horizontally, sighted mouse and touchscreen users can drag or swipe the table. However, keyboard-only users cannot interact with or scroll the table unless the overflow container can receive keyboard focus. Adding `tabindex="0"` allows users to press <kbd>Tab</kbd> to enter the container and scroll left/right using arrow keys.
</details>

<details>
<summary><strong>7. How do you implement the mobile Card Reflow pattern using `attr(data-label)`?</strong></summary>

**Answer:**
1. In HTML, assign `data-label="Column Name"` to every `<td>`.
2. Inside a `@media (max-width: 640px)` query, set `display: none` on `<thead>`.
3. Set `display: block` on `<tr>` and `<td>`.
4. Inject the label before cell content: `td::before { content: attr(data-label); font-weight: bold; }`.
5. Format `<td>` with `display: flex; justify-content: space-between;` to produce clean label-value card rows.
</details>

<details>
<summary><strong>8. How should high contrast mode (`forced-colors: active`) be styled on alternating table rows?</strong></summary>

**Answer:**
In High Contrast Mode, zebra striping backgrounds (`nth-child(even) { background: #f8fafc; }`) are discarded by the operating system. To ensure readability:
- Apply visible borders between rows: `border-bottom: 1px solid CanvasText;`.
- Use explicit cell focus rings: `td:focus-visible { outline: 2px solid Highlight; }`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Scroll-Shadow Table Wrapper
**Task**: Construct an overflow wrapper that displays a subtle right-side gradient shadow when content overflows horizontally, extinguishing the shadow when scrolled to the far right.
<details>
<summary>View Verified Solution</summary>

```css
.table-shadow-wrap {
  width: 100%;
  overflow-x: auto;
  background:
    linear-gradient(to right, white 30%, transparent),
    linear-gradient(to left, white 30%, transparent) 100% 0,
    radial-gradient(farthest-side at 0 50%, rgba(0, 0, 0, 0.2), transparent),
    radial-gradient(farthest-side at 100% 50%, rgba(0, 0, 0, 0.2), transparent) 100% 0;
  background-repeat: no-repeat;
  background-size: 40px 100%, 40px 100%, 14px 100%, 14px 100%;
  background-attachment: local, local, scroll, scroll;
}
```
</details>

---

### Exercise 2: Freeze the First Column of a Scrollable Table
**Task**: In an `overflow-x: auto` table, configure the first column (`<th>`) to remain pinned to the left edge while subsequent columns scroll behind it.
<details>
<summary>View Verified Solution</summary>

```css
.freeze-first-col {
  border-collapse: separate;
  border-spacing: 0;
  width: 100%;
}

.freeze-first-col th:first-child,
.freeze-first-col td:first-child {
  position: sticky;
  left: 0;
  background-color: #ffffff;
  z-index: 1;
  border-right: 2px solid #cbd5e1;
}
```
</details>

---

### Exercise 3: Zebra-Striped Accessible Table with Hover State
**Task**: Create an accessible table with alternating row colors and a distinct hover highlight that maintains WCAG AA contrast.
<details>
<summary>View Verified Solution</summary>

```css
.zebra-table {
  width: 100%;
  border-collapse: collapse;
}

.zebra-table tbody tr:nth-child(even) {
  background-color: #f8fafc;
}

.zebra-table tbody tr:hover {
  background-color: #f1f5f9;
}

.zebra-table td,
.zebra-table th {
  padding: 0.75rem 1rem;
  border-bottom: 1px solid #e2e8f0;
}
```
</details>

---

### Exercise 4: Tabular Numeric Decimal Alignment
**Task**: Style monetary cells in an invoice table so that all currency symbols, digits, and decimal points line up vertically.
<details>
<summary>View Verified Solution</summary>

```css
.currency-cell {
  text-align: right;
  font-family: system-ui, -apple-system, sans-serif;
  font-variant-numeric: tabular-nums lining-nums;
  white-space: nowrap;
}
```
</details>

---

### Exercise 5: Text Truncation inside Fixed Table Cells
**Task**: In a `table-layout: fixed` table, ensure long user email strings truncate cleanly with an ellipsis without expanding the table column.
<details>
<summary>View Verified Solution</summary>

```css
.fixed-table {
  table-layout: fixed;
  width: 100%;
}

.truncate-cell {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
```
</details>

---

### Exercise 6: Pure CSS Mobile Card Transformation
**Task**: Transform a standard 3-column table into single-column vertical cards on viewports under 500px using pseudo-elements.
<details>
<summary>View Verified Solution</summary>

```css
@media (max-width: 500px) {
  .mobile-cards thead {
    display: none;
  }
  .mobile-cards,
  .mobile-cards tbody,
  .mobile-cards tr,
  .mobile-cards td {
    display: block;
    width: 100%;
  }
  .mobile-cards tr {
    margin-bottom: 1rem;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    padding: 0.75rem;
  }
  .mobile-cards td {
    display: flex;
    justify-content: space-between;
    padding: 0.35rem 0;
  }
  .mobile-cards td::before {
    content: attr(data-label);
    font-weight: 600;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+------------------------------------------------------------------------------------+
|                         RESPONSIVE TABLES CHEAT SHEET                              |
+------------------------------------------------------------------------------------+
| 1. High-Performance Fixed Layout:                                                  |
|    table { width: 100%; table-layout: fixed; border-collapse: separate; }          |
+------------------------------------------------------------------------------------+
| 2. Sticky Header Setup:                                                            |
|    border-collapse: separate; border-spacing: 0;                                    |
|    thead th { position: sticky; top: 0; z-index: 2; }                              |
+------------------------------------------------------------------------------------+
| 3. Sticky Freeze Corner:                                                           |
|    .freeze-corner { position: sticky; top: 0; left: 0; z-index: 3; }                |
+------------------------------------------------------------------------------------+
| 4. Keyboard Accessible Scroll Wrapper:                                             |
|    <div tabindex="0" role="region" aria-label="Data Table">...</div>               |
+------------------------------------------------------------------------------------+
| 5. Tabular Digits:                                                                 |
|    td.num { text-align: right; font-variant-numeric: tabular-nums; }               |
+------------------------------------------------------------------------------------+
| 6. Card Reflow Content Injection:                                                  |
|    td::before { content: attr(data-label); font-weight: 600; }                      |
+------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [W3C HTML5 Table Model Specification](https://html.spec.whatwg.org/multipage/tables.html)
- [MDN Web Docs: `table-layout`](https://developer.mozilla.org/en-US/docs/Web/CSS/table-layout)
- [MDN Web Docs: `font-variant-numeric`](https://developer.mozilla.org/en-US/docs/Web/CSS/font-variant-numeric)
- [Lea Verou: Pure CSS Scroll Shadows with `background-attachment`](https://lea.verou.me/blog/2012/04/background-attachment-local/)
- [Adrian Roselli: Accessible Responsive Tables](https://adrianroselli.com/2020/11/under-engineered-responsive-tables.html)

---

## Navigation
- **Previous**: [Form Layouts and Input Styling](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/09-forms-layouts.md)
- **Next**: [Tabs, Accordions, and Toggles](file:///e:/Learning/engineering-notes/01-languages/css/13-layout-and-component-recipes/11-tabs-accordions-and-toggles.md)
