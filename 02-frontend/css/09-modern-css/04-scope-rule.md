# CSS Scope (`@scope`) and Modular Encapsulation

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Cascading and Inheritance Level 6 / CSS Scoping Module Level 1 (W3C Working Draft) |
| **Baseline Status** | Baseline 2024 (Newly Available: Chrome 118+, Edge 118+, Safari 17.4+, Firefox in active development) |
| **Browsers Tested** | Chrome 118+, Edge 118+, Safari 17.4+ |
| **Tested With** | W3C Web Platform Tests, Chrome DevTools Style Inspector, WebKit Layout Tests |

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

In large-scale web applications, **style leakage** has historically been one of the hardest challenges to overcome. A CSS rule intended for a specific component (e.g. `.card .title`) frequently bleeds into nested sub-components (such as an embedded author badge with its own `.title`).

Over the years, engineering teams devised heavy workarounds to simulate encapsulation:
- **BEM Naming Conventions**: Long, verbose class names like `.card__header-title`.
- **CSS Modules / Scoped CSS**: Build-time tools that append random hashes to class names (e.g. `.title_7x9a1`).
- **Shadow DOM**: Heavy browser primitives that completely isolate DOM subtrees, but break form association, font inheritance, and global styling.

The modern **`@scope`** at-rule natively solves component-level encapsulation in CSS without build tools, class hashing, or Shadow DOM overhead. Crucially, `@scope` introduces **scoping limits**—commonly known as **Donut Scoping**—allowing styles to target an outer component without leaking into nested sub-components.

---

## 2. Core Concepts

### The Scoping Root
The scoping root defines the boundary element where scoping begins:
```css
@scope (.card) {
  /* Only styles elements inside .card */
  img {
    border-radius: 0.5rem;
  }
}
```

### The Scoping Limit ("Donut Scoping")
The scoping limit defines where the style rules must **stop applying**. The syntax uses the `to` keyword:
```css
@scope (.article-card) to (.author-bio) {
  /* Targets p elements inside .article-card,
     BUT STOPS before penetrating .author-bio */
  p {
    color: #334155;
  }
}
```
This creates a "donut": the outer ring is styled, while the "hole" in the middle (the nested sub-component) remains untouched.

### The `:scope` Pseudo-Class
Inside an `@scope` block, the `:scope` pseudo-class references the scoping root itself:
```css
@scope (.modal) {
  :scope {
    border: 1px solid #cbd5e1;
    background-color: #ffffff;
  }

  /* Target direct children of the scoping root */
  :scope > h2 {
    margin-top: 0;
  }
}
```

### Scoping Proximity in the Cascade
When two scoped selectors have identical specificity, the CSS Cascade resolves the winner using **Scoping Proximity**: **the rule whose scoping root is closest (in DOM hops) to the targeted element wins**, completely ignoring source order.

```css
@scope (.light-theme) {
  p { color: black; }
}

@scope (.dark-theme) {
  p { color: white; }
}
```
If a `.dark-theme` element is nested inside a `.light-theme` element, any `p` inside `.dark-theme` will be white, because the `.dark-theme` scoping root is fewer hops away from the `p` than `.light-theme`.

---

## 3. How It Works Internally

```
Donut Scoping Architectural Model:

  <div class="article-card">        <── Scoping Root: @scope (.article-card)
    <h1>Article Title</h1>             │  [MATCHES: Styled by @scope]
    <p>Article introduction.</p>       │  [MATCHES: Styled by @scope]
                                       │
    <div class="author-bio">        <── Scoping Limit: to (.author-bio)
      <h3>Author Name</h3>             │  [HOLE IN DONUT: STOPS HERE]
      <p>Author description.</p>       │  [DOES NOT MATCH: Completely ignored!]
    </div>                             │
  </div>                            <── End of Article Card
```

```
Cascade Priority with Scoping Proximity:
┌────────────────────────────────────────────────────────┐
│ 1. Cascade Layer (@layer)                              │
│ 2. Specificity (A, B, C)                               │
│ 3. Scoping Proximity (Closer scoping root in DOM tree) │  <-- Modern Tie-Breaker!
│ 4. Order of Appearance (Source Order)                  │
└────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline Status:
├── Chrome / Edge: 118+ (October 2023)
├── Safari: 17.4+ (March 2024)
└── Firefox: Under active implementation (behind layout.css.scope.enabled flag)
```

### Basic Syntax Forms
```css
/* 1. Scoped to a root */
@scope (scoping-root-selector) {
  selector {
    /* declarations */
  }
}

/* 2. Donut Scoping (Root to Limit) */
@scope (scoping-root-selector) to (scoping-limit-selector) {
  selector {
    /* declarations */
  }
}

/* 3. Inline HTML Scoping (<style> inside DOM element) */
/* Inside <div class="widget"><style> @scope { ... } </style></div> */
@scope {
  :scope {
    padding: 1rem;
  }
  p {
    font-size: 0.875rem;
  }
}
```

---

## 5. Practical Examples

### Example 1: Donut Scoping (Card with Embedded Avatar/Bio)
Ensure typography in a card does not bleed into an embedded author badge component.

```html
<article class="media-card">
  <h2 class="title">Web Standards 2026</h2>
  <p class="summary">The evolution of native browser specifications.</p>

  <!-- Embedded Sub-Component -->
  <aside class="author-badge">
    <h3 class="title">Sarah Connor</h3>
    <p class="summary">Staff Platform Engineer</p>
  </aside>
</article>
```

```css
/* Card Scoping: Stops before penetrating .author-badge */
@scope (.media-card) to (.author-badge) {
  :scope {
    padding: 1.5rem;
    border: 1px solid #cbd5e1;
    border-radius: 0.75rem;
    background: #ffffff;
  }

  .title {
    font-size: 1.5rem;
    color: #0f172a;
    margin: 0 0 0.5rem;
  }

  .summary {
    font-size: 1rem;
    color: #475569;
    line-height: 1.6;
  }
}

/* Author Badge Scoping: Independent encapsulation */
@scope (.author-badge) {
  :scope {
    display: flex;
    flex-direction: column;
    padding: 0.75rem;
    background: #f8fafc;
    border-radius: 0.375rem;
  }

  .title {
    font-size: 0.875rem;
    font-weight: 600;
    color: #2563eb;
    margin: 0;
  }

  .summary {
    font-size: 0.75rem;
    color: #64748b;
    margin: 0;
  }
}
```

### Example 2: Scoping Proximity Resolving Nested Themes
Demonstrating how scoping proximity cleanly overrides source order without specificity hacks.

```html
<div class="theme-light">
  <p>Light text in light container.</p>

  <div class="theme-dark">
    <p>Dark text in nested dark container.</p>
  </div>
</div>
```

```css
/* Source Order: Light theme defined FIRST */
@scope (.theme-light) {
  :scope { background-color: #ffffff; }
  p { color: #0f172a; }
}

/* Source Order: Dark theme defined SECOND */
@scope (.theme-dark) {
  :scope { background-color: #0f172a; }
  p { color: #f8fafc; }
}

/* If .theme-light is nested inside .theme-dark, proximity automatically handles it! */
```

### Example 3: Tab Navigation without Component Style Leaks
Ensure tab styling only applies to immediate tab panels and not nested tab bars.

```html
<div class="tabs-container">
  <nav class="tabs-nav">
    <button class="tab-btn active">Tab 1</button>
  </nav>
  <div class="tab-panel">
    <!-- Nested Inner Tabs -->
    <div class="tabs-container">
      <nav class="tabs-nav">
        <button class="tab-btn active">Sub Tab A</button>
      </nav>
      <div class="tab-panel">Inner content</div>
    </div>
  </div>
</div>
```

```css
/* Outer tabs donut-scoped to stop at nested .tabs-container */
@scope (.tabs-container) to (.tabs-container) {
  .tabs-nav {
    display: flex;
    gap: 1rem;
    border-bottom: 2px solid #e2e8f0;
  }

  .tab-btn {
    padding: 0.75rem 1.25rem;
    border: none;
    background: none;
    color: #64748b;
    cursor: pointer;

    &.active {
      color: #2563eb;
      font-weight: 600;
      border-bottom: 2px solid #2563eb;
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: -2px;
    }
  }

  .tab-panel {
    padding: 1.5rem 0;
  }
}
```

### Example 4: Scoping Roots Combined with Native Nesting
Combining `@scope` with CSS native nesting for expressive component architectures.

```css
@scope (.user-profile-widget) to (.widget-footer) {
  :scope {
    border-radius: 0.5rem;
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  }

  .profile-header {
    display: flex;
    align-items: center;
    gap: 1rem;

    img {
      width: 3rem;
      height: 3rem;
      border-radius: 50%;
    }

    h4 {
      margin: 0;
      color: #1e293b;
    }
  }
}
```

### Example 5: Inline `<style>` Scoping without Arguments
Applying scoped styles to a web component or HTML fragment without needing a unique class name.

```html
<div class="standalone-card">
  <style>
    @scope {
      :scope {
        padding: 1.5rem;
        background-color: #f1f5f9;
        border-radius: 0.5rem;
      }
      h3 {
        color: #0f172a;
        margin-top: 0;
      }
      p {
        color: #475569;
      }
    }
  </style>
  <h3>Self-Contained Fragment</h3>
  <p>Scoped natively without build-step hashes.</p>
</div>
```

---

## 6. Best Practices

1. **Use Donut Scoping (`to (<selector>)`) for compound components**: Whenever building parent containers (cards, accordions, tabs) that can house arbitrary child components, always establish a scoping limit to prevent style leakage.
2. **Combine with Cascade Layers**: Wrap `@scope` inside `@layer components` to cleanly control both modular boundary limits and cascade precedence.
3. **Use `:scope` for container-level rules**: Use `:scope` to style the root container rather than declaring a redundant class name selector inside the block.
4. **Prefer `@scope` over CSS Modules in modern toolchains**: Replacing CSS Modules or CSS-in-JS runtimes with `@scope` eliminates compilation step overhead and produces smaller bundles.
5. **Keep scoping limits intuitive**: Use semantic class boundaries for limits (e.g. `to (.author-bio, .comment-thread)`) so future team members easily understand where styles stop applying.
6. **Ensure focus outlines are never hidden inside scoped rules**: Always provide explicit `:focus-visible` rules for interactive elements inside `@scope`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting scoping limits when nesting components | Outer component styles bleed into nested components (e.g., card styles leak into inner cards). | Add `to (<limit-selector>)` to enforce Donut Scoping. |
| Expecting `@scope` to prevent parent style inheritance | Inherited properties (like `color`, `font-family`) still pass through the DOM tree. | Use explicit property resets (`all: revert` or `initial`) if total isolation is needed. |
| Confusing Scoping Proximity with Specificity | A high-specificity rule with a distant root will still beat a low-specificity rule with a close root. | Proximity only breaks ties when specificities are equal. Keep specificities uniform. |
| Chaining `:scope` incorrectly: `:scope.card` | Fails to match unless `:scope` directly represents that element. | Write `:scope` or `:scope.modifier` directly. |
| Using `@scope` across Shadow DOM boundaries | `@scope` operates within a single document tree; it does not cross shadow boundaries. | Use native Shadow DOM styling (`:host`, `::slotted`) for web components. |
| Forgetting browser fallback for Firefox | Firefox users may see unscoped styles if flags are disabled. | Use `@supports (@scope (.a)) { ... }` or PostCSS scope polyfill for legacy fallbacks. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **CSSOM Optimization**: Because the browser engine can discard subtrees beyond the scoping limit (`to`), style matching trees are smaller, reducing style recalculation time during dynamic DOM updates.
- **Zero Runtime Overhead**: Unlike CSS-in-JS, `@scope` operates purely in C++ inside the browser's CSS parser.

### Accessibility (A11y)
- Unlike Shadow DOM, `@scope` does **not** create encapsulated document fragments. As a result, ARIA relationships (`aria-labelledby`, `aria-describedby`, `id` references) remain fully accessible and discoverable across the entire document tree.

### Browser Compatibility
- **Chromium Engines (Chrome / Edge)**: Supported since version 118 (October 2023).
- **WebKit (Safari)**: Supported since Safari 17.4 (March 2024).
- **Gecko (Firefox)**: In active development (available behind `layout.css.scope.enabled`).
- **Feature Detection**:
```css
@supports (@scope (.a)) {
  /* @scope is supported */
}
```

---

## 9. Comparison / When to Use What

| Technique | Encapsulation Type | Build Step Needed? | Shadow DOM Overhead? | Donut Scoping Support? |
| :--- | :--- | :--- | :--- | :--- |
| **Native `@scope`** | CSSOM (Runtime) | No | None (Light DOM) | **Yes (`to`)** |
| **BEM Methodology** | Naming Convention | No | None | No (manual) |
| **CSS Modules** | Class Name Hashing | Yes (Webpack/Vite) | None | No |
| **Shadow DOM** | DOM Subtree Tree | No (Native Web API) | High (isolated tree) | No (binary boundary) |

---

## 10. Interview Questions

### Q1: What is "Donut Scoping" and what problem does it solve in CSS?
<details>
<summary>View Answer</summary>
Donut Scoping refers to defining a scoping root and a scoping limit using the syntax `@scope (.root) to (.limit)`. It allows an outer component to style its internal children while explicitly excluding any DOM subtrees that match the limit selector. This solves the classic problem of nested components, ensuring an outer card does not bleed styles into an embedded author badge or sub-card.
</details>

### Q2: What is "Scoping Proximity" in the CSS Cascade?
<details>
<summary>View Answer</summary>
Scoping Proximity is a tie-breaking criterion in the CSS Cascade. When two matching rules have identical specificity and belong to the same cascade layer, the browser compares the distance (number of DOM ancestor hops) between the targeted element and the respective scoping roots. The rule whose scoping root is closer to the targeted element wins, regardless of source order in the stylesheet.
</details>

### Q3: How does the specificity of a selector inside `@scope` calculate?
<details>
<summary>View Answer</summary>
The specificity of a selector inside an `@scope` block is calculated using the selector itself plus the scoping root selector (if `:scope` is used). If a bare selector is written inside `@scope (.card) { p { ... } }`, the specificity is that of `p` $(0, 0, 1)$ plus the scoping root `.card` $(0, 1, 0)$ when evaluated, giving $(0, 1, 1)$.
</details>

### Q4: How does `@scope` differ fundamentally from Shadow DOM encapsulation?
<details>
<summary>View Answer</summary>
Shadow DOM creates an isolated document fragment that completely separates DOM nodes, preventing external CSS selectors from penetrating and breaking standard ID-based ARIA associations (`aria-labelledby`). `@scope` operates entirely within the Light DOM, providing purely stylistic boundaries while keeping DOM traversal, accessibility APIs, and inherited properties intact.
</details>

### Q5: Can `@scope` be used without passing a root selector?
<details>
<summary>View Answer</summary>
Yes. An inline `<style>` element placed directly inside an HTML element can contain an anonymous `@scope { ... }` block. In this scenario, the scoping root defaults implicitly to the parent element containing the `<style>` block, providing scoped styles for HTML fragments or server-rendered widgets.
</details>

### Q6: Does `@scope` prevent inherited properties from passing into child elements?
<details>
<summary>View Answer</summary>
No. `@scope` limits selector matching boundaries, but it does not alter standard CSS inheritance. Inherited properties (like `color`, `font-family`, or `line-height`) set on an element will still cascade down to its descendants unless explicitly reset.
</details>

### Q7: Can you nest `@scope` rules inside other `@scope` rules?
<details>
<summary>View Answer</summary>
Yes. `@scope` rules can be nested inside other `@scope` rules or combined with native CSS nesting and `@layer` declarations to create multi-tier component architectures.
</details>

### Q8: What happens in a browser that does not support `@scope`?
<details>
<summary>View Answer</summary>
Unsupporting browsers ignore the `@scope` at-rule and discard the entire block. To ensure graceful degradation, developers can use `@supports (@scope (.a))` or compile through PostCSS plugins that convert `@scope` into descendant selectors during legacy builds.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Article Card with Donut Scoping
**Objective**: Style an article card where paragraphs are styled with specific fonts and colors, but nested quote cards (`.quote-box`) remain unstyled by the article rule.
<details>
<summary>View Solution</summary>

```css
@scope (.article-card) to (.quote-box) {
  :scope {
    padding: 2rem;
    background: #ffffff;
    border-radius: 0.5rem;
  }

  p {
    font-family: 'Merriweather', serif;
    font-size: 1.125rem;
    line-height: 1.75;
    color: #334155;
  }
}
```
</details>

### Exercise 2: Scoping Proximity Navigation Test
**Objective**: Write scoped theme rules for light and dark navigation bars demonstrating that the innermost navigation always adopts the closest theme without needing `!important`.
<details>
<summary>View Solution</summary>

```css
@scope (.theme-light) {
  .nav-link {
    color: #0f172a;
    background-color: #f1f5f9;
  }
}

@scope (.theme-dark) {
  .nav-link {
    color: #f8fafc;
    background-color: #1e293b;
  }
}
```
</details>

### Exercise 3: Self-Contained Modal Component
**Objective**: Build a modal dialog styling rule using `:scope` to style the dialog window, backdrop, and internal controls cleanly.
<details>
<summary>View Solution</summary>

```css
@scope (.custom-dialog) {
  :scope {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    padding: 2rem;
    border: none;
    border-radius: 0.75rem;
    box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
    background: white;
  }

  .dialog-title {
    margin-top: 0;
    font-size: 1.25rem;
    color: #0f172a;
  }

  .close-btn {
    position: absolute;
    top: 1rem;
    right: 1rem;
    background: none;
    border: none;
    cursor: pointer;
  }
}
```
</details>

### Exercise 4: Nested Accordion Encapsulation
**Objective**: Write an accordion component rule that stops at nested sub-accordions using Donut Scoping.
<details>
<summary>View Solution</summary>

```css
@scope (.accordion) to (.accordion) {
  .accordion-trigger {
    width: 100%;
    padding: 1rem;
    font-weight: 600;
    text-align: left;
    background-color: #f8fafc;
    border: 1px solid #e2e8f0;
    cursor: pointer;

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: -2px;
    }
  }

  .accordion-panel {
    padding: 1rem;
    border: 1px solid #e2e8f0;
    border-top: none;
  }
}
```
</details>

### Exercise 5: Scoped Styling within Cascade Layers
**Objective**: Combine `@layer` and `@scope` to enforce both architectural layer precedence and modular component boundaries.
<details>
<summary>View Solution</summary>

```css
@layer components {
  @scope (.alert-box) {
    :scope {
      padding: 1rem 1.25rem;
      border-radius: 0.375rem;
      display: flex;
      gap: 0.75rem;
    }

    .alert-icon {
      flex-shrink: 0;
      width: 1.25rem;
      height: 1.25rem;
    }

    .alert-text {
      font-size: 0.875rem;
      line-height: 1.4;
    }
  }
}
```
</details>

### Exercise 6: Interactive Button States with `:scope`
**Objective**: Style a complex multi-part button group where `:scope` styles the container and internal buttons react to hover and focus.
<details>
<summary>View Solution</summary>

```css
@scope (.btn-group) {
  :scope {
    display: inline-flex;
    border-radius: 0.375rem;
    overflow: hidden;
    border: 1px solid #cbd5e1;
  }

  button {
    padding: 0.5rem 1rem;
    border: none;
    background: white;
    color: #334155;
    cursor: pointer;

    & + button {
      border-left: 1px solid #cbd5e1;
    }

    &:hover {
      background: #f1f5f9;
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: -2px;
    }
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Scoped to Root */
@scope (.card) {
  :scope { background: white; }
  h3 { color: #0f172a; }
}

/* 2. Donut Scoping (Stops at limit) */
@scope (.article) to (.author-bio) {
  p { color: #334155; }
}

/* 3. Scoping Proximity */
/* The closer scoping root in DOM tree wins tie-breaks! */
```

- `@scope` provides modular CSS encapsulation without class hashing or Shadow DOM.
- Donut scoping (`to (<selector>)`) carves holes in scoping regions to protect nested sub-components.
- Scoping Proximity breaks specificity ties by preferring the nearest DOM scoping root.
- Baseline 2024: Supported in Chrome 118+ and Safari 17.4+.

---

## 13. Further Reading

- [MDN Web Docs: @scope](https://developer.mozilla.org/en-US/docs/Web/CSS/@scope)
- [W3C CSS Cascading and Inheritance Level 6: Scoping](https://drafts.csswg.org/css-cascade-6/#scoped-styles)
- [Chrome for Developers: Limit the reach of your selectors with the CSS @scope at-rule](https://developer.chrome.com/blog/css-scope/)
- [WebKit: CSS @scope support in Safari 17.4](https://webkit.org/blog/14955/webkit-features-in-safari-17-4/)

---

### Navigation
- **Previous**: [03-is-where-and-not.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/03-is-where-and-not.md)
- **Next**: [05-container-style-queries.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/05-container-style-queries.md)
