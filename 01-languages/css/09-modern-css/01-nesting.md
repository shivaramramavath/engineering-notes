# CSS Native Nesting

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Nesting Module Level 1 (W3C Candidate Recommendation) |
| **Baseline Status** | Baseline 2023 (Widely Available across all evergreen browsers) |
| **Browsers Tested** | Chrome 120+, Firefox 117+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C Web Platform Tests, Chromium Rendering Engine, Playwright |

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

For nearly two decades, CSS preprocessors like Sass, Less, and Stylus maintained dominance in frontend development primarily because of one feature: **nesting**. Nesting enables developers to visually organize related style declarations hierarchically, matching the mental model of the underlying HTML tree, reducing selector repetition, and scoping component styles.

However, preprocessors introduced friction:
- Build step dependencies and compiler performance overhead (e.g., node-sass / dart-sass).
- Source map mismatches during debugging in browser developer tools.
- Bloated output CSS caused by runaway nesting depth (the classic "Sass Inception" anti-pattern).

The **CSS Nesting Module Level 1** introduces native nesting directly into the CSS parsing engine of modern web browsers. Officially achieving Baseline 2023 status, CSS native nesting eliminates the necessity of preprocessors for structural hierarchy while enforcing standardized specificity algorithms governed by `:is()`.

---

## 2. Core Concepts

### The Nesting Selector (`&`)
In CSS native nesting, the ampersand symbol (`&`) represents the parent selector:
- When used at the start of a nested rule (e.g. `&:hover`), it acts as a combinator or modifier attached to the parent.
- When used with descendant selectors (e.g. `& .sub-item`), it explicitly denotes a descendant relationship.
- When placed after a selector (e.g. `.dark-mode &`), it shifts the parent to a child role within an ancestor context.

### Relaxed Nesting (The 2023 Spec Update)
In the initial 2021 specification draft, nested selectors were strictly required to begin with a symbol (`&`, `.`, `#`, `:`, `[`, `*`). Starting a nested rule with a direct HTML tag name (such as `p` or `span`) was invalid because it was difficult for naive parsers to distinguish property names from tag selectors.

In mid-2023, the CSS Working Group resolved to relax this parsing rule:
```css
/* Fully valid in all modern browsers today */
.article {
  color: #1e293b;

  p {
    line-height: 1.6;
  }
}
```
The browser's parser uses lookahead tokenization to detect whether a token sequence is followed by a declaration colon `:` or a nested rule block `{`.

### The `:is()` Desugaring Principle
Internally, the browser does **not** perform plain string concatenation like Sass. Instead, native CSS nesting desugars nested selectors by wrapping the parent selector in the `:is()` functional pseudo-class:

```css
/* Authored CSS */
.card, .banner {
  & p {
    color: red;
  }
}

/* Internal Browser Representation */
:is(.card, .banner) p {
  color: red;
}
```
Because of this mechanism:
1. Specificity is calculated according to `:is()` rules (the highest specificity among the arguments).
2. It is **impossible** to concatenate strings to form class names (e.g., `&__element` from BEM does NOT work in native CSS).

---

## 3. How It Works Internally

```
Authoring Nesting:
  .alert {
    background-color: #fef2f2;
    &.alert-danger { color: #dc2626; }
    p { margin-block: 0.5rem; }
  }

Browser Parsing & Desugaring:
  1. Tokenizer encounters '.alert' rule set.
  2. Parses declarations: 'background-color: #fef2f2;'.
  3. Detects nested selector '&.alert-danger'.
     - Translates '&' to reference current parent style rule.
     - Evaluates as ':is(.alert).alert-danger'.
  4. Detects nested element selector 'p'.
     - Implicitly inserts '&' descendant relationship.
     - Evaluates as ':is(.alert) p'.
  5. Inserts rules into CSSOM under parent rule hierarchy.
```

```
Native CSS Nesting vs. Preprocessor (Sass) Internal Resolution:
┌─────────────────────┬────────────────────────────────┬────────────────────────────────┐
│ Feature             │ Native CSS Nesting             │ Preprocessor (Sass / Less)     │
├─────────────────────┼────────────────────────────────┼────────────────────────────────┤
│ Evaluation Time     │ Runtime in Browser Engine      │ Compile time in Node/Dart CLI  │
│ Selector Resolution │ Encapsulated via :is(parent)   │ Direct string concatenation    │
│ Class Concatenation │ INVALID (`&__element` fails)   │ Supported (`&__element`)       │
│ Specificity Impact  │ Inherits :is() max specificity │ Exact concatenated specificity │
│ Media Query Nesting │ Native (preserves hierarchy)   │ Bubble-up flattened emission   │
└─────────────────────┴────────────────────────────────┴────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2023 Support Box:
├── Chrome / Edge: 120+ (Relaxed parsing; direct element nesting supported)
├── Firefox: 117+ (Relaxed parsing since v117)
└── Safari: 17.2+ (Relaxed parsing supported)
```

### Basic Syntax Forms
```css
/* 1. Implicit Descendant Nesting */
.parent {
  color: black;

  .child {
    color: blue;
  }

  span {
    font-weight: bold;
  }
}

/* 2. Pseudo-classes and Modifiers using '&' */
.button {
  background-color: #2563eb;

  &:hover {
    background-color: #1d4ed8;
  }

  &.is-active {
    box-shadow: 0 0 0 2px white;
  }
}

/* 3. Ancestor / Contextual Inversion */
.theme-dark {
  /* Parent context inversion */
  .panel & {
    background-color: #0f172a;
  }
}

/* 4. Nested At-Rules (@media, @supports, @layer) */
.card {
  padding: 1rem;

  @media (min-width: 768px) {
    padding: 2rem;
  }
}
```

---

## 5. Practical Examples

### Example 1: Encapsulated Component Hierarchy
A complete interactive component demonstrating pseudo-classes, modifiers, and descendant nesting.

```html
<article class="product-card is-featured">
  <div class="badge">Sale</div>
  <h3 class="title">Wireless Noise-Cancelling Headphones</h3>
  <p class="description">Experience pure studio sound with active cancellation.</p>
  <button class="buy-btn">Purchase Now</button>
</article>
```

```css
.product-card {
  position: relative;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  padding: 1.5rem;
  background-color: #ffffff;
  transition: transform 0.2s ease, box-shadow 0.2s ease;

  /* Sibling/Descendant elements */
  .badge {
    position: absolute;
    top: 1rem;
    right: 1rem;
    background-color: #ef4444;
    color: #ffffff;
    padding: 0.25rem 0.5rem;
    font-size: 0.75rem;
    font-weight: 700;
    border-radius: 0.25rem;
  }

  .title {
    margin: 0 0 0.5rem;
    font-size: 1.25rem;
    color: #0f172a;
  }

  p {
    color: #64748b;
    line-height: 1.5;
  }

  /* Modifiers on the parent card */
  &.is-featured {
    border-color: #3b82f6;
    box-shadow: 0 10px 15px -3px rgba(59, 130, 246, 0.1);
  }

  /* State pseudo-classes */
  &:hover {
    transform: translateY(-4px);
    box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);
  }

  /* Nested button */
  .buy-btn {
    margin-top: 1rem;
    padding: 0.5rem 1rem;
    background-color: #2563eb;
    color: white;
    border: none;
    border-radius: 0.375rem;
    cursor: pointer;

    &:hover {
      background-color: #1d4ed8;
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: 2px;
    }
  }
}
```

### Example 2: Ancestor Context Inversion (Theming)
Placing the ampersand `&` after an ancestor selector to conditionally style a component when it resides inside a dark theme or high-contrast container.

```html
<div class="theme-dark">
  <div class="user-profile">
    <h4>User Settings</h4>
  </div>
</div>
```

```css
.user-profile {
  background-color: #f8fafc;
  color: #1e293b;
  padding: 1.25rem;
  border-radius: 0.5rem;

  h4 {
    margin: 0;
  }

  /* When .user-profile is inside a .theme-dark container */
  .theme-dark & {
    background-color: #0f172a;
    color: #f8fafc;
    border: 1px solid #334155;
  }
}
```

### Example 3: Nested Media Queries and Container Queries
Encapsulating responsive viewport breakpoints and container dimensions inside individual components.

```css
.responsive-navbar {
  display: flex;
  flex-direction: column;
  padding: 1rem;
  background-color: #ffffff;

  .nav-links {
    display: none;
    flex-direction: column;
    gap: 0.75rem;
  }

  /* Viewport query nested cleanly inside the component */
  @media (min-width: 64rem) {
    flex-direction: row;
    align-items: center;
    justify-content: space-between;

    .nav-links {
      display: flex;
      flex-direction: row;
    }
  }
}
```

### Example 4: Compound Pseudo-Elements and Form Field States
Nesting states on complex inputs like floating label patterns.

```html
<div class="input-field">
  <input type="text" id="email-in" placeholder=" " required>
  <label for="email-in">Email Address</label>
</div>
```

```css
.input-field {
  position: relative;
  margin-top: 1.5rem;

  input {
    width: 100%;
    padding: 0.75rem;
    border: 1px solid #cbd5e1;
    border-radius: 0.375rem;

    &:focus {
      border-color: #2563eb;
      outline: none;
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: 1px;
    }

    /* Target label when input is focused or not empty */
    &:focus ~ label,
    &:not(:placeholder-shown) ~ label {
      transform: translateY(-1.5rem) scale(0.85);
      color: #2563eb;
    }
  }

  label {
    position: absolute;
    left: 0.75rem;
    top: 0.75rem;
    color: #94a3b8;
    pointer-events: none;
    transition: transform 0.2s ease, color 0.2s ease;
  }
}
```

### Example 5: Combining Nesting with Cascade Layers
Nesting rules inside `@layer` blocks to enforce cascade architecture.

```css
@layer components {
  .nav-menu {
    list-style: none;
    padding: 0;

    li {
      display: inline-block;

      a {
        text-decoration: none;
        color: #475569;

        &:hover {
          color: #0284c7;
        }

        &[aria-current="page"] {
          font-weight: 700;
          color: #0369a1;
        }
      }
    }
  }
}
```

---

## 6. Best Practices

1. **Avoid nesting more than 3 levels deep**: The "Inception" anti-pattern is just as harmful in native CSS as it was in Sass. Deep nesting inflates specificity and couples styles rigidly to the DOM structure.
2. **Do not attempt BEM string concatenation**: Remember that `&__element` or `&--modifier` does NOT work in native CSS. Write explicit class names: `.card { .card__element { ... } }` or `.card { & .card__element { ... } }`.
3. **Use `&` intentionally for pseudo-classes**: Always prepend `&` when attaching pseudo-classes (`&:hover`, `&:focus-visible`, `&:disabled`) or modifier classes (`&.is-open`).
4. **Group media queries inside the relevant component**: Rather than maintaining a giant disconnected `@media` block at the bottom of the file, nest media queries inside their respective components.
5. **Keep layout and parent-child coupling loose**: Nest elements only when they have an intrinsic semantic coupling (e.g., `.accordion > .accordion-item`). Avoid nesting unrelated utility classes.
6. **Prefer class selectors over bare tag selectors in nested blocks**: While direct tag nesting (`p { ... }`) is valid under relaxed nesting, class selectors (`.content-body { ... }`) are less prone to unintended cascade pollution.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Writing BEM string concatenation: `&__title` | Selector is completely ignored by browser parser as invalid syntax. | Write full descendant selector: `.card { .card__title { ... } }`. |
| Missing `&` before adjacent class: `.button { .active { ... } }` | Interprets as descendant `.button .active` instead of compound `.button.active`. | Write `&.active` with the ampersand attached. |
| Forgetting specificity effects of `:is()` | A nested rule with multiple parents takes the specificity of the most specific parent. | Keep parent selector lists uniform in specificity. |
| Over-nesting DOM structures (4+ levels) | Styles become brittle and impossible to override without `!important`. | Refactor deeply nested components into flat, modular classes. |
| Missing space when descendant intended: `&.item` vs `& .item` | `&.item` targets `<div class="card item">`, whereas `& .item` targets `<div class="item">` inside card. | Use spaces intentionally between `&` and child classes. |
| Neglecting focus outlines when removing focus rings in nested rules | Fails accessibility audits (WCAG 2.4.7). | Always pair `outline: none` with `:focus-visible` rules. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- Native CSS nesting parses faster than preprocessor output because the browser engine parses nested rule trees directly in C++ / Rust without redundant selector tokenization.
- By structuring CSS into component hierarchies, browsers can optimize style invalidation scopes during DOM mutations.

### Accessibility (A11y)
- Nested styles frequently target interactive pseudo-classes (`:hover`, `:active`, `:focus-visible`). Ensure that interactive styling nested under hover states is duplicated or matched under keyboard focus states.

### Browser Compatibility
- **Baseline 2023**: Fully supported across Chromium (120+), Firefox (117+), and WebKit / Safari (17.2+).
- **Feature Detection**:
```css
@supports selector(& p) {
  /* Native CSS nesting is supported */
}
```

---

## 9. Comparison / When to Use What

| Feature | Native CSS Nesting | Sass / SCSS Nesting | CSS Modules |
| :--- | :--- | :--- | :--- |
| **Compiler Requirement** | None (runs in browser) | Node/Dart Sass compiler | Webpack/Vite bundler plugin |
| **BEM String Concat (`&__elt`)** | No (syntax error) | Yes | No |
| **Internal Mechanism** | `:is(parent)` desugaring | Raw selector string concat | Unique class name hashing |
| **Build Speed** | Instant (zero build time) | Adds compile overhead | Minimal bundler overhead |
| **Source Maps Needed?** | No (direct line debugging)| Yes | Yes |

---

## 10. Interview Questions

### Q1: How does native CSS nesting desugar nested selectors under the hood?
<details>
<summary>View Answer</summary>
The browser engine desugars nested selectors by wrapping the parent selector list into the functional pseudo-class `:is()`. For example, `.parent { .child { color: red; } }` is evaluated by the browser as `:is(.parent) .child { color: red; }`.
</details>

### Q2: Why does Sass-style BEM string concatenation (`&__header`) fail in native CSS?
<details>
<summary>View Answer</summary>
In Sass, nesting is a compile-time string manipulation step that produces flattened string output prior to browser evaluation. In native CSS, the browser tokenizes stylesheets directly into the CSSOM. The ampersand `&` is a selector node (representing the `:is()` construct of the parent), not a text macro. Consequently, attaching text directly to `&` creates an invalid token sequence.
</details>

### Q3: How was the CSS nesting specification updated between the initial 2021 draft and the 2023 relaxed specification?
<details>
<summary>View Answer</summary>
In the original draft, every nested rule had to begin with an explicit non-identifier symbol (such as `&`, `.`, `#`, `:`, `[`, `*`). Nested element selectors required an explicit ampersand (e.g. `& p { ... }`). In 2023, the CSSWG updated the parser grammar with lookahead tokenization to distinguish properties from tag selectors, allowing direct element nesting (e.g. `.card { p { ... } }`).
</details>

### Q4: How does native nesting handle specificity when multiple selectors are listed on the parent?
<details>
<summary>View Answer</summary>
Because parent selectors are wrapped in `:is()`, the specificity of the parent reference in nested rules equals the highest specificity among the selectors in the parent list. For example, in `#header, .banner { p { ... } }`, the parent reference has the specificity of `#header` (1,0,0) even when matching within `.banner`.
</details>

### Q5: Can you place the ampersand `&` after another selector in a nested rule, and what does it achieve?
<details>
<summary>View Answer</summary>
Yes. Placing `&` after another selector (e.g. `body.dark-mode &`) inverts the relationship, making the nested rule match when the parent element is a descendant of the specified ancestor. This provides clean contextual theming or state overrides without moving outside the component's CSS block.
</details>

### Q6: What happens when an at-rule such as `@media` is nested inside a rule?
<details>
<summary>View Answer</summary>
When `@media` is nested inside a selector rule, the browser treats it as if the selector is nested inside the `@media` query. The declarations inside the nested `@media` apply only when both the media condition is true and the parent selector matches.
</details>

### Q7: What is the specificity of `.card { &:hover { ... } }`?
<details>
<summary>View Answer</summary>
The parent `.card` has a specificity of (0, 1, 0). The pseudo-class `:hover` has a specificity of (0, 1, 0). Combined, `&:hover` evaluates to `:is(.card):hover`, resulting in a compound specificity of (0, 2, 0).
</details>

### Q8: Does native CSS nesting support multiple levels of nesting?
<details>
<summary>View Answer</summary>
Yes, native nesting supports arbitrary nesting depth. However, architectural best practices recommend limiting nesting to 2 or 3 levels to avoid high selector specificity, tight DOM coupling, and maintenance friction.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Scss Nesting to Native CSS
**Objective**: Convert a legacy SCSS button block with pseudo-classes and media queries to standards-compliant native CSS.
<details>
<summary>View Solution</summary>

```css
.btn-primary {
  display: inline-flex;
  align-items: center;
  padding: 0.625rem 1.25rem;
  font-size: 0.875rem;
  font-weight: 600;
  color: #ffffff;
  background-color: #2563eb;
  border: 1px solid transparent;
  border-radius: 0.375rem;
  cursor: pointer;
  transition: background-color 0.2s ease;

  &:hover {
    background-color: #1d4ed8;
  }

  &:active {
    background-color: #1e40af;
  }

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }

  &:disabled {
    opacity: 0.6;
    cursor: not-allowed;
  }

  @media (min-width: 768px) {
    padding: 0.75rem 1.5rem;
    font-size: 1rem;
  }
}
```
</details>

### Exercise 2: Contextual High-Contrast Override
**Objective**: Build a callout alert box that changes its background and borders when rendered inside a high-contrast container.
<details>
<summary>View Solution</summary>

```css
.callout-box {
  padding: 1rem;
  background-color: #eff6ff;
  border-left: 4px solid #3b82f6;
  color: #1e3a8a;

  h4 {
    margin: 0 0 0.5rem;
    font-size: 1rem;
  }

  /* Invert when inside .high-contrast */
  .high-contrast & {
    background-color: #000000;
    border-left: 4px solid #ffffff;
    color: #ffffff;
  }
}
```
</details>

### Exercise 3: Nested Dropdown Menu Navigation
**Objective**: Style a nested multi-level navigation menu with hover-revealed sub-menus using native nesting.
<details>
<summary>View Solution</summary>

```css
.site-nav {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  gap: 1.5rem;

  li {
    position: relative;

    a {
      text-decoration: none;
      color: #334155;
      padding: 0.5rem 0;
      display: inline-block;

      &:hover {
        color: #0284c7;
      }
    }

    .submenu {
      display: none;
      position: absolute;
      top: 100%;
      left: 0;
      min-width: 12rem;
      background-color: #ffffff;
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
      border-radius: 0.375rem;
      padding: 0.5rem 0;
      list-style: none;
    }

    &:hover > .submenu,
    &:focus-within > .submenu {
      display: block;
    }
  }
}
```
</details>

### Exercise 4: Nested Element and Adjacent Sibling Spacing
**Objective**: Build a content article where adjacent paragraphs automatically get spacing using sibling operators inside a nested block.
<details>
<summary>View Solution</summary>

```css
.prose-content {
  font-family: Georgia, serif;
  font-size: 1.125rem;
  line-height: 1.75;
  color: #334155;

  p {
    margin-block: 0;

    & + p {
      margin-top: 1.25rem;
    }
  }

  blockquote {
    border-left: 3px solid #cbd5e1;
    padding-left: 1rem;
    margin-inline: 0;
    font-style: italic;
    color: #64748b;
  }
}
```
</details>

### Exercise 5: Tab Component with Active Indicator
**Objective**: Construct an accessible tab bar with nested active state styling and bottom border positioning.
<details>
<summary>View Solution</summary>

```css
.tab-group {
  display: flex;
  border-bottom: 2px solid #e2e8f0;

  .tab-item {
    padding: 0.75rem 1.25rem;
    background: none;
    border: none;
    font-weight: 500;
    color: #64748b;
    cursor: pointer;
    position: relative;

    &:hover {
      color: #1e293b;
    }

    &[aria-selected="true"] {
      color: #2563eb;
      font-weight: 600;

      &::after {
        content: '';
        position: absolute;
        bottom: -2px;
        left: 0;
        right: 0;
        height: 2px;
        background-color: #2563eb;
      }
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: -2px;
    }
  }
}
```
</details>

### Exercise 6: Card with Nested Container Query
**Objective**: Write a self-contained responsive card that shifts from single-column to two-column when its own container width exceeds 400px.
<details>
<summary>View Solution</summary>

```css
.adaptive-card-wrapper {
  container-type: inline-size;

  .adaptive-card {
    display: flex;
    flex-direction: column;
    gap: 1rem;
    padding: 1rem;
    border: 1px solid #cbd5e1;
    border-radius: 0.5rem;

    .card-thumb {
      width: 100%;
      height: 10rem;
      object-fit: cover;
      border-radius: 0.375rem;
    }

    @container (min-width: 25rem) {
      flex-direction: row;

      .card-thumb {
        width: 8rem;
        height: 8rem;
      }
    }
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
.parent {
  /* 1. Declarations for .parent */
  color: #1e293b;

  /* 2. Direct child/descendant element nesting */
  h2 { font-size: 1.5rem; }
  p { line-height: 1.6; }

  /* 3. Pseudo-classes & pseudo-elements require '&' */
  &:hover { background: #f8fafc; }
  &::before { content: ''; }

  /* 4. Compound modifier classes require '&' */
  &.is-selected { border-color: #2563eb; }

  /* 5. Direct descendant combinator */
  & > .child { font-weight: bold; }

  /* 6. Inverted ancestor contextual styling */
  .dark-mode & { color: #f8fafc; }

  /* 7. Nested @media and @container rules */
  @media (min-width: 768px) {
    padding: 2rem;
  }
}
```

- Native nesting desugars via `:is(parent)`.
- No string concatenation: `&__elt` fails.
- Direct element nesting (`p { ... }`) is fully standard under Baseline 2023 relaxed rules.
- Supported cross-browser in all modern engines.

---

## 13. Further Reading

- [MDN Web Docs: CSS Nesting](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_nesting)
- [W3C CSS Nesting Module Level 1](https://www.w3.org/TR/css-nesting-1/)
- [Chrome for Developers: CSS nesting gets relaxed](https://developer.chrome.com/blog/css-nesting-relaxed)
- [WebKit: CSS Nesting and the Cascade](https://webkit.org/blog/13607/css-nesting-and-the-cascade/)

---

### Navigation
- **Previous**: [README.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/README.md)
- **Next**: [02-has-selector-and-relational-styling.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/02-has-selector-and-relational-styling.md)
