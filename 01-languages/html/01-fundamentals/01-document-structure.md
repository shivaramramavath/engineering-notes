# Document Structure: DOCTYPE, html, head, body & lang

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md`, `00-setup/05-your-first-page.md` |
| **Time to Complete** | 30 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal Standard) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The Anatomy of Document-Level Elements](#the-anatomy-of-document-level-elements)
4. [Language & Text Direction: lang and dir](#language--text-direction-lang-and-dir)
5. [Syntax & Usage](#syntax--usage)
6. [Browser Engine Support & Baseline Status](#browser-engine-support--baseline-status)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
11. [Comparison Matrix: Document Wrapper Elements](#comparison-matrix-document-wrapper-elements)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

At the highest level of abstraction, an HTML document is not a flat list of tags; it is an **enclosed hierarchy** representing an entire document browsing context.

The document structure dictates how browser engines initialize memory, select parsing modes, load language dictionaries, and configure the accessibility tree:
- **`<!DOCTYPE html>`** signals to the parser that the document must be evaluated using modern standard layout rules rather than legacy Quirks Mode.
- **`<html lang="...">`** establishes the root document node and declares the default natural language for speech synthesis and typography.
- **`<head>`** acts as the machine-readable metadata registry, configuring character encoding, responsive viewport boundaries, scripts, and stylesheets before rendering begins.
- **`<body>`** encapsulates the human-visible content rendered inside the browser's graphical viewport.

Omitting or misconfiguring these foundational wrapper elements leads directly to character corruption, unstyled mobile views, accessibility failures, and broken CSS inheritance.

---

## Core Concepts

### 1. The Document Type Declaration (DOCTYPE)
In modern HTML5, the DOCTYPE is not an SGML tag and does not reference an external DTD schema. It is a **parser directive** that serves a single, crucial function:
```html
<!DOCTYPE html>
```
If present as the first token of the document stream, the browser activates **Standards Mode** (No-Quirks Mode). If missing, malformed, or preceded by an XML declaration, the engine drops into **Quirks Mode**, emulating bugs from 1998 (such as broken box-sizing in table cells and non-standard layout calculations).

### 2. The Root Element (`<html>`)
The `<html>` element represents the root node of the DOM tree. Every other element in the document (except the DOCTYPE and comment tokens at the root level) must be a descendant of `<html>`.
- It is the container where global document metadata like language (`lang`) and text orientation (`dir`) must be declared so they cascade naturally down the entire DOM hierarchy.

### 3. The Boundary Between Head and Body
- **`<head>` (Document Metadata)**: Contains elements that describe the document or link external dependencies: `<meta>`, `<title>`, `<link>`, `<style>`, `<script>`, `<noscript>`, `<base>`, `<template>`. No visual content (text, paragraphs, images) is permitted inside `<head>`.
- **`<body>` (Document Content)**: Contains everything intended for human interaction and visual rendering on the canvas.

---

## The Anatomy of Document-Level Elements

```
+-----------------------------------------------------------------------------------------+
|                                  THE ROOT DOM HIERARCHY                                 |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  Document Root                                                                          |
|    |                                                                                    |
|    +-- DOCTYPE: html (Triggers Standards Mode)                                          |
|    |                                                                                    |
|    +-- Element: html (lang="en", dir="ltr")                                             |
|          |                                                                              |
|          +-- Element: head                                                              |
|          |     |-- Element: meta (charset="UTF-8")                                      |
|          |     |-- Element: meta (name="viewport")                                      |
|          |     |-- Element: title ("Document Title")                                    |
|          |     +-- Element: link (rel="stylesheet")                                     |
|          |                                                                              |
|          +-- Element: body                                                              |
|                |-- Element: header                                                      |
|                |-- Element: main                                                        |
|                +-- Element: footer                                                      |
+-----------------------------------------------------------------------------------------+
```

---

## Language & Text Direction: lang and dir

### 1. The `lang` Attribute (BCP 47 Language Tags)
The `lang` attribute declares the human language of the element's text content. Values must conform to **IETF BCP 47** (RFC 5646):
- Primary language subtag: `en` (English), `es` (Spanish), `fr` (French), `de` (German), `ja` (Japanese), `ar` (Arabic).
- Optional region/dialect subtag: `en-US` (American English), `en-GB` (British English), `zh-CN` (Simplified Chinese, China), `zh-TW` (Traditional Chinese, Taiwan).

#### What Depends on `lang`:
1. **Screen Readers**: Selects the appropriate speech synthesizer and phoneme pronunciation dictionary.
2. **CSS Hyphenation**: The CSS property `hyphens: auto` requires `lang` to determine where words may be hyphenated at line wraps.
3. **Font Rendering & Glyph Selection**: Certain characters in Unicode share the same codepoint but render different glyphs depending on whether the language is Chinese, Japanese, or Korean (CJK Unified Ideographs).
4. **Browser Translation**: Enables automatic prompt: *"Translate this page?"*

### 2. The `dir` Attribute (Bidirectional Text)
Specifies the base directionality of text and layout:
- `dir="ltr"`: Left-to-right (default for Latin, Cyrillic, Greek scripts).
- `dir="rtl"`: Right-to-left (mandatory for Arabic, Hebrew, Persian, Urdu).
- `dir="auto"`: Instructs the engine to inspect the first strong directional character in the text and set directionality dynamically (useful for user-generated content).

---

## Syntax & Usage

```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Standard Document Structure</title>
</head>
<body>
  <h1>Valid Document Root</h1>
  <p>All visible elements reside inside the body container.</p>
</body>
</html>
```

---

## Browser Engine Support & Baseline Status

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available (Universal HTML5 Foundation)                        |
+-------------------+--------------------------------------------------------------------------------+
| Chromium (Blink)  | Chrome 1+, Edge 1+, Android WebView                                            |
+-------------------+--------------------------------------------------------------------------------+
| Gecko (Firefox)   | Firefox 1+, Firefox Mobile                                                     |
+-------------------+--------------------------------------------------------------------------------+
| WebKit (Safari)   | Safari 1+, iOS Safari 1+                                                       |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Minimal Valid Document Structure
- **Goal**: Construct the cleanest, standards-compliant document skeleton.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Minimal Valid Template</title>
</head>
<body>
  <h1>Document Title</h1>
  <p>Core content paragraph.</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: Satisfies all W3C validation rules: standard HTML5 DOCTYPE, language declaration on `<html>`, UTF-8 charset, non-empty `<title>`, and valid content inside `<body>`.

### Example 2: Multilingual Document with Mixed LTR and RTL Sections
- **Goal**: Demonstrate how `lang` and `dir` cascade and how to override them for foreign quotes.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Multilingual Literature</title>
</head>
<body>
  <header>
    <h1>Global Typography</h1>
  </header>
  <main>
    <article>
      <h2>Arabic Calligraphy</h2>
      <p>The standard Arabic greeting is:</p>
      <!-- Overriding language and directionality for a specific blockquote -->
      <blockquote lang="ar" dir="rtl">
        <p>السلام عليكم ورحمة الله وبركاته</p>
      </blockquote>
    </article>
  </main>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: The document root establishes English LTR layout. The `<blockquote>` explicitly overrides both `lang="ar"` and `dir="rtl"`, allowing Arabic text to align to the right margin and pronounce correctly in screen readers.

### Example 3: Edge Case — Handling Omitted `<html>`, `<head>`, or `<body>`
- **Goal**: Observe the browser's parser synthesizing missing structural tags.
- **Code**:
```html
<!DOCTYPE html>
<meta charset="UTF-8">
<title>Omitted Wrapper Tags</title>
<p>The parser automatically inserts html, head, and body nodes into the DOM tree.</p>
```
- **Real Validator & DOM Output**:
```
DOM Inspection in DevTools:
#document
  <!DOCTYPE html>
  <html>
    <head>
      <meta charset="UTF-8">
      <title>Omitted Wrapper Tags</title>
    </head>
    <body>
      <p>The parser automatically inserts html, head, and body nodes into the DOM tree.</p>
    </body>
  </html>
```
- **Explanation**: The WHATWG specification allows omitting `<html>`, `<head>`, and `<body>` tags in certain contexts. The parser automatically synthesizes them in memory. However, omitting `<html>` is bad practice because you cannot specify `lang="en"`.

### Example 4: Failure Mode — Misplacing Content inside `<head>`
- **Goal**: Analyze what happens when visible text or elements are placed in `<head>`.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Invalid Head Content</title>
  <!-- FATAL MISTAKE: Visible text placed inside <head> -->
  <h1>Welcome to our Company</h1>
  <p>Our services are listed below.</p>
</head>
<body>
  <p>Standard body content.</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
Computed DOM after parser error recovery:
html
  ├── head
  │     ├── meta (charset="UTF-8")
  │     └── title ("Invalid Head Content")
  └── body (Parser implicitly CLOSED head and OPENED body here!)
        ├── h1 ("Welcome to our Company")
        ├── p ("Our services are listed below.")
        └── p ("Standard body content.")
```
- **Explanation**: The moment the HTML parser encounters an element that is not permitted inside `<head>` (like `<h1>` or text), **it immediately and silently closes the `<head>` element and opens the `<body>` element**! Any subsequent metadata tags (like `<link rel="stylesheet">`) now reside inside `<body>`, impairing performance.

### Example 5: Production Enterprise Document Template
- **Goal**: Full enterprise document with Open Graph, color scheme, and preconnect hints.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="color-scheme" content="light dark">
  <meta name="description" content="Enterprise documentation for distributed cloud infrastructure.">
  <title>Enterprise Cloud Architecture - Core Platform</title>

  <!-- Preconnect to critical assets -->
  <link rel="preconnect" href="https://assets.example.com" crossorigin>

  <!-- Canonical link -->
  <link rel="canonical" href="https://cloud.example.com/docs">

  <!-- Core stylesheet -->
  <link rel="stylesheet" href="/css/main.css">
</head>
<body>
  <a href="#main-content" class="skip-link">Skip to main content</a>
  <header>
    <p>Acme Enterprise Systems</p>
  </header>
  <main id="main-content">
    <h1>Platform Architecture Overview</h1>
    <p>High-availability multi-region active-active cluster documentation.</p>
  </main>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: Includes `<meta name="color-scheme">` to inform the browser engine that the document natively supports both dark and light operating system themes, preventing default white background flashes.

---

## Best Practices

1. **Always Declare `lang` on the `<html>` Tag**: Never omit `lang`. Specify the exact BCP 47 code (e.g. `lang="en"`, `lang="en-US"`, `lang="es"`).
2. **Place `<meta charset="UTF-8">` as the Very First Child of `<head>`**: Guarantees character decoding starts immediately without buffering delays.
3. **Never Place Renderable Elements in `<head>`**: Tags like `<div>`, `<p>`, `<h1>`, or raw text will immediately force the parser to close `<head>` prematurely.
4. **Use `<meta name="color-scheme" content="light dark">`**: Informs the browser that your page handles dark mode, preventing white flashes during initial paint.
5. **Declare `dir="rtl"` Explicitly for Right-to-Left Languages**: When authoring Arabic, Hebrew, or Persian sites, set `<html lang="ar" dir="rtl">` on the root element.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Stylesheets ignored or load late** | Content (text or `<h1>`) placed before `<link>` inside `<head>`, forcing premature `<body>` opening. | Ensure only metadata elements reside in `<head>`. |
| **Browser translation prompt never triggers** | Missing `lang` attribute on `<html>` tag. | Always specify `<html lang="...">`. |
| **Foreign phrases mispronounced by screen reader** | Foreign words embedded in English paragraph without inline `lang` attribute. | Wrap foreign phrases in `<span lang="es">...</span>`. |
| **Arabic text aligns left and punctuation flips** | Missing `dir="rtl"` attribute. | Declare `dir="rtl"` on root `<html>` or container element. |
| **Page renders in Quirks Mode** | XML declaration (`<?xml ...>`) or comment placed before `<!DOCTYPE html>`. | Make `<!DOCTYPE html>` the absolute first token on line 1. |
| **Accidental horizontal scrollbars on mobile** | Missing `<meta name="viewport" content="width=device-width, initial-scale=1.0">`. | Include standard viewport meta tag in `<head>`. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
- **The 1,024-Byte Rule**: The `<meta charset="UTF-8">` tag must appear within the first 1,024 bytes of the HTML response stream. If delayed, browsers buffer network bytes, increasing Time to First Meaningful Paint.

### Accessibility
- **WCAG 2.2 Success Criterion 3.1.1 (Language of Page - Level A)**: Requires the default human language of each web page to be programmatically determinable via the `lang` attribute.
- **WCAG 2.2 Success Criterion 3.1.2 (Language of Parts - Level AA)**: Requires any inline phrase written in a different language to declare its language via an inline `lang` attribute.

### SEO
- Googlebot and Bingbot use the root `lang` attribute and `<head>` canonical tags to serve the correct regional version of a page to international searchers.

### Security
- **Strict DOCTYPE Enforcement**: Modern browser security mechanisms (such as Cross-Origin Embedder Policy and strict MIME handling) require documents to run in Standards Mode. Quirks Mode relaxes certain security boundaries for legacy compatibility.

---

## Comparison Matrix: Document Wrapper Elements

| Element | Permitted Parent | Permitted Children | Primary Role | Mandatory in HTML5? |
|:---|:---|:---|:---|:---:|
| **`<!DOCTYPE html>`** | None (File root) | None (Parser directive) | Triggers Standards Mode | **Yes** |
| **`<html>`** | Document root | Exactly one `<head>`, followed by one `<body>` | Root container; defines `lang` & `dir` | Technically inferable; **Mandatory in practice** |
| **`<head>`** | `<html>` | Metadata elements only (`<meta>`, `<title>`, `<link>`, `<style>`, `<script>`) | Machine-readable metadata & resource links | Technically inferable; **Mandatory in practice** |
| **`<body>`** | `<html>` | Flow content (all visible visual elements) | Document canvas containing all visual UI | Technically inferable; **Mandatory in practice** |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What happens under the hood if a developer accidentally types visible text or an &lt;h1&gt; inside the &lt;head&gt; element?</strong></summary>

**Answer:**
The WHATWG HTML parser is a deterministic state machine that operates using **Insertion Modes**.
When the parser is in the `"in head"` insertion mode, it maintains a strict whitelist of permitted elements: `<base>`, `<link>`, `<meta>`, `<noframes>`, `<script>`, `<style>`, `<template>`, and `<title>`.

The instant the parser encounters an unexpected start tag (like `<h1>`) or any character token other than whitespace, the tree construction dispatcher executes its default error-recovery rule:
1. It **implicitly synthesizes an end tag for `<head>`** (`</head>`), terminating the head container.
2. It **implicitly switches the insertion mode to `"after head"` and then `"in body"`**, opening the `<body>` element.
3. It appends the `<h1>` as the first child of `<body>`.

**Catastrophic Consequence**: Any `<link rel="stylesheet">` or `<meta>` tags authored *after* the misplaced text now reside inside `<body>` instead of `<head>`, delaying stylesheet discovery by the parser and breaking resource prioritization.
</details>

<details>
<summary><strong>2. Explain how the BCP 47 language tagging system works in the lang attribute.</strong></summary>

**Answer:**
The `lang` attribute uses **IETF BCP 47** language tags, which follow a standardized hierarchical subtag format:
- **Language Subtag (2-3 letters)**: Identifies the primary language using ISO 639-1 (e.g., `en`, `es`, `zh`, `de`).
- **Script Subtag (4 letters, optional)**: Identifies the writing system using ISO 15924 (e.g., `zh-Hans` for Simplified Chinese, `zh-Hant` for Traditional Chinese, `sr-Cyrl` for Serbian Cyrillic).
- **Region Subtag (2 letters or 3 digits, optional)**: Identifies the national territory or dialect using ISO 3166-1 (e.g., `en-US`, `en-GB`, `fr-CA`, `pt-BR`).

Example: `lang="zh-Hans-CN"` indicates Chinese language, written in Simplified script, localized for the People's Republic of China.
</details>

<details>
<summary><strong>3. What is the difference between dir="rtl" and CSS text-align: right?</strong></summary>

**Answer:**
`dir="rtl"` and `text-align: right` operate at completely different layers of the web platform:
- **`text-align: right` (Visual CSS)**: Merely shifts visual text alignment to the right margin of the box. It **does not alter the semantic directionality** of the text. Punctuation (such as question marks, parentheses, and exclamation marks) will still be calculated as LTR and will appear on the wrong side of sentences in bidirectional scripts.
- **`dir="rtl"` (Semantic HTML)**: Directly configures the **Unicode Bidirectional Algorithm (Bidi)**. It reverses the logical reading order, positions punctuation correctly at the end of sentences, flips column ordering in tables, reverses flexbox/grid layout directions in CSS, and informs screen readers to announce characters in RTL sequence.
</details>

<details>
<summary><strong>4. Why should you never use the XML prologue &lt;?xml version="1.0"?&gt; in HTML documents?</strong></summary>

**Answer:**
The XML declaration `<?xml version="1.0" encoding="utf-8"?>` is strictly an XML syntax construct. 
When an HTML document begins with an XML declaration:
1. In modern HTML5 parsers, the XML prologue is not recognized as a valid DOCTYPE; it is treated as an invalid processing instruction or comment.
2. In Internet Explorer (IE 6 through 8) and older browser engines, encountering *anything* before the `<!DOCTYPE>` tag (even a comment or XML declaration) immediately caused the engine to ignore the DOCTYPE and drop into **Quirks Mode**.
Always begin files with `<!DOCTYPE html>` directly on line 1 without any preceding prologue.
</details>

<details>
<summary><strong>5. Can an HTML document contain multiple &lt;head&gt; or &lt;body&gt; tags? What does the parser do?</strong></summary>

**Answer:**
No, valid HTML permits exactly one `<head>` and one `<body>` per document. 
If an author mistakenly writes multiple `<head>` or `<body>` tags:
- **Multiple `<head>` tags**: The parser creates the first `<head>`. When it encounters a second `<head>` tag, the parser is already in `"in head"` mode; it ignores the second `<head>` token as redundant.
- **Multiple `<body>` tags**: When the parser encounters a second `<body>` tag while in `"in body"` mode, it discards the second tag. However, any attributes present on the second `<body>` tag are merged into the existing `<body>` node if those attributes were not already present.
</details>

<details>
<summary><strong>6. What is the difference between Standards Mode, Almost Standards Mode, and Quirks Mode?</strong></summary>

**Answer:**
- **Standards Mode (No-Quirks Mode)**: Triggered by `<!DOCTYPE html>`. Full conformance with modern CSS and HTML specifications.
- **Almost Standards Mode**: Historically triggered by transitional HTML4 doctypes. The only difference from Standards Mode is in table cells: images in table cells are laid out at the bottom of the cell (baseline alignment) without the standard font descender gap.
- **Quirks Mode**: Triggered by an omitted or malformed doctype. Emulates Netscape 4 / IE 5 behaviors: box-sizing includes padding/border on elements, percentage heights behave inconsistently, and font sizes inherit improperly inside tables.
</details>

<details>
<summary><strong>7. What is the color-scheme meta tag, and why should it be in &lt;head&gt;?</strong></summary>

**Answer:**
`<meta name="color-scheme" content="light dark">` informs the browser rendering engine that the web page is designed to support both light and dark operating system preferences.
When present in `<head>`, the browser engine:
1. Automatically renders form controls, scrollbars, and user agent default elements with dark mode colors when the user's OS is in dark mode.
2. Prevents the "white flash" during navigation: without this tag, the browser initializes a white background canvas before the stylesheet loads; with this tag, the browser initializes a dark canvas immediately for dark mode users.
</details>

<details>
<summary><strong>8. Is the &lt;base&gt; tag safe to use in modern web applications? What risks does it introduce?</strong></summary>

**Answer:**
The `<base href="...">` tag specifies the base URL for all relative URLs in the document (links, images, scripts).
While technically valid, it is considered a **fragile anti-pattern** in modern applications:
1. **Hash Fragment Anchors**: Clicking `<a href="#section">` will navigate relative to the base URL (e.g. `https://example.com/other/#section`) rather than scrolling the current page!
2. **SVG Mask & ClipPath Breakage**: Inline SVGs using `fill="url(#gradient)"` will fail because `#gradient` resolves against the `<base>` URL.
3. **Security**: If an XSS vulnerability exists, an attacker injecting a malicious `<base>` tag can hijack all relative script and asset URLs.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Authoring a Valid Multilingual Document Root</strong></summary>

**Problem:** Write the complete document wrapper for a Japanese website with title "東京ガイド" (Tokyo Guide) using UTF-8 and modern DOCTYPE.

**Solution:**
```html
<!DOCTYPE html>
<html lang="ja" dir="ltr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>東京ガイド</title>
</head>
<body>
  <h1>東京へようこそ</h1>
  <p>東京の公式観光ガイドへようこそ。</p>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Fixing a Misplaced Head Element</strong></summary>

**Problem:** In the following snippet, identify why the stylesheet fails to load properly and correct it:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <p>Loading application...</p>
  <link rel="stylesheet" href="styles.css">
  <title>App</title>
</head>
<body></body>
</html>
```

**Solution:**
**Problem**: The `<p>Loading application...</p>` is visual flow content. The parser encounters `<p>`, immediately closes `<head>`, and opens `<body>`. The `<link>` and `<title>` are now misplaced inside `<body>`.
**Correct Approach**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>App</title>
  <link rel="stylesheet" href="styles.css">
</head>
<body>
  <p>Loading application...</p>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Handling In-line Mixed Language Content</strong></summary>

**Problem:** Write an English paragraph quoting a famous French phrase ("C'est la vie") and a Spanish phrase ("Hasta la vista"), ensuring full accessibility compliance.

**Solution:**
```html
<p>
  In Paris, when things go unexpectedly, locals simply remark 
  <span lang="fr">c'est la vie</span>. Similarly, in Madrid, 
  a common farewell is <span lang="es">hasta la vista</span>.
</p>
```
*(By tagging the spans with `lang="fr"` and `lang="es"`, screen readers switch pronunciation dictionaries for those exact words).*
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verifying Standards Mode Activation in JavaScript</strong></summary>

**Problem:** Write a JavaScript snippet to programmatically verify whether the current document was loaded in Standards Mode or Quirks Mode.

**Solution:**
```javascript
// document.compatMode returns 'CSS1Compat' for Standards Mode, 'BackCompat' for Quirks Mode
if (document.compatMode === 'CSS1Compat') {
  console.log('Running in Standards Mode (No-Quirks Mode)');
} else {
  console.warn('Running in Quirks Mode! Check <!DOCTYPE html>');
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Right-to-Left (RTL) Layout Skeleton with Form</strong></summary>

**Problem:** Author a complete, valid RTL login page in Arabic (`lang="ar"`, `dir="rtl"`) containing an accessible form with email and password fields.

**Solution:**
```html
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>تسجيل الدخول - النظام السحابي</title>
</head>
<body>
  <main>
    <h1>تسجيل الدخول</h1>
    <form action="/login" method="post">
      <p>
        <label for="user-email">البريد الإلكتروني:</label>
        <input type="email" id="user-email" name="email" required autocomplete="email">
      </p>
      <p>
        <label for="user-pwd">كلمة المرور:</label>
        <input type="password" id="user-pwd" name="password" required autocomplete="current-password">
      </p>
      <p>
        <button type="submit">دخول</button>
      </p>
    </form>
  </main>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Automated Document Wrapper Validation</strong></summary>

**Problem:** Write a script using our scratch validation runner that verifies whether an HTML string has valid DOCTYPE, language, and viewport declarations.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const snippet = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Validator Test</title>
</head>
<body>
  <h1>Valid Skeleton</h1>
</body>
</html>`;

const report = validateHtml(snippet);
console.log('Passed:', report.status === 'VALID' && report.issueCount === 0);
```
</details>

---

## Summary & Further Reading

- The foundational document structure consists of `<!DOCTYPE html>`, `<html>`, `<head>`, and `<body>`.
- `<!DOCTYPE html>` is mandatory on line 1 to activate Standards Mode.
- Always declare the document's primary natural language via `<html lang="...">` using IETF BCP 47 tags.
- Use `dir="rtl"` on Arabic/Hebrew content to engage the Unicode Bidirectional Algorithm.
- Never place visible content in `<head>`, as it forces the parser to close `<head>` immediately.
- **Next Topic**: [02-syntax-rules-and-error-recovery.md](02-syntax-rules-and-error-recovery.md) dives into normal vs void elements, tag casing rules, attribute quoting, omitted tags, and parser error recovery modes.

---
[Previous](README.md) | [Section Index](README.md) | [Next](02-syntax-rules-and-error-recovery.md)
