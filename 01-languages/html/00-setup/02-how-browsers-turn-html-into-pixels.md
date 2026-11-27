# How Browsers Turn HTML into Pixels: The Rendering Pipeline

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md` |
| **Time to Complete** | 40 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal Engine Pipeline) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The 6 Stages of the Rendering Pipeline](#the-6-stages-of-the-rendering-pipeline)
4. [The Tokenizer & Tree Construction Internals](#the-tokenizer--tree-construction-internals)
5. [The Preload Scanner (Background Parser)](#the-preload-scanner-background-parser)
6. [Browser Engine Support & Baseline Status](#browser-engine-support--baseline-status)
7. [Practical Examples](#practical-examples)
8. [Best Practices](#best-practices)
9. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
10. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
11. [Comparison Matrix: Script Loading Attributes](#comparison-matrix-script-loading-attributes)
12. [Real-World Interview Questions](#real-world-interview-questions)
13. [Practice Exercises](#practice-exercises)
14. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

To the software engineer, HTML is a text file containing structured angle brackets. To the computer's graphics hardware (GPU), the screen is a matrix of millions of physical liquid crystal or OLED subpixels emitting light at 60Hz or 120Hz.

The bridge between these two extremes is **the browser rendering engine** (Chromium's Blink, Firefox's Gecko, Safari's WebKit).

Understanding how the engine converts raw text bytes into pixels—known as the **Critical Rendering Path**—is the foundational prerequisite for high-performance frontend engineering. Without this mental model, developers inadvertently introduce parser-blocking scripts, unoptimized font downloads, catastrophic layout thrashing, and high Cumulative Layout Shifts (CLS) that destroy Core Web Vitals and user experience.

---

## Core Concepts

### The Two Parallel Trees
To render a webpage, the browser engine must construct two independent, in-memory tree data structures:
1. **The DOM (Document Object Model)**: The parsed representation of HTML tags, text nodes, and attributes.
2. **The CSSOM (CSS Object Model)**: The parsed representation of all CSS rules, cascade hierarchies, and selector mappings.

Neither the DOM nor the CSSOM alone is sufficient to paint the screen:
- The DOM contains structure and content, but knows nothing about visual styling, widths, or colors.
- The CSSOM contains visual rules, but knows nothing about what text or buttons actually exist.
- Only when the browser merges the DOM and CSSOM into the **Render Tree** can visual layout and pixel rasterization begin.

---

## The 6 Stages of the Rendering Pipeline

```
+-----------------------------------------------------------------------------------------+
|                              THE CRITICAL RENDERING PATH                                |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ 1. Network Stream ]                                                                  |
|    Raw Bytes (0x3C 0x68 ...) -> Character Decoder -> Characters ("<html...")            |
|                                                                                         |
|  [ 2. Tokenization & Tree Construction ]                                                |
|    Tokenizer -> Tokens (StartTag: html, StartTag: head...) -> DOM Tree                  |
|    (Concurrently: Preload Scanner fetches CSS, Fonts & Images in background)            |
|                                                                                         |
|  [ 3. CSSOM Construction ]                                                              |
|    External CSS files & <style> blocks parsed into CSS Object Model                     |
|                                                                                         |
|  [ 4. Render Tree Synthesis ]                                                           |
|    DOM + CSSOM = Render Tree (Filters out <head>, <script>, and 'display: none')        |
|                                                                                         |
|  [ 5. Layout (Reflow) ]                                                                 |
|    Computes exact geometric bounding boxes (x, y, width, height) in viewport pixels    |
|                                                                                         |
|  [ 6. Paint & Composite ]                                                               |
|    Rasterizes boxes into bitmap layers; GPU blends layers and flushes to display frame  |
+-----------------------------------------------------------------------------------------+
```

---

## The Tokenizer & Tree Construction Internals

The WHATWG specification defines the HTML parsing algorithm as two tightly coupled state machines:

### Stage A: Tokenization
The **Tokenizer** consumes a stream of unicode characters and emits **Tokens**:
- `DOCTYPE token`
- `Start tag token` (with attribute key-value pairs)
- `End tag token`
- `Comment token`
- `Character token` (text content)
- `End-of-file token`

The tokenizer operates across over 80 distinct states (e.g. *Data state*, *Tag open state*, *Tag name state*, *Before attribute name state*, *Attribute value (double-quoted) state*).

### Stage B: Tree Construction (The Dispatcher)
As tokens are emitted, they are immediately fed into the **Tree Construction Dispatcher**, which maintains a stack of open elements.
- The tree builder progresses through defined **Insertion Modes**:
  1. `initial`
  2. `before html`
  3. `before head`
  4. `in head`
  5. `after head`
  6. `in body`
  7. `after body`
  8. `after after body`
- **Error Recovery Invariant**: If an invalid token arrives (e.g. text placed directly inside `<table>` before `<tr>`), the parser does not abort. It enters **foster parenting mode**, shifting the text node outside and immediately before the table in the DOM!

---

## The Preload Scanner (Background Parser)

While the main thread parser is blocked executing a synchronous `<script>`, how does the browser avoid sitting completely idle?

Modern engines utilize a secondary, highly optimized thread called the **Preload Scanner**:
- It runs concurrently alongside the main parser.
- It scans upcoming raw HTML tokens strictly looking for external resource URLs: `<link rel="stylesheet">`, `<script src="...">`, `<img src="...">`, and `<link rel="preload">`.
- It immediately dispatches asynchronous speculative HTTP requests over the network.
- By the time the main thread finishes executing JavaScript, the stylesheets and images are already downloaded in memory, eliminating idle network stalls.

---

## Browser Engine Support & Baseline Status

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available (Universal across all browser engines)              |
+-------------------+--------------------------------------------------------------------------------+
| Chromium (Blink)  | Chrome 1+, Edge 1+, Opera 15+ (V8 JavaScript + Blink Rendering)               |
+-------------------+--------------------------------------------------------------------------------+
| Gecko (Firefox)   | Firefox 1+ (SpiderMonkey JavaScript + Gecko / Servo-derived Layout)            |
+-------------------+--------------------------------------------------------------------------------+
| WebKit (Safari)   | Safari 1+, iOS Safari 1+ (JavaScriptCore + WebKit Rendering)                  |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Demonstrating Parser-Blocking vs Non-Blocking Scripts
- **Goal**: Show how script placement impacts parsing and DOM availability.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Script Execution Order</title>
  <!-- Parser-blocking: stops HTML parsing until downloaded & run! -->
  <script src="blocking.js"></script>

  <!-- Defer: downloads in background, executes in order AFTER DOM is built -->
  <script src="deferred.js" defer></script>
</head>
<body>
  <h1>DOM Construction in Progress</h1>
  <p>This paragraph is delayed by blocking.js, but NOT by deferred.js.</p>
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
- **Explanation**: A plain `<script src="...">` halts the HTML tokenizer immediately because the script might call `document.write()`. Adding `defer` allows the tokenizer to stream the entire HTML document uninterrupted.

### Example 2: Inspecting What Enters the Render Tree
- **Goal**: Illustrate how elements in the DOM may or may not enter the Render Tree.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Render Tree Filtering</title>
  <style>
    .hidden-box { display: none; }
    .invisible-box { visibility: hidden; }
  </style>
</head>
<body>
  <h1>Visible Heading</h1>
  <div class="hidden-box">
    <p>I am in the DOM, but completely ABSENT from the Render Tree!</p>
  </div>
  <div class="invisible-box">
    <p>I AM in the Render Tree! I take up layout space, but paint transparently.</p>
  </div>
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
- **Explanation**:
  - `display: none`: The node is present in the DOM tree, but generates **zero boxes in the Render Tree** (no layout geometry, no paint).
  - `visibility: hidden`: The node **is present in the Render Tree** and generates layout geometry, reserving blank space on screen.

### Example 3: Edge Case — Foster Parenting in Action
- **Goal**: Demonstrate how browser engines handle misnested table text.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Foster Parenting</title>
</head>
<body>
  <table border="1">
    Misplaced Text directly in table!
    <tr>
      <td>Valid Cell</td>
    </tr>
  </table>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
Computed DOM Tree after Foster Parenting:
body
  ├── "Misplaced Text directly in table!" (Moved BEFORE table!)
  └── table
        └── tbody
              └── tr
                    └── td
                          └── "Valid Cell"
```
- **Explanation**: The WHATWG tree construction algorithm disallows text nodes as direct children of `<table>`. Rather than dropping the text, the parser executes "foster parenting", placing the text immediately before the table in the parent container.

### Example 4: Failure Mode — Layout Thrashing (Forced Synchronous Layout)
- **Goal**: Demonstrate how naive JavaScript interleaved with DOM manipulation causes multiple recalculate-layout passes.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Layout Thrashing</title>
</head>
<body>
  <div id="container">
    <div class="box">Box 1</div>
    <div class="box">Box 2</div>
  </div>
  <script>
    // ANTI-PATTERN: Interleaved read and write causes forced layout!
    const boxes = document.querySelectorAll('.box');
    for (let i = 0; i < boxes.length; i++) {
      // 1. Write to DOM (invalidates layout)
      boxes[i].style.width = '200px';
      // 2. Read from DOM (FORCES browser to immediately pause and calculate layout!)
      const height = boxes[i].offsetHeight;
    }
  </script>
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
- **Explanation**: Reading geometry properties (`offsetHeight`, `clientWidth`, `getBoundingClientRect()`) immediately after modifying styles forces the browser engine to halt execution and execute a full synchronous layout calculation on every loop iteration ("layout thrashing").

### Example 5: Optimized Critical Rendering Path Markup
- **Goal**: Modern, zero-render-blocking document structure.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>High Performance Page</title>

  <!-- Preconnect to external critical origins -->
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>

  <!-- Inlined Critical CSS for instant FCP -->
  <style>
    body { font-family: system-ui, sans-serif; margin: 0; padding: 1rem; }
    h1 { color: #0d47a1; }
  </style>

  <!-- Asynchronous non-critical CSS -->
  <link rel="preload" href="styles.css" as="style" onload="this.rel='stylesheet'">

  <!-- Modern Module Script (Deferred by default!) -->
  <script type="module" src="app.js"></script>
</head>
<body>
  <h1>Critical Content Rendered Instantly</h1>
  <p>Zero blocking scripts in head; inlined critical styles guarantee immediate paint.</p>
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
- **Explanation**: Eliminates render-blocking stylesheets via inlined critical CSS, preconnects to third-party domains, and uses ES modules which are deferred by specification.

---

## Best Practices

1. **Never Place Synchronous `<script src="...">` in `<head>`**: Always append `defer` or use `<script type="module">` so the parser streams the entire HTML document without blocking.
2. **Inline Critical CSS, Defer Non-Critical CSS**: The CSSOM blocks page rendering. Inlining above-the-fold styles directly inside `<style>` guarantees the fastest possible First Contentful Paint (FCP).
3. **Declare Image Dimensions (`width` and `height`) in HTML**: Always specify intrinsic dimensions (`<img width="800" height="600">`). This allows the layout engine to calculate aspect ratios and reserve layout geometry *before* image bytes download, eliminating Cumulative Layout Shift (CLS).
4. **Avoid Forced Synchronous Layouts in JavaScript**: Batch all DOM style reads first, then batch all DOM style writes (or use `requestAnimationFrame()`).
5. **Leverage the Preload Scanner**: Keep asset URLs discoverable directly in static HTML attributes (`src`, `href`, `srcset`). Avoid injecting critical images and fonts solely through client-side JavaScript.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **White screen delay on slow 3G/4G** | Synchronous `<script src="...">` in `<head>` blocking the HTML parser. | Add `defer` attribute or move `<script>` to the end of `<body>`. |
| **Page elements violently jump as images load** | Omitting `width` and `height` attributes on `<img>` tags (causing high CLS). | Explicitly declare intrinsic `width` and `height` on all image elements. |
| **Flash of Unstyled Text (FOUT / FOIT)** | Custom web fonts discovered late in the parsing cycle via deeply nested CSS. | Add `<link rel="preload" as="font" crossorigin href="...">` in `<head>`. |
| **Unresponsive UI, jerky scrolling** | JavaScript modifying styles and reading computed geometry in a tight loop (Layout Thrashing). | Separate DOM reads and writes into batched passes or use FastDOM/RAF. |
| **Preload scanner unable to discover assets** | Loading critical CSS or images via JavaScript strings (`new Image().src = ...`). | Expose critical assets in HTML `<link rel="preload">` or native `<img>` tags. |
| **Invisible content during font load** | Using `@font-face` with default `font-display: block`. | Use `font-display: swap` or `optional` to render system fonts immediately. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance: The Golden Rule of CRP
- **HTML is parser-streaming**: Nodes become available incrementally.
- **CSS is render-blocking**: The browser *will not paint a single pixel* until the complete CSSOM has been constructed to prevent Flash of Unstyled Content (FOUC).
- **JavaScript is parser-blocking by default**: A synchronous script pauses tokenization until downloaded and executed.

### Accessibility: The AOM Update Cycle
Every time JavaScript triggers a DOM mutation or layout shift, the browser engine must recalculate the Accessibility Tree (AOM). Severe DOM churn (e.g. re-rendering 5,000 table rows every second) causes screen reader performance stutters and high CPU usage.

### SEO: Googlebot Crawling Budgets
Search engine crawlers enforce strict rendering timeouts (~5 seconds). If a page relies on heavy, render-blocking scripts that delay DOM completion, the crawler will index an empty or incomplete page.

### Security: DOM Clobbering
In poorly sanitized code, named HTML elements (e.g. `<form id="config">` or `<img name="avatar">`) pollute the global `window` object in the DOM, potentially clobbering internal application configuration objects.

---

## Comparison Matrix: Script Loading Attributes

| Attribute | Parsing Paused During Download? | Parsing Paused During Execution? | Execution Timing | Execution Order Guaranteed? |
|:---|:---:|:---:|:---|:---:|
| **`<script>`** (Default) | **Yes** | **Yes** | Immediately upon download | Yes (Source order) |
| **`<script defer>`** | No (Background) | No (Runs after parse) | When HTML parsing completes (before `DOMContentLoaded`) | **Yes** (Source order) |
| **`<script async>`** | No (Background) | **Yes** (Pauses parser!) | As soon as downloaded (Unpredictable) | **No** (First to download runs first) |
| **`<script type="module">`** | No (Background) | No | When HTML parsing completes | **Yes** (Source order) |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between DOMContentLoaded and the window.onload event.</strong></summary>

**Answer:**
- **`DOMContentLoaded`**: Fires when the initial HTML document has been completely parsed into the DOM tree, and all deferred scripts (`<script defer>` and `<script type="module">`) have executed. External resources such as images, subframes, and asynchronous stylesheets do **not** need to be finished loading.
- **`window.onload` (load event)**: Fires later, only when the entire page—including all external stylesheets, images, iframes, fonts, and network assets—has completely finished loading.
</details>

<details>
<summary><strong>2. Why is CSS render-blocking, but not parser-blocking?</strong></summary>

**Answer:**
An external stylesheet (`<link rel="stylesheet">`) does not block the HTML parser: the tokenizer continues reading raw HTML tokens and constructing the DOM tree below the link tag. 

However, CSS **is render-blocking**: the browser engine deliberately refuses to construct the Render Tree or paint pixels to the screen until the CSSOM has finished loading and parsing. If the browser rendered content before the CSSOM was ready, users would observe a jarring Flash of Unstyled Content (FOUC), followed by massive layout recalculations when styles arrived.
</details>

<details>
<summary><strong>3. What is the Preload Scanner, and why was it invented?</strong></summary>

**Answer:**
Historically, if the HTML parser encountered a synchronous `<script src="large.js">`, it halted completely. The browser could not look ahead into the remainder of the document to discover subsequent external stylesheets or images until `large.js` had downloaded and executed, resulting in serialized, waterfall network requests.

The **Preload Scanner** is a secondary, lightweight thread that runs ahead of the main parser. Even while the main parser is blocked by JavaScript execution, the preload scanner inspects upcoming HTML tokens, extracts resource URLs (`<link>`, `<img>`, `<script>`), and initiates parallel speculative network downloads, eliminating network idle time.
</details>

<details>
<summary><strong>4. What is the difference between Reflow (Layout) and Repaint? Which is more expensive?</strong></summary>

**Answer:**
- **Reflow (Layout)**: The calculation of the physical geometry, position ($x, y$), and dimensions (width, height) of every render box in the viewport. Reflow is hierarchical: changing the size of a parent element forces layout recalculations on all of its children and subsequent siblings.
- **Repaint**: The process of filling in the calculated pixels (colors, borders, shadows, text glyphs) into raster bitmaps. Repaint occurs whenever visual styles change that do not affect geometry (e.g. `color`, `background-color`, `visibility`).
- **Cost**: **Reflow is significantly more computationally expensive** than Repaint because it involves recursive geometric tree calculations across the entire layout tree.
</details>

<details>
<summary><strong>5. Explain Layout Thrashing (Forced Synchronous Layout) and how to avoid it.</strong></summary>

**Answer:**
Modern browser engines batch layout recalculations and flush them asynchronously at the end of the current frame (e.g. right before V-Sync). 

**Layout Thrashing** occurs when JavaScript interleaves DOM style writes with DOM geometry reads in a loop:
```javascript
for (let item of items) {
  item.style.width = '100px'; // Invalidate layout (Write)
  console.log(item.offsetWidth); // Force immediate recalculation! (Read)
}
```
Because the browser must return accurate geometry for `offsetWidth`, it is forced to execute an immediate synchronous layout calculation inside the loop. In a loop of 100 items, the browser reflows the page 100 times in a single frame.

**Solution**: Decouple reads and writes. Perform all reads first, then execute all writes.
</details>

<details>
<summary><strong>6. Does an element with display: none trigger Reflow when toggled? What about visibility: hidden?</strong></summary>

**Answer:**
- Toggling `display: none` **triggers Reflow and Repaint**: When `display: none` is applied, the node is completely removed from the Render Tree; when removed, the node and all its children are re-inserted into the Render Tree, requiring the browser to calculate geometry for the element and shift surrounding content.
- Toggling `visibility: hidden` **triggers Repaint only**: The element remains in the Render Tree and retains its exact physical layout box and bounding dimensions. Because no geometric coordinates change, no Reflow occurs; the browser simply skips painting the pixels.
</details>

<details>
<summary><strong>7. How does the browser determine which elements belong in the Render Tree?</strong></summary>

**Answer:**
The browser traverses the DOM tree starting from the root `<html>` element:
1. Non-visual elements that do not produce render boxes are completely excluded: `<head>`, `<meta>`, `<title>`, `<link>`, `<script>`, `<style>`.
2. Any element (and its descendants) whose computed CSS resolves to `display: none` is completely excluded from the Render Tree.
3. Elements with `visibility: hidden` or `opacity: 0` **are included** in the Render Tree because they occupy physical layout space.
4. Pseudo-elements (such as `::before` and `::after`) that generate boxes are added to the Render Tree, even though they do not exist in the DOM!
</details>

<details>
<summary><strong>8. What is GPU Compositing, and why are transform and opacity performant?</strong></summary>

**Answer:**
In modern browser architectures, page content is partitioned into multiple independent bitmap **Compositing Layers** (e.g. elements with `will-change: transform`, `<video>`, or 3D transforms).

When an animation mutates properties like `left`, `top`, or `width`, the browser must execute Layout and Paint on the CPU. However, when mutating `transform` or `opacity`:
1. Neither Layout nor Paint is invalidated.
2. The layer's existing bitmap texture is already stored in GPU VRAM.
3. The GPU simply applies matrix transformation (scaling, translation) or alpha blending during the **Composite stage**.
4. This offloads the animation to the GPU hardware thread, guaranteeing buttery 60fps/120fps performance without main thread CPU bottlenecks.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Optimizing Script Tags for Fast Parsing</strong></summary>

**Problem:** You are given an HTML file with three script tags in the `<head>`:
1. `analytics.js` (independent tracking script)
2. `library.js` (core UI utility library)
3. `main.js` (depends on `library.js`)
Apply the optimal attributes (`async`, `defer`) to maximize parsing performance while preserving dependency execution order.

**Solution:**
```html
<head>
  <!-- Independent analytics: download in background, execute as soon as ready -->
  <script src="analytics.js" async></script>

  <!-- Dependent scripts: download in parallel, execute in exact source order after DOM parse -->
  <script src="library.js" defer></script>
  <script src="main.js" defer></script>
</head>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Preventing Cumulative Layout Shift on Images</strong></summary>

**Problem:** Fix the following markup to ensure the browser's layout engine can compute the aspect ratio and reserve layout space before the image downloads:
```html
<img src="hero.jpg" alt="Conference Keynote">
```

**Solution:**
```html
<!-- Explicit intrinsic dimensions reserve aspect-ratio box during layout! -->
<img src="hero.jpg" alt="Conference Keynote" width="1200" height="600">
```
*(In modern CSS, the browser automatically applies `aspect-ratio: width / height`, preventing any layout shifts when image bytes load).*
</details>

<details>
<summary><strong>Exercise 3 (Medium): Fixing a Layout Thrashing Function</strong></summary>

**Problem:** Refactor the following JavaScript function to eliminate forced synchronous layouts:
```javascript
function resizeParagraphs() {
  const paragraphs = document.querySelectorAll('p');
  for (let p of paragraphs) {
    if (p.offsetWidth > 500) {
      p.style.fontSize = '14px';
    }
  }
}
```

**Solution:**
```javascript
function resizeParagraphs() {
  const paragraphs = document.querySelectorAll('p');
  
  // Phase 1: Batch all reads
  const wideParagraphs = [];
  for (let p of paragraphs) {
    if (p.offsetWidth > 500) {
      wideParagraphs.push(p);
    }
  }
  
  // Phase 2: Batch all writes (Single layout pass at end of frame!)
  for (let p of wideParagraphs) {
    p.style.fontSize = '14px';
  }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Tracing Foster Parenting in Custom Markup</strong></summary>

**Problem:** Given the following malformed HTML, predict where the `<span>` element will be located in the final DOM tree:
```html
<table>
  <span>Misplaced Span</span>
  <tr><td>Cell 1</td></tr>
</table>
```

**Solution:**
Because a `<span>` is not permitted as a direct child of `<table>`, the tree construction dispatcher triggers foster parenting. The `<span>` is moved **outside and directly before the `<table>`** in the DOM:
```html
<span>Misplaced Span</span>
<table>
  <tbody>
    <tr><td>Cell 1</td></tr>
  </tbody>
</table>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Creating an Inlined Critical Rendering Path Shell</strong></summary>

**Problem:** Construct an HTML page skeleton where the critical hero content paints in under 50ms on simulated slow 3G network without waiting for external stylesheets.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Ultra Fast FCP</title>
  
  <!-- Inlined critical CSS for above-the-fold hero -->
  <style>
    body { margin: 0; font-family: system-ui; background: #fff; color: #111; }
    .hero { min-height: 80vh; display: flex; align-items: center; justify-content: center; }
    .hero h1 { font-size: 2.5rem; text-align: center; }
  </style>

  <!-- Non-critical external stylesheet loaded asynchronously -->
  <link rel="preload" href="full-site.css" as="style" onload="this.rel='stylesheet'">
  <noscript><link rel="stylesheet" href="full-site.css"></noscript>
</head>
<body>
  <section class="hero">
    <h1>Instantaneous First Paint</h1>
  </section>
</body>
</html>
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Simulating Render Tree Box Generation in Node.js</strong></summary>

**Problem:** Using a simple DOM-like data structure, write a function `computeRenderTree(node)` that recursively returns an array of nodes that would be included in the browser Render Tree, properly filtering out `<head>`, `<script>`, and nodes with `display: none`.

**Solution:**
```javascript
function computeRenderTree(node) {
  const nonRenderTags = ['HEAD', 'SCRIPT', 'STYLE', 'META', 'LINK', 'TITLE'];
  if (nonRenderTags.includes(node.tagName)) return null;
  if (node.style && node.style.display === 'none') return null;

  const renderNode = {
    tagName: node.tagName,
    text: node.textContent || '',
    children: []
  };

  if (node.children) {
    for (let child of node.children) {
      const childRender = computeRenderTree(child);
      if (childRender) renderNode.children.push(childRender);
    }
  }

  return renderNode;
}
```
</details>

---

## Summary & Further Reading

- The Critical Rendering Path consists of 6 stages: Network Bytes $\rightarrow$ Tokenization $\rightarrow$ DOM $\rightarrow$ CSSOM $\rightarrow$ Render Tree $\rightarrow$ Layout $\rightarrow$ Paint & Composite.
- The **Preload Scanner** scans ahead in HTML to discover assets speculatively while the main parser is busy.
- Use `defer` or `type="module"` to prevent JavaScript from blocking the HTML parser.
- Specify `width` and `height` on all images to eliminate Cumulative Layout Shift (CLS).
- Avoid Layout Thrashing by batching DOM reads before DOM writes.
- **Next Topic**: [03-html-history-and-the-living-standard.md](03-html-history-and-the-living-standard.md) explores the history of HTML, the XML/XHTML schism, the creation of WHATWG, and the modern Web Platform Baseline.

---
[Previous](01-what-is-html-and-the-web.md) | [Section Index](README.md) | [Next](03-html-history-and-the-living-standard.md)
