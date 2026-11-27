# 03. Prefers Color Scheme, Contrast and Reduced Data

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Media Queries Level 5 / W3C Candidate Recommendation |
| **Browsers Tested** | Chrome 96+, Firefox 89+, Safari 14.1+ (Universal modern baseline) |
| **Tested With** | Modern DevTools Rendering Emulation, macOS & Windows Accessibility Settings |
| **Prerequisites** | `05-responsive-design/01-media-queries.md`, `10-architecture-and-methodologies/07-dark-mode-and-multi-theme.md` |

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
Traditional responsive web design focused almost exclusively on physical hardware viewports: screen width, device pixel ratio, and orientation. However, users interact with web applications across diverse physiological conditions, cognitive preferences, and network constraints.

A user with low vision may configure their OS to enforce maximum contrast; an individual with photophobia or migraines may require dark interfaces; a user with visual processing fatigue may need frosted-glass background blurs removed; and a commuter on a cellular connection may be operating under severe data caps. When websites ignore these operating system preferences, they deliver uncomfortable, inaccessible, or financially costly experiences.

### The Solution
CSS Media Queries Level 5 introduces **User Preference Media Features**. These features query the user's host operating system settings directly, enabling stylesheets to adapt proactively without requiring manual account configuration or intrusive permission prompts:
- `prefers-color-scheme`: Light vs. dark theme adaptation.
- `prefers-contrast`: High-contrast or low-contrast accommodations.
- `prefers-reduced-transparency`: Disabling blur filters and semi-transparent overlays.
- `prefers-reduced-data`: Suppressing heavy background images, preloads, and video autoplay.

---

## 2. Core Concepts

### Taxonomy of User Preference Media Features

```
┌─────────────────────────────────────────────────────────────┐
│             OS ACCESSIBILITY & PREFERENCE ENGINE            │
└─────────────────────────────────────────────────────────────┘
                               │
       ┌───────────────────────┼───────────────────────┐
       ▼                       ▼                       ▼
 [Visual Ergonomics]     [Visual Processing]     [Network & Data]
 - prefers-color-scheme  - prefers-contrast      - prefers-reduced-data
 - prefers-reduced-motion- prefers-reduced-      - Save-Data HTTP Header
                         transparency
```

1. **`prefers-color-scheme` (`light` | `dark` | `no-preference`)**:
   - Reflects the host operating system's global appearance theme (macOS Dark Mode, Windows Dark Theme, iOS/Android Dark Mode).
2. **`prefers-contrast` (`no-preference` | `more` | `less` | `custom`)**:
   - `more`: User enabled OS high-contrast settings (e.g. macOS "Increase Contrast" or Windows Contrast Themes). Demands higher color contrast ratios and thicker, clearer borders.
   - `less`: User requested reduced visual contrast (e.g. to relieve eye strain or photophobia).
   - `custom`: User configured an OS-enforced custom color palette (e.g. Windows High Contrast Mode).
3. **`prefers-reduced-transparency` (`no-preference` | `reduce`)**:
   - Reflects settings like macOS "Reduce transparency" or Windows "Transparency effects: Off". Requires eliminating CSS `backdrop-filter: blur()` and making semi-transparent backgrounds opaque.
4. **`prefers-reduced-data` (`no-preference` | `reduce`)**:
   - Reflects Android/iOS "Data Saver" or "Low Data Mode". Suggests avoiding heavy background images, suppressing non-critical font weights, and halting asset preloading.

---

## 3. How It Works Internally

### Browser & OS Preference Synchronization
When the operating system settings change, the OS triggers a platform notification. The browser catches this signal and immediately updates its internal media query environment variables:

```
[User Toggles "Increase Contrast" in macOS Settings]
                       │
                       ▼
          [macOS CoreGraphics Signal]
                       │
                       ▼
          [Browser Engine Process]
       (Updates prefers-contrast = more)
                       │
                       ▼
       [Recalculate Style & Layout Pass]
                       │
                       ▼
  Styles inside `@media (prefers-contrast: more)` apply instantly!
```

Because CSS custom properties can be mapped inside these media queries, the entire website re-themes dynamically with zero page reloads and zero JavaScript execution.

---

## 4. Syntax & Usage

### 1. Basic Media Query Syntax
```css
/* Dark Mode */
@media (prefers-color-scheme: dark) {
  :root {
    --bg-page: #0f172a;
    --text-primary: #f8fafc;
  }
}

/* Increased Contrast */
@media (prefers-contrast: more) {
  :root {
    --text-secondary: #000000;
    --border-card: 2px solid #000000;
  }
}

/* Reduced Transparency */
@media (prefers-reduced-transparency: reduce) {
  .frosted-glass-nav {
    backdrop-filter: none;
    background-color: #ffffff;
  }
}

/* Data Saver */
@media (prefers-reduced-data: reduce) {
  .hero-banner {
    background-image: none;
    background-color: #1e3a8a;
  }
}
```

---

## 5. Practical Examples

### Example 1: Holistic Multi-Tiered Preference Theme Engine
Architecting a unified, tokenized stylesheet that responds harmoniously to color scheme, contrast preferences, and transparency preferences simultaneously:

```css
:root {
  color-scheme: light dark;

  /* Default Light Tokens */
  --surface-canvas: #ffffff;
  --surface-card: #f8fafc;
  --surface-card-border: 1px solid #e2e8f0;
  --text-primary: #1e293b;
  --text-muted: #64748b;
  --brand-accent: #2563eb;
  --nav-backdrop: rgba(255, 255, 255, 0.8);
  --nav-blur: blur(12px);
}

/* Dark Mode Adaptation */
@media (prefers-color-scheme: dark) {
  :root {
    --surface-canvas: #090d16;
    --surface-card: #131b2e;
    --surface-card-border: 1px solid #1e293b;
    --text-primary: #f8fafc;
    --text-muted: #94a3b8;
    --brand-accent: #38bdf8;
    --nav-backdrop: rgba(9, 13, 22, 0.8);
  }
}

/* High Contrast Override (Applies to both light and dark) */
@media (prefers-contrast: more) {
  :root {
    --surface-card-border: 2px solid currentColor;
    --text-muted: var(--text-primary); /* Eliminate low-contrast secondary text */
    --brand-accent: #0000ee;
  }

  @media (prefers-color-scheme: dark) {
    :root {
      --brand-accent: #38bdf8;
    }
  }

  /* Force strong focus rings */
  :focus-visible {
    outline: 3px solid currentColor !important;
    outline-offset: 3px !important;
  }
}

/* Reduced Transparency Override */
@media (prefers-reduced-transparency: reduce) {
  :root {
    --nav-backdrop: var(--surface-canvas);
    --nav-blur: none;
  }
}
```

### Example 2: Frosted Glass UI with Transparency Fallback
Frosted glass (glassmorphism) creates severe legibility problems for users with low vision or astigmatism:

```html
<nav class="glass-navbar">
  <span class="logo">Acme Systems</span>
  <ul class="nav-links">
    <li><a href="#features">Features</a></li>
    <li><a href="#pricing">Pricing</a></li>
  </ul>
</nav>
```

```css
.glass-navbar {
  position: sticky;
  top: 0;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 2rem;
  z-index: 100;

  /* Default frosted glass */
  background-color: rgba(255, 255, 255, 0.7);
  backdrop-filter: blur(16px);
  border-bottom: 1px solid rgba(229, 231, 235, 0.8);
}

/* Respect user preference to remove blur/transparency */
@media (prefers-reduced-transparency: reduce) {
  .glass-navbar {
    background-color: #ffffff;
    backdrop-filter: none;
    border-bottom: 2px solid #d1d5db;
  }
}
```

### Example 3: Low Data Mode (`prefers-reduced-data`) Optimization
Save cellular data and mobile battery by disabling non-critical decorative assets when the user is in Data Saver mode:

```css
/* Standard high-resolution decorative background */
.hero-showcase {
  background-image: url("/assets/hero-bg-4k.webp");
  background-size: cover;
  background-position: center;
  min-height: 80vh;
  display: flex;
  align-items: center;
  justify-content: center;
}

/* When the user enabled OS Data Saver */
@media (prefers-reduced-data: reduce) {
  .hero-showcase {
    /* Drop heavy 4K raster image in favor of lightweight CSS gradient */
    background-image: linear-gradient(135deg, #1e3a8a, #0f172a);
  }

  /* Suppress secondary decorative web fonts */
  .decorative-subheading {
    font-family: system-ui, -apple-system, sans-serif !important;
  }

  /* Stop infinite CSS background animations that consume GPU memory */
  .animated-particles {
    display: none;
  }
}
```

### Example 4: HTML `Save-Data` Client Hint Integration
In addition to CSS media queries, browsers send the `Save-Data: on` HTTP request header. Combine both for optimal asset delivery:

```html
<!-- Load high-res video only when data saver is NOT active -->
<picture>
  <source media="(prefers-reduced-data: reduce)" srcset="/img/chart-static.png">
  <img src="/img/chart-interactive.webp" alt="Quarterly performance chart" loading="lazy">
</picture>
```

### Example 5: High Contrast Mode Button Differentiation
Ensuring button variants remain distinguishable when background colors are stripped or inverted:

```css
.btn {
  padding: 0.625rem 1.25rem;
  border-radius: 0.375rem;
  font-weight: 600;
  cursor: pointer;
  border: 1px solid transparent;
}

.btn--primary {
  background-color: #2563eb;
  color: #ffffff;
}

.btn--secondary {
  background-color: #f1f5f9;
  color: #0f172a;
  border-color: #cbd5e1;
}

/* High contrast mode enhancements */
@media (prefers-contrast: more) {
  .btn--primary {
    background-color: #000000;
    color: #ffffff;
    border: 2px solid #000000;
  }

  .btn--secondary {
    background-color: #ffffff;
    color: #000000;
    border: 2px solid #000000;
  }
}
```

---

## 6. Best Practices

1. **Use Token-Based Cascades**: Define preference overrides by re-assigning CSS custom properties rather than rewriting duplicate component selectors across media blocks.
2. **Always Pair `prefers-color-scheme` with `color-scheme`**: Declare `color-scheme: light dark;` on `:root` so native form controls, scrollbars, and select dropdowns automatically inherit the OS theme.
3. **Enhance Borders in `prefers-contrast: more`**: Card containers, inputs, and button boundaries should upgrade from subtle 1px border or box-shadows to 2px solid high-contrast borders.
4. **Remove Subtle Muted Text in High Contrast**: Reassign secondary/muted text tokens (`--text-secondary: #64748b`) directly to high-contrast body text (`--text-secondary: var(--text-primary)`) when `prefers-contrast: more` is active.
5. **Honor `prefers-reduced-transparency` on Overlays**: Modals, drop-down flyouts, and sticky headers must use 100% opaque background colors when transparency reduction is requested.
6. **Provide Lightweight CSS Fallbacks for Heavy Assets**: Replace video backgrounds and large 4K hero graphics with pure CSS linear gradients when `prefers-reduced-data: reduce` is detected.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Overriding OS dark mode without user opt-in** | Forcing light theme on photophobic users causes immediate eye strain. | Default to `prefers-color-scheme`; offer manual toggle that overrides it only on user action. |
| **Assuming `prefers-contrast: more` only means light mode** | Breaking high-contrast dark mode users by forcing white backgrounds. | Nest `prefers-contrast: more` within both light and dark scheme definitions. |
| **Using `display: none` on critical images in `prefers-reduced-data`** | Hides essential informational content from data-conscious users. | Replace heavy images with compressed versions or vector SVG illustrations; never hide content. |
| **Forgetting to test transparency reduction** | Frosted glass text overlays remain unreadable for users with astigmatism. | Test via Chrome DevTools Rendering panel: "Emulate CSS media feature prefers-reduced-transparency". |
| **Relying solely on JavaScript to listen for media query changes** | Causes layout flashes (FOUC) while JS initializes. | Use pure CSS media queries in stylesheets for instantaneous rendering. |
| **Hardcoding colors inside media queries instead of swapping tokens** | Explodes stylesheet size with repetitive component selector blocks. | Reassign CSS custom properties inside `@media (prefers-*)`. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Support Matrix
- **`prefers-color-scheme`**: Supported across all modern browsers (Chrome 76+, Safari 12.1+, Firefox 67+).
- **`prefers-contrast`**: Supported across Chrome 96+, Safari 14.1+, Firefox 89+.
- **`prefers-reduced-transparency`**: Supported across Chrome 118+, Safari 16.0+, Firefox 113+.
- **`prefers-reduced-data`**: Supported in Chrome/Chromium 85+ (experimental flag / partial Baseline).

### Emulation in Chrome & Firefox DevTools
1. Press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows) in Chrome DevTools.
2. Type "Rendering" and press Enter.
3. Scroll to the **Emulate CSS media feature** dropdowns:
   - `prefers-color-scheme`
   - `prefers-contrast`
   - `prefers-reduced-transparency`
   - `prefers-reduced-data`
   - `prefers-reduced-motion`

---

## 9. Comparison / When to Use What

| Media Feature | Trigger Event | Primary Visual Accommodation | Typical Impact |
| :--- | :--- | :--- | :--- |
| **`prefers-color-scheme`** | OS Dark/Light Mode | Background/text palette inversion | Aesthetic & photophobia support |
| **`prefers-contrast: more`** | OS "Increase Contrast" | Thicker borders, pure black/white, AAA contrast | Essential for low vision |
| **`prefers-contrast: less`** | OS "Reduce Contrast" | Muted, softer tones, lower glare | Visual comfort for sensory sensitivity |
| **`prefers-reduced-transparency`**| OS "Reduce Transparency" | Make glassmorphism / overlays 100% opaque | Astigmatism & dyslexia legibility |
| **`prefers-reduced-data`** | OS "Data Saver" | Strip heavy raster backgrounds & video | Mobile bandwidth & battery saving |

---

## 10. Interview Questions

### Q1: How do `prefers-color-scheme` and the CSS `color-scheme` property interact in modern browsers?
<details>
<summary>Answer</summary>
- **`prefers-color-scheme`**: A media query feature used to detect the operating system's visual theme preference. Author CSS uses it to selectively apply custom styling blocks or override custom properties.
- **`color-scheme`**: A CSS property (declared on `:root`) that formally informs the browser engine which color schemes the page supports:
  ```css
  :root { color-scheme: light dark; }
  ```
  When `color-scheme: light dark;` is present, the browser engine automatically switches user-agent default styles—including the canvas background, text color, native form controls (checkboxes, inputs, datepickers), and system scrollbars—to match the operating system theme without writing any custom CSS.
</details>

### Q2: What specific design adjustments should be implemented when `@media (prefers-contrast: more)` matches?
<details>
<summary>Answer</summary>
When a user enables "Increase Contrast" in their OS, stylesheets should:
1. **Eliminate Low-Contrast Muted Text**: Convert secondary gray copy (`#64748b`) to solid high-contrast text (`#000000` or `#ffffff`).
2. **Solidify Borders**: Replace subtle 1px border or box-shadow separations with 2px or 3px solid, high-contrast borders.
3. **Elevate Focus Ring Prominence**: Increase `:focus-visible` outline thickness and contrast (e.g. 3px solid with 3px offset).
4. **Clarify Interactive States**: Make hover and active states starkly distinct from idle states.
5. **Aim for WCAG AAA**: Elevate text contrast from AA (4.5:1) to AAA (7:1).
</details>

### Q3: Why is frosted glass (glassmorphism via `backdrop-filter: blur()`) an accessibility hazard, and how does CSS address it?
<details>
<summary>Answer</summary>
Frosted glass blurs the content underneath a floating header or modal while rendering the foreground text on a semi-transparent surface. For users with low vision, astigmatism, or visual processing disorders, the varying luminosity and high-frequency noise of the blurred background bleeding through the surface severely degrades contrast and readability.

The `prefers-reduced-transparency` media feature enables developers to detect when users have disabled transparency in macOS or Windows accessibility settings and replace frosted glass with a solid, opaque background color:
```css
@media (prefers-reduced-transparency: reduce) {
  .modal-backdrop { backdrop-filter: none; background: #ffffff; }
}
```
</details>

### Q4: How should a design system handle dark mode when the user requests both dark mode AND increased contrast (`prefers-contrast: more`)?
<details>
<summary>Answer</summary>
A common pitfall is assuming that `prefers-contrast: more` implies a light theme with black text on white. Many visually impaired users require both dark mode (to prevent glare and photophobia) and maximum contrast.

In CSS, nest or combine the media queries:
```css
@media (prefers-color-scheme: dark) and (prefers-contrast: more) {
  :root {
    --bg-canvas: #000000;         /* True pitch black, not dark gray */
    --text-primary: #ffffff;      /* Pure white, 21:1 contrast */
    --border-element: 2px solid #ffffff;
    --brand-accent: #38bdf8;
  }
}
```
This delivers a pure black-and-white inverted palette with maximum edge definition.
</details>

### Q5: How does `prefers-reduced-data` benefit users on mobile devices beyond reducing monetary cellular costs?
<details>
<summary>Answer</summary>
Beyond saving money on metered bandwidth plans, `prefers-reduced-data`:
1. **Accelerates Page Render Times**: Stripping multi-megabyte hero images and web font files drastically improves First Contentful Paint (FCP) and Largest Contentful Paint (LCP) on weak 3G/4G connections.
2. **Conserves Device Battery**: Downloading large payloads and decoding heavy compressed video or raster images causes intense CPU/GPU utilization, rapidly draining mobile batteries.
3. **Reduces Main-Thread Contention**: Fewer media elements mean lower memory consumption and smoother Interaction to Next Paint (INP).
</details>

### Q6: Can user preference media queries be emulated in automated testing suites (like Playwright or Cypress)?
<details>
<summary>Answer</summary>
Yes. In modern automated test runners like Playwright, user preference media features can be set programmatically per test context:
```javascript
// Playwright example
await page.emulateMedia({
  colorScheme: 'dark',
  reducedMotion: 'reduce'
});
```
Or via Chromium CDP commands to emulate `prefers-contrast`:
```javascript
const client = await page.context().newCDPSession(page);
await client.send('Emulation.setEmulatedMedia', {
  features: [{ name: 'prefers-contrast', value: 'more' }]
});
```
</details>

### Q7: What is the architectural drawback of writing component rules directly inside media queries versus modifying CSS custom properties?
<details>
<summary>Answer</summary>
Writing component rules directly inside media queries:
```css
/* Brittle Approach */
@media (prefers-color-scheme: dark) {
  .card { background: #111; color: #fff; }
  .btn { background: #222; }
  .modal { background: #111; }
}
```
causes massive code duplication, balloons stylesheet bundle size, and fragments component definitions across multiple files.

Using CSS Custom Properties:
```css
/* Resilient Token Architecture */
:root { --bg: #fff; --text: #000; }
@media (prefers-color-scheme: dark) { :root { --bg: #111; --text: #fff; } }

.card { background: var(--bg); color: var(--text); }
```
keeps component definitions unified and single-source, while media queries are confined strictly to redefining tokens at the root level.
</details>

### Q8: How should a website resolve a conflict between the user's OS preference (`prefers-color-scheme`) and an explicit theme selection in a site UI dropdown?
<details>
<summary>Answer</summary>
The industry standard pattern follows this hierarchy:
1. **Default State**: Follow the OS preference via `@media (prefers-color-scheme)`.
2. **User Explicit Choice**: When a user clicks a theme toggle in the UI, persist their explicit choice in `localStorage` and set a corresponding attribute on `<html>` (e.g. `<html data-theme="dark">`).
3. **CSS Architecture**: Apply themes based on both:
   ```css
   :root { --bg: #fff; }
   @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg: #111; } }
   [data-theme="dark"] { --bg: #111; }
   [data-theme="light"] { --bg: #fff; }
   ```
This respects the system by default, but allows an explicit user override whenever requested.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a High-Contrast Card Adaptation
**Objective**: Style a content card that defaults to subtle modern styling (soft gray border, subtle shadow), but converts to high-contrast mode (2px solid black border, no shadow, bold text) when `prefers-contrast: more` is triggered.

<details>
<summary>Solution</summary>

```html
<article class="info-card">
  <h3 class="info-card__title">Security Audit Report</h3>
  <p class="info-card__desc">All API gateways successfully passed encryption compliance.</p>
  <a href="/reports" class="info-card__link">View Audit Details &rarr;</a>
</article>
```

```css
.info-card {
  --card-border: 1px solid #e2e8f0;
  --card-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
  --card-text-muted: #64748b;
  --card-bg: #ffffff;

  background: var(--card-bg);
  border: var(--card-border);
  box-shadow: var(--card-shadow);
  border-radius: 0.5rem;
  padding: 1.5rem;
  max-width: 400px;
}

.info-card__title {
  margin: 0 0 0.5rem;
  font-size: 1.25rem;
  color: #0f172a;
}

.info-card__desc {
  margin: 0 0 1rem;
  color: var(--card-text-muted);
  line-height: 1.5;
}

.info-card__link {
  color: #2563eb;
  font-weight: 600;
  text-decoration: underline;
}

.info-card__link:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

/* HIGH CONTRAST ADAPTATION */
@media (prefers-contrast: more) {
  .info-card {
    --card-border: 2px solid #000000;
    --card-shadow: none; /* Remove soft shadows in favor of crisp borders */
    --card-text-muted: #000000; /* Upgrade muted text to pure black */
  }

  .info-card__link {
    color: #0000ee;
    font-weight: 700;
    text-decoration-thickness: 2px;
  }

  .info-card__link:focus-visible {
    outline: 3px solid #000000;
    outline-offset: 3px;
  }
}
```
</details>

### Exercise 2: Responsive Image Delivery for Data Saver Users
**Objective**: Write CSS and HTML markup that replaces a 2MB animated GIF / video background with a clean CSS gradient when the user has enabled Data Saver mode.

<details>
<summary>Solution</summary>

```html
<section class="hero-header">
  <div class="hero-content">
    <h1>Enterprise Data Intelligence</h1>
    <p>Real-time analytics across distributed clusters.</p>
  </div>
</section>
```

```css
.hero-header {
  min-height: 60vh;
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  color: #ffffff;
  padding: 2rem;
  background-image: url("/assets/network-animation.webp");
  background-size: cover;
  background-position: center;
  background-color: #0f172a;
}

@media (prefers-reduced-data: reduce) {
  .hero-header {
    /* Strip heavy raster file download */
    background-image: radial-gradient(circle at center, #1e3a8a, #0f172a);
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`prefers-color-scheme`**: `light` vs `dark`. Always declare `color-scheme: light dark;` on `:root`.
- **`prefers-contrast: more`**: Elevate borders from 1px to 2px solid, eliminate muted text, remove soft box-shadows, and target 7:1 (AAA) contrast.
- **`prefers-contrast: less`**: Soften stark contrasts to alleviate eye strain and sensory sensitivity.
- **`prefers-reduced-transparency: reduce`**: Disable `backdrop-filter: blur()` and make semi-transparent navigation or modal overlays 100% opaque.
- **`prefers-reduced-data: reduce`**: Suppress heavy raster background images, non-critical web fonts, and autoplaying videos.
- **Architecture**: Keep media queries restricted to redefining root CSS custom properties; do not duplicate component selector declarations.

---

## 13. Further Reading
- [W3C Media Queries Level 5: User Preference Media Features](https://www.w3.org/TR/mediaqueries-5/#user-preferences)
- [MDN Web Docs: prefers-contrast](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-contrast)
- [MDN Web Docs: prefers-reduced-transparency](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-transparency)
- [web.dev: Adapting to user preferences](https://web.dev/learn/design/responsive-design#adapting-to-user-preferences)
- [Smashing Magazine: Designing For Contrast Preferences](https://www.smashingmagazine.com/2022/07/designing-for-contrast-preferences/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [02. Colour Contrast and Colour Independence](02-colour-contrast-and-colour-independence.md) | [04. Forced Colors and High Contrast Mode](04-forced-colors-and-high-contrast-mode.md) |
