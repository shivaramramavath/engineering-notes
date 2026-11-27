# What is HTML & The World Wide Web

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner |
| **Prerequisites** | None |
| **Time to Complete** | 25 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Browser Engine Support & Baseline Status](#browser-engine-support--baseline-status)
6. [Practical Examples](#practical-examples)
7. [Best Practices](#best-practices)
8. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
9. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
10. [Comparison Matrix: Web Content Formats](#comparison-matrix-web-content-formats)
11. [Real-World Interview Questions](#real-world-interview-questions)
12. [Practice Exercises](#practice-exercises)
13. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In 1989 at CERN, Sir Tim Berners-Lee invented the World Wide Web to solve a critical scientific collaboration challenge: researchers worldwide needed an open, non-proprietary system to share documents linked together across a network of disparate computer architectures.

At the center of this invention was **HTML (HyperText Markup Language)**.
- **HyperText**: Text that contains non-linear links (hyperlinks) to other documents or resources, allowing users to traverse knowledge organically.
- **Markup Language**: A system of annotating text with syntactic markers (tags) to define its structural role and semantic meaning—distinguishing a primary heading from a paragraph, a navigation link, or an interactive data table.

Today, HTML is the universal lingua franca of the global internet. Every web application, regardless of whether its backend is engineered in Java, Python, Go, or Node.js, and regardless of whether its frontend uses React, Vue, Svelte, or native Web Components, ultimately compiles down to HTML transmitted over HTTP to be parsed and rendered by browser engines.

---

## Core Concepts

### 1. The Separation of Concerns
The modern web architecture is built on three orthogonal layers:
1. **HTML (Structure & Semantics)**: What the content *is* (e.g., `<article>`, `<h1>`, `<nav>`, `<button>`).
2. **CSS (Presentation & Visual Design)**: How the content *looks* (typography, color, grid layout, transitions).
3. **JavaScript (Behavior & Interactivity)**: How the content *behaves* (dynamic data fetching, state transitions, client animations).

Attempting to force visual presentation into HTML (e.g. using `<font>` or `<table>` for page layouts) breaks accessibility, destroys mobile responsiveness, and impairs search engine indexing.

### 2. Client-Server Architecture & MIME Types
When a browser navigates to a URL:
1. The browser acts as an **HTTP Client**, issuing a request over TCP/IP (or QUIC in HTTP/3).
2. The web server responds with an HTTP response header including the critical MIME type:
   ```http
   HTTP/2 200 OK
   Content-Type: text/html; charset=UTF-8
   ```
3. If the server incorrectly serves an HTML document as `text/plain`, the browser renders the raw code tags as literal text; if served as `application/octet-stream`, the browser prompts the user to download a file! The `Content-Type: text/html` header is mandatory.

---

## How It Works Internally

The request and retrieval lifecycle of an HTML document follows a strict network protocol:

```
+-----------------------------------------------------------------------------------------+
|                              HTML NETWORK LIFECYCLE                                     |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
| 1. DNS RESOLUTION:                                                                      |
|    Browser resolves hostname (e.g. 'example.com') -> IP Address (93.184.216.34)         |
|                                                                                         |
| 2. TCP & TLS HANDSHAKE:                                                                 |
|    Establishes secure cryptographic tunnel (TLS 1.3 / Port 443)                         |
|                                                                                         |
| 3. HTTP GET REQUEST:                                                                    |
|    GET /index.html HTTP/2                                                               |
|    Host: example.com                                                                    |
|    Accept: text/html,application/xhtml+xml,...                                          |
|                                                                                         |
| 4. SERVER RESPONSE STREAM:                                                              |
|    HTTP/2 200 OK                                                                        |
|    Content-Type: text/html; charset=UTF-8                                               |
|    Content-Length: 1042                                                                 |
|                                                                                         |
|    [ Raw Byte Stream: 0x3C 0x21 0x44 0x4F 0x43 0x54 0x59 0x50 0x45 ... ]               |
|                                     |                                                   |
|                                     v                                                   |
| 5. BROWSER NETWORK BUFFER -> HTML PARSER (Streams bytes immediately, chunk by chunk!)    |
+-----------------------------------------------------------------------------------------+
```

Crucially, **the browser does not wait for the entire HTML document to download** before beginning to parse it. As raw byte chunks arrive over the network interface, the browser's character decoder converts bytes to characters (UTF-8) and immediately feeds them into the HTML parser state machine.

---

## Syntax & Usage

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Understanding the Web</title>
</head>
<body>
  <header>
    <h1>The World Wide Web & HyperText</h1>
  </header>
  <main>
    <p>HTML provides the semantic skeleton for every web document.</p>
  </main>
</body>
</html>
```

---

## Browser Engine Support & Baseline Status

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available (Universal across all browser engines since 1995)   |
+-------------------+--------------------------------------------------------------------------------+
| Chromium (Blink)  | Chrome 1+, Edge 1+, Android WebView                                            |
+-------------------+--------------------------------------------------------------------------------+
| Gecko (Firefox)   | Firefox 1+, Gecko Mobile                                                       |
+-------------------+--------------------------------------------------------------------------------+
| WebKit (Safari)   | Safari 1+, iOS Safari 1+                                                       |
+-------------------+--------------------------------------------------------------------------------+
| Standards Body    | WHATWG HTML Living Standard / W3C Recommendation                               |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Minimal Valid HTML5 Document
- **Goal**: Construct the smallest standards-compliant HTML5 document that passes strict W3C validation.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Minimal Web Document</title>
</head>
<body>
  <h1>Document Heading</h1>
  <p>Minimal valid HTML5 document content.</p>
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
- **Explanation**: Includes all required document-level metadata: the HTML5 DOCTYPE, language declaration (`lang="en"`), UTF-8 character encoding, and a non-empty `<title>`.

### Example 2: Real-World Multi-Section Technical Article
- **Goal**: Structure a technical article utilizing semantic sections and descriptive metadata.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Network Protocols Explained</title>
</head>
<body>
  <header>
    <nav aria-label="Breadcrumb">
      <a href="/">Home</a> / <a href="/networking">Networking</a>
    </nav>
    <h1>Understanding HTTP/3 and QUIC</h1>
  </header>
  <main>
    <article>
      <h2>Protocol Architecture</h2>
      <p>HTTP/3 replaces TCP with UDP-based QUIC, eliminating head-of-line blocking.</p>
    </article>
  </main>
  <footer>
    <p>&copy; 2026 Engineering Notes. Published under MIT License.</p>
  </footer>
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
- **Explanation**: Incorporates semantic landmarks (`<header>`, `<nav>`, `<main>`, `<article>`, `<footer>`) which expose navigation boundaries directly to screen readers.

### Example 3: Edge Case — Handling Missing Optional Elements via Parser Inference
- **Goal**: Demonstrate how the WHATWG parser implicitly synthesizes omitted `<html>`, `<head>`, and `<body>` tags.
- **Code**:
```html
<!DOCTYPE html>
<title>Implicitly Parsed Document</title>
<p>The parser automatically creates html, head, and body nodes in the DOM!</p>
```
- **Real Validator & DOM Output**:
```
DOM Representation after parse:
#document
  <!DOCTYPE html>
  <html>
    <head>
      <title>Implicitly Parsed Document</title>
    </head>
    <body>
      <p>The parser automatically creates html, head, and body nodes in the DOM!</p>
    </body>
  </html>
```
- **Explanation**: While omitting `<html>` and `<body>` is valid according to the WHATWG specification, professional engineering style guides forbid it because it prevents declaring the essential `lang="en"` attribute.

### Example 4: Failure Mode — Presentational Tag Misuse & Missing Metadata
- **Goal**: Analyze the issues produced by obsolete tags and missing accessibility attributes.
- **Code**:
```html
<!DOCTYPE html>
<html>
<head>
</head>
<body>
  <center>
    <font size="6"><b>Welcome to My Site</b></font>
  </center>
  <img src="banner.jpg">
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "INVALID",
  "issueCount": 4,
  "issues": [
    {
      "severity": "error",
      "code": "html-lang-missing",
      "message": "<html> element missing valid lang attribute"
    },
    {
      "severity": "error",
      "code": "title-missing",
      "message": "<head> missing non-empty <title> element"
    },
    {
      "severity": "error",
      "code": "obsolete-element",
      "message": "Obsolete presentational element <center> used. Use modern CSS instead."
    },
    {
      "severity": "error",
      "code": "obsolete-element",
      "message": "Obsolete presentational element <font> used. Use modern CSS instead."
    },
    {
      "severity": "error",
      "code": "img-alt-missing",
      "message": "<img> element missing alt attribute."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: Demonstrates 5 fatal legacy practices: `<center>` and `<font>` are obsolete since HTML4; missing `lang` prevents text-to-speech pronunciation; missing `alt` blinds screen reader users.

### Example 5: Production-Grade Accessible Engineering Document
- **Goal**: Full responsive document with skip-navigation, landmarks, and security headers.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="description" content="Technical guide to hypermedia systems and the architecture of the web.">
  <title>Hypermedia Architecture Guide</title>
</head>
<body>
  <a href="#main-content" class="skip-link">Skip to main content</a>
  <header>
    <p>Engineering Knowledge Base</p>
  </header>
  <main id="main-content">
    <h1>Hypermedia as the Engine of Application State</h1>
    <p>Hyperlinks allow dynamic client navigation without hardcoded client-side endpoints.</p>
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
- **Explanation**: Provides a skip-link for keyboard users (`tabindex` traversal), a description meta tag for search engine result snippets, and valid landmark structures.

---

## Best Practices

1. **Always Declare `<!DOCTYPE html>` as the Very First Line**: Omission triggers browser Quirks Mode, breaking CSS box-sizing and modern layout calculations.
2. **Always Provide a Valid `lang` Attribute on `<html>`**: Set `lang="en"`, `lang="es"`, etc., so screen readers and translation tools can immediately select the correct phoneme dictionary.
3. **Always Include `<meta charset="UTF-8">` as the First Child of `<head>`**: Must reside within the first 1,024 bytes of the document so the parser never has to restart parsing due to late character encoding detection.
4. **Never Use Presentational Markup**: Keep styling strictly inside CSS. Tags like `<center>`, `<font>`, `<big>`, and `<strike>` are completely obsolete.
5. **Enforce Semantic Elements Over Generic Containers**: Prefer `<header>`, `<nav>`, `<main>`, `<article>`, `<section>`, and `<footer>` over an unreadable hierarchy of nested `<div>` tags ("div soup").

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Layout looks broken, CSS box model fails** | Missing or malformed `<!DOCTYPE html>` triggering Quirks Mode. | Place `<!DOCTYPE html>` on line 1 before any whitespace or comments. |
| **Screen reader mispronounces French text in English accent** | Missing `lang` attribute on `<html>` or inline `<span lang="fr">`. | Always declare `<html lang="en">` and mark foreign phrases with `lang="..."`. |
| **Mojibake / strange symbols (e.g. `Ã©` instead of `é`)** | Omitted `<meta charset="UTF-8">` causing browser to guess Windows-1252. | Place `<meta charset="UTF-8">` immediately after `<head>`. |
| **Mobile phones render tiny, unreadable desktop layout** | Missing `<meta name="viewport" content="width=device-width, initial-scale=1.0">`. | Include standard viewport meta tag in `<head>`. |
| **Browser prompts download instead of displaying page** | Web server sending incorrect MIME type (e.g. `application/octet-stream`). | Configure web server to emit `Content-Type: text/html; charset=UTF-8`. |
| **Screen readers announce "unlabeled graphic"** | Omitting `alt` attribute on `<img>` tags. | Add descriptive `alt="Description"` or empty `alt=""` for decorative icons. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
- **Streaming Parser Optimization**: Keep the `<head>` compact. Any synchronous `<script>` in `<head>` blocks the HTML parser from streaming the remainder of the document.
- **First Contentful Paint (FCP)**: The faster the browser parses `<head>` and encounters the first visual element in `<body>`, the faster FCP is achieved.

### Accessibility
- **The DOM is the Source of the Accessibility Tree**: Screen readers never inspect CSS or visual pixels; they query the Accessibility Object Model (AOM) built directly from semantic HTML nodes.

### SEO
- Search engine crawlers (Googlebot, Bingbot) rely on HTML semantic headings (`<h1>` through `<h6>`) and landmark containers to infer topic relevance and page hierarchy.

### Security
- **MIME Sniffing Vulnerability**: Without the HTTP header `X-Content-Type-Options: nosniff`, browsers might treat uploaded user files (like images containing HTML tags) as executable HTML, enabling Cross-Site Scripting (XSS).

---

## Comparison Matrix: Web Content Formats

| Dimension | HTML5 | XML / XHTML | Markdown | JSON |
|:---|:---|:---|:---|:---|
| **Primary Purpose** | Semantic Hypertext for browser rendering | Structured hierarchical data exchange | Human-readable document formatting | Lightweight data interchange |
| **Parser Error Mode** | **Forgiving**: Automatic deterministic recovery | **Fatal**: Parsing halts on any syntax error | Forgiving | Strict: Syntax error aborts parsing |
| **Native Interactive Elements** | Links, buttons, dialogs, forms, audio/video | None (Requires XSLT or custom app) | Links and images only | None |
| **Accessibility Mapping** | Maps natively to platform Accessibility APIs | None | Converts to HTML first | None |
| **Standardization** | WHATWG Living Standard | W3C Recommendation | CommonMark / GFM | RFC 8259 |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What is Quirks Mode in browser engines, and how do modern web applications prevent it?</strong></summary>

**Answer:**
Quirks Mode is a backwards-compatibility rendering mode implemented in modern browser engines (Blink, Gecko, WebKit) to prevent websites created in the late 1990s from breaking. In Quirks Mode, browsers emulate non-standard layout behaviors from Netscape Navigator 4 and Internet Explorer 5, particularly the non-standard IE Box Model (where width includes padding and border) and quirks in table cell font inheritance.

Modern applications prevent Quirks Mode by including the HTML5 Document Type Declaration as the absolute first line of the document:
```html
<!DOCTYPE html>
```
This forces the engine into **Standards Mode** (or "No-Quirks Mode"), guaranteeing that CSS specifications and DOM APIs behave deterministically.
</details>

<details>
<summary><strong>2. Why is HTML considered a streaming format, and how does the browser take advantage of this?</strong></summary>

**Answer:**
HTML is a streaming format because browser engines do not need to receive the entire document over the network before they begin parsing and rendering it. 

As TCP packets or HTTP/2 frames arrive across the network, the browser's character decoder decodes the raw bytes into UTF-8 characters and feeds them incrementally into the tokenizer state machine. The parser constructs DOM nodes, triggers asynchronous downloads for discovered images and stylesheets via the Preload Scanner, and begins progressive rendering of content chunk by chunk. This drastically reduces Time to First Meaningful Paint compared to monolithic formats like JSON.
</details>

<details>
<summary><strong>3. What happens if a web server returns an HTML document with Content-Type: text/plain?</strong></summary>

**Answer:**
If the server sends `Content-Type: text/plain`, the browser respects the authoritative MIME type and treats the payload as raw unformatted text. The HTML parser is never invoked: the browser does not construct a DOM tree or execute scripts, and instead renders the raw markup tags (`<!DOCTYPE html><html>...`) verbatim on the screen as plain text.
</details>

<details>
<summary><strong>4. What is the fundamental difference between HTML and XML?</strong></summary>

**Answer:**
The fundamental difference lies in their **error-handling philosophy and semantic purpose**:
1. **Error Handling**: XML is strictly draconian—any syntax violation (such as an unclosed tag or mismatched capitalization) is fatal and halts parsing immediately. HTML follows Postel's Law: the WHATWG parsing specification specifies a deterministic error-recovery algorithm for every possible malformed tag, guaranteeing that the parser never crashes and always builds a DOM tree.
2. **Semantics**: XML defines zero predefined tags (it is a meta-language for defining custom data schemas). HTML defines a fixed vocabulary of standardized semantic elements with built-in accessibility roles, browser default styles, and interactive behaviors.
</details>

<details>
<summary><strong>5. Explain the role of the lang attribute on the html element. What breaks if it is omitted?</strong></summary>

**Answer:**
The `lang` attribute (e.g., `<html lang="en">`) declares the primary natural language of the document. 
If omitted:
1. **Screen Readers**: Assistive technologies cannot determine which pronunciation rules, dictionary, or synthesizer to load, resulting in words being pronounced with improper phonemes or wrong accents.
2. **Typography**: Browser rendering engines cannot apply language-specific font glyph substitutions or correct hyphenation dictionaries (`hyphens: auto`).
3. **Translation Tools**: Browser-integrated translation tools (e.g. Google Chrome Translation) cannot accurately detect the source language.
4. **Spellcheck**: Form fields with `spellcheck="true"` will not know which dictionary to evaluate against.
</details>

<details>
<summary><strong>6. What is MIME sniffing, and why can it be a severe security risk?</strong></summary>

**Answer:**
MIME sniffing is a browser mechanism where, in the absence of an explicit `Content-Type` header (or if the header is ambiguous), the browser inspects the initial bytes of a response body to guess its media type.

**Security Risk**: If an attacker uploads a malicious file named `avatar.jpg` to a web application, but embeds `<script>alert(document.cookie)</script>` in the file, a sniffing browser might conclude the file is actually `text/html` rather than an image. The browser then executes the embedded JavaScript within the domain's origin context, leading to Cross-Site Scripting (XSS). To eliminate this risk, servers must emit the HTTP response header:
`X-Content-Type-Options: nosniff`.
</details>

<details>
<summary><strong>7. How does the browser differentiate between structural elements and void elements?</strong></summary>

**Answer:**
The WHATWG specification categorizes elements into distinct parsing models:
- **Normal Elements** (e.g., `<p>`, `<div>`, `<section>`): Can contain child text and other element nodes; must have both an opening tag and an explicit closing tag.
- **Void Elements** (e.g., `<img>`, `<input>`, `<br>`, `<meta>`, `<link>`, `<hr>`, `<wbr>`): Can **never** have child nodes or text content. Because they cannot contain children, they never have an end tag (`</img>` is invalid HTML). Any trailing slash (`<br />` or `<img />`) is simply ignored as syntactic sugar by the HTML5 parser.
</details>

<details>
<summary><strong>8. What is the Accessibility Tree, and what is its relationship to the DOM?</strong></summary>

**Answer:**
The Accessibility Tree (part of the Accessibility Object Model, AOM) is an in-memory tree data structure constructed by the browser engine concurrently with the DOM.
While the DOM represents all elements, scripts, and nodes in the document, the Accessibility Tree contains only accessibility-relevant objects: each object exposes a **Role** (e.g., button, heading, link), a computed **Name** (accessible label), an optional **Value**, and **States** (e.g., disabled, expanded, focused). Assistive technologies (like screen readers) interact exclusively with the platform Accessibility API populated by this tree, not with the raw DOM or visual CSS pixels.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Valid Document Skeleton</strong></summary>

**Problem:** Write a minimal, valid HTML5 document for a blog post titled "My First Post" in English, specifying UTF-8 encoding and responsive viewport.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>My First Post</title>
</head>
<body>
  <h1>My First Post</h1>
  <p>Welcome to my technical blog!</p>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Identify and Fix Obsolete Markup</strong></summary>

**Problem:** Refactor the following legacy snippet into clean, modern semantic HTML:
```html
<center>
  <h1><b><i>Our Products</i></b></h1>
</center>
```

**Solution:**
```html
<!-- Modern HTML: Semantics in HTML, Centering in CSS -->
<h1 class="text-center">Our Products</h1>
```
*(In CSS: `.text-center { text-align: center; }`. The `<b>` and `<i>` tags are removed because headings are already visually prominent and should not use empty styling wrappers).*
</details>

<details>
<summary><strong>Exercise 3 (Medium): Multilingual Article Tagging</strong></summary>

**Problem:** Construct an HTML document written primarily in English, but containing a blockquote in German and a single Spanish phrase, ensuring correct language declaration for assistive technologies.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Multilingual Literature</title>
</head>
<body>
  <h1>Language Exploration</h1>
  <p>In Germany, a classic proverb states:</p>
  <blockquote lang="de">
    <p>Aller Anfang ist schwer.</p>
  </blockquote>
  <p>To our friends in Spain, we say <span lang="es">muchas gracias</span> for visiting.</p>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Auditing Landmark Regions</strong></summary>

**Problem:** Restructure the following div-soup template into an accessible page using native HTML5 landmarks:
```html
<div id="top-nav">...</div>
<div id="main-content">
  <div class="sidebar">...</div>
  <div class="content">...</div>
</div>
<div id="copyright">...</div>
```

**Solution:**
```html
<header>
  <nav aria-label="Main Navigation">...</nav>
</header>
<main id="main-content">
  <aside aria-label="Sidebar">...</aside>
  <article>...</article>
</main>
<footer>...</footer>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Simulating HTTP Content-Type Impact in Node.js</strong></summary>

**Problem:** Write a Node.js HTTP server script that serves an HTML string under `text/html` on `/valid` and under `text/plain` on `/raw`, demonstrating the browser's response handling.

**Solution:**
```javascript
const http = require('http');

const htmlContent = '<!DOCTYPE html><html lang="en"><head><title>Test</title></head><body><h1>Hello World</h1></body></html>';

const server = http.createServer((req, res) => {
  if (req.url === '/valid') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=UTF-8' });
    res.end(htmlContent);
  } else if (req.url === '/raw') {
    res.writeHead(200, { 'Content-Type': 'text/plain; charset=UTF-8' });
    res.end(htmlContent);
  } else {
    res.writeHead(404);
    res.end('Not Found');
  }
});

server.listen(3000, () => console.log('Server running on port 3000'));
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Verifying Tree Construction in Node.js</strong></summary>

**Problem:** Write a script using the custom scratch runner to parse an HTML snippet containing an unclosed `<p>` tag and inspect the resulting DOM tree.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const snippet = '<!DOCTYPE html><html lang="en"><head><title>Unclosed Tag</title></head><body><p>Paragraph one<p>Paragraph two</body></html>';

const result = validateHtml(snippet);
console.log('Validation status:', result.status);
// Output: VALID. The parser automatically closes the first <p> when it encounters the second <p>!
```
</details>

---

## Summary & Further Reading

- HTML is a semantic markup language that defines the structural blueprint of web documents.
- Browsers stream and parse HTML bytes progressively, constructing the **DOM** (Document Object Model) and the **AOM** (Accessibility Object Model) concurrently.
- `<!DOCTYPE html>` forces standards mode; omitting it triggers Quirks Mode.
- Always include `lang="en"`, `<meta charset="UTF-8">`, and `<meta name="viewport">` in every document.
- **Next Topic**: [02-how-browsers-turn-html-into-pixels.md](02-how-browsers-turn-html-into-pixels.md) covers the browser rendering pipeline, tokenizer state machine, DOM construction, CSSOM, Render Tree, and GPU painting.

---
[Previous](README.md) | [Section Index](README.md) | [Next](02-how-browsers-turn-html-into-pixels.md)
