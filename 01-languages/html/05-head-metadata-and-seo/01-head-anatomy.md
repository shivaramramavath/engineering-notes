# Document Head Anatomy, Parser Execution Flow, and Optimal Tag Ordering

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.2 The document's head) |
| **Prerequisites** | `00-setup/02-how-browsers-turn-html-into-pixels.md`, `01-fundamentals/01-document-structure.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<head>`, `<meta>`, `<link>`, `<title>`, `<base>`) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, Chrome DevTools Network & Performance, WebPageTest |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Canonical Production `<head>` Template](#example-1-the-canonical-production-head-template)
   - [Example 2: Demonstrating the `<base>` Element and URL Resolution](#example-2-demonstrating-the-base-element-and-url-resolution)
   - [Example 3: Browser Parser Self-Closing of `<head>` on Stray Body Tags](#example-3-browser-parser-self-closing-of-head-on-stray-body-tags)
   - [Example 4: Accelerating Critical Rendering Path via Tag Reordering](#example-4-accelerating-critical-rendering-path-via-tag-reordering)
   - [Example 5: HTTP 103 Early Hints Integration with Head Elements](#example-5-http-103-early-hints-integration-with-head-elements)
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

The `<head>` element represents the machine-readable command center of an HTML document. Unlike the `<body>`, which contains content rendered visually into the viewport, the `<head>` contains metadata that instructs the browser engine, search crawlers, social share scrapers, and operating systems how to interpret, render, secure, and index the webpage.

In high-performance web engineering, the **ordering of tags inside `<head>` directly determines your site's Core Web Vitals**. 
- Placing a synchronous `<script>` before a stylesheet delays CSSOM construction.
- Placing `<meta charset>` deep within the head can trigger full document re-parsing.
- Delaying `<title>` hurts SEO crawl efficiency and tab recognition.
- Putting a render-blocking tag before an LCP preload starves the network pipeline.

Mastering head anatomy and strict tag ordering guarantees that browser preload scanners discover critical assets immediately, eliminating render bottlenecks and maximizing search engine visibility.

---

## 2. Core Concepts

### Allowed Elements Inside `<head>`
According to the WHATWG specification, the `<head>` element has a strict content model. Only the following 8 elements are legally permitted inside `<head>`:
1. **`<title>`**: The document title shown in browser tab bars and search engine result pages (SERPs). Exactly one `<title>` element is mandatory.
2. **`<meta>`**: Various types of metadata (character encoding, viewport scaling, descriptions, Open Graph, CSP).
3. **`<link>`**: Relationships to external resources (stylesheets, favicons, canonical URLs, preloads).
4. **`<style>`**: Embedded internal CSS rules.
5. **`<script>`**: Executable code or data blocks (JSON-LD, Speculation Rules, JS modules).
6. **`<base>`**: Specifies the base URL and default browsing target for all relative URLs in the document.
7. **`<noscript>`**: Fallback content displayed only when JavaScript execution is disabled.
8. **`<template>`**: Inert HTML fragments intended for JavaScript cloning.

> [!WARNING]
> Any visual or content-holding elements (e.g., `<div>`, `<img>`, `<p>`, `<h1>`, or raw unescaped text) placed inside `<head>` will force the browser HTML parser to **immediately close the `<head>` element implicitly** and open the `<body>`. Any subsequent `<meta>`, `<link>`, or `<title>` tags will be expelled into the `<body>`, completely ruining metadata and preloading!

### The `<base>` Element
The `<base>` element specifies the base URL against which all relative URLs (in `<a>`, `<img>`, `<link>`, `<script>`, `<form>`) are resolved:
```html
<base href="https://cdn.example.com/assets/" target="_blank">
```
- If present, `<base>` **must appear before any other element** that contains relative URLs.
- Only one `<base>` element is permitted per document.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Browser HTML Parser & Head Execution Flow                              |
+-----------------------------------------------------------------------------------------+

  Byte Stream arrives from network
       |
       v
  1. Inspect first 1024 bytes -> Locate <meta charset="utf-8">
     (Locks in character decoder; prevents buffer rewind)
       |
       v
  2. Parse <meta name="viewport" content="...">
     (Initializes layout viewport dimensions for CSS media queries)
       |
       v
  3. Preload Scanner scans upcoming head tokens:
     - Dispatches HTTP requests for <link rel="preload">
     - Dispatches DNS/TCP handshakes for <link rel="preconnect">
     - Queues critical stylesheets (<link rel="stylesheet">)
       |
       v
  4. Encounter <script defer src="..."> or <script type="module">:
     - Dispatches background download without pausing HTML tokenizer!
     - Main thread continues parsing without delay.
       |
       v
  5. Encounter </head> -> Transition tokenizer state to "after head" -> Open <body>!
```

---

## 4. Syntax & Usage

### The Optimal Head Tag Ordering Checklist

To achieve maximum performance and eliminate render blocking, order `<head>` elements in this exact sequence:

1. **`<meta charset="utf-8">`**: Must be within the first 1024 bytes of the document.
2. **`<meta name="viewport" ...>`**: Defines layout viewport dimensions for mobile devices.
3. **`<title>`**: Essential for browser tabs, bookmarks, and screen readers.
4. **`<base>`** (if used): Must precede any relative URL declarations.
5. **`<link rel="preconnect">` / `<link rel="dns-prefetch">`**: Warms up third-party network sockets.
6. **`<link rel="preload">`**: High-priority assets (LCP hero image, critical web fonts).
7. **Critical CSS (`<style>` or `<link rel="stylesheet">`)**: Styles needed for initial paint.
8. **Deferred Scripts (`<script defer>` or `<script type="module">`)**: Executed after document parse.
9. **SEO & Social Metadata**: `<meta name="description">`, `rel="canonical"`, Open Graph (`og:*`), Twitter Cards.
10. **Icons & PWA Manifest**: `<link rel="icon">`, `<link rel="manifest">`, `<meta name="theme-color">`.

---

## 5. Practical Examples

### Example 1: The Canonical Production `<head>` Template
A battle-tested, high-performance `<head>` configuration adhering strictly to modern standards and optimal tag order.

```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <!-- 1. Character Encoding (Within first 1024 bytes) -->
  <meta charset="utf-8" />

  <!-- 2. Responsive Mobile Viewport -->
  <meta name="viewport" content="width=device-width, initial-scale=1" />

  <!-- 3. Document Title -->
  <title>High-Performance Cloud Architecture | Acme Corp</title>

  <!-- 4. Early Resource Hints (Preconnect to critical external CDN) -->
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />

  <!-- 5. Preload Critical LCP Assets (Web Font and Hero Banner) -->
  <link 
    rel="preload" 
    as="font" 
    type="font/woff2" 
    href="/fonts/inter-bold.woff2" 
    crossorigin 
  />
  <link 
    rel="preload" 
    as="image" 
    href="/images/hero-1200.avif" 
    type="image/avif" 
    fetchpriority="high" 
  />

  <!-- 6. Critical Stylesheet (Blocks render until downloaded, which is desired for base styles) -->
  <link rel="stylesheet" href="/css/critical-styles.css" />

  <!-- 7. Asynchronous / Deferred Scripts (Never block parser!) -->
  <script type="module" src="/js/app.js"></script>

  <!-- 8. Primary SEO Metadata -->
  <meta 
    name="description" 
    content="Enterprise-grade distributed computing infrastructure. Deploy serverless container clusters across 40 global edge regions." 
  />
  <link rel="canonical" href="https://example.com/cloud-architecture" />

  <!-- 9. Open Graph & Social Share Metadata -->
  <meta property="og:type" content="website" />
  <meta property="og:title" content="High-Performance Cloud Architecture | Acme Corp" />
  <meta property="og:description" content="Deploy serverless container clusters across 40 global edge regions." />
  <meta property="og:image" content="https://example.com/images/og-cloud-card.jpg" />
  <meta property="og:url" content="https://example.com/cloud-architecture" />

  <!-- 10. Twitter Card Metadata -->
  <meta name="twitter:card" content="summary_large_image" />
  <meta name="twitter:site" content="@AcmeCloud" />

  <!-- 11. Favicons, Web App Manifest & Browser Theme Colors -->
  <link rel="icon" href="/favicon.ico" sizes="32x32" />
  <link rel="icon" href="/icon.svg" type="image/svg+xml" />
  <link rel="apple-touch-icon" href="/apple-touch-icon.png" />
  <link rel="manifest" href="/manifest.webmanifest" />
  <meta name="color-scheme" content="light dark" />
  <meta name="theme-color" media="(prefers-color-scheme: light)" content="#ffffff" />
  <meta name="theme-color" media="(prefers-color-scheme: dark)" content="#0f172a" />
</head>
<body>
  <header><h1>Welcome to Acme Cloud</h1></header>
</body>
</html>
```

---

### Example 2: Demonstrating the `<base>` Element and URL Resolution
Illustrates how the `<base>` tag changes the resolution root for all relative hyperlinks, scripts, and image tags.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>Base Element Demonstration</title>

  <!-- 
    All relative URLs will resolve against this CDN base,
    and all links will open in a new tab by default!
  -->
  <base href="https://static.cdn-assets.com/v3/" target="_blank" />

  <!-- Resolves to: https://static.cdn-assets.com/v3/css/style.css -->
  <link rel="stylesheet" href="css/style.css" />
</head>
<body>
  <!-- Resolves to: https://static.cdn-assets.com/v3/images/logo.png -->
  <img src="images/logo.png" alt="Company Logo" />

  <!-- 
    Resolves to: https://static.cdn-assets.com/v3/about.html
    Opens in a new browser tab because of target="_blank" on <base>!
  -->
  <a href="about.html">About Our CDN</a>

  <!-- Target override on specific anchor -->
  <a href="https://example.com" target="_self">Return to Main Site</a>
</body>
</html>
```

---

### Example 3: Browser Parser Self-Closing of `<head>` on Stray Body Tags
Demonstrating the catastrophic bug where an errant `<img>` or `<div>` inside `<head>` causes the browser to close `<head>` prematurely.

```html
<!-- DANGEROUS ERROR EXAMPLE -->
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>Accidental Premature Head Termination</title>

  <!-- ACCIDENTAL STRAY TAG IN HEAD! -->
  <img src="/logo.png" alt="Company logo" />

  <!-- 
    CRITICAL BROWSER PARSER ERROR:
    The moment the parser hits <img>, it implicitly injects:
    </head><body>
    
    All tags below are expelled into the <body>!
  -->
  <link rel="stylesheet" href="/critical.css" /> <!-- Now in <body>! -->
  <meta name="description" content="This description is ignored by search engines!" />
</head>
<body>
  <h1>Page Content</h1>
</body>
</html>
```

---

### Example 4: Accelerating Critical Rendering Path via Tag Reordering
Comparing a poorly ordered head with a performance-optimized head.

```html
<!-- SLOW HEAD (ANTI-PATTERN) -->
<head>
  <!-- Synchronous external script blocks parser before styles or preloads! -->
  <script src="/scripts/heavy-analytics.js"></script>
  <meta name="description" content="Slow page description" />
  <!-- Stylesheet blocked by script! -->
  <link rel="stylesheet" href="/main.css" />
  <!-- Charset placed too late! May trigger re-parse! -->
  <meta charset="utf-8" />
  <title>Slow Page</title>
</head>

<!-- FAST HEAD (OPTIMIZED) -->
<head>
  <!-- Charset first! -->
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Fast Page</title>
  <!-- Stylesheet loaded immediately without script block! -->
  <link rel="stylesheet" href="/main.css" />
  <!-- Analytics script deferred! -->
  <script defer src="/scripts/heavy-analytics.js"></script>
</head>
```

---

### Example 5: HTTP 103 Early Hints Integration with Head Elements
Modern edge servers (Cloudflare, Fastly, Nginx) read `<link rel="preload">` in the HTML head and emit an HTTP `103 Early Hints` response before the server even finishes database lookups.

```http
HTTP/1.1 103 Early Hints
Link: </css/main.css>; rel=preload; as=style
Link: </fonts/inter.woff2>; rel=preload; as=font; crossorigin
Link: <https://api.example.com>; rel=preconnect

HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
```

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Dashboard</title>
  <!-- Matches the 103 Early Hints preloaded above -->
  <link rel="stylesheet" href="/css/main.css" />
</head>
<body>
  <div id="app"></div>
</body>
</html>
```

---

## 6. Best Practices

- [ ] **Always declare `<meta charset="utf-8">` as the very first child of `<head>`**: Must reside entirely within the first 1024 bytes of the document to avoid costly buffer encoding restarts.
- [ ] **Declare `<meta name="viewport" ...>` immediately after `<meta charset>`**: Ensures device width and pixel density calculations are active before stylesheets load.
- [ ] **Always include a descriptive, unique `<title>`**: Every document must have exactly one `<title>` element inside `<head>`.
- [ ] **Place stylesheets before deferred scripts**: Keeps the critical rendering path clean and allows the CSSOM to construct concurrently with script streaming.
- [ ] **Avoid the `<base>` tag unless strictly necessary**: `<base>` modifies every relative URL across the entire page (including in-page `#fragment` jump links and SVGs), frequently causing hard-to-debug link breakage.
- [ ] **Never place visual tags (`<img>`, `<div>`, `<p>`) inside `<head>`**: Causes the browser parser to immediately close `<head>` and dump subsequent metadata into `<body>`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Placing `<meta charset>` after 1024 bytes or after `<style>` | **Critical** | Browser may buffer and re-parse the entire HTML document if encoding changes. | Place `<meta charset="utf-8">` as the first line inside `<head>`. |
| Placing synchronous `<script src="...">` before `<link rel="stylesheet">` | **High** | Script blocks the HTML parser, delaying stylesheet discovery and increasing First Contentful Paint (FCP). | Add `defer` or `type="module"` to the script. |
| Inserting visual elements (`<img>`, `<div>`, `<p>`) inside `<head>` | **Critical** | Parser implicitly emits `</head><body>`, pushing all subsequent metadata and links into `<body>`. | Keep only allowed metadata elements in `<head>`. |
| Declaring multiple `<title>` tags | **Moderate** | Browser takes the first title and ignores subsequent ones; creates validator warnings. | Retain only one `<title>` per document. |
| Using `<base href="...">` and breaking in-page hash anchors | **High** | Clicking `<a href="#section-2">` jumps to `https://base-url.com/#section-2` instead of the current page! | Avoid `<base>` or write full page URLs in anchors. |
| Placing `<base>` after relative `<link>` or `<script>` tags | **High** | Earlier elements resolve against the document URL, while later ones resolve against the base URL. | Place `<base>` before any elements with URLs. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 2.4.2 Page Titled (Level A)**: Every webpage must have a descriptive `<title>` element that identifies the specific purpose or topic of the page (e.g., `"Shopping Cart (3 items) | Acme Store"`). Screen readers announce the `<title>` first upon page load.

### Core Web Vitals
- **First Contentful Paint (FCP)**: Placing critical CSS and font preloads at the top of `<head>` accelerates stylesheet availability and avoids invisible text flashes (FOIT).
- **Cumulative Layout Shift (CLS)**: The `<meta name="viewport">` tag prevents mobile browsers from defaulting to desktop 980px viewport simulation, preventing layout shifts.

---

## 9. Comparison / When to Use What

| Element / Technique | Placement in `<head>` | Purpose | When to Use |
| :--- | :--- | :--- | :--- |
| **`<meta charset="utf-8">`** | Line 1 | Locks byte decoder to UTF-8 | **Every single HTML page.** |
| **`<meta name="viewport">`** | Line 2 | Enables responsive mobile scaling | **Every responsive webpage.** |
| **`<title>`** | Line 3 | Accessible page title, tab title, SEO | **Every single HTML page.** |
| **`<link rel="preload">`** | Lines 4-6 | Downloads critical font or LCP image early | Above-the-fold hero images, primary fonts. |
| **`<script defer>`** | Lines 7-8 | Executes scripts after HTML parsing completes | All standard UI application scripts. |
| **`<base>`** | Top of `<head>` | Centralizes relative URL root directory | Specialized static asset CDNs (use with caution). |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must <code>&lt;meta charset="utf-8"&gt;</code> appear within the first 1024 bytes of an HTML document?</strong></summary>

Browsers begin parsing an incoming HTML byte stream using a provisional encoding. The WHATWG specification mandates that browsers inspect only the first 1024 bytes for a character encoding declaration. If the declaration appears after the 1024-byte boundary and specifies an encoding different from the initial guess, the browser must discard all previously generated tokens, flush the DOM, and re-parse the document from byte 0, causing a severe performance penalty.
</details>

<details>
<summary><strong>2. What happens if a developer accidentally places a <code>&lt;div&gt;</code> or an <code>&lt;img&gt;</code> inside the <code>&lt;head&gt;</code>?</strong></summary>

The HTML Living Standard parser state machine operates in the "in head" insertion mode. Only specific metadata tags are valid in this mode. If the tokenizer encounters an unexpected start tag like `<div>` or `<img>`, the parser executes the "anything else" fallback rule: it automatically emits a virtual `</head>` end token, emits a `<body>` start token, and transitions to the "in body" insertion mode. Any subsequent tags in the HTML file (such as `<title>`, `<meta>`, or `<link>`) are parsed as children of `<body>`, invalidating metadata and social tags.
</details>

<details>
<summary><strong>3. What is the optimal execution-order sequence of tags inside <code>&lt;head&gt;</code> to optimize the Critical Rendering Path?</strong></summary>

1. `<meta charset="utf-8">` (Locks encoding within 1024 bytes).
2. `<meta name="viewport" content="...">` (Configures viewport dimensions for media queries).
3. `<title>` (Identifies page for screen readers and tab).
4. `<link rel="preconnect">` (Warms DNS/TLS sockets).
5. `<link rel="preload">` (Preloads LCP image and critical fonts).
6. `<link rel="stylesheet">` (Loads critical CSSOM rules).
7. `<script defer>` or `<script type="module">` (Downloads scripts asynchronously).
8. Secondary SEO and social tags (Open Graph, Twitter Cards, Favicons).
</details>

<details>
<summary><strong>4. How does the <code>&lt;base&gt;</code> element impact in-page fragment links (e.g., <code>&lt;a href="#faq"&gt;</code>)?</strong></summary>

The `<base>` element re-roots ALL relative URLs against its `href` attribute. If a page at `https://example.com/docs/page.html` has `<base href="https://example.com/assets/">`, clicking `<a href="#faq">` will resolve to `https://example.com/assets/#faq` rather than `https://example.com/docs/page.html#faq`. This breaks in-page navigation anchors unless full absolute paths are used.
</details>

<details>
<summary><strong>5. Can an HTML document have multiple <code>&lt;base&gt;</code> tags?</strong></summary>

No. The WHATWG HTML specification states that there must be at most one `<base>` element in a document. If multiple `<base>` elements are declared, the browser obeys the *first* one encountered with an `href` or `target` attribute and completely ignores all subsequent `<base>` tags.
</details>

<details>
<summary><strong>6. What are the only 8 elements legally allowed inside the <code>&lt;head&gt;</code>?</strong></summary>

1. `<title>`
2. `<meta>`
3. `<link>`
4. `<style>`
5. `<script>`
6. `<base>`
7. `<noscript>`
8. `<template>`
</details>

<details>
<summary><strong>7. How does the browser Preload Scanner interact with elements in the <code>&lt;head&gt;</code>?</strong></summary>

The speculative Preload Scanner runs concurrently with the HTML tokenizer. While the main parser may be waiting for a critical stylesheet to download and parse, the Preload Scanner looks ahead through `<head>` and `<body>` tokens, identifying `<link rel="stylesheet">`, `<script src="...">`, `<link rel="preload">`, and `<img>` tags, dispatching network requests in parallel long before the main DOM parser reaches them.
</details>

<details>
<summary><strong>8. What is HTTP 103 Early Hints and how does it relate to <code>&lt;head&gt;</code> tags?</strong></summary>

HTTP 103 Early Hints is an informational HTTP status code sent by a web server or CDN while the backend application is still assembling the main HTML response. It passes `Link: </style.css>; rel=preload; as=style` headers matching the `<head>` resource links, allowing the browser to begin downloading critical stylesheets and fonts while waiting for the main HTML response.
</details>

---

## 11. Practice Exercises

### Exercise 1: Valid Minimal Document Head (Easy)
- **Task**: Author a minimal, 100% valid HTML `<head>` containing character encoding, mobile viewport, and an accessible title in optimal order.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Minimal Accessible Webpage</title>
</head>
```
</details>

---

### Exercise 2: Base Element Link Resolution (Medium)
- **Task**: Given the following head:
  ```html
  <head>
    <base href="https://media.acme.org/v2/" target="_blank" />
  </head>
  ```
  Write the fully resolved absolute URLs for:
  1. `<img src="icons/check.svg">`
  2. `<a href="../index.html">`
  3. `<a href="/login">`

<details>
<summary>View Solution</summary>

1. `https://media.acme.org/v2/icons/check.svg`
2. `https://media.acme.org/index.html` (the `../` navigates up one directory from `v2/`)
3. `https://media.acme.org/login` (a leading slash resolves against the domain origin)
</details>

---

### Exercise 3: Head Tag Ordering Refactoring (Hard)
- **Task**: The following `<head>` is in disarray, causing render blocking, font flashes, and validator errors. Re-order and clean it up to achieve optimal performance:
  ```html
  <head>
    <script src="bundle.js"></script>
    <meta name="description" content="Welcome to our store.">
    <div>Welcome banner</div>
    <link rel="stylesheet" href="styles.css">
    <meta charset="utf-8">
    <link rel="preload" as="font" href="font.woff2" type="font/woff2" crossorigin>
    <title>Store Home</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  ```

<details>
<summary>View Solution</summary>

**Refactored High-Performance `<head>`:**

```html
<head>
  <!-- 1. Charset first! -->
  <meta charset="utf-8" />

  <!-- 2. Viewport second! -->
  <meta name="viewport" content="width=device-width, initial-scale=1" />

  <!-- 3. Title third! -->
  <title>Store Home</title>

  <!-- 4. Critical Font Preload -->
  <link rel="preload" as="font" href="font.woff2" type="font/woff2" crossorigin />

  <!-- 5. Stylesheet before scripts -->
  <link rel="stylesheet" href="styles.css" />

  <!-- 6. Defer script (bundle.js should never block parse) -->
  <script defer src="bundle.js"></script>

  <!-- 7. SEO metadata -->
  <meta name="description" content="Welcome to our store." />

  <!-- NOTE: The stray <div> was deleted because divs are ILLEGAL in <head>! -->
</head>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                     <head> ANATOMY CHEAT SHEET                                  |
+-------------------------------------------------------------------------------------------------+
| Canonical Order          | Tag / Pattern                           | Purpose                    |
+-------------------------------------------------------------------------------------------------+
| 1. Encoding (First 1024b)| <meta charset="utf-8">                  | Prevents document re-parse |
| 2. Viewport              | <meta name="viewport" content="...">    | Sets mobile layout width   |
| 3. Title                 | <title>Unique Page Title</title>        | Screen reader & tab label  |
| 4. Base (Optional)       | <base href="..." target="...">          | Roots relative URLs        |
| 5. Preconnect            | <link rel="preconnect" href="...">      | Warms DNS/TLS sockets      |
| 6. Preload               | <link rel="preload" as="font|image">    | Accelerates LCP assets     |
| 7. Critical CSS          | <link rel="stylesheet" href="...">      | Critical rendering path    |
| 8. Deferred JS           | <script defer> / <script type="module"> | Executes after DOM parse   |
| 9. Metadata & SEO        | <meta name="description">, og:*, rel=can| Search & social shares    |
| 10. Favicons / Manifest  | <link rel="icon">, <link rel="manifest">| Browser tabs & PWA install |
| ILLEGAL IN HEAD          | <div>, <img>, <p>, <h1>, raw text       | Closes <head> immediately! |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: The `head` element](https://html.spec.whatwg.org/multipage/semantics.html#the-head-element)
- [Harry Roberts: The Optimal Head Order for Web Performance](https://csswizardry.com/)
- [MDN Web Docs: What's in the head? Metadata in HTML](https://developer.mozilla.org/en-US/docs/Learn/HTML/Introduction_to_HTML/The_head_metadata_in_HTML)
- [Google Web Fundamentals: Rendering Performance & Critical Rendering Path](https://web.dev/explore/fast)

---

[← Previous: Module Overview](README.md) | [Section Index](README.md) | [Next: Meta Charset, Viewport & Color Scheme →](02-meta-charset-viewport-and-color-scheme.md)
