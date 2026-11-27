# 09. Print and Alternative Media

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | CSS Paged Media Module Level 3 / CSS Fragmentation Level 3 / W3C Recommendation |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Chrome DevTools Print Emulation, macOS & Windows PDF Export |
| **Prerequisites** | `05-responsive-design/01-media-queries.md`, `11-accessibility-and-ux-in-css/02-colour-contrast-and-colour-independence.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
When users print web content—such as invoices, medical records, legal receipts, academic papers, or shipping labels—or export pages as PDF documents, unoptimized stylesheets produce disastrous results:
- Hundreds of megabytes of ink wasted on black headers, background gradients, and decorative images.
- Unnecessary interactive UI elements (sticky navigation bars, cookie banners, search bars, chat widgets, pagination controls) cluttering the printed paper.
- Critical content blocks (such as invoice signature lines or data tables) awkwardly sliced in half across physical page breaks.
- Dead hyperlinks: on a physical sheet of paper, an underline on "Click here" provides zero information on where the resource actually lives.

### The Solution
The CSS Web Platform provides dedicated print and paged media specifications:
- **`@media print`**: A native media query targeting physical paper printers and PDF export engines.
- **CSS Paged Media (`@page`)**: Controls physical paper geometry, margins, headers, footers, and page orientations (Portrait vs. Landscape).
- **CSS Fragmentation (`break-inside`, `break-before`, `break-after`)**: Guarantees that tables, cards, and diagrams do not fracture across paper sheets.
- **Generated Link Expansion (`attr(href)`)**: Automatically outputs the destination URL in parentheses after hyperlink text.
- **Ink Conservation**: Forcing clean black text on crisp white paper with stripped shadows and backgrounds.

---

## 2. Core Concepts

### 1. Screen Media vs. Paged Print Media

```
SCREEN MEDIA (Continuous Canvas)          PAGED MEDIA (Discrete Paper Sheets)
┌───────────────────────────────┐         ┌───────────────┐ ┌───────────────┐
│ Sticky Header (Fixed)         │         │ PAGE 1        │ │ PAGE 2        │
│ Continuous Scrolling Body     │ ──────► │ Header        │ │ Header        │
│ Floating Chat Widget          │         │ Content...    │ │ Content...    │
│ Giant Dark Background         │         │ [No Splits!]  │ │ Page 2 of 2   │
└───────────────────────────────┘         └───────────────┘ └───────────────┘
```

1. **Continuous vs. Paged Flow**: Screens provide an infinite scrollable canvas; print divides content into discrete physical page boxes (A4, Letter) governed by margins and physical pagination boundaries.
2. **Ink Economics**: On screens, dark mode saves OLED battery; on physical printers, dark backgrounds exhaust expensive black ink cartridges and warp paper fibers.
3. **Hyperlink Permanence**: On screen, links are clicked; on paper, links must display their explicit URI destination.

### 2. Page Break Controls (CSS Fragmentation Level 3)
Modern CSS replaces legacy `page-break-*` properties with the unified CSS Fragmentation specification:
- **`break-inside: avoid`** (replaces `page-break-inside: avoid`): Prevents an element (table row, card, chart) from being broken across page boundaries.
- **`break-before: page`** (replaces `page-break-before: always`): Forces a clean page break immediately before the element (ideal for major section headings or chapters).
- **`break-after: page`**: Forces a page break immediately following the element.
- **`orphans` & `widows`**:
  - `orphans: 3`: Minimum number of lines of a paragraph that must be left at the bottom of a page before breaking.
  - `widows: 3`: Minimum number of lines of a paragraph that must be carried over to the top of the next page.

---

## 3. How It Works Internally

### Paged Box Model Processing Pipeline
When a user presses `Cmd+P` or triggers `window.print()`:

```
[Print Trigger Initiated]
           │
           ▼
[Style Engine evaluates @media print rules]
           │
           ▼
[Paged Box Generation (@page)]
  - Assigns paper dimensions (e.g. A4: 210mm x 297mm)
  - Subtracts @page margins (e.g. 20mm all around)
  - Computes Page Area box: W = 170mm, H = 257mm
           │
           ▼
[Fragmentation & Page Splitting Pass]
  - Calculates element heights against available page area height
  - Checks break-inside: avoid rules
  - Inserts page break tokens where elements exceed available vertical space
           │
           ▼
[Rasterization to PDF or PostScript / CUPS Spooler]
```

---

## 4. Syntax & Usage

### 1. The Production Print Reset Boilerplate
```css
@media print {
  /* 1. Reset color schemes to black text on white paper */
  *, *::before, *::after {
    background: transparent !important;
    color: #000000 !important;
    box-shadow: none !important;
    text-shadow: none !important;
  }

  /* 2. Strip non-essential screen UI */
  nav,
  header,
  footer,
  aside,
  .sidebar,
  .cookie-banner,
  .chat-widget,
  .btn-action,
  .no-print {
    display: none !important;
  }

  /* 3. Configure paper size and margins */
  @page {
    size: A4 portrait;
    margin: 20mm;
  }

  /* 4. Prevent awkward paragraph fragmentations */
  p, h2, h3 {
    orphans: 3;
    widows: 3;
  }

  /* 5. Prevent headings from sitting alone at the bottom of a page */
  h1, h2, h3, h4, h5, h6 {
    break-after: avoid;
    break-inside: avoid;
  }

  /* 6. Expand hyperlink destinations */
  a[href^="http"]::after {
    content: " (" attr(href) ")";
    font-size: 85%;
    font-weight: normal;
    color: #333333 !important;
  }

  /* Do not print anchor links or javascript voids */
  a[href^="#"]::after,
  a[href^="javascript:"]::after {
    content: "";
  }
}
```

---

## 5. Practical Examples

### Example 1: Professional Invoice Print Layout
Engineering an e-commerce invoice that formats cleanly for physical print and PDF accounting archives:

```html
<article class="invoice-container">
  <header class="invoice-header">
    <div class="brand">
      <h1>CloudEngine LLC</h1>
      <p>100 Enterprise Way, Suite 400<br>San Francisco, CA 94105</p>
    </div>
    <div class="invoice-meta">
      <h2>INVOICE</h2>
      <p><strong>Invoice #:</strong> INV-2026-8819</p>
      <p><strong>Date:</strong> November 26, 2026</p>
      <p><strong>Due Date:</strong> Due on receipt</p>
    </div>
  </header>

  <table class="invoice-table">
    <thead>
      <tr>
        <th>Description</th>
        <th>Hours / Qty</th>
        <th>Rate</th>
        <th>Amount</th>
      </tr>
    </thead>
    <tbody>
      <tr>
        <td>Edge Compute Cluster Deployment</td>
        <td>40 hrs</td>
        <td>$175.00</td>
        <td>$7,000.00</td>
      </tr>
      <tr>
        <td>PostgreSQL 17 Database Migration</td>
        <td>15 hrs</td>
        <td>$200.00</td>
        <td>$3,000.00</td>
      </tr>
    </tbody>
    <tfoot>
      <tr>
        <td colspan="3" class="total-label">Total Due:</td>
        <td class="total-amount">$10,000.00</td>
      </tr>
    </tfoot>
  </table>

  <footer class="invoice-footer">
    <p>Thank you for your business. Please remit payment via ACH or Wire Transfer.</p>
  </footer>
</article>
```

```css
/* Screen Styles */
.invoice-container {
  max-width: 800px;
  margin: 2rem auto;
  padding: 2.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
}

/* PRINT STYLES */
@media print {
  body {
    background: #ffffff;
    font-family: system-ui, -apple-system, sans-serif;
    font-size: 10pt;
    line-height: 1.4;
  }

  .invoice-container {
    max-width: 100%;
    margin: 0;
    padding: 0;
    border: none;
    box-shadow: none;
  }

  .invoice-header {
    display: flex;
    justify-content: space-between;
    border-bottom: 2pt solid #000000;
    padding-bottom: 15pt;
    margin-bottom: 20pt;
  }

  .invoice-table {
    width: 100%;
    border-collapse: collapse;
    margin-bottom: 20pt;
    /* Prevent table from splitting across pages if short */
    break-inside: auto;
  }

  .invoice-table thead {
    /* Guarantees table header repeats at the top of every printed page! */
    display: table-header-group;
  }

  .invoice-table tr {
    /* Never slice a table row in half */
    break-inside: avoid;
  }

  .invoice-table th, .invoice-table td {
    padding: 8pt 10pt;
    border-bottom: 1pt solid #cccccc;
    text-align: start;
  }

  .invoice-table tfoot {
    font-weight: bold;
    border-top: 2pt solid #000000;
  }

  .invoice-footer {
    break-inside: avoid;
    margin-top: 30pt;
    border-top: 1pt solid #eeeeee;
    padding-top: 10pt;
    font-size: 9pt;
    color: #444444 !important;
  }
}
```

### Example 2: Repeating Table Headers Across Printed Pages
When long data tables span multiple physical sheets, the table header (`<thead>`) must repeat at the top of each page:

```css
@media print {
  table {
    width: 100%;
    border-collapse: collapse;
  }

  /* Browser engines repeat the header group on every new printed sheet */
  thead {
    display: table-header-group;
  }

  /* Browser engines repeat the footer group at the bottom of every sheet */
  tfoot {
    display: table-footer-group;
  }

  tr {
    break-inside: avoid;
    break-after: auto;
  }
}
```

### Example 3: Academic Paper Section Page Breaks
Ensuring that major chapters or sections always begin on a fresh physical page:

```css
@media print {
  .chapter-heading {
    /* Forces a new physical sheet */
    break-before: page;
    padding-top: 2cm;
  }

  .figure-box {
    /* Keep diagram image and caption together on the same sheet */
    break-inside: avoid;
    margin-block: 1.5cm;
  }

  .figure-caption {
    font-style: italic;
    font-size: 9pt;
  }
}
```

### Example 4: Preserving Essential Brand Logos via `print-color-adjust`
By default, browsers strip background colors and images in print to save ink. If an element *must* preserve its background color or image (e.g. a brand banner or color-coded status indicator), use `print-color-adjust`:

```css
@media print {
  .status-badge--success {
    /* Forces the browser to print the actual background color */
    print-color-adjust: exact;
    -webkit-print-color-adjust: exact;
    background-color: #dcfce7 !important;
    color: #14532d !important;
    border: 1pt solid #86efac;
    padding: 2pt 6pt;
    border-radius: 4pt;
  }
}
```

### Example 5: Custom Paged Margin Boxes (`@page`)
Using CSS Paged Media to control page numbers and headers directly:

```css
@media print {
  @page {
    size: letter portrait;
    margin-top: 25mm;
    margin-bottom: 25mm;
    margin-left: 20mm;
    margin-right: 20mm;
  }

  /* First page has different margins (e.g. Cover Page) */
  @page :first {
    margin-top: 0;
  }
}
```

---

## 6. Best Practices

1. **Strip All Interactive Elements**: Always hide navigation menus, search bars, modals, cookies, audio players, and floating buttons using `display: none !important`.
2. **Force High-Contrast Black on White**: Override background colors with `transparent` and text with `#000000` to prevent unreadable printouts and preserve printer toner.
3. **Use `break-inside: avoid` on Atomic Content**: Apply `break-inside: avoid` to table rows, card widgets, signatures, and chart diagrams so they never get sliced across paper pages.
4. **Expand Hyperlinks Explicitly**: Use `a[href^="http"]::after { content: " (" attr(href) ")"; }` so readers holding a physical piece of paper can see where links point.
5. **Ensure Tables Have `<thead>` Formatted as `table-header-group`**: This instructs print drivers to duplicate table column headers at the top of every physical printed page.
6. **Set `orphans: 3` and `widows: 3`**: Prevent lonely single lines from stranding at the bottom or top of printed sheets.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Forgetting to hide `position: fixed` elements** | Sticky headers reprint across the middle of every single physical sheet. | Set `position: static` or `display: none !important` on fixed/sticky elements. |
| **Leaving dark backgrounds intact** | Wastes entire black ink cartridges and saturates the physical paper with ink. | Force `background: transparent !important; color: #000 !important;`. |
| **Printing internal anchor links (`#main-content`)** | Outputs nonsensical text like `Skip to main ("#main-content")` on physical paper. | Filter out anchors: `a[href^="#"]::after { content: ""; }`. |
| **Using `overflow: hidden` on `body` or wrappers** | Cuts off print output entirely after page 1, failing to print remaining pages. | Reset `overflow: visible !important; height: auto !important;` on `html, body`. |
| **Slicing table rows across page edges** | Half a line of text prints at the bottom of page 1 and the other half on page 2. | Apply `tr { break-inside: avoid; }`. |
| **Relying on deprecated `page-break-*` properties alone** | Modern browsers standardize on CSS Fragmentation (`break-*`). | Use modern `break-inside: avoid`, with fallback to `page-break-inside: avoid`. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Support Matrix
- **`@media print`**: Universally supported across all web browsers.
- **`break-inside: avoid`**: Supported in Chrome 50+, Firefox 65+, Safari 10+.
- **`@page` (size, margins)**: Supported in Chrome 17+, Firefox 19+, Safari 14.1+.
- **`print-color-adjust: exact`**: Universal baseline in all modern browsers.

### DevTools Print Emulation Testing
Never waste physical paper while building print stylesheets:
1. Open Chrome DevTools $\rightarrow$ Press `Cmd+Shift+P` / `Ctrl+Shift+P`.
2. Type "Rendering" and select **Show Rendering**.
3. Scroll to **Emulate CSS media type** and select **print**.
4. The DevTools viewport will instantly re-render the page using your `@media print` rules!

---

## 9. Comparison / When to Use What

| Property | Legacy Equivalent | Standard Value | Primary Role |
| :--- | :--- | :--- | :--- |
| **`break-inside`** | `page-break-inside` | `avoid` | Keeps cards, rows, or figures intact on one sheet |
| **`break-before`** | `page-break-before` | `page` | Forces section to begin on a new printed sheet |
| **`break-after`** | `page-break-after` | `page` | Forces page break immediately following element |
| **`print-color-adjust`** | `-webkit-print-color-adjust` | `exact` | Overrides printer ink-saver to print true colors |
| **`orphans`** | N/A | `3` | Minimum lines at bottom of page before break |
| **`widows`** | N/A | `3` | Minimum lines at top of page after break |

---

## 10. Interview Questions

### Q1: What is CSS Fragmentation, and how do modern `break-*` properties replace legacy `page-break-*` properties?
<details>
<summary>Answer</summary>
CSS Fragmentation (Level 3) generalizes how content breaks across fragmented contexts (including paged print media, multi-column CSS columns, and CSS regions).
Historically, developers used print-only properties like `page-break-inside: avoid`, `page-break-before: always`, and `page-break-after: always`.

The modern standard unifies these into:
- `break-inside: avoid`
- `break-before: page` (or `break-before: column`)
- `break-after: page`
Modern properties work identically across both physical printed pages and responsive multi-column layouts (`columns: 3`). Modern browsers support both, but `break-*` is the official W3C Recommendation.
</details>

### Q2: How does the generated content `attr()` function enable accessible print styling for hyperlinks?
<details>
<summary>Answer</summary>
On a physical sheet of printed paper, interactive hyperlinks cannot be clicked. An underline on "Learn more" or "Documentation" provides no useful destination information to someone reading the paper document.

Using the CSS `attr()` function inside a `::after` pseudo-element:
```css
a[href^="http"]::after {
  content: " (" attr(href) ")";
}
```
The browser reads the HTML element's `href` attribute string and renders it in plain text directly after the link label, allowing the reader to read or type the full URL.
</details>

### Q3: Why does a webpage with `overflow: hidden` on `body` or `html` fail to print multiple pages?
<details>
<summary>Answer</summary>
When `overflow: hidden` is declared on `html` or `body` (often done by Single Page Application frameworks or modal scripts to prevent background scrolling), the browser's rendering engine treats the document as a fixed-height, single-screen clipping box.

When the print engine runs, it sees that overflow is explicitly disabled and clips all content beyond the height of the first page. As a result, the print preview generates only page 1, completely truncating all subsequent pages.
To fix this in print stylesheets:
```css
@media print {
  html, body {
    overflow: visible !important;
    height: auto !important;
  }
}
```
</details>

### Q4: What is the purpose of `print-color-adjust: exact`, and when should it be avoided?
<details>
<summary>Answer</summary>
By default, printer drivers execute an automatic ink-saving heuristic: they strip all author background colors, gradients, and background images, forcing white backgrounds.

`print-color-adjust: exact` instructs the browser and print driver to bypass this heuristic and render the exact author background colors and images specified in the stylesheet.
- **When to use**: Highlighting critical status badges (e.g. green "PAID" badge on an invoice), brand logos, or color-coded data charts where color conveys essential data.
- **When to avoid**: Giant dark hero banners, decorative page backgrounds, or dark mode themes, as printing them consumes enormous amounts of ink and damages physical paper.
</details>

### Q5: How do `orphans` and `widows` improve typographic readability in printed documents?
<details>
<summary>Answer</summary>
- **Orphan**: When a single line at the start of a paragraph is left isolated at the bottom of a page, while the rest of the paragraph continues on the next page.
- **Widow**: When a paragraph starts at the bottom of a page, but its final single line is pushed over to the top of the next page.
Both create visual awkwardness and disrupt reading flow. Setting:
```css
p {
  orphans: 3;
  widows: 3;
}
```
instructs the print engine that a paragraph must display at least 3 lines before breaking across pages, and must leave at least 3 lines on the top of the subsequent page, cleanly preventing isolated single-line text fragments.
</details>

### Q6: How does declaring `<thead>` as `table-header-group` affect multi-page printed tables?
<details>
<summary>Answer</summary>
In HTML/CSS, `thead { display: table-header-group; }` has a special behavior in paged media:
When a table is longer than a single physical sheet of paper, the print rendering engine automatically duplicates and prints the entire `<thead>` row at the top of every single subsequent page that contains rows from that table. This ensures readers never lose column context when reading page 2, 3, or 4 of a complex data report.
</details>

### Q7: Why should fixed and sticky headers be explicitly removed in `@media print`?
<details>
<summary>Answer</summary>
Elements styled with `position: fixed` or `position: sticky` are anchored to the screen viewport.
In paged media, some browser print engines interpret `position: fixed` as an instruction to print that element at the exact same physical coordinates on **every single printed page**. If a fixed header has a height of 80px, it will print across the top of page 1, page 2, page 3, etc., frequently rendering directly on top of the document's body text and destroying legibility.
Setting `display: none !important` or `position: static` resolves this.
</details>

### Q8: How do you configure paper dimensions and custom margins using the `@page` at-rule?
<details>
<summary>Answer</summary>
Inside `@media print`:
```css
@page {
  size: A4 portrait; /* Or 'letter landscape' */
  margin: 20mm 15mm 20mm 15mm;
}
```
You can also target specific pages using pseudo-classes:
```css
@page :first {
  margin-top: 0; /* Cover page layout */
}
@page :left {
  margin-right: 25mm; /* Binding margin for double-sided booklets */
}
@page :right {
  margin-left: 25mm;
}
```
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Comprehensive Print Stylesheet for Blog Articles
**Objective**: Author an `@media print` block that strips navigation, ads, and footers, expands HTTP URLs, avoids breaking figure images, and formats clean black text on white paper.

<details>
<summary>Solution</summary>

```css
@media print {
  /* 1. Global Reset */
  html, body {
    background: #ffffff !important;
    color: #000000 !important;
    font-size: 11pt;
    line-height: 1.5;
    overflow: visible !important;
    height: auto !important;
  }

  /* 2. Hide Non-Print Elements */
  nav, .site-header, .site-footer, .comments-section, .share-buttons, .ads-banner {
    display: none !important;
  }

  /* 3. Paper Configuration */
  @page {
    size: A4 portrait;
    margin: 20mm;
  }

  /* 4. Article Structure */
  .article-container {
    max-width: 100% !important;
    margin: 0 !important;
    padding: 0 !important;
    border: none !important;
  }

  /* 5. Figures and Images */
  figure, img {
    break-inside: avoid;
    max-width: 100% !important;
  }

  /* 6. Break Prevention */
  h1, h2, h3 {
    break-after: avoid;
    color: #000000 !important;
  }

  p {
    orphans: 3;
    widows: 3;
  }

  /* 7. Hyperlink Expansion */
  a[href^="http"]::after {
    content: " (" attr(href) ")";
    font-size: 80%;
    color: #444444 !important;
  }

  a[href^="#"]::after {
    content: "";
  }
}
```
</details>

### Exercise 2: Printable Certificate / Award with Landscape Orientation
**Objective**: Build a certificate page that forces landscape A4 printing, preserves borders, and avoids page breaks.

<details>
<summary>Solution</summary>

```html
<section class="certificate">
  <h1 class="cert-title">Certificate of Excellence</h1>
  <p class="cert-recipient">Presented to <strong>Shiva Ram</strong></p>
  <p class="cert-body">For outstanding architecture in CSS curriculum engineering.</p>
  <div class="cert-signature">
    <div class="sig-line">Verified Lead Evaluator</div>
  </div>
</section>
```

```css
@media print {
  @page {
    size: A4 landscape;
    margin: 10mm;
  }

  body {
    background: #ffffff;
    margin: 0;
  }

  .certificate {
    box-sizing: border-box;
    width: 100%;
    height: 100%;
    border: 8pt double #0f172a;
    padding: 40pt;
    text-align: center;
    break-inside: avoid;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
  }

  .cert-title {
    font-size: 32pt;
    font-weight: bold;
    margin-bottom: 20pt;
  }

  .cert-recipient {
    font-size: 20pt;
    margin-bottom: 15pt;
  }

  .cert-body {
    font-size: 14pt;
    max-width: 80%;
    margin-bottom: 40pt;
  }

  .cert-signature {
    width: 250pt;
    border-top: 1pt solid #000000;
    padding-top: 5pt;
    font-size: 11pt;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`@media print`**: Targets physical paper and PDF generators.
- **Hiding UI**: `nav, footer, aside, .btn, .no-print { display: none !important; }`.
- **Overflow Reset**: Always set `html, body { overflow: visible !important; height: auto !important; }`.
- **`break-inside: avoid`**: Keeps tables, cards, and diagrams intact on a single sheet.
- **`break-before: page`**: Forces a section or chapter to begin on a brand new sheet.
- **`thead { display: table-header-group; }`**: Automatically repeats table column headers at the top of every printed page.
- **Link URLs**: `a[href^="http"]::after { content: " (" attr(href) ")"; }`.
- **`@page`**: Controls sheet size and margins (`@page { size: A4; margin: 20mm; }`).
- **`print-color-adjust: exact`**: Preserves essential background colors from being stripped by printer toner-savers.

---

## 13. Further Reading
- [W3C CSS Paged Media Module Level 3](https://www.w3.org/TR/css-page-3/)
- [W3C CSS Fragmentation Module Level 3](https://www.w3.org/TR/css-break-3/)
- [MDN Web Docs: Printing](https://developer.mozilla.org/en-US/docs/Web/Guide/Printing)
- [Smashing Magazine: Designing For Print With CSS](https://www.smashingmagazine.com/2015/01/designing-for-print-with-css/)
- [A List Apart: Improving Web Typography With Print CSS](https://alistapart.com/article/improving-web-typography-with-print-css/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [08. Readable Line Length and Spacing](08-readable-line-length-and-spacing.md) | [10. CSS and Screen Readers](10-css-and-screen-readers.md) |
