# 08. Cross-Browser Strategy and Baseline

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Web Platform Baseline / W3C Feature Detection & CSS Conditional Rules L3/L4 |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Web Platform Baseline API, CanIUse data, LightningCSS 1.25, Playwright |
| **Prerequisites** | `00-setup/06-css-modules-levels-and-baseline.md`, `12-performance-and-debugging/01-css-performance-myths-and-realities.md` |

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
In the early web, cross-browser compatibility was a chaotic landscape of browser wars, fragmented CSS implementations, and fragile hacks (`*html`, CSS conditional comments, `-webkit-`, `-moz-`, and `-ms-` vendor prefixes). Developers wrote redundant, brittle code trying to force every browser to look pixel-identical across wildly different operating systems and devices.

Today, while the browser engine landscape has consolidated around three primary rendering engines—**Chromium Blink** (Chrome, Edge, Brave, Opera), **Mozilla Gecko** (Firefox), and **Apple WebKit** (Safari, iOS browsers)—new CSS specifications ship at unprecedented velocity. Features like CSS Subgrid, Anchor Positioning, View Transitions, and Color Spaces arrive in different engines at different times. If engineering teams lack a systematic strategy, they either block adoption of modern CSS out of fear, or ship broken interfaces that lock out Safari or Firefox users.

### The Solution
The modern Web Platform introduces a unified framework for cross-browser architecture:
- **Web Platform Baseline**: An industry-wide standard (curated by Google, Apple, Microsoft, and Mozilla under the W3C WebDX Community Group) categorizing every web feature into **Baseline Newly Available** or **Baseline Widely Available** (supported across all engines for 30+ months).
- **CSS Feature Queries (`@supports`)**: Native, conditional CSS evaluation for properties, experimental values, selectors (`@supports selector(...)`), and font technologies.
- **Progressive Enhancement Architecture**: Guaranteeing that content and core functionality render cleanly on all baseline browsers, while layering enhanced visual ergonomics (e.g. subgrid alignment, smooth view transitions) for capable engines.
- **Automated Compiler Targeting (LightningCSS)**: Compiling modern standard CSS directly to target specific browser versions at build time without manual prefixing.

---

## 2. Core Concepts

### 1. The Three Modern Browser Engines

```
┌─────────────────────────────────────────────────────────────┐
│                 THE THREE WEB ENGINE FAMILIES               │
└─────────────────────────────────────────────────────────────┘
  1. Chromium Blink (Google / Microsoft / Opera)
     - Powers: Chrome, Edge, Brave, Vivaldi, Samsung Internet
     - Strengths: Rapid spec implementation, devtools, CSS Houdini, View Transitions

  2. Apple WebKit (Apple)
     - Powers: Safari (macOS, iOS, iPadOS), VisionOS
     - Note: ALL iOS web browsers (including Chrome on iOS) MUST use WebKit!
     - Strengths: Color spaces (Display P3), power efficiency, typography

  3. Mozilla Gecko (Mozilla)
     - Powers: Firefox, Firefox Android, Tor Browser
     - Strengths: Pioneering CSS Grid implementation, developer-focused inspection
```

### 2. The Web Platform Baseline Spectrum

```
           [ NOT YET BASELINE ]
           Supported in 1 or 2 engines (e.g., CSS Anchor Positioning in Blink)
                    │
                    ▼
           [ BASELINE NEWLY AVAILABLE ]
           Shipped across ALL 3 core engines: Chrome, Firefox, and Safari!
           (e.g., CSS Subgrid, :has(), CSS Nesting)
                    │
                    ▼ (30 Months of universal availability)
           [ BASELINE WIDELY AVAILABLE ]
           Universally safe for 99.5%+ global audience without polyfills.
           (e.g., CSS Grid, Flexbox, CSS Custom Properties, @layer)
```

### 3. Progressive Enhancement vs. Graceful Degradation
- **Graceful Degradation (Legacy)**: Building a complex experience for the latest browser, and then attempting to patch broken pieces for older browsers. Often leads to neglected, broken fallbacks.
- **Progressive Enhancement (Modern)**: Building an accessible, robust baseline experience using universal CSS (Flexbox, standard grid, reliable fallbacks) first, and then using `@supports` to progressively enhance the experience with modern features (subgrid, anchor positioning, view transitions) for browsers that support them.

---

## 3. How It Works Internally

### How the Engine Evaluates `@supports`
The browser engine parses `@supports` conditions at the parser level during CSSOM construction:

```
@supports selector(:has(a)) and (color: oklch(0.6 0.2 150)) {
  .card:has(.active) {
    border-color: oklch(0.6 0.2 150);
  }
}

BROWSER PARSER EVALUATION:
  1. Checks selector grammar: Does the internal parser support :has()? ──► TRUE
  2. Checks CSS property and value: Does the color engine parse oklch()? ──► TRUE
  3. Logical AND evaluation: TRUE && TRUE ──► TRUE
  4. RESULT: The block is incorporated into the CSSOM!
```
If an engine does not recognize the syntax or property value, it evaluates the condition as `false` and skips the entire block with **zero runtime overhead**.

---

## 4. Syntax & Usage

### 1. The Complete `@supports` Syntax Hierarchy
```css
/* 1. Property and Value check */
@supports (display: grid) {
  .container { display: grid; }
}

/* 2. Selector check (Level 4 feature queries) */
@supports selector(:has(a)) {
  .nav:has(.is-active) {
    background-color: #f1f5f9;
  }
}

/* 3. Logical Operators: and, or, not */
@supports (display: flex) and (not (display: grid)) {
  .layout { display: flex; }
}

/* 4. Font Technology check */
@supports font-tech(color-COLRv1) {
  @font-face {
    font-family: 'ColorEmoji';
    src: url('emoji-colrv1.woff2') format('woff2');
  }
}
```

### 2. The Fallback-First Declaration Stacking Pattern
Browsers discard properties they do not understand while keeping valid preceding declarations. You often do not even need `@supports`—simply stack declarations:

```css
.card {
  /* 1. Universal Fallback: sRGB hex code for older browsers */
  background-color: #2563eb;

  /* 2. Modern Enhancement: Perceptually uniform OKLCH color */
  /* Modern engines overwrite the hex; older engines ignore this line */
  background-color: oklch(0.55 0.22 260);
}
```

---

## 5. Practical Examples

### Example 1: Progressive Subgrid Enhancement
CSS Grid is Baseline Widely Available; CSS Subgrid is Baseline Newly Available. Structure cards with robust Flexbox fallbacks, upgrading to Subgrid:

```html
<div class="card-grid">
  <article class="card">
    <h3 class="card-title">Edge Compute</h3>
    <p class="card-desc">Short description.</p>
    <button class="card-btn">Deploy</button>
  </article>
  <article class="card">
    <h3 class="card-title">Distributed Data Warehouse with Automated Backups</h3>
    <p class="card-desc">Multi-line detailed description that pushes content.</p>
    <button class="card-btn">Deploy</button>
  </article>
</div>
```

```css
/* BASELINE EXPERIENCE: Standard Flexbox vertical stack */
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 1.5rem;
}

.card {
  display: flex;
  flex-direction: column;
  padding: 1.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
}

.card-btn {
  margin-top: auto; /* Pushes button to bottom in Flexbox */
}

/* PROGRESSIVE ENHANCEMENT: Subgrid aligns titles and buttons perfectly */
@supports (grid-template-rows: subgrid) {
  .card-grid {
    grid-auto-rows: auto auto 1fr auto;
  }

  .card {
    display: grid;
    grid-row: span 4;
    grid-template-rows: subgrid;
  }

  .card-btn {
    margin-top: 0; /* Let subgrid track govern button alignment */
  }
}
```

### Example 2: Anchor Positioning with Absolute Fallback
CSS Anchor Positioning is currently supported in Chromium, with Safari and Firefox in active development:

```html
<div class="anchor-wrapper">
  <button type="button" class="anchor-target" id="help-btn">Need Help?</button>
  <div class="tooltip-popup">Click to speak with an engineer.</div>
</div>
```

```css
/* BASELINE: Relative parent wrapper with standard absolute positioning */
.anchor-wrapper {
  position: relative;
  display: inline-block;
}

.tooltip-popup {
  position: absolute;
  top: 100%;
  left: 50%;
  transform: translateX(-50%);
  margin-top: 8px;
  background: #0f172a;
  color: #fff;
  padding: 0.5rem 1rem;
  border-radius: 4px;
  white-space: nowrap;
}

/* PROGRESSIVE ENHANCEMENT: Anchor Positioning */
@supports (anchor-name: --help-btn) {
  .anchor-wrapper {
    position: static; /* No longer trapped by ancestor positioning context! */
  }

  .anchor-target {
    anchor-name: --help-btn;
  }

  .tooltip-popup {
    position: fixed;
    position-anchor: --help-btn;
    top: anchor(bottom);
    left: anchor(center);
    transform: translateX(-50%);
    margin-top: 8px;
    /* Automatically flip above button if near bottom of screen */
    position-try-options: flip-block;
  }
}
```

### Example 3: View Transitions API Progressive Layering
```css
/* Baseline: Standard instant cut transitions */
.page-header {
  background-color: #ffffff;
}

/* Enhanced: Cross-fade transition across DOM updates */
@supports (view-transition-name: site-header) {
  .page-header {
    view-transition-name: site-header;
  }
}
```

### Example 4: Testing Cross-Browser Layout in Playwright CI
Automate cross-browser regression testing across Chromium, Firefox, and WebKit simultaneously:

```javascript
// tests/layout.spec.js
const { test, expect } = require('@playwright/test');

test.describe('Responsive Card Grid Cross-Browser', () => {
  test('should render 3 columns on desktop and 1 column on mobile', async ({ page }) => {
    await page.goto('http://localhost:3000');
    
    // Test on desktop viewport
    await page.setViewportSize({ width: 1280, height: 800 });
    const cards = page.locator('.card');
    await expect(cards).toHaveCount(3);
    
    const cardBox = await cards.first().boundingBox();
    expect(cardBox.width).toBeGreaterThan(250);

    // Verify screenshot matches baseline image across Blink, Gecko, and WebKit
    await expect(page).toHaveScreenshot('card-grid-desktop.png');
  });
});
```

### Example 5: Automated Vendor Prefixing with LightningCSS
Never author manual `-webkit-` prefixes in source code. Let LightningCSS insert prefixes based on your exact `browserslist` target:

```javascript
// lightningcss targets configuration
const targets = {
  chrome: 100 << 16,
  safari: 15 << 16,
  firefox: 100 << 16
};

// Input CSS written by author:
// .box { user-select: none; }

// LightningCSS automatically outputs:
// .box { -webkit-user-select: none; user-select: none; }
```

---

## 6. Best Practices

1. **Verify Baseline Status Before Introducing New Features**: Check [web.dev/baseline](https://web.dev/baseline) to confirm whether a feature is "Baseline Widely Available" or requires progressive enhancement.
2. **Never Write Manual Vendor Prefixes**: Do not clutter stylesheets with `-webkit-` or `-moz-` prefixes; configure **LightningCSS** or **Autoprefixer** to manage prefixes automatically based on your browser support targets.
3. **Always Build an Accessible Baseline First**: Design layout and color using features that render reliably across all engines (Flexbox, standard Grid, sRGB fallbacks), then layer `@supports` enhancements on top.
4. **Use `@supports selector(...)` for Complex Pseudo-Classes**: When using relational selectors like `:has()` or `:focus-visible`, check selector support to ensure older engines do not invalidate the entire selector rule.
5. **Remember the iOS WebKit Mandate**: Regardless of whether an iPhone user downloads Chrome, Firefox, or Edge, **Apple requires all iOS browsers to run on the WebKit engine**. Testing on Safari is functionally testing all iOS devices.
6. **Integrate Multi-Engine Visual Regression in CI**: Run automated Playwright tests against Chromium, Firefox, and WebKit to catch browser-specific rendering bugs before production deployment.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Using experimental CSS without fallback** | Page breaks or renders blank in browsers that have not shipped the feature yet. | Implement fallback-first CSS or wrap in `@supports`. |
| **Writing manual vendor prefixes (`-webkit-border-radius`)** | Code clutter and maintenance debt; often missing modern standard syntax. | Let LightningCSS or Autoprefixer handle prefixes automatically. |
| **Testing only on Google Chrome Desktop** | Disastrous surprises when Safari (WebKit) or Firefox render layouts differently. | Test across Chrome, Safari, and Firefox during active development. |
| **Assuming Chrome on iOS is Chromium** | Expecting Chromium-only features to work on iPhone Chrome. | Chrome on iOS runs on WebKit; test against Safari/WebKit standards. |
| **Using CSS hacks (e.g. `@media screen and (-webkit-min-device-pixel-ratio:0)`)** | Extremely brittle; breaks unexpectedly when browsers update. | Use standard `@supports` feature queries. |
| **Neglecting Fallback Stacking Order** | Placing standard properties *before* fallbacks causes the fallback to override the modern syntax. | Place fallback first, modern declaration second. |

---

## 8. Performance, Accessibility & Browser Support

### The Web Platform Baseline Commitment
Created in 2023 by the WebDX Community Group (Google, Mozilla, Apple, Microsoft), Baseline provides transparency on feature stability:
- **Baseline Newly Available**: Available in the latest versions of Chrome, Edge, Firefox, and Safari.
- **Baseline Widely Available**: Reached 30 months after becoming Newly Available. Considered universally safe for production use without fallbacks or polyfills.

---

## 9. Comparison / When to Use What

| Strategy | When to Use | Maintenance Cost | User Experience |
| :--- | :--- | :--- | :--- |
| **Declaration Stacking** | Colors (OKLCH $\rightarrow$ sRGB), units | Zero | Seamless fallback |
| **`@supports (property: value)`** | Layout engines (Subgrid, Masonry) | Low | Enhanced layout on modern engines |
| **`@supports selector(...)`** | Relational pseudo-classes (`:has`) | Low | Safe styling without selector breakage |
| **JS Feature Polyfill** | Functional APIs (Popover, Dialog) | High | Clutters bundle; use only if critical |

---

## 10. Interview Questions

### Q1: What is the Web Platform Baseline initiative, and how does it change how engineering teams make architectural decisions?
<details>
<summary>Answer</summary>
The Web Platform Baseline is an initiative established by the W3C WebDX Community Group in collaboration with Google, Apple, Microsoft, and Mozilla. It establishes clear, objective thresholds for web platform feature support:
1. **Newly Available**: The feature is supported in the stable releases of all three core browser engines: Chromium (Chrome/Edge), Gecko (Firefox), and WebKit (Safari).
2. **Widely Available**: Exactly 30 months have passed since the feature became Newly Available. At this stage, the feature is supported by over 99% of global internet users.
This eliminates guesswork: engineering teams no longer debate complex browser matrices. Teams establish an organizational policy (e.g. "We can adopt any feature that is Baseline Widely Available without fallbacks, and use `@supports` for Newly Available features"), streamlining architectural choices.
</details>

### Q2: Why must all iOS web browsers (including Chrome and Firefox on iPhone) be evaluated as WebKit rather than Chromium or Gecko?
<details>
<summary>Answer</summary>
Under Apple's iOS App Store Review Guidelines (Section 2.5.6), all third-party web browsers and applications that browse the web on iOS and iPadOS must use the iOS WebKit framework.
Even though a user downloads "Google Chrome" or "Mozilla Firefox" from the iOS App Store, those applications are custom browser UI skins built on top of **Apple WebKit**. They share the exact same CSS rendering engine, layout parser, and JavaScript Nitro engine as Safari.
Therefore, testing an application on desktop Chrome does **not** verify how Chrome on iPhone will render your website; developers must test against WebKit/Safari.
</details>

### Q3: How does the CSS `@supports selector(...)` syntax protect against catastrophic stylesheet invalidation?
<details>
<summary>Answer</summary>
In CSS selector parsing rules, if a browser encounters an unrecognized or invalid selector in a comma-separated selector list:
```css
/* CATASTROPHIC FAILURE IN OLDER BROWSERS */
.card:has(.active),
.card.is-active {
  background: red;
}
```
If an older engine does not recognize `:has()`, the specification dictates that the engine must **invalidate the entire selector rule**! Even though `.card.is-active` is valid, the entire block is thrown away.
By wrapping modern selectors in `@supports selector(...)`:
```css
@supports selector(:has(a)) {
  .card:has(.active) { background: red; }
}
```
Browsers that do not support the selector cleanly skip the block without invalidating other styles.
</details>

### Q4: Why is writing manual vendor prefixes (like `-webkit-box-shadow` or `-moz-border-radius`) an anti-pattern in modern CSS?
<details>
<summary>Answer</summary>
1. **Obsolete Bloat**: Modern browser engines dropped prefixes for standard properties over a decade ago. Writing manual prefixes bloats stylesheet file sizes with dead code that modern browsers ignore.
2. **Maintenance Drag**: Manually maintaining four vendor prefix variants per property increases human error and creates situations where prefixes drift out of sync with standard syntax.
3. **Automated Tooling**: Modern CSS compilers (like LightningCSS, esbuild, and Autoprefixer) ingest standard CSS and query live CanIUse/Browserslist databases, automatically injecting vendor prefixes *only* for the specific browser versions targeted by your project.
</details>

### Q5: Explain the difference between "Progressive Enhancement" and "Graceful Degradation".
<details>
<summary>Answer</summary>
- **Graceful Degradation**: Focuses on building the application with all advanced features enabled for the newest, most capable browser engines. When tested on older or constrained browsers, developers patch broken functionality so the site "degrades gracefully" without crashing. In practice, this often leads to neglected fallbacks and broken user experiences on non-target browsers.
- **Progressive Enhancement**: Starts with a solid, resilient foundation that works universally across every browser (semantic HTML, basic styling, fluid Flexbox). Then, using feature queries (`@supports`) or modern capabilities, developers layer on enhanced visual aesthetics (subgrid, view transitions, advanced animations) for capable engines. The site is guaranteed to work everywhere, and gets progressively better on modern browsers.
</details>

### Q6: How does the "Declaration Stacking" pattern work without writing `@supports`?
<details>
<summary>Answer</summary>
When a CSS parser encounters an unrecognized property or an invalid value, the CSS specification dictates that the parser must silently discard that specific declaration and proceed to the next line.
By stacking declarations in order of increasing modernness:
```css
.hero {
  /* Fallback: understood by 100% of browsers */
  color: #2563eb;
  /* Modern: understood by modern browsers */
  color: oklch(0.55 0.22 260);
}
```
- An older engine parses `#2563eb`. When it reaches the `oklch(...)` line, it fails to parse the color space and ignores it, retaining `#2563eb`.
- A modern engine parses `#2563eb`, then parses `oklch(...)`, which cleanly overwrites the previous value in the cascade.
</details>

### Q7: What is the purpose of `@supports font-tech(...)` introduced in CSS Conditional Rules Level 4?
<details>
<summary>Answer</summary>
Modern web fonts support various advanced font technologies, such as color font formats (`COLRv1`, `SVG`, `sbix`) and variable font capabilities (`variations`, `palettes`).
`@supports font-tech(...)` allows developers to test whether the client browser engine supports a specific font format before requesting the file over the network:
```css
@supports font-tech(color-COLRv1) {
  @font-face {
    font-family: 'ColorEmoji';
    src: url('emoji-colrv1.woff2') format('woff2');
  }
}
```
This prevents browsers that lack COLRv1 color vector support from wasting bandwidth downloading heavy font binaries they cannot render.
</details>

### Q8: How can Playwright be used to catch cross-browser CSS rendering regressions automatically?
<details>
<summary>Answer</summary>
Playwright controls headless instances of all three core engines: Chromium, Firefox, and WebKit.
In a continuous integration (CI) pipeline, Playwright can:
1. Launch identical test pages in all three engines simultaneously.
2. Capture screenshot snapshots of key components and full pages.
3. Compare the rendered pixel output against approved golden baseline images using `expect(page).toHaveScreenshot()`.
4. If a CSS change in Safari/WebKit introduces a visual discrepancy exceeding a pixel threshold, the test suite fails and generates a visual diff image showing the exact pixels that shifted.
</details>

---

## 11. Practice Exercises

### Exercise 1: Progressive OKLCH Color Palette with sRGB Fallbacks
**Objective**: Build a color palette utility block that provides vibrant, perceptually uniform OKLCH colors for modern browsers, with rock-solid sRGB hex fallbacks for older engines.

<details>
<summary>Solution</summary>

```css
:root {
  /* 1. Universal sRGB Fallbacks */
  --color-primary: #1d4ed8;
  --color-accent: #059669;
  --color-surface: #f8fafc;
  --color-surface-border: #e2e8f0;
}

/* 2. Modern Engine Progressive Enhancement */
@supports (color: oklch(0.6 0.2 250)) {
  :root {
    --color-primary: oklch(0.55 0.22 260);
    --color-accent: oklch(0.65 0.18 155);
    --color-surface: oklch(0.98 0.01 250);
    --color-surface-border: oklch(0.92 0.02 250);
  }
}

.button-brand {
  background-color: var(--color-primary);
  color: #ffffff;
  border-radius: 0.375rem;
  padding: 0.5rem 1rem;
  border: 1px solid transparent;
}
```
</details>

### Exercise 2: Progressive Masonry Grid with Column Fallbacks
**Objective**: Build an image gallery that utilizes CSS Multi-column layout as a baseline, progressively enhancing to CSS Grid Masonry when supported.

<details>
<summary>Solution</summary>

```html
<div class="gallery-container">
  <figure class="gallery-item"><img src="/img/1.webp" alt="Landscape"></figure>
  <figure class="gallery-item"><img src="/img/2.webp" alt="Portrait"></figure>
  <figure class="gallery-item"><img src="/img/3.webp" alt="Architecture"></figure>
</div>
```

```css
/* BASELINE: CSS Multi-column layout (Universal baseline) */
.gallery-container {
  column-count: 3;
  column-gap: 1.5rem;
}

.gallery-item {
  break-inside: avoid;
  margin-bottom: 1.5rem;
}

.gallery-item img {
  width: 100%;
  height: auto;
  display: block;
  border-radius: 0.5rem;
}

/* PROGRESSIVE ENHANCEMENT: Native CSS Grid Masonry */
@supports (grid-template-rows: masonry) {
  .gallery-container {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    grid-template-rows: masonry;
    gap: 1.5rem;
    column-count: auto; /* Disables multi-column */
  }

  .gallery-item {
    margin-bottom: 0;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **The Big 3 Engines**: Chromium (Blink), Mozilla (Gecko), Apple (WebKit). All iOS browsers run on WebKit!
- **Web Platform Baseline**:
  - *Newly Available*: Shipped in Chrome, Firefox, and Safari.
  - *Widely Available*: 30 months in all engines (universally safe without fallbacks).
- **`@supports (prop: val)`**: Evaluates property and value support at the parser level.
- **`@supports selector(selector)`**: Safely tests complex pseudo-classes (like `:has()`) to prevent entire selector rules from being discarded.
- **Declaration Stacking**: Write fallback value first, modern value second.
- **No Manual Prefixes**: Use **LightningCSS** to target browser versions automatically.
- **CI Testing**: Use **Playwright** to run visual regression tests across Chromium, Firefox, and WebKit.

---

## 13. Further Reading
- [web.dev: Introducing Web Platform Baseline](https://web.dev/baseline)
- [W3C CSS Conditional Rules Module Level 3](https://www.w3.org/TR/css-conditional-3/)
- [W3C CSS Conditional Rules Module Level 4 (@supports selector & font-tech)](https://www.w3.org/TR/css-conditional-4/)
- [MDN Web Docs: @supports](https://developer.mozilla.org/en-US/docs/Web/CSS/@supports)
- [Playwright: Cross-Browser Visual Regression Testing](https://playwright.dev/docs/test-snapshots)

---

| Previous Note | Next Note |
| :--- | :--- |
| [07. Common Layout Bugs and Fixes](07-common-layout-bugs-and-fixes.md) | [09. CSS Testing and Stylelint](09-css-testing-and-stylelint.md) |
