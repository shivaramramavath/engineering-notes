# Ways to Add CSS

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `00-setup/01-what-is-css-and-the-cascade-idea.md`, `00-setup/02-how-the-browser-renders-css.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Foundation) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Playwright css_runner.js, Chrome DevTools Network & Coverage, stylelint |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

### What are the Ways to Add CSS?
CSS can be attached to an HTML document through four primary integration mechanisms:
1. **External Stylesheet**: Linking an autonomous `.css` file via the `<link rel="stylesheet" href="...">` element inside the `<head>`.
2. **Internal (Embedded) Stylesheet**: Declaring rules within an inline `<style>` block directly inside HTML `<head>`.
3. **Inline Styles**: Declaring rules directly on individual HTML elements using the `style="..."` attribute.
4. **Imported Stylesheets**: Referencing secondary stylesheets from within an existing stylesheet using the `@import url(...)` at-rule.

Modern web architectures also leverage **CSS Modules**, **CSS in JS**, and **Constructable Stylesheets** (`adoptedStyleSheets`), but all ultimately resolve into these fundamental browser representations.

### Why Does the Integration Method Matter?
How you deliver CSS fundamentally dictates:
- **Network Performance & Critical Path**: External sheets require round-trip network requests (RTT), while embedded styles increase initial HTML payload size.
- **Cache Reusability**: External CSS files are cached by browser HTTP caches and CDNs across page transitions; internal and inline styles must be downloaded again on every document request.
- **Maintainability & Separation of Concerns**: Inline styles scatter presentation across markup, break design system tokens, and cannot utilize pseudo-classes (`:hover`, `:focus-visible`) or media queries.
- **Cascade Specificity**: Inline styles bypass selector specificity rules and outrank all normal author rules in the cascade.

---

## 2. Core Concepts

### 1. External Stylesheets (`<link rel="stylesheet">`)
The industry standard for multi-page web applications. The browser downloads the CSS asynchronously in parallel with other assets, but blocks initial rendering until parsing completes to avoid Flash of Unstyled Content (FOUC).
```html
<head>
  <link rel="stylesheet" href="/styles/main.css">
</head>
```
- **Advantages**: Cached across routes, enables strict Separation of Concerns, clean HTML markup, allows browser preloading and compression (Brotli/Gzip).
- **Disadvantages**: Introduces extra network latency on cold starts (First Contentful Paint).

### 2. Internal / Embedded Styles (`<style>`)
Styles placed inside a `<style>` block in the HTML `<head>`. Commonly used for "Critical CSS"—the minimal rules needed to render above-the-fold content instantly before external sheets download.
```html
<head>
  <style>
    :root { --brand-primary: #0284c7; }
    body { font-family: system-ui, sans-serif; margin: 0; }
  </style>
</head>
```
- **Advantages**: Zero network round-trips for the CSS file itself; immediate CSSOM construction on HTML stream receipt.
- **Disadvantages**: Bloats HTML size, cannot be cached independently from the HTML document, increases time-to-first-byte (TTFB) rendering overhead if oversized.

### 3. Inline Styles (`style="..."`)
Declarations placed directly on the HTML element via its `style` attribute.
```html
<div style="display: flex; gap: 1rem; color: #0f172a;">...</div>
```
- **Advantages**: Useful for dynamic, runtime values calculated by JavaScript (e.g., coordinates from dragging, progress bar percentages, CSS Custom Property overrides).
- **Disadvantages**: Highest specificity (`1-0-0-0`), impossible to cache, cannot define pseudo-classes (`:hover`, `:active`), pseudo-elements (`::before`, `::after`), `@keyframes`, or media queries.

### 4. The `@import` Rule
The `@import` at-rule allows a stylesheet to load another external stylesheet from within itself.
```css
/* Inside base.css */
@import url("typography.css");
@import url("components.css");
```
> [!WARNING]
> `@import` is considered an anti-pattern in modern performance engineering. It hides secondary CSS resources from the browser's preload scanner, creating a sequential **request waterfall** that drastically increases First Contentful Paint (FCP) and Largest Contentful Paint (LCP).

### 5. Media-Targeted Loading (`media` attribute)
You can assign media queries directly to `<link>` tags:
```html
<!-- Blocks rendering only on print dialog -->
<link rel="stylesheet" href="print.css" media="print">
<!-- Blocks rendering only on viewports >= 1024px -->
<link rel="stylesheet" href="desktop.css" media="(min-width: 1024px)">
```
The browser still downloads non-matching stylesheets at low priority (for rapid responsiveness if conditions change), but **they do not block the critical rendering path**!

---

## 3. How It Works Internally

### Parser Flow & The `@import` Waterfall Trap
When a browser HTML parser encounters a `<link rel="stylesheet">`, the background **Preload Scanner** detects it immediately and initiates parallel network streaming.

However, when styles use `@import`, the browser must:
1. Download HTML.
2. Discover and download `main.css`.
3. Parse `main.css`.
4. Discover `@import url("components.css")`.
5. Only then start downloading `components.css`.
6. Parse `components.css` and construct the full CSSOM.

```
PARALLEL LOADING (<link>):
HTML Download  : |========|
main.css       :    |==========|
components.css :    |============|
Render Block   :                 |==> Paint

SEQUENTIAL WATERFALL (@import):
HTML Download  : |========|
main.css       :    |==========|
@import Parse  :               |===|
components.css :                   |============|
Render Block   :                                |==> Paint (Delayed by 1 RTT!)
```

### Cascade Resolution Order of Injection
The cascade resolves rules based on **Order of Appearance** when specificity is identical:
1. An internal `<style>` element placed *after* a `<link>` element will override matching rules in the `<link>` file.
2. A `<link>` element placed *after* a `<style>` element will override matching rules in the `<style>` block.
3. An inline `style="..."` attribute always overrides external and internal rules (unless overridden by an `!important` declaration in external/internal CSS).

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| <link rel="stylesheet">, <style>, style attribute, and @import have     |
| been standardized across all web browsers since CSS Level 1 (1996).     |
+-------------------------------------------------------------------------+
```

### External Stylesheet
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Modern CSS Architecture</title>
  
  <!-- Preconnect to external font CDN if applicable -->
  <link rel="preconnect" href="https://fonts.example.com" crossorigin>
  
  <!-- High-priority main stylesheet -->
  <link rel="stylesheet" href="/assets/css/main.css">
  
  <!-- Non-blocking print stylesheet -->
  <link rel="stylesheet" href="/assets/css/print.css" media="print">
</head>
<body>
  <main class="container">
    <h1>Performance-First CSS Loading</h1>
  </main>
</body>
</html>
```

### Preloading Critical Stylesheets
When a stylesheet is critical but discovered late or dynamically inserted, use `rel="preload"` paired with standard loading:
```html
<!-- Speculatively fetch with highest network priority -->
<link rel="preload" href="/assets/css/theme-dark.css" as="style">
<!-- Standard application stylesheet -->
<link rel="stylesheet" href="/assets/css/theme-dark.css">
```

---

## 5. Practical Examples

### Example 1: Basic Integration Patterns
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>CSS Integration Methods</title>
  
  <!-- External stylesheet -->
  <link rel="stylesheet" href="styles.css">

  <!-- Internal stylesheet -->
  <style>
    .highlight {
      background-color: #fef08a;
      padding: 0.25rem 0.5rem;
      border-radius: 4px;
    }
  </style>
</head>
<body>
  <!-- Inline style used strictly for dynamic coordinate binding -->
  <div class="card highlight" style="--card-elevation: 4px;">
    Card Content
  </div>
</body>
</html>
```

### Example 2: Real-World Critical CSS Pattern
High-traffic e-commerce and media sites inline critical above-the-fold layout styles directly into HTML, deferring remaining components:
```html
<head>
  <meta charset="UTF-8">
  <!-- Critical CSS Inlined to guarantee sub-500ms First Contentful Paint -->
  <style>
    *, *::before, *::after { box-sizing: border-box; }
    body { margin: 0; font-family: system-ui, -apple-system, sans-serif; }
    .hero { min-height: 80vh; display: grid; place-items: center; background: #0f172a; color: #f8fafc; }
    .hero-title { font-size: clamp(2rem, 5vw, 4rem); font-weight: 800; }
  </style>

  <!-- Non-critical external stylesheet loaded asynchronously -->
  <link rel="preload" href="/css/non-critical.css" as="style" onload="this.onload=null;this.rel='stylesheet'">
  <noscript><link rel="stylesheet" href="/css/non-critical.css"></noscript>
</head>
```

### Example 3: Edge Case — Media Query Non-Blocking Behavior
Demonstrating how browser priority engines treat non-matching media stylesheets:
```html
<head>
  <!-- Active on mobile: BLOCKS initial render on mobile viewports -->
  <link rel="stylesheet" href="mobile.css" media="(max-width: 767px)">

  <!-- Inactive on mobile: DOES NOT block rendering on phone, loaded at Low Priority! -->
  <link rel="stylesheet" href="desktop.css" media="(min-width: 768px)">
</head>
```
*Verification*: In Chrome DevTools Network Tab, on a 375px viewport simulation, `desktop.css` appears with priority `Lowest` / `Low` and does not defer the `DOMContentLoaded` or First Contentful Paint events.

### Example 4: Failure Case — Deep Nested `@import` Chains
```css
/* Bad Practice: styles/app.css */
@import url("vendor/normalize.css");
@import url("base/typography.css");
@import url("components/buttons.css");
```
```css
/* Inside components/buttons.css */
@import url("icons.css"); /* Triggers a 3rd sequential network roundtrip! */
```
*Failure Analysis*: If each RTT is 80ms on 4G mobile, the browser stalls visual rendering for $80\text{ms} \times 3 = 240\text{ms}$ purely waiting on asset discovery before even initiating CSSOM calculation.

### Example 5: Accessible & Production-Grade Setup with CSP (Content Security Policy)
Enterprise security policies frequently prohibit inline styles (`style="..."` or `<style>`) via `Content-Security-Policy: default-src 'self'; style-src 'self'` to prevent cross-site scripting (XSS).
```html
<!-- Secure, CSP-compliant HTML -->
<head>
  <meta charset="UTF-8">
  <!-- Stylesheet loaded with cryptographic nonce or strict self origin -->
  <link rel="stylesheet" href="/dist/bundle.min.css">
</head>
<body>
  <!-- Clean semantic markup with class tokens instead of inline styles -->
  <button type="button" class="btn btn-primary" data-status="active">
    Submit Order
  </button>
</body>
```
*Computed Output Verification*:
```javascript
// Verified via Playwright test runner:
const btn = document.querySelector('.btn-primary');
const computed = window.getComputedStyle(btn);
console.log('Background:', computed.backgroundColor); // rgb(2, 132, 199)
console.log('Cursor:', computed.cursor); // pointer
```

---

## 6. Best Practices

1. **Use Single Bundled External Stylesheet in Production**: Concatenate, minify, and hash production stylesheets (e.g., `app.c7a8b1.css`) to maximize HTTP caching efficiency.
2. **Never Use `@import` in Production Runtime CSS**: Bundle imported files at build time using PostCSS, Lightning CSS, or Vite rather than delivering raw `@import` rules to the browser.
3. **Limit Inline Styles to Dynamic Variables**: Restrict `style="..."` attributes strictly to dynamic CSS custom properties (e.g., `style="--x: 140px"`), avoiding static styling declarations.
4. **Leverage the `media` Attribute for Print & Specialized Layouts**: Offload print styles (`media="print"`) and extreme breakpoint sheets so they do not block the primary render path.
5. **Inline Critical CSS for High-Conversion Landing Pages**: If First Contentful Paint is a core KPI, inline the top ~10–14 KB of critical CSS directly into `<head>`.
6. **Enforce Content Security Policy (CSP)**: Avoid `style-src 'unsafe-inline'` in production to eliminate XSS injection vectors through manipulated DOM style attributes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Placing `<link rel="stylesheet">` at bottom of `<body>` | Misunderstanding async loading | Massive Flash of Unstyled Content (FOUC); jarring visual layout shift (CLS). | Always place stylesheets in `<head>`. |
| Using `@import` at top of CSS files | Convenience of modular files | Multi-tier network waterfalls, delayed First Contentful Paint. | Bundle files at build time; load single `<link>`. |
| Overusing `style="..."` inline attributes | Quick debugging or convenience | Overrides stylesheet cascade; cannot handle `:hover` or responsive breakpoints. | Apply reusable utility classes or BEM class names. |
| Inlining multi-megabyte stylesheets into `<head>` | Misinterpreting Critical CSS advice | Massive HTML document size; increases Time To First Byte (TTFB) and wastes bandwidth. | Keep inlined critical CSS under 14 KB (1 TCP packet). |
| Forgetting `media="print"` on print styles | Defaulting to universal loading | Print rules download and block screen rendering on cold page load. | Specify `<link rel="stylesheet" href="print.css" media="print">`. |
| Modifying DOM `element.style.left` in animations | Imperative JavaScript styling | Triggers synchronous layout reflow on every animation frame; drops to 20 FPS. | Animate `transform` via CSS classes or Web Animations API. |

---

## 8. Performance, Accessibility & Browser Support

### Performance Metrics Impact
- **First Contentful Paint (FCP)**: Directly tied to CSS delivery. Because CSS is render-blocking, FCP cannot trigger until all external `<head>` stylesheets are parsed.
- **Largest Contentful Paint (LCP)**: If the LCP element is styled by an external stylesheet delayed by `@import`, LCP score degrades significantly.
- **Cumulative Layout Shift (CLS)**: Late-loaded stylesheets (such as stylesheets injected via JavaScript without layout reservations) cause visual layout jumping.

### Accessibility Considerations
- **High Contrast & Forced Colors**: Delivering CSS through standard stylesheets allows users in Windows Contrast Themes or assistive software to override author colors cleanly via User Agent stylesheets.
- **Inline Style Overrides**: Overuse of inline `style="color: #666 !important"` damages user stylesheets written by visually impaired users needing 7:1 contrast ratios.

---

## 9. Comparison / When to Use What

| Method | Primary Use Case | Performance Profile | Caching | Specificity |
|:---|:---|:---|:---|:---|
| **External `<link>`** | Global application design, component libraries, typography | Parallel download, render-blocking | Long-term HTTP/CDN cacheable | Standard Selector Specificity |
| **Internal `<style>`** | Critical above-the-fold styling, standalone single-file widgets | 0 RTT, increases HTML payload | Re-downloaded with HTML document | Standard Selector Specificity |
| **Inline `style=""`** | Dynamic coordinates, user-configured themes, CSS custom property overrides | Zero download, bloats DOM tree | Not cacheable | Highest Author Specificity (`1-0-0-0`) |
| **`@import`** | Development-only quick imports (NOT production) | Worst: Sequential blocking waterfall | Cached individually per file | Follows declaration order |
| **Constructable Stylesheets** (`adoptedStyleSheets`) | Web Components & Shadow DOM encapsulation | Ultra-fast; shared memory instance across 1000s of elements | Managed in JS memory | Scoped to Shadow Root |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does CSS block browser rendering by default, whereas asynchronous JavaScript does not?</strong></summary>

CSS is render-blocking by design to prevent **Flash of Unstyled Content (FOUC)**. If the browser rendered HTML incrementally before receiving the CSSOM, users would see raw unstyled text, incorrect fonts, and unpositioned elements that would violently shift across the screen as soon as the stylesheet finished downloading. The browser deliberately pauses rendering until the CSSOM is constructed to ensure visual integrity and prevent severe layout instability (high CLS).
</details>

<details>
<summary><strong>2. Explain why `@import url("styles.css")` is severely detrimental to page load speed.</strong></summary>

When `<link>` tags are used in HTML, the browser's preload scanner detects all stylesheet URLs in parallel before parsing finishes. In contrast, `@import` statements are embedded inside CSS files. The browser cannot discover an `@import` until the parent stylesheet has been completely downloaded and parsed. This creates a sequential "waterfall": Download HTML &rarr; Download CSS 1 &rarr; Parse CSS 1 &rarr; Discover `@import` &rarr; Download CSS 2. This serializes network requests and delays First Contentful Paint by at least one full network Round Trip Time (RTT).
</details>

<details>
<summary><strong>3. How does the `media` attribute on a `<link>` element improve performance?</strong></summary>

The `media` attribute (e.g., `media="print"` or `media="(min-width: 1200px)"`) informs the browser under what conditions the stylesheet applies. If the media query evaluates to `false` for the current device/viewport, the browser still downloads the stylesheet with low network priority (so it is ready if the window is resized or printed), but **it does not block the Critical Rendering Path**. This allows the page to paint immediately without waiting for non-matching stylesheets.
</details>

<details>
<summary><strong>4. What is Critical CSS, and what is the optimal threshold for its payload size?</strong></summary>

Critical CSS is the minimal subset of CSS declarations required to render the "above-the-fold" content of a webpage (the visible viewport before scrolling). It is extracted and embedded directly inside `<style>` tags in the document `<head>`, while the non-critical stylesheet is loaded asynchronously. The optimal size budget is under **14 KB** (compressed), which corresponds to the initial TCP congestion window (IW14) payload received during the very first round trip.
</details>

<details>
<summary><strong>5. Can an external stylesheet override an inline style? If so, how?</strong></summary>

Yes. Although an inline style attribute has an extraordinarily high specificity tuple of `[1, 0, 0, 0]`, any external or internal CSS declaration flagged with `!important` will win. In the CSS cascade origin and importance rules, `!important` author declarations override all normal author declarations, including inline styles without `!important`.
</details>

<details>
<summary><strong>6. What are Constructable Stylesheets and `document.adoptedStyleSheets`?</strong></summary>

Constructable Stylesheets allow developers to create and mutate CSS stylesheets programmatically using `new CSSStyleSheet()`, and apply them across multiple Shadow DOM roots or the main document by assigning them to `adoptedStyleSheets = [sheet]`. Unlike creating multiple `<style>` tags, a Constructable Stylesheet is parsed once and shared by reference in memory, yielding massive performance benefits in large-scale Web Component applications.
</details>

<details>
<summary><strong>7. How does HTTP/2 and HTTP/3 multiplexing affect the decision between one single CSS bundle vs multiple split CSS files?</strong></summary>

Under HTTP/1.1, browsers were limited to ~6 concurrent TCP connections per domain, heavily penalizing multiple stylesheet requests and making monolithic single-file bundling mandatory. HTTP/2 and HTTP/3 introduce multiplexing, allowing dozens of requests simultaneously over a single connection with zero head-of-line blocking. While bundling is still recommended to reduce compression dictionary overhead, granular route-based code-splitting of CSS is now practical and efficient without incurring heavy connection penalties.
</details>

<details>
<summary><strong>8. Why should you avoid using `style-src 'unsafe-inline'` in your Content Security Policy?</strong></summary>

Permitting `'unsafe-inline'` allows attackers who discover a Cross-Site Scripting (XSS) vulnerability to inject malicious `<style>` tags or inline `style="..."` attributes. Attackers can abuse CSS injection to exfiltrate sensitive data (e.g., CSRF tokens or passwords) via CSS attribute selectors and background image URL requests (`input[value^="a"] { background: url('/exfil?a'); }`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Eliminate the `@import` Waterfall (Easy)
**Task**: Refactor the following HTML and CSS setup so that all stylesheets are fetched in parallel without sequential waterfall delays.
```html
<!-- Input HTML -->
<head>
  <link rel="stylesheet" href="main.css">
</head>
```
```css
/* Input main.css */
@import url("reset.css");
@import url("layout.css");
body { font-family: sans-serif; }
```

<details>
<summary><strong>View Solution</strong></summary>

```html
<head>
  <!-- Load all sheets in parallel using link tags in head -->
  <link rel="stylesheet" href="reset.css">
  <link rel="stylesheet" href="layout.css">
  <link rel="stylesheet" href="main.css">
</head>
```
```css
/* Refactored main.css */
body { font-family: sans-serif; }
```
*Explanation*: Moving references from `@import` inside CSS to top-level `<link>` tags in HTML allows the browser's preload scanner to discover and stream all three files simultaneously.
</details>

---

### Exercise 2: Configure Non-Blocking Print Stylesheets (Easy)
**Task**: Write the `<link>` tag to load `print.css` so that it applies only during document printing and does not block initial on-screen painting.

<details>
<summary><strong>View Solution</strong></summary>

```html
<link rel="stylesheet" href="/assets/print.css" media="print">
```
*Explanation*: Adding `media="print"` instructs the engine that this resource is not relevant for screen media, stripping it from the render-blocking list on initial paint.
</details>

---

### Exercise 3: Dynamic Theme Injection via CSS Custom Properties (Medium)
**Task**: An application receives a user-selected primary brand color (`#10b981`) dynamically from a database API. Implement a pattern that applies this color cleanly without scattering hardcoded hex codes across inline element styles.

<details>
<summary><strong>View Solution</strong></summary>

```html
<!-- Set the custom property once on the document root or container -->
<body style="--brand-primary: #10b981;">
  <header class="navbar">
    <button class="btn-primary">Save Changes</button>
  </header>
</body>
```
```css
/* Clean, reusable stylesheet rules */
:root {
  --brand-primary: #0284c7; /* Fallback default */
}

.btn-primary {
  background-color: var(--brand-primary);
  color: #ffffff;
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 4px;
}
```
*Explanation*: Injecting a single CSS custom property via inline style allows the entire stylesheet to remain fully cached, decoupled, and responsive while enabling dynamic runtime theming.
</details>

---

### Exercise 4: Asynchronous CSS Loading with Fallback (Medium)
**Task**: Implement an asynchronous loading pattern for `deferred.css` using `rel="preload"` that works without JavaScript enabled.

<details>
<summary><strong>View Solution</strong></summary>

```html
<!-- 1. Speculatively prefetch stylesheet without blocking render -->
<link rel="preload" href="/css/deferred.css" as="style" onload="this.onload=null;this.rel='stylesheet'">

<!-- 2. Fallback for users with JavaScript disabled -->
<noscript>
  <link rel="stylesheet" href="/css/deferred.css">
</noscript>
```
*Explanation*: The `rel="preload"` directive instructs the browser to stream the resource at high priority without applying it as a render-blocking stylesheet. The `onload` handler switches its `rel` to `'stylesheet'` once downloaded. The `<noscript>` tag guarantees rendering for non-JS clients.
</details>

---

### Exercise 5: Specificity Conflict Resolution (Hard)
**Task**: An element has an inline style `<p id="notice" class="alert" style="color: red;">Alert</p>`. In an external stylesheet, write a selector that changes this element's text color to `#1e293b` without editing the HTML markup.

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Solution: Use !important to override the inline author specificity */
#notice.alert {
  color: #1e293b !important;
}
```
*Explanation*: An inline style attribute has specificity `[1, 0, 0, 0]`. Normal external selectors cannot override it regardless of how many IDs or classes are chained. Adding `!important` moves the declaration into the "Author Important" cascade origin, which trumps normal author inline declarations.
</details>

---

### Exercise 6: Constructable Stylesheets in Shadow DOM (Hard)
**Task**: Write a modern JavaScript snippet that creates a single shared `CSSStyleSheet` instance and attaches it to two distinct Shadow DOM roots using `adoptedStyleSheets`.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
// 1. Create a single constructable stylesheet instance
const sharedSheet = new CSSStyleSheet();

// 2. Replace rules synchronously
sharedSheet.replaceSync(`
  :host {
    display: block;
    padding: 1rem;
    border-radius: 8px;
    background: #f8fafc;
  }
  .title {
    color: #0f172a;
    font-weight: 700;
  }
`);

// 3. Attach to two different shadow roots by reference
const host1 = document.querySelector('#widget-1').shadowRoot;
const host2 = document.querySelector('#widget-2').shadowRoot;

host1.adoptedStyleSheets = [sharedSheet];
host2.adoptedStyleSheets = [sharedSheet];
```
*Explanation*: Both shadow roots share the exact same parsed stylesheet object in memory. Updates to `sharedSheet` immediately reflect across both components without re-parsing CSS text.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| WAYS TO ADD CSS - QUICK REFERENCE CHEAT SHEET                                                   |
+----------------------+-----------------------------+--------------------+-----------------------+
| Method               | Syntax                      | Performance        | Primary Use Case      |
+----------------------+-----------------------------+--------------------+-----------------------+
| External Link        | <link rel="stylesheet" ...> | Render-blocking    | Production production |
| Preloaded Link       | <link rel="preload" ...>    | Async streaming    | Critical fonts/sheets |
| Non-blocking Media   | media="print"               | Non-blocking       | Print/conditional     |
| Embedded Style       | <style>...</style>          | 0 RTT, adds bytes  | Critical above-fold   |
| Inline Style         | style="..."                 | High specificity   | Dynamic variables     |
| CSS @import          | @import url(...)            | Sequential cascade | NEVER in production   |
| Constructable Sheet  | adoptedStyleSheets = [...]  | Zero DOM overhead  | Web Components        |
+----------------------+-----------------------------+--------------------+-----------------------+
```

---

## 13. Further Reading

- [W3C HTML 5.3: The link element](https://www.w3.org/TR/html53/document-metadata.html#the-link-element)
- [MDN Web Docs: Ways to apply CSS](https://developer.mozilla.org/en-US/docs/Learn/CSS/First_steps/How_CSS_is_structured)
- [web.dev: Defer non-critical CSS](https://web.dev/articles/defer-non-critical-css)
- [web.dev: Extract critical CSS](https://web.dev/articles/extract-critical-css)
- [W3C CSS Cascading and Inheritance Level 5](https://www.w3.org/TR/css-cascade-5/)

---

[Previous: 02 - How the Browser Renders CSS](02-how-the-browser-renders-css.md) | [Section Index](README.md) | [Next: 04 - DevTools for CSS](04-devtools-for-css.md)
