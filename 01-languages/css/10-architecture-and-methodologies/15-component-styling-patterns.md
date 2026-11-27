# 15. Component Styling Patterns

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Modern Component Architecture / W3C Web Platform Baseline |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ |
| **Tested With** | Modern DevTools, React 19, Web Components, Vite 5 |
| **Prerequisites** | `04-grid/`, `05-responsive-design/03-container-queries.md`, `10-architecture-and-methodologies/01-css-architecture-principles.md` |

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
Legacy component styling treated components as fixed visual artifacts tightly coupled to global page viewports. Components styled with viewport media queries (`@media (min-width: 768px)`) break immediately when placed inside narrow layout sidebars or multi-column grids. Furthermore, components that enforce rigid styling APIs force developers into dangerous specificity overrides (`!important` or deep descendant selectors) whenever subtle visual variations or contextual modifications are required.

### The Solution
Modern component styling decouples a component's internal layout from the global page context:
- **Container-Query Powered Components**: Components self-adapt based on their parent container's allocated dimensions, making them truly plug-and-play across any page context.
- **Custom Property Style APIs**: Exposing a clean public surface of CSS custom properties (`--card-bg`, `--card-padding`) allows parent contexts to re-skin components without breaking encapsulation.
- **Composition over Inheritance**: Assembling complex components from small, modular primitives (Card Header, Card Media, Card Body) rather than creating dozens of monolithic, rigid variants.
- **State-Driven Styling via ARIA & Data Attributes**: Styling components directly against their accessibility state (`[aria-expanded="true"]`, `[aria-disabled="true"]`) ensures visual states and assistive-technology states can never get out of sync.

---

## 2. Core Concepts

### 1. The Anatomy of a Resilient Modern Component

```
┌─────────────────────────────────────────────────────────────┐
│                   RESILIENT COMPONENT API                   │
└─────────────────────────────────────────────────────────────┘
  1. Internal Encapsulation:
     - Defines local CSS Custom Property fallbacks
     - Uses `:where()` or `@layer` for flat baseline specificity
     - Self-contained Container Context: `container-type: inline-size`

  2. Public Customisation Surface:
     - Consumes configurable variables: `var(--ui-card-padding, 1.5rem)`
     - Overridable via parent selectors or HTML inline styles

  3. Slot & Compositional Architecture:
     - Independent compound subcomponents:
       `<UiCard.Header>`, `<UiCard.Body>`, `<UiCard.Footer>`

  4. Accessibility-Coupled States:
     - `&[aria-selected="true"]` instead of `.is-selected`
     - `&[data-loading="true"]` instead of `.loading`
```

### 2. Responsive Components vs. Responsive Pages
- **Responsive Page (Legacy)**: Uses `@media` queries to reconfigure the overall page layout at global viewport breakpoints (320px, 768px, 1024px). Individual components do not know where they are placed.
- **Responsive Component (Modern)**: Declares `container-type: inline-size`. Uses `@container` queries to adjust its own layout whether placed in a full-width hero or a 250px right-hand sidebar.

### 3. Open Styling vs. Shadow DOM Encapsulation
- **CSS Modules / Scoped CSS**: Provides build-time class-name hashing (`.button_a8f9`). Styles remain in the standard document cascade, allowing easy custom property penetration.
- **Shadow DOM (Web Components)**: Provides runtime browser-level style encapsulation. Global CSS cannot pierce the shadow root, except via **CSS Custom Properties**, `::part()`, and `::slotted()`.

---

## 3. How It Works Internally

### Custom Property Penetration Across Encapsulation Boundaries
CSS Custom Properties naturally inherit through the DOM tree, including across Shadow DOM boundaries:

```
DOCUMENT ROOT (:root)
  --color-primary: #0284c7;
       │
       ▼ (Inherits down DOM)
  <custom-card> (Host Element)
       │
       ├─► SHADOW ROOT BOUNDARY (Blocks normal CSS rules)
       │     │
       │     ▼
       │   .internal-card-title {
       │     color: var(--color-primary); /* SUCCEEDS: Variables penetrate! */
       │   }
       │
       └─► LIGHT DOM SLOTTED CONTENT
             <p slot="description">...</p>
             (Styled via ::slotted() or document CSS)
```

---

## 4. Syntax & Usage

### 1. Resilient Component API Pattern (Pure CSS)

```css
@layer components {
  .c-card {
    /* 1. Private / Configurable API Tokens */
    --_card-bg: var(--card-bg, #ffffff);
    --_card-radius: var(--card-radius, 0.75rem);
    --_card-padding: var(--card-padding, 1.5rem);
    --_card-border: var(--card-border, 1px solid #e5e7eb);

    /* 2. Container Context */
    container-type: inline-size;
    container-name: card;

    /* 3. Base Properties */
    background-color: var(--_card-bg);
    border-radius: var(--_card-radius);
    padding: var(--_card-padding);
    border: var(--_card-border);
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  /* 4. Self-Adapting Container Queries */
  @container card (min-width: 450px) {
    .c-card__content {
      display: grid;
      grid-template-columns: 120px 1fr;
      align-items: center;
    }
  }

  /* 5. ARIA State Coupling */
  .c-card[aria-busy="true"] {
    opacity: 0.7;
    pointer-events: none;
  }
}
```

### 2. Shadow DOM Export via `::part`
Allowing selective external styling of encapsulated Web Components:

```html
<!-- Inside Web Component Template -->
<div class="modal-backdrop">
  <div class="modal-dialog" part="dialog">
    <h2 part="title">Dialog Header</h2>
    <slot></slot>
  </div>
</div>
```

```css
/* External Application Stylesheet targeting the exported part */
custom-modal::part(dialog) {
  border-radius: 1.5rem;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
}

custom-modal::part(title) {
  font-family: "Inter Display", sans-serif;
  color: #1e3a8a;
}
```

---

## 5. Practical Examples

### Example 1: Autonomous Self-Adapting User Profile Card
This component renders as a vertical stacked card when rendered in narrow sidebars (<360px), and automatically reflows into a horizontal media object when rendered in wide main content areas, with zero global media queries.

```html
<article class="profile-card" aria-label="Team Member Profile">
  <img src="/avatars/sarah.webp" alt="Sarah Connor" class="profile-card__avatar">
  <div class="profile-card__info">
    <h3 class="profile-card__name">Sarah Connor</h3>
    <p class="profile-card__role">Principal Systems Architect</p>
    <div class="profile-card__actions">
      <button type="button" class="profile-card__btn">Connect</button>
    </div>
  </div>
</article>
```

```css
@layer components {
  .profile-card {
    --avatar-size: 4rem;

    container-type: inline-size;
    background: #ffffff;
    border-radius: 1rem;
    padding: 1.25rem;
    border: 1px solid #e2e8f0;
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    gap: 1rem;
  }

  .profile-card__avatar {
    inline-size: var(--avatar-size);
    block-size: var(--avatar-size);
    border-radius: 9999px;
    object-fit: cover;
  }

  .profile-card__name {
    font-size: 1.125rem;
    font-weight: 700;
    color: #0f172a;
    margin: 0;
  }

  .profile-card__role {
    font-size: 0.875rem;
    color: #64748b;
    margin: 0.25rem 0 0.75rem 0;
  }

  .profile-card__btn {
    padding: 0.5rem 1rem;
    background: #0284c7;
    color: #ffffff;
    border: none;
    border-radius: 0.5rem;
    font-weight: 600;
    cursor: pointer;
  }

  .profile-card__btn:focus-visible {
    outline: 2px solid #0284c7;
    outline-offset: 2px;
  }

  /* Autonomous container query transformation */
  @container (min-width: 400px) {
    .profile-card {
      flex-direction: row;
      text-align: start;
      align-items: center;
    }

    .profile-card__actions {
      margin-inline-start: auto;
    }
  }
}
```

### Example 2: Polymorphic Accordion Driven by Native `<details>` & Data Attributes
Eliminating JavaScript state toggling in favor of native HTML disclosure elements and semantic styling:

```html
<details class="c-accordion">
  <summary class="c-accordion__header">
    <span class="c-accordion__title">What is zero-runtime CSS?</span>
    <span class="c-accordion__icon" aria-hidden="true">+</span>
  </summary>
  <div class="c-accordion__content">
    <p>Zero-runtime CSS extracts and generates pure CSS files at build time without evaluating styling functions in the client browser thread.</p>
  </div>
</details>
```

```css
@layer components {
  .c-accordion {
    border-bottom: 1px solid #e5e7eb;
    padding-block: 1rem;
  }

  .c-accordion__header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    cursor: pointer;
    list-style: none; /* Strip default marker */
    font-weight: 600;
    color: #111827;
  }

  /* Strip marker in WebKit */
  .c-accordion__header::-webkit-details-marker {
    display: none;
  }

  .c-accordion__header:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 4px;
    border-radius: 4px;
  }

  .c-accordion__icon {
    font-size: 1.25rem;
    transition: transform 0.2s ease;
  }

  /* Native open state selector */
  .c-accordion[open] .c-accordion__icon {
    transform: rotate(45deg);
  }

  .c-accordion__content {
    padding-block-start: 0.75rem;
    color: #4b5563;
    line-height: 1.6;
  }
}
```

### Example 3: Slot-Based Compositional Card API
Demonstrating how compound components avoid the "prop explosion" anti-pattern:

```html
<!-- Composed Card Structure -->
<article class="ui-card">
  <header class="ui-card__header">
    <span class="ui-card__badge">New Feature</span>
    <button type="button" class="ui-card__action-btn" aria-label="Bookmark">★</button>
  </header>
  <div class="ui-card__body">
    <h3>Container Query Units</h3>
    <p>Using cqw and cqh for fluid typographic scaling relative to parents.</p>
  </div>
  <footer class="ui-card__footer">
    <time datetime="2026-11-26">November 2026</time>
    <a href="/notes" class="ui-card__link">Read more &rarr;</a>
  </footer>
</article>
```

```css
@layer components {
  .ui-card {
    display: flex;
    flex-direction: column;
    background: #ffffff;
    border: 1px solid #e5e7eb;
    border-radius: 0.75rem;
    overflow: clip;
  }

  .ui-card__header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 1rem 1.25rem 0.5rem;
  }

  .ui-card__body {
    padding: 0.5rem 1.25rem;
    flex: 1 1 auto;
  }

  .ui-card__footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.75rem 1.25rem 1rem;
    border-top: 1px solid #f3f4f6;
    font-size: 0.875rem;
    color: #6b7280;
  }
}
```

### Example 4: Contextual Theming via CSS Custom Property Injection
Parent contexts modify child components cleanly by injecting property overrides:

```html
<section class="promotional-banner">
  <!-- The child card inherits custom properties injected by .promotional-banner -->
  <article class="c-card">
    <h3>Special Offer</h3>
    <p>Deploy globally in seconds.</p>
  </article>
</section>
```

```css
/* Promotional Banner context overrides child card variables */
.promotional-banner {
  --card-bg: #0f172a;
  --card-border: 1px solid #334155;
  --card-radius: 1.5rem;
  color: #f8fafc;
  padding: 3rem;
  background: radial-gradient(circle at top right, #1e293b, #0f172a);
}
```

---

## 6. Best Practices

1. **Adopt Container Queries over Media Queries for Components**: Components should never query the viewport width (`@media`); use `@container` so components respond accurately inside any parent layout column.
2. **Expose Custom Properties for Stylistic Variations**: Do not create dozens of modifier classes (`.card--blue`, `.card--rounded`, `.card--compact`). Expose CSS variables (`--card-bg`, `--card-radius`, `--card-padding`) as the component's public styling contract.
3. **Bind Styles to ARIA and Semantic HTML States**: Style `[aria-expanded="true"]`, `[aria-disabled="true"]`, and `:checked` instead of maintaining duplicate custom state classes like `.is-expanded` or `.disabled`.
4. **Use Compound Subcomponents Instead of Monolithic Configs**: Break complex components into composable primitives (Header, Body, Actions, Footer). This eliminates brittle prop drilling and nested conditionals.
5. **Set `:where()` on Component Roots**: Wrap component root selectors in `:where(.component)` to give them zero specificity, ensuring application layouts can override positioning easily.
6. **Encapsulate Layout externally; Encapsulate Structure internally**: A component should manage its internal padding and flex alignment, but should NEVER define its own external `margin` or global positioning; parent layouts must manage component placement.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Setting `margin` on component root elements** | Breaks layout reusability; component cannot be placed inside flex/grid parents cleanly. | Remove external margins; let parent layout grids/gap control spacing. |
| **Using `@media` queries inside reusable components** | Component breaks or looks distorted when rendered inside narrow sidebars or split panes. | Use `@container` queries anchored to parent container contexts. |
| **Duplicating accessibility states with JS classes** | Inevitable bugs where `.active` is added but `aria-selected="true"` is omitted. | Style directly against the ARIA attribute: `[aria-selected="true"]`. |
| **Deep descendant styling (`.card .btn span`)** | Breaks internal component encapsulation and causes high specificity collisions. | Use direct, flat classes: `.card__btn-text` or modern BEM. |
| **Hardcoding layout dimensions in pixels** | Components overflow or clip text when users zoom or change system font sizes. | Use relative units (`rem`, `ch`, `%`) and `min()`, `max()`, `clamp()`. |
| **Relying on Shadow DOM for everything** | Extreme difficulty sharing global typography, theme tokens, and CSS resets. | Use CSS Modules or Cascade Layers for standard UI; reserve Shadow DOM for true leaf-node widgets. |

---

## 8. Performance, Accessibility & Browser Support

### Performance Benchmark
- **Container Queries Performance**: Modern browser engines (Blink, Gecko, WebKit) optimize container queries via localized layout containment. Style recalculations are constrained strictly to the container subtree, avoiding expensive full-page reflow passes.
- **Selector Performance**: Attribute selectors like `[aria-selected="true"]` match with identical $(0, 1, 0)$ performance to standard class selectors.

### Accessibility Integration
- Always provide visible `:focus-visible` styling on interactive component primitives (`<button>`, `<summary>`, `<a>`).
- Never set `display: none` on components that must remain discoverable to assistive technologies; use the standard `.sr-only` visually-hidden pattern.

---

## 9. Comparison / When to Use What

| Pattern | Encapsulation Mechanism | Customisation API | Best Suited For |
| :--- | :--- | :--- | :--- |
| **CSS Modules / BEM** | Build-time class hashing / Conventions | Custom properties & modifier classes | Standard SPA web applications (React, Next.js, Vue) |
| **Web Components (Shadow DOM)** | Browser-native boundary | CSS Variables & `::part()` | Cross-framework enterprise design systems |
| **Compound Subcomponents** | Architectural composition | Modular HTML composition | Design systems, complex widgets (Cards, Tables, Modals) |
| **Container-Query Primitives** | Native CSS `@container` | Inline-size thresholds | Truly responsive UI widgets (Cards, Navbars, Media players) |

---

## 10. Interview Questions

### Q1: Why should reusable UI components avoid defining their own external `margin`?
<details>
<summary>Answer</summary>
External margins dictate how an element relates to its surrounding siblings and parent layout context. If a component defines `margin: 1.5rem 0`, it assumes it will always sit in a vertical stack with standard spacing.

When that component is later placed inside a Flexbox container with `gap: 1rem`, an absolute positioning context, or a CSS Grid, that hardcoded margin conflicts with the parent's layout rules, causing alignment bugs and forcing developers to write override hacks like `.parent-layout .component { margin: 0; }`.
The gold standard rule of component architecture is: **Components control their internal padding; parent layouts control external spacing via grid, flexbox, or layout wrappers.**
</details>

### Q2: How does styling directly against ARIA attributes improve both code quality and accessibility?
<details>
<summary>Answer</summary>
When developers style states using arbitrary custom classes (e.g. `.is-active`, `.open`, `.disabled`), the visual state is completely decoupled from the DOM accessibility tree. It is extremely common for developers to update the CSS class while forgetting to update the corresponding ARIA attribute (`aria-expanded="true"`, `aria-selected="true"`).

By styling directly against the accessibility attribute:
```css
.tab-button[aria-selected="true"] {
  border-bottom: 2px solid #2563eb;
  font-weight: 700;
}
```
the visual design cannot work unless the correct accessibility attribute is physically present in the DOM. This creates an automated incentive for developers to implement proper accessibility semantics.
</details>

### Q3: Explain the difference between `::part()` and `::slotted()` in Web Component styling.
<details>
<summary>Answer</summary>
- **`::part()`**: Allows external CSS to style an element located **inside the component's Shadow DOM**, provided that internal element was explicitly exported with the `part` attribute (e.g. `<button part="submit-button">`). External styles target it as `custom-widget::part(submit-button) { background: red; }`.
- **`::slotted()`**: Used **inside the Web Component's Shadow DOM stylesheet** to style elements that were passed into a `<slot>` from the external Light DOM. Crucially, `::slotted()` can only style top-level slotted elements (it cannot match nested descendants of slotted nodes).
</details>

### Q4: What is the "Prop Explosion" anti-pattern in component design, and how does composition solve it?
<details>
<summary>Answer</summary>
Prop Explosion occurs when a monolithic component attempts to handle every possible visual permutation via configuration props:
```jsx
<Card
  hasHeader={true}
  headerTitle="Welcome"
  headerAction="Share"
  isMediaLeft={true}
  mediaUrl="/pic.jpg"
  hasFooter={true}
  footerVariant="extended"
/>
```
This produces bloated internal templates filled with complex ternary operators and fragile CSS cascades.

**Composition** solves this by breaking the component into small, single-responsibility subcomponents:
```jsx
<Card>
  <Card.Header>
    <Card.Title>Welcome</Card.Title>
    <Button>Share</Button>
  </Card.Header>
  <Card.Media src="/pic.jpg" />
  <Card.Footer />
</Card>
```
Each subcomponent manages its own styling independently, providing infinite flexibility without increasing component complexity.
</details>

### Q5: How do CSS Custom Properties allow a design system to offer an "Open-Closed" styling API?
<details>
<summary>Answer</summary>
In object-oriented programming, the Open-Closed Principle states that software entities should be open for extension but closed for modification.

In CSS components, we can achieve this by implementing private fallback variables:
```css
.btn {
  background-color: var(--btn-bg, #2563eb);
  color: var(--btn-color, #ffffff);
  border-radius: var(--btn-radius, 0.5rem);
}
```
The internal rules of `.btn` are "closed" (authors do not need to rewrite or override the selector). However, it is "open for extension" because any consuming parent or context can inject `--btn-bg: #dc2626;` to safely alter the button's appearance without breaking specificity or encapsulation.
</details>

### Q6: Why are Container Queries fundamentally superior to Viewport Media Queries for modular component libraries?
<details>
<summary>Answer</summary>
Viewport media queries evaluate the global browser window width (`1920px`, `768px`, etc.). A component styled with media queries only knows how wide the *entire monitor* is; it has zero knowledge of how much space it has been allocated on the screen.

If a developer places a media-query-responsive card into a narrow 300px sidebar on a 1920px desktop monitor, the card mistakenly triggers its "desktop" layout (e.g. wide horizontal banner), breaking out of the container and overflowing the sidebar.
Container queries (`@container`) measure the component's immediate parent container width, allowing the card to render in its compact mobile layout whenever placed in a narrow column, regardless of the screen size.
</details>

### Q7: What is the purpose of declaring `container-type: inline-size` instead of `container-type: size`?
<details>
<summary>Answer</summary>
- **`container-type: inline-size`**: Establishes a container context based only on the container's inline (horizontal) dimension. The container's vertical block height is allowed to grow naturally based on its children's content. This is used in 98% of UI designs.
- **`container-type: size`**: Establishes containment on both horizontal (inline) and vertical (block) dimensions. This requires the container to have an explicitly fixed or constrained height; otherwise, child elements query the container height, which depends on child height, causing circular layout dependency loops and collapsing the element's height to zero.
</details>

### Q8: How can `:where()` be used on component roots to create completely friction-free component libraries?
<details>
<summary>Answer</summary>
By wrapping the component root selector in `:where()`:
```css
:where(.design-system-card) {
  background: white;
  border-radius: 8px;
  padding: 16px;
}
```
The selector evaluates to $(0, 0, 0)$ specificity. When consumers import this design system, they can override any property using a standard single class selector (`.user-profile { padding: 24px; }`), completely eliminating the need for `!important` or multi-class specificity hacks.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Container-Query Media Card with CSS Variable Theming
**Objective**: Build a responsive media card that adapts its layout from vertical stack to horizontal split at 420px container width, exposing `--card-accent` and `--card-padding` custom properties.

<details>
<summary>Solution</summary>

```html
<article class="media-card">
  <div class="media-card__image-wrap">
    <img src="/photos/tech.webp" alt="Cloud Data Center" class="media-card__img">
  </div>
  <div class="media-card__body">
    <span class="media-card__category">Infrastructure</span>
    <h3 class="media-card__title">Edge Computing Patterns</h3>
    <p class="media-card__snippet">Explore latency reduction strategies using edge compute nodes.</p>
  </div>
</article>
```

```css
@layer components {
  .media-card {
    --card-accent: #0284c7;
    --card-padding: 1.25rem;

    container-type: inline-size;
    display: flex;
    flex-direction: column;
    background: #ffffff;
    border-radius: 0.75rem;
    overflow: clip;
    border: 1px solid #e2e8f0;
  }

  .media-card__image-wrap {
    inline-size: 100%;
    aspect-ratio: 16 / 9;
  }

  .media-card__img {
    inline-size: 100%;
    block-size: 100%;
    object-fit: cover;
  }

  .media-card__body {
    padding: var(--card-padding);
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .media-card__category {
    font-size: 0.75rem;
    font-weight: 700;
    text-transform: uppercase;
    color: var(--card-accent);
    letter-spacing: 0.05em;
  }

  .media-card__title {
    font-size: 1.25rem;
    font-weight: 700;
    margin: 0;
    color: #0f172a;
  }

  .media-card__snippet {
    font-size: 0.875rem;
    color: #64748b;
    line-height: 1.5;
    margin: 0;
  }

  /* Container query adaptation */
  @container (min-width: 420px) {
    .media-card {
      flex-direction: row;
      align-items: center;
    }

    .media-card__image-wrap {
      inline-size: 180px;
      aspect-ratio: 1 / 1;
      flex-shrink: 0;
    }
  }
}
```
</details>

### Exercise 2: Accessible Tab Component Styled via ARIA Attributes
**Objective**: Style a compound tabs component using zero custom modifier classes, driving all active, focus, and disabled states strictly via ARIA attributes.

<details>
<summary>Solution</summary>

```html
<div class="tabs-widget" role="tablist" aria-label="Project Views">
  <button type="button" role="tab" aria-selected="true" id="tab-1" aria-controls="panel-1">Overview</button>
  <button type="button" role="tab" aria-selected="false" id="tab-2" aria-controls="panel-2">Activity</button>
  <button type="button" role="tab" aria-selected="false" aria-disabled="true" id="tab-3" aria-controls="panel-3">Settings</button>
</div>
```

```css
@layer components {
  .tabs-widget {
    display: flex;
    gap: 0.5rem;
    border-bottom: 2px solid #e5e7eb;
  }

  .tabs-widget button[role="tab"] {
    padding: 0.75rem 1.25rem;
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
    margin-bottom: -2px;
    font-size: 0.95rem;
    font-weight: 500;
    color: #6b7280;
    cursor: pointer;
    transition: color 0.15s ease, border-color 0.15s ease;
  }

  .tabs-widget button[role="tab"]:hover:not([aria-disabled="true"]) {
    color: #111827;
  }

  .tabs-widget button[role="tab"]:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 4px;
    border-radius: 4px;
  }

  /* Active state coupled directly to aria-selected */
  .tabs-widget button[role="tab"][aria-selected="true"] {
    color: #2563eb;
    border-bottom-color: #2563eb;
    font-weight: 700;
  }

  /* Disabled state coupled directly to aria-disabled */
  .tabs-widget button[role="tab"][aria-disabled="true"] {
    color: #d1d5db;
    cursor: not-allowed;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **No External Margins**: Components manage internal padding only; parent layouts dictate external spacing.
- **Responsive Primitives**: Always favor `@container` queries over `@media` queries for modular UI widgets.
- **Container Declaration**: `container-type: inline-size` on container parents.
- **Tokenized API Surface**: Expose `--component-*` custom properties with fallback values for painless consumer theming.
- **Accessibility Coupling**: Style `[aria-selected="true"]`, `[aria-expanded="true"]`, and `[aria-busy="true"]` instead of duplicate JavaScript classes.
- **Zero-Specificity Baselines**: Use `:where(.component)` for library defaults so consuming applications can override styles without specificity inflation.
- **Composition over Monoliths**: Split cards, modals, and lists into composable subcomponents (`Header`, `Body`, `Actions`).

---

## 13. Further Reading
- [MDN Web Docs: CSS Container Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_containment/Container_queries)
- [W3C CSS Containment Module Level 3](https://www.w3.org/TR/css-contain-3/)
- [W3C Web Components Specifications](https://www.w3.org/standards/techs/components)
- [WAI-ARIA Authoring Practices Guide (APG)](https://www.w3.org/WAI/ARIA/apg/)
- [Ahmad Shadeed: Responsive Components with Container Queries](https://ishadeed.com/article/css-container-queries-guide/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [14. Dead CSS and Critical CSS](14-dead-css-and-critical-css.md) | [Module 11: Accessibility & UX in CSS](../11-accessibility-and-ux-in-css/README.md) |
