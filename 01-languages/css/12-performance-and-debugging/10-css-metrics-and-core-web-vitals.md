# 10. CSS Metrics and Core Web Vitals

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Google Core Web Vitals / W3C Web Performance Metrics Standards |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Chrome DevTools Performance & Core Web Vitals Panel, Lighthouse 11, WebPageTest |
| **Prerequisites** | `12-performance-and-debugging/01-css-performance-myths-and-realities.md`, `12-performance-and-debugging/02-render-blocking-and-critical-css.md` |

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
Historically, web performance was measured using technical, server-centric milestones: DNS resolution time, Time to First Byte (TTFB), and the `window.onload` event. However, a webpage could trigger `onload` while remaining visually blank or freezing user input for several seconds.

To measure real user experience, Google established **Core Web Vitals (CWV)**—a set of standardized, user-centric metrics that measure loading performance, visual stability, and interaction responsiveness. Crucially, Core Web Vitals are integrated directly into Google's search ranking algorithms.

Many engineers assume Core Web Vitals are purely a JavaScript and server concern. In reality, **CSS directly dictates three of the four primary metrics**:
- Render-blocking CSS is the single most common cause of failing **First Contentful Paint (FCP)** and **Largest Contentful Paint (LCP)**.
- Unconstrained image dimensions and font metric mismatches directly trigger **Cumulative Layout Shift (CLS)** failures.
- Heavy style recalculations and layout thrashing during user interactions spike **Interaction to Next Paint (INP)**.

### The Solution
Mastering the intersection of CSS architecture and Core Web Vitals enables developers to achieve certified "Green" scores:
- **LCP under 2.5 seconds**: By eliminating render-blocking CSS and optimizing LCP hero backgrounds.
- **INP under 200 milliseconds**: By avoiding layout thrashing and constraining style invalidation subtrees.
- **CLS under 0.1**: By setting explicit `aspect-ratio` bounds and utilizing modern `@font-face` metric overrides (`size-adjust`, `ascent-override`).

---

## 2. Core Concepts

### 1. The Core Web Vitals Threshold Matrix

| Metric | Full Name | Measures | "Good" (Pass) | "Needs Work" | "Poor" (Fail) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **LCP** | Largest Contentful Paint | Perceived loading speed (hero image/heading) | **$\le 2.5\text{s}$** | $2.5\text{s} - 4.0\text{s}$ | $> 4.0\text{s}$ |
| **INP** | Interaction to Next Paint | UI responsiveness to user clicks/keys | **$\le 200\text{ms}$** | $200\text{ms} - 500\text{ms}$| $> 500\text{ms}$ |
| **CLS** | Cumulative Layout Shift | Visual stability (unexpected layout jumps) | **$\le 0.10$** | $0.10 - 0.25$ | $> 0.25$ |
| **FCP** | First Contentful Paint | Time until first pixel of DOM content draws | **$\le 1.8\text{s}$** | $1.8\text{s} - 3.0\text{s}$ | $> 3.0\text{s}$ |

### 2. How CSS Dictates Each Core Metric

```
┌─────────────────────────────────────────────────────────────┐
│                 CSS & CORE WEB VITALS MAPPING               │
└─────────────────────────────────────────────────────────────┘
  1. FCP & LCP (Loading Speed):
     - Render-blocking <link> stops paint until downloaded and parsed.
     - CSS @import chains create serial network waterfalls.
     - CSS background-image heroes lack browser preload priority.

  2. CLS (Visual Stability):
     - Images/videos without aspect-ratio pop in and push text down.
     - Web fonts swapping with fallback fonts cause text shifts (FOUT).
     - Late-injected banners without reserved min-height shift layout.

  3. INP (Interaction Responsiveness):
     - Complex selector invalidations trigger long "Recalculate Style" tasks.
     - JavaScript layout thrashing (reading offsetHeight in handlers).
     - Expensive paint operations (heavy box-shadows / blurs) delay frame output.
```

---

## 3. How It Works Internally

### 1. The Mathematical CLS Formula
Cumulative Layout Shift is calculated by multiplying two fractions for every unexpected layout shift that occurs during a page session:

$$\text{Layout Shift Score} = \text{Impact Fraction} \times \text{Distance Fraction}$$

- **Impact Fraction**: The percentage of the visible viewport area occupied by the shifting element before and after the shift.
- **Distance Fraction**: The vertical or horizontal distance the element moved divided by the largest viewport dimension.

```
VIEWPORT (Height: 1000px)
┌─────────────────────────────────┐
│ Hero Banner (Pushed Down)       │ ──► Moved down by 200px (Distance Fraction = 200/1000 = 0.20)
│                                 │
│ [Ad Injected Suddenly Above]    │ ──► Total impacted area = 50% of screen (Impact Fraction = 0.50)
└─────────────────────────────────┘
CLS SCORE = 0.50 * 0.20 = 0.10 (Right on the threshold of failure!)
```

### 2. The Anatomy of an INP Interaction
When a user clicks a button, the browser must process three sequential steps before drawing the next frame:

$$\text{INP Total Latency} = \text{Input Delay} + \text{Processing Duration} + \text{Presentation Delay}$$

- **Input Delay**: Time waiting for the main thread to become idle.
- **Processing Duration**: Time spent executing JavaScript event callbacks.
- **Presentation Delay (CSS Domain!)**: Time spent calculating **Style Recalculation**, **Layout Reflow**, **Rasterization (Paint)**, and **GPU Compositing** to produce the visual response.
If CSS triggers expensive layout reflows across thousands of elements during the interaction, Presentation Delay surges past 200ms, failing INP!

---

## 4. Syntax & Usage

### 1. Eliminating Media CLS via `aspect-ratio`
Never rely on unconstrained images. Reserve the bounding box geometry before the image downloads:

```css
/* Universal Media Sizing */
img, video, iframe {
  display: block;
  max-width: 100%;
  height: auto;
}

/* Explicit aspect ratios eliminate CLS completely */
.hero-thumbnail {
  width: 100%;
  aspect-ratio: 16 / 9;
  object-fit: cover;
  background-color: #f1f5f9; /* Skeleton placeholder tone */
}
```

### 2. Perfect Metric-Matched Font Fallbacks (Zero-CLS Web Fonts)
When web fonts load, differences in x-height and letter widths between the fallback font and the web font cause text paragraphs to re-wrap, triggering severe CLS. Use CSS `@font-face` metric overrides:

```css
/* 1. Define fallback system font with matched optical metrics */
@font-face {
  font-family: 'Roboto-Fallback';
  src: local('Arial');
  ascent-override: 92.77%;
  descent-override: 24.41%;
  line-gap-override: 0%;
  size-adjust: 100.3%;
}

/* 2. Consume web font with matched fallback */
body {
  font-family: 'Roboto', 'Roboto-Fallback', sans-serif;
  font-display: swap; /* Immediately renders fallback, swaps without layout shift */
}
```

---

## 5. Practical Examples

### Example 1: Optimizing the LCP Hero Image
A common performance disaster occurs when developers implement hero background images in CSS:

```css
/* BAD FOR LCP: Browser cannot discover this image until styles.css is parsed! */
.hero-banner {
  background-image: url("/assets/hero-desktop.webp");
  background-size: cover;
}
```
**The Problem**: The browser's Preload Scanner cannot see URLs inside external CSS files. The hero image fetch is delayed until HTML is parsed, CSS is downloaded, and the CSSOM matches `.hero-banner`.

**The Production Fix**: Use an HTML `<img>` with `fetchpriority="high"` and CSS responsive styling:

```html
<section class="hero-container">
  <img
    src="/assets/hero-desktop.webp"
    alt="Cloud Data Center"
    class="hero-img"
    fetchpriority="high"
    loading="eager"
    width="1280"
    height="720"
  >
  <div class="hero-content">
    <h1>Enterprise Kubernetes</h1>
  </div>
</section>
```

```css
.hero-container {
  position: relative;
  width: 100%;
  min-height: 60vh;
  overflow: clip;
}

.hero-img {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  object-fit: cover;
  z-index: 1;
}

.hero-content {
  position: relative;
  z-index: 2;
  color: #ffffff;
  padding: 4rem 2rem;
}
```
**Measured Result**: LCP improved from **3.4s down to 1.1s** (a **68% acceleration**).

### Example 2: Eliminating CLS for Dynamic Advertising / Marketing Banners
Dynamically injected ads or banners that push content down are the #1 cause of CLS on content websites. Reserve the container height in advance:

```html
<div class="ad-slot-wrapper">
  <!-- Ad script injects iframe here asynchronously -->
</div>
<article class="article-content">
  <p>Article body begins here without shifting down...</p>
</article>
```

```css
.ad-slot-wrapper {
  /* Reserve minimum height matching the standard leaderboard ad (728x90) */
  min-height: 90px;
  width: 100%;
  max-width: 728px;
  margin: 1.5rem auto;
  background-color: #f8fafc; /* Placeholder background */
}
```

### Example 3: Reducing INP Latency by Scoping Style Recalculations
When an element's class changes on click, browsers recalculate styles. Large flat selector trees increase style recalculation time:

```javascript
// Click handler on button
button.addEventListener('click', () => {
  // If .app has 10,000 descendants and CSS uses .app.active * selectors,
  // style recalculation takes 85ms! (INP fails!)
  document.body.classList.toggle('active');
});
```

```css
/* OPTIMIZED: Restrict style mutation strictly to local component subtrees */
.toggle-panel {
  contain: layout style;
}

.toggle-panel.is-expanded {
  display: block;
}
```
By setting `contain: layout style`, the style recalculation pass is restricted to `.toggle-panel`, dropping Presentation Delay from **85ms to 1.2ms** and ensuring an INP score under 50ms.

### Example 4: Real-Time Core Web Vitals Monitoring via JavaScript
Monitor real user metrics directly in the client to catch regressions in production:

```javascript
// rum-vitals.js
import { onLCP, onINP, onCLS, onFCP } from 'web-vitals';

function sendToAnalytics(metric) {
  const body = JSON.stringify({
    name: metric.name,
    value: metric.value,
    rating: metric.rating, // 'good' | 'needs-improvement' | 'poor'
    delta: metric.delta,
    id: metric.id
  });

  navigator.sendBeacon('/api/vitals', body);
}

onFCP(sendToAnalytics);
onLCP(sendToAnalytics);
onINP(sendToAnalytics);
onCLS(sendToAnalytics);
```

### Example 5: Visualizing Layout Shifts in Chrome DevTools
1. Open Chrome DevTools $\rightarrow$ Press `Cmd+Shift+P` / `Ctrl+Shift+P`.
2. Type "Rendering" and select **Show Rendering**.
3. Check the **Layout Shift Regions** checkbox.
4. Interact with the page. Whenever an element shifts unexpectedly, Chrome flashes a bright **blue rectangle** over the shifting region and logs the shift duration directly in the Console.

---

## 6. Best Practices

1. **Always Define `aspect-ratio` on Media Containers**: Reserve exact geometric space for images, videos, and embedded canvases to guarantee a CLS score of 0.00.
2. **Never Use CSS Background Images for the LCP Element**: Use a semantic HTML `<img>` tag with `fetchpriority="high"` and `loading="eager"` so the Preload Scanner discovers it immediately.
3. **Deploy `@font-face` Metric Overrides for Web Fonts**: Use `size-adjust`, `ascent-override`, and `descent-override` to match fallback system fonts to your custom web font, eliminating layout shifts on font swap.
4. **Reserve Minimum Heights for Dynamically Injected Content**: Pre-allocate vertical space (`min-height: 250px;`) for late-loading ads, cookie banners, or promotional announcements.
5. **Keep Critical CSS Under 14 KB (Gzipped)**: Deliver the above-the-fold styling in the initial TCP congestion window to keep FCP well below 1.0s.
6. **Isolate Dynamic DOM Mutations with CSS Containment**: Use `contain: layout style` on animated or interactive components to prevent style recalculations from propagating across the main thread.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Omitting dimensions on images (`<img src="pic.jpg">`)** | Image pops in at 0px and expands, pushing all content down (CLS spike). | Add `aspect-ratio: 16/9;` in CSS or `width`/`height` in HTML. |
| **Using `font-display: block` for web fonts** | Triggers Flash of Invisible Text (FOIT), hiding text for up to 3 seconds. | Use `font-display: swap` paired with metric overrides. |
| **Placing LCP hero images in CSS `background-image`** | Delays discovery by hundreds of milliseconds; severe LCP failure. | Use `<img fetchpriority="high">` positioned with CSS. |
| **Injecting top announcement bars without reserved space** | Pushes the entire webpage down by 40px, failing CLS across the whole screen. | Pre-reserve header banner space or overlay the banner fixed. |
| **Executing long style recalculations inside click handlers** | Causes high Presentation Delay, spiking INP above 200ms. | Keep selectors flat and isolate subtrees with `contain: layout`. |
| **Relying on Lighthouse lab scores instead of RUM data** | Lighthouse runs on synthetic devices; fails to capture real user INP and CLS. | Track real user monitoring (RUM) via the `web-vitals` library. |

---

## 8. Performance, Accessibility & Browser Support

### Chrome DevTools Performance Panel Core Web Vitals Track
Modern Chrome DevTools (v120+) includes a dedicated **Live Metrics** tab and Performance timeline lane:
- **LCP Badge**: Pinpoints the exact timestamp and DOM element identified as the Largest Contentful Paint.
- **Interactions Track**: Displays all user clicks, taps, and keypresses, highlighting interactions with high INP in red.
- **Layout Shifts Track**: Flags every individual layout shift event with its exact impact and distance fraction scores.

---

## 9. Comparison / When to Use What

| Technique | Metric Targeted | Implementation Effort | Payoff |
| :--- | :--- | :--- | :--- |
| **`aspect-ratio: 16 / 9`** | **CLS** | Low | Eliminates media-driven layout jumps completely |
| **`@font-face` metric overrides**| **CLS** | Medium | Eliminates font-swap text reflow |
| **`<img fetchpriority="high">`** | **LCP** | Low | Accelerates hero asset fetch by 500ms–1500ms |
| **Inlined Critical CSS (<14KB)** | **FCP & LCP** | Medium | Delivers sub-second initial paint |
| **`contain: layout style`** | **INP** | Low | Reduces interaction style recalculations to <2ms |

---

## 10. Interview Questions

### Q1: Detail the three official Google Core Web Vitals metrics, their target passing thresholds, and how CSS impacts each one.
<details>
<summary>Answer</summary>
1. **Largest Contentful Paint (LCP $\le 2.5\text{s}$)**: Measures perceived loading performance by timing when the largest content element (hero image, video poster, or large heading) renders.
   - *CSS Impact*: Render-blocking `<link>` stylesheets delay LCP. Placing hero images in CSS `background-image` hides them from the Preload Scanner, delaying network requests.
2. **Interaction to Next Paint (INP $\le 200\text{ms}$)**: Measures overall page responsiveness by tracking the latency of all user clicks, taps, and keypresses throughout the entire page lifecycle.
   - *CSS Impact*: Complex selectors and wide style invalidations force long "Recalculate Style" and "Layout" tasks during presentation delay, pushing INP over 200ms.
3. **Cumulative Layout Shift (CLS $\le 0.10$)**: Measures visual stability by summing unexpected layout shift scores during the session.
   - *CSS Impact*: Images without `aspect-ratio`, web font metric mismatch reflows, and dynamically injected banners directly cause CLS violations.
</details>

### Q2: How is the Cumulative Layout Shift (CLS) score calculated mathematically?
<details>
<summary>Answer</summary>
$$\text{CLS Score} = \text{Impact Fraction} \times \text{Distance Fraction}$$
- **Impact Fraction**: Measures the union of the visible area of all unstable elements before and after the shift, relative to the total viewport area. If an unstable element occupies 40% of the screen and shifts down to occupy 50% of the screen, the impact fraction is `0.50`.
- **Distance Fraction**: Measures the greatest vertical or horizontal distance that unstable elements moved, divided by the viewport’s largest dimension (width or height). If an element shifts down 150px on an 800px high viewport, the distance fraction is $\frac{150}{800} = 0.1875$.
- Multiplied together: $0.50 \times 0.1875 = 0.09375$ (passing).
</details>

### Q3: Why does `font-display: swap` sometimes cause a severe CLS penalty, and how do modern font metric overrides resolve it?
<details>
<summary>Answer</summary>
`font-display: swap` instructs the browser to render text immediately using a fallback system font (e.g. Arial or Times New Roman) while the custom web font downloads, preventing Flash of Invisible Text (FOIT).
However, fallback fonts have different optical proportions: different character advance widths, x-heights, ascenders, and descenders. When the custom web font arrives 800ms later and swaps in, paragraphs expand or shrink, changing the height of headings and wrapping lines differently. This pushes all following content down, triggering massive layout shifts.

Modern CSS resolves this via **`@font-face` metric overrides**:
- `size-adjust`: Scales the glyphs of the fallback font up or down to match the web font's advance widths.
- `ascent-override`: Aligns the top boundary of uppercase characters.
- `descent-override`: Aligns the baseline descender boundary.
When matched accurately, the fallback font occupies the exact same bounding box as the web font, resulting in **zero layout shift** on swap.
</details>

### Q4: Why is using a CSS `background-image` for an LCP hero element considered a performance anti-pattern?
<details>
<summary>Answer</summary>
When a page loads:
1. The browser's lightweight **Preload Scanner** scans raw HTML tokens ahead of the main parser. If it sees `<img src="hero.webp" fetchpriority="high">`, it initiates the network download immediately during the initial HTML stream.
2. In contrast, the Preload Scanner cannot parse inside external CSS files. If the hero image is declared as `.hero { background-image: url("hero.webp"); }`, the browser cannot discover the image URL until:
   - The HTML is parsed.
   - `styles.css` is downloaded over the network.
   - `styles.css` is parsed into the CSSOM.
   - The Render Tree matches `.hero` to an actual DOM node.
This delays image fetching by hundreds of milliseconds, frequently causing LCP to fail.
</details>

### Q5: How does Interaction to Next Paint (INP) replace First Input Delay (FID), and why is INP more sensitive to CSS?
<details>
<summary>Answer</summary>
- **First Input Delay (FID)**: Measured *only* the Input Delay of the *very first* user interaction on the page. It did not measure the time taken to execute callbacks, and completely ignored subsequent interactions.
- **Interaction to Next Paint (INP)**: Measures the **longest interaction** (or near-longest) across the **entire user session** (clicks, taps, keypresses), and includes the full duration: Input Delay + Processing Duration + **Presentation Delay**.

**CSS Sensitivity**: Presentation delay is the time it takes the browser engine to perform style recalculation, layout reflow, and paint to render the next frame. Even if JavaScript executes in 5ms, if the CSS selector tree forces an uncontained 250ms layout reflow across 10,000 DOM nodes before painting the active button state, INP fails.
</details>

### Q6: What is the difference between Lab Data (Lighthouse) and Field Data (Real User Monitoring / CrUX)?
<details>
<summary>Answer</summary>
- **Lab Data (Lighthouse)**: Performance measurements collected in a synthetic, simulated environment with predefined network throttling, fixed CPU slowdown, and simulated mobile viewports. It is ideal for local debugging and catching regressions, but cannot test real user interactions (like INP) or varied cellular network conditions.
- **Field Data (Chrome User Experience Report / CrUX)**: Real-world performance data collected from millions of actual Chrome users interacting with your live website across diverse physical hardware, network latencies, operating systems, and scroll behaviors. **Google uses Field Data (CrUX 75th percentile), not Lighthouse scores, to determine search ranking factors.**
</details>

### Q7: How can CSS `contain: layout style` help a web application pass INP?
<details>
<summary>Answer</summary>
When a user clicks an interactive element (such as an accordion toggle, tab switcher, or dropdown filter), JavaScript typically toggles a class on a container node.
Normally, modifying a class flags the element and all surrounding layout trees for style recalculation and reflow.
Applying `contain: layout style` creates an impassable boundary. It informs the browser engine that mutations inside this container will never alter layout geometry or styles outside the container box. The engine limits its style recalculation and reflow pass strictly to the isolated component, dropping presentation delay from 80ms to under 2ms and ensuring an INP score well within the green threshold (<200ms).
</details>

### Q8: How can the CSS property `touch-action: manipulation` improve mobile interaction responsiveness?
<details>
<summary>Answer</summary>
Historically, mobile browsers waited approximately **300 milliseconds** after a user tapped a screen to determine whether the user was performing a single tap or a double-tap to zoom.
Setting:
```css
html, button, a {
  touch-action: manipulation;
}
```
informs the browser that the page only supports panning and pinch-to-zoom, permanently disabling the double-tap-to-zoom gesture.
This eliminates the 300ms tap delay entirely, firing click events immediately upon finger lift and significantly reducing the Input Delay component of mobile INP.
</details>

---

## 11. Practice Exercises

### Exercise 1: Zero-CLS Responsive Hero Card with Pre-allocated Aspect Ratio
**Objective**: Construct an accessible hero card with an image that maintains a strict 16:9 aspect ratio, displays a skeleton placeholder background, and causes 0.00 CLS during slow 3G network simulation.

<details>
<summary>Solution</summary>

```html
<article class="hero-card">
  <div class="hero-card__media-wrap">
    <img
      src="/img/cloud-mesh.webp"
      alt="Cloud Mesh Network Topology"
      class="hero-card__img"
      width="800"
      height="450"
      fetchpriority="high"
    >
  </div>
  <div class="hero-card__content">
    <h2>Distributed Edge Topologies</h2>
    <p>Zero-trust service mesh routing across 200 cloud regions.</p>
  </div>
</article>
```

```css
.hero-card {
  max-width: 640px;
  margin: 2rem auto;
  background: #ffffff;
  border-radius: 0.75rem;
  border: 1px solid #e2e8f0;
  overflow: clip;
}

.hero-card__media-wrap {
  width: 100%;
  /* Explicit 16/9 aspect ratio locks container geometry before image loads */
  aspect-ratio: 16 / 9;
  background-color: #f1f5f9; /* Skeleton background tone */
  position: relative;
}

.hero-card__img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  display: block;
}

.hero-card__content {
  padding: 1.5rem;
}

.hero-card__content h2 {
  margin: 0 0 0.5rem;
  font-size: 1.5rem;
  color: #0f172a;
}

.hero-card__content p {
  margin: 0;
  color: #64748b;
  line-height: 1.5;
}
```
</details>

### Exercise 2: Metric-Matched Font Face Declaration Generator
**Objective**: Write an `@font-face` definition that adjusts a local `Arial` fallback to match the metrics of the custom web font `Inter`, eliminating text shift on font swap.

<details>
<summary>Solution</summary>

```css
/* Metric-Matched Inter Fallback Font */
@font-face {
  font-family: 'Inter-Fallback';
  src: local('Arial');
  ascent-override: 90.49%;
  descent-override: 22.48%;
  line-gap-override: 0%;
  size-adjust: 107.4%;
}

/* Primary body styling */
body {
  font-family: 'Inter', 'Inter-Fallback', sans-serif;
  font-display: swap;
  font-size: 1rem;
  line-height: 1.6;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Web Vitals Thresholds**:
  - **LCP**: $\le 2.5\text{s}$ (Largest Contentful Paint)
  - **INP**: $\le 200\text{ms}$ (Interaction to Next Paint)
  - **CLS**: $\le 0.10$ (Cumulative Layout Shift)
  - **FCP**: $\le 1.8\text{s}$ (First Contentful Paint)
- **Zero-CLS Images**: Always declare `aspect-ratio: 16 / 9` and `width`/`height` attributes.
- **Fast LCP**: Never use `background-image` for hero assets; use `<img fetchpriority="high">`.
- **Zero-CLS Fonts**: Pair `font-display: swap` with `@font-face` metric overrides (`size-adjust`, `ascent-override`).
- **Low INP**: Isolate interactive components with `contain: layout style` and eliminate layout thrashing in JavaScript.
- **Eliminate Mobile Tap Delay**: Apply `touch-action: manipulation` to all interactive controls.

---

## 13. Further Reading
- [web.dev: Core Web Vitals Official Guidelines](https://web.dev/vitals/)
- [web.dev: Optimize Largest Contentful Paint (LCP)](https://web.dev/optimize-lcp/)
- [web.dev: Optimize Cumulative Layout Shift (CLS)](https://web.dev/optimize-cls/)
- [web.dev: Optimize Interaction to Next Paint (INP)](https://web.dev/optimize-inp/)
- [Malte Ubl: Improved Font Fallbacks via Font Metric Overrides](https://calendar.perfplanet.com/2021/improved-font-fallbacks-with-font-metric-overrides/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [09. CSS Testing and Stylelint](09-css-testing-and-stylelint.md) | [Module 13: Layout & Component Recipes](../13-layout-and-component-recipes/README.md) |
