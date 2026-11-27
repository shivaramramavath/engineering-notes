# Dark Mode and Multi-Theme Architecture

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Color Module Level 5 / Media Queries Level 5 / Architecture Best Practices |
| **Baseline Status** | Universal Standards & Modern Baseline 2024 Integration |
| **Target Audience** | Frontend Architects, Full-Stack Engineers, Design System Leads |
| **Tested With** | Next.js SSR, Vite SPA, Tailwind, Pure Native CSS Design Systems |

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

Today, dark mode and custom brand themes are no longer optional novelty features; they are expected core user experience requirements:
- **Visual Ergonomics**: Dark interfaces reduce glare, alleviate eye strain in low-light environments, and lower device battery consumption on OLED displays.
- **Accessibility**: Users with photophobia, cataracts, or sensory processing disorders rely heavily on high-contrast dark themes to browse comfortably.
- **Multi-Tenant SaaS Platforms**: Enterprise applications frequently must brand their software differently for white-label corporate clients (e.g. Acme Corp Blue vs Globex Green) from a single shared codebase.

However, poorly architected theming systems introduce critical bugs:
- **Flash of Wrong Theme (FOWT)**: The page briefly flashes bright white during Server-Side Rendering (SSR) before flipping to dark mode.
- **CSS Code Bloat**: Maintaining duplicate stylesheets for light and dark modes doubles CSS bundle size.
- **Unintended Inversions**: Sub-components accidentally inherit broken background and text contrast combinations.

A modern theming architecture leverages **CSS Custom Properties, `color-scheme`, and `light-dark()`** to provide zero-boilerplate, multi-tenant theming that updates instantaneously with zero flash.

---

## 2. Core Concepts

### The Three Theming States
A robust user-facing theme switcher must accommodate three distinct states:
1. **System / Auto**: Follows the user's operating system setting (`prefers-color-scheme`).
2. **Forced Light**: Explicitly locked to light mode regardless of OS setting.
3. **Forced Dark**: Explicitly locked to dark mode regardless of OS setting.

### The Role of `data-theme` and `color-scheme`
Instead of scattering `.dark` classes across thousands of individual DOM elements, the application orchestrates themes at the root:
```html
<html data-theme="dark">
```
```css
:root {
  color-scheme: light dark;
}

html[data-theme="light"] {
  color-scheme: light;
}

html[data-theme="dark"] {
  color-scheme: dark;
}
```
Setting `color-scheme` automatically updates native form controls, scrollbars, and informs the modern `light-dark()` CSS function.

### Preventing Flash of Wrong Theme (FOWT) in SSR/SSG
When HTML is rendered by a server (e.g. Next.js, Nuxt, Astro), the server cannot know the user's client-side `localStorage` theme preference. If the server delivers default light HTML and a client script flips to dark mode 200ms later, the user experiences a blinding white flash.

**The Solution**: An inline blocking `<script>` placed in the `<head>` *before* any stylesheet or body renders. It reads `localStorage` or `window.matchMedia` and sets `document.documentElement.dataset.theme = ...` synchronously before the browser paints frame 0.

### Subtree Theme Inversion
In complex layouts, specific sections must remain locked to an inverted theme (e.g. an always-dark navigation drawer or always-dark code snippet inside a light-themed blog). This is achieved cleanly using CSS Custom Properties and `color-scheme: dark`.

---

## 3. How It Works Internally

```
Theme Resolution and Preventing FOWT (Flash of Wrong Theme):

  1. Client requests page from Server.
  2. Server responds with HTML containing blocking <head> script.
  3. Browser begins parsing <head>:
     ┌────────────────────────────────────────────────────────┐
     │ Inline Blocking Script in <head>:                     │
     │   const theme = localStorage.getItem('theme') ||       │
     │     (window.matchMedia('(prefers-color-scheme: dark)') │
     │      .matches ? 'dark' : 'light');                     │
     │   document.documentElement.dataset.theme = theme;      │
     └────────────────────────────────────────────────────────┘
  4. Browser parses CSSOM:
     - html[data-theme="dark"] evaluates to TRUE before body is painted!
  5. Frame 0 Painted:
     - Rendered in dark mode on the very first paint! Zero flash!
```

```
Theming Architecture Layer Matrix:
┌─────────────────────┬────────────────────────────────┬──────────────────────────────┐
│ Layer               │ Implementation                 │ Role                         │
├─────────────────────┼────────────────────────────────┼──────────────────────────────┤
│ 1. Detection        │ `@media (prefers-color-scheme)`│ Reads OS level setting       │
│ 2. Storage          │ `localStorage.setItem('theme')`│ Persists explicit user choice│
│ 3. Dispatch         │ `html[data-theme="..."]`       │ Global CSS selector anchor   │
│ 4. Consumption      │ `light-dark()` or Semantic CSS │ Automatically updates styles │
└─────────────────────┴────────────────────────────────┴──────────────────────────────┘
```

---

## 4. Syntax & Usage

### Modern Pure CSS Adaptive Theming
```css
:root {
  /* 1. Declare native color scheme support */
  color-scheme: light dark;

  /* 2. Semantic tokens with light-dark() */
  --bg-canvas:        light-dark(#ffffff, #090d16);
  --bg-surface:       light-dark(#f8fafc, #131b2e);
  --border-subtle:    light-dark(#e2e8f0, #222f4c);
  --text-primary:     light-dark(#0f172a, #f8fafc);
  --text-secondary:   light-dark(#475569, #94a3b8);
  --brand-primary:    light-dark(#2563eb, #3b82f6);
}

/* User Explicit Overrides */
html[data-theme="light"] { color-scheme: light; }
html[data-theme="dark"]  { color-scheme: dark; }

/* Global Element Consumption */
body {
  background-color: var(--bg-canvas);
  color: var(--text-primary);
  font-family: system-ui, sans-serif;
  margin: 0;
}
```

---

## 5. Practical Examples

### Example 1: Zero-FOUC Blocking Head Script
The canonical inline script to insert into `<head>` to prevent flash of unstyled theme in production web applications.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>App</title>

  <!-- CRITICAL: Inline script before stylesheets prevents FOWT -->
  <script>
    (function() {
      try {
        const storedTheme = localStorage.getItem('app-theme');
        const systemDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
        const theme = storedTheme || (systemDark ? 'dark' : 'light');
        document.documentElement.dataset.theme = theme;
      } catch (e) {}
    })();
  </script>

  <link rel="stylesheet" href="/styles/main.css">
</head>
<body>
  ...
</body>
</html>
```

### Example 2: Multi-Tenant Enterprise Theming (Customer Brands)
Switching visual identities dynamically for multi-tenant enterprise customers.

```html
<!-- Customer: Apex Financial (Navy & Crimson) -->
<body data-tenant="apex">
  <header class="c-header">Apex Analytics</header>
  <button class="c-btn">Export Report</button>
</body>

<!-- Customer: EcoPulse Energy (Emerald & Amber) -->
<body data-tenant="ecopulse">
  <header class="c-header">EcoPulse Grid</header>
  <button class="c-btn">Export Report</button>
</body>
```

```css
:root {
  /* Default Tenant */
  --tenant-brand: #2563eb;
  --tenant-accent: #3b82f6;
  --tenant-radius: 0.375rem;
}

/* Tenant 1: Apex Financial */
body[data-tenant="apex"] {
  --tenant-brand: #1e3a8a;
  --tenant-accent: #be123c;
  --tenant-radius: 0.125rem;
}

/* Tenant 2: EcoPulse Energy */
body[data-tenant="ecopulse"] {
  --tenant-brand: #059669;
  --tenant-accent: #d97706;
  --tenant-radius: 9999px;
}

/* Component consumes tenant variables */
.c-btn {
  background-color: var(--tenant-brand);
  border-radius: var(--tenant-radius);
  color: white;
  padding: 0.5rem 1.25rem;
  border: none;
  cursor: pointer;

  &:hover {
    background-color: var(--tenant-accent);
  }
}
```

### Example 3: Always-Dark Code Snippet Block (Subtree Inversion)
A syntax-highlighted code block that remains permanently dark inside a light application.

```html
<article class="doc-post">
  <h2>Using Cascade Layers</h2>
  <p>Cascade layers allow developers to control specificity explicitly.</p>

  <!-- Permanently Dark Subtree -->
  <div class="code-window">
    <div class="code-header">terminal.bash</div>
    <pre><code>npm install @design-system/tokens</code></pre>
  </div>
</article>
```

```css
:root {
  color-scheme: light dark;
  --bg-card: light-dark(#ffffff, #1e293b);
  --text-card: light-dark(#0f172a, #f8fafc);
}

.doc-post {
  background-color: var(--bg-card);
  color: var(--text-card);
  padding: 2rem;
}

/* Force code block to be ALWAYS dark */
.code-window {
  color-scheme: dark;
  background-color: #0f172a;
  color: #f8fafc;
  border-radius: 0.5rem;
  overflow: hidden;
  border: 1px solid #334155;

  .code-header {
    background-color: #1e293b;
    padding: 0.5rem 1rem;
    font-size: 0.75rem;
    color: #94a3b8;
    border-bottom: 1px solid #334155;
  }

  pre {
    margin: 0;
    padding: 1rem;
    font-family: 'JetBrains Mono', monospace;
  }
}
```

### Example 4: Tri-State Accessible Toggle Controller
A complete semantic accessible radio button toggle for switching between Light, Dark, and System modes.

```html
<fieldset class="theme-picker" role="radiogroup" aria-label="Select color theme">
  <legend class="sr-only">Color Theme</legend>
  <label>
    <input type="radio" name="theme-opt" value="light" class="theme-radio">
    <span>Light</span>
  </label>
  <label>
    <input type="radio" name="theme-opt" value="dark" class="theme-radio">
    <span>Dark</span>
  </label>
  <label>
    <input type="radio" name="theme-opt" value="auto" checked class="theme-radio">
    <span>System</span>
  </label>
</fieldset>
```

```css
.theme-picker {
  display: inline-flex;
  gap: 0.5rem;
  padding: 0.25rem;
  background-color: light-dark(#f1f5f9, #1e293b);
  border-radius: 2rem;
  border: 1px solid light-dark(#cbd5e1, #334155);

  label {
    display: flex;
    align-items: center;
    cursor: pointer;
  }

  span {
    padding: 0.375rem 0.75rem;
    font-size: 0.8125rem;
    font-weight: 500;
    border-radius: 1.5rem;
    color: light-dark(#64748b, #94a3b8);
    transition: background-color 0.15s ease, color 0.15s ease;
  }

  .theme-radio {
    position: absolute;
    opacity: 0;
    width: 0;
    height: 0;
  }

  .theme-radio:checked + span {
    background-color: light-dark(#ffffff, #0f172a);
    color: light-dark(#0f172a, #f8fafc);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
  }

  .theme-radio:focus-visible + span {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }
}
```

### Example 5: High-Contrast Mode Synergy (`forced-colors`)
Harmonizing dark mode architecture with Windows High Contrast Mode.

```css
@media (forced-colors: active) {
  /* Ensure transparent buttons render visible borders in high contrast mode */
  .c-btn {
    forced-color-adjust: none;
    border: 2px solid ButtonText !important;
    background-color: ButtonFace !important;
    color: ButtonText !important;
  }
}
```

---

## 6. Best Practices

1. **Avoid pitch-black (`#000000`) backgrounds for dark mode**: Pure black against pure white text creates extreme contrast ($21:1$) that induces optical halation and eye fatigue. Use dark slate grays (`#0f172a`, `#121212`, or `oklch(0.18 0.02 250)`).
2. **Always include the inline `<head>` script to prevent FOWT**: Never wait for an asynchronous React `useEffect` or external bundle to set the theme in `localStorage`.
3. **Declare `color-scheme: light dark` on `:root`**: Informs the browser to adapt form inputs, datepickers, and scrollbars automatically.
4. **Desaturate vibrant colors in dark mode**: Highly saturated brand colors (e.g. pure neon blue or bright red) vibrate against dark backgrounds. Lower the chroma ($C$) in dark mode for better legibility.
5. **Never swap themes by duplicating whole CSS files**: Theme switching should modify CSS custom properties at the root, not re-download a `dark.css` stylesheet over the network.
6. **Ensure interactive states have dark mode focus rings**: Default browser focus outlines often become invisible against dark backgrounds. Use `light-dark()` to guarantee visible focus contrast.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Setting theme via `useEffect` in React/Next.js | Blinding white flash (FOWT) on every page refresh for dark mode users. | Inject blocking inline script in `<head>` before body render. |
| Hardcoding `#ffffff` in component backgrounds | Components remain bright white when page is in dark mode. | Use semantic tokens: `background: var(--bg-surface)`. |
| Using pure black (`#000`) with pure white (`#fff`) text | Causes severe visual halation, glare, and eye strain. | Use dark gray backgrounds (e.g. `#131b2e`, `#121212`). |
| Forgetting native scrollbars in dark mode | Bright white scrollbar blinds users on dark pages. | Add `color-scheme: light dark;` to `:root`. |
| Neglecting image contrast in dark mode | Bright white diagrams or logos blind dark mode users. | Use `filter: invert(1)` or opacity dims on non-essential imagery. |
| Over-saturating primary buttons in dark mode | Neon colors cause visual vibration and eye discomfort. | Desaturate chroma slightly in dark mode. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Instantaneous DOM Mutation**: Modifying custom properties on `<html>` updates all component styling within a single paint frame in native browser C++, avoiding Virtual DOM reconciliation.
- **Zero Additional Network Requests**: All theme variants reside in the same stylesheet as CSS custom properties.

### Accessibility (A11y)
- **WCAG 2.2 Contrast Verification**: Verify that both light and dark variations satisfy WCAG AA minimum contrast ($4.5:1$ for regular text, $3.0:1$ for large text and UI components).
- **Photosensitive Comfort**: Respecting `prefers-color-scheme` provides immediate relief to users with migraines or light-triggered sensory conditions.

### Browser Compatibility
- **Baseline 2024**: Full cross-browser support for `color-scheme` and `light-dark()` across Chrome 123+, Safari 17.5+, and Firefox 120+.

---

## 9. Comparison / When to Use What

| Theming Pattern | FOWT Risk | Bundle Overhead | Maintenance Effort | Subtree Isolation? |
| :--- | :--- | :--- | :--- | :--- |
| **`light-dark()` + CSS Variables** | **Zero (with script)** | **0 KB extra** | **Very Low** | **Yes (`color-scheme`)** |
| **Separate `dark.css` File** | High (Network delay) | High (2x CSS) | High | No |
| **CSS-in-JS `ThemeProvider`** | Medium (Hydration lag)| Heavy JS runtime | High | Complex prop drilling |
| **Pure `@media` queries only** | Zero | Low | Low | No (Cannot manually toggle) |

---

## 10. Interview Questions

### Q1: What causes Flash of Wrong Theme (FOWT), and how is it eliminated in modern web engineering?
<details>
<summary>View Answer</summary>
FOWT occurs when an application renders its default theme (typically light mode) on the initial HTML paint, and then a client-side JavaScript bundle (such as a React `useEffect` or DOM ready listener) reads `localStorage` and toggles a dark class moments later. The user experiences a jarring visual flash. It is eliminated by placing a tiny, synchronous blocking inline script inside the document's `<head>` *before* any stylesheets or HTML body elements. The script reads `localStorage` or `window.matchMedia` and sets `document.documentElement.dataset.theme = ...` synchronously before the browser renders frame 0.
</details>

### Q2: Why is pure black (`#000000`) generally discouraged as a dark mode background color?
<details>
<summary>View Answer</summary>
Pure black produces a maximum mathematical contrast ratio ($21:1$) against pure white text. This extreme contrast induces an optical phenomenon called "halation," where light letters appear to bleed or blur into the background, causing severe eye strain, particularly for users with astigmatism. Dark UI design systems recommend using dark gray or slate backgrounds (e.g. `#121212` or `#0f172a`), which provide ample contrast while remaining comfortable for extended reading.
</details>

### Q3: How does `color-scheme: light dark` affect native browser elements?
<details>
<summary>View Answer</summary>
Declaring `color-scheme: light dark` informs the browser's User Agent (UA) styling engine that the document supports both themes. The browser automatically renders native scrollbars, form inputs (`<input>`, `<textarea>`, `<select>`), calendar pickers, and text selection highlights in dark mode when dark mode is active, eliminating the need for third-party scrollbar or form restyling libraries.
</details>

### Q4: How does `light-dark()` improve over traditional `@media (prefers-color-scheme: dark)` blocks?
<details>
<summary>View Answer</summary>
`light-dark()` simplifies theming by co-locating light and dark values in a single declaration (`background: light-dark(#fff, #0f172a)`), eliminating the need to write separate media queries that duplicate selectors. Furthermore, `light-dark()` responds to the CSS `color-scheme` property, allowing localized subtrees (such as an always-dark sidebar) to be created simply by setting `color-scheme: dark` on that container.
</details>

### Q5: What is "Subtree Inversion" in theming architecture?
<details>
<summary>View Answer</summary>
Subtree inversion occurs when a specific component or section inside a webpage is deliberately rendered in the opposite theme of the parent page (e.g. a permanently dark syntax-highlighted code block inside a light-themed blog, or a light modal on a dark background). This is achieved cleanly in modern CSS by applying `color-scheme: dark` or re-mapping semantic tokens directly on that container's selector.
</details>

### Q6: Why should saturated brand colors be modified or desaturated in dark mode?
<details>
<summary>View Answer</summary>
Vibrant, saturated colors that look great on white backgrounds (such as deep royal blue or neon red) create visual vibration ("chromatic aberration") against dark backgrounds, making text difficult to read and causing optical fatigue. In dark mode, design systems typically lighten the lightness ($L$) and reduce the chroma ($C$) to produce softer, more legible pastel tones.
</details>

### Q7: How do you support a "System" mode choice alongside explicit "Light" and "Dark" in UI pickers?
<details>
<summary>View Answer</summary>
When the user selects "System", the application deletes the stored theme from `localStorage` (`localStorage.removeItem('app-theme')`) and removes the explicit `data-theme` attribute from `<html>`. The browser then naturally falls back to evaluating `color-scheme: light dark` based on the operating system's `prefers-color-scheme` media query.
</details>

### Q8: What role does `window.matchMedia` play in theming scripts?
<details>
<summary>View Answer</summary>
`window.matchMedia('(prefers-color-scheme: dark)')` allows JavaScript to evaluate the user's OS-level dark mode preference programmatically (`.matches`). It also provides an event listener (`.addEventListener('change', ...)`) that allows the application to update themes on the fly if the user changes their OS settings while the website is open.
</details>

---

## 11. Practice Exercises

### Exercise 1: Zero-FOUC Head Script Implementation
**Objective**: Write the exact minimal inline JavaScript snippet to place in `<head>` to prevent flash of wrong theme.
<details>
<summary>View Solution</summary>

```html
<script>
  (function() {
    try {
      const saved = localStorage.getItem('theme');
      const isDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
      document.documentElement.setAttribute('data-theme', saved || (isDark ? 'dark' : 'light'));
    } catch (e) {}
  })();
</script>
```
</details>

### Exercise 2: Declarative Root Theme Tokens
**Objective**: Declare a complete semantic palette in pure CSS using `color-scheme` and `light-dark()`.
<details>
<summary>View Solution</summary>

```css
:root {
  color-scheme: light dark;

  --bg-page:        light-dark(#ffffff, #090d16);
  --bg-card:        light-dark(#f8fafc, #131b2e);
  --border-color:   light-dark(#e2e8f0, #1e293b);
  --text-primary:   light-dark(#0f172a, #f8fafc);
  --text-muted:     light-dark(#64748b, #94a3b8);
}

html[data-theme="light"] { color-scheme: light; }
html[data-theme="dark"]  { color-scheme: dark; }
```
</details>

### Exercise 3: Subtree Always-Dark Navigation Drawer
**Objective**: Ensure that `.side-drawer` is permanently dark, regardless of whether the document is in light or dark mode.
<details>
<summary>View Solution</summary>

```css
.side-drawer {
  color-scheme: dark;
  background-color: #0f172a;
  color: #f8fafc;
  border-right: 1px solid #1e293b;
  padding: 1.5rem;
}
```
</details>

### Exercise 4: White-Label Customer Brand Tokens
**Objective**: Configure multi-brand tokens where client "Zenith" uses purple brand accents and pill buttons.
<details>
<summary>View Solution</summary>

```css
body[data-client="zenith"] {
  --brand-primary: #7c3aed;
  --brand-hover: #6d28d9;
  --btn-radius: 9999px;
}
```
</details>

### Exercise 5: Desaturate Primary Brand for Dark Mode
**Objective**: Use OKLCH Relative Color Syntax to automatically soften brand chroma in dark mode.
<details>
<summary>View Solution</summary>

```css
:root {
  --seed-brand: oklch(0.6 0.24 250);

  /* Light mode: full saturation; Dark mode: reduce chroma and boost lightness */
  --brand-interactive: light-dark(
    var(--seed-brand),
    oklch(from var(--seed-brand) 0.7 calc(c * 0.7) h)
  );
}
```
</details>

### Exercise 6: Contrast-Safe Dark Mode Focus Ring
**Objective**: Provide an adaptive focus ring that shifts to cyan in dark mode for maximum visibility.
<details>
<summary>View Solution</summary>

```css
.interactive-element:focus-visible {
  outline: 2px solid light-dark(#2563eb, #38bdf8);
  outline-offset: 2px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Core Adaptive Architecture */
:root {
  color-scheme: light dark;
  --bg-page:   light-dark(#ffffff, #090d16);
  --text-main: light-dark(#0f172a, #f8fafc);
}

/* Explicit User Overrides */
html[data-theme="light"] { color-scheme: light; }
html[data-theme="dark"]  { color-scheme: dark; }

/* Subtree Inversion */
.always-dark-box { color-scheme: dark; }
```

- Prevent FOWT with a blocking inline script in `<head>`.
- Never use pitch black (`#000`); use dark slate grays (`#121212`, `#0f172a`).
- Use `color-scheme: light dark` to automatically style native form controls and scrollbars.
- Baseline 2024 (Supported in Chrome 123+, Safari 17.5+, Firefox 120+).

---

## 13. Further Reading

- [web.dev: Building a theme switch component](https://web.dev/building-a-theme-switch-component/)
- [Josh W. Comeau: The Quest for the Perfect Dark Mode](https://www.joshwcomeau.com/react/dark-mode/)
- [MDN Web Docs: prefers-color-scheme](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-color-scheme)
- [W3C CSS Color Module Level 5: light-dark()](https://drafts.csswg.org/css-color-5/#light-dark)

---

### Navigation
- **Previous**: [06-design-tokens-and-theming.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/06-design-tokens-and-theming.md)
- **Next**: [08-resets-and-normalisation.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/08-resets-and-normalisation.md)
