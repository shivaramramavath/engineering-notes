# 12. Component Frameworks Compared: Bootstrap, Bulma, Tailwind, Open Props, and DaisyUI

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Architecture & Framework Design Patterns / Industry Baseline |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Modern DevTools, Webpack 5, Vite 5, PostCSS 8 |
| **Prerequisites** | `01-css-architecture-principles.md`, `03-utility-first-and-tailwind-overview.md` |

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

### The Problem
Building production-grade web interfaces from scratch requires solving thousands of foundational UI problems: grid alignment, responsive navigation, accessible modal dialogs, cross-browser form normalisation, coherent typography hierarchies, and thematic colour schemes. Without a shared foundation, engineering teams inevitably duplicate low-level CSS, introduce conflicting visual languages, and ship bloated, fragmented stylesheets.

### The Solution
CSS component frameworks and styling libraries provide pre-engineered design abstractions, visual patterns, and design tokens:
- **Component-Driven Frameworks (Bootstrap, Bulma)**: Deliver turn-key HTML component patterns (`.card`, `.navbar`, `.btn-primary`) styled via opinionated BEM-like classes.
- **Utility-First Engines (Tailwind CSS)**: Provide atomic, single-purpose classes (`flex`, `p-4`, `text-sm`) paired with an on-demand JIT compiler that generates only the CSS used in source templates.
- **Token & Custom Property Frameworks (Open Props)**: Provide modular, native CSS custom properties (`var(--size-4)`, `var(--shadow-3)`) that leave HTML semantics and component markup entirely up to the author.
- **Semantic Component Layers (DaisyUI)**: Provide semantic utility component classes (`btn`, `card`, `badge`) built on top of Tailwind CSS, bridging component convenience with utility composability.

Understanding the internal mechanics, bundle overhead, runtime trade-offs, and customization limits of each paradigm enables teams to select the right tool for their product scale and lifecycle.

---

## 2. Core Concepts

### Taxonomy of CSS Framework Paradigms

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       CSS FRAMEWORK SPECTRUM                                │
└─────────────────────────────────────────────────────────────────────────────┘
  Opinionated Components                                 Unopinionated Tokens
  ◄─────────────────────────────────────────────────────────────────────────►

  [ Bootstrap 5 ]       [ Bulma ]        [ DaisyUI ]    [ Tailwind ]    [ Open Props ]
  HTML + Classes        CSS-only BEM     Tailwind Plugin  Atomic JIT     Native Variables
  Pre-baked JS          No JS            Semantic UI     Zero UI         Zero Classes
  Heavy CSS (~280KB)    Flexbox (~200KB) Clean HTML      Purged (~10KB)  Modular CSS (~15KB)
```

1. **Monolithic Component Frameworks (Bootstrap, Bulma)**:
   - Provide pre-styled, complete UI components.
   - HTML markup must strictly conform to the framework's DOM expectations.
   - Customisation requires overriding Sass variables or writing cascading override rules with elevated specificity.
2. **Atomic Utility Frameworks (Tailwind CSS)**:
   - Provide zero pre-styled UI components out of the box.
   - Styling occurs in the HTML template by composing atomic classes.
   - Dead-code elimination via JIT produces exceptionally small production bundles (typically 5KB–15KB gzipped).
3. **Design-Token Foundations (Open Props)**:
   - Contains no classes or layout components.
   - Injects normalized, carefully calibrated CSS custom properties for colours, typography, shadows, animations, and spacing.
   - Completely agnostic of HTML structure, component libraries, or build pipelines.
4. **Hybrid Semantic Extensions (DaisyUI)**:
   - A Tailwind CSS plugin that introduces semantic component class names (`btn`, `modal`, `navbar`) while preserving Tailwind's utility modifiers (`btn-primary`, `btn-sm`, `gap-2`).

---

## 3. How It Works Internally

### Architectural Comparison Flow

```
BOOTSTRAP / BULMA PIPELINE:
  Sass Source Files ──► Sass Compiler ──► Full CSS Bundle (~250KB)
                                                │
                                                ▼ (In Browser)
                                      HTML matches `.btn-primary`

TAILWIND CSS PIPELINE:
  Template Files (*.html, *.tsx)
            │
            ▼ (Regex Token Extraction)
  Tailwind JIT Engine ──► Evaluates Tokens against Config
            │
            ▼
  Minimal Scanned CSS (~10KB) ──► PostCSS ──► Browser Output

OPEN PROPS PIPELINE:
  Static / CDN CSS Tokens ──► Browser Native Variables :root { --size-1: 0.25rem; }
                                    │
                                    ▼
                           Author CSS uses `var(--size-1)`
```

### Bundle Size & Overhead Comparison
- **Bootstrap 5**: CSS ~280 KB uncompressed (~32 KB brotli) + Optional JS bundle ~165 KB (~50 KB brotli).
- **Bulma 1.0**: CSS ~220 KB uncompressed (~28 KB brotli), pure CSS (no JavaScript included).
- **Tailwind CSS v3/v4**: JIT output scales with template vocabulary. Average production bundle: ~8 KB to ~18 KB brotli.
- **Open Props**: Granular imports (~1 KB to ~15 KB brotli total depending on imported token packs).
- **DaisyUI**: Adds ~10 KB brotli on top of Tailwind JIT output.

---

## 4. Syntax & Usage

### Comparing Button Implementations Across Paradigms

#### 1. Bootstrap 5 (Sass-compiled BEM-style)
```html
<!-- HTML Structure -->
<button type="button" class="btn btn-primary btn-lg shadow-sm">
  Confirm Order
</button>
```

#### 2. Tailwind CSS (Atomic Utility)
```html
<!-- HTML Structure -->
<button type="button" class="inline-flex items-center justify-center px-6 py-3 text-base font-semibold text-white bg-blue-600 rounded-lg shadow-sm hover:bg-blue-700 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600 transition-colors">
  Confirm Order
</button>
```

#### 3. Open Props (Native CSS Custom Properties)
```html
<button type="button" class="app-btn-primary">
  Confirm Order
</button>
```
```css
/* Custom Component CSS leveraging Open Props */
@import "open-props/style";
@import "open-props/buttons";

.app-btn-primary {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: var(--size-2) var(--size-4);
  font-family: var(--font-sans);
  font-size: var(--font-size-1);
  font-weight: var(--font-weight-6);
  color: var(--gray-0);
  background: var(--blue-7);
  border-radius: var(--radius-2);
  box-shadow: var(--shadow-1);
  border: 1px solid transparent;
  transition: background-color var(--ease-2) 0.15s;
}

.app-btn-primary:hover {
  background: var(--blue-8);
}

.app-btn-primary:focus-visible {
  outline: 2px solid var(--blue-6);
  outline-offset: 2px;
}
```

#### 4. DaisyUI (Semantic Layer over Tailwind)
```html
<!-- HTML Structure -->
<button type="button" class="btn btn-primary btn-lg shadow-sm">
  Confirm Order
</button>
```

---

## 5. Practical Examples

### Example 1: Responsive Grid & Card Pattern in Tailwind vs. Bootstrap

#### Bootstrap 5 Layout
```html
<div class="container my-5">
  <div class="row g-4">
    <div class="col-12 col-md-6 col-lg-4">
      <div class="card h-100 shadow-sm border-0">
        <div class="card-body d-flex flex-column">
          <h5 class="card-title fw-bold">Cloud Migration</h5>
          <p class="card-text text-secondary flex-grow-1">
            Enterprise infrastructure migration strategies.
          </p>
          <a href="#" class="btn btn-outline-primary mt-3">Read Case Study</a>
        </div>
      </div>
    </div>
  </div>
</div>
```

#### Modern Native CSS (Standard Modern Platform equivalent)
```css
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 320px), 1fr));
  gap: 1.5rem;
  padding: 2rem 1rem;
  max-width: 1200px;
  margin-inline: auto;
}

.card {
  display: flex;
  flex-direction: column;
  background-color: #ffffff;
  border-radius: 0.75rem;
  padding: 1.5rem;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.card-title {
  font-size: 1.25rem;
  font-weight: 700;
  margin-block-end: 0.5rem;
}

.card-body {
  flex: 1 1 auto;
  color: #4b5563;
  line-height: 1.5;
}

.card-action {
  align-self: flex-start;
  margin-block-start: 1rem;
}
```

### Example 2: DaisyUI Multi-Theme Toggle
DaisyUI compiles theme definitions into custom property sets applied via data attributes.

```html
<!-- Theme Controller in DaisyUI -->
<div data-theme="dark" class="p-6 bg-base-100 text-base-content rounded-xl shadow-lg">
  <h2 class="text-xl font-bold">DaisyUI Theme Sandbox</h2>
  <p class="text-sm opacity-80 mt-2">
    Themes switch automatically via data-theme attribute with zero runtime JS recalculation.
  </p>
  <div class="mt-4 flex gap-2">
    <button class="btn btn-primary">Primary</button>
    <button class="btn btn-secondary">Secondary</button>
    <button class="btn btn-accent">Accent</button>
  </div>
</div>
```

### Example 3: Open Props Fluid Typography & Shadows
```css
@import "open-props/style";
@import "open-props/normalize";

.article-container {
  max-inline-size: var(--size-content-3);
  margin-inline: auto;
  padding-inline: var(--size-4);
}

.fluid-heading {
  font-family: var(--font-sans);
  font-size: var(--font-size-fluid-3);
  font-weight: var(--font-weight-8);
  letter-spacing: var(--font-letterspacing-1);
  color: var(--text-1);
  text-wrap: balance;
}

.elevation-card {
  background-color: var(--surface-2);
  border-radius: var(--radius-3);
  padding: var(--size-5);
  box-shadow: var(--shadow-3);
  transition: box-shadow var(--ease-3) 0.2s, transform var(--ease-3) 0.2s;
}

.elevation-card:hover {
  transform: translateY(-2px);
  box-shadow: var(--shadow-5);
}
```

### Example 4: Overriding Bootstrap via Modern `@layer`
Before modern `@layer`, customizing Bootstrap often required high-specificity selectors or `!important`. Modern setups wrap third-party CSS in lower-priority layers.

```css
/* index.css */
@layer bootstrap, overrides;

@import "bootstrap/dist/css/bootstrap.min.css" layer(bootstrap);

@layer overrides {
  /* Simple (0, 0, 1) selector reliably overrides Bootstrap's .btn without !important */
  .btn {
    border-radius: 9999px;
    font-weight: 700;
    letter-spacing: 0.05em;
  }
}
```

### Example 5: Customizing Tailwind with Design Tokens via `@theme` (Tailwind v4)
In Tailwind v4, configuration transitions from JavaScript (`tailwind.config.js`) to pure CSS `@theme` blocks:

```css
@import "tailwindcss";

@theme {
  --color-brand-primary: oklch(0.62 0.24 260);
  --color-brand-accent: oklch(0.78 0.18 140);
  --font-display: "Inter Display", sans-serif;
  --spacing-18: 4.5rem;
}

/* Now usable as utility classes immediately */
.hero-banner {
  background-color: var(--color-brand-primary);
  font-family: var(--font-display);
}
```

---

## 6. Best Practices

1. **Wrap Monolithic Frameworks in `@layer`**: Always import Bootstrap or Bulma inside a dedicated `@layer framework` or `@layer vendor` block to maintain flat specificity for custom application styles.
2. **Standardise on Design Tokens Before Choosing a Library**: Establish colors, scale factors, typography, and spacing tokens in native CSS custom properties before committing to Tailwind or Bootstrap.
3. **Avoid Mixing Multiple CSS Frameworks**: Never run Bootstrap and Tailwind concurrently on the same DOM subtree; reset collisions, class naming clashes (`.container`, `.d-none` vs `.hidden`), and specificity wars will ensue.
4. **Enforce Component Encapsulation for Repetitive Utilities**: If a Tailwind utility chain exceeds 6–8 classes and repeats across 5+ templates, extract it into a dedicated component (e.g. React/Vue component or `@apply` class).
5. **Verify Accessibility Defaults**: Do not trust default framework accessible styling blindly; verify keyboard focus indicators, screen reader labels, and color contrast compliance in all deployed variants.
6. **Use On-Demand Compilation in Production**: Ensure PurgeCSS or Tailwind JIT scanners inspect all dynamic template extensions (`.html`, `.jsx`, `.vue`, `.mdx`) to prevent missing styles or dead-weight bundle leaks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Importing unpurged Bootstrap CSS in full** | Ships ~280 KB of unparsed CSS blocking First Contentful Paint. | Import only required modules via Sass (`@import "bootstrap/scss/buttons"`) or wrap in `@layer`. |
| **Using dynamic string concatenation in Tailwind** | E.g. `class="bg-${color}-500"`. JIT scanner misses class; styles are missing in production. | Write complete class names in code or use lookup maps: `const colors = { blue: 'bg-blue-500' };`. |
| **Overriding framework styles with `!important`** | Corrupts specificity hierarchy; makes future overrides brittle. | Lower framework specificity via `@layer vendor { ... }` or modern selector wrappers. |
| **Abandoning semantic HTML for utility `<div>`s** | Severe accessibility degradation for assistive tech and SEO. | Retain semantic tags (`<nav>`, `<main>`, `<article>`, `<button>`) styled with utility classes. |
| **Mixing DaisyUI classes without responsive modifiers** | Inflexible mobile layouts when relying solely on default component width/heights. | Combine semantic classes with Tailwind responsive prefixes: `card w-full md:w-96`. |
| **Relying on JavaScript component plugins without checking bundle impact** | Shipping 160 KB Bootstrap JS just for a modal or dropdown. | Use native HTML `<dialog>` and `popover` APIs styled via modern CSS. |

---

## 8. Performance, Accessibility & Browser Support

### Performance Benchmark Table

| Library / Tool | Uncompressed Size | Minified & Brotli | JIT Purging | Runtime JS Cost |
| :--- | :--- | :--- | :--- | :--- |
| **Bootstrap 5.3** | ~280 KB | ~32 KB | Manual Sass tree-shaking | ~50 KB (Optional) |
| **Bulma 1.0** | ~220 KB | ~28 KB | Manual Sass | 0 KB (CSS only) |
| **Tailwind CSS v4** | Variable | 8 KB – 16 KB | Automatic JIT | 0 KB |
| **Open Props** | ~15 KB total | ~3.8 KB | Modular sub-imports | 0 KB |
| **DaisyUI** | Tailwind + ~50 KB | Tailwind + ~10 KB | Automatic JIT | 0 KB |

### Accessibility Considerations
- **Form Controls**: Bootstrap provides extensive `.form-check` and `.is-invalid` visual states, but developers must still manage `aria-invalid` and `aria-describedby` manually.
- **Focus Rings**: Tailwind v3+ applies `outline-offset` and transparent outlines for high-contrast accessibility. DaisyUI components default to visible focus states.
- **High Contrast Mode**: Frameworks relying heavily on custom box-shadow borders fail in Windows Forced Colors Mode unless fallback `border: 1px solid transparent` or `outline` is provided.

---

## 9. Comparison / When to Use What

| Feature / Metric | Bootstrap 5 | Bulma | Tailwind CSS | Open Props | DaisyUI |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Primary Philosophy** | Full-fledged UI suite | Clean CSS-only components | Atomic utility engine | Native CSS token suite | Semantic Tailwind plugin |
| **Learning Curve** | Low (copy-paste HTML) | Low (semantic classes) | Medium (utility vocabulary) | Low (pure CSS variables) | Low-Medium |
| **Design Uniqueness** | Low ("looks like Bootstrap") | Medium | Maximum (unconstrained) | Maximum (author writes CSS) | High (customizable themes) |
| **Maintenance at Scale** | Hard (specificity fights) | Medium | Excellent (locational styling) | Excellent (token-driven) | Good |
| **Recommended Use** | Internal tools, enterprise portals | Content blogs, light sites | SaaS apps, custom design systems | Modern vanilla/hybrid CSS | Rapid prototyping with Tailwind |

---

## 10. Interview Questions

### Q1: What are the fundamental trade-offs between a utility-first CSS framework like Tailwind and a component framework like Bootstrap?
<details>
<summary>Answer</summary>
The trade-offs centre on design freedom, bundle footprint, maintenance, and HTML ergonomics:
1. **Design Flexibility**: Bootstrap provides pre-baked component patterns with opinionated margins, padding, and colors. Overriding them leads to high-specificity stylesheet conflicts. Tailwind provides unopinionated utility atoms, enabling exact custom designs without fighting framework opinions.
2. **Bundle Footprint**: Bootstrap ships a fixed CSS payload (~280 KB uncompressed) regardless of whether 5% or 95% of classes are used (unless carefully configured via Sass imports). Tailwind utilizes a JIT scanner that produces a tiny, fixed-size bundle (~10–15 KB) proportional only to unique utilities used.
3. **Code Ergonomics**: Bootstrap yields cleaner HTML with fewer class names (`class="btn btn-primary"`), but shifts style management to external CSS. Tailwind produces verbose HTML templates (`class="px-4 py-2 bg-blue-600 rounded-md font-medium text-white hover:bg-blue-700"`), but eliminates dead CSS and context-switching between template and stylesheet.
</details>

### Q2: How has the modern `@layer` specification altered the way third-party component frameworks are consumed in large codebases?
<details>
<summary>Answer</summary>
Historically, integrating third-party component libraries (like Bootstrap) into custom applications resulted in "specificity wars." Overriding a framework selector like `.navbar .nav-link.active` required either equal-or-higher selector specificity or resorting to `!important`.

With CSS Cascade Layers (`@layer`), developer styles can be placed in a layer declared after the framework layer:
```css
@layer vendor, app;
@import "bootstrap.css" layer(vendor);

@layer app {
  /* An un-nested, single-class selector beats any selector in the vendor layer */
  .nav-link {
    color: red;
  }
}
```
Rules in subsequent layers always triumph over preceding layers regardless of selector specificity, cleanly solving the third-party override dilemma without `!important`.
</details>

### Q3: Explain why Tailwind's JIT compiler fails to compile dynamic class expressions such as `bg-${themeColor}-500`.
<details>
<summary>Answer</summary>
Tailwind's JIT scanner does not execute JavaScript or evaluate runtime template variables. Instead, it runs a regex-based static text extraction over raw template source files (`.html`, `.jsx`, `.vue`) to find string patterns that match known utility classes.

When it encounters `bg-${themeColor}-500`, the scanner sees only the literal text `bg-${themeColor}-500`, which is not a valid CSS class in its configuration. Consequently, the JIT engine generates no CSS for `bg-blue-500` or `bg-red-500`. To resolve this, developers must write complete class names in code or use a lookup dictionary (safelisting or map objects) containing full string identifiers.
</details>

### Q4: How does Open Props differ from utility-first frameworks like Tailwind and component frameworks like DaisyUI?
<details>
<summary>Answer</summary>
Open Props does not provide utility classes (`.flex`, `.p-4`) or UI component classes (`.btn`, `.card`). Instead, it is a curated library of standard CSS custom properties (`var(--size-4)`, `var(--shadow-3)`, `var(--gradient-1)`) injected directly into `:root`.

Authors continue writing standard, semantic CSS in external stylesheets or CSS modules, referencing Open Props tokens for consistent scale, typography, animation curves, and color palettes. It has zero HTML footprint and requires no build step or build-time compiler.
</details>

### Q5: What is DaisyUI, and what architectural problem does it solve for Tailwind CSS teams?
<details>
<summary>Answer</summary>
DaisyUI is a component plugin for Tailwind CSS. The primary criticism of Tailwind is template bloat—repeating long strings of 10+ utility classes across dozens of buttons, inputs, and cards.

DaisyUI introduces semantic component class names (`btn`, `card`, `toggle`, `badge`) implemented via pure Tailwind CSS utility definitions internally. Developers can write `<button class="btn btn-primary">` for standard elements while retaining the ability to modify or override styling using Tailwind utility classes (`class="btn btn-primary btn-sm md:btn-lg"`).
</details>

### Q6: Can modern native CSS nesting replace Sass in a project migrating away from Bootstrap?
<details>
<summary>Answer</summary>
Yes for almost all standard layout and selector grouping, but with one critical syntax caveat regarding BEM class concatenation:
In Sass:
```scss
.block {
  &__element { color: red; } /* Compiles to .block__element */
}
```
In native CSS nesting, `&` desugars to `:is(.block)`. Therefore, `&__element` is parsed as an invalid compound selector and fails. Native CSS requires explicitly writing `.block__element` or using nesting for descendant/state modifiers (`&:hover`, `&.is-active`, `& .child`). With native CSS custom properties, `@layer`, and `color-mix()`, the need for a Sass preprocessor compiler is virtually eliminated in modern projects.
</details>

### Q7: How do design token naming conventions prevent framework lock-in?
<details>
<summary>Answer</summary>
By introducing a tiered token architecture:
1. **Global/Reference Tokens**: Primitive values (`--palette-blue-500: #3b82f6`).
2. **Semantic/System Tokens**: Intent-based aliases (`--color-surface-brand: var(--palette-blue-500)`).
3. **Component Tokens**: Specific component bindings (`--button-primary-bg: var(--color-surface-brand)`).

If an application references semantic or component tokens rather than hardcoded framework class names (`btn-primary` or `bg-blue-500`), switching the underlying styling engine (e.g., from Bootstrap to Tailwind or native CSS) requires merely remapping the token assignments rather than refactoring thousands of template files.
</details>

### Q8: What performance implications arise from using CSS-only component libraries like Bulma compared to utility-first JIT frameworks?
<details>
<summary>Answer</summary>
CSS-only component libraries like Bulma must ship their complete CSS ruleset (~200 KB) over the network because they cannot predict which component combinations the user might render dynamically. Browsers must download, parse, and construct style rules for thousands of unused classes, delaying DOMContentLoaded and FCP.

Utility-first JIT frameworks (Tailwind) inspect template files during the build and generate only the exact classes utilized. The resulting stylesheet is typically under 15 KB, dramatically reducing network latency, CSSOM construction time, and memory footprint.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactoring a Bootstrap Card into Native Modern CSS with `@layer`
**Objective**: Convert a Bootstrap-dependent card layout into clean, native CSS using modern grid, aspect-ratio, and custom properties, isolated in an `@layer components` block.

<details>
<summary>Solution</summary>

```html
<article class="product-card">
  <div class="product-card__media">
    <img src="/images/product.webp" alt="Wireless Headset" width="400" height="300" loading="lazy">
  </div>
  <div class="product-card__content">
    <h3 class="product-card__title">Acoustic Pro Headset</h3>
    <p class="product-card__desc">Active noise-cancelling wireless headphones with 40h battery life.</p>
    <div class="product-card__footer">
      <span class="product-card__price">$249.00</span>
      <button type="button" class="product-card__btn">Add to Cart</button>
    </div>
  </div>
</article>
```

```css
@layer components {
  .product-card {
    --card-bg: #ffffff;
    --card-radius: 0.75rem;
    --card-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);

    display: flex;
    flex-direction: column;
    background-color: var(--card-bg);
    border-radius: var(--card-radius);
    overflow: clip;
    box-shadow: var(--card-shadow);
    max-inline-size: 360px;
    border: 1px solid #e5e7eb;
  }

  .product-card__media img {
    inline-size: 100%;
    block-size: auto;
    aspect-ratio: 4 / 3;
    object-fit: cover;
  }

  .product-card__content {
    display: flex;
    flex-direction: column;
    padding: 1.25rem;
    flex: 1 1 auto;
    gap: 0.75rem;
  }

  .product-card__title {
    font-size: 1.25rem;
    font-weight: 700;
    color: #111827;
    margin: 0;
  }

  .product-card__desc {
    font-size: 0.875rem;
    color: #6b7280;
    line-height: 1.5;
    margin: 0;
  }

  .product-card__footer {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-block-start: auto;
    padding-block-start: 1rem;
  }

  .product-card__price {
    font-size: 1.25rem;
    font-weight: 800;
    color: #111827;
  }

  .product-card__btn {
    padding: 0.5rem 1rem;
    background-color: #2563eb;
    color: #ffffff;
    font-weight: 600;
    font-size: 0.875rem;
    border-radius: 0.5rem;
    border: none;
    cursor: pointer;
    transition: background-color 0.15s ease;
  }

  .product-card__btn:hover {
    background-color: #1d4ed8;
  }

  .product-card__btn:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }
}
```
</details>

### Exercise 2: Building an Open Props Themed Alert Box
**Objective**: Construct an accessible notification alert box leveraging Open Props variables for colours, shadows, border-radius, and animation.

<details>
<summary>Solution</summary>

```css
@import "open-props/style";
@import "open-props/animations";

.alert-box {
  display: flex;
  align-items: flex-start;
  gap: var(--size-3);
  padding: var(--size-3) var(--size-4);
  border-radius: var(--radius-2);
  background-color: var(--surface-1);
  box-shadow: var(--shadow-2);
  border-inline-start: var(--border-size-3) solid var(--blue-6);
  animation: var(--animation-slide-in-down);
}

.alert-box--success {
  border-inline-start-color: var(--green-6);
  background-color: var(--green-0);
  color: var(--green-11);
}

.alert-box--error {
  border-inline-start-color: var(--red-6);
  background-color: var(--red-0);
  color: var(--red-11);
}

.alert-box__message {
  font-family: var(--font-sans);
  font-size: var(--font-size-1);
  line-height: var(--font-lineheight-2);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Bootstrap 5**: Best for internal dashboards, enterprise apps, and legacy teams; provides pre-styled HTML components and JS widgets at the cost of a larger CSS payload (~280 KB).
- **Bulma**: Pure CSS component library; zero JS runtime overhead; uses flexbox layouts and clean BEM-like classes.
- **Tailwind CSS**: Atomic utility framework with JIT compilation; produces ultra-small bundles (~10–15 KB); eliminates specificity wars and dead CSS; creates verbose template markup.
- **Open Props**: Zero-class, zero-runtime design token system using native CSS variables; maximum flexibility and HTML independence.
- **DaisyUI**: Tailwind CSS plugin delivering semantic component classes (`btn`, `card`) while retaining Tailwind's responsive and state modifier superpowers.
- **Isolation Pattern**: Always isolate third-party frameworks inside `@layer vendor` or `@layer framework` to prevent third-party selectors from polluting custom application specificity.

---

## 13. Further Reading
- [Tailwind CSS Documentation](https://tailwindcss.com/docs)
- [Open Props Documentation & Showcase](https://open-props.style/)
- [Bootstrap 5 Documentation](https://getbootstrap.com/docs/5.3/getting-started/introduction/)
- [DaisyUI Semantic Tailwind Components](https://daisyui.com/)
- [MDN: Using CSS Cascade Layers (@layer)](https://developer.mozilla.org/en-US/docs/Learn/CSS/Building_blocks/Cascade_layers)
- [W3C CSS Cascading and Inheritance Level 5](https://www.w3.org/TR/css-cascade-5/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [11. Sass and Less vs Native CSS](11-sass-less-vs-native-css.md) | [13. Managing Specificity at Scale](13-managing-specificity-at-scale.md) |
