# Naming Conventions and File Organisation

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Project Architecture, Codebase Organisation & Team Conventions |
| **Baseline Status** | Universal Architectural Frameworks |
| **Target Audience** | Frontend Leads, Design System Authors, Senior Web Developers |
| **Tested With** | Enterprise Repositories, Monorepos, Vite, Next.js, LightningCSS |

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

In multi-engineer web projects, inconsistent file organization and ad-hoc class naming are among the fastest routes to technical debt. When every developer follows their own naming style:
- Some developers write camelCase (`.userProfile`), some write snake_case (`.user_profile`), and others write kebab-case (`.user-profile`).
- Stylesheets become monolithic 3,000-line dumping grounds where nobody knows where a component's styles reside.
- Renaming or deleting components becomes terrifying because files lack clear boundaries of ownership.

A standardized file organization structure and explicit naming convention establish a **predictable mental model**:
- A developer can instantly deduce an element's architectural role simply by looking at its class name prefix.
- The directory tree mirrors the mental model of the design system, allowing new team members to onboard and contribute within hours.
- Modern bundlers (Vite, esbuild, LightningCSS) can efficiently tree-shake, bundle, and generate source maps.

---

## 2. Core Concepts

### Modern Evolution of the 7-1 Folder Pattern
Originally designed for Sass, the **7-1 Pattern** organizes stylesheets into 7 specialized folders feeding into a single root entry file (`main.css`). In modern native CSS with `@layer`, this pattern has evolved into an explicit architectural pipeline:

```
styles/
├── 0-settings/      (Design Tokens, CSS Custom Properties)
├── 1-tools/         (Custom Functions, Mixin helpers)
├── 2-generic/       (Box-sizing, Resets, Normalization)
├── 3-elements/      (Unclassed HTML tags: body, h1, a, table)
├── 4-layout/        (Macro scaffolding: grid, header, sidebar)
├── 5-components/    (Self-contained BEM widgets: card, button)
├── 6-utilities/     (Single-purpose helpers: sr-only, text-center)
└── main.css         (Root entry point / barrel file)
```

### Component Co-location vs. Centralized Styles
- **Centralized (`/src/styles/components/`)**: All CSS resides in a centralized directory. Ideal for multi-page server-rendered sites, static blogs, and traditional web platforms.
- **Co-located (`/src/components/Card/Card.css`)**: Styles sit directly next to their component template (`Card.tsx`, `Card.test.tsx`, `Card.css`). Ideal for modern component frameworks (React, Vue, Svelte, Next.js), promoting strong ownership and automated tree-shaking when components are deleted.

### Hungarian Prefixes (Functional Namespacing)
Prefixing classes with a single character immediately communicates an element's scope and intent:
- `c-`: **Component** (`.c-card`, `.c-button`) — self-contained UI widget.
- `o-`: **Object** (`.o-media`, `.o-container`) — structural layout without cosmetic skin.
- `u-`: **Utility** (`.u-text-center`, `.u-visually-hidden`) — single-purpose, high specificity.
- `is-` / `has-`: **State** (`.is-active`, `.has-error`) — transient interactive flags.
- `t-`: **Theme** (`.t-dark`, `.t-holiday`) — visual skin overrides.
- `js-`: **JavaScript Hook** (`.js-modal-trigger`) — attached exclusively for DOM queries; **NEVER** styled in CSS!

---

## 3. How It Works Internally

```
The Bundler & Barrel File Pipeline:

  1. Barrel File (main.css) defines architectural layer order:
     @layer reset, tokens, elements, layout, components, utilities;

  2. Imports modular sub-files:
     @import './0-settings/tokens.css' layer(tokens);
     @import './2-generic/reset.css' layer(reset);
     @import './5-components/button.css' layer(components);

  3. Modern Build Engine (LightningCSS / Vite):
     - Resolves dependency graph
     - Inlines @import files without runtime network waterfalls
     - Transpiles modern syntax
     - Strips dead unused utilities
     - Outputs single, optimized, minified production stylesheet!
```

```
Prefix Semantic Hierarchy Matrix:
┌────────┬──────────────────────┬────────────────────────┬─────────────────────┐
│ Prefix │ Architectural Role   │ Scope of Reach         │ Can declare margin? │
├────────┼──────────────────────┼────────────────────────┼─────────────────────┤
│ `o-`   │ Structural Object    │ Broad / Multi-context  │ Yes (Manages layout)│
│ `c-`   │ Component Widget     │ Local / Isolated       │ NO (Self-contained!)│
│ `u-`   │ Utility Helper       │ Surgical / Universal   │ Yes (Explicit intent│
│ `is-`  │ State Flag           │ Temporary / Interactive│ No                  │
│ `js-`  │ JavaScript Hook      │ Zero CSS Declarations  │ FORBIDDEN in CSS    │
└────────┴──────────────────────┴────────────────────────┴─────────────────────┘
```

---

## 4. Syntax & Usage

### Root Barrel File Pattern (`main.css`)
```css
/* 1. Define explicit Cascade Layer priority */
@layer reset, tokens, elements, layout, components, utilities;

/* 2. Import layers via @import */
@import "./0-settings/tokens.css" layer(tokens);
@import "./2-generic/reset.css" layer(reset);
@import "./3-elements/base.css" layer(elements);
@import "./4-layout/grid.css" layer(layout);
@import "./5-components/card.css" layer(components);
@import "./5-components/button.css" layer(components);
@import "./6-utilities/helpers.css" layer(utilities);
```

---

## 5. Practical Examples

### Example 1: Full Component Co-Location Architecture (React / Vite)
A production component folder containing template, CSS, tests, and types in one place.

```
src/components/Button/
├── Button.tsx
├── Button.test.tsx
├── Button.types.ts
├── Button.css
└── index.ts
```

```css
/* Button.css - Strictly scoped to this component */
@layer components {
  .c-button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 0.5rem 1rem;
    font-family: inherit;
    font-size: 0.9375rem;
    font-weight: 600;
    border: 1px solid transparent;
    border-radius: 0.375rem;
    cursor: pointer;
    text-decoration: none;
    transition: background-color 0.15s ease;

    /* Focus accessibility */
    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: 2px;
    }

    /* Modifiers */
    &--primary {
      background-color: #2563eb;
      color: #ffffff;
      &:hover { background-color: #1d4ed8; }
    }

    &--outline {
      background-color: transparent;
      border-color: #cbd5e1;
      color: #1e293b;
      &:hover { background-color: #f8fafc; }
    }

    /* States */
    &.is-loading {
      pointer-events: none;
      opacity: 0.7;
    }
  }
}
```

### Example 2: The `js-` Separation Rule in Action
Preventing design refactors from silently breaking JavaScript business logic.

```html
<!-- Clean Separation: Styling uses .c-button; JS uses .js-cart-add -->
<button class="c-button c-button--primary js-cart-add" data-product-id="492">
  Add to Cart
</button>
```

```javascript
// JavaScript event listener binds ONLY to .js- prefixed hook
const cartBtn = document.querySelector('.js-cart-add');
cartBtn.addEventListener('click', handleAddToCart);
```

```css
/* CSS strictly styles the visual class.
   .js-cart-add is NEVER styled in CSS!
   Designers can rename .c-button--primary without breaking cart functionality! */
.c-button { ... }
```

### Example 3: Enterprise Design System Barrel Structure
A comprehensive `main.css` orchestrating modular architectural layers.

```css
/* main.css */
@layer reset, tokens, elements, objects, components, utilities;

@layer reset {
  *, *::before, *::after {
    box-sizing: border-box;
    margin: 0;
  }
}

@layer tokens {
  :root {
    --color-primary: oklch(0.6 0.22 250);
    --space-unit: 1rem;
  }
}

@layer objects {
  .o-container {
    max-width: 80rem;
    margin-inline: auto;
    padding-inline: 1.5rem;
  }
}

@layer components {
  .c-dialog {
    border: none;
    border-radius: 0.5rem;
    padding: 2rem;
  }
}

@layer utilities {
  .u-hidden {
    display: none !important;
  }
}
```

### Example 4: Enforcing Strict Kebab-Case and BEM via Stylelint
Configuring automated linting to enforce naming standards across a git repository.

```json
{
  "rules": {
    "selector-class-pattern": [
      "^(c-|o-|u-|t-|is-|has-)[a-z0-9]+(-[a-z0-9]+)*(__[a-z0-9]+(-[a-z0-9]+)*)?(--[a-z0-9]+(-[a-z0-9]+)*)?$",
      {
        "message": "Class names must follow prefixed BEM: e.g. .c-card__title--featured or .u-text-center"
      }
    ]
  }
}
```

---

## 6. Best Practices

1. **Standardize on strict `kebab-case`**: Never use `camelCase` or `snake_case` in class names. Kebab-case matches standard CSS property grammar (`background-color`, `border-radius`).
2. **Never style `js-` prefixed classes**: The `js-` prefix is a sacred contract with JavaScript engineers. If a class starts with `js-`, it must never have styles attached in CSS.
3. **Co-locate component styles when using modern frameworks**: In React, Vue, Svelte, or Astro projects, keep `Component.css` in the same directory as `Component.tsx` to enable dead-code deletion when components are retired.
4. **Use barrel files (`index.css` or `main.css`) to define explicit `@layer` order**: Establish layer precedence at the top of the bundle before importing child files.
5. **Use BEM functional prefixes (`c-`, `o-`, `u-`) in design systems**: Prefixes instantly communicate whether an element is an isolated widget, a layout primitive, or a single-purpose utility.
6. **Limit file sizes to under 300 lines of CSS**: If a component stylesheet exceeds 300 lines, decompose it into smaller sub-components or extract repeated layout logic into objects.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Styling `js-` classes in CSS (`.js-modal-close { color: red; }`) | JavaScript developers refactor class name; layout silently breaks. | Style `.c-modal__close`; keep `.js-modal-close` for script events only. |
| Mixed casing: `.userCard` alongside `.user-card` | Duplication, naming confusion, and broken searchability. | Enforce strict `kebab-case` via Stylelint. |
| Monolithic 5,000-line `styles.css` file | Unmaintainable; merge conflicts; fear of deleting dead code. | Break into modular component files feeding a barrel file. |
| Deeply nested directory structures (5+ folders deep) | Developers struggle to locate files; import paths become unreadable. | Keep directory depth shallow (2–3 levels max). |
| Forgetting `box-sizing: border-box` in the reset layer | Padding unexpectedly adds to element widths across components. | Always include a universal box-sizing reset in the generic layer. |
| Using IDs for styling (`#nav`) | High specificity destroys modular reusability. | Strictly use class selectors (`.c-nav`). |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Tree Shaking & Pruning**: Co-located component styling enables bundlers (Webpack, Vite) to automatically purge styles when a component is no longer imported in the JavaScript application tree.
- **Fast Search & Code Navigation**: Predictable BEM and prefixed naming allows developers to `Ctrl+F` class names across codebases with zero false-positive matches.

### Accessibility (A11y)
- Standardized file organization ensures that accessibility utilities (like `.u-visually-hidden` and high-contrast focus tokens) are centralized, audited, and consistently reused across every component.

### Browser Compatibility
- Universal. Directory organization and naming conventions operate at the authoring and build level; the resulting CSS is standard class-based CSS compatible with all browsers.

---

## 9. Comparison / When to Use What

| Architectural Pattern | Best Used In | Maintenance Effort | Tree-Shaking Support |
| :--- | :--- | :--- | :--- |
| **Co-located Components** | React, Vue, Svelte, Next.js | Very Low (Self-contained)| Excellent (Automated) |
| **Centralized 7-1 Architecture** | Multi-page apps, Rails, Django | Moderate | Requires PurgeCSS |
| **Single Monolithic File** | Proof of concepts, tiny landing pages | High at scale | None |

---

## 10. Interview Questions

### Q1: What is the purpose of the `js-` class prefix, and why must it never be styled in CSS?
<details>
<summary>View Answer</summary>
The `js-` prefix (e.g. `.js-menu-toggle`) explicitly denotes that an element is targeted by JavaScript event listeners or DOM manipulation scripts. Styling a `js-` class in CSS couples visual presentation to JavaScript behavior. If a designer changes the button style or a frontend engineer refactors the class, the JavaScript functionality silently breaks. Keeping `js-` classes unstyled ensures that styling refactors never break application behavior.
</details>

### Q2: How does the modern 7-1 pattern interact with CSS Cascade Layers (`@layer`)?
<details>
<summary>View Answer</summary>
In legacy Sass, the 7-1 pattern relied strictly on `@import` source order to manage specificity. If files were imported out of order, the cascade broke. In modern CSS, the root barrel file declares `@layer reset, tokens, elements, layout, components, utilities;` at the very top. Even if files are loaded asynchronously or reordered by a bundler, the browser engine guarantees architectural layer priority natively.
</details>

### Q3: What is the architectural difference between an Object (`o-`) and a Component (`c-`)?
<details>
<summary>View Answer</summary>
An **Object** (`o-media`, `.o-container`, `.o-grid`) is a structural layout primitive with zero cosmetic styling (no background colors, borders, or typography). It can be reused in any visual context to arrange content. A **Component** (`c-card`, `.c-button`) is a bespoke UI widget that possesses full visual aesthetics (brand colors, borders, shadows, and internal padding).
</details>

### Q4: What are the benefits of component co-location over centralized stylesheets?
<details>
<summary>View Answer</summary>
Component co-location groups the template (`.tsx`), styles (`.css`), tests, and documentation into a single folder. Benefits include:
1. **Easy Deletion**: Deleting the component folder automatically deletes all associated CSS, eliminating dead code.
2. **Contextual Focus**: Developers working on a button don't have to navigate to a distant `/src/styles/components/` folder.
3. **Automated Tree Shaking**: Bundlers only include the CSS for components actively imported into the application bundle.
</details>

### Q5: How does a regex class pattern in Stylelint prevent codebase degradation?
<details>
<summary>View Answer</summary>
Using `selector-class-pattern`, Stylelint can enforce a strict regular expression that rejects any pull request containing non-conforming classes (e.g., rejecting camelCase, unprefixed classes, or BEM grandchild syntax). This guarantees consistency across dozens of contributing engineers automatically in CI/CD.
</details>

### Q6: What is a "barrel file" in CSS architecture?
<details>
<summary>View Answer</summary>
A barrel file (typically `main.css` or `index.css`) is a central entry point that defines the master architecture (such as `@layer` declarations) and imports all modular stylesheets across the project. The build tool consumes the barrel file to compile, bundle, and optimize the final production stylesheet.
</details>

### Q7: Why is `kebab-case` universally preferred over `camelCase` for CSS classes?
<details>
<summary>View Answer</summary>
1. CSS property names and HTML attributes are native kebab-case (`font-size`, `aria-label`).
2. CSS is historically case-insensitive in certain document modes, making camelCase prone to subtle bugs.
3. Kebab-case improves readability in compound class names (`c-user-profile-header`).
</details>

### Q8: What is the maximum recommended nesting depth when organizing component files?
<details>
<summary>View Answer</summary>
A maximum nesting depth of **2 to 3 levels**. Nesting deeper than 3 levels inflates specificity, couples styles rigidly to DOM hierarchy, and makes rules difficult to override without `!important`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Architect a Modular Barrel File
**Objective**: Create a production `main.css` file establishing `@layer` priority and importing 7-1 architectural modules.
<details>
<summary>View Solution</summary>

```css
/* main.css */
@layer reset, tokens, base, layout, components, utilities;

@import "./0-settings/tokens.css" layer(tokens);
@import "./1-generic/reset.css" layer(reset);
@import "./2-base/typography.css" layer(base);
@import "./3-layout/grid.css" layer(layout);
@import "./4-components/card.css" layer(components);
@import "./5-utilities/spacing.css" layer(utilities);
```
</details>

### Exercise 2: Refactor Mixed-Convention HTML
**Objective**: Refactor an HTML snippet with camelCase and un-prefixed classes into clean BEM with functional prefixes.
<details>
<summary>View Solution</summary>

```html
<!-- Before (Messy) -->
<div class="userCard cardActive">
  <img src="avatar.jpg" class="avatarImg">
  <span class="userName">Alex</span>
  <button class="btnSubmit jsClick">Save</button>
</div>

<!-- Refactored (Clean Architectural Standard) -->
<div class="c-user-card is-active">
  <img src="avatar.jpg" alt="Alex" class="c-user-card__avatar">
  <span class="c-user-card__name">Alex</span>
  <button class="c-button c-button--primary js-save-user">Save</button>
</div>
```
</details>

### Exercise 3: Stylelint Kebab-Case Enforcer
**Objective**: Write a Stylelint rule that enforces `kebab-case` for all class selectors.
<details>
<summary>View Solution</summary>

```json
{
  "rules": {
    "selector-class-pattern": [
      "^[a-z0-9]+(-[a-z0-9]+)*$",
      {
        "message": "Class names must be strictly lowercase kebab-case (e.g. .site-header)"
      }
    ]
  }
}
```
</details>

### Exercise 4: Co-located Card Directory Setup
**Objective**: Write the self-contained CSS file for a co-located `Card/Card.css` component using `@layer components`.
<details>
<summary>View Solution</summary>

```css
@layer components {
  .c-card {
    padding: 1.5rem;
    background-color: #ffffff;
    border: 1px solid #e2e8f0;
    border-radius: 0.5rem;
  }

  .c-card__title {
    margin-top: 0;
    font-size: 1.25rem;
    color: #0f172a;
  }

  .c-card--elevated {
    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  }
}
```
</details>

### Exercise 5: Pure CSS Layout Object
**Objective**: Create a reusable layout container object `.o-wrapper` with horizontal padding and max-width.
<details>
<summary>View Solution</summary>

```css
@layer layout {
  .o-wrapper {
    width: 100%;
    max-width: 72rem;
    margin-inline: auto;
    padding-inline: 1.5rem;
  }
}
```
</details>

### Exercise 6: State and Transition Class Contract
**Objective**: Define a dropdown component where visibility state is managed via `.is-open` and ARIA attributes.
<details>
<summary>View Solution</summary>

```css
@layer components {
  .c-dropdown-menu {
    display: none;
    position: absolute;
    top: 100%;
    left: 0;
    min-width: 12rem;
    background: white;
    border: 1px solid #cbd5e1;
    border-radius: 0.375rem;
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);

    &.is-open,
    &[aria-hidden="false"] {
      display: block;
    }
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
Functional Prefixes:
  c-   Component      .c-card, .c-button
  o-   Layout Object  .o-container, .o-grid
  u-   Utility        .u-text-center, .u-hidden
  is-  State Flag     .is-active, .is-loading
  js-  Script Hook    .js-modal-open (NEVER style in CSS!)
```

- Standardize on lowercase `kebab-case`.
- Co-locate styles next to component templates in modern frameworks.
- Use barrel files to declare `@layer` priority explicitly.
- Never style `js-` hooks; keep JavaScript decoupled from CSS.

---

## 13. Further Reading

- [Harry Roberts: More Transparent UI Code with Namespaces](https://csswizardry.com/2015/03/more-transparent-ui-code-with-namespaces/)
- [Sass Guidelines: The 7-1 Pattern](https://sass-guidelin.es/#the-7-1-pattern)
- [MDN Web Docs: @import](https://developer.mozilla.org/en-US/docs/Web/CSS/@import)
- [Stylelint Documentation: selector-class-pattern](https://stylelint.io/user-guide/rules/selector-class-pattern/)

---

### Navigation
- **Previous**: [04-cube-css-and-modern-methodologies.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/04-cube-css-and-modern-methodologies.md)
- **Next**: [06-design-tokens-and-theming.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/06-design-tokens-and-theming.md)
