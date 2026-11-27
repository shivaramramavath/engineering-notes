# 14. Dead CSS and Critical CSS

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Web Performance Optimization / Core Web Vitals Standards |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Chrome DevTools Coverage Tab, Lighthouse 11, PurgeCSS 6, Critters |
| **Prerequisites** | `01-fundamentals/01-how-css-works-engine-pipeline.md`, `10-architecture-and-methodologies/01-css-architecture-principles.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
CSS is an inherently **render-blocking resource**. By default, when a browser encounters `<link rel="stylesheet">` in an HTML document, it halts DOM rendering until the stylesheet is completely downloaded, parsed, and synthesized into the CSSOM (CSS Object Model).

In enterprise applications that have evolved over years, stylesheets routinely grow to 300 KB – 1 MB+, containing thousands of selectors for obsolete features, deleted templates, or components only present on other pages. Shipping this "Dead CSS" produces severe performance degradation:
- Delayed **First Contentful Paint (FCP)** and **Largest Contentful Paint (LCP)** due to network blocking.
- Excessive browser memory usage and CPU time spent during CSSOM construction and selector matching.
- Regressions in **Interaction to Next Paint (INP)** caused by main-thread contention during style recalculations.

### The Solution
Two complementary performance patterns eliminate this bottleneck:
1. **Dead CSS Elimination (Tree-Shaking / Purging)**: Statically or dynamically scanning template files to strip unused selectors from production bundles.
2. **Critical CSS Extraction**: Splitting stylesheets into:
   - **Critical Path CSS**: The minimal styles required to render the initial above-the-fold viewport, inlined directly into `<style>` tags in the HTML `<head>`.
   - **Non-Critical CSS**: The remaining styles, deferred and loaded asynchronously without blocking initial render.

---

## 2. Core Concepts

### Render-Blocking Timeline vs. Critical CSS Path

```
TRADITIONAL RENDER-BLOCKING PIPELINE:
HTML Request ──► Parse HTML ──► [BLOCKED: Fetch styles.css (250KB)] ──► Parse CSSOM ──► First Paint
                                                                                    (e.g., 2.8s)

CRITICAL CSS PIPELINE:
HTML Request ──► Parse HTML + Inlined Critical CSS (<14KB) ──────────────────────────► First Paint (0.6s!)
                       │
                       ▼ (Background non-blocking)
                 Asynchronous fetch of full styles.css ──► Cache for route navigation
```

### 1. Above-the-Fold vs. Below-the-Fold
- **Above-the-Fold**: The visible portion of a webpage when the user first loads the screen (without scrolling). Includes header navigation, hero banners, and primary headline typography.
- **Below-the-Fold**: Everything requiring scrolling: footer links, comments, modals, image galleries, and tab panels.

### 2. The 14 KB TCP Initial Window Limit
TCP connections initiate data transmission using **Slow-Start algorithm**, where the initial congestion window (cwnd) typically allows **10 TCP packets (~14.6 KB)** to be transmitted in the first round-trip time (RTT). Keeping inlined HTML plus Critical CSS under 14 KB ensures the initial viewport renders in a single network round-trip.

### 3. Safelisting Dynamic Selectors
Build-time tree-shaking tools (like PurgeCSS) evaluate source files via static text patterns. Selectors generated dynamically via JavaScript (e.g. `is-open`, `status-alert`, `theme-dark`) must be explicitly safelisted to prevent accidental removal.

---

## 3. How It Works Internally

### Chrome DevTools Coverage Profiling
DevTools measures the ratio of executed style rules to total downloaded rules:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        DEVTOOLS COVERAGE PANEL                         │
├────────────────────────────────┬──────────┬─────────────┬──────────────┤
│ URL                            │ Total    │ Unused      │ Usage Visual │
├────────────────────────────────┼──────────┼─────────────┼──────────────┤
│ /assets/vendor.bundle.css      │ 284 KB   │ 241 KB (85%)│ ████████░░░░ │
│ /assets/app.css                │  68 KB   │  22 KB (32%)│ ███░░░░░░░░░ │
│ Inlined <style> (Critical)     │   8 KB   │   0 KB (0%) │ ░░░░░░░░░░░░ │
└────────────────────────────────┴──────────┴─────────────┴──────────────┘
```

1. **V8 & Blink Profiler**: When the page renders, the browser tracks every CSS rule attached to matched DOM elements.
2. **Red vs. Blue Bars**: In DevTools, bytes highlighted in red represent unused selectors during the recorded page session; blue represents applied selectors.

---

## 4. Syntax & Usage

### 1. Asynchronous Non-Critical CSS Loading Pattern
The standard cross-browser pattern for loading deferred stylesheets without blocking rendering:

```html
<!-- 1. Preload hint for high-priority network discovery -->
<link rel="preload" href="/css/deferred.css" as="style">

<!-- 2. Non-blocking load: switch media from print to all upon arrival -->
<link rel="stylesheet" href="/css/deferred.css" media="print" onload="this.media='all'">

<!-- 3. Fallback for users with JavaScript disabled -->
<noscript>
  <link rel="stylesheet" href="/css/deferred.css">
</noscript>
```

### 2. PurgeCSS Configuration (`purgecss.config.js`)
Configuring static purging across modern frontend source trees:

```javascript
module.exports = {
  content: [
    './src/**/*.html',
    './src/**/*.jsx',
    './src/**/*.tsx',
    './src/**/*.vue'
  ],
  css: ['./src/css/input.css'],
  output: './dist/css/purged.css',
  safelist: {
    standard: [
      /^is-/,
      /^has-/,
      'active',
      'visible'
    ],
    deep: [/^modal-/],
    greedy: [/tooltip$/]
  },
  defaultExtractor: content => content.match(/[\w-/:]+(?<!:)/g) || []
};
```

---

## 5. Practical Examples

### Example 1: Critical CSS Inlining in HTML `<head>`
Demonstrating the separation of above-the-fold inline critical styles from deferred site-wide styling.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>High Performance Commerce</title>

  <!-- CRITICAL CSS: Inlined directly into head (< 10 KB) -->
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; }
    body { font-family: system-ui, -apple-system, sans-serif; line-height: 1.5; color: #111; }
    .site-header { display: flex; align-items: center; justify-content: space-between; padding: 1rem 1.5rem; background: #fff; border-bottom: 1px solid #e5e7eb; }
    .logo { font-size: 1.25rem; font-weight: 700; text-decoration: none; color: #111; }
    .hero { min-block-size: 60vh; display: flex; flex-direction: column; justify-content: center; align-items: center; text-align: center; padding: 2rem; background: #f8fafc; }
    .hero-title { font-size: clamp(2rem, 5vw, 3.5rem); font-weight: 800; line-height: 1.1; margin-bottom: 1rem; }
    .btn-cta { display: inline-flex; padding: 0.75rem 1.5rem; background: #2563eb; color: #fff; text-decoration: none; border-radius: 0.5rem; font-weight: 600; }
  </style>

  <!-- NON-CRITICAL CSS: Deferred asynchronously -->
  <link rel="preload" href="/css/site-deferred.css" as="style">
  <link rel="stylesheet" href="/css/site-deferred.css" media="print" onload="this.media='all'">
  <noscript><link rel="stylesheet" href="/css/site-deferred.css"></noscript>
</head>
<body>
  <!-- Above the fold content renders immediately with zero network delay -->
  <header class="site-header">
    <a href="/" class="logo">ApexEngine</a>
  </header>
  <main>
    <section class="hero">
      <h1 class="hero-title">Next Generation Edge Deployment</h1>
      <a href="#signup" class="btn-cta">Start Free Trial</a>
    </section>
  </main>
</body>
</html>
```

### Example 2: PostCSS with PurgeCSS in Vite Build
Integrating automatic dead CSS elimination into a modern Vite build pipeline:

```javascript
// postcss.config.js
const purgecss = require('@fullhuman/postcss-purgecss')({
  content: ['./index.html', './src/**/*.{vue,js,ts,jsx,tsx}'],
  safelist: {
    standard: [/^router-link-/, /^theme-/],
    deep: [/swiper-/]
  },
  defaultExtractor: (content) => {
    // Broad regex extractor covering Tailwind/BEM selectors
    const broadMatches = content.match(/[^<>"'`\s]*[^<>"'`\s:]/g) || [];
    return broadMatches;
  }
});

module.exports = {
  plugins: [
    require('autoprefixer'),
    ...(process.env.NODE_ENV === 'production' ? [purgecss] : [])
  ]
};
```

### Example 3: Automated Inlining via Critters in Webpack / Next.js
Critters parses the rendered HTML output during SSR, extracts matched CSS rules, inlines them, and modifies external links to load asynchronously automatically:

```javascript
// next.config.js
module.exports = {
  experimental: {
    // Automatically inlines Critical CSS and defers external files
    optimizeCss: true
  }
};
```

### Example 4: Auditing Unused CSS via Node Script & Puppeteer
Programmatically measure CSS coverage in automated performance regression suites:

```javascript
// audit-coverage.js
const puppeteer = require('puppeteer');

(async () => {
  const browser = await puppeteer.launch({ headless: 'new' });
  const page = await browser.newPage();

  // Start style coverage tracking
  await page.coverage.startCSSCoverage();
  await page.goto('http://localhost:3000', { waitUntil: 'networkidle0' });
  const cssCoverage = await page.coverage.stopCSSCoverage();

  let totalBytes = 0;
  let usedBytes = 0;

  for (const entry of cssCoverage) {
    totalBytes += entry.text.length;
    let totalEntryUsed = 0;
    for (const range of entry.ranges) {
      totalEntryUsed += range.end - range.start;
    }
    usedBytes += totalEntryUsed;
    const unusedPct = (((entry.text.length - totalEntryUsed) / entry.text.length) * 100).toFixed(1);
    console.log(`[Coverage] ${entry.url}: ${unusedPct}% unused (${entry.text.length} bytes)`);
  }

  const overallUnused = (((totalBytes - usedBytes) / totalBytes) * 100).toFixed(1);
  console.log(`\nOverall Unused CSS: ${overallUnused}% of ${(totalBytes / 1024).toFixed(1)} KB`);

  await browser.close();
})();
```

### Example 5: Resolving Flash of Unstyled Content (FOUC)
When Critical CSS is missing key typography or layout values, elements flash or shift layout when deferred CSS arrives:

```css
/* Critical CSS must explicitly include layout containers and dimensions */
.product-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 1.5rem;
  /* Prevent layout collapse before deferred images/data hydrate */
  min-height: 400px;
}

.hero-banner {
  /* Reserve vertical height matching expected render to prevent CLS */
  aspect-ratio: 16 / 9;
  background-color: #f1f5f9; /* Skeleton placeholder tone */
}
```

---

## 6. Best Practices

1. **Keep Critical CSS Under 14 KB (Gzipped)**: Ensure inlined HTML plus critical CSS fits inside the first TCP round-trip transmission window.
2. **Always Include Dimension Constraints in Critical Styles**: Prevent Cumulative Layout Shift (CLS) by giving images and hero containers explicit `aspect-ratio` or `min-height` in critical CSS.
3. **Safelist Dynamic Selectors Extensively**: Whenever UI state transitions are toggled via JS (`.is-active`, `.modal-open`), ensure your purge tool's safelist regex accounts for them.
4. **Leverage HTTP Caching for Deferred CSS**: Inlined CSS cannot be cached independently of the HTML page. Deferred external CSS files should be hashed (`app.a8f9d3.css`) and cached with `Cache-Control: public, max-age=31536000, immutable`.
5. **Re-evaluate Critical CSS per Route**: Do not inline the homepage's Critical CSS on an article or dashboard page; route-based Critical CSS extraction ensures optimal initial paint for each page type.
6. **Integrate DevTools Coverage Checks in PR Gates**: Set budgets (e.g. max 20% unused CSS on primary landing pages) to catch accidental bundle leaks before production deploy.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Purging CSS without scanning dynamic JS classes** | UI components break at runtime because `.is-open` or `.has-error` were stripped. | Add dynamic class prefixes to PurgeCSS `safelist.standard`. |
| **Inlining huge stylesheets (50 KB+) into every HTML page** | Inflates HTML document size, defeats browser caching, and wastes bandwidth. | Inline only minimal above-the-fold rules (<14 KB); defer the rest. |
| **Triggering Flash of Unstyled Content (FOUC)** | Page renders with raw text then jumps violently when deferred CSS loads. | Ensure critical typography, container grids, and layout bounds are inlined. |
| **Using `rel="preload"` without `onload` or fallback** | Stylesheet downloads with high priority but is never applied to the DOM. | Use `media="print" onload="this.media='all'"` paired with a `<noscript>` tag. |
| **Extracting Critical CSS only for desktop viewports** | Mobile users experience broken layouts or layout shifts if mobile critical styles were omitted. | Run critical extraction across both desktop (1440px) and mobile (375px) viewports. |
| **Purging third-party web component styles inside Shadow DOM** | Build scanners fail to inspect encapsulated shadow DOM templates. | Exclude shadow DOM component libraries or safelist web component selectors. |

---

## 8. Performance, Accessibility & Browser Support

### Core Web Vitals Impact

```
┌───────────────────────────────┬───────────────────────────────┬───────────────────────────────┐
│ Metric                        │ Without Critical / Dead CSS   │ With Critical CSS & Purging   │
├───────────────────────────────┼───────────────────────────────┼───────────────────────────────┤
│ First Contentful Paint (FCP)  │ 2.4s (Network blocking)       │ 0.5s (Instant head render)    │
│ Largest Contentful Paint (LCP)│ 3.8s (Delayed by CSSOM)       │ 1.2s (Rapid text/hero paint)  │
│ Cumulative Layout Shift (CLS) │ 0.28 (Layout flash on load)   │ 0.01 (Reserved dimensions)    │
│ Interaction to Next Paint(INP)│ High (Large style recalc)     │ Low (Lean CSSOM trees)        │
└───────────────────────────────┴───────────────────────────────┴───────────────────────────────┘
```

### Browser Compatibility
- `<link rel="preload">` is supported in all modern browsers (Chrome 50+, Safari 11.1+, Firefox 85+).
- The `media="print" onload="this.media='all'"` trick requires zero polyfills and functions universally.

---

## 9. Comparison / When to Use What

| Technique | Implementation Complexity | Cacheability | Best Suited For |
| :--- | :--- | :--- | :--- |
| **PurgeCSS / Dead CSS Removal** | Low (Build-time plugin) | Fully cacheable via standard CSS files | Any project using large component libraries or utility CSS |
| **Critical CSS Inlining** | Medium-High (Requires SSR/tooling) | Inlined styles re-downloaded with HTML | Content-heavy sites, landing pages, e-commerce stores |
| **CSS Modules / Scoped CSS** | Low (Integrated in modern frameworks) | Bundled into route-specific chunks | Single Page Applications (SPAs) with localized styles |
| **Tailwind JIT On-Demand Engine**| Zero (Native engine capability) | Highly cacheable single file | Modern web applications starting from scratch |

---

## 10. Interview Questions

### Q1: Why is CSS considered a render-blocking resource, and how does the browser handle external stylesheets in the `<head>`?
<details>
<summary>Answer</summary>
CSS is render-blocking because the browser cannot construct the **Render Tree** (which determines what pixels to draw) without both the DOM (HTML nodes) and the CSSOM (computed styles). If the browser rendered HTML before downloading and parsing CSS, every element would display with default user-agent styles and then violently repaint when the stylesheet arrived—causing an unacceptable Flash of Unstyled Content (FOUC).

Consequently, when the HTML parser encounters an external `<link rel="stylesheet">`, it initiates a network fetch and halts the painting phase until the file is received and the CSSOM is calculated.
</details>

### Q2: Explain the mechanics of the asynchronous CSS loading trick: `media="print" onload="this.media='all'"`.
<details>
<summary>Answer</summary>
1. **`media="print"`**: When the browser parses `<link rel="stylesheet" href="style.css" media="print">`, it recognizes that the styles apply only to print media, not screen rendering. Therefore, the browser does not block rendering while fetching the file; it downloads it at a lower network priority in the background.
2. **`onload="this.media='all'"`**: When the network request completes, the `onload` event fires and executes a single line of JavaScript that switches the `media` attribute to `all` (or `screen`). The browser then applies the newly available styles to the active viewport immediately without having blocked the initial render.
3. **`<noscript>`**: Ensures users with JavaScript disabled still receive and apply the stylesheet normally.
</details>

### Q3: What is the "14 KB TCP threshold," and why does it dictate Critical CSS budgets?
<details>
<summary>Answer</summary>
When a client initiates an HTTPS connection to a server, the TCP handshake begins with **TCP Slow Start**. The Initial Congestion Window (InitCwnd) is typically 10 TCP segments. In standard ethernet networks (MTU 1500 bytes), 10 segments equate to approximately **14.6 KB** of data.

If the HTML response—including all inlined critical CSS, critical JavaScript, and structural markup—is under ~14 KB compressed, the browser can receive and render the initial page in a single TCP network round trip. Exceeding 14 KB requires an additional round trip to acknowledge receipt and expand the congestion window, increasing FCP by an entire network RTT (often 50–150ms on mobile).
</details>

### Q4: How does PurgeCSS determine which CSS classes to keep or remove, and what are its inherent limitations?
<details>
<summary>Answer</summary>
PurgeCSS uses static regular expressions to scan content source files (`.html`, `.js`, `.tsx`, `.vue`) and extracts every sequence of characters that looks like a word. It then compares this word list against the selectors in the CSS bundle. If a selector doesn't appear anywhere in the content word list, PurgeCSS drops it.

**Limitations**:
1. **Dynamic Class Generation**: It cannot execute JavaScript. Code like `className={`alert-${isError ? 'danger' : 'info'}`}` causes PurgeCSS to miss `.alert-danger` and `.alert-info` because the full string does not exist in the source text.
2. **External Data/CMS Content**: Styles meant for dynamic HTML fetched from a headless CMS or database API will be purged unless source HTML is fed into PurgeCSS or classes are explicitly safelisted.
</details>

### Q5: What is the trade-off of inlining Critical CSS into HTML versus caching an external stylesheet?
<details>
<summary>Answer</summary>
- **Inlining Critical CSS**:
  - *Advantage*: Guarantees instant initial rendering with zero additional HTTP round trips on the first visit (cold cache).
  - *Disadvantage*: Inlined CSS cannot be cached independently of the HTML document. On repeat page navigations (warm cache), the user re-downloads those inlined bytes on every single page load, increasing overall bandwidth.
- **External Stylesheet**:
  - *Advantage*: Fully cacheable via HTTP headers (`Cache-Control: immutable`). Once downloaded, subsequent pages incur zero CSS network overhead.
  - *Disadvantage*: Blocks initial render on first visit while the network connection and file transfer complete.
</details>

### Q6: How do modern build engines like Tailwind CSS v3/v4 approach dead CSS differently than traditional tools like PurgeCSS?
<details>
<summary>Answer</summary>
Traditional PurgeCSS is a **post-processor**: it generates a massive CSS file first (e.g. 3 MB of Bootstrap or utility rules) and then parses the AST to delete unused rules.

Modern Tailwind uses an **on-demand JIT (Just-In-Time) compiler**: it starts with zero CSS rules. As the compiler watches template files, it scans for utility classes on the fly and generates only the exact CSS rules required. It never creates dead CSS in the first place, resulting in drastically faster build times and zero residual dead-code leakage.
</details>

### Q7: How can inlining Critical CSS accidentally cause Cumulative Layout Shift (CLS)?
<details>
<summary>Answer</summary>
If the extracted Critical CSS omits critical structural dimensions (such as `aspect-ratio` on hero images, `min-height` on grid wrappers, or web font fallbacks with matching metric overrides), the browser will render the page initially using default inline heights.

When the full deferred stylesheet subsequently arrives and applies the missing layout rules or web fonts, elements suddenly jump down or resize, triggering a significant CLS penalty. Critical CSS must include all layout bounding boxes for above-the-fold content.
</details>

### Q8: How can you measure the real-world performance impact of dead CSS in Chrome DevTools?
<details>
<summary>Answer</summary>
Open **Chrome DevTools** $\rightarrow$ Press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows) $\rightarrow$ Type "Coverage" and select **Show Coverage** $\rightarrow$ Click the reload icon to record the page load.
DevTools will display every loaded CSS and JS file, its total byte size, and the exact percentage and bytes of unused code. Double-clicking a CSS file in the list opens the Sources panel with line-by-line red (unused) and blue (used) annotations.
</details>

---

## 11. Practice Exercises

### Exercise 1: Asynchronous CSS Loader Helper Function
**Objective**: Author a robust, accessible HTML snippet that preloads an external icon font and stylesheet asynchronously, applies them upon completion, and provides full fallback support for environments with disabled scripting.

<details>
<summary>Solution</summary>

```html
<!-- High-priority preload discovery -->
<link rel="preload" href="/assets/icons.css" as="style">
<link rel="preload" href="/assets/deferred-theme.css" as="style">

<!-- Asynchronous execution via media attribute swap -->
<link rel="stylesheet"
      href="/assets/icons.css"
      media="print"
      onload="this.media='all'; this.onload=null;">

<link rel="stylesheet"
      href="/assets/deferred-theme.css"
      media="print"
      onload="this.media='all'; this.onload=null;">

<!-- No-JS Fallback -->
<noscript>
  <link rel="stylesheet" href="/assets/icons.css">
  <link rel="stylesheet" href="/assets/deferred-theme.css">
</noscript>
```
</details>

### Exercise 2: Safelisting Regex Pattern for Dynamic BEM Modifiers
**Objective**: Write a PurgeCSS safelist configuration block that preserves all BEM modifier classes matching `--is-*`, `--active`, and stateful data attributes dynamically injected by React.

<details>
<summary>Solution</summary>

```javascript
// purgecss-safelist.js
module.exports = {
  safelist: {
    standard: [
      /^is-/,
      /^has-/,
      /--active$/,
      /--selected$/,
      /--open$/
    ],
    deep: [
      /toast-notification/,
      /modal-backdrop/
    ],
    greedy: [
      /data-theme$/
    ]
  }
};
```
</details>

---

## 12. Summary / Cheat Sheet

- **Render-Blocking Nature**: External `<link rel="stylesheet">` blocks HTML parsing and rendering until downloaded and parsed.
- **The 14 KB Rule**: Keep inlined HTML + Critical CSS within 14.6 KB to render the initial viewport in a single TCP round-trip.
- **Critical Path Strategy**:
  1. Extract above-the-fold CSS.
  2. Inline Critical CSS into `<style>` in `<head>`.
  3. Defer remaining CSS via `<link rel="preload" as="style" onload="this.media='all'">`.
- **Dead CSS Elimination**:
  - PurgeCSS: Post-build static scanner.
  - JIT Engines (Tailwind): On-demand generation that never creates dead CSS.
- **Chrome DevTools Coverage**: Check byte usage and unused selector percentage before shipping to production.
- **Preventing CLS in Critical CSS**: Always define `aspect-ratio`, container `min-height`, and placeholder backgrounds for above-the-fold media.

---

## 13. Further Reading
- [web.dev: Defer non-critical CSS](https://web.dev/defer-non-critical-css/)
- [web.dev: Extract Critical CSS](https://web.dev/extract-critical-css/)
- [PurgeCSS Official Documentation](https://purgecss.com/)
- [Critters: Fast Critical CSS Inliner](https://github.com/GoogleChromeLabs/critters)
- [Ilya Grigorik: High Performance Browser Networking (HTTP & TCP Basics)](https://hpbn.co/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [13. Managing Specificity at Scale](13-managing-specificity-at-scale.md) | [15. Component Styling Patterns](15-component-styling-patterns.md) |
