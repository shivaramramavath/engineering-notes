# HTML History, The XHTML Schism & The Living Standard

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md` |
| **Time to Complete** | 25 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Governing Standard Architecture) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Chronological Evolution of HTML](#chronological-evolution-of-html)
4. [The XML / XHTML 2.0 Schism & The Yellow Screen of Death](#the-xml--xhtml-20-schism--the-yellow-screen-of-death)
5. [The Rise of WHATWG & The Living Standard](#the-rise-of-whatwg--the-living-standard)
6. [The Modern Web Platform Baseline](#the-modern-web-platform-baseline)
7. [Syntax & Usage Across Eras](#syntax--usage-across-eras)
8. [Practical Examples](#practical-examples)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
12. [Comparison Matrix: HTML Eras & Standards Bodies](#comparison-matrix-html-eras--standards-bodies)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

Unlike traditional programming languages that release major versioned bumps (e.g. Java 17 to 21, Python 2 to 3), modern HTML has **no version number**. There is no "HTML6" or "HTML7".

Since 2011, HTML has been governed as a continuously evolving, single canonical specification: the **WHATWG HTML Living Standard**.

To understand why modern HTML behaves the way it does—why the parser never crashes on broken tags, why `<br />` and `<br>` behave identically, and why presentational elements like `<center>` are outlawed—an engineer must understand the dramatic historical struggle between the **academic XML purism** of the early W3C and the **real-world pragmatic resilience** demanded by web browser implementers.

---

## Core Concepts

### 1. Draconian Error Handling vs Postel's Robustness Principle
- **Draconian Error Handling (XML/XHTML)**: If an author makes a single typo (e.g., leaving a tag unclosed or using lowercase `&` instead of `&amp;`), the parser must immediately abort parsing and display a fatal error screen ("The Yellow Screen of Death").
- **Postel's Law (Robustness Principle)**: *"Be conservative in what you send, be liberal in what you accept."* Real users should never be denied access to information because an author made a minor typographic error.
- Modern HTML is explicitly built on Postel's Law: the WHATWG parser specification details deterministic recovery algorithms for every conceivable malformed markup error.

### 2. The Living Standard Model
In a Living Standard:
- Features are not batched into monolithic multi-year releases.
- The specification is a continuous, living document updated multiple times per week.
- A feature is standardized only when browser engine implementers (Blink, Gecko, WebKit) reach consensus and build interoperable implementations tested against the Web Platform Tests (WPT) suite.

---

## Chronological Evolution of HTML

```
1989-1991: HTML 1.0 (CERN, Tim Berners-Lee, 18 tags)
   ↓
1995: HTML 2.0 (IETF RFC 1866) - First formal standard, added Form controls
   ↓
1996-1999: Browser Wars (Netscape vs Microsoft IE)
   Proprietary anarchy: <marquee>, <blink>, <font>, nested layout tables
   ↓
1999: HTML 4.01 (W3C Recommendation: Strict, Transitional, Frameset)
   ↓
2000: The W3C Abandons HTML -> Pivots to XML / XHTML 1.0 & XHTML 2.0
   Goal: Enforce strict XML parsing on the web; eliminate backward compatibility
   ↓
2004: The Rebellion -> Formation of the WHATWG
   Engineers from Opera, Mozilla, and Apple form WHATWG; author "Web Applications 1.0"
   ↓
2007: W3C Capitulates -> Adopts WHATWG draft as "HTML5"
   ↓
2014: W3C HTML5 Recommendation -> Last formal snapshot
   ↓
2019: Formal Memorandum of Understanding (MoU)
   W3C officially yields governance; WHATWG becomes sole authoritative body for HTML
   ↓
2023-Present: The Web Platform Baseline
   W3C WebDX & browser vendors standardize "Baseline Widely Available" & "Newly Available"
```

---

## The XML / XHTML 2.0 Schism & The Yellow Screen of Death

In the late 1990s, the World Wide Web Consortium (W3C) decided that HTML was inherently flawed because it tolerated sloppy syntax. The W3C declared that the future of the web was **XML**.

They created **XHTML 1.0** (HTML 4.01 recast in XML syntax) and began designing **XHTML 2.0**, which was deliberately backwards-incompatible: it discarded `<img>`, `<a>`, and standard forms in favor of pure XML namespaces.

### The Fatal Flaw of XHTML
If served with the true XML MIME type (`application/xhtml+xml`):
```html
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" ...>
<html xmlns="http://www.w3.org/1999/xhtml">
<body>
  <p>Welcome to our site & enjoy the visit!</p> <!-- FATAL ERROR: unescaped & -->
</body>
</html>
```
Because the `&` was not written as `&amp;`, XML parsers in Firefox and Safari halted immediately and painted the entire browser window yellow with an unrecoverable XML Parsing Error (**The Yellow Screen of Death**).

Commercial websites refused to risk taking down their entire business over an unescaped ampersand in a user comment. Developers served XHTML files with `Content-Type: text/html`, causing browsers to parse them with regular, forgiving HTML parsers anyway. XHTML was dead in practice.

---

## The Rise of WHATWG & The Living Standard

In 2004, at a W3C workshop in San Jose, engineers from **Opera Software, the Mozilla Foundation, and Apple** proposed that the W3C should continue evolving classic HTML to support modern rich web applications (adding native video, audio, canvas, and canvas drawing) while preserving 100% backwards compatibility with the existing web.

The W3C membership voted down the proposal.

In response, Ian Hickson (Opera/Google), David Hyatt (Apple), and Brendan Eich (Mozilla) founded the **WHATWG (Web Hypertext Application Technology Working Group)**. They worked in the open, authoring the specification that became **HTML5**.

By 2007, XHTML 2.0 had collapsed, and the W3C officially requested to adopt WHATWG's specification as the foundation for the W3C HTML5 standard. In 2019, the W3C formally signed a partnership agreement designating WHATWG as the sole authority maintaining the single authoritative HTML and DOM standards.

---

## The Modern Web Platform Baseline

To replace confusing version numbers, the major browser vendors (Google, Apple, Microsoft, Mozilla) partnered within the W3C WebDX Community Group to establish **Web Platform Baseline**:

```
+-----------------------------------------------------------------------------------------+
|                              WEB PLATFORM BASELINE LIFECYCLE                            |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ Limited Availability ]                                                               |
|    Feature implemented in only 1 or 2 engines (e.g. Speculation Rules in Chromium).    |
|    Must be used with progressive enhancement fallbacks.                                 |
|                                     |                                                   |
|                                     v All major engines ship support                    |
|  [ Baseline: Newly Available ]                                                          |
|    Supported across Chrome, Edge, Firefox, and Safari (e.g. Popover API in Jan 2025).   |
|    Safe for modern evergreen production deployments.                                    |
|                                     |                                                   |
|                                     v 30 months elapsed                                 |
|  [ Baseline: Widely Available ]                                                         |
|    Universally supported for 30+ months (e.g. <dialog>, CSS Grid, inert, fetchpriority).|
|    Safe for 100% of global web traffic without polyfills.                               |
+-----------------------------------------------------------------------------------------+
```

---

## Syntax & Usage Across Eras

```html
<!-- ERA 1: HTML 4.01 Strict (1999) - Horrendously verbose DTD -->
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <title>HTML 4.01</title>
</head>
<body>...</body>
</html>

<!-- ERA 2: XHTML 1.0 Strict (2000) - XML prologue, trailing slashes mandatory -->
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <title>XHTML 1.0</title>
</head>
<body>
  <img src="pic.jpg" alt="Pic" />
</body>
</html>

<!-- ERA 3: The HTML Living Standard (Modern) - Clean, semantic, human-friendly -->
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Living Standard</title>
</head>
<body>
  <img src="pic.jpg" alt="Pic">
</body>
</html>
```

---

## Practical Examples

### Example 1: Modern Living Standard Document with Feature Detection
- **Goal**: Demonstrate how modern HTML features are safely used with progressive enhancement.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Living Standard Feature Detection</title>
</head>
<body>
  <!-- Baseline Newly Available (2024/2025): Exclusive Accordion -->
  <details name="faq">
    <summary>What is the HTML Living Standard?</summary>
    <p>It is the continuously maintained, unversioned specification for HTML.</p>
  </details>
  <details name="faq">
    <summary>Is there an HTML6?</summary>
    <p>No. New features are added directly to the Living Standard.</p>
  </details>
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
- **Explanation**: In modern browsers supporting `details[name]`, opening one panel closes the other; in older browsers, both panels open independently without error.

### Example 2: The Self-Closing Slash Reality in Modern HTML
- **Goal**: Clarify the browser's internal handling of XHTML-style self-closing slashes (`/>`).
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Void Elements and Slashes</title>
</head>
<body>
  <!-- Void elements: slash is permitted but completely ignored by the parser -->
  <input type="text" name="username" />
  <img src="logo.png" alt="Logo" />
  <br />

  <!-- Non-void element with slash: ANTI-PATTERN! -->
  <!-- The parser treats this as an OPENING <div>! It DOES NOT close the div! -->
  <div class="card" />
  <p>This paragraph accidentally becomes a CHILD of the div!</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
DOM Result after parse:
body
  ├── input
  ├── img
  ├── br
  └── div.card
        └── p
              └── "This paragraph accidentally becomes a CHILD of the div!"
```
- **Explanation**: In HTML5, the `/>` syntax has **no effect on non-void elements**. Writing `<div />` creates an unclosed open `<div>` tag that swallows subsequent elements!

### Example 3: Edge Case — Testing DOCTYPE Case Insensitivity
- **Goal**: Verify that the HTML5 DOCTYPE is completely case-insensitive per specification.
- **Code**:
```html
<!doctype HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Case Insensitive DOCTYPE</title>
</head>
<body>
  <p>The parser matches the DOCTYPE string ASCII case-insensitively.</p>
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
- **Explanation**: `<!DOCTYPE html>`, `<!doctype html>`, and `<!DoCtYpE hTmL>` are 100% syntactically identical to the parser, though uppercase `<!DOCTYPE html>` is the industry convention.

### Example 4: Failure Mode — Accidental Quirks Mode via XML Prologue
- **Goal**: Illustrate how legacy XHTML habits break modern browsers.
- **Code**:
```html
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html lang="en">
<head>
  <title>Broken by XML Prologue</title>
</head>
<body>
  <p>The XML prologue before <!DOCTYPE> triggered Quirks Mode in IE/older engines!</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "INVALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "error",
      "code": "doctype-missing",
      "message": "Missing <!DOCTYPE html> at start of stream (XML declaration preceded doctype)."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: Placing anything (even an XML declaration or a comment) before `<!DOCTYPE html>` historically forced engines into Quirks Mode. Never put anything before line 1.

### Example 5: Production-Grade Baseline Document with Polyfill Fallbacks
- **Goal**: A modern document using Baseline features with robust progressive enhancement.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Production Baseline Template</title>
</head>
<body>
  <!-- Native Dialog (Baseline Widely Available) -->
  <button id="open-btn" onclick="document.getElementById('terms-dialog').showModal()">View Terms</button>

  <dialog id="terms-dialog">
    <h2>Terms of Service</h2>
    <p>These terms govern your use of the web platform.</p>
    <button onclick="document.getElementById('terms-dialog').close()">Accept & Close</button>
  </dialog>
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
- **Explanation**: Relies on native `<dialog>` and `.showModal()`, which achieved Baseline Widely Available status across all browsers in 2022, eliminating thousands of lines of fragile JavaScript modal libraries.

---

## Best Practices

1. **Use `<!DOCTYPE html>` Exclusively**: Never write legacy HTML4 or XHTML DTD strings.
2. **Consult Web Platform Baseline**: Before adopting new HTML features (e.g. Popover API, Customizable Select), check its Baseline status on MDN or `web.dev/baseline`.
3. **Avoid the Self-Closing Slash on Non-Void Elements**: Never write `<div />` or `<script src="..." />`. Always provide explicit closing tags `</div>` and `</script>`.
4. **Treat New Features as Progressive Enhancements**: If a feature is *Limited Availability* (e.g. Speculation Rules), ensure older browsers still function seamlessly without it.
5. **Never Expect an "HTML6"**: The web is a continuous platform. Features are added to the Living Standard as soon as two independent engines implement them.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Entire DOM tree swallowed into a child element** | Writing `<div />` thinking it closes the element like in JSX/React. | Write `<div></div>`. HTML parsers ignore slashes on non-void tags! |
| **Old IE engines enter Quirks Mode** | Putting an XML declaration (`<?xml ...>`) or comment before `<!DOCTYPE html>`. | Ensure `<!DOCTYPE html>` is the absolute first token in the byte stream. |
| **Site broken on mobile Safari** | Using bleeding-edge features without checking Baseline status or providing fallbacks. | Check MDN Baseline widget; provide polyfills or semantic fallbacks. |
| **Waiting for "HTML6" release** | Misunderstanding the Living Standard governance model. | Follow the WHATWG GitHub repository and browser engine intent-to-ship threads. |
| **Serving HTML as `application/xhtml+xml`** | Old XHTML habits causing fatal XML parser crashes on minor errors. | Serve all documents with standard `Content-Type: text/html`. |
| **Using deprecated XHTML attributes** | Using `xml:lang` or `xmlns` in standard HTML documents. | Use standard HTML5 `lang="en"`; omit unnecessary XML namespaces. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
The shift from XHTML to HTML5 enabled modern **streaming parsing**: because HTML does not require whole-document validation before building the DOM, browsers can stream and render content immediately as packets arrive.

### Accessibility
The HTML Living Standard formally specified the **HTML Accessibility API Mappings (HTML-AAM)**, codifying for the first time exactly how every native HTML tag must map to platform accessibility APIs (UIA, NSAccessibility, ATK).

### SEO
Modern search engine crawlers ignore legacy XHTML namespace headers. They evaluate documents against the modern HTML Living Standard.

### Security
The Living Standard explicitly standardized the **Same-Origin Policy**, the `sandbox` attribute for `<iframe>`, and modern security headers, replacing ad-hoc browser implementations with unified security invariants.

---

## Comparison Matrix: HTML Eras & Standards Bodies

| Dimension | HTML 4.01 (1999) | XHTML 1.0 / 2.0 (2000) | WHATWG HTML Living Standard (Current) |
|:---|:---|:---|:---|
| **Governing Body** | W3C | W3C | **WHATWG** (Endorsed by W3C) |
| **Parsing Model** | SGML-based | Strict XML (Draconian) | **Deterministic HTML Parser State Machine** |
| **Error Handling** | Undefined (Engine quirks) | Fatal Abort (Yellow Screen of Death) | **Standardized Error Recovery (Zero Crashes)** |
| **Version Model** | Frozen monolithic versions | Frozen monolithic versions | **Living Standard (Continuous Updates)** |
| **Multimedia** | Required proprietary plugins (Flash) | Required custom XML plugins | **Native `<video>`, `<audio>`, `<canvas>`, `<svg>`** |
| **Cross-Browser Test Suite** | None | Limited | **Web Platform Tests (WPT)** (Millions of daily tests) |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What was the XHTML 2.0 controversy, and why did it fail?</strong></summary>

**Answer:**
In the early 2000s, the W3C attempted to force the web toward XML purism with XHTML 2.0. XHTML 2.0 abandoned backwards compatibility with the existing web: it discarded foundational elements like `<img>` and `<a>`, removed support for forgiving error handling, and enforced **draconian error handling** (where any unescaped `&` or unclosed tag threw an unrecoverable "Yellow Screen of Death").

Commercial websites refused to adopt it because a minor typo in dynamic content could take down an entire page. Furthermore, browser vendors realized that users would simply switch to browsers that continued rendering broken pages. In response, browser engineers formed the WHATWG to evolve HTML pragmatically, leading to HTML5 and the eventual abandonment of XHTML 2.0.
</details>

<details>
<summary><strong>2. What is the difference between a versioned specification (like ECMAScript 2024) and the WHATWG Living Standard?</strong></summary>

**Answer:**
- **Versioned Specifications**: Freeze standards into periodic snapshots (e.g. ECMAScript 2024, C++20). Features are bundled together and given an official year or version number.
- **The Living Standard**: HTML has no frozen versions. It is an unversioned, continuous document updated multiple times a week. Features graduate from experimental proposals (via Open UI or WICG) into the Living Standard as soon as browser vendors achieve interoperable implementations backed by the Web Platform Tests (WPT) suite. Browser support is tracked via **Web Platform Baseline** rather than version numbers.
</details>

<details>
<summary><strong>3. What happens if you write &lt;div class="box" /&gt; in modern HTML5? Why does it differ from JSX?</strong></summary>

**Answer:**
In JSX (which is parsed by a JavaScript transpiler following XML rules), `<div class="box" />` is valid self-closing syntax equivalent to `React.createElement('div', { className: 'box' })`.

However, in native **HTML5**, the self-closing slash (`/>`) **has no effect on non-void elements**. The HTML parser treats `<div class="box" />` simply as an **opening `<div>` tag**. Because there is no closing `</div>`, the element remains open in the DOM, accidentally nesting all subsequent siblings inside it and breaking the visual layout.
</details>

<details>
<summary><strong>4. What is the Web Platform Baseline, and what is the difference between Newly Available and Widely Available?</strong></summary>

**Answer:**
Web Platform Baseline is an initiative by the W3C WebDX group and major browser vendors to clarify feature support without referencing arbitrary browser version numbers:
- **Baseline: Newly Available**: A feature reaches this status the instant it is supported across all four core browser engines (Chrome, Edge, Firefox, Safari).
- **Baseline: Widely Available**: Reached when a feature has been *Newly Available* for **30 months**. At this point, the feature is supported by roughly 98%+ of global users, meaning developers can safely use it without polyfills or build-time transpilation.
</details>

<details>
<summary><strong>5. Why is the HTML5 DOCTYPE simply &lt;!DOCTYPE html&gt; with no DTD URL?</strong></summary>

**Answer:**
In HTML 4.01, HTML was technically an application of SGML (Standard Generalized Markup Language), which required referencing a Document Type Definition (DTD) file over the network to declare element rules.

During HTML5 development, WHATWG discovered that browser engines never actually fetched or read the DTD file over the network; they merely checked whether the DOCTYPE string was present to avoid triggering Quirks Mode. Therefore, HTML5 intentionally simplified the DOCTYPE to the absolute shortest string that still triggered Standards Mode across all existing and legacy browser engines: `<!DOCTYPE html>`.
</details>

<details>
<summary><strong>6. What is the role of the Web Platform Tests (WPT) in the HTML Living Standard?</strong></summary>

**Answer:**
Web Platform Tests (WPT) is an open-source, cross-browser test suite containing hundreds of thousands of automated tests covering every aspect of web standards. 
Under the modern Living Standard process, a feature cannot be added to the HTML specification without corresponding tests in WPT. All major browser engines (Chromium, Gecko, WebKit) continuously run WPT in their CI/CD pipelines, ensuring that any engine regression or divergence from the specification is instantly flagged and resolved.
</details>

<details>
<summary><strong>7. What was the 2019 agreement between the W3C and the WHATWG?</strong></summary>

**Answer:**
For several years, the W3C and WHATWG published competing, slightly divergent versions of the HTML specification (W3C HTML5.x vs WHATWG HTML Living Standard). 

In May 2019, the two organizations signed a formal Memorandum of Understanding (MoU):
1. W3C agreed to stop publishing competing HTML and DOM specifications.
2. WHATWG became the sole authority responsible for authoring and maintaining the HTML and DOM Living Standards.
3. W3C periodically endorses WHATWG snapshots as official W3C Recommendations.
</details>

<details>
<summary><strong>8. Why does HTML follow Postel's Law while XML rejects it?</strong></summary>

**Answer:**
XML was designed for automated machine-to-machine data communication where data corruption or ambiguity cannot be tolerated (e.g. banking or medical schemas). 

HTML was designed for human communication across an open, decentralized network. Because web content is authored by millions of people with varying levels of technical skill, halting page rendering over a minor typo (draconian error handling) ruins the user experience. By following Postel's Law ("Be liberal in what you accept"), HTML ensures that information remains accessible even if the markup is imperfect.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactoring Legacy DTD to Modern HTML5</strong></summary>

**Problem:** Convert the following legacy HTML 4.01 document declaration into the modern HTML Living Standard format:
```html
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
```

**Solution:**
```html
<!DOCTYPE html>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Checking Baseline Compatibility</strong></summary>

**Problem:** Given the native `<dialog>` element (Baseline Widely Available since 2022) and the `<selectedcontent>` element (Limited Availability), identify which one can be deployed today without progressive enhancement fallbacks.

**Solution:**
`<dialog>` can be deployed universally without fallbacks because it achieved **Baseline: Widely Available** status in 2022. `<selectedcontent>` has **Limited Availability** (under development in Open UI / Chromium) and must have standard `<select>` fallback styling.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Fixing Non-Void Self-Closing Traps</strong></summary>

**Problem:** The following markup creates unexpected nesting bugs in the browser. Identify the bug and write the corrected markup:
```html
<section class="banner" />
<main>
  <h1>Main Title</h1>
</main>
```

**Solution:**
**Bug**: `<section>` is a non-void element. The HTML parser ignores the trailing slash (`/>`), treating it as an opening `<section>`. As a result, the `<main>` element is incorrectly parsed as a child inside the section.
**Correct Approach**:
```html
<section class="banner"></section>
<main>
  <h1>Main Title</h1>
</main>
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verifying XML vs HTML Error Recovery</strong></summary>

**Problem:** Explain what happens when a browser loads a document containing an unescaped ampersand (`AT&T`) under `Content-Type: text/html` vs `Content-Type: application/xhtml+xml`.

**Solution:**
- Under `text/html`: The HTML5 tokenizer encounters the `&` not followed by a valid entity name, treats it as an "ambiguous ampersand", and recovers gracefully by emitting a literal `&` character token. The page renders normally.
- Under `application/xhtml+xml`: The XML parser encounters a well-formedness error, halts parsing immediately, and displays a fatal XML Parsing Error page.
</details>

<details>
<summary><strong>Exercise 5 (Hard): Creating a Baseline Feature Detection Script</strong></summary>

**Problem:** Write a JavaScript snippet that detects whether the current browser supports the Popover API, the Dialog element, and Declarative Shadow DOM.

**Solution:**
```javascript
const featureSupport = {
  dialog: typeof HTMLDialogElement === 'function',
  popover: HTMLElement.prototype.hasOwnProperty('popover'),
  declarativeShadowDom: HTMLTemplateElement.prototype.hasOwnProperty('shadowRootMode')
};

console.log('Web Platform Baseline Feature Support:', featureSupport);
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Simulating Parser Behavior with Omitted Tags</strong></summary>

**Problem:** Using Node.js and our scratch runner, verify that omitting the closing `</li>` and `</p>` tags produces a completely valid and identical DOM tree according to WHATWG rules.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const snippet = `<!DOCTYPE html>
<html lang="en">
<head><title>Omitted Tags</title></head>
<body>
  <ul>
    <li>Item 1
    <li>Item 2
  </ul>
</body>
</html>`;

const result = validateHtml(snippet);
console.log('Validation status:', result.status);
// Output: VALID. Per WHATWG Section 13.1.2.4, an <li> element's end tag may be omitted if immediately followed by another <li> or the parent closing tag.
```
</details>

---

## Summary & Further Reading

- The HTML Living Standard is an unversioned, continuously updated specification governed by the **WHATWG**.
- XHTML 2.0 failed because draconian error handling violated the web's foundational requirement for backward compatibility and resilience.
- The **Web Platform Baseline** provides a clear, vendor-neutral definition of when features become safe for production use (*Newly Available* $\rightarrow$ *Widely Available* after 30 months).
- Trailing slashes (`/>`) have no effect on non-void elements in HTML5.
- **Next Topic**: [04-tools-editor-and-devtools.md](04-tools-editor-and-devtools.md) explores professional engineering toolchains, VS Code setup, and browser DevTools inspection of the DOM and Accessibility Tree.

---
[Previous](02-how-browsers-turn-html-into-pixels.md) | [Section Index](README.md) | [Next](04-tools-editor-and-devtools.md)
