# CSS Container Style Queries

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Containment Module Level 3 (W3C Working Draft / Candidate Recommendation) |
| **Baseline Status** | Limited Availability / In Progress (Chrome 107+, Safari 18+, Firefox experimental) |
| **Browsers Tested** | Chrome 107+, Edge 107+, Safari 18+ |
| **Tested With** | W3C Web Platform Tests, Chrome DevTools Rendering, WebKit Technology Preview |

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

While Container Size Queries (`@container (min-width: 400px)`) allow elements to adapt their layouts based on the **geometric dimensions** of their parent container, they cannot respond to the **stylistic context** of the container.

In component-driven architectures, child components often need to adapt based on high-level container states:
- A button needs to render with an inverted contrast palette when placed inside a dark-themed card.
- A list item needs to change its border or icon color based on a parent container's `--status: success` token.
- A typography component needs to adapt its font family and line weight depending on whether its parent is marked `--card-variant: editorial`.

Historically, this required cascading custom property overrides, redundant BEM modifier classes (e.g., `.card--dark .button--inverted`), or CSS-in-JS prop drilling.

**CSS Container Style Queries** (`@container style(...)`), specified in **CSS Containment Module Level 3**, allow child components to query the computed CSS custom properties of their ancestor containers directly in CSS.

---

## 2. Core Concepts

### Size Queries vs. Style Queries
- **Container Size Queries**: Require setting `container-type: inline-size` or `size` on the parent, which establishes a containment context that detaches intrinsic sizing.
- **Container Style Queries**: **Do not require `container-type`**! Every HTML element is inherently a style container for its descendants.

### Querying Custom Properties
The current Level 3 specification focuses primarily on querying CSS Custom Properties (variables):

```css
.card {
  --theme: dark;
}

@container style(--theme: dark) {
  .card-button {
    background-color: #ffffff;
    color: #0f172a;
  }
}
```

### Named Containers with Style Queries
Containers can be labeled with `container-name` so child queries target a specific ancestor rather than the nearest parent:

```css
.sidebar {
  container-name: sidebar-context;
  --layout-density: compact;
}

@container sidebar-context style(--layout-density: compact) {
  .widget-item {
    padding: 0.25rem 0.5rem;
    font-size: 0.8125rem;
  }
}
```

### Boolean Style Queries
Checking for the presence of a variable regardless of its value:
```css
@container style(--is-promoted) {
  .badge {
    display: inline-block;
  }
}
```

---

## 3. How It Works Internally

```
Container Style Query Resolution Pipeline:

1. Child element encounters '@container style(--theme: dark)'
2. Browser walks up the ancestor DOM chain:
   - Does direct parent define --theme: dark?
     - Yes -> Match succeeded! Apply declaration.
     - No  -> Check next parent up the tree...
3. Inherited Custom Properties:
   - Custom properties inherit down the tree by default.
   - If an ancestor defines '--theme: dark', the style query matches
     all descendants until an intervening element overrides '--theme'.
4. Invalidation Engine:
   - When JavaScript updates 'element.style.setProperty("--theme", "light")',
     the browser invalidates only matching @container subtrees.
```

```
Size Query vs. Style Query Architecture:
┌─────────────────────────┬─────────────────────────────┬─────────────────────────────┐
│ Feature                 │ Size Query (@container size)│ Style Query (@container style)
├─────────────────────────┼─────────────────────────────┼─────────────────────────────┤
│ Required Container Rule │ `container-type: inline-size│ None (Every element is a    │
│                         │ or normal`                  │ style container)            │
│ Intrinsic Sizing Impact │ Disables intrinsic sizing   │ Zero impact on layout flow  │
│ Target Evaluated        │ Width, Height, Aspect Ratio │ CSS Custom Properties       │
│ Infinite Loop Risk      │ High (layout cycles)        │ Low (acyclic property trees)│
└─────────────────────────┴─────────────────────────────┴─────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Browser Implementation Status:
├── Chrome / Edge: 107+ (Full support for custom property style queries)
├── Safari: 18+ (Supported in macOS Sequoia / iOS 18)
└── Firefox: Under active development in nightly builds
```

### Basic Syntax
```css
/* Anonymous container style query */
@container style(--variable-name: value) {
  .child-selector {
    /* Declarations */
  }
}

/* Named container style query */
@container container-name style(--variable-name: value) {
  .child-selector {
    /* Declarations */
  }
}

/* Combining multiple style queries */
@container style(--theme: dark) and style(--variant: outlined) {
  .button {
    background: transparent;
    border: 2px solid white;
  }
}
```

---

## 5. Practical Examples

### Example 1: Contextual Button Theming
Buttons automatically adapt their visual treatment based on the theme variable set on their parent container.

```html
<div class="panel" style="--panel-theme: dark;">
  <h3>Dark Card</h3>
  <button class="action-btn">Action</button>
</div>

<div class="panel" style="--panel-theme: warning;">
  <h3>Warning Card</h3>
  <button class="action-btn">Action</button>
</div>
```

```css
.panel {
  padding: 1.5rem;
  border-radius: 0.5rem;
  border: 1px solid #cbd5e1;
}

/* Default button styling */
.action-btn {
  padding: 0.5rem 1rem;
  font-family: inherit;
  font-weight: 600;
  border-radius: 0.375rem;
  border: none;
  cursor: pointer;
  background-color: #2563eb;
  color: #ffffff;
  transition: background-color 0.2s ease;
}

.action-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

/* Container Style Queries */
@container style(--panel-theme: dark) {
  .panel {
    background-color: #0f172a;
    color: #f8fafc;
  }

  .action-btn {
    background-color: #ffffff;
    color: #0f172a;
  }
}

@container style(--panel-theme: warning) {
  .panel {
    background-color: #fffbeb;
    border-color: #f59e0b;
  }

  .action-btn {
    background-color: #d97706;
    color: #ffffff;
  }
}
```

### Example 2: Dynamic Layout Density (Compact vs. Comfortable)
Adjusting data table cell heights and icon sizes based on a parent container's density setting.

```html
<section class="dashboard-widget" style="--density: compact;">
  <table class="data-table">
    <tr>
      <td>User Login Event</td>
      <td>2 minutes ago</td>
    </tr>
  </table>
</section>
```

```css
.dashboard-widget {
  container-name: widget-ctx;
  padding: 1rem;
  background: white;
  border-radius: 0.5rem;
}

.data-table {
  width: 100%;
  border-collapse: collapse;

  td {
    padding: 0.75rem 1rem;
    border-bottom: 1px solid #e2e8f0;
  }
}

/* Style query targeting named container */
@container widget-ctx style(--density: compact) {
  .data-table td {
    padding: 0.25rem 0.5rem;
    font-size: 0.8125rem;
  }
}
```

### Example 3: Card Status Modes (Success, Error, Pending)
Styling status indicators and borders without adding classes to every nested element.

```html
<div class="status-card" style="--status: error;">
  <span class="status-indicator"></span>
  <p class="status-message">Connection timed out after 30 seconds.</p>
</div>
```

```css
.status-card {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 1rem;
  border-radius: 0.375rem;
  border: 1px solid #e2e8f0;
}

.status-indicator {
  width: 0.75rem;
  height: 0.75rem;
  border-radius: 50%;
  background-color: #94a3b8;
}

@container style(--status: error) {
  .status-card {
    border-color: #ef4444;
    background-color: #fef2f2;
  }

  .status-indicator {
    background-color: #dc2626;
  }

  .status-message {
    color: #991b1b;
    font-weight: 500;
  }
}

@container style(--status: success) {
  .status-card {
    border-color: #10b981;
    background-color: #f0fdf4;
  }

  .status-indicator {
    background-color: #16a34a;
  }

  .status-message {
    color: #166534;
  }
}
```

### Example 4: Combining Size and Style Queries
Creating a responsive component that adapts based on both width and theme.

```css
.product-card {
  container-name: product;
  container-type: inline-size;
}

/* Size query */
@container product (min-width: 30rem) {
  .product-details {
    display: grid;
    grid-template-columns: 1fr 1fr;
  }
}

/* Style query */
@container product style(--highlight: premium) {
  .product-details {
    border: 2px solid #eab308;
    box-shadow: 0 10px 20px -5px rgba(234, 179, 8, 0.2);
  }
}
```

### Example 5: Progressive Enhancement Fallback
Using `@supports` to provide standard custom property fallbacks for browsers lacking style query support.

```css
/* Standard custom property fallback */
.badge {
  background-color: var(--badge-bg, #e2e8f0);
  color: var(--badge-text, #334155);
}

/* Progressive enhancement with style queries */
@supports (container-type: normal) {
  @container style(--badge-theme: alert) {
    .badge {
      background-color: #ef4444;
      color: #ffffff;
      font-weight: 700;
    }
  }
}
```

---

## 6. Best Practices

1. **Use style queries for contextual design token variations**: Instead of writing deep combinators like `.dark-mode .card .title`, query the token: `@container style(--mode: dark)`.
2. **Assign `container-name` when targeting specific ancestors**: Using named containers prevents style queries from accidentally matching an unexpected intermediate parent that defines the same variable.
3. **Do not declare `container-type` unless size querying is also required**: Style queries operate on any standard element; declaring unnecessary `container-type: inline-size` triggers containment that can disrupt intrinsic sizing.
4. **Use boolean checks for feature flags**: Use `@container style(--has-icons)` as a clean feature flag to reveal or conceal UI embellishments.
5. **Always provide accessible fallbacks**: Because style queries are newly supported in Safari 18 and Blink, provide baseline styles using standard CSS custom property fallbacks.
6. **Keep custom property values semantic**: Use clear, string-like identifiers (e.g. `--theme: dark`, `--density: compact`, `--status: active`) rather than ambiguous numbers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Expecting style queries to evaluate standard properties (`style(color: red)`) | Fails in all browsers. Current implementations only support querying custom properties (`--*`). | Query CSS variables: `style(--theme-color: red)`. |
| Adding `container-type: normal` unnecessarily | Harmless, but unnecessary boilerplate; all elements are already style containers. | Omit `container-type` for pure style queries. |
| Querying a variable on an element from within that same element | A container query evaluates the **parent container**, not the element itself. | Set the variable on the parent container. |
| Forgetting variable inheritance | Inherited variables trigger style queries on deep descendants unless overridden. | Explicitly reset or isolate variables where appropriate. |
| Missing quotes on complex string values | Complex token evaluation can fail or parse as unquoted tokens. | Stick to simple identifier tokens without whitespace. |
| Assuming full cross-browser availability in 2024 | Firefox lacks default enabled support (under active development). | Use `@supports` or custom property fallbacks for Firefox. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- Container style queries do not incur layout calculation penalties because they query the computed value tree rather than physical geometric bounds.
- Style recalculation occurs only when the queried custom property changes.

### Accessibility (A11y)
- When using style queries to invert container themes or colors, always verify that the resulting text and background combinations maintain WCAG AA minimum contrast ($4.5:1$ for normal text).

### Browser Compatibility
- **Chromium (Chrome / Edge)**: Supported since version 107 (October 2022).
- **WebKit (Safari)**: Supported since Safari 18 (September 2024).
- **Gecko (Firefox)**: In active development.
- **Feature Detection**:
```css
@supports (container-type: normal) {
  /* Container queries supported */
}
```

---

## 9. Comparison / When to Use What

| Feature | Query Target | Container-Type Required? | Performance Cost |
| :--- | :--- | :--- | :--- |
| **Container Size Queries** | Width / Height / Aspect Ratio | Yes (`inline-size` or `size`) | Recalculates on resize |
| **Container Style Queries** | Custom Properties (`--var`) | No (all elements qualify) | Recalculates on style change |
| **Media Queries** | Global Viewport Dimensions | No | Minimal (global) |
| **CSS Custom Properties** | Direct Cascade Inheritance | No | Zero query overhead |

---

## 10. Interview Questions

### Q1: Can container style queries query standard CSS properties like `display` or `color`?
<details>
<summary>View Answer</summary>
While the CSS Containment Module Level 3 specification defines style queries for standard CSS properties (such as `@container style(display: flex)`), no browser currently implements standard property queries due to the risk of infinite cyclic dependencies. Current production implementations (Chrome 107+, Safari 18+) exclusively support querying CSS Custom Properties (e.g., `@container style(--theme: dark)`).
</details>

### Q2: Do you need to declare `container-type` on an element to use it as a style container?
<details>
<summary>View Answer</summary>
No. In modern CSS, every element in the DOM tree is automatically a style container for its descendants. Unlike size queries, which require `container-type: inline-size` or `size` to establish containment, style queries work out of the box on any parent element.
</details>

### Q3: Why does setting `--theme: dark` on `.button` fail to trigger `@container style(--theme: dark)` on that same `.button`?
<details>
<summary>View Answer</summary>
Container queries always evaluate an element's **ancestor containers**, never the element itself. To style `.button` via a container query, `--theme: dark` must be defined on an ancestor element (such as `.card` or `body`).
</details>

### Q4: How does container naming with `container-name` interact with style queries?
<details>
<summary>View Answer</summary>
When multiple ancestors define the same custom property, an anonymous query `@container style(...)` matches against the nearest ancestor. By declaring `container-name: card-ctx` on a specific ancestor, a child can use `@container card-ctx style(...)` to query that specific ancestor directly, skipping any intermediate containers.
</details>

### Q5: Can container size queries and style queries be combined in the same at-rule?
<details>
<summary>View Answer</summary>
Yes. You can combine size conditions and style conditions using logical operators:
```css
@container (min-width: 400px) and style(--variant: highlighted) {
  /* Declarations */
}
```
In this scenario, the target container must have `container-type` defined to satisfy the size portion of the query.
</details>

### Q6: How do container style queries differ from standard CSS custom property inheritance?
<details>
<summary>View Answer</summary>
With standard inheritance, custom properties pass values down the tree for direct use in properties (`color: var(--primary)`). However, inheritance cannot conditionally alter rules, switch layouts, or toggle pseudo-elements. Container style queries allow entire rule blocks, layouts, borders, and animations to conditionally apply based on the variable's value.
</details>

### Q7: What happens if a style query evaluates a custom property that has not been defined?
<details>
<summary>View Answer</summary>
If the queried custom property is not defined on any ancestor, the query evaluates to false, and the styles inside the `@container` block are completely ignored.
</details>

### Q8: What is the current browser support status for container style queries?
<details>
<summary>View Answer</summary>
Container style queries for custom properties are supported in Chromium browsers (Chrome/Edge 107+) and WebKit (Safari 18+). Firefox has implementation underway. Full Baseline availability across all engines is expected once Firefox ships support.
</details>

---

## 11. Practice Exercises

### Exercise 1: Dark Mode Card Inversion
**Objective**: Create a card component that inverts its background, border, and button styles whenever an ancestor sets `--theme: dark`.
<details>
<summary>View Solution</summary>

```css
.card {
  padding: 1.5rem;
  border-radius: 0.5rem;
  border: 1px solid #e2e8f0;
  background-color: #ffffff;
  color: #1e293b;
}

@container style(--theme: dark) {
  .card {
    background-color: #0f172a;
    border-color: #334155;
    color: #f8fafc;
  }
}
```
</details>

### Exercise 2: Alert Banner Variant Engine
**Objective**: Build an alert banner whose icons and border accents change according to `--alert-type: info`, `--alert-type: success`, or `--alert-type: error`.
<details>
<summary>View Solution</summary>

```css
.alert-box {
  padding: 1rem;
  border-left: 4px solid #94a3b8;
  background-color: #f8fafc;
}

@container style(--alert-type: info) {
  .alert-box {
    border-left-color: #3b82f6;
    background-color: #eff6ff;
  }
}

@container style(--alert-type: success) {
  .alert-box {
    border-left-color: #10b981;
    background-color: #f0fdf4;
  }
}

@container style(--alert-type: error) {
  .alert-box {
    border-left-color: #ef4444;
    background-color: #fef2f2;
  }
}
```
</details>

### Exercise 3: Compact Toolbar Item Sizing
**Objective**: Style toolbar buttons to shrink their padding and hide text labels when a parent toolbar specifies `--toolbar-mode: compact`.
<details>
<summary>View Solution</summary>

```css
.tool-btn {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.5rem 1rem;
  border: 1px solid #cbd5e1;
  background: white;
  cursor: pointer;

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }
}

@container style(--toolbar-mode: compact) {
  .tool-btn {
    padding: 0.25rem 0.5rem;

    .btn-label {
      display: none;
    }
  }
}
```
</details>

### Exercise 4: Named Container Priority Check
**Objective**: Target an outer dashboard wrapper named `dash-ctx` rather than an immediate widget card when querying `--system-tier: enterprise`.
<details>
<summary>View Solution</summary>

```css
.dashboard-wrapper {
  container-name: dash-ctx;
}

@container dash-ctx style(--system-tier: enterprise) {
  .pro-badge {
    display: inline-block;
    background-color: #7c3aed;
    color: white;
    padding: 0.25rem 0.5rem;
    border-radius: 0.25rem;
  }
}
```
</details>

### Exercise 5: Boolean Flag Reveal Pattern
**Objective**: Build a user profile card that reveals an avatar verification checkmark only when the parent container has `--verified` defined.
<details>
<summary>View Solution</summary>

```css
.verified-icon {
  display: none;
  width: 1.25rem;
  height: 1.25rem;
  color: #0284c7;
}

@container style(--verified) {
  .verified-icon {
    display: inline-block;
  }
}
```
</details>

### Exercise 6: Multi-Condition Container Style Query
**Objective**: Write a container query that applies styles only when a parent container has `--theme: dark` AND `--layout: grid`.
<details>
<summary>View Solution</summary>

```css
@container style(--theme: dark) and style(--layout: grid) {
  .gallery-item {
    background-color: #1e293b;
    border: 1px solid #475569;
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3);
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Custom Property Query */
@container style(--theme: dark) {
  .button { background: white; color: black; }
}

/* 2. Named Container Query */
@container my-sidebar style(--density: tight) {
  .list-item { padding: 0.25rem; }
}

/* 3. Combined Logic */
@container style(--variant: outline) and style(--color: danger) {
  .card { border-color: red; }
}
```

- Style queries inspect CSS custom properties on ancestor elements.
- No `container-type` is required; all elements act as style containers.
- Queries evaluate parent ancestors, never the element itself.
- Supported in Chrome 107+ and Safari 18+; Firefox underway.

---

## 13. Further Reading

- [MDN Web Docs: CSS Container Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_containment/Container_queries)
- [W3C CSS Containment Module Level 3](https://drafts.csswg.org/css-contain-3/#style-container)
- [Chrome for Developers: Style queries for custom properties](https://developer.chrome.com/docs/css-ui/style-queries)
- [WebKit: CSS Container Style Queries](https://webkit.org/blog/15444/webkit-features-in-safari-18-0/)

---

### Navigation
- **Previous**: [04-scope-rule.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/04-scope-rule.md)
- **Next**: [06-anchor-positioning.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/06-anchor-positioning.md)
