# Embedded SVG, Vector Graphics, and SVG Accessibility

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C Scalable Vector Graphics (SVG) 2 / WHATWG HTML Living Standard (§4.8.16 SVG) |
| **Prerequisites** | `01-fundamentals/01-document-structure.md`, `01-fundamentals/06-images-and-alt-text.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (Inline SVG, `viewBox`, `<symbol>`, `<use>`, SVG accessibility patterns) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Accessible Informative SVG Chart with Title and Desc](#example-1-accessible-informative-svg-chart-with-title-and-desc)
   - [Example 2: Pure Decorative Icon with `aria-hidden` and `currentColor`](#example-2-pure-decorative-icon-with-aria-hidden-and-currentcolor)
   - [Example 3: Enterprise SVG Sprite Sheet with `<symbol>` and `<use>`](#example-3-enterprise-svg-sprite-sheet-with-symbol-and-use)
   - [Example 4: Interactive Responsive SVG with CSS Animations](#example-4-interactive-responsive-svg-with-css-animations)
   - [Example 5: SVG Security Sanitization and XSS Prevention](#example-5-svg-security-sanitization-and-xss-prevention)
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

Scalable Vector Graphics (SVG) is an XML-based markup language for describing two-dimensional vector graphics. Unlike raster image formats (JPEG, PNG, WebP) which store fixed grids of colored pixels, SVG stores mathematical definitions of geometric primitives—lines, curves, polygons, and text. 

As a result, SVG graphics scale infinitely without any loss of sharpness or resolution, from tiny 16px navigation icons to gigantic 8K billboard displays, all while maintaining microscopic file sizes.

In modern web engineering, SVG is the undisputed foundation for iconography, interactive UI components, data visualization (D3.js), and brand identity assets. Because SVG elements can be embedded directly into HTML documents (**inline SVG**), they participate directly in the Document Object Model (DOM), CSS styling cascade, JavaScript event system, and browser Accessibility Object Model (AOM).

---

## 2. Core Concepts

### Five Ways to Include SVG in HTML
1. **Inline `<svg>`**: Embedded directly in HTML markup.
   - *Pros*: Fully style-able with CSS (`currentColor`, `fill`), scriptable with JS, zero extra HTTP requests.
   - *Cons*: Cannot be cached independently by browser HTTP cache, clutters HTML.
2. **`<img>` Tag (`<img src="icon.svg">`)**:
   - *Pros*: Browser HTTP cacheable, clean markup, safe from XSS (browser disables scripts in `<img>`).
   - *Cons*: Cannot style internal shapes with external CSS; no DOM interaction.
3. **CSS `background-image: url('icon.svg')`**:
   - *Pros*: Great for purely decorative UI accents.
   - *Cons*: Inaccessible to screen readers; no DOM or CSS control over internal shapes.
4. **`<svg><use href="#id" /></svg>` (Sprite System)**:
   - *Pros*: Combines the benefits of inline SVG with caching and modular code reuse.
5. **`<iframe>` or `<object>`**:
   - *Pros*: Supports internal interactive scripts within an isolated browsing context.
   - *Cons*: Heavy memory overhead; clumsy cross-boundary CSS styling.

### The `viewBox` Coordinate System
The `viewBox` attribute defines the internal coordinate space and aspect ratio of an SVG:
```html
<svg viewBox="min-x min-y width height">
```
Example: `viewBox="0 0 100 100"`.
Even if CSS resizes the `<svg>` to `width: 400px; height: 400px;`, every child element (e.g., `<circle cx="50" cy="50" r="40">`) scales proportionally within that virtual 100x100 canvas.

### The `currentColor` Keyword
In SVG CSS styling, setting `fill: currentColor;` or `stroke: currentColor;` causes the vector graphic to automatically inherit the computed CSS `color` property of its parent text. This makes icons seamlessly adapt to button states (hover, focus, dark mode) without duplicating SVG markup.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Inline SVG DOM & Rendering Engine Integration                          |
+-----------------------------------------------------------------------------------------+

  HTML Parser encounters <svg> tag
       |
       +--> Switches to XML/SVG Foreign Content Tokenizer
               |
               v
  +---------------------------------------------------------------+
  | Creates DOM Nodes in SVG Namespace:                           |
  | http://www.w3.org/2000/svg                                    |
  | - SVGSVGElement, SVGPathElement, SVGGElement                  |
  +---------------------------------------------------------------+
       |
       +--> CSS Engine matches selectors:
       |      .nav-icon { fill: currentColor; }
       |
       +--> Accessibility Tree (AOM) evaluates:
       |      - Is aria-hidden="true"? -> Exclude from AOM completely!
       |      - Has role="img" + <title>? -> Expose accessible name!
       |
       +--> GPU Vector Rasterizer:
              Tessellates bezier curves and paths directly into GPU draw commands!
```

---

## 4. Syntax & Usage

```html
<!-- Accessible Informative SVG -->
<svg 
  viewBox="0 0 24 24" 
  width="24" 
  height="24" 
  role="img" 
  aria-labelledby="search-icon-title"
  class="icon"
>
  <title id="search-icon-title">Search Documents</title>
  <path 
    fill="currentColor" 
    d="M15.5 14h-.79l-.28-.27A6.471 6.471 0 0 0 16 9.5 6.5 6.5 0 1 0 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"
  />
</svg>
```

> **Cross-Browser & Baseline Status Box**
>
> - Inline SVG and `viewBox`: **Baseline Widely Available** across all major browsers.
> - `<use href="#id">` (without `xlink:` namespace): **Baseline Widely Available** (SVG 2 standard supported across all modern browsers).
> - SVG accessibility (`role="img"`, `aria-labelledby` linking to `<title>`): **Baseline Widely Available**.

---

## 5. Practical Examples

### Example 1: Accessible Informative SVG Chart with Title and Desc
When an SVG communicates real information (such as a donut chart or diagram), it must provide a structured accessible name and description.

```html
<figure class="chart-container">
  <svg 
    viewBox="0 0 200 200" 
    width="300" 
    height="300" 
    role="img" 
    aria-labelledby="donut-title donut-desc" 
    class="donut-chart"
  >
    <title id="donut-title">Quarterly Browser Market Share</title>
    <desc id="donut-desc">
      A donut chart illustrating 65% Chromium, 20% Safari, 10% Firefox, and 5% Other.
    </desc>

    <!-- Background Track -->
    <circle cx="100" cy="100" r="70" class="donut-bg" />
    
    <!-- Data Segment: 65% Chromium -->
    <circle 
      cx="100" 
      cy="100" 
      r="70" 
      class="donut-segment segment-chrome" 
      stroke-dasharray="286 440" 
      stroke-dashoffset="0" 
    />
  </svg>
  
  <figcaption>Figure 1: Global desktop browser market breakdown.</figcaption>
</figure>

<style>
  .donut-chart {
    transform: rotate(-90deg); /* Start segment from 12 o'clock */
    display: block;
    margin: 0 auto;
  }
  .donut-bg {
    fill: none;
    stroke: #e6e6e6;
    stroke-width: 24;
  }
  .donut-segment {
    fill: none;
    stroke-width: 24;
    transition: stroke-dashoffset 0.6s ease;
  }
  .segment-chrome {
    stroke: #005a9c;
  }
</style>
```

---

### Example 2: Pure Decorative Icon with `aria-hidden` and `currentColor`
When an icon sits next to descriptive text (e.g., inside a button), the icon is purely decorative and must be hidden from screen readers to prevent redundant speech.

```html
<button type="button" class="action-btn">
  <!-- 
    aria-hidden="true" excludes SVG from the accessibility tree.
    focusable="false" prevents Internet Explorer / legacy Edge from tabbing into the SVG.
  -->
  <svg 
    viewBox="0 0 24 24" 
    width="18" 
    height="18" 
    aria-hidden="true" 
    focusable="false"
    class="btn-icon"
  >
    <path fill="currentColor" d="M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z" />
  </svg>
  <span>Create New Project</span>
</button>

<style>
  .action-btn {
    display: inline-flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    font-size: 1rem;
    font-weight: 600;
    color: #ffffff;
    background-color: #0b7285;
    border: none;
    border-radius: 6px;
    cursor: pointer;
  }
  .action-btn:hover {
    background-color: #085664;
  }
  .btn-icon {
    /* Uses parent text color automatically */
    fill: currentColor;
  }
</style>
```

---

### Example 3: Enterprise SVG Sprite Sheet with `<symbol>` and `<use>`
The gold standard for multi-icon web applications: define icons once inside a hidden `<svg>` sprite, and instantiate them anywhere via `<use>`.

```html
<!-- 1. Global Sprite Definitions (Placed once at top or bottom of body) -->
<svg xmlns="http://www.w3.org/2000/svg" style="display: none;" aria-hidden="true">
  <!-- User Profile Icon Symbol -->
  <symbol id="icon-user" viewBox="0 0 24 24">
    <path fill="currentColor" d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"/>
  </symbol>

  <!-- Notification Bell Icon Symbol -->
  <symbol id="icon-bell" viewBox="0 0 24 24">
    <path fill="currentColor" d="M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2zm6-6v-5c0-3.07-1.63-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5s-1.5.67-1.5 1.5v.68C7.64 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z"/>
  </symbol>
</svg>

<!-- 2. Clean, Modular Usage Anywhere in the Page -->
<nav class="user-nav">
  <button type="button" class="icon-trigger" aria-label="View notifications">
    <svg class="ui-icon" aria-hidden="true" focusable="false">
      <!-- Modern SVG 2 standard href replaces deprecated xlink:href -->
      <use href="#icon-bell" />
    </svg>
  </button>

  <a href="/profile" class="profile-link">
    <svg class="ui-icon" aria-hidden="true" focusable="false">
      <use href="#icon-user" />
    </svg>
    <span>My Account</span>
  </a>
</nav>

<style>
  .ui-icon {
    width: 24px;
    height: 24px;
    fill: currentColor;
    display: inline-block;
    vertical-align: middle;
  }
</style>
```

---

### Example 4: Interactive Responsive SVG with CSS Animations
Demonstrating how inline SVGs can be styled and animated using standard CSS keyframes and hover transitions.

```html
<div class="card-status">
  <svg viewBox="0 0 100 100" width="80" height="80" class="animated-spinner" aria-hidden="true">
    <circle 
      cx="50" 
      cy="50" 
      r="40" 
      class="spinner-track"
    />
    <circle 
      cx="50" 
      cy="50" 
      r="40" 
      class="spinner-indicator"
    />
  </svg>
  <p role="status">Processing backup archive...</p>
</div>

<style>
  .animated-spinner {
    animation: rotate-spinner 2s linear infinite;
  }
  .spinner-track {
    fill: none;
    stroke: #eee;
    stroke-width: 8;
  }
  .spinner-indicator {
    fill: none;
    stroke: #0284c7;
    stroke-width: 8;
    stroke-linecap: round;
    stroke-dasharray: 200;
    stroke-dashoffset: 150;
    animation: dash-pulse 1.5s ease-in-out infinite alternate;
  }
  @keyframes rotate-spinner {
    100% { transform: rotate(360deg); }
  }
  @keyframes dash-pulse {
    0% { stroke-dashoffset: 180; }
    100% { stroke-dashoffset: 50; }
  }
</style>
```

---

### Example 5: SVG Security Sanitization and XSS Prevention
SVG files can contain `<script>`, `onload` attributes, and external CSS references. Here is how to prevent XSS when accepting SVG uploads.

```html
<!-- DANGEROUS: Untrusted user SVG uploaded and rendered inline -->
<!-- <div id="container"> <svg><script>stealCookies()</script></svg> </div> -->

<!-- SAFE PATTERN 1: Render untrusted SVGs via <img> -->
<!-- Browsers enforce that images loaded via <img> CANNOT execute JavaScript -->
<img 
  src="/uploads/user-avatar.svg" 
  alt="User uploaded avatar vector" 
  width="120" 
  height="120" 
  loading="lazy"
/>

<!-- SAFE PATTERN 2: If inline SVG is mandatory, sanitize using DOMPurify -->
<div id="sanitized-svg-box"></div>

<script>
  // Simulated un-sanitized payload containing malicious script
  const rawSvgPayload = `
    <svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
      <circle cx="50" cy="50" r="40" fill="green" />
      <script>alert("XSS Vulnerability!");<\/script>
    </svg>
  `;

  // Use DOMPurify with SVG profile to strip all <script>, <a>, and event handlers
  // const cleanSvg = DOMPurify.sanitize(rawSvgPayload, { USE_PROFILES: { svg: true } });
  // document.getElementById('sanitized-svg-box').innerHTML = cleanSvg;
</script>
```

---

## 6. Best Practices

- [ ] **Always define a `viewBox` attribute on root `<svg>`**: Without a `viewBox`, resizing an SVG with CSS causes clipping or unpredictable whitespace rather than proportional scaling.
- [ ] **Mark decorative icons with `aria-hidden="true"` and `focusable="false"`**: Prevents screen readers from announcing meaningless glyphs and stops legacy browsers from tabbing into the graphic.
- [ ] **Provide `role="img"` and `aria-labelledby` on informative SVGs**: Ensures assistive technologies reliably recognize the SVG as an image element and announce the `<title>` and `<desc>`.
- [ ] **Use `fill: currentColor;` for monochromatic icons**: Allows vector assets to automatically inherit text colors and adapt to CSS theme variables.
- [ ] **Adopt modern `href` over deprecated `xlink:href`**: SVG 2 standardizes standard `href` for `<use>` references (e.g., `<use href="#icon-id" />`).
- [ ] **Never inject un-sanitized user-submitted SVG inline into the DOM**: SVGs can contain executable `<script>` tags, causing stored Cross-Site Scripting (XSS). Either sanitize with DOMPurify or render via `<img src="user.svg">`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Omitting `viewBox` on `<svg>` | **High** | SVG will not scale responsively with CSS; shapes clip or render at incorrect coordinates. | Always specify `viewBox="0 0 width height"`. |
| Unlabeled informative SVG | **Critical** | Screen reader skips the graphic or reads garbled path data (WCAG 1.1.1 violation). | Add `role="img"`, `<title id="t">`, and `aria-labelledby="t"`. |
| Omitting `aria-hidden="true"` on decorative icons inside buttons | **Moderate** | Screen reader reads both the button label and extraneous icon noise. | Add `aria-hidden="true"` to decorative icons. |
| Injecting user-uploaded SVG into `innerHTML` | **Critical** | User-controlled `<script>` runs, causing stored XSS. | Sanitize SVG or serve via `<img src="...">`. |
| Using deprecated `xlink:href` in modern projects | **Low** | Requires XML namespace declaration; outdated SVG 1.1 syntax. | Use standard `href="#icon-id"`. |
| Hardcoding fill colors (`fill="#000"`) inside reusable symbols | **Moderate** | Prevents CSS from overriding the icon's color dynamically. | Replace hardcoded fills with `fill="currentColor"`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 1.1.1 Non-text Content**: Informative vector graphics require equivalent textual descriptions via `<title>` and `<desc>`. Decorative icons must be hidden using `aria-hidden="true"`.
- **Contrast**: Vector paths representing text or interactive boundaries must meet WCAG 3:1 contrast against their background.

### Security Defenses
- **XSS in SVG**: SVG is essentially an XML document. It supports `<script>`, `onload="..."`, `href="javascript:..."`, and external entity injection (XXE). Never serve SVGs with `Content-Type: image/svg+xml` without sanitizing them on the server or setting a strict Content Security Policy.

---

## 9. Comparison / When to Use What

| Implementation Method | CSS Styling | JavaScript Access | Browser Cacheable | XSS Risk | Best For |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Inline `<svg>`** | Full (CSS fill, stroke, vars) | Full DOM access | No (part of HTML) | High (if untrusted) | Interactive charts, animated icons, dynamic UI. |
| **`<img>` tag** | None (Static) | No DOM access | Yes (Standard HTTP) | **Zero (Scripts blocked)** | Static illustrations, user avatars, content images. |
| **`<use href="#id">` (Sprite)** | Good (`currentColor`) | Limited | Yes (if external file) | Low | Large icon sets across large multi-page web applications. |
| **CSS `background-image`** | None | No DOM access | Yes | Zero | Decorative UI backgrounds, button bullets. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does the <code>viewBox</code> attribute on an <code>&lt;svg&gt;</code> element actually define?</strong></summary>

The `viewBox` attribute specifies the internal coordinate system and aspect ratio of the SVG canvas. It accepts four numbers: `min-x, min-y, width, height`. The browser maps this virtual coordinate space onto the rendered display dimensions defined by the SVG's `width` and `height` (or CSS properties), automatically scaling and fitting all child paths and geometric shapes without distortion according to the `preserveAspectRatio` setting.
</details>

<details>
<summary><strong>2. How do you make an inline SVG fully accessible to screen readers?</strong></summary>

For an informative SVG:
1. Add `role="img"` to the `<svg>` root element so assistive technologies treat it as an image.
2. Insert a `<title id="svg-title">` element as the first child of the SVG.
3. Optionally add a `<desc id="svg-desc">` for detailed descriptions.
4. Link them via `aria-labelledby="svg-title svg-desc"` on the `<svg>` root.
For a decorative SVG, simply add `aria-hidden="true"` and `focusable="false"`.
</details>

<details>
<summary><strong>3. Why is an SVG loaded via an <code>&lt;img&gt;</code> tag immune to Cross-Site Scripting (XSS)?</strong></summary>

Browser specifications enforce strict security sandboxing on all resources loaded via `<img>`. Even if an SVG file contains `<script>alert(1)</script>` or inline `onload` handlers, the browser's image rendering pipeline executes the SVG in a restricted, stateless image mode where script execution, form submissions, and external network requests are completely disabled.
</details>

<details>
<summary><strong>4. How does the <code>currentColor</code> CSS keyword work with SVG iconography?</strong></summary>

`currentColor` is an indirect CSS value that resolves to the computed value of the element's (or its closest ancestor's) `color` property. When an SVG icon specifies `fill="currentColor"`, the icon automatically inherits the text color of its parent container. For instance, if a button has text that changes from black to white on hover, the icon automatically matches that color without custom SVG classes.
</details>

<details>
<summary><strong>5. What is an SVG Sprite, and how does the <code>&lt;use&gt;</code> element reference symbols?</strong></summary>

An SVG sprite is a collection of SVG graphics grouped into a single document or container, where each graphic is enclosed inside a `<symbol id="unique-name" viewBox="...">` element. Other parts of the HTML document instantiate these graphics by referencing their IDs via `<svg><use href="#unique-name" /></svg>`. This avoids repeating complex SVG path markup across the DOM while allowing individual instances to be styled with CSS.
</details>

<details>
<summary><strong>6. Why is <code>focusable="false"</code> commonly added to inline SVGs alongside <code>aria-hidden="true"</code>?</strong></summary>

Legacy versions of Internet Explorer and early versions of Microsoft Edge suffered from a bug where inline `<svg>` elements were mistakenly included in the keyboard `Tab` navigation order as focusable objects. Adding `focusable="false"` explicitly tells these user agents not to trap focus on the SVG icon.
</details>

<details>
<summary><strong>7. What is the difference between <code>fill</code> and <code>stroke</code> in SVG?</strong></summary>

- `fill`: Paints the interior area enclosed by a vector path, polygon, or shape.
- `stroke`: Paints the boundary outline or perimeter of a vector path or shape, with configurable properties like `stroke-width`, `stroke-linecap`, and `stroke-dasharray`.
</details>

<details>
<summary><strong>8. Can an external SVG file referenced via <code>&lt;use href="icons.svg#menu"&gt;</code> be styled with CSS from the parent HTML document?</strong></summary>

External SVG sprites referenced across origins are subject to CORS. When loaded same-origin, styles declared in the parent document can influence properties inherited via the shadow tree (such as `fill: currentColor;` and `stroke`). However, direct CSS selectors targeting internal paths inside the external `<symbol>` cannot pierce the closed shadow root created by `<use>`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Bookmark Icon (Easy)
- **Task**: Author an accessible inline SVG representing a bookmark. It must be informative (not decorative), have `role="img"`, a `<title>` linked via `aria-labelledby`, a `24x24` viewBox, and use `fill="currentColor"`.

<details>
<summary>View Solution</summary>

```html
<svg 
  viewBox="0 0 24 24" 
  width="24" 
  height="24" 
  role="img" 
  aria-labelledby="bookmark-title"
  class="bookmark-icon"
>
  <title id="bookmark-title">Save to Bookmarks</title>
  <path 
    fill="currentColor" 
    d="M17 3H7c-1.1 0-2 .9-2 2v16l7-3 7 3V5c0-1.1-.9-2-2-2z"
  />
</svg>
```
</details>

---

### Exercise 2: Two-Icon SVG Sprite Sheet (Medium)
- **Task**: Build a hidden SVG sprite containing two symbols: `icon-check` and `icon-close`. Create two separate buttons using `<use>` that instantiate each icon with `aria-hidden="true"`.

<details>
<summary>View Solution</summary>

```html
<!-- Sprite Definitions -->
<svg style="display: none;" aria-hidden="true">
  <symbol id="icon-check" viewBox="0 0 24 24">
    <path fill="currentColor" d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/>
  </symbol>
  <symbol id="icon-close" viewBox="0 0 24 24">
    <path fill="currentColor" d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
  </symbol>
</svg>

<!-- Button 1: Accept -->
<button type="button" class="btn-confirm">
  <svg width="20" height="20" aria-hidden="true" focusable="false">
    <use href="#icon-check" />
  </svg>
  <span>Accept</span>
</button>

<!-- Button 2: Reject -->
<button type="button" class="btn-cancel">
  <svg width="20" height="20" aria-hidden="true" focusable="false">
    <use href="#icon-close" />
  </svg>
  <span>Reject</span>
</button>
```
</details>

---

### Exercise 3: Animated SVG Circle Progress Bar (Hard)
- **Task**: Create an interactive SVG circle component with a radius of `45` and a stroke width of `10`. Write the CSS to configure `stroke-dasharray` and `stroke-dashoffset` representing a 75% complete task, with full accessibility annotations.

<details>
<summary>View Solution</summary>

```html
<div class="progress-widget" role="progressbar" aria-valuenow="75" aria-valuemin="0" aria-valuemax="100" aria-label="Upload progress">
  <svg viewBox="0 0 100 100" width="120" height="120" aria-hidden="true" class="progress-svg">
    <!-- Circumference = 2 * PI * r = 2 * 3.14159 * 40 = 251.32 -->
    <!-- Background Track -->
    <circle cx="50" cy="50" r="40" class="track" />
    
    <!-- 75% complete: stroke-dashoffset = 251.32 * (1 - 0.75) = 62.83 -->
    <circle cx="50" cy="50" r="40" class="bar" />
  </svg>
  <span class="progress-label">75%</span>
</div>

<style>
  .progress-widget {
    position: relative;
    width: 120px;
    height: 120px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
  }
  .progress-svg {
    transform: rotate(-90deg);
  }
  .track {
    fill: none;
    stroke: #e2e8f0;
    stroke-width: 8;
  }
  .bar {
    fill: none;
    stroke: #3b82f6;
    stroke-width: 8;
    stroke-linecap: round;
    stroke-dasharray: 251.32;
    stroke-dashoffset: 62.83; /* 75% filled */
    transition: stroke-dashoffset 0.5s ease;
  }
  .progress-label {
    position: absolute;
    font-weight: bold;
    font-family: sans-serif;
    color: #1e293b;
  }
</style>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                    SVG IN HTML CHEAT SHEET                                      |
+-------------------------------------------------------------------------------------------------+
| Technique / Attribute    | Purpose / Rule                                                       |
+-------------------------------------------------------------------------------------------------+
| viewBox="0 0 w h"        | Defines internal coordinate space (MANDATORY for fluid scaling)      |
| fill="currentColor"      | Inherits CSS color from parent element                               |
| aria-hidden="true"       | Hides decorative icons from screen readers                           |
| role="img" + <title>     | Exposes informative SVG in the accessibility tree                    |
| <symbol id="...">        | Defines reusable vector template in a sprite sheet                   |
| <use href="#id">         | Instantiates a symbol template anywhere in the document              |
| <img src="icon.svg">     | Renders SVG in secure sandboxed mode (scripts disabled)              |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Scalable Vector Graphics (SVG) 2 Specification](https://www.w3.org/TR/SVG2/)
- [MDN Web Docs: SVG: Scalable Vector Graphics](https://developer.mozilla.org/en-US/docs/Web/SVG)
- [CSS-Tricks: A Complete Guide to SVG Fallbacks](https://css-tricks.com/)
- [Sara Soueidan: Accessible SVGs In-Depth Guide](https://www.sarasoueidan.com/blog/accessible-svgs/)

---

[← Previous: iFrames & Sandboxing](04-iframes-and-sandboxing.md) | [Section Index](README.md) | [Next: Canvas vs. SVG →](06-canvas-vs-svg.md)
