# `@supports` and Progressive Enhancement in Modern CSS

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Conditional Rules Module Level 4 (W3C Candidate Recommendation) |
| **Baseline Status** | Widely Available (Supported across all modern browsers; selector() added in 2021) |
| **Browsers Tested** | Chrome 90+, Safari 14.1+, Firefox 69+, Edge 90+ |
| **Tested With** | W3C Web Platform Tests, Playwright Feature Emulation, BrowserStack |

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

The web is an open, heterogeneous environment. Users access websites across thousands of different devices, operating systems, and browser versions. When adopting cutting-edge CSS features—such as Subgrid, Container Queries, `:has()`, Anchor Positioning, or `@scope`—developers face a dilemma:
- How do we leverage modern CSS capabilities without breaking experiences for users on older browsers?

Historically, developers relied on **User-Agent sniffing** (brittle and spoofable) or client-side JavaScript feature detection libraries like **Modernizr** (which added bundle weight, delayed rendering, and caused flash-of-unstyled-content).

The **`@supports`** at-rule (CSS Feature Queries) brings native, high-performance feature detection directly into the stylesheet. Rather than guessing capabilities based on browser names, `@supports` allows CSS to query whether the executing browser engine actually supports a specific property, selector, or font technology before applying progressive enhancement styles.

---

## 2. Core Concepts

### Property Queries
Tests whether the browser supports a specific CSS property and value pair:
```css
@supports (display: subgrid) {
  .subgrid-card {
    grid-template-rows: subgrid;
  }
}
```

### Selector Queries: `@supports selector(...)`
Tests whether a specific selector syntax or pseudo-class is valid and supported:
```css
@supports selector(:has(img)) {
  /* Only runs if :has() is supported */
  .card:has(img) {
    grid-template-columns: 200px 1fr;
  }
}
```

### Font Technology Queries: `font-tech()` and `font-format()`
Tests support for modern font formats and features (such as Variable Fonts or Color Vector Fonts):
```css
@supports font-tech(variations) {
  @font-face {
    font-family: 'Inter';
    src: url('inter-variable.woff2') format('woff2');
  }
}
```

### Logical Operators (`and`, `or`, `not`)
Combine multiple conditions using boolean logic:
```css
@supports (display: grid) and (not (display: subgrid)) {
  /* Browser supports Grid, but lacks Subgrid */
}
```

### Progressive Enhancement vs. Graceful Degradation
- **Graceful Degradation**: Build for modern browsers first, then scramble to fix or patch older browsers with hacks.
- **Progressive Enhancement**: Provide a solid, functional baseline experience that works on **all** browsers (even without modern CSS), then layer advanced enhancements inside `@supports` for browsers that support them.

---

## 3. How It Works Internally

```
@supports Parser Evaluation Pipeline:

  1. CSS Parser encounters '@supports (display: subgrid)'
  2. Engine queries internal CSS Property Registry:
     - Is 'display' recognized? YES.
     - Is 'subgrid' valid for 'display'?
        - In modern engine: Evaluates to TRUE.
        - In legacy engine: Evaluates to FALSE.
  3. Conditional Tree Branching:
     - If TRUE: Rules inside block are parsed and inserted into the active CSSOM.
     - If FALSE: The entire block is skipped at zero computation cost.
```

```
Progressive Enhancement Layering Strategy:
┌────────────────────────────────────────────────────────┐
│ Layer 3: Cutting-Edge Modern Layer                     │
│   @supports (anchor-name: --a) { Anchor Positioning }  │
├────────────────────────────────────────────────────────┤
│ Layer 2: Progressive Enhancement Layer                 │
│   @supports (display: grid) { Multi-column CSS Grid }  │
├────────────────────────────────────────────────────────┤
│ Layer 1: Universal Semantic Baseline                   │
│   Standard Normal Flow / Flexbox / Single Column       │
│   (Guaranteed accessible on EVERY browser!)            │
└────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline Availability:
├── Chrome / Edge: 90+ (Full support for property & selector queries)
├── Safari: 14.1+ (Full support including @supports selector)
└── Firefox: 69+ (Full support)
* Property queries widely available across all browsers since 2013.
```

### Basic Syntax Forms
```css
/* 1. Property-Value query */
@supports (property: value) {
  /* Declarations */
}

/* 2. Negation query */
@supports not (property: value) {
  /* Fallback declarations */
}

/* 3. Selector query */
@supports selector(:pseudo-class) {
  /* Declarations */
}

/* 4. Combined Boolean queries */
@supports (transform: scale(1)) and (display: flex) {
  /* Declarations */
}
```

---

## 5. Practical Examples

### Example 1: Progressive Grid to Subgrid Card Alignment
Provide clean independent card rows by default, enhanced with Subgrid when supported.

```html
<div class="card-grid">
  <article class="card">
    <h3>Title</h3>
    <p>Short body.</p>
    <button>Action</button>
  </article>
</div>
```

```css
/* Baseline: Standard Flexbox Card */
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(18rem, 1fr));
  gap: 1.5rem;
}

.card {
  display: flex;
  flex-direction: column;
  padding: 1.5rem;
  border: 1px solid #cbd5e1;
  border-radius: 0.5rem;

  p {
    flex-grow: 1; /* Pushes button to bottom in baseline */
  }
}

/* Progressive Enhancement: Subgrid */
@supports (grid-template-rows: subgrid) {
  .card-grid {
    grid-auto-rows: auto auto 1fr auto;
  }

  .card {
    display: grid;
    grid-template-rows: subgrid;
    grid-row: span 4;

    p {
      flex-grow: initial;
    }
  }
}
```

### Example 2: Progressive Theming with `light-dark()`
Use standard CSS custom properties for legacy browsers, enhanced with `light-dark()` in modern engines.

```css
:root {
  /* Baseline: light theme default */
  --bg-color: #ffffff;
  --text-color: #0f172a;
}

/* Legacy Dark Mode Query */
@media (prefers-color-scheme: dark) {
  :root {
    --bg-color: #0f172a;
    --text-color: #f8fafc;
  }
}

/* Modern Zero-Overhead light-dark() Enhancement */
@supports (color: light-dark(black, white)) {
  :root {
    color-scheme: light dark;
    --bg-color: light-dark(#ffffff, #0f172a);
    --text-color: light-dark(#0f172a, #f8fafc);
  }
}

body {
  background-color: var(--bg-color);
  color: var(--text-color);
}
```

### Example 3: Relational Styling with `@supports selector(:has(...))`
Conditionally style an article header layout based on the availability of `:has()`.

```css
/* Baseline: Default text styling */
.article-header {
  padding: 2rem;
  background-color: #f8fafc;
}

/* Enhanced: Layout reacts dynamically to presence of hero imagery */
@supports selector(:has(img)) {
  .article-header:has(img) {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 2rem;
    align-items: center;
  }
}
```

### Example 4: Backdrop Blur Fallback
Provide an opaque background on browsers that do not support `backdrop-filter`.

```css
.frosted-glass-nav {
  /* Fallback: 95% solid background */
  background-color: rgba(15, 23, 42, 0.95);
  color: #ffffff;
  padding: 1rem;
}

/* Modern Enhancement: Translucent glass with blur */
@supports (backdrop-filter: blur(10px)) or (-webkit-backdrop-filter: blur(10px)) {
  .frosted-glass-nav {
    background-color: rgba(15, 23, 42, 0.65);
    backdrop-filter: blur(10px);
    -webkit-backdrop-filter: blur(10px);
  }
}
```

### Example 5: Native View Transitions Enhancement
Provide smooth view transitions on browsers supporting `@view-transition` while preserving instant page changes elsewhere.

```css
/* Baseline: Instant standard navigation */
body {
  margin: 0;
}

/* Modern Enhancement */
@supports (view-transition-name: root) {
  @view-transition {
    navigation: auto;
  }

  ::view-transition-old(root) {
    animation: 90ms ease-out both fadeOut;
  }

  ::view-transition-new(root) {
    animation: 210ms ease-in both fadeIn;
  }
}
```

---

## 6. Best Practices

1. **Always write baseline styles first outside `@supports`**: The stylesheet outside `@supports` should deliver a complete, accessible, legible experience. Modern features should only ever be additive.
2. **Use `@supports selector(...)` for pseudo-classes**: Never write `@supports (:has(p))`. Pseudo-classes and selectors must be wrapped inside `selector()`: `@supports selector(:has(p))`.
3. **Avoid excessive `@supports not (...)` chaining**: Writing separate "positive" and "negative" blocks doubles maintenance effort. Instead, declare the baseline styles globally and override them inside a single positive `@supports` block.
4. **Always wrap complex boolean queries in parentheses**: Each individual condition in `@supports` must be wrapped in its own parentheses: `@supports (a: 1) and (b: 2)`. Omitting parentheses causes a syntax error.
5. **Include vendor prefixes when querying transitional features**: When feature querying features like `backdrop-filter`, test both standard and webkit variants: `@supports (backdrop-filter: blur(5px)) or (-webkit-backdrop-filter: blur(5px))`.
6. **Ensure accessible contrast across both baseline and enhanced versions**: Verify that fallback colors maintain WCAG AA contrast just as rigorously as the enhanced gradients or blurs.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting parentheses: `@supports display: grid` | Syntax error; browser ignores the entire block. | Wrap in parentheses: `@supports (display: grid)`. |
| Testing selectors without `selector()`: `@supports (:has(p))` | Invalid syntax; fails in all browsers. | Write `@supports selector(:has(p))`. |
| Writing empty baseline and putting all styles in `@supports` | Users on older browsers or RSS readers see an unstyled broken page. | Provide baseline styles globally. |
| Forgetting parenthetical grouping with `not`: `@supports not (a: 1) and (b: 2)` | Ambiguous boolean precedence causes parsing failure. | Use explicit grouping: `@supports (not (a: 1)) and (b: 2)`. |
| Testing properties that browsers accept but don't implement | Rare edge cases where a browser recognizes a keyword but renders nothing. | Test specific combinations or use Web Platform verified queries. |
| Over-using `@supports` for simple cascade overrides | Unnecessary code bloat when standard cascade order would suffice. | Rely on the natural cascade (e.g. `color: red; color: oklch(...);`) when possible. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero Runtime Scripting**: Replaces heavy JavaScript feature-detection libraries (like Modernizr), saving bundle size and reducing page initialization time.
- **Instant Branching**: The browser parses and filters `@supports` blocks at compile time in C++, avoiding paint and layout recalculations.

### Accessibility (A11y)
- Progressive enhancement guarantees that users on assistive devices, text-based browsers (e.g. Lynx), reading modes, or legacy screen readers always receive functional semantic content.

### Browser Compatibility
- **Widely Available**: Basic `@supports` has been universally supported across all browsers since 2013–2015.
- **`@supports selector()`**: Supported in Chrome 90+, Safari 14.1+, Firefox 69+.
- **`@supports font-tech()`**: Supported in Chrome 106+, Safari 16+, Firefox 120+.

---

## 9. Comparison / When to Use What

| Technique | Execution Time | Bundle Overhead | JavaScript Needed? | FOUC Risk? |
| :--- | :--- | :--- | :--- | :--- |
| **CSS `@supports`** | **Compile Time (CSSOM)** | **0 KB** | **No** | **None** |
| **Cascade Fallback (2 declarations)** | Compile Time | 0 KB | No | None |
| **Modernizr (JavaScript)** | Runtime (Main Thread) | ~10–25 KB | Yes | High (FOUC) |
| **User-Agent Sniffing** | Server Time | Variable | Backend JS / PHP | High (Spoofable) |

---

## 10. Interview Questions

### Q1: What is the difference between progressive enhancement and graceful degradation in CSS architecture?
<details>
<summary>View Answer</summary>
Graceful degradation designs an experience for the most advanced browser first, then retrofits fallbacks or patches for older browsers when things break. Progressive enhancement builds a solid, resilient, functional core baseline that works on any browser first, then layers enhanced capabilities (such as subgrid, modern colors, or view transitions) inside `@supports` for browsers equipped to handle them.
</details>

### Q2: Why is `@supports selector(...)` necessary when testing selectors like `:has()`?
<details>
<summary>View Answer</summary>
Standard `@supports (property: value)` syntax only tests CSS property-value pairs. Prior to CSS Conditional Rules Level 4, there was no way to test whether a browser recognized a new selector syntax or pseudo-class. Writing `@supports (:has(p))` is invalid because `:has(p)` is not a property-value declaration. Wrapping it in `selector(...)` allows the browser's selector parser to validate the selector syntax directly.
</details>

### Q3: What is the difference between a natural cascade fallback and an `@supports` block?
<details>
<summary>View Answer</summary>
A natural cascade fallback relies on the parser ignoring unrecognized property declarations:
```css
.card {
  color: #2563eb; /* Fallback */
  color: oklch(0.6 0.22 250); /* Modern */
}
```
This works when testing a single property value. `@supports` is required when a modern feature involves **structural or architectural changes across multiple properties and selectors** (e.g., swapping a flexbox card layout for a subgrid layout), where a single fallback line is insufficient.
</details>

### Q4: What are the strict parenthetical syntax rules of `@supports`?
<details>
<summary>View Answer</summary>
Every feature condition must be wrapped in its own parentheses. For example, `@supports (display: grid)` is valid, but `@supports display: grid` is a syntax error. Furthermore, when combining operators (`and`, `or`, `not`), each term must be parenthesized to avoid ambiguity (e.g. `@supports ((display: flex) and (not (display: grid)))`).
</details>

### Q5: Can `@supports` test CSS custom property values?
<details>
<summary>View Answer</summary>
Testing `--my-var: 20px` inside `@supports` is generally unhelpful because browsers accept almost any arbitrary token sequence as valid for a custom property. However, testing typed properties registered via `@property` or testing standard properties that accept variables is fully supported.
</details>

### Q6: How does `@supports font-tech()` improve web typography?
<details>
<summary>View Answer</summary>
`@supports font-tech(variations)` allows developers to verify if the browser supports Variable Fonts before attempting to download a single large variable font file. If unsupported, the stylesheet falls back to serving discrete static font weights (`regular.woff2`, `bold.woff2`).
</details>

### Q7: What happens if an `@supports` at-rule is nested inside a CSS selector rule?
<details>
<summary>View Answer</summary>
Under CSS Nesting and Conditional Rules specifications, `@supports` can be nested directly inside selector rules:
```css
.card {
  padding: 1rem;
  @supports (backdrop-filter: blur(5px)) {
    backdrop-filter: blur(5px);
  }
}
```
The browser evaluates the condition and applies the nested declarations only when supported.
</details>

### Q8: Does an un-matching `@supports` block incur any layout or painting performance cost?
<details>
<summary>View Answer</summary>
No. When an `@supports` condition evaluates to false, the browser's parser immediately skips the block without inserting the declarations into the CSSOM, ensuring zero layout or painting overhead.
</details>

---

## 11. Practice Exercises

### Exercise 1: CSS Grid with Flexbox Fallback
**Objective**: Build a responsive card gallery with a flexbox baseline that upgrades to a pure CSS Grid layout when supported.
<details>
<summary>View Solution</summary>

```css
/* Baseline: Flexbox */
.gallery-container {
  display: flex;
  flex-wrap: wrap;
  gap: 1rem;
}

.gallery-container > * {
  flex: 1 1 18rem;
}

/* Progressive Enhancement: Grid */
@supports (display: grid) {
  .gallery-container {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(18rem, 1fr));
  }

  .gallery-container > * {
    flex: initial;
  }
}
```
</details>

### Exercise 2: Feature-Gated `:has()` Form Validation
**Objective**: Style invalid form fields using traditional child selectors by default, and upgrade to parent card highlights if `:has()` is supported.
<details>
<summary>View Solution</summary>

```css
/* Baseline */
.form-input:invalid {
  border-color: #ef4444;
}

/* Enhanced with :has() */
@supports selector(:has(input:invalid)) {
  .form-group:has(.form-input:invalid) {
    background-color: #fef2f2;
    border-left: 4px solid #ef4444;
  }
}
```
</details>

### Exercise 3: Container Queries with Viewport Media Fallback
**Objective**: Style a component using standard `@media` queries by default, enhanced with `@container` queries when available.
<details>
<summary>View Solution</summary>

```css
/* Baseline: Viewport Media Query */
.widget-card {
  padding: 1rem;
}

@media (min-width: 48rem) {
  .widget-card {
    display: flex;
    gap: 1.5rem;
  }
}

/* Enhanced: Component Container Query */
@supports (container-type: inline-size) {
  .widget-wrapper {
    container-type: inline-size;
  }

  @container (min-width: 30rem) {
    .widget-card {
      display: flex;
      gap: 1.5rem;
    }
  }
}
```
</details>

### Exercise 4: Frosted Glass Overlay with Solid Fallback
**Objective**: Build a navigation header that is 90% solid gray by default, but uses `backdrop-filter: blur(8px)` when supported.
<details>
<summary>View Solution</summary>

```css
.glass-header {
  background-color: #1e293b;
  color: white;
}

@supports (backdrop-filter: blur(8px)) or (-webkit-backdrop-filter: blur(8px)) {
  .glass-header {
    background-color: rgba(30, 41, 59, 0.75);
    backdrop-filter: blur(8px);
    -webkit-backdrop-filter: blur(8px);
  }
}
```
</details>

### Exercise 5: Relative Color Syntax Check
**Objective**: Guard a Relative Color Syntax tint rule with `@supports`.
<details>
<summary>View Solution</summary>

```css
:root {
  --base-accent: #2563eb;
  --accent-tint: #dbeafe; /* Static fallback */
}

@supports (color: oklch(from red l c h)) {
  :root {
    --accent-tint: oklch(from var(--base-accent) l c h / 0.15);
  }
}
```
</details>

### Exercise 6: Variable Font Progressive Enhancement
**Objective**: Serve static web fonts by default, upgrading to a single variable font file using `@supports font-tech(variations)`.
<details>
<summary>View Solution</summary>

```css
/* Baseline: Static Regular & Bold */
@font-face {
  font-family: 'AppFont';
  src: url('/fonts/appfont-regular.woff2') format('woff2');
  font-weight: 400;
}

/* Enhanced: Variable Font */
@supports font-tech(variations) {
  @font-face {
    font-family: 'AppFont';
    src: url('/fonts/appfont-variable.woff2') format('woff2');
    font-weight: 100 900;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Property Query */
@supports (display: subgrid) { ... }

/* Selector Query */
@supports selector(:has(img)) { ... }

/* Negation Query */
@supports not (backdrop-filter: blur(5px)) { ... }

/* Combined Logical Operators */
@supports (display: grid) and (not (display: subgrid)) { ... }
```

- Always wrap feature conditions in parentheses: `@supports (prop: val)`.
- Use `selector(...)` when testing pseudo-classes and modern selector grammar.
- Deliver a solid universal baseline first; use `@supports` strictly for additive enhancements.
- Widely available across all modern browser engines.

---

## 13. Further Reading

- [MDN Web Docs: @supports](https://developer.mozilla.org/en-US/docs/Web/CSS/@supports)
- [W3C CSS Conditional Rules Module Level 4](https://www.w3.org/TR/css-conditional-4/)
- [web.dev: Testing support for modern CSS features](https://web.dev/at-supports/)
- [CSS-Tricks: How @supports Works](https://css-tricks.com/how-supports-works/)

---

### Navigation
- **Previous**: [15-masonry-and-grid-lanes.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/15-masonry-and-grid-lanes.md)
- **Next**: [17-sibling-index-and-count.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/17-sibling-index-and-count.md)
