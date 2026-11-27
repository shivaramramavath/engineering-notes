# 02. Render Blocking and Critical CSS

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | HTML Living Standard / W3C Resource Loading & Web Performance Standards |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Chrome DevTools Performance & Network Panels, Lighthouse 11, WebPageTest |
| **Prerequisites** | `00-setup/02-how-the-browser-renders-css.md`, `12-performance-and-debugging/01-css-performance-myths-and-realities.md` |

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
When a web browser encounters an external stylesheet reference:
```html
<link rel="stylesheet" href="/assets/app.css">
```
it marks that resource as **render-blocking**. By architectural design, the browser parser will continue downloading HTML tokens in the background, but **it will not paint a single pixel to the screen** until that CSS file is completely downloaded over the network, parsed into memory, and combined with the DOM into the CSSOM (CSS Object Model).

On slow cellular connections (such as 3G or congested 4G) or high-latency mobile networks, fetching a 150 KB external stylesheet can stall initial painting for **1,500ms to 4,000ms**, leaving the user staring at a blank white screen. This directly crushes Google Core Web Vitals metrics:
- **First Contentful Paint (FCP)** is severely delayed.
- **Largest Contentful Paint (LCP)** fails the 2.5-second threshold.
- User bounce rates surge exponentially with every 500ms of blank delay.

### The Solution
The **Critical CSS Strategy** splits stylesheet delivery into two distinct operational paths:
1. **Critical Path CSS**: The minimal subset of CSS required to render the initial above-the-fold viewport (the top header, navigation, hero banner, and primary typography), inlined directly inside a `<style>` block in the HTML `<head>`.
2. **Deferred Non-Critical CSS**: The remaining 90%+ of the stylesheet (modals, tabs, below-the-fold content, footers), loaded asynchronously in the background without blocking the render pipeline.

---

## 2. Core Concepts

### 1. Render-Blocking vs. Critical Rendering Pipeline

```
TRADITIONAL RENDER-BLOCKING PIPELINE:
HTML Arrives ──► Parse DOM ──► [BLOCKED: Waiting for styles.css (150KB)] ──► Parse CSSOM ──► First Paint
                               ▲                                                          (e.g., 2.8s)
                               └── 3 Network Round Trips! Blank screen for user.

CRITICAL CSS PIPELINE:
HTML Arrives + Inlined Critical CSS (<14KB) ──► Instant DOM + CSSOM ──────────────────────► First Paint
                      │                                                                   (0.5s! FAST)
                      ▼ (Non-blocking background fetch)
              Asynchronous load of full styles.css ──► Cached for subsequent routes
```

### 2. The 14 KB TCP Initial Congestion Window (InitCwnd)
When an HTTPS connection is negotiated between a browser and a server:
- TCP starts data transmission using the **TCP Slow-Start algorithm**.
- The server initializes the connection with an **Initial Congestion Window (`InitCwnd`)**, which in standard modern networking (RFC 6928) is set to **10 TCP segments**.
- With a standard Maximum Transmission Unit (MTU) of 1,500 bytes (minus TCP/IP headers $\approx$ 1,460 bytes per packet), the server can send approximately **14.6 KB of compressed data** in the very first network round-trip time (RTT).
- If your HTML document plus inlined Critical CSS fits within **14.6 KB (gzipped)**, the browser receives the entire initial page payload in a **single network packet burst**, painting the screen in record time.

### 3. Flash of Unstyled Content (FOUC) vs. Flash of Invisible Text (FOIT)
- **FOUC**: Occurs if the browser renders HTML before the appropriate CSS arrives, causing a momentary flash of raw black Times New Roman text and broken layout.
- **FOIT**: Occurs when web fonts are blocked, rendering invisible blank spaces where text should appear.
- Properly engineered Critical CSS prevents FOUC by inlining exact typography, layout dimensions, and background colors for everything in the initial viewport.

---

## 3. How It Works Internally

### Browser Engine CSSOM Synthesis & Render Tree Construction
The browser engine cannot create the **Render Tree** (which dictates what geometric boxes exist and how they are painted) without both the DOM and the CSSOM:

```
HTML Tokens ──► Tokenizer ──► DOM Tree Nodes ────────┐
                                                     ├─► Render Tree ──► Layout ──► Paint
CSS Tokens  ──► Tokenizer ──► CSSOM Rules ───────────┘
```
1. If the browser rendered the DOM before computing the CSSOM, it would paint with default User-Agent styles, and then immediately repaint when author styles arrived, causing violent visual jarring (FOUC) and massive Cumulative Layout Shift (CLS).
2. To prevent this visual corruption, the browser intentionally freezes the rendering pipeline at the start of `<head>` until all synchronous `<link rel="stylesheet">` elements resolve.

---

## 4. Syntax & Usage

### 1. The Standard Asynchronous CSS Loading Pattern
The modern cross-browser standard for loading non-critical stylesheets without blocking page rendering:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>High-Performance Architecture</title>

  <!-- 1. INLINED CRITICAL CSS (< 14 KB gzipped) -->
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; }
    body { font-family: system-ui, -apple-system, sans-serif; line-height: 1.5; color: #0f172a; background: #fff; }
    .site-header { display: flex; justify-content: space-between; align-items: center; padding: 1rem 1.5rem; border-bottom: 1px solid #e2e8f0; }
    .brand-logo { font-size: 1.25rem; font-weight: 700; color: #0f172a; text-decoration: none; }
    .hero { padding: 4rem 1.5rem; text-align: center; max-width: 800px; margin: 0 auto; }
    .hero-title { font-size: clamp(2rem, 4vw, 3.5rem); font-weight: 800; line-height: 1.15; margin-bottom: 1rem; }
    .btn-cta { display: inline-flex; padding: 0.75rem 1.5rem; background: #2563eb; color: #fff; border-radius: 0.5rem; text-decoration: none; font-weight: 600; }
  </style>

  <!-- 2. HIGH-PRIORITY PRELOAD DISCOVERY -->
  <link rel="preload" href="/assets/app-deferred.css" as="style">

  <!-- 3. NON-BLOCKING ASYNCHRONOUS LOAD VIA MEDIA SWAP -->
  <link rel="stylesheet"
        href="/assets/app-deferred.css"
        media="print"
        onload="this.media='all'; this.onload=null;">

  <!-- 4. FALLBACK FOR JAVASCRIPT-DISABLED BROWSERS -->
  <noscript>
    <link rel="stylesheet" href="/assets/app-deferred.css">
  </noscript>
</head>
<body>
  <header class="site-header">
    <a href="/" class="brand-logo">CloudApex</a>
  </header>
  <main class="hero">
    <h1 class="hero-title">Next Generation Edge Deployment</h1>
    <a href="/signup" class="btn-cta">Start 14-Day Free Trial</a>
  </main>
</body>
</html>
```

---

## 5. Practical Examples

### Example 1: Eliminating Cumulative Layout Shift (CLS) in Critical CSS
If inlined Critical CSS omits the dimensions of containers, the arrival of the deferred stylesheet triggers layout reflows and layout jumps:

```css
/* INLINED CRITICAL CSS */
/* Always reserve exact layout boundaries for media and hero containers */
.hero-media-wrap {
  width: 100%;
  max-width: 640px;
  margin: 1.5rem auto;
  /* aspect-ratio reserves the box geometry before the image downloads */
  aspect-ratio: 16 / 9;
  background-color: #f1f5f9; /* Skeleton placeholder tone */
  border-radius: 0.75rem;
  overflow: clip;
}

.hero-media-wrap img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
```

### Example 2: Media-Specific Stylesheet Decoupling
Browsers will NOT block rendering for stylesheets whose `media` attribute does not match the active device:

```html
<!-- Blocks rendering ONLY on screen viewports -->
<link rel="stylesheet" href="/css/screen.css" media="screen">

<!-- Does NOT block initial screen rendering! Downloaded with low priority in background -->
<link rel="stylesheet" href="/css/print.css" media="print">

<!-- Does NOT block mobile screens (<768px wide)! -->
<link rel="stylesheet" href="/css/desktop-enhancements.css" media="(min-width: 768px)">
```

### Example 3: Automated Critical CSS Inlining in Vite / Next.js with Critters
Modern server-side rendering (SSR) frameworks can automate Critical CSS extraction during production builds using **Critters**:

```javascript
// next.config.js
module.exports = {
  experimental: {
    // Automatically inlines above-the-fold CSS and defers external links
    optimizeCss: true
  }
};
```
**Build-Time Operation of Critters**:
1. Renders HTML for each static or SSR route.
2. Runs a headless parser (like JSDOM) to evaluate which CSS rules match the rendered HTML.
3. Inlines the matched critical CSS into a `<style>` block in `<head>`.
4. Converts original `<link rel="stylesheet">` tags into asynchronous preload links.

### Example 4: Preventing Web Font Layout Shifts via Font Fallback Overrides
When using web fonts in Critical CSS, mismatched fallback system fonts cause jarring layout shifts when the web font hydrates:

```css
/* INLINED CRITICAL CSS */
/* 1. Define fallback font with matched optical metrics */
@font-face {
  font-family: 'FallbackInter';
  src: local('Arial');
  /* Adjust system font metrics to match Inter's exact x-height and advance width */
  ascent-override: 90%;
  descent-override: 22%;
  line-gap-override: 0%;
  size-adjust: 107%;
}

body {
  /* Use custom web font with seamless metric-matched fallback */
  font-family: 'Inter', 'FallbackInter', sans-serif;
}
```

### Example 5: Auditing Render-Blocking Resources in Chrome DevTools
Run an audit in Chrome DevTools to verify zero render-blocking stylesheets:
1. Open Chrome DevTools $\rightarrow$ **Network** panel.
2. Check the **Disable cache** checkbox.
3. Reload the page with throttling set to **Fast 3G**.
4. In the Network table, look at the **Priority** column:
   - External stylesheets with standard `<link>` show **Highest** priority and block the initial paint bar (vertical blue/green line).
   - Asynchronous `<link rel="preload">` downloads without blocking the DOMContentLoaded event.

---

## 6. Best Practices

1. **Keep Inlined Critical CSS Under 14 KB (Gzipped)**: Ensure HTML document markup plus inlined CSS fits within the first TCP transmission window (`InitCwnd`), rendering the initial screen in a single network RTT.
2. **Always Pair Preload with `<noscript>`**: When using the `media="print" onload="this.media='all'"` trick, always include a `<noscript><link rel="stylesheet" ...></noscript>` tag for users with disabled JavaScript or search crawlers.
3. **Reserve Element Dimensions to Prevent CLS**: Ensure inlined critical styles define `aspect-ratio` or `min-height` on hero containers, images, and card grids to eliminate layout shifts when deferred CSS loads.
4. **Leverage Media Queries on External Links**: Split large stylesheets into media-specific files (`media="(min-width: 1024px)"`, `media="print"`). Browsers download non-matching media sheets at low priority without blocking initial render.
5. **Set Long-Term HTTP Caching on Deferred Stylesheets**: Because external deferred stylesheets contain the bulk of the CSS, hash their filenames (`app.4f9a1b.css`) and cache them with `Cache-Control: public, max-age=31536000, immutable`.
6. **Automate Extraction via CI/SSR Tooling**: Never attempt to extract Critical CSS manually; integrate tools like Critters, Beastcss, or framework-native plugins in your build pipeline.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Inlining huge stylesheets (>50 KB) on every page** | Destroys HTTP caching, inflates HTML size, and slows down subsequent page navigations. | Inline only critical above-the-fold styles (<14 KB); defer the rest. |
| **Using `rel="preload"` without an `onload` handler** | Stylesheet downloads with high priority but is never applied to the page. | Use `onload="this.media='all'; this.onload=null;"`. |
| **Omitting critical layout containers from inlined styles** | Causes catastrophic layout flash (FOUC) and CLS when deferred CSS suddenly arrives. | Include structural container widths, heights, and grids in critical CSS. |
| **Inlining homepage Critical CSS across all site routes** | Subpages (e.g. blog, checkout) suffer layout flashes because their critical components were missing. | Generate route-specific Critical CSS bundles per page template. |
| **Using `@import` inside external CSS files** | Creates a serial network waterfall; browser must download parent CSS before discovering `@import`. | Bundle files at build time or use multiple parallel `<link>` tags. |
| **Extracting Critical CSS only for desktop screens** | Mobile users receive a desktop layout that shifts violently when mobile deferred CSS arrives. | Extract Critical CSS across both mobile (375px) and desktop (1280px) viewports. |

---

## 8. Performance, Accessibility & Browser Support

### Core Web Vitals Impact Analysis

```
┌───────────────────────────────────┬──────────────────────┬──────────────────────┐
│ Core Web Vitals Metric            │ Unoptimized Link     │ Inlined Critical CSS │
├───────────────────────────────────┼──────────────────────┼──────────────────────┤
│ First Contentful Paint (FCP)      │ 2.4s (High latency)  │ 0.5s (Instant)       │
│ Largest Contentful Paint (LCP)    │ 3.8s (Delayed CSSOM) │ 1.1s (Instant Hero)  │
│ Cumulative Layout Shift (CLS)     │ 0.24 (FOUC jump)     │ 0.00 (Zero Shift)    │
│ First Meaningful Paint (FMP)      │ 2.6s                 │ 0.6s                 │
└───────────────────────────────────┴──────────────────────┴──────────────────────┘
```

### Universal Browser Support
- The `<link rel="stylesheet" media="print" onload="this.media='all'">` pattern functions across all modern and legacy browsers.
- `<link rel="preload" as="style">` is supported in all evergreen browsers (Chrome 50+, Safari 11.1+, Firefox 85+).

---

## 9. Comparison / When to Use What

| Delivery Strategy | First Visit (Cold Cache) | Repeat Visit (Warm Cache) | Complexity | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Standard `<link>`** | Slow (Render-blocked) | Instant (Cached locally) | Zero | Simple intranet tools, low-traffic sites |
| **Critical Inlining + Deferred Link**| Blazing fast (Single RTT) | Fast (Cached deferred CSS) | Medium | E-commerce, marketing sites, news portals |
| **100% Inlined CSS** | Fast (Zero external files) | Slow (No cache reuse) | Low | Single-page standalone landing pages |
| **Tailwind JIT Minified File** | Fast (~10KB total payload)| Fast (Tiny cached file) | Low | Modern SPAs with tiny total CSS payloads |

---

## 10. Interview Questions

### Q1: Why is CSS fundamentally classified as a "render-blocking" resource in browser engine architectures?
<details>
<summary>Answer</summary>
The browser engine cannot construct the **Render Tree** (which dictates what pixels to draw) without combining the DOM Tree (HTML structure) with the CSSOM Tree (computed styles).
If the browser rendered HTML before downloading and parsing external stylesheets, elements would briefly appear with default User-Agent styles (raw black Times New Roman text, unstyled links, full-width blocks) and then violently repaint when the stylesheet arrived—causing an unacceptable Flash of Unstyled Content (FOUC) and severe Cumulative Layout Shift (CLS).

To prevent this visual corruption, browser engines intentionally pause the painting pipeline whenever a synchronous `<link rel="stylesheet">` is encountered in `<head>`, blocking all visual rendering until the CSSOM is synthesized.
</details>

### Q2: What is the "14 KB TCP threshold," and why does it dictate Critical CSS budgets?
<details>
<summary>Answer</summary>
When a client initiates an HTTPS connection to a web server, the TCP handshake begins with **TCP Slow-Start** (RFC 6928). The Initial Congestion Window (`InitCwnd`) is standardized at **10 TCP segments**. In standard ethernet networks (MTU of 1,500 bytes minus TCP/IP headers), 10 segments equate to approximately **14.6 KB** of data.

If the entire initial HTML response—including all HTML markup, critical inlined CSS, and metadata—is under ~14.6 KB compressed (gzipped or brotli), the server can transmit the complete payload in a **single network round-trip time (RTT)**. Exceeding 14.6 KB requires an additional network round trip to acknowledge packet receipt and expand the congestion window, delaying initial render by an entire network round-trip (often 50–200ms on mobile networks).
</details>

### Q3: Explain how the asynchronous stylesheet pattern `media="print" onload="this.media='all'"` avoids blocking rendering.
<details>
<summary>Answer</summary>
1. **`media="print"`**: When the browser parser encounters `<link rel="stylesheet" href="style.css" media="print">`, it checks whether the media query matches the current rendering context. Because the active viewport is a screen (not a printer), the browser recognizes that these styles are not required for the initial screen render. Therefore, the engine fetches the stylesheet with low priority in the background without blocking the rendering pipeline.
2. **`onload="this.media='all'"`**: When the background network download finishes, the browser fires the `onload` event. The inline JavaScript switches the `media` attribute from `print` to `all`. The browser immediately parses the newly arrived CSSOM and applies the styles to the DOM smoothly.
3. **`<noscript>`**: Ensures users with JavaScript disabled or search engine crawlers still load the stylesheet synchronously.
</details>

### Q4: Why is using CSS `@import` inside an external stylesheet considered a severe performance anti-pattern?
<details>
<summary>Answer</summary>
When an author writes `@import url("components.css");` inside an external stylesheet:
1. The browser must first discover, download, and parse `main.css`.
2. Only after `main.css` is downloaded and parsed does the browser engine discover that `components.css` is required.
3. The engine initiates a second, sequential network request for `components.css`.
This creates a **serial network waterfall**, forcing multiple sequential round-trips while rendering remains completely blocked. In contrast, parallel `<link rel="stylesheet">` tags in HTML are discovered immediately by the browser's Preload Scanner and downloaded simultaneously.
</details>

### Q5: How can inlining Critical CSS accidentally cause a high Cumulative Layout Shift (CLS) penalty?
<details>
<summary>Answer</summary>
If the extracted Critical CSS omits critical structural layout bounds—such as `aspect-ratio` on hero images, `min-height` on grid containers, or font metric overrides on fallback system fonts—the browser initially renders the page with default inline dimensions.

When the deferred external stylesheet arrives 500ms later, it applies the missing layout rules, flexbox tracks, or font paddings. Elements suddenly expand or jump across the screen, shifting visible content down and triggering a severe CLS score penalty. Critical CSS must include all layout geometry for above-the-fold content.
</details>

### Q6: What is the Preload Scanner, and how does it interact with `<link rel="preload">`?
<details>
<summary>Answer</summary>
While the primary HTML parser is blocked (e.g. executing synchronous scripts or waiting for stylesheets), modern browsers run a lightweight background thread called the **Preload Scanner** (or Speculative Parser).
The Preload Scanner rapidly scans raw HTML tokens ahead of the main parser, searching for external resource URLs (`<img>`, `<link>`, `<script>`).
By declaring `<link rel="preload" href="/css/deferred.css" as="style">`, the Preload Scanner immediately requests the deferred stylesheet with high network priority, ensuring the file is already downloaded and cached in memory by the time the asynchronous loader swaps the media attribute.
</details>

### Q7: When is inlining Critical CSS NOT recommended for a web application?
<details>
<summary>Answer</summary>
1. **Single Page Applications (SPAs) with Small Total CSS**: If your total production stylesheet is already under 15 KB (e.g. using Tailwind CSS with JIT tree-shaking), the entire CSS file already fits within the initial TCP window. Inlining adds architectural build complexity for zero real-world performance gain.
2. **Authenticated Multi-Page Dashboards**: For internal enterprise tools where users log in and navigate across 20+ distinct pages per session, caching a single external stylesheet immutably on the first page load provides superior warm-cache performance across all subsequent route transitions.
</details>

### Q8: How does the HTTP/2 and HTTP/3 multiplexing protocol affect the Critical CSS equation?
<details>
<summary>Answer</summary>
HTTP/2 and HTTP/3 eliminate head-of-line blocking at the transport layer, allowing browsers to download multiple files simultaneously over a single TCP/QUIC connection.
However, HTTP/2 does **not** eliminate the render-blocking nature of CSS! Even with instant multiplexing, external `<link>` tags still require network latency to fetch and process. Inlining Critical CSS remains the only guaranteed method to achieve sub-second FCP on cold-cache mobile connections.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Complete Asynchronous CSS Loader Template
**Objective**: Author an HTML `<head>` snippet that inlines critical layout styles, asynchronously preloads and loads a deferred stylesheet, and provides accessible fallbacks.

<details>
<summary>Solution</summary>

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Resilient Page Load</title>

  <!-- 1. INLINED CRITICAL CSS (< 14 KB) -->
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; }
    body { font-family: system-ui, sans-serif; line-height: 1.5; color: #1e293b; background: #ffffff; }
    .site-nav { display: flex; justify-content: space-between; align-items: center; padding: 1rem 2rem; border-bottom: 1px solid #e2e8f0; }
    .hero { max-width: 960px; margin: 3rem auto; padding: 0 1rem; text-align: center; }
    .hero-h1 { font-size: clamp(2rem, 5vw, 3.25rem); font-weight: 800; line-height: 1.2; margin-bottom: 1rem; }
    .cta-btn { display: inline-block; padding: 0.75rem 1.5rem; background: #0284c7; color: #fff; text-decoration: none; border-radius: 0.5rem; font-weight: 600; }
  </style>

  <!-- 2. HIGH PRIORITY ASYNCHRONOUS RESOURCE DISCOVERY -->
  <link rel="preload" href="/css/app-deferred.css" as="style">

  <!-- 3. NON-BLOCKING ASYNC STYLESHEET APPLICATION -->
  <link rel="stylesheet"
        href="/css/app-deferred.css"
        media="print"
        onload="this.media='all'; this.onload=null;">

  <!-- 4. NO-JS ACCESSIBILITY FALLBACK -->
  <noscript>
    <link rel="stylesheet" href="/css/app-deferred.css">
  </noscript>
</head>
<body>
  <nav class="site-nav">
    <strong>ApexEdge</strong>
  </nav>
  <main class="hero">
    <h1 class="hero-h1">Distributed Cloud Orchestration</h1>
    <a href="/get-started" class="cta-btn">Deploy Cluster</a>
  </main>
</body>
</html>
```
</details>

### Exercise 2: Metric-Matched Font Fallback Definition
**Objective**: Create an `@font-face` declaration for a local fallback font that matches the advance width and x-height of a custom web font to prevent Cumulative Layout Shift.

<details>
<summary>Solution</summary>

```css
/* Metric-matched fallback font */
@font-face {
  font-family: 'Roboto-Fallback';
  src: local('Arial');
  ascent-override: 92%;
  descent-override: 24%;
  line-gap-override: 0%;
  size-adjust: 100%;
}

/* Primary heading typography */
h1 {
  font-family: 'Roboto', 'Roboto-Fallback', sans-serif;
  font-size: 2.5rem;
  line-height: 1.2;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Render-Blocking**: External `<link rel="stylesheet">` halts all pixel painting until downloaded and parsed.
- **The 14 KB Rule**: Keep inlined HTML + Critical CSS within 14.6 KB to render in a single TCP round-trip (`InitCwnd`).
- **Asynchronous Pattern**: `<link rel="stylesheet" href="deferred.css" media="print" onload="this.media='all'">`.
- **Preload Pairing**: Always pair with `<link rel="preload" href="..." as="style">` for high-priority network discovery.
- **No-JS Fallback**: Always include `<noscript><link rel="stylesheet" href="..."></noscript>`.
- **Preventing CLS**: Always define container `aspect-ratio` and `min-height` in critical CSS.
- **Avoid `@import`**: Never use `@import` inside CSS; it creates slow serial network waterfalls.

---

## 13. Further Reading
- [web.dev: Defer Non-Critical CSS](https://web.dev/defer-non-critical-css/)
- [web.dev: Extract Critical CSS](https://web.dev/extract-critical-css/)
- [Ilya Grigorik: High Performance Browser Networking (TCP Optimizations)](https://hpbn.co/building-blocks-of-tcp/#slow-start)
- [Critters: Fast Critical CSS Inliner](https://github.com/GoogleChromeLabs/critters)
- [Addy Osmani: Critical Path CSS Generator & Tools](https://github.com/addyosmani/critical)

---

| Previous Note | Next Note |
| :--- | :--- |
| [01. CSS Performance Myths and Realities](01-css-performance-myths-and-realities.md) | [03. Layout, Paint and Composite Costs](03-layout-paint-and-composite-costs.md) |
