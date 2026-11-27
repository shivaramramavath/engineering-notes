# Accessible Tables & Lists

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `02-semantics-and-structure/06-accessible-data-tables.md`, `01-fundamentals/04-lists-and-grouping.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (HTML Table & List Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver (macOS / iOS) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What are Accessible Tables and Lists?
Tables and lists are structural grouping mechanisms used to convey two-dimensional data relationships and hierarchical collections of items. When coded with semantic HTML:
- Screen readers announce the total row and column dimensions (*"Table with 5 rows and 4 columns"*).
- Screen readers automatically announce the corresponding column and row header labels as the user navigates across individual cells using two-dimensional arrow keys.
- Screen readers announce list item counts and positions (*"List of 6 items, item 1 of 6"*).

### The Accessibility Stakes (WCAG 1.3.1)
Under **WCAG Success Criterion 1.3.1 (Info and Relationships - Level A)**:
> *"Information, structure, and relationships conveyed through presentation can be programmatically determined or are available in text."*

When developers build pseudo-tables out of `<div>` tags or fail to define header scopes (`<th scope="col">`), the two-dimensional relationships are completely severed in the Accessibility Tree. A blind user hearing *"1,420"* has no way of knowing whether that number represents annual profit, employee count, or product weight.

---

## 2. Core Concepts

### Data Tables vs Layout Tables
1. **Data Tables**: Used exclusively to present tabular data with row and column headers. Must use semantic table tags (`<table>`, `<caption>`, `<thead>`, `<tbody>`, `<th>`, `<td>`).
2. **Layout Tables**: Historical artifact from the 1990s where `<table>` was used for multi-column page positioning. If legacy layout tables exist, they must be stripped of semantics using `role="presentation"` or `role="none"` so screen readers treat them as plain generic containers.

### The Role of `<caption>`
The `<caption>` element is the title of the table. Placed as the immediate first child of `<table>`, it serves as the table's **Accessible Name**:
```html
<table>
  <caption>Quarterly Regional Sales Performance (2025)</caption>
  <!-- thead and tbody -->
</table>
```

### Associating Headers with Data Cells
- **Simple Tables**: Use the `scope` attribute on `<th>` elements:
  - `<th scope="col">`: Header applies to the entire column below it.
  - `<th scope="row">`: Header applies to the entire row to its right.
- **Complex Multi-Tier Tables**: When headers span multiple rows or columns (e.g., grouped sub-headers), use explicit `id` attributes on headers and reference them using the `headers` attribute on data cells:
  `<td headers="year-2025 q3 profit">\$42,000</td>`

### The Infamous WebKit `list-style: none` Quirk
In Apple Safari (WebKit), applying CSS `list-style: none` to `<ul>` or `<ol>` elements **causes the browser to strip list semantics from the Accessibility Tree**. Safari assumes that because the author removed the bullet points, the element is being used as a layout container rather than a semantic list. To restore list semantics in Safari, authors must explicitly add `role="list"`:

```html
<ul class="clean-list" role="list">
  <li>Item One</li>
  <li>Item Two</li>
</ul>
```

---

## 3. How It Works Internally

### Screen Reader 2D Grid Traversal
Screen readers switch into **Table Navigation Mode** when entering a semantic `<table>`:

```
User navigates Table using [Ctrl + Alt + Arrow Keys] (NVDA/JAWS)
                              │
                              ▼
        User moves cursor Right from Cell (1,1) to Cell (1,2)
                              │
                              ▼
         Accessibility Engine (AOM) queries cell coordinates:
         - New Cell: <td>$42,000</td>
         - Column Header: <th scope="col">Net Profit</th>
         - Row Header:    <th scope="row">Q3 2025</th>
                              │
                              ▼
               Screen Reader Speech Output:
    "Net Profit, Q3 2025, column 2, row 3: $42,000"
```

If `<th scope="col">` is replaced with `<td><b>Net Profit</b></td>`, the browser treats the header as plain data. When the user moves between cells, the screen reader announces only *"\$42,000"*, forcing the user to manually backtrack to row 1 to remind themselves what the column represented.

---

## 4. Syntax & Usage

### Accessible Table Syntax Skeleton

```html
<table>
  <!-- Mandatory accessible title -->
  <caption>Employee Department Roster</caption>
  
  <thead>
    <tr>
      <th scope="col">Employee Name</th>
      <th scope="col">Department</th>
      <th scope="col">Status</th>
    </tr>
  </thead>
  
  <tbody>
    <tr>
      <!-- Row header identifies the record -->
      <th scope="row">Sarah Connor</th>
      <td>Engineering</td>
      <td>Active</td>
    </tr>
    <tr>
      <th scope="row">John Smith</th>
      <td>Product</td>
      <td>On Leave</td>
    </tr>
  </tbody>
</table>
```

---

## 5. Practical Examples

### Example 1: Basic — Responsive Scrollable Table Wrapper
Tables often overflow on narrow mobile screens. Wrapping a table in an overflow container requires making that container keyboard-focusable (`tabindex="0"`) and labeled, so keyboard users can scroll it:

```html
<style>
  .table-scroll-container {
    overflow-x: auto;
    border: 1px solid #ccc;
    max-width: 100%;
  }
  .table-scroll-container:focus-visible {
    outline: 3px solid #005a9c;
  }
</style>

<!-- WCAG 2.1.1 (Keyboard) Compliance: tabindex="0" makes overflow region scrollable -->
<div 
  class="table-scroll-container" 
  tabindex="0" 
  role="region" 
  aria-labelledby="roster-caption"
>
  <table>
    <caption id="roster-caption">Global Server Fleet Capacity</caption>
    <thead>
      <tr>
        <th scope="col">Region</th>
        <th scope="col">Active Nodes</th>
        <th scope="col">CPU Utilization</th>
        <th scope="col">Memory Usage</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <th scope="row">us-east-1</th>
        <td>128</td>
        <td>42%</td>
        <td>68%</td>
      </tr>
      <tr>
        <th scope="row">eu-west-1</th>
        <td>64</td>
        <td>58%</td>
        <td>72%</td>
      </tr>
    </tbody>
  </table>
</div>
```

### Example 2: Real-World — Multi-Tier Complex Header Table using `id` and `headers`
When a table contains multi-level column headers:

```html
<table>
  <caption>2024-2025 Enterprise Hardware Procurement</caption>
  <thead>
    <tr>
      <th id="hdr-category" rowspan="2" scope="col">Category</th>
      <th id="hdr-2024" colspan="2" scope="colgroup">Fiscal Year 2024</th>
      <th id="hdr-2025" colspan="2" scope="colgroup">Fiscal Year 2025</th>
    </tr>
    <tr>
      <th id="hdr-2024-qty" scope="col">Units</th>
      <th id="hdr-2024-cost" scope="col">Cost (USD)</th>
      <th id="hdr-2025-qty" scope="col">Units</th>
      <th id="hdr-2025-cost" scope="col">Cost (USD)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th id="row-servers" scope="row">Blade Servers</th>
      <td headers="hdr-category row-servers hdr-2024 hdr-2024-qty">40</td>
      <td headers="hdr-category row-servers hdr-2024 hdr-2024-cost">$240,000</td>
      <td headers="hdr-category row-servers hdr-2025 hdr-2025-qty">55</td>
      <td headers="hdr-category row-servers hdr-2025 hdr-2025-cost">$330,000</td>
    </tr>
  </tbody>
</table>
```

### Example 3: Edge Case — Resolving the Safari `list-style: none` Bug
Using CSS reset styles often inadvertently strips list announcements in VoiceOver on iOS and macOS:

```html
<style>
  /* Removing default bullets triggers WebKit heuristic stripping list semantics */
  .nav-pills {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    gap: 12px;
  }
</style>

<!-- Adding role="list" forces WebKit to retain list semantics in the AOM! -->
<ul class="nav-pills" role="list">
  <li><a href="/tab1">Overview</a></li>
  <li><a href="/tab2">Security</a></li>
  <li><a href="/tab3">Billing</a></li>
</ul>
```

### Example 4: Failure Case — Inaccessible Pseudo-Table Anti-Pattern
Constructing a table with `<div>` tags without grid ARIA roles destroys data comprehension:

```html
<!-- BROKEN: Fails WCAG 1.3.1 (Info and Relationships) -->
<div class="div-table">
  <div class="div-row header">
    <div class="div-cell">Product</div>
    <div class="div-cell">Price</div>
  </div>
  <div class="div-row">
    <div class="div-cell">Enterprise License</div>
    <div class="div-cell">$4,999</div>
  </div>
</div>
<!-- Screen readers treat this as plain paragraphs. 
     Two-dimensional relationship between "Enterprise License" and "$4,999" is lost! -->
```

### Example 5: Accessible Production-Grade — Definition List for Metadata Key-Value Pairs
Using `<dl>`, `<dt>`, and `<dd>` for semantic key-value metadata:

```html
<section class="invoice-details">
  <h2>Invoice #INV-2026-904</h2>
  
  <dl class="metadata-grid">
    <dt>Issued Date</dt>
    <dd><time datetime="2026-03-15">March 15, 2026</time></dd>

    <dt>Payment Status</dt>
    <dd><span class="badge-paid">Paid</span></dd>

    <dt>Billed To</dt>
    <dd>Acme Industrial Corporation</dd>

    <dt>Total Amount</dt>
    <dd><strong>$14,250.00 USD</strong></dd>
  </dl>
</section>
```

---

## 6. Best Practices

1. **Always Include a `<caption>` for Data Tables**: The caption serves as the accessible title and explains the table's purpose.
2. **Assign `scope="col"` or `scope="row"` to Every `<th>`**: Explicit scopes guarantee correct column and row header association in 2D traversal.
3. **Use `<th scope="row">` for Row Identifiers**: The first cell of each data row should be a `<th scope="row">` if it identifies the record (e.g., the person's name or product ID).
4. **Apply `role="list"` on Styled Lists**: When applying `list-style: none` in CSS, add `role="list"` to `<ul>` or `<ol>` to prevent Apple Safari from stripping list semantics.
5. **Make Overflow Table Wrappers Focusable**: Give horizontal scrolling containers `tabindex="0"` and `role="region"` so keyboard users can scroll through wide tables.
6. **Use Semantic `<dl>`, `<dt>`, `<dd>` for Key-Value Pairs**: Avoid building key-value pairs with generic `<div>` tags; definition lists are natively mapped to description lists in modern screen readers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Building tables with `<div>` tags without ARIA | Critical | Strips all 2D navigation and header relationships | Use semantic `<table>` elements |
| Omitting `scope="col"` and `scope="row"` on `<th>` | High | Screen reader cannot reliably identify header associations | Add explicit `scope` attributes |
| Removing bullets without `role="list"` in WebKit | High | Safari VoiceOver loses list item count announcements | Add `role="list"` to `<ul>` and `<ol>` |
| Unfocusable horizontal scroll container | High | Keyboard users cannot view clipped table columns | Add `tabindex="0"` to scrolling container |
| Using `<table>` for visual layout without ARIA | Medium | Screen readers announce meaningless rows and cols | Add `role="presentation"` or migrate to CSS Grid |
| Putting interactive buttons inside `<th>` without labels | Medium | Header association is announced as a button action | Wrap button cleanly inside header cell |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility Engine Table Rendering
Massive data tables (e.g., 5,000 rows by 30 columns) create over 150,000 DOM nodes. Every DOM mutation triggers an expensive recalculation of the Accessibility Tree's grid matrix. In enterprise web applications, use **virtualized tables** (rendering only visible rows in the viewport) combined with WAI-ARIA `aria-rowcount`, `aria-colcount`, and `aria-rowindex` to maintain 60 FPS performance while accurately reporting total dataset size.

### SEO Synergy
Search engines (Googlebot) read HTML `<table>` tags directly to generate rich SERP features, including **Featured Snippet Comparison Tables** and knowledge graph entity data. Tables with valid `<caption>` and `<th>` elements achieve significantly higher indexing prominence.

### Security Notes
When rendering tabular data from untrusted sources (e.g., user CSV uploads), ensure that all cell values are strictly escaped to prevent Cross-Site Scripting (XSS) and formula injection attacks (CSV Injection / Formula Injection).

---

## 9. Comparison / When to Use What

### Table vs List vs Definition List

| Content Type | Semantic Element | AOM Role | Best Used For |
|:---|:---|:---|:---|
| **2D Relational Data** | `<table>` | `table` / `grid` | Financial reports, rosters, comparison matrices |
| **Ordered Sequence** | `<ol>` | `list` | Step-by-step guides, ranked leaderboards |
| **Unordered Set** | `<ul>` | `list` | Navigation menus, feature bullet lists |
| **Key-Value Pairs** | `<dl>` + `<dt>` + `<dd>` | `description-list` | Metadata, glossaries, item specifications |
| **Visual Layout Columns**| CSS Grid / Flexbox | Generic `<div>` | Page columns, hero split sections |

---

## 10. Interview Questions

### Q1: What is the purpose of the `scope` attribute on `<th>` elements?
<details>
<summary>View Answer</summary>

The `scope` attribute explicitly specifies whether a table header cell applies to a **column** (`scope="col"`), a **row** (`scope="row"`), a group of columns (`scope="colgroup"`), or a group of rows (`scope="rowgroup"`).

This allows browser accessibility engines to associate data cells (`<td>`) with their appropriate header titles, enabling screen readers to automatically speak the header name whenever the user navigates across cells using table navigation shortcuts.
</details>

### Q2: What is the WebKit `list-style: none` bug, and how is it resolved?
<details>
<summary>View Answer</summary>

In Apple Safari (WebKit engine), if an author applies CSS `list-style: none` to a `<ul>` or `<ol>` element, WebKit's accessibility heuristics assume the list is being used for visual layout rather than semantic content, and strips the `list` role from the Accessibility Tree. VoiceOver will no longer announce the item count or position.

The industry-standard workaround is to explicitly add **`role="list"`** to the `<ul>` or `<ol>` element:
`<ul style="list-style: none;" role="list">...</ul>`
This forces WebKit to retain full list semantics in the AOM.
</details>

### Q3: How should a table with multi-tier, complex spanning headers be made accessible?
<details>
<summary>View Answer</summary>

Complex tables with nested or multi-level headers cannot always be resolved using `scope` alone. 

To make them accessible:
1. Assign unique `id` attributes to every header cell (`<th id="h-2025">`, `<th id="h-q1">`, `<th id="h-sales">`).
2. On every data cell (`<td>`), use the **`headers`** attribute containing a space-separated list of IDs identifying all applicable parent headers:
`<td headers="h-2025 h-q1 h-sales">$150,000</td>`
Screen readers traverse this list to announce the complete hierarchical context for each cell.
</details>

### Q4: Why is wrapping a wide table in a `<div>` with `overflow-x: auto` an accessibility hazard if `tabindex="0"` is omitted?
<details>
<summary>View Answer</summary>

A container with `overflow-x: auto` requires scrolling to view clipped content. Sighted mouse users can click and drag scrollbars. 

However, **keyboard-only users cannot scroll an element unless that element can receive keyboard focus**. If `tabindex="0"` is omitted, the scrollable container is excluded from the sequential tab sequence, rendering columns beyond the viewport completely inaccessible to keyboard navigators (violating **WCAG 2.1.1 Keyboard**).
</details>

### Q5: What is the difference between `<dl>`, `<dt>`, and `<dd>`?
<details>
<summary>View Answer</summary>

A `<dl>` is a **Description List** (formerly Definition List). It is used to present pairs or groups of terms and descriptions:
- `<dt>`: The **Description Term** (the label, key, or question).
- `<dd>`: The **Description Details** (the value, answer, or definition).
A single term can have multiple definitions, and multiple terms can share a single definition.
</details>

### Q6: What should an author do if a legacy codebase uses `<table>` for visual page layout?
<details>
<summary>View Answer</summary>

If a table is used strictly for visual presentation:
1. Add **`role="presentation"`** or **`role="none"`** to the `<table>` element.
2. Remove any `<caption>`, `<th>`, `<thead>`, or `scope` attributes.
This strips all table semantics from the Accessibility Tree, preventing screen readers from entering table mode or announcing misleading row/column coordinates.
</details>

### Q7: What is the purpose of the `<caption>` element in an HTML data table?
<details>
<summary>View Answer</summary>

The `<caption>` provides the programmatic title and Accessible Name for the table. When a screen reader encounters the table, it speaks the caption first (e.g., *"Table: Monthly Cloud Expenses, 12 rows, 4 columns"*). It must be placed as the immediate first child inside the `<table>` tag.
</details>

### Q8: How do NVDA and JAWS users navigate data tables?
<details>
<summary>View Answer</summary>

Users hold down **`Control + Alt`** and use the **Arrow Keys**:
- `Ctrl + Alt + Right Arrow`: Move to next cell in current row.
- `Ctrl + Alt + Left Arrow`: Move to previous cell in current row.
- `Ctrl + Alt + Down Arrow`: Move to next cell in current column.
- `Ctrl + Alt + Up Arrow`: Move to previous cell in current column.
At each step, the screen reader automatically announces the associated column and row header text.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Adding Scopes to a Table
**Problem**: Add the correct `scope` attributes to make this table accessible to screen readers:

```html
<table>
  <caption>Team Performance</caption>
  <tr><th>Name</th><th>Role</th><th>Rating</th></tr>
  <tr><th>Alice</th><td>Lead</td><td>5.0</td></tr>
</table>
```

<details>
<summary>View Solution</summary>

```html
<table>
  <caption>Team Performance</caption>
  <thead>
    <tr>
      <th scope="col">Name</th>
      <th scope="col">Role</th>
      <th scope="col">Rating</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th scope="row">Alice</th>
      <td>Lead</td>
      <td>5.0</td>
    </tr>
  </tbody>
</table>
```
</details>

### Exercise 2: Easy — Fixing the Safari List Style Bug
**Problem**: An author removed bullet points from a navigation list. Fix the markup to ensure Safari VoiceOver continues announcing it as a list.

<details>
<summary>View Solution</summary>

```html
<ul style="list-style: none;" role="list">
  <li><a href="/home">Home</a></li>
  <li><a href="/about">About</a></li>
</ul>
```
</details>

### Exercise 3: Medium — Semantic Key-Value Pairs
**Problem**: Author a semantic description list showing three properties of a virtual server: Operating System (Ubuntu 24.04), vCPU (8 cores), RAM (32 GB).

<details>
<summary>View Solution</summary>

```html
<dl>
  <dt>Operating System</dt>
  <dd>Ubuntu 24.04 LTS</dd>

  <dt>vCPU Allocation</dt>
  <dd>8 cores</dd>

  <dt>System RAM</dt>
  <dd>32 GB</dd>
</dl>
```
</details>

### Exercise 4: Medium — Keyboard-Accessible Scrollable Table Container
**Problem**: Create a CSS and HTML container that wraps a wide data table, supports horizontal scrolling, and is fully operable via keyboard.

<details>
<summary>View Solution</summary>

```html
<div 
  class="table-responsive" 
  tabindex="0" 
  role="region" 
  aria-labelledby="cap-pricing"
>
  <table>
    <caption id="cap-pricing">Tiered Pricing Architecture</caption>
    <thead>
      <tr>
        <th scope="col">Feature</th>
        <th scope="col">Starter</th>
        <th scope="col">Professional</th>
        <th scope="col">Enterprise</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <th scope="row">SLA Guarantee</th>
        <td>99.0%</td>
        <td>99.9%</td>
        <td>99.99%</td>
      </tr>
    </tbody>
  </table>
</div>

<style>
  .table-responsive {
    overflow-x: auto;
    border: 1px solid #ddd;
  }
  .table-responsive:focus-visible {
    outline: 3px solid #005a9c;
  }
</style>
```
</details>

### Exercise 5: Hard — Multi-Tier Complex Header Mapping
**Problem**: Write the HTML for a 2-tier table showing Q1 and Q2 sales for two regions (North and South), wiring headers and cells using `id` and `headers`.

<details>
<summary>View Solution</summary>

```html
<table>
  <caption>Quarterly Regional Sales Breakdown</caption>
  <thead>
    <tr>
      <th id="hdr-region" rowspan="2" scope="col">Region</th>
      <th id="hdr-q1" scope="col">Q1 Sales</th>
      <th id="hdr-q2" scope="col">Q2 Sales</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <th id="row-north" scope="row">North</th>
      <td headers="hdr-region row-north hdr-q1">$120,000</td>
      <td headers="hdr-region row-north hdr-q2">$145,000</td>
    </tr>
    <tr>
      <th id="row-south" scope="row">South</th>
      <td headers="hdr-region row-south hdr-q1">$98,000</td>
      <td headers="hdr-region row-south hdr-q2">$115,000</td>
    </tr>
  </tbody>
</table>
```
</details>

### Exercise 6: Hard — Automated DOM Table Accessibility Auditor
**Problem**: Write a vanilla JavaScript function that checks all `<table>` elements on a page and logs a warning for any table missing a `<caption>`, or any `<th>` missing a `scope` or `id` attribute.

<details>
<summary>View Solution</summary>

```javascript
function auditTableAccessibility() {
  const tables = document.querySelectorAll('table');

  tables.forEach((table, tIdx) => {
    // Ignore layout tables explicitly stripped of semantics
    if (table.getAttribute('role') === 'presentation' || table.getAttribute('role') === 'none') {
      return;
    }

    // 1. Check for <caption>
    const caption = table.querySelector('caption');
    if (!caption || !caption.textContent.trim()) {
      console.warn(`[Table A11y] Table #${tIdx} is missing a <caption> element!`);
    }

    // 2. Check <th> elements for scope or id
    const thElements = table.querySelectorAll('th');
    if (thElements.length === 0) {
      console.error(`[Table A11y] Table #${tIdx} has NO <th> header elements defined!`);
    }

    thElements.forEach((th, thIdx) => {
      const hasScope = th.hasAttribute('scope');
      const hasId = th.hasAttribute('id');
      if (!hasScope && !hasId) {
        console.warn(`[Table A11y] Table #${tIdx}, Header #${thIdx} ("${th.textContent.trim()}") is missing both 'scope' and 'id' attributes!`);
      }
    });
  });
}

auditTableAccessibility();
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 1.3.1 (Info & Relationships)**: Tabular data must be programmatically determinable.
- **Caption**: Always provide `<caption>` as immediate first child of `<table>`.
- **Headers & Scope**: Use `<th scope="col">` for column headers and `<th scope="row">` for row headers.
- **Complex Tables**: Use `id` on multi-tier headers and `headers="id1 id2"` on data cells.
- **Safari List Quirk**: Add `role="list"` to `<ul>`/`<ol>` when CSS `list-style: none` is applied.
- **Scrollable Tables**: Add `tabindex="0"` and `role="region"` to overflow container so keyboard users can scroll.
- **Key-Value Pairs**: Use `<dl>`, `<dt>`, and `<dd>`.

---

## 13. Further Reading

- [W3C WAI: Tables Concepts and Tutorials](https://www.w3.org/WAI/tutorials/tables/)
- [WebAIM: Creating Accessible Data Tables](https://webaim.org/techniques/tables/data)
- [Adrian Roselli: VoiceOver and list-style: none](https://adrianroselli.com/2019/05/voiceover-and-list-style-none.html)
- [MDN Web Docs: HTML Table Advanced Features and Accessibility](https://developer.mozilla.org/en-US/docs/Learn/HTML/Tables/Advanced)

---

[Previous: 08 - Accessible Forms & Error Announcement (08-accessible-forms-and-error-announcement.md)](08-accessible-forms-and-error-announcement.md) | [Section Index](README.md) | [Next: 10 - Accessible Dialogs, Popovers & Menus ->](10-accessible-dialogs-popovers-and-menus.md)
