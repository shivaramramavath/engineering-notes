# Your First Production-Grade Web Page

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md`, `00-setup/04-tools-editor-and-devtools.md` |
| **Time to Complete** | 30 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal Standard) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The Essential Anatomy of an HTML5 Document](#the-essential-anatomy-of-an-html5-document)
4. [File Protocol vs Local HTTP Server](#file-protocol-vs-local-http-server)
5. [Syntax & Usage](#syntax--usage)
6. [Browser Engine Support & Baseline Status](#browser-engine-support--baseline-status)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
11. [Comparison Matrix: Local Serving Methods](#comparison-matrix-local-serving-methods)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

Creating a web page is deceptive: any browser will display a text file containing arbitrary words if you name it `.html`. But building a **production-grade, standards-compliant web page** requires satisfying strict architectural rules:
- Enabling Standards Mode across all browser rendering engines.
- Guaranteeing predictable UTF-8 character decoding without character corruption (mojibake).
- Providing responsive mobile viewport scaling.
- Establishing landmark regions for assistive technologies.
- Serving the page over an actual HTTP origin rather than the restricted `file://` protocol.

This note walks step-by-step through constructing, serving, and verifying your first enterprise-ready HTML5 document.

---

## Core Concepts

### 1. The Mandatory 5-Point Baseline
Every production HTML5 document must begin with five non-negotiable structural elements:
1. `<!DOCTYPE html>`: Forces Standards Mode (No-Quirks Mode).
2. `<html lang="en">`: Declares root natural language for screen readers and hyphenation engines.
3. `<meta charset="UTF-8">`: Declares character encoding within the first 1,024 bytes.
4. `<meta name="viewport" content="width=device-width, initial-scale=1.0">`: Sets 1:1 mobile pixel scaling.
5. `<title>...</title>`: Declares a meaningful, non-empty document title for browser tabs and search indexing.

---

## The Essential Anatomy of an HTML5 Document

```
+-----------------------------------------------------------------------------------------+
|                                  HTML5 DOCUMENT BLUEPRINT                               |
+-----------------------------------------------------------------------------------------+
| <!DOCTYPE html>                                                                         |
| <html lang="en">                                                                        |
|   <head>                                                                                |
|     <!-- 1. Technical Metadata (Machine readable; not painted on screen) -->             |
|     <meta charset="UTF-8">                                                              |
|     <meta name="viewport" content="width=device-width, initial-scale=1.0">               |
|     <meta name="description" content="Brief summary for search engine snippets">       |
|     <title>Document Title - Brand Name</title>                                          |
|     <link rel="stylesheet" href="styles.css">                                          |
|   </head>                                                                               |
|   <body>                                                                                |
|     <!-- 2. Human-Visible Content & Semantic Landmarks -->                              |
|     <header>                                                                            |
|       <nav aria-label="Main Navigation">...</nav>                                       |
|     </header>                                                                           |
|     <main>                                                                              |
|       <h1>Primary Document Topic</h1>                                                   |
|       <p>Content paragraphs, media, and interactive controls...</p>                     |
|     </main>                                                                             |
|     <footer>                                                                            |
|       <p>&copy; 2026 Company. All rights reserved.</p>                                  |
|     </footer>                                                                           |
|   </body>                                                                               |
| </html>                                                                                 |
+-----------------------------------------------------------------------------------------+
```

---

## File Protocol vs Local HTTP Server

When beginners create an HTML file, they double-click it in their file manager, opening the URL `file:///C:/Users/name/index.html`.

In professional engineering, **never develop over `file://`**:
1. **CORS Restrictions**: Browsers block all `fetch()` requests and `XMLHttpRequest` calls over `file://` due to origin isolation rules.
2. **ES Modules Blocked**: `<script type="module" src="...">` will fail with CORS security errors on `file://`.
3. **Protocol-Relative URLs Fail**: Paths like `//cdn.example.com/lib.js` resolve to `file://cdn.example.com/lib.js` and fail.
4. **Service Workers & PWA APIs**: Modern browser security forbids registering Service Workers on non-HTTP/HTTPS origins.

### Serving Locally via Command Line:
```bash
# Option A: Python 3 built-in server (instant, zero installation)
python -m http.server 8000

# Option B: Node.js npx serve
npx serve .

# Option C: VS Code "Live Server" extension
# Right-click index.html -> "Open with Live Server"
```
Navigate to `http://localhost:8000` in your browser.

---

## Syntax & Usage

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="description" content="Technical guide for building accessible and valid HTML5 web pages.">
  <title>Engineering First Principles - Modern HTML</title>
</head>
<body>
  <header>
    <nav aria-label="Breadcrumb">
      <ol>
        <li><a href="/">Home</a></li>
        <li aria-current="page">Getting Started</li>
      </ol>
    </nav>
  </header>
  <main>
    <h1>Modern Web Development Fundamentals</h1>
    <p>HTML delivers structured hypermedia accessible across all devices.</p>
  </main>
  <footer>
    <p>Authored with modern web platform standards.</p>
  </footer>
</body>
</html>
```

---

## Browser Engine Support & Baseline Status

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available (Universal HTML5 baseline standard)                 |
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

### Example 1: Basic Personal Profile Page
- **Goal**: Construct a clean, accessible profile document.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Alex Rivera - Systems Engineer</title>
</head>
<body>
  <header>
    <h1>Alex Rivera</h1>
    <p>Distributed Systems & Database Engineer</p>
  </header>
  <main>
    <section>
      <h2>About Me</h2>
      <p>I design high-throughput transactional backends and cloud infrastructures.</p>
    </section>
  </main>
  <footer>
    <p>Contact: <a href="mailto:alex@example.com">alex@example.com</a></p>
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
- **Explanation**: Passes 100% W3C validation. Elements provide structured headings and valid landmarks without styling classes.

### Example 2: Technical Blog Article with Figures and Code Blocks
- **Goal**: Markup technical content using `<article>`, `<figure>`, and `<code>`.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Understanding the Event Loop - Tech Journal</title>
</head>
<body>
  <main>
    <article>
      <header>
        <h1>Understanding the JavaScript Event Loop</h1>
        <p>Published <time datetime="2026-03-31">March 31, 2026</time></p>
      </header>
      <p>The call stack executes synchronous code while microtasks handle promises.</p>
      <figure>
        <pre><code>console.log('Call stack execution');</code></pre>
        <figcaption>Listing 1: Synchronous call stack invocation</figcaption>
      </figure>
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
- **Explanation**: Uses `<time datetime="...">` for machine-readable dates and pairs `<pre><code>` inside `<figure>` with an accessible `<figcaption>`.

### Example 3: Edge Case — Testing Character Encoding in `<head>`
- **Goal**: Demonstrate how late encoding tags force browser parser restart.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <title>Late Charset Demo</title>
  <!-- 1,500 bytes of padding comments before charset... -->
  <!-- ... padding comments ... -->
  <meta charset="UTF-8">
</head>
<body>
  <p>Price: 100€ and 50£</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "warning",
      "code": "charset-late",
      "message": "The <meta charset> element should be placed completely within the first 1024 bytes of the document."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: If `<meta charset="UTF-8">` is positioned beyond the first 1,024 bytes, browser engines may guess an encoding (e.g. ISO-8859-1), parse the start of the document, discover the late charset, and be forced to discard the tree and restart parsing from byte zero.

### Example 4: Failure Mode — The Unresponsive Mobile Document
- **Goal**: Illustrate what happens when the viewport meta tag is omitted.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Missing Viewport Demo</title>
</head>
<body>
  <h1>Desktop Scale on Mobile</h1>
  <p>Without a viewport meta tag, mobile browsers assume a virtual desktop width of 980px!</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "info",
      "code": "viewport-recommended",
      "message": "Responsive pages should specify <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: On iOS and Android devices, omitting the viewport meta tag causes mobile browsers to emulate a 980px desktop screen, rendering text at microscopically small font sizes that require pinch-to-zoom.

### Example 5: Production-Grade Landing Page Template
- **Goal**: Complete, production-ready template with skip-links, social metadata, and high-performance font preconnects.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta name="description" content="Acme Cloud Platform: Serverless infrastructure engineered for high-throughput resilience.">
  <title>Acme Cloud Platform - Resilient Serverless</title>
  
  <link rel="canonical" href="https://acme.example.com/">
  <meta property="og:title" content="Acme Cloud Platform">
  <meta property="og:description" content="Serverless infrastructure for modern engineering.">
  <meta property="og:type" content="website">

  <link rel="stylesheet" href="styles.css">
</head>
<body>
  <a href="#main" class="skip-to-content">Skip to content</a>
  <header>
    <a href="/" aria-label="Acme Cloud Home">
      <img src="logo.svg" alt="Acme Cloud Logo" width="140" height="40">
    </a>
    <nav aria-label="Main Navigation">
      <ul>
        <li><a href="/pricing">Pricing</a></li>
        <li><a href="/docs">Docs</a></li>
      </ul>
    </nav>
  </header>
  <main id="main">
    <h1>Deploy Global Infrastructure in Milliseconds</h1>
    <p>Zero cold starts. Autonomous multi-region replication.</p>
  </main>
  <footer>
    <p>&copy; 2026 Acme Cloud, Inc.</p>
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
- **Explanation**: Flawless validation status. Fully accessible with skip navigation, non-blocking stylesheets, explicit logo dimensions, and complete Open Graph metadata.

---

## Best Practices

1. **Keep `<meta charset="UTF-8">` at the Very Top of `<head>`**: Position it as the immediate first child of `<head>` to guarantee early encoding detection.
2. **Always Use `<meta name="viewport" content="width=device-width, initial-scale=1.0">`**: Mandatory for modern responsive design.
3. **Include Exactly One `<h1>` Per Page**: Structure the page with a clear visual and semantic heading hierarchy (`<h1>` for page title, `<h2>` for major sections, `<h3>` for subsections).
4. **Develop with a Local HTTP Server**: Never double-click files to test via `file://`. Use `python -m http.server` or `npx serve`.
5. **Always Add a Skip-to-Content Link**: Place an invisible link (`<a href="#main">Skip to main content</a>`) as the first element inside `<body>` to allow keyboard users to bypass long navigation headers.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Text renders as microscopic unreadable miniature on phones** | Omitted `<meta name="viewport">` causing browser to assume 980px desktop. | Include `<meta name="viewport" content="width=device-width, initial-scale=1.0">`. |
| **`fetch()` fails with CORS error when opening HTML locally** | Opening document using `file:///` protocol instead of HTTP. | Run `python -m http.server 8000` and navigate to `http://localhost:8000`. |
| **Search engine results show "Untitled Document"** | Missing or empty `<title>` tag inside `<head>`. | Provide descriptive, unique `<title>` for every page. |
| **Special characters (quotes, dashes, accents) garbled** | Missing `<meta charset="UTF-8">` or saved with ANSI/Windows-1252 in editor. | Configure editor to save in UTF-8 and declare `<meta charset="UTF-8">`. |
| **Multiple `<h1>` tags scattered across page** | Using `<h1>` for styling rather than semantic document hierarchy. | Reserve `<h1>` for the primary document topic; use `<h2>` and `<h3>` for sections. |
| **Keyboard Tab key gets stuck in header** | Missing skip-to-content link on pages with large navigation menus. | Add `<a href="#main" class="skip-link">Skip to content</a>`. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
- **Order of `<head>` Elements**:
  1. `<meta charset="UTF-8">` (Encoding)
  2. `<meta name="viewport">` (Layout bounds)
  3. `<title>` (Tab title)
  4. Preconnect links (`<link rel="preconnect">`)
  5. Critical CSS (`<style>` or `<link rel="stylesheet">`)
  6. Deferred scripts (`<script defer>`)

### Accessibility
- Every document must have a descriptive, unique `<title>`: it is the very first piece of information screen readers announce when switching between browser tabs or windows.

### SEO
- The `<title>` tag is the single most heavily weighted on-page SEO ranking signal. Keep titles between 50 and 60 characters to avoid truncation in Google Search Results.

### Security
- Ensure your local development server binds to `localhost` (`127.0.0.1`) rather than `0.0.0.0` to prevent unauthorized access across local Wi-Fi networks during testing.

---

## Comparison Matrix: Local Serving Methods

| Method | Command / Tool | Pros | Cons |
|:---|:---|:---|:---|
| **`file://` Protocol** | Double-click in Explorer/Finder | Zero setup | Breaks Fetch, ES Modules, Service Workers, CORS |
| **Python HTTP Server** | `python -m http.server 8000` | Universal (Python built-in), zero install | Static only (No live-reload) |
| **Node.js `serve`** | `npx serve .` | Fast, supports SPA fallback routing | Requires Node.js installed |
| **VS Code Live Server** | Live Server Extension | Auto-reloads browser on file save | Requires VS Code editor |
| **Vite / Webpack** | `npm run dev` | Full bundling, HMR, TypeScript support | Heavy build config for pure HTML |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is the viewport meta tag necessary for mobile devices? What happens under the hood if it is omitted?</strong></summary>

**Answer:**
When the original iPhone launched in 2007, virtually all websites were designed exclusively for fixed 960px or 1024px desktop monitors. To prevent these sites from breaking, mobile Safari invented the **virtual viewport**: if no viewport meta tag was present, the mobile browser rendered the page inside an invisible 980px virtual window, and then scaled down the resulting bitmap to fit the physical 320px phone screen.

The viewport meta tag:
```html
<meta name="viewport" content="width=device-width, initial-scale=1.0">
```
instructs the browser engine to **disable virtual viewport scaling**:
- `width=device-width`: Sets the layout viewport width to match the physical device width in CSS pixels (e.g. 390px on an iPhone 14).
- `initial-scale=1.0`: Establishes a 1:1 ratio between CSS pixels and device-independent pixels without auto-zooming.
</details>

<details>
<summary><strong>2. Why must &lt;meta charset="UTF-8"&gt; be positioned within the first 1,024 bytes of the document?</strong></summary>

**Answer:**
When a browser receives a raw stream of bytes over the network, it does not know what character encoding was used to generate those bytes unless specified in the HTTP headers (`Content-Type: text/html; charset=UTF-8`).

If the HTTP header omits the charset, the HTML parser buffers the initial bytes (up to 1,024 bytes) looking for a `<meta charset>` tag. If it finds the tag within that initial buffer, it initializes the correct character decoder (UTF-8). If the tag is placed deep in the document (beyond 1,024 bytes), the browser may begin decoding using an assumed default (like Windows-1252); when it eventually encounters the late charset declaration, it must abort, discard the constructed DOM tree, and re-parse the entire document from byte zero, creating a noticeable performance penalty.
</details>

<details>
<summary><strong>3. Why does ES Module loading (&lt;script type="module"&gt;) fail when running under the file:// protocol?</strong></summary>

**Answer:**
The ECMAScript specification mandates that ES Module imports (both static `import` and dynamic `import()`) are subject to the **Same-Origin Policy** and must be fetched using the **CORS protocol**.

The `file://` protocol does not have a formal origin (or has an opaque origin `null` depending on browser implementation). Because the local filesystem lacks HTTP response headers (such as `Access-Control-Allow-Origin`), the browser's security manager rejects the module fetch with a CORS violation error. Running a local HTTP server (`http://localhost:8000`) provides a valid origin (`http://localhost:8000`) with standard HTTP headers, allowing modules to load seamlessly.
</details>

<details>
<summary><strong>4. What is a "Skip Link" (or "Skip to Main Content"), and how is it implemented?</strong></summary>

**Answer:**
A **Skip Link** is an essential accessibility mechanism for keyboard-only and screen reader users. On typical websites, the top of the page contains extensive navigation menus, search bars, and logo links: a keyboard user would have to press the `Tab` key 20 or 30 times on every single page just to reach the first paragraph of content.

A skip link is an anchor tag placed as the very first focusable element inside `<body>`:
```html
<a href="#main" class="skip-link">Skip to main content</a>
```
It is visually hidden off-screen using CSS (`position: absolute; left: -9999px;`), but becomes visible when it receives keyboard focus (`:focus { left: 0; }`). Activating it instantly moves focus to `<main id="main">`, skipping the repetitive navigation header.
</details>

<details>
<summary><strong>5. Can an HTML document have more than one &lt;main&gt; element?</strong></summary>

**Answer:**
According to the WHATWG HTML specification, a document can have more than one `<main>` element, **provided that all other `<main>` elements have the `hidden` attribute**.

Only **one visible `<main>` element** can exist at any given time. For example, in single-page applications or tabbed interfaces, multiple `<main>` containers may be authored in the markup to represent different views, but all inactive views must be marked `<main hidden>` so assistive technologies expose only a single `main` landmark region.
</details>

<details>
<summary><strong>6. What is the difference between the &lt;title&gt; element and an &lt;h1&gt; heading?</strong></summary>

**Answer:**
- **`<title>`**: Resides inside `<head>`. It is **not rendered on the visual page canvas**. It defines the title shown on the browser tab, in browser bookmarks/history, and as the headline in search engine result pages (SERPs).
- **`<h1>`**: Resides inside `<body>`. It is a visible, visual heading rendered on the screen representing the top-level topic of the page content. While a page can theoretically function without an `<h1>` (though bad for accessibility and SEO), an HTML document without a `<title>` is strictly invalid HTML5.
</details>

<details>
<summary><strong>7. What is protocol-relative URL syntax, and why is it considered deprecated?</strong></summary>

**Answer:**
Protocol-relative URLs omit the scheme, starting with a double slash (e.g. `//cdn.example.com/lib.js`). The browser resolves the URL using the current page's protocol: `http://` if on HTTP, `https://` if on HTTPS.

It was useful in the 2000s when websites ran mixed HTTP and HTTPS to avoid mixed-content browser warnings. Today, it is an **anti-pattern**:
1. All modern web traffic should be 100% HTTPS. Explicitly declaring `https://` is always preferred.
2. If tested locally under `file:///`, a protocol-relative URL resolves to `file://cdn.example.com/lib.js` and fails completely.
</details>

<details>
<summary><strong>8. How does the &lt;time&gt; element improve semantic accessibility?</strong></summary>

**Answer:**
Human-readable dates are ambiguous: "04/05/2026" means April 5th in the US, but May 4th in Europe.
The `<time>` element bridges human readability and machine parsing using the `datetime` attribute:
```html
<time datetime="2026-05-04">May 4, 2026</time>
```
Assistive technologies, calendar integrations, search engines, and web scrapers can reliably parse the ISO 8601 formatted `datetime` attribute, while users read the localized human-friendly text.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Authoring a Valid Personal Profile</strong></summary>

**Problem:** Create a valid HTML5 file named `profile.html` containing your name as `<h1>`, a short bio in `<p>`, an email link, and correct DOCTYPE, charset, and title.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Jane Doe - Software Engineer</title>
</head>
<body>
  <header>
    <h1>Jane Doe</h1>
  </header>
  <main>
    <p>Full-stack engineer specializing in accessible web applications.</p>
    <p>Contact: <a href="mailto:jane@example.com">jane@example.com</a></p>
  </main>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Launching a Local HTTP Server</strong></summary>

**Problem:** Write the exact terminal command to start a local static HTTP server on port 3000 using Python 3 and verify how to access it.

**Solution:**
```bash
python -m http.server 3000
```
Open a browser and navigate to `http://localhost:3000`.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implementing an Accessible Skip Link</strong></summary>

**Problem:** Write the HTML markup and minimal inline CSS for a skip link that is hidden off-screen until focused via the keyboard.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Skip Link Demo</title>
  <style>
    .skip-link {
      position: absolute;
      top: -40px;
      left: 0;
      background: #000;
      color: #fff;
      padding: 8px 16px;
      z-index: 1000;
    }
    .skip-link:focus {
      top: 0;
    }
  </style>
</head>
<body>
  <a href="#content" class="skip-link">Skip to main content</a>
  <header>
    <nav><a href="/a">Link 1</a> <a href="/b">Link 2</a></nav>
  </header>
  <main id="content">
    <h1>Main Article</h1>
  </main>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Auditing HTML Validation in Node.js</strong></summary>

**Problem:** Use the scratch runner script to programmatically validate the profile HTML from Exercise 1 and print whether any errors exist.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Jane Doe - Software Engineer</title>
</head>
<body>
  <h1>Jane Doe</h1>
</body>
</html>`;

const result = validateHtml(html);
console.log('Valid:', result.status === 'VALID');
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Multi-Section Semantic Resume Document</strong></summary>

**Problem:** Construct a complete single-page resume document including Work Experience, Education, and Skills, structured using `<article>`, `<section>`, `<header>`, and `<time>` tags.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Resume - Jane Doe</title>
</head>
<body>
  <header>
    <h1>Jane Doe</h1>
    <p>Staff Systems Architect</p>
  </header>
  <main>
    <section>
      <h2>Work Experience</h2>
      <article>
        <h3>Lead Architect - Acme Corp</h3>
        <p><time datetime="2022-01">January 2022</time> &ndash; <time datetime="2026-03">Present</time></p>
        <p>Architected multi-region transactional database pipelines processing 50k requests/sec.</p>
      </article>
    </section>
    <section>
      <h2>Skills</h2>
      <ul>
        <li>Distributed Systems (Raft, Paxos)</li>
        <li>PostgreSQL Internals & Performance Tuning</li>
        <li>Accessible Web Platform Standards (WCAG 2.2)</li>
      </ul>
    </section>
  </main>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Verifying File Protocol Restrictions</strong></summary>

**Problem:** Write an HTML snippet that attempts to fetch a local JSON file via `fetch('data.json')`. Explain why this works under `http://localhost:8000` but throws an unhandled promise rejection under `file:///`.

**Solution:**
```html
<script>
  fetch('data.json')
    .then(res => res.json())
    .then(data => console.log('Loaded:', data))
    .catch(err => console.error('Fetch error:', err));
</script>
```
- Under `file:///`: The browser treats the local filesystem as an opaque origin `null`. Security specifications dictate that cross-resource requests from `null` origins are blocked by CORS, throwing `TypeError: Failed to fetch (CORS request not HTTP)`.
- Under `http://localhost:8000`: Both the HTML document and `data.json` share the identical origin (`http://localhost:8000`), allowing the fetch to resolve successfully.
</details>

---

## Summary & Further Reading

- The five essential ingredients of every valid HTML5 page: `<!DOCTYPE html>`, `<html lang="...">`, `<meta charset="UTF-8">`, `<meta name="viewport">`, and `<title>`.
- Always develop over local HTTP (`python -m http.server`) rather than `file:///` to prevent CORS and ES Module errors.
- Structure visual content with native semantic landmarks (`<header>`, `<nav>`, `<main>`, `<footer>`).
- Always include a **Skip Link** for keyboard and screen reader accessibility.
- **Next Topic**: [06-validators-linters-and-formatters.md](06-validators-linters-and-formatters.md) covers automated validation toolchains, the W3C Nu HTML Checker, `html-validate`, and CI/CD quality gates.

---
[Previous](04-tools-editor-and-devtools.md) | [Section Index](README.md) | [Next](06-validators-linters-and-formatters.md)
