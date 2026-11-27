# CSS Modules, Spec Levels & Baseline

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `00-setup/01-what-is-css-and-the-cascade-idea.md`, `00-setup/02-how-the-browser-renders-css.md` |
| **Estimated Reading Time** | 27 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Industry Standard) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Web Platform Tests (WPT), @supports API, Playwright css_runner.js |

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

### What are CSS Spec Modules and Levels?
A frequent misconception among developers is asking: *"When will CSS4 or CSS5 be released?"* 

There will **never** be a monolithic "CSS4" release. After the completion of CSS Level 2 Revision 1 (CSS 2.1) in 2011, the W3C CSS Working Group (CSSWG) abandoned monolithic specifications because updating the entire language simultaneously stalled progress for years whenever a single feature stalled.

Instead, CSS was decentralized into dozens of independent, modular specifications called **CSS Modules**:
- **CSS Color Module**: Levels 3, 4, and 5
- **CSS Grid Layout Module**: Levels 1, 2 (Subgrid), and 3
- **CSS Selectors Module**: Levels 3 and 4
- **CSS Cascading and Inheritance**: Levels 3, 4, 5, and 6

Each module advances through the W3C standardization track at its own velocity.

### Why Does "Web Platform Baseline" Exist?
Because modules advance independently, developers and engineering teams historically struggled to answer a simple question: *"Can I safely use this CSS feature in production today without breaking users' browsers?"*

To solve this fragmentation, the **WebDX Community Group** (comprising Google, Apple, Microsoft, Mozilla, and Bocoup) created **Web Platform Baseline**:
- **Newly Available**: The feature is interoperable across the current stable release of all four major browser engines: **Chrome, Edge, Firefox, and Safari**.
- **Widely Available**: The feature has been interoperable across all core engines for at least **30 months**, meaning it is safe to use in enterprise production without polyfills or fallbacks.

---

## 2. Core Concepts

### 1. The W3C Standardization Stages
Every CSS module follows a formal lifecycle governed by the W3C:
1. **Editor's Draft (ED)**: Internal draft maintained by specification editors. Highly volatile and experimental.
2. **First Public Working Draft (FPWD)**: The first public release inviting broad community review.
3. **Working Draft (WD)**: Iterative refinements incorporating feedback from browser engine implementers and developers.
4. **Candidate Recommendation Snapshot (CR / CRS)**: The specification is functionally complete. Browser vendors are invited to build official implementations.
5. **Proposed Recommendation (PR)**: Two independent interoperable implementations have passed the comprehensive **Web Platform Tests (WPT)** suite.
6. **W3C Recommendation (REC)**: The official gold standard of the Web.

### 2. CSS Snapshots
Because there is no "CSS4", how do browser vendors declare overall CSS compliance? Through **CSS Snapshots** (e.g., CSS Snapshot 2023, CSS Snapshot 2024). A Snapshot is a document published by the CSSWG that lists all modular specifications that have reached stable, interoperable Candidate Recommendation status during that calendar year.

### 3. Web Platform Baseline Tiers
Web Platform Baseline categorizes features into clear adoption tiers:
```
+-------------------------------------------------------------------------+
| WEB PLATFORM BASELINE TIERS                                             |
|                                                                         |
| [1] LIMITED AVAILABILITY                                                |
|     Supported in only 1-3 engines. Experimental or flag-gated.         |
|     Example: CSS Anchor Positioning (Chrome/Edge 125+, Safari In-Prog)  |
|                                                                         |
| [2] NEWLY AVAILABLE (Baseline YYYY)                                     |
|     Supported in ALL 4 engines (Chrome, Edge, Firefox, Safari).         |
|     Example: CSS Nesting (Baseline 2023), :has() (Baseline 2023)        |
|                                                                         |
| [3] WIDELY AVAILABLE (Baseline 30+ Months)                              |
|     Supported in all 4 engines for > 30 months. Enterprise-safe.       |
|     Example: CSS Grid (Widely Available since 2019), Flexbox, rem units |
+-------------------------------------------------------------------------+
```

### 4. Feature Queries (`@supports`)
The native mechanism to inspect browser engine capabilities at runtime:
```css
/* Test property support */
@supports (display: grid) {
  .gallery { display: grid; }
}

/* Test modern selector support */
@supports selector(:has(img)) {
  .card:has(img) { border-radius: 12px; }
}
```

---

## 3. How It Works Internally

### The Web Platform Tests (WPT) Engine
How does the industry prove two browser engines implement a CSS module identically?
Every browser vendor (Chromium/Blink, Gecko/Firefox, WebKit/Safari) continuously runs against the open-source **Web Platform Tests (WPT)** repository.

```
+--------------------------------------------------------------------+
| W3C CSS WORKING GROUP SPECIFICATION                                |
| Defines normative syntax, grammar, and layout algorithm equations |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| WEB PLATFORM TESTS (WPT) SUITE                                     |
| 10,000+ Reftests comparing pixel-perfect browser rasterization     |
+--------------------------------------------------------------------+
          |                       |                       |
          v                       v                       v
+------------------+    +------------------+    +--------------------+
| CHROMIUM (Blink) |    | FIREFOX (Gecko)  |    | SAFARI (WebKit)    |
| Pass: 99.4%      |    | Pass: 99.2%      |    | Pass: 99.1%        |
+------------------+    +------------------+    +--------------------+
          |                       |                       |
          +-----------------------+-----------------------+
                                  |
                                  v
+--------------------------------------------------------------------+
| INTEROP INITIATIVE -> BASELINE STATUS CERTIFICATION                |
| Confirms "Newly Available" -> Starts 30-Month Clock to "Widely"    |
+--------------------------------------------------------------------+
```

- **Reftests**: Tests that load two separate HTML files (one using the new CSS feature, and one using an established reference layout). The testing harness takes a screenshot of both; if the rasterized bitmap differs by even 1 pixel, the test fails.
- **Interop Effort**: Annual industry commitments (Interop 2023, Interop 2024, Interop 2025) where Google, Apple, Microsoft, and Mozilla agree to resolve specific bugs in high-priority CSS modules to achieve 100% interoperability.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| The @supports at-rule (CSS Conditional Rules Level 3) is supported in    |
| all browsers since Chrome 28, Firefox 22, Safari 9, Edge 12.            |
+-------------------------------------------------------------------------+
```

### 1. Checking Property and Value Support
```css
/* Fallback for legacy browsers */
.badge {
  background-color: #0284c7;
}

/* Modern progressive enhancement with OKLCH */
@supports (background-color: oklch(0.6 0.22 250)) {
  .badge {
    background-color: oklch(0.6 0.22 250);
  }
}
```

### 2. Checking Modern Selector Support (`selector()`)
Supported across all major browsers since Baseline 2020:
```css
/* Test whether browser supports the relational parent selector */
@supports selector(:has(> .avatar)) {
  .user-card:has(> .avatar) {
    grid-template-columns: 48px 1fr;
  }
}
```

### 3. Logical Operators (`and`, `or`, `not`)
```css
@supports (display: grid) and (gap: 1rem) {
  .container {
    display: grid;
    gap: 1rem;
  }
}

@supports not (display: subgrid) {
  .nested-grid {
    /* Fallback polyfill or manual layout */
    display: grid;
    grid-template-columns: repeat(3, 1fr);
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic `@supports` Progressive Enhancement
```css
/* Base layout using Flexbox (Widely Available) */
.card-grid {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
}

.card-grid > * {
  flex: 1 1 calc(33.333% - 16px);
}

/* Enhanced layout using Grid (Widely Available) */
@supports (display: grid) {
  .card-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  }

  .card-grid > * {
    flex: initial;
  }
}
```

### Example 2: Real-World Color Enhancement with OKLCH & P3
```css
:root {
  /* Baseline sRGB fallback */
  --accent: #2563eb;
}

/* Enhanced vibrant wide-gamut color for modern monitors */
@supports (color: oklch(0.55 0.25 260)) {
  :root {
    --accent: oklch(0.55 0.25 260);
  }
}

.button-primary {
  background-color: var(--accent);
  color: #ffffff;
  padding: 0.75rem 1.5rem;
  border-radius: 6px;
  border: none;
}
```

### Example 3: Edge Case — Testing `@starting-style` for Popover Animations
`@starting-style` enables smooth CSS transition entry animations from `display: none`:
```css
/* Dialog / Popover entry animation */
.dialog-modal {
  transition: opacity 0.3s ease, transform 0.3s ease;
  opacity: 1;
  transform: scale(1);
}

/* Modern entry transition state */
@supports (transition-behavior: allow-discrete) {
  @starting-style {
    .dialog-modal[open] {
      opacity: 0;
      transform: scale(0.95);
    }
  }
}
```

### Example 4: Failure Case — Premature Adoption of Limited Availability Features
```css
/* Risky: CSS Anchor Positioning (Limited Availability in 2024) */
.tooltip {
  position: fixed;
  position-anchor: --my-anchor;
  top: anchor(bottom);
  left: anchor(center);
}
```
*Failure Analysis*: In Safari and Firefox versions where CSS Anchor Positioning is under active development, this tooltip will fail completely, rendering at `(0, 0)` in the top-left corner of the viewport and covering navigation elements.
*Remedy*: Always wrap Limited Availability features in `@supports (position-anchor: --anchor)` and provide a standard JavaScript or `position: absolute` fallback.

### Example 5: Accessible High-Contrast Support with Modern Fallback
```css
.card {
  background: #ffffff;
  color: #1e293b;
  border: 1px solid #e2e8f0;
}

/* Ensure users in Windows High Contrast / Forced Colors mode see clear boundaries */
@media (forced-colors: active) {
  .card {
    /* CanvasText and Canvas are system color keywords guaranteed to adapt */
    border: 2px solid CanvasText;
    background: Canvas;
    color: CanvasText;
  }
}
```
*Playwright Test Runner Verification*:
```javascript
// Test CSS supports evaluation
const supportsOKLCH = CSS.supports('color', 'oklch(0.6 0.2 150)');
console.log('Engine supports OKLCH:', supportsOKLCH); // true in modern engines
```

---

## 6. Best Practices

1. **Verify Baseline Status Before Committing to Production**: Use [web.dev/baseline](https://web.dev/baseline) or MDN tables to confirm whether a feature is "Widely Available" or "Newly Available".
2. **Apply Progressive Enhancement via `@supports`**: Author a reliable fallback for older devices first, then layer advanced features inside `@supports` blocks.
3. **Never Rely on Vendor Prefixes for New Features**: Vendor prefixes (`-webkit-`, `-moz-`, `-ms-`) are obsolete for modern CSS modules; modern engines use feature flags during development and ship un-prefixed standards.
4. **Use `@supports selector(...)` for Complex Selectors**: When using `:has()`, `:focus-visible`, or `:is()`, test selector support to prevent entire CSS rule blocks from being dropped by legacy parsers.
5. **Leverage the 30-Month Rule for Enterprise SLA**: For government, healthcare, or corporate applications with conservative upgrade cycles, require features to be "Widely Available" before removing polyfills.
6. **Include CSS Reset and Typography in Widely Available Core**: Ensure foundational layout, fonts, and box-sizing utilize only universal, widely available properties.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Waiting for "CSS4" before adopting modern features | Misunderstanding modular specification structure | Missing out on modern native features already in all browsers. | Adopt individual modules according to their Baseline status. |
| Using CSS Anchor Positioning without fallbacks | Ignoring Limited Availability status | Broken, unpositioned popovers on Firefox and older Safari browsers. | Wrap in `@supports` or use Floating UI / Popper fallback. |
| Grouping unsupported selectors in a comma list | CSS parsing rule: invalid selector invalidates entire rule | `.card, :has(.invalid) { color: red; }` fails completely in older browsers. | Use `@supports selector()` or separate selectors into distinct rules. |
| Assuming "Newly Available" means 100% user coverage | Forgetting enterprise and mobile update lag | Users on older devices or iOS versions experience broken interfaces. | Check your target analytics against CanIUse statistics. |
| Writing duplicate `@supports` blocks everywhere | Scattering checks across files | Fragmented, unmaintainable stylesheets with conflicting logic. | Group progressive enhancements at component boundaries or design token level. |
| Using `-webkit-` prefixes in modern stylesheets | Copy-pasting 10-year-old StackOverflow snippets | Code clutter; potential discrepancies between prefix and spec behavior. | Author standard CSS; let Lightning CSS handle target compatibility. |

---

## 8. Performance, Accessibility & Browser Support

### Impact of `@supports` on Parse Time
Feature queries (`@supports`) are evaluated directly by the browser's C++ parser during the initial tokenization and CSSOM construction phase. Because the engine already knows its internal property dispatch table, `@supports` evaluation takes less than **0.01 milliseconds** and introduces zero measurable runtime layout overhead.

### Accessibility Safeguards Across Spec Levels
- The CSSWG requires every new CSS module to complete an **Accessibility Horizontal Review** before advancing to Candidate Recommendation.
- Features like `text-wrap: balance`, `accent-color`, and `forced-colors` are designed specifically to elevate accessibility across diverse screen sizes and assistive hardware.

---

## 9. Comparison / When to Use What

| Baseline Tier | Criteria | Safe for Production? | Action Required |
|:---|:---|:---|:---|
| **Widely Available** | Interoperable across Chrome, Edge, Firefox, Safari for **30+ months** | **Yes (100% Safe)** | Use directly without polyfills or fallbacks. |
| **Newly Available** | Interoperable across all 4 core engines within the last 1–29 months | **Yes (General Web)** | Verify analytics; use progressive enhancement for legacy users. |
| **Limited Availability** | Supported in 1, 2, or 3 engines; missing from at least one core browser | **No (Not without fallback)** | Mandatory `@supports` gate with battle-tested fallback. |
| **Editor's Draft / Flag** | Experimental stage; disabled by default in browsers | **Development Only** | Test locally behind flags; never ship to production users. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is there no such thing as "CSS4", and how does the CSS standardization process actually work today?</strong></summary>

After CSS 2.1, the CSS Working Group recognized that bundling the entire language into single monolithic versions (like CSS1, CSS2) slowed web evolution because complex features held back simple ones. In 1999–2011, CSS was permanently reorganized into independent modular specifications (CSS Color, CSS Grid, CSS Flexbox, CSS Selectors), each incrementing its own version number (e.g., Selectors Level 4, Grid Level 2). The term "CSS4" is merely marketing shorthand used by developers to refer to modern features collectively.
</details>

<details>
<summary><strong>2. What is the difference between "Newly Available" and "Widely Available" in Web Platform Baseline?</strong></summary>

**Newly Available** means a feature has achieved cross-browser interoperability across the latest stable releases of all four major browser engines (Chrome, Edge, Firefox, Safari). This starts the clock on the feature. **Widely Available** means the feature has maintained interoperability across all four engines for at least **30 months**, providing enough time for users on corporate networks, older smartphones, and extended-support releases to update.
</details>

<details>
<summary><strong>3. Why does writing `.box, :has(.child) { ... }` break entirely in browsers that do not support `:has()`?</strong></summary>

According to the W3C CSS parsing specification, if a browser encounters an unrecognized or invalid selector within a comma-separated selector list, **the entire selector rule must be discarded**. In older browsers without `:has()` support, the browser cannot parse `:has(.child)` and therefore discards the entire block, causing even `.box` to fail. To prevent this, developers must write separate rules or use `@supports selector(:has(.child))`.
</details>

<details>
<summary><strong>4. How does the Web Platform Tests (WPT) project improve cross-browser consistency?</strong></summary>

WPT is a shared, open-source test suite containing hundreds of thousands of automated tests for web platform specifications. Browser engines (Chromium, Gecko, WebKit) integrate WPT directly into their continuous integration (CI) pipelines. Whenever an engineer submits code to Blink or WebKit, WPT tests verify that the rendering output matches the exact mathematical specification, preventing proprietary divergence between browsers.
</details>

<details>
<summary><strong>5. How does `@supports selector(...)` differ from a standard `@supports (...)` condition?</strong></summary>

Standard `@supports (property: value)` checks whether the browser's CSS parser recognizes and executes a specific property-value pair (e.g., `display: grid`). In contrast, `@supports selector(...)` checks whether the browser supports a modern selector syntax (such as `:has()`, `:focus-visible`, or `::part()`), allowing safe progressive enhancement for complex selector logic.
</details>

<details>
<summary><strong>6. What is a CSS Snapshot?</strong></summary>

A CSS Snapshot is an official document published periodically (e.g., CSS Snapshot 2023) by the W3C CSS Working Group. It acts as an authoritative index collecting all CSS modular specifications that are considered stable, complete, and battle-tested for implementation by browser vendors during that timeframe.
</details>

<details>
<summary><strong>7. How do modern build tools like Lightning CSS use Baseline information?</strong></summary>

Lightning CSS uses browser target queries (derived from Browserslist and CanIUse data). When an authored stylesheet uses a feature that has not yet reached "Widely Available" status (or falls outside the user's configured target browser matrix), the compiler automatically downlevels the syntax (e.g., flattening native nesting or converting OKLCH to sRGB), ensuring optimal output without developer guesswork.
</details>

<details>
<summary><strong>8. What was the Interop initiative, and why was it significant for CSS developers?</strong></summary>

The Interop initiative (Interop 2022, 2023, 2024, 2025) is an annual joint alliance between Google, Apple, Microsoft, Mozilla, and web consulting firms. Rather than competing blindly, the vendors agree on a focused list of known web platform compatibility pain points (such as CSS Subgrid, Container Queries, and `:has()`) and commit engineering resources to achieve 100% interoperability across all engines within that calendar year.
</details>

---

## 11. Practice Exercises

### Exercise 1: Write a Progressive Feature Query for CSS Subgrid (Easy)
**Task**: Write a CSS snippet for a `.grid-item` that uses a standard 3-column grid fallback, but utilizes `grid-template-columns: subgrid` when the engine supports it.

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Fallback */
.grid-item {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
}

/* Progressive Enhancement */
@supports (grid-template-columns: subgrid) {
  .grid-item {
    grid-template-columns: subgrid;
  }
}
```
*Explanation*: Browsers supporting subgrid will override the static repeat columns with the synchronized parent tracks.
</details>

---

### Exercise 2: Safe Selector Feature Query for `:has()` (Easy)
**Task**: Wrap a card styling rule using the relational `:has()` selector inside a safe feature query so it never breaks legacy CSS parsers.

<details>
<summary><strong>View Solution</strong></summary>

```css
@supports selector(:has(*)) {
  .card:has(.badge--featured) {
    border: 2px solid #3b82f6;
  }
}
```
*Explanation*: Using `@supports selector(:has(*))` isolates the relational selector, ensuring legacy parsers never attempt to parse the inner block.
</details>

---

### Exercise 3: Detect View Transitions API Support in CSS (Medium)
**Task**: Write the CSS declaration that configures `view-transition-name` only if the browser engine supports modern CSS View Transitions.

<details>
<summary><strong>View Solution</strong></summary>

```css
@supports (view-transition-name: main-header) {
  .site-header {
    view-transition-name: main-header;
  }
}
```
*Explanation*: The `@supports` block queries the `view-transition-name` property, preventing parsing errors on engines that do not yet support Level 1 View Transitions.
</details>

---

### Exercise 4: Negative Feature Query Fallback (Medium)
**Task**: An engineer wants to apply a JavaScript polyfill or specific fallback style only if the browser does *not* support CSS Container Queries (`container-type`). Write the negative `@supports` query.

<details>
<summary><strong>View Solution</strong></summary>

```css
@supports not (container-type: inline-size) {
  .card-container {
    /* Fallback layout for legacy browsers lacking Container Queries */
    width: 100%;
    max-width: 600px;
    margin: 0 auto;
  }
}
```
*Explanation*: The `not` operator negates the test, applying declarations exclusively to older engines.
</details>

---

### Exercise 5: Test Multiple Feature Queries with `and` / `or` (Hard)
**Task**: Write an `@supports` block that applies advanced responsive image styles only if the browser supports both CSS aspect-ratio (`aspect-ratio: 16/9`) AND modern object-fit (`object-fit: cover`).

<details>
<summary><strong>View Solution</strong></summary>

```css
@supports (aspect-ratio: 16 / 9) and (object-fit: cover) {
  .responsive-banner {
    width: 100%;
    aspect-ratio: 16 / 9;
    object-fit: cover;
  }
}
```
*Explanation*: Both conditions must evaluate to true for the block to execute.
</details>

---

### Exercise 6: Programmatic Baseline Feature Detection via JavaScript (Hard)
**Task**: Write a JavaScript utility function `isBaselineFeatureSupported(property, value)` that returns a boolean indicating whether the current browser engine supports that CSS feature using the DOM `CSS.supports()` API.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Detects whether a CSS property/value pair is supported by the client engine.
 * @param {string} property - CSS property name (e.g. 'display')
 * @param {string} value - CSS value to test (e.g. 'subgrid')
 * @returns {boolean}
 */
function isBaselineFeatureSupported(property, value) {
  if (typeof window !== 'undefined' && window.CSS && typeof window.CSS.supports === 'function') {
    return window.CSS.supports(property, value);
  }
  return false;
}

// Example usage:
if (isBaselineFeatureSupported('display', 'subgrid')) {
  console.log('Subgrid is active on this browser.');
} else {
  console.log('Subgrid missing; loading fallback layout.');
}
```
*Explanation*: `window.CSS.supports()` provides a direct JavaScript binding to the browser engine's internal style support parser.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS MODULES & BASELINE CHEAT SHEET                                                              |
+--------------------------+-------------------------------------+--------------------------------+
| Concept                  | Definition                          | Status / Strategy              |
+--------------------------+-------------------------------------+--------------------------------+
| CSS4 Myth                | CSS is modular; no monolithic v4    | Adopt individual modules       |
| Web Platform Baseline    | Interop standard across 4 engines   | Governed by WebDX group        |
| Newly Available          | Interoperable in Chrome,Safari,FF,Ed| Ready for modern production    |
| Widely Available         | Interoperable for 30+ months        | Safe for all enterprise apps   |
| Limited Availability     | Missing in 1+ major browser         | Requires @supports fallback    |
| @supports (prop: val)    | Property/value feature detection    | Native CSS condition           |
| @supports selector(...)  | Selector syntax feature detection   | Prevents list invalidation     |
| CSS.supports(prop, val)  | JavaScript runtime API check        | Dynamic JS enhancement         |
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [W3C CSS Working Group Official Current Work](https://www.w3.org/Style/CSS/current-work)
- [Web Platform Baseline (web.dev)](https://web.dev/baseline)
- [MDN Web Docs: CSS Conditional Rules (@supports)](https://developer.mozilla.org/en-US/docs/Web/CSS/@supports)
- [Web Platform Tests Dashboard (wpt.fyi)](https://wpt.fyi/)
- [Can I Use: Browser Support Tables](https://caniuse.com/)

---

[Previous: 05 - Tooling: Stylelint, Prettier & Lightning CSS](05-tooling-stylelint-prettier-lightningcss.md) | [Section Index](README.md) | [Next: Module 01 - Fundamentals & Core Syntax (../01-fundamentals/README.md)](../01-fundamentals/README.md)
