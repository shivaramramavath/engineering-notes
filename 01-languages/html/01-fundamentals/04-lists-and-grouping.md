# Lists, Grouping Elements, and Semantic Collections

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `01-document-structure.md`, `03-text-content-and-typography.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Baseline Status** | Widely Available across all modern browsers |
| **Browsers Tested** | Chrome 120+, Firefox 120+, Safari 17+, Edge 120+ |
| **Tested With** | W3C Nu HTML Checker, Axe-core v4.8, NVDA 2023.3, VoiceOver macOS Sonoma |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

On the web, information rarely exists as isolated sentences. Data naturally forms taxonomies, ordered sequences, glossary mappings, and standalone editorial assets. HTML provides specialized **grouping elements** to represent these relationships:
- **Unordered Lists (`<ul>`)**: Collections of items where changing the sequence does not change the core meaning (e.g., feature lists, navigation menus).
- **Ordered Lists (`<ol>`)**: Numbered collections where sequence is meaningful and ordinal (e.g., algorithmic steps, recipes, leaderboards).
- **Description Lists (`<dl>`)**: Key-value pairs, metadata registries, or glossaries associating terms (`<dt>`) with descriptions (`<dd>`).
- **Figures (`<figure>` and `<figcaption>`)**: Self-contained units of editorial flow, such as code snippets, charts, diagrams, or photographs with captions.
- **Blockquotes (`<blockquote>`)**: Extended quotations drawn from external sources.
- **Preformatted Text (`<pre>`)**: Blocks where whitespace, indentation, and alignment are preserved.

Assistive technologies (screen readers) announce the total count of items in a list (e.g., *"List, 5 items"*) and inform users of their exact position as they navigate (*"Item 2 of 5"*). Without native list markup, screen reader users cannot estimate the scope of collections or jump sequentially through options.

---

## 2 Core Concepts

### Unordered Lists (`<ul>`) and List Items (`<li>`)
The `<ul>` element represents an unordered list. The only permitted direct children of `<ul>` are `<li>` (list item) elements (and modern `<script>` / `<template>` utility tags). Placing raw text, `<div>`, or `<p>` directly as children of `<ul>` violates the WHATWG content model.

### Ordered Lists (`<ol>`)
The `<ol>` element represents an ordered sequence. It supports three critical native attributes:
- `start`: An integer specifying the starting value of the first list item (defaults to `1`).
- `reversed`: A boolean attribute indicating that the list items are displayed in descending ordinal order (e.g., top 10 countdowns).
- `type`: Specifies the numbering marker style (`1` for decimal, `a` for lowercase letters, `A` for uppercase letters, `i` for lowercase Roman numerals, `I` for uppercase Roman numerals). *Note: CSS `list-style-type` is generally preferred for styling, but HTML `type` retains semantic value for legacy/fallback modes.*
- On individual `<li>` elements inside an `<ol>`, the `value` attribute allows authors to manually override the ordinal integer for that specific item, shifting all subsequent automatic counters.

### Description Lists (`<dl>`, `<dt>`, `<dd>`)
Formerly called "definition lists", `<dl>` represents an association list consisting of zero or more name-value groups.
- `<dt>` (Description Term): Represents the term, name, or key.
- `<dd>` (Description Details): Represents the value, definition, or description.
- A `<dl>` can have multiple `<dt>` elements for a single `<dd>` (synonyms), or a single `<dt>` with multiple `<dd>` elements (multiple definitions).
- **HTML5 Standardized Wrapper**: Since HTML 5.2, `<div>` elements are explicitly permitted as direct children of `<dl>` to group individual `<dt>`/`<dd>` pairs for CSS styling (e.g., Flexbox or Grid key-value row layouts).

### Figures and Captions (`<figure>`, `<figcaption>`)
The `<figure>` element represents self-contained content, optionally with a caption (`<figcaption>`), that is referenced as a single unit from the main flow. It can be moved away from the primary text stream (e.g., to an appendix or sidebar) without affecting the document's rhetorical meaning.

### Blockquotes (`<blockquote>`)
The `<blockquote>` element represents a section that is quoted from another source. It accepts a `cite` attribute containing the URL of the quotation's original source.

---

## 3 How It Works Internally

### Tree Construction and Content Model Verification
During HTML tree construction, the parser strictly validates parent-child relationships for lists. If an author improperly places an element like `<div>` directly inside `<ul>`, the parser handles it according to error-recovery rules:

```
Author Markup (Invalid):
<ul>
  <div>Not a list item</div>
  <li>Valid Item</li>
</ul>

Parser Behavior:
1. "in list" insertion mode.
2. Token: <div> start tag.
3. <div> is not <li>, <script>, or <template>.
4. Parser emits parse error.
5. In DOM tree: <div> is retained inside <ul>, but AOM marks the list role as invalid or degraded.
```

### Accessibility Object Model (AOM) List Construction

```mermaid
flowchart TD
    UL["&lt;ul&gt; element"] --> RoleList["AOM Node: role='list'<br>Property: childCount = 3"]
    RoleList --> LI1["&lt;li&gt; Node 1: role='listitem'<br>PosInSet: 1, SetSize: 3"]
    RoleList --> LI2["&lt;li&gt; Node 2: role='listitem'<br>PosInSet: 2, SetSize: 3"]
    RoleList --> LI3["&lt;li&gt; Node 3: role='listitem'<br>PosInSet: 3, SetSize: 3"]

    CSSReset["CSS: ul { list-style: none; }"] -.-> SafariEngine["WebKit / Safari AOM Engine"]
    SafariEngine -.-> StrippedRole["Heuristic: Strips role='list'<br>(Treated as generic text)"]
    ARIFix["Fix: &lt;ul role='list'&gt;"] --> RestoredRole["Restores role='list' in Safari"]
```

When assistive technologies inspect a list:
1. The browser engine computes `SetSize` (total count of `<li>` siblings) and `PosInSet` (the 1-indexed position of each item).
2. Screen readers like VoiceOver and NVDA read this metadata aloud.
3. **The Safari / WebKit List-Style Quirks**: WebKit historically removes the `list` role from `<ul>` or `<ol>` elements when CSS applies `list-style: none;` (assuming the author intended to use the list purely for visual layout). To preserve accessibility when styling navigation bars without bullets, developers add `role="list"` explicitly.

---

## 4 Syntax & Usage

```html
<!-- 1. Unordered List with strict children -->
<ul class="feature-matrix">
  <li>Encrypted data persistence at rest via AES-256</li>
  <li>Multi-region automated failover within 15 seconds</li>
  <li>Granular role-based access control (RBAC)</li>
</ul>

<!-- 2. Ordered List with start and reversed attributes -->
<ol start="10" reversed class="countdown-list">
  <li>Lift-off sequence initiated</li>
  <li>Auxiliary power unit switched to internal batteries</li>
  <li>Main engine ignition sequence start</li>
</ol>

<!-- 3. Description List using modern <div> wrappers -->
<dl class="metadata-grid">
  <div class="row">
    <dt>Node Architecture</dt>
    <dd>x86_64 / AMD64</dd>
  </div>
  <div class="row">
    <dt>Kernel Version</dt>
    <dd>6.8.0-45-generic</dd>
  </div>
</dl>

<!-- 4. Figure with Code Listing -->
<figure>
  <figcaption>Listing 1: Environment variable loader pattern</figcaption>
  <pre><code>export DATABASE_URL="postgres://user:pass@localhost:5432/app"
export LOG_LEVEL="info"</code></pre>
</figure>
```

> [!NOTE]
> **Baseline Status: Widely Available**
> All list elements (`<ul>`, `<ol>`, `<li>`, `<dl>`, `<dt>`, `<dd>`), `<figure>`, `<figcaption>`, and `<blockquote>` are baseline HTML features supported across every web browser engine since HTML 4.01/HTML5. The inclusion of `<div>` inside `<dl>` was standardized in HTML 5.2 and is widely supported.

---

## 5 Practical Examples

### Example 1: Multi-Level Nested Navigation List
```html
<nav aria-label="Documentation Directory">
  <ul class="nav-tree" role="list">
    <li>
      <a href="/intro">Introduction</a>
    </li>
    <li>
      <span>System Architecture</span>
      <ul role="list">
        <li><a href="/arch/networking">Networking Core</a></li>
        <li><a href="/arch/consensus">Raft Consensus Protocol</a></li>
        <li>
          <span>Storage Engines</span>
          <ul role="list">
            <li><a href="/arch/storage/lsm">LSM-Trees</a></li>
            <li><a href="/arch/storage/b-tree">B-Tree Indexes</a></li>
          </ul>
        </li>
      </ul>
    </li>
    <li>
      <a href="/deployment">Deployment Guide</a>
    </li>
  </ul>
</nav>
```

### Example 2: Technical Glossary with Multiple Terms and Descriptions
```html
<article>
  <h2>Distributed Systems Terminology</h2>
  <dl class="glossary">
    <!-- Multiple terms pointing to one definition -->
    <dt>Split-Brain</dt>
    <dt>Network Partition</dt>
    <dd>
      A condition where a distributed cluster loses communication between sub-groups of nodes,
      risking concurrent inconsistent state updates if both partitions continue accepting writes.
    </dd>

    <!-- Single term with multiple distinct definitions -->
    <dt>CAP Theorem</dt>
    <dd>
      Formulated by Eric Brewer, stating that a distributed data store can simultaneously
      guarantee at most two out of three properties: Consistency, Availability, and Partition Tolerance.
    </dd>
    <dd>
      In practice, because network partitions cannot be avoided in real networks, distributed
      architectures must choose between Consistency (CP) and Availability (AP) during partitions.
    </dd>
  </dl>
</article>
```

### Example 3: Edge Case — Manual Ordered List Item Numbering
```html
<article>
  <h2>Legal Contract Stipulations</h2>
  <ol>
    <li value="1">Definitions and Interpretations</li>
    <li value="2">Term and Termination Provisions</li>
    <!-- Deliberately skip clause 3 (superseded) and jump to clause 4 -->
    <li value="4">Intellectual Property Assignments (Clause 3 Revoked)</li>
    <li>Indemnification Obligations (Automatically assigned value 5)</li>
    <li>Limitation of Liability (Automatically assigned value 6)</li>
  </ol>
</article>
```

### Example 4: Failure Case — Broken Content Model and Missing Wrappers
```html
<!-- ANTI-PATTERN: DO NOT COPY -->

<!-- Error 1: Placing text and links directly inside <ul> without <li> -->
<ul class="actions">
  <a href="/edit">Edit File</a>
  <a href="/delete">Delete File</a>
</ul>

<!-- Error 2: Nesting a sub-list directly inside <ul> instead of inside an <li> -->
<ul>
  <li>Main Item</li>
  <ul> <!-- BROKEN: Sub-list must be inside an <li> -->
    <li>Sub Item</li>
  </ul>
</ul>

<!-- Error 3: Orphaned <dt> and <dd> outside <dl> -->
<div>
  <dt>Server</dt>
  <dd>Apache</dd>
</div>

<!-- W3C Nu Validator Result:
1. Error: Element a not allowed as child of element ul in this context.
2. Error: Element ul not allowed as child of element ul in this context.
3. Error: Element dt not allowed as child of element div in this context.
-->
```

### Example 5: Production-Grade Figure with Accessible Code Block and Attribution
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Production Figure Pattern</title>
  <style>
    figure { margin: 1.5rem 0; border: 1px solid #d1d5db; border-radius: 8px; overflow: hidden; }
    figcaption { background: #f3f4f6; padding: 0.75rem 1rem; font-size: 0.875rem; font-weight: 600; border-bottom: 1px solid #d1d5db; }
    pre { margin: 0; padding: 1rem; background: #1e293b; color: #f8fafc; overflow-x: auto; font-size: 0.875rem; }
    blockquote { border-left: 4px solid #3b82f6; margin: 1.5rem 0; padding: 0.5rem 1rem; background: #eff6ff; }
  </style>
</head>
<body>
  <main>
    <article>
      <h1>Configuring High-Throughput Linux Sockets</h1>
      
      <p>
        To accommodate massive concurrent connection spikes, tune the TCP listen backlog
        parameter within the sysctl configuration file.
      </p>

      <figure aria-labelledby="listing-1-caption">
        <figcaption id="listing-1-caption">Listing 1: Kernel parameter tuning for socket queues</figcaption>
        <pre><code class="language-bash"># Increase system-wide socket backlog limit
sysctl -w net.core.somaxconn=65535

# Increase max pending TCP connection requests
sysctl -w net.ipv4.tcp_max_syn_backlog=65535</code></pre>
      </figure>

      <blockquote cite="https://man7.org/linux/man-pages/man2/listen.2.html">
        <p>
          "The backlog argument defines the maximum length to which the queue of
          pending connections for sockfd may grow. If a connection request arrives
          when the queue is full, the client may receive an error with an indication
          of ECONNREFUSED."
        </p>
        <footer>— Linux Programmer's Manual, <cite>listen(2)</cite></footer>
      </blockquote>
    </article>
  </main>
</body>
</html>
```

---

## 6 Best Practices

1. **Strictly Abide by List Content Models**: The direct children of `<ul>` and `<ol>` must only be `<li>` elements.
2. **Properly Nest Sub-Lists Inside `<li>`**: A nested sub-list must **always** be placed inside a parent `<li>`, never as a sibling to `<li>` inside `<ul>`.
3. **Preserve WebKit Accessibility with `role="list"`**: Whenever removing bullets via CSS (`list-style: none;`), append `role="list"` to `<ul>` or `<ol>` if the list items represent a meaningful enumeration.
4. **Use `<dl>` for Key-Value Metadata**: For product specifications, user profiles, server metrics, or settings registries, choose `<dl>` over `<table>` or generic `<div>` rows.
5. **Group `<dt>` and `<dd>` with `<div>`**: When styling description lists with Flexbox or Grid, wrap each key-value pair in a valid `<div>` inside the `<dl>`.
6. **Use `<figure>` Only for Movable Content**: If a diagram, image, or snippet cannot be relocated to an appendix without breaking the linear reading flow of the surrounding sentence, use an inline element rather than `<figure>`.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Bad Code | Good Code | Technical Explanation |
| :--- | :--- | :--- | :--- |
| **Direct Child Violation** | `<ul><div><li>Item</li></div></ul>` | `<ul><li><div>Item</div></li></ul>` | `<div>` cannot be a direct child of `<ul>`. The browser parser will generate a malformed AOM structure. |
| **Un-nested Sub-list** | `<ul><li>Parent</li><ul><li>Child</li></ul></ul>` | `<ul><li>Parent<ul><li>Child</li></ul></li></ul>` | Placing `<ul>` directly inside `<ul>` violates the content model. It must be nested inside an `<li>`. |
| **Lost List Semantics in Safari** | `<ul style="list-style: none;">` | `<ul style="list-style: none;" role="list">` | VoiceOver in WebKit drops `role="list"` when `list-style: none` is applied; `role="list"` forces restoration. |
| **Misusing `<dl>` for Tables** | Using `<dl>` for tabular data with headers | Use `<table>`, `<thead>`, `<tbody>` | Tables provide 2D row/column spatial coordinates; `<dl>` is purely a 1D associative key-value list. |
| **Missing `<figcaption>` Context** | `<figure><h3>Title</h3><img ...></figure>` | `<figure><figcaption>Title</figcaption><img ...></figure>` | `<figcaption>` explicitly provides the accessible label for `<figure>` via native browser accessibility mappings. |
| **Using `<blockquote>` for Indentation** | `<blockquote style="margin-left: 20px;">Indented note</blockquote>` | `<p class="indented-note">Note</p>` | `<blockquote>` conveys quotation semantics. Using it for visual indenting tricks screen readers into announcing false citations. |

---

## 8 Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Criterion 1.3.1 (Info and Relationships)**: Collections of related items must be marked up with list elements so their relationship is programmatically exposed to assistive agents.
- Screen readers announce list metrics:
  - NVDA: *"List with 4 items"*
  - VoiceOver: *"List, 4 items"*
  - JAWS: *"List of 4 items"*
- If `<ul>` is replaced with `<div>Item 1</div><div>Item 2</div>`, users lose the ability to skip entire lists using standard screen reader shortcut keys (e.g., `L` for next list, `I` for next list item).

### Search Engine Optimization (SEO)
- Search engines parse `<ol>` and `<ul>` extensively to construct **Featured Snippet lists** on Google search results (e.g., "Top 5 algorithms", "Step-by-step tutorial").
- Structured `<dl>` markup is utilized by search crawlers to extract rich entity metadata for Knowledge Graph indexing.

### Performance Notes
- Native lists are computationally lightweight. However, rendering lists with thousands of items (e.g., long data logs) directly in the DOM creates massive node counts. Use DOM virtualization techniques (windowing) for collections exceeding 500 items.

### Security Notes
- Content within `<pre><code>` blocks often displays raw code. When outputting user-submitted code in templates, all characters like `<`, `>`, and `&` must be strictly escaped to `&lt;`, `&gt;`, and `&amp;` to prevent stored Cross-Site Scripting (XSS).

---

## 9 Comparison / When to Use What

| Element Pattern | Structural Relationship | Screen Reader Announcement | Ideal Use Case |
| :--- | :--- | :--- | :--- |
| `<ul>` / `<li>` | Non-ordinal collection | "List, X items" | Navigation links, feature bullet points, tags. |
| `<ol>` / `<li>` | Ordinal, sequential collection | "List, X items, Item 1..." | Instructions, tutorials, rank leaderboards. |
| `<dl>` / `<dt>` / `<dd>` | Key-value associative mapping | "Definition list / Description list" | System specs, glossaries, item metadata grids. |
| `<figure>` / `<figcaption>` | Self-contained referenced unit | "Figure, [caption name]" | Code listings, diagrams, annotated charts. |
| `<blockquote>` / `<cite>` | External quotation | "Quote / Blockquote" | Direct quotes from papers, speeches, articles. |

---

## 10 Interview Questions

<details>
<summary><strong>1. Why does Safari/WebKit strip the accessibility role from &lt;ul&gt; when CSS `list-style: none` is applied, and how do you remediate it?</strong></summary>

**Answer:**
WebKit introduced a heuristic designed to eliminate "annoying" screen reader chatter. Many websites historically used `<ul><li>` purely as markup scaffolding to build horizontal navigation bars or custom button toolbars where bullet points were hidden. WebKit engineers observed that hearing *"List, 8 items"* on every navbar was disruptive, so their accessibility engine automatically strips the `list` role when `list-style: none` is applied.

To remediate this and force the browser to retain the semantic list structure for screen readers, add the ARIA role attribute explicitly:
```html
<ul style="list-style: none;" role="list">
  <li>Item</li>
</ul>
```
This forces WebKit's accessibility mapping to preserve `role="list"`.
</details>

<details>
<summary><strong>2. Can a &lt;dl&gt; element contain &lt;div&gt; elements, and what was the historical reason for this specification update?</strong></summary>

**Answer:**
Yes. Under the original HTML5 specification, `<dl>` could strictly contain only `<dt>` and `<dd>` elements. This made styling description lists using modern CSS layout models (Flexbox and Grid) virtually impossible, because developers could not wrap each individual term/definition pair in a container to act as a flex or grid item (e.g., to create two-column key-value rows).

In HTML 5.2, the WHATWG and W3C updated the content model of `<dl>` to explicitly allow `<div>` elements as direct children, provided that each `<div>` contains only one or more `<dt>` elements followed by one or more `<dd>` elements (and optional `<script>`/`<template>`).
</details>

<details>
<summary><strong>3. How does the `reversed` attribute on an &lt;ol&gt; affect the DOM `value` properties of child &lt;li&gt; elements?</strong></summary>

**Answer:**
The `reversed` boolean attribute indicates that the list is numbered in descending order (e.g., 3, 2, 1). If an `<ol reversed>` contains three `<li>` elements and no explicit `start` attribute is specified, the browser calculates the starting value as equal to the total number of items (in this case, 3).
- The first `<li>` receives computed value `3`.
- The second `<li>` receives computed value `2`.
- The third `<li>` receives computed value `1`.

In JavaScript, inspecting `liElement.value` reflects these assigned ordinal integers.
</details>

<details>
<summary><strong>4. What is the semantic difference between the `cite` attribute on a &lt;blockquote&gt; and the &lt;cite&gt; element?</strong></summary>

**Answer:**
- The **`cite` attribute** (on `<blockquote>` or `<q>`) takes a **URL string** pointing to the online source of the quotation. It is intended for machine consumption and is not displayed visually by default in browser viewports.
- The **`<cite>` element** is an inline HTML element used to mark up the **title of a work** (e.g., a book title, research paper title, song, or film). According to the HTML specification, `<cite>` must contain the title of the work, not the author's personal name.
</details>

<details>
<summary><strong>5. Can an &lt;ol&gt; list start at a negative number or zero?</strong></summary>

**Answer:**
Yes. The `start` attribute on `<ol>` accepts any valid integer (positive, zero, or negative). For example:
```html
<ol start="-2">
  <li>Negative two</li>
  <li>Negative one</li>
  <li>Zero</li>
  <li>One</li>
</ol>
```
The browser will correctly increment the values mathematically from -2 upwards.
</details>

<details>
<summary><strong>6. Where must a &lt;figcaption&gt; be positioned inside a &lt;figure&gt; element?</strong></summary>

**Answer:**
According to the WHATWG specification, a `<figcaption>` must be placed either as the **very first child** or the **very last child** of a `<figure>` element. It cannot be sandwiched between other sibling elements inside the figure, and a `<figure>` can contain at most one `<figcaption>`.
</details>

<details>
<summary><strong>7. How does nesting a sub-list incorrectly outside of an &lt;li&gt; corrupt the accessibility tree?</strong></summary>

**Answer:**
When an author writes:
```html
<ul>
  <li>Item 1</li>
  <ul><li>Sub Item</li></ul> <!-- INVALID -->
</ul>
```
Assistive software parses the parent `<ul>` and encounters an unexpected nested list instead of an expected `listitem`. As a result:
1. The announced total count for the top list becomes erroneous (e.g., it may count 1 item, or get confused by the unattached list).
2. The hierarchical parent-child relation between "Item 1" and "Sub Item" is lost; the sub-list is treated as a detached sibling rather than a branch of "Item 1".
</details>

<details>
<summary><strong>8. What is the difference in content models between &lt;pre&gt; and &lt;code&gt;?</strong></summary>

**Answer:**
- `<pre>` is a **block-level grouping element** representing preformatted text where whitespace and line breaks are preserved verbatim.
- `<code>` is an **inline phrasing element** representing a fragment of computer code.
To mark up a multi-line code block semantically, you wrap `<code>` inside `<pre>`: `<pre><code>...</code></pre>`. `<pre>` handles formatting preservation, while `<code>` imparts the semantic meaning of computer language syntax.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Author a Valid Nested Navigation Menu
**Task**: Write the HTML for a two-level navigation menu for a tech blog containing "Home", "Articles" (with sub-links "Rust" and "Go"), and "About". Ensure perfect nesting validity.

<details>
<summary><strong>Solution</strong></summary>

```html
<nav aria-label="Main Navigation">
  <ul role="list">
    <li><a href="/">Home</a></li>
    <li>
      <a href="/articles">Articles</a>
      <ul role="list">
        <li><a href="/articles/rust">Rust</a></li>
        <li><a href="/articles/go">Go</a></li>
      </ul>
    </li>
    <li><a href="/about">About</a></li>
  </ul>
</nav>
```
*Explanation*: Notice that the inner `<ul>` is nested **inside** the `<li>` of "Articles", making it syntactically compliant with HTML standards.
</details>

### Exercise 2 (Easy): Countdown Leaderboard with Attributes
**Task**: Create an ordered list displaying a Top 3 countdown (3, 2, 1) using native HTML `<ol>` attributes without any CSS.

<details>
<summary><strong>Solution</strong></summary>

```html
<ol start="3" reversed>
  <li>Bronze Medal: Node Alpha</li>
  <li>Silver Medal: Node Beta</li>
  <li>Gold Medal: Node Gamma</li>
</ol>
```
*Explanation*: Setting `start="3"` and `reversed` counts down from 3 to 1 automatically across the three child `<li>` items.
</details>

### Exercise 3 (Medium): System Specification Key-Value Grid
**Task**: Mark up a system hardware specifications card using `<dl>`, `<div>`, `<dt>`, and `<dd>` styled for a key-value grid.

<details>
<summary><strong>Solution</strong></summary>

```html
<section aria-labelledby="specs-title">
  <h2 id="specs-title">Host Server Telemetry</h2>
  <dl class="specs-grid">
    <div class="spec-row">
      <dt>Operating System</dt>
      <dd>Ubuntu 24.04 LTS</dd>
    </div>
    <div class="spec-row">
      <dt>Memory Allocated</dt>
      <dd>64 GB ECC DDR5</dd>
    </div>
    <div class="spec-row">
      <dt>Storage Volume</dt>
      <dd>2 TB NVMe RAID-1</dd>
    </div>
  </dl>
</section>
```
*Explanation*: Wrapping each `<dt>/<dd>` pair in a `<div>` inside `<dl>` is valid HTML 5.2+ and provides clean CSS flex/grid layout hooks.
</details>

### Exercise 4 (Medium): Accessible Multi-Line Code Figure
**Task**: Build an accessible figure containing a multi-line Dockerfile snippet with an associated `<figcaption>` and copyable preformatted code.

<details>
<summary><strong>Solution</strong></summary>

```html
<figure aria-labelledby="dockerfile-caption">
  <figcaption id="dockerfile-caption">Listing 2: Multi-stage production container build</figcaption>
  <pre><code class="language-dockerfile">FROM golang:1.22-alpine AS builder
WORKDIR /app
COPY . .
RUN CGO_ENABLED=0 go build -o server .

FROM scratch
COPY --from=builder /app/server /server
ENTRYPOINT ["/server"]</code></pre>
</figure>
```
*Explanation*: The `<figure>` is explicitly labelled by its `<figcaption>` via `aria-labelledby`, and `<pre><code>` correctly combines code semantics with whitespace preservation.
</details>

### Exercise 5 (Hard): Identify and Fix 5 Content-Model Violations
**Task**: Review this malformed snippet and rewrite it to pass strict W3C validation.
```html
<ol>
  <h4>Installation Steps</h4>
  <li>Clone repo</li>
  <p>Make sure you have Git installed.</p>
  <li>Run build script</li>
</ol>
```

<details>
<summary><strong>Solution</strong></summary>

```html
<section aria-labelledby="install-heading">
  <h4 id="install-heading">Installation Steps</h4>
  <ol>
    <li>
      <p>Clone repo</p>
      <p>Make sure you have Git installed.</p>
    </li>
    <li>
      <p>Run build script</p>
    </li>
  </ol>
</section>
```
*Explanation*: Neither `<h4>` nor `<p>` can be direct children of `<ol>`. The heading is moved outside the list, and the explanatory paragraph is nested inside the first `<li>`.
</details>

### Exercise 6 (Hard): Accessible Multi-Term Description List with Synonyms
**Task**: Construct a financial terminology glossary where multiple acronyms ("EBITDA", "Operating Cash Flow Proxy") map to a single definition, and a single term ("Yield") maps to two distinct investment definitions.

<details>
<summary><strong>Solution</strong></summary>

```html
<dl class="financial-glossary">
  <!-- Multiple terms for one definition -->
  <div class="glossary-group">
    <dt>EBITDA</dt>
    <dt>Earnings Before Interest, Taxes, Depreciation, and Amortization</dt>
    <dd>
      A widely utilized metric of core corporate profitability that evaluates
      operational performance before the impact of capital structure and accounting charges.
    </dd>
  </div>

  <!-- One term for multiple definitions -->
  <div class="glossary-group">
    <dt>Yield</dt>
    <dd>
      In fixed-income securities: the annual percentage rate of return earned on a bond,
      calculated as coupon payments divided by current market price.
    </dd>
    <dd>
      In equity investments: the dividend yield, calculated as annual dividends paid per
      share divided by the current share price.
    </dd>
  </div>
</dl>
```
</details>

---

## 12 Summary / Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        HTML COLLECTIONS CHEAT SHEET                    │
├───────────────┬────────────────────────────┬───────────────────────────┤
│ Element       │ Direct Permitted Children  │ Key Attributes / Rules    │
├───────────────┼────────────────────────────┼───────────────────────────┤
│ <ul>          │ <li>, <script>, <template> │ role="list" when unstyled │
│ <ol>          │ <li>, <script>, <template> │ start, reversed, type     │
│ <li>          │ Flow content (any elements)│ value (inside <ol>)       │
│ <dl>          │ <dt>, <dd>, <div>          │ Key-value pairings        │
│ <dt>          │ Phrasing content           │ Description term / key    │
│ <dd>          │ Flow content               │ Description value / detail│
│ <figure>      │ Flow content + <figcaption>│ Independent movable unit  │
│ <figcaption>  │ Flow content               │ Must be first/last child  │
│ <blockquote>  │ Flow content               │ cite="https://..." URL    │
│ <pre>         │ Phrasing content           │ Preserves spaces/newlines │
└───────────────┴────────────────────────────┴───────────────────────────┘
```

---

## 13 Further Reading

- [WHATWG HTML Living Standard: Section 4.4 Grouping Content](https://html.spec.whatwg.org/multipage/grouping-content.html)
- [MDN Web Docs: Description List (&lt;dl&gt;)](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/dl)
- [MDN Web Docs: The Figure with Optional Caption (&lt;figure&gt;)](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/figure)
- [W3C Web Accessibility Initiative (WAI): Page Structure - Lists](https://www.w3.org/WAI/tutorials/page-structure/content/#lists)
- [Scott O'Hara: 'Fixing' Lists with role="list"](https://www.scottohara.me/blog/2019/01/12/lists-and-safari.html)

---

| [Previous: 03 Text Content & Typography](03-text-content-and-typography.md) | [Section Index](README.md) | [Next: 05 Links & URLs](05-links-and-urls.md) |
