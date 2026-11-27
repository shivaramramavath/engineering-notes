# BEM, OOCSS, SMACSS, and ITCSS

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Established Methodologies & CSS Architecture Standards |
| **Baseline Status** | Universal Architectural Frameworks |
| **Target Audience** | Frontend Engineers, Design System Leads, Senior Web Developers |
| **Tested With** | Enterprise Design Systems, Production Codebases (Bootstrap, GitHub Primer, Salesforce Lightning) |

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

Before modern CSS primitives like Cascade Layers (`@layer`) and scoped encapsulation, developers had to build mental models and strict naming conventions to tame the global cascade. Without agreed-upon conventions:
- Class names collided constantly across disparate parts of the codebase.
- Specificity spiraled out of control as developers chained selectors to override each other.
- Stylesheets lacked structural organization, making it impossible to know where to find or add rules.

Four landmark methodologies emerged between 2009 and 2015 to bring engineering discipline to CSS:
1. **OOCSS (Object-Oriented CSS)** by Nicole Sullivan (2009): Pioneered the separation of structure from skin, and container from content.
2. **SMACSS (Scalable and Modular Architecture for CSS)** by Jonathan Snook (2011): Introduced systematic categorization (Base, Layout, Module, State, Theme).
3. **BEM (Block, Element, Modifier)** by Yandex (2010): Created a strict, self-documenting naming convention that flattened specificity to $(0, 1, 0)$.
4. **ITCSS (Inverted Triangle CSS)** by Harry Roberts (2014): Organized stylesheets into a visual triangle where specificity and reach increase progressively from top to bottom.

Even in the era of native CSS nesting and `@layer`, these four methodologies form the intellectual bedrock of every modern CSS framework and design system.

---

## 2. Core Concepts

### 1. OOCSS (Object-Oriented CSS)
OOCSS established two foundational principles:
- **Separate Structure from Skin**: Define geometry (padding, dimensions, overflow) separately from aesthetics (colors, backgrounds, borders). A `.button` object manages layout; `.button-primary` applies the blue theme.
- **Separate Container from Content**: An element should look the same regardless of where it is placed. Avoid location-dependent selectors like `.sidebar h2` or `.footer a`.

### 2. SMACSS (Scalable and Modular Architecture)
Categorizes CSS rules into 5 distinct buckets:
- **Base**: Plain tag resets and element defaults (`body`, `p`, `a`).
- **Layout**: Major page scaffolding blocks (`.l-header`, `.l-grid`, `.l-footer`).
- **Module**: Reusable, discrete component widgets (`.card`, `.modal`, `.dialog`).
- **State**: Transient conditions indicating interactivity (`.is-active`, `.is-collapsed`, `.is-disabled`).
- **Theme**: Visual skins and brand modes (`.theme-dark`, `.theme-holiday`).

### 3. BEM (Block, Element, Modifier)
A strict, self-documenting naming syntax:
- **Block (`.block`)**: A standalone component entity that has meaning on its own (e.g. `.card`, `.nav`, `.menu`).
- **Element (`.block__element`)**: A tied child part of a block that cannot be used outside it (e.g. `.card__title`, `.nav__item`).
- **Modifier (`.block--modifier` or `.block__element--modifier`)**: A flag on a block or element that changes appearance or behavior (e.g. `.card--featured`, `.btn--disabled`).

Crucially, BEM enforces **flat specificity**: every selector is a single class with specificity $(0, 1, 0)$.

### 4. ITCSS (Inverted Triangle CSS)
Organizes files into seven discrete layers of increasing specificity and narrowing reach:
1. **Settings**: Preprocessor variables, global design tokens.
2. **Tools**: Mixins, functions, calculation helpers.
3. **Generic**: Resets, box-sizing normalization (zero specificity).
4. **Elements**: Bare HTML element styling (`h1`, `table`, `ul`).
5. **Objects**: Non-cosmetic structural layout primitives (`.o-media`, `.o-grid`, `.o-container`).
6. **Components**: Designed UI widgets (`.c-card`, `.c-button`, `.c-avatar`).
7. **Utilities / Trumps**: High-specificity helper overrides (`.u-hidden`, `.u-text-center`).

---

## 3. How It Works Internally

```
The Inverted Triangle CSS (ITCSS) Specificity Hierarchy:

   ▼ Low Specificity / High Reach (Affects whole DOM)
┌──────────────────────────────────────────────┐  1. Settings (Design Tokens)
│                 SETTINGS                     │
├──────────────────────────────────────────────┤  2. Tools (Functions/Mixins)
│                  TOOLS                       │
├──────────────────────────────────────────────┤  3. Generic (Resets/Normalize)
│                 GENERIC                      │
├──────────────────────────────────────────────┤  4. Elements (Bare HTML Tags)
│                 ELEMENTS                     │
├──────────────────────────────────────────────┤  5. Objects (Layout Primitives)
│                 OBJECTS                      │
├──────────────────────────────────────────────┤  6. Components (BEM Widgets)
│                COMPONENTS                    │
├──────────────────────────────────────────────┤  7. Utilities (!important overrides)
│                UTILITIES                     │
└──────────────────────────────────────────────┘
   ▲ High Specificity / Narrow Reach (Single elements)
```

```
BEM Naming Grammar:
┌─────────────────┬────────────────────────────────┬───────────────────────────┐
│ Entity          │ Syntax Rule                    │ Example                   │
├─────────────────┼────────────────────────────────┼───────────────────────────┤
│ Block           │ `.block`                       │ `.search-form`            │
│ Element         │ `.block__element` (two under)  │ `.search-form__input`     │
│ Modifier        │ `.block--modifier` (two dash)  │ `.search-form--compact`   │
│ Element Modifier│ `.block__element--modifier`    │ `.search-form__btn--busy` │
└─────────────────┴────────────────────────────────┴───────────────────────────┘
```

---

## 4. Syntax & Usage

### BEM CSS Declaration Pattern
```css
/* Block */
.user-card {
  display: flex;
  padding: 1.5rem;
  background: white;
  border-radius: 0.5rem;
}

/* Block Modifier */
.user-card--vip {
  border: 2px solid #eab308;
  box-shadow: 0 10px 15px -3px rgba(234, 179, 8, 0.2);
}

/* Element */
.user-card__avatar {
  width: 3rem;
  height: 3rem;
  border-radius: 50%;
}

/* Element */
.user-card__name {
  font-size: 1.125rem;
  font-weight: 700;
  margin: 0;
}

/* Element Modifier */
.user-card__name--verified {
  color: #2563eb;
}
```

---

## 5. Practical Examples

### Example 1: Complete BEM Card Component
A media card implementing blocks, elements, modifiers, and interactive states.

```html
<article class="media-card media-card--featured">
  <img src="poster.jpg" alt="Film poster" class="media-card__image">
  <div class="media-card__body">
    <span class="media-card__badge media-card__badge--trending">Trending</span>
    <h2 class="media-card__title">The Quantum Nexus</h2>
    <p class="media-card__description">A deep exploration of modern web architectures.</p>
    <button class="media-card__action media-card__action--primary">Watch Now</button>
  </div>
</article>
```

```css
/* Block */
.media-card {
  display: flex;
  flex-direction: column;
  border: 1px solid #e2e8f0;
  border-radius: 0.75rem;
  overflow: hidden;
  background-color: #ffffff;
  transition: transform 0.2s ease;
}

/* Block Modifier */
.media-card--featured {
  border-color: #3b82f6;
  box-shadow: 0 20px 25px -5px rgba(59, 130, 246, 0.15);
}

/* Elements */
.media-card__image {
  width: 100%;
  height: 12rem;
  object-fit: cover;
}

.media-card__body {
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.media-card__badge {
  display: inline-block;
  align-self: flex-start;
  padding: 0.25rem 0.5rem;
  font-size: 0.75rem;
  font-weight: 700;
  border-radius: 0.25rem;
  background-color: #e2e8f0;
  color: #334155;
}

.media-card__badge--trending {
  background-color: #fef3c7;
  color: #b45309;
}

.media-card__title {
  margin: 0;
  font-size: 1.25rem;
  color: #0f172a;
}

.media-card__description {
  margin: 0;
  color: #64748b;
  font-size: 0.875rem;
  line-height: 1.5;
}

.media-card__action {
  margin-top: 1rem;
  padding: 0.5rem 1rem;
  font-weight: 600;
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;
}

.media-card__action--primary {
  background-color: #2563eb;
  color: #ffffff;

  &:hover {
    background-color: #1d4ed8;
  }
}
```

### Example 2: OOCSS Media Object (The Nicole Sullivan Classic)
Separating the structural media layout from individual content themes.

```html
<!-- Structural Object -->
<div class="media">
  <img src="avatar.jpg" alt="User" class="media__img">
  <div class="media__body">
    <h4>User Comments</h4>
    <p>OOCSS separates layout geometry from cosmetic skin.</p>
  </div>
</div>
```

```css
/* OOCSS Structure (Reusable anywhere!) */
.media {
  display: flex;
  align-items: flex-start;
  gap: 1rem;
}

.media__img {
  flex-shrink: 0;
}

.media__body {
  flex-grow: 1;
}

/* Skin: Cosmetic styling applied independently */
.surface-card {
  padding: 1rem;
  background: white;
  border-radius: 0.5rem;
  border: 1px solid #e2e8f0;
}
```

### Example 3: SMACSS State Classes (`.is-*`, `.has-*`)
Using standardized SMACSS state indicators with ARIA attributes.

```html
<nav class="navigation is-collapsed">
  <button class="nav-toggle" aria-expanded="false">Menu</button>
  <ul class="nav-menu">...</ul>
</nav>
```

```css
.navigation {
  transition: max-height 0.3s ease;
}

/* SMACSS State: Collapsed */
.navigation.is-collapsed .nav-menu {
  display: none;
}

/* Modern ARIA synergy */
.nav-toggle[aria-expanded="false"] + .nav-menu {
  display: none;
}
```

### Example 4: ITCSS File Structure in Modern Sass/CSS
How an enterprise ITCSS directory tree is organized with barrel files.

```
styles/
├── 1-settings/
│   ├── _colors.css
│   └── _typography.css
├── 2-tools/
│   └── _functions.css
├── 3-generic/
│   ├── _box-sizing.css
│   └── _normalize.css
├── 4-elements/
│   ├── _headings.css
│   └── _links.css
├── 5-objects/
│   ├── _o-container.css
│   └── _o-grid.css
├── 6-components/
│   ├── _c-button.css
│   └── _c-card.css
└── 7-utilities/
    └── _u-spacing.css
```

### Example 5: Modernizing BEM with Native Cascade Layers
Embedding BEM components directly into `@layer` blocks for bulletproof cascade management.

```css
@layer components {
  .modal-dialog {
    padding: 2rem;
    background: white;
  }

  .modal-dialog__header {
    border-bottom: 1px solid #e2e8f0;
  }

  .modal-dialog--fullscreen {
    width: 100vw;
    height: 100vh;
  }
}
```

---

## 6. Best Practices

1. **Avoid Grandchild BEM Selectors**: Never write `.block__element__grandchild`. If an element has internal parts, either create a new block (e.g. `.item-badge` inside `.card`) or keep it flat: `.card__author-avatar`.
2. **Never chain multiple classes in BEM rules**: Avoid `.card.card--featured .card__title`. Write `.card--featured .card__title` or use single classes. Chaining destroys flat specificity.
3. **Use prefixes to clarify ITCSS/SMACSS roles**:
   - `c-` for Components (`.c-card`, `.c-btn`)
   - `o-` for Structural Objects (`.o-container`, `.o-grid`)
   - `u-` for Utilities (`.u-text-center`, `.u-visually-hidden`)
   - `is-` / `has-` for States (`.is-open`, `.has-error`)
4. **Use BEM modifiers strictly for deviations, not base styles**: An element must retain its base class alongside the modifier in HTML: `<button class="c-btn c-btn--primary">`.
5. **Combine BEM with CSS Custom Properties**: Instead of writing 10 different modifier rules, use modifiers to adjust CSS variables:
   ```css
   .c-badge { background-color: var(--badge-bg, #e2e8f0); }
   .c-badge--success { --badge-bg: #10b981; }
   ```
6. **Remember native nesting differences**: Native CSS nesting does NOT support Sass string concatenation (`&__element`). Write full element class names.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Writing BEM grandchild syntax: `.card__header__title` | Creates rigid coupling and unreadable class names. | Flatten to `.card__title` or make `.header` its own block. |
| Attempting native CSS string concatenation: `&__title` | Syntax error in native CSS; rule is dropped. | Write explicit classes: `.card { .card__title { ... } }`. |
| Omitting the base class: `<div class="card--featured">` | Element misses foundational base styles. | Always pair with base class: `<div class="card card--featured">`. |
| Writing location-dependent selectors in BEM: `.sidebar .card` | Violates OOCSS and BEM; breaks component portability. | Use modifiers or layout wrappers: `.card--compact`. |
| Using BEM syntax for pure utility classes: `.u__color--red` | Inverts the purpose of utilities. | Keep utilities simple and prefixed: `.u-text-red`. |
| Modifying state via tag names instead of state classes | Fragile and couples state to HTML markup. | Use standardized state classes: `.is-active`, `.is-disabled`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Optimal Browser Matching**: Browsers evaluate CSS right-to-left. A single class selector like `.c-card__title` evaluates in $O(1)$ time, whereas deep descendant selectors (`.sidebar div > ul > li a`) force the engine to walk ancestor chains.
- **Cache Hit Rates**: Flat BEM architectures produce stable, long-lived stylesheets with high browser cache hit rates.

### Accessibility (A11y)
- **Pairing SMACSS States with ARIA**: When toggling states, synchronize visual classes with ARIA attributes:
  ```css
  /* State class and semantic ARIA attribute work in harmony */
  .accordion-trigger.is-expanded,
  .accordion-trigger[aria-expanded="true"] {
    background-color: #eff6ff;
  }
  ```

### Browser Compatibility
- Universal. BEM, OOCSS, and SMACSS rely strictly on class selectors, compatible with 100% of browsers.

---

## 9. Comparison / When to Use What

| Methodology | Core Strength | Naming Style | Specificity Strategy |
| :--- | :--- | :--- | :--- |
| **BEM** | Predictable naming, component isolation | `block__element--mod` | Strict flat $(0, 1, 0)$ |
| **OOCSS** | Reusable structural patterns, small CSS size | `.media`, `.btn` | Decoupled structure/skin |
| **SMACSS** | Clear architectural categorization | `l-`, `is-`, `theme-` | Layered categorization |
| **ITCSS** | File ordering and cascade management | Folder-based | Inverted triangle specificity |

---

## 10. Interview Questions

### Q1: What is the primary problem that BEM solves in large codebases?
<details>
<summary>View Answer</summary>
BEM solves class naming collisions and specificity wars. By using explicit block namespaces (`.card__title`) and single-class selectors, BEM guarantees that every selector has identical $(0, 1, 0)$ specificity. This makes components completely self-documenting, portable, and immune to accidental cascading side-effects from other stylesheets.
</details>

### Q2: What is the "Grandchild Trap" in BEM, and how should it be avoided?
<details>
<summary>View Answer</summary>
The Grandchild Trap refers to nesting element names like `.card__header__nav__item`. This exposes the internal DOM structure of the card, creating brittle class names that break whenever HTML is refactored. The correction is to flatten the element hierarchy (`.card__item`) or promote intermediate containers into independent blocks (e.g. creating a standalone `.card-nav` block).
</details>

### Q3: What are the two core tenets of OOCSS defined by Nicole Sullivan?
<details>
<summary>View Answer</summary>
1. **Separate Structure from Skin**: Abstract repetitive structural layout geometry (like padding, flexbox alignments, or positioning) into base objects, while managing visual cosmetics (colors, box-shadows, borders) with separate skin classes.
2. **Separate Container from Content**: Ensure components are completely independent of their parent containers; a `.heading` should look identical whether it is placed in the main article, sidebar, or footer.
</details>

### Q4: Why does BEM syntax require applying both the base class and the modifier class in HTML (`class="btn btn--primary"`)?
<details>
<summary>View Answer</summary>
The base class (`.btn`) contains the core structural properties (display, padding, border-radius, cursor, transitions). The modifier class (`.btn--primary`) contains only the differential variations (background-color, text color). This preserves DRY principles and ensures modifiers only override specific traits.
</details>

### Q5: How does the ITCSS specificity graph prevent cascade bugs?
<details>
<summary>View Answer</summary>
ITCSS organizes stylesheets so that specificity and specificity reach progress in a monotonically increasing direction: broad, zero-specificity rules (resets and element tags) are loaded first at the top of the file, followed by layout objects, components, and finally single-purpose utilities at the bottom. Because styles naturally flow from low to high specificity, developers never need to fight the cascade with `!important`.
</details>

### Q6: Can BEM and SMACSS be used together in the same project?
<details>
<summary>View Answer</summary>
Yes. In fact, combining them is an industry standard. SMACSS is used to categorize directory and file architecture (`Base`, `Layout`, `Module`, `State`), while BEM is used as the strict naming convention for the `Module` component layer.
</details>

### Q7: Why does Sass-style string concatenation (`&__element`) fail in native CSS nesting?
<details>
<summary>View Answer</summary>
In Sass, the compiler treats `&` as a text token macro and performs string concatenation during pre-processing. In native browser CSS, `&` is a selector object representing `:is(parent)`. Because the browser parses stylesheets directly into the CSSOM, the parser cannot concatenate strings to invent new class names at runtime.
</details>

### Q8: How do Cascade Layers (`@layer`) modernize ITCSS?
<details>
<summary>View Answer</summary>
ITCSS relied entirely on source order in stylesheets to enforce the specificity triangle. If an asset bundler accidentally reordered files, the hierarchy broke. Native `@layer` formalizes ITCSS layers directly in the browser engine (`@layer reset, base, objects, components, utilities;`), guaranteeing architectural priority regardless of file import sequence.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactor Nested CSS into Clean BEM
**Objective**: Convert a deeply nested navigation menu into strict BEM syntax.
<details>
<summary>View Solution</summary>

```css
/* Legacy: Deep Nesting */
nav.site-menu ul li a.active {
  color: #2563eb;
}

/* Refactored BEM */
.site-nav { ... }
.site-nav__list { list-style: none; padding: 0; }
.site-nav__item { display: inline-block; }
.site-nav__link { color: #64748b; text-decoration: none; }
.site-nav__link--active { color: #2563eb; font-weight: 700; }
```
</details>

### Exercise 2: OOCSS Media Object Component
**Objective**: Build a classic OOCSS media object with an avatar on the left and body content on the right, keeping skin separate.
<details>
<summary>View Solution</summary>

```css
/* Structure */
.o-media {
  display: flex;
  align-items: flex-start;
  gap: 1rem;
}

.o-media__figure {
  flex-shrink: 0;
}

.o-media__body {
  flex: 1;
}

/* Skin */
.s-box-white {
  background: white;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  padding: 1rem;
}
```
</details>

### Exercise 3: SMACSS State Indicator Toggle
**Objective**: Create a collapsible drawer that transitions between collapsed and expanded states using SMACSS `.is-*` classes.
<details>
<summary>View Solution</summary>

```css
.c-drawer {
  max-height: 20rem;
  overflow: hidden;
  transition: max-height 0.3s ease;
}

.c-drawer.is-collapsed {
  max-height: 0;
}
```
</details>

### Exercise 4: Flatten BEM Specificity
**Objective**: Refactor a compound rule `.c-card.c-card--promoted .c-card__title` to flat $(0, 1, 0)$ specificity.
<details>
<summary>View Solution</summary>

```css
/* High specificity: (0, 3, 0) */
.c-card.c-card--promoted .c-card__title { color: gold; }

/* Flat specificity: (0, 2, 0) */
.c-card--promoted .c-card__title { color: #eab308; }
```
</details>

### Exercise 5: ITCSS Layer Definition in CSS
**Objective**: Formalize the 7 ITCSS layers using modern CSS `@layer`.
<details>
<summary>View Solution</summary>

```css
@layer settings, tools, generic, elements, objects, components, utilities;

@layer elements {
  h1, h2, h3 { font-family: 'Inter', sans-serif; }
}

@layer objects {
  .o-container { max-width: 75rem; margin-inline: auto; padding-inline: 1.5rem; }
}

@layer components {
  .c-card { padding: 1.5rem; background: white; }
}

@layer utilities {
  .u-text-center { text-align: center !important; }
}
```
</details>

### Exercise 6: Accessible BEM Form Group
**Objective**: Style an input group with error modifiers and error text indicators using BEM naming.
<details>
<summary>View Solution</summary>

```css
.form-group {
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
}

.form-group__label {
  font-size: 0.875rem;
  font-weight: 600;
  color: #334155;
}

.form-group__input {
  padding: 0.5rem 0.75rem;
  border: 1px solid #cbd5e1;
  border-radius: 0.375rem;
}

.form-group__input--error {
  border-color: #ef4444;
  background-color: #fef2f2;
}

.form-group__message {
  font-size: 0.75rem;
  color: #dc2626;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* BEM Syntax Structure */
.block { ... }
.block__element { ... }
.block--modifier { ... }
.block__element--modifier { ... }

/* SMACSS Prefixes */
.l-header { ... }   /* Layout */
.is-active { ... }  /* State */
.theme-dark { ... } /* Theme */

/* ITCSS Inverted Specificity Layers */
@layer settings, tools, generic, elements, objects, components, utilities;
```

- **BEM**: Flattens specificity, eliminates selector side-effects, self-documenting.
- **OOCSS**: Separates structure from skin, container from content.
- **SMACSS**: Categorizes rules into Base, Layout, Module, State, Theme.
- **ITCSS**: Progressively increases specificity and narrows reach.

---

## 13. Further Reading

- [BEM Official Methodology Guidelines](https://en.bem.info/methodology/quick-start/)
- [Nicole Sullivan: Object Oriented CSS](http://oocss.org/)
- [Jonathan Snook: Scalable and Modular Architecture for CSS (SMACSS)](https://smacss.com/)
- [Harry Roberts: Managing CSS Projects with ITCSS](https://www.xfive.co/blog/itcss-scalable-maintainable-css-architecture/)

---

### Navigation
- **Previous**: [01-css-architecture-principles.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/01-css-architecture-principles.md)
- **Next**: [03-utility-first-and-tailwind-overview.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/03-utility-first-and-tailwind-overview.md)
