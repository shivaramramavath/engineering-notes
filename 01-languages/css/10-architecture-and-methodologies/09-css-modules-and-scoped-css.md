# CSS Modules and Scoped CSS

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Build-Time Modular CSS Architectures / Scoping Tooling Standards |
| **Baseline Status** | Universal Tooling Standards (Vite, Next.js, Webpack, Nuxt, SvelteKit) |
| **Target Audience** | Frontend Engineers, React/Vue/Svelte Developers, UI Architects |
| **Tested With** | Vite 5+, Next.js 14+, Vue SFC Scoped, Native CSS `@scope` |

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

In large-scale Single Page Applications (SPAs) and component-based architectures (React, Vue, Svelte, Angular), one of the greatest operational hazards is **class name collision in the global namespace**. If a developer builds a `.title` class in a Card component, and another developer builds a `.title` class in a Modal component, their styles collide unpredictably based on bundler load order.

To solve this, developers created **CSS Modules** (Mark Dalgleish, Glen Maddern, Tobias Koppers, 2015):
- Instead of relying on manual, disciplined BEM class prefixes, CSS Modules **automates encapsulation at build time**.
- The compiler rewrites local class names into unique, randomized hashes (e.g. `.title` becomes `.Card_title__x892a`).
- The JavaScript component imports a JavaScript object mapping the human-readable class name to the hashed class name.

Simultaneously, frameworks like Vue and Svelte introduced **Single File Component (SFC) Scoped Styles**, using custom HTML data attributes (`data-v-xxxx`) to isolate styles to their templates.

Today, while modern browsers are adopting native **`@scope`**, CSS Modules and Scoped CSS remain the dominant encapsulation standard in enterprise web development.

---

## 2. Core Concepts

### How CSS Modules Works
A CSS Module is a standard CSS file with a `.module.css` extension:
```css
/* Button.module.css */
.btn {
  background-color: #2563eb;
  color: white;
}
```
When imported into JavaScript/TypeScript:
```jsx
// Button.tsx
import styles from './Button.module.css';

export function Button() {
  // Renders: <button class="Button_btn__1a2b3">
  return <button className={styles.btn}>Click</button>;
}
```

### The `composes` Keyword
CSS Modules supports composing styles from other classes or even other module files:
```css
/* Card.module.css */
.baseCard {
  padding: 1.5rem;
  border-radius: 0.5rem;
}

.featuredCard {
  composes: baseCard; /* Automatically adds both classes in JS! */
  border: 2px solid #3b82f6;
}
```

### Scoped CSS in Vue and Svelte
Rather than hashing class names, Vue and Svelte append a unique data attribute to the HTML element and rewrite the CSS selectors at compile time:
```html
<!-- Vue SFC: <template> -->
<div class="card">Hello</div>

<!-- Compiled HTML -->
<div class="card" data-v-f3f3eg9>Hello</div>
```
```css
/* Compiled CSS */
.card[data-v-f3f3eg9] {
  padding: 1.5rem;
}
```

### The Global Escape Hatch (`:global`)
When you need to style a third-party child component or an un-scoped DOM element rendered by an external library:
```css
/* Styles .markdown-body locally, but preserves global unhashed <code> tags */
.markdownBody :global(pre code) {
  background-color: #0f172a;
}
```

---

## 3. How It Works Internally

```
CSS Modules Build-Time Compilation Lifecycle:

  1. Source Files:
     Button.module.css ──> .btn { background: blue; }
     Button.tsx        ──> <button className={styles.btn}>

  2. PostCSS / Bundler (Vite / Webpack / Next.js):
     - Calculates content hash: md5("Button.module.css" + "btn") = "9x7a2"
     - Transforms CSS to: .Button_btn__9x7a2 { background: blue; }
     - Generates JS Object: { btn: "Button_btn__9x7a2" }

  3. Runtime Output:
     - HTML renders: <button class="Button_btn__9x7a2">
     - CSSOM contains: .Button_btn__9x7a2 { background: blue; }
     - Collision probability: ZERO!
```

```
CSS Modules vs. Vue Scoped CSS vs. Native @scope:
┌─────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Criteria        │ CSS Modules          │ Vue / Svelte Scoped  │ Native CSS `@scope`  │
├─────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Mechanism       │ Class Hashing        │ Data Attribute Inset │ Runtime CSSOM Engine │
│ Build Tool Need │ Mandatory (Bundler)  │ Mandatory (Compiler) │ None (Pure CSS)      │
│ Output Classes  │ `.Card_btn__7x2`     │ `.btn[data-v-xxxx]`  │ `.btn` (Standard)    │
│ Donut Scoping   │ No                   │ Partial (`:deep()`)  │ Full (`to (.limit)`) │
└─────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 4. Syntax & Usage

### Standard CSS Module File
```css
/* Card.module.css */
.card {
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
}

.title {
  margin-top: 0;
  color: #0f172a;
}

/* Global rule inside module */
:global(.theme-dark) .card {
  background-color: #0f172a;
  border-color: #334155;
}
```

### JavaScript / React Consumption
```tsx
import styles from './Card.module.css';

export function Card({ title, children }: CardProps) {
  return (
    <div className={styles.card}>
      <h3 className={styles.title}>{title}</h3>
      <div className={styles.body}>{children}</div>
    </div>
  );
}
```

---

## 5. Practical Examples

### Example 1: Full React TypeScript Modal with CSS Modules
An encapsulated dialog with backdrop and entrance animation using CSS Modules.

```css
/* Modal.module.css */
.backdrop {
  position: fixed;
  inset: 0;
  background-color: rgba(15, 23, 42, 0.6);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 50;
}

.window {
  background: white;
  border-radius: 0.75rem;
  padding: 2rem;
  max-width: 32rem;
  width: 90%;
  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
  animation: popIn 0.25s cubic-bezier(0.16, 1, 0.3, 1);
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 1rem;
}

.closeBtn {
  background: none;
  border: none;
  font-size: 1.25rem;
  cursor: pointer;
  color: #64748b;

  &:hover {
    color: #0f172a;
  }
}

@keyframes popIn {
  from {
    opacity: 0;
    transform: scale(0.95);
  }
  to {
    opacity: 1;
    transform: scale(1);
  }
}
```

```tsx
// Modal.tsx
import styles from './Modal.module.css';

export function Modal({ isOpen, onClose, children }) {
  if (!isOpen) return null;

  return (
    <div className={styles.backdrop} onClick={onClose}>
      <div className={styles.window} onClick={(e) => e.stopPropagation()}>
        <div className={styles.header}>
          <h2>Dialog Title</h2>
          <button className={styles.closeBtn} onClick={onClose}>✕</button>
        </div>
        <div>{children}</div>
      </div>
    </div>
  );
}
```

### Example 2: Class Composition across Files (`composes`)
Reusing base typography styles from a shared utility module inside a component module.

```css
/* typography.module.css */
.truncateText {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* UserProfile.module.css */
.userName {
  composes: truncateText from './typography.module.css';
  font-weight: 700;
  color: #2563eb;
}
```

### Example 3: Deep Selector Penetration in Vue Scoped Styles (`:deep()`)
Overriding styles in a nested child component without breaking parent scoping.

```html
<!-- Vue SFC -->
<template>
  <div class="calendar-wrapper">
    <ThirdPartyDatePicker />
  </div>
</template>

<style scoped>
.calendar-wrapper {
  padding: 1rem;
}

/* :deep() penetrates down to child component's un-scoped DOM */
.calendar-wrapper :deep(.dp__input) {
  border-color: #2563eb;
  border-radius: 0.5rem;
}
</style>
```

### Example 4: Type-Safe CSS Modules via Typed-CSS-Modules
Generating TypeScript `.d.ts` definitions automatically so invalid class names cause build-time compiler errors.

```typescript
// Card.module.css.d.ts (Auto-generated by typed-css-modules)
export const card: string;
export const title: string;
export const body: string;
export const isFeatured: string;
```

```tsx
// TypeScript verifies styles.title exists!
// Writing styles.titl fails compile check!
<h3 className={styles.title}>{title}</h3>
```

### Example 5: Migrating from CSS Modules to Native `@scope`
Transitioning a legacy CSS Module to zero-build native browser scoping.

```css
/* Modern Native CSS Alternative (Zero build-step hashing!) */
@scope (.c-card) to (.c-card__sub-item) {
  :scope {
    padding: 1.5rem;
    border: 1px solid #e2e8f0;
  }

  .title {
    color: #0f172a;
  }
}
```

---

## 6. Best Practices

1. **Keep class names simple inside modules**: Inside a CSS Module, there is no need for BEM prefixes (`.card__title`). Write clean, semantic identifiers (`.title`, `.body`, `.header`). The bundler will handle unique namespacing.
2. **Use CamelCase or Kebab-Case consistently**: In JavaScript, camelCase allows dot-notation (`styles.cardTitle`), whereas kebab-case requires bracket notation (`styles['card-title']`). Agree on a project-wide convention.
3. **Generate TypeScript definitions**: Use tools like `typed-css-modules` or Vite plugins (`vite-plugin-sass-dts`) to catch typos in class names at compile time.
4. **Use `:global()` sparingly**: Only use `:global()` to style un-scoped third-party libraries (like datepickers, code highlighters, or CMS prose).
5. **Combine CSS Modules with Global Design Tokens**: CSS Modules should consume global custom properties (`var(--color-primary)`) rather than hardcoding static design values.
6. **Ensure focus outlines remain intact**: Never remove focus rings inside module files without providing `:focus-visible` replacements.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Over-using BEM inside CSS Modules (`.card__header__title`) | Double-namespacing; generates ridiculous hashed class names. | Write simple classes: `.header`, `.title`. |
| Typing errors in JS: `styles.crad` | Evaluates to `undefined`; element renders completely unstyled without warning. | Generate TypeScript definitions for CSS Modules. |
| Over-relying on `:global()` everywhere | Destroys the encapsulation of CSS Modules; introduces global leaks. | Keep rules local; scope `:global()` inside parent module selectors. |
| Confusing `composes` with `@extend` | Expecting complex nested rules to copy over. | `composes` only appends class names in the generated JavaScript object. |
| Forgetting `.module.css` extension in Vite/Webpack | Bundler treats file as global CSS, dumping unhashed classes into the global scope. | Ensure all modular files use `.module.css` extension. |
| Expecting CSS Modules to work without a build tool | Native browsers cannot parse `.module.css` imports in JavaScript. | Use bundlers (Vite/Webpack) or migrate to native `@scope`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero Runtime Script Overhead**: Unlike CSS-in-JS (Emotion, Styled Components), CSS Modules has zero JavaScript runtime cost. All class hashing and file compilation occur at build time, outputting static, highly-cacheable `.css` files.
- **Automated Dead Code Elimination**: When a component is deleted, its CSS Module import is removed, allowing the bundler to automatically tree-shake the CSS.

### Accessibility (A11y)
- **Standard DOM Semantics**: CSS Modules does not affect the Accessibility Tree. Screen readers and assistive devices interact with standard HTML elements without interference from hashed class names.

### Browser Compatibility
- Universal. The output of CSS Modules is standard CSS classes and standard HTML elements, supported across 100% of web browsers.

---

## 9. Comparison / When to Use What

| Feature | CSS Modules | Vue / Svelte Scoped | Native `@scope` | CSS-in-JS (Styled) |
| :--- | :--- | :--- | :--- | :--- |
| **Encapsulation Level** | High (Hashed classes)| High (Data attributes)| High (CSSOM Root) | High (Hashed classes) |
| **Build Tool Required?** | **Yes** | **Yes** | **No** | **Yes** |
| **Runtime JS Cost** | **Zero (0 KB)** | **Zero (0 KB)** | **Zero (0 KB)** | High (~12 KB runtime) |
| **SSR Streaming Friendly**| Flawless | Flawless | Flawless | Complex hydration |
| **Standard CSS Syntax** | 100% Standard CSS | 100% Standard CSS | 100% Standard CSS | Template literals / JS |

---

## 10. Interview Questions

### Q1: How does CSS Modules work under the hood during a production build?
<details>
<summary>View Answer</summary>
During compilation, a PostCSS loader (in Webpack or Vite) parses `.module.css` files. For every selector class, it generates a unique cryptographic hash based on the file path and class name (e.g., `[name]_[local]__[hash:base64:5]`). It rewrites the stylesheet using the hashed classes and simultaneously generates a JavaScript module exporting a dictionary object mapping original class names to their hashed values. At runtime, the component injects the hashed string into the element's `class` attribute.
</details>

### Q2: Why does CSS Modules have zero runtime performance cost compared to CSS-in-JS?
<details>
<summary>View Answer</summary>
CSS-in-JS libraries (like Styled Components or Emotion) execute JavaScript at runtime in the browser to parse template strings, calculate dynamic styles, generate hashes, and inject `<style>` tags into the document `<head>` during render cycles. CSS Modules executes all transformations at build time. The browser receives pre-compiled static `.css` files that are cached and parsed natively by the browser's C++ engine with zero JavaScript execution overhead.
</details>

### Q3: How does the `composes` keyword work in CSS Modules?
<details>
<summary>View Answer</summary>
The `composes` keyword does not copy or duplicate CSS declarations like Sass `@extend`. Instead, it alters the JavaScript class map exported by the module. When `.btnPrimary { composes: baseBtn; }` is evaluated, the exported object produces `{ btnPrimary: "Button_baseBtn__123 Button_btnPrimary__456" }`. When the element renders, both class names are automatically applied in the HTML.
</details>

### Q4: How does Vue / Svelte Scoped CSS differ from CSS Modules?
<details>
<summary>View Answer</summary>
CSS Modules transforms class names into unique hashes. Vue and Svelte Scoped CSS keep class names readable and intact, but append an autogenerated data attribute (e.g. `data-v-7ba5bd90`) to all template elements during template compilation. The compiler then rewrites the CSS selectors to include an attribute selector: `.card[data-v-7ba5bd90] { ... }`.
</details>

### Q5: What is the purpose of the `:global` pseudo-selector in CSS Modules?
<details>
<summary>View Answer</summary>
By default, CSS Modules hashes every class in the file. `:global(.class-name)` tells the compiler to exempt that specific selector from hashing, outputting it as a plain, un-hashed global class. This is essential when authoring styles that must target third-party library widgets, CMS-rendered HTML, or root document themes (`:global(.theme-dark)`).
</details>

### Q6: Can CSS Modules classes be typed in TypeScript?
<details>
<summary>View Answer</summary>
Yes. Using tools like `typed-css-modules` or Vite's `vite-plugin-sass-dts`, build pipelines can automatically emit companion `.d.ts` declaration files for every `.module.css` file. TypeScript then verifies that any imported `styles.myClass` reference exists, turning missing or misspelled classes into build-time compilation errors.
</details>

### Q7: How does native CSS `@scope` compare to CSS Modules?
<details>
<summary>View Answer</summary>
CSS Modules requires a build tool (Vite, Webpack) to hash class names, creating opaque classes that are difficult to inspect in browser developer tools. Native CSS `@scope` achieves encapsulation directly in the browser's CSSOM at runtime without build tools, preserves clean semantic class names, and supports "Donut Scoping" (`to (.limit)`) to carve out holes for nested components.
</details>

### Q8: What happens if two separate CSS Module files declare a class named `.header`?
<details>
<summary>View Answer</summary>
They do not collide. Because the compiler includes the file path in the hash generation formula (e.g. `Card_header__9a82` vs `Modal_header__1b74`), the two classes compile to distinct identifiers, ensuring total isolation between components.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a CSS Module Card Component
**Objective**: Create a `Card.module.css` file with title, body, and featured modifier.
<details>
<summary>View Solution</summary>

```css
.card {
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
}

.title {
  margin-top: 0;
  font-size: 1.25rem;
  color: #0f172a;
}

.body {
  color: #64748b;
  line-height: 1.5;
}

.isFeatured {
  border-color: #3b82f6;
  box-shadow: 0 10px 15px -3px rgba(59, 130, 246, 0.1);
}
```
</details>

### Exercise 2: React Consumption with Dynamic Class Names
**Objective**: Write the React JSX component that consumes the card module and applies `.isFeatured` conditionally.
<details>
<summary>View Solution</summary>

```tsx
import styles from './Card.module.css';

export function Card({ title, children, isFeatured }) {
  const cardClasses = `${styles.card} ${isFeatured ? styles.isFeatured : ''}`;

  return (
    <div className={cardClasses}>
      <h3 className={styles.title}>{title}</h3>
      <div className={styles.body}>{children}</div>
    </div>
  );
}
```
</details>

### Exercise 3: Composition between Modules
**Objective**: Compose an alert banner style from a shared button module using `composes`.
<details>
<summary>View Solution</summary>

```css
/* Shared.module.css */
.pill {
  border-radius: 9999px;
  padding: 0.25rem 0.75rem;
  font-size: 0.75rem;
}

/* Alert.module.css */
.badge {
  composes: pill from './Shared.module.css';
  background-color: #fef2f2;
  color: #991b1b;
}
```
</details>

### Exercise 4: Global Third-Party Markdown Integration
**Objective**: Style an external markdown previewer rendered inside a scoped component using `:global`.
<details>
<summary>View Solution</summary>

```css
.previewWrapper :global(.markdown-body pre) {
  background-color: #0f172a;
  color: #f8fafc;
  padding: 1rem;
  border-radius: 0.375rem;
}
```
</details>

### Exercise 5: Vue Scoped Deep Penetration
**Objective**: Write a Vue scoped rule that modifies an un-scoped pagination button inside a child table.
<details>
<summary>View Solution</summary>

```css
.table-container :deep(.pagination-btn) {
  background-color: #2563eb;
  color: white;
  border: none;
}
```
</details>

### Exercise 6: Accessible Focus Ring in CSS Modules
**Objective**: Ensure all interactive buttons styled in a module have high-contrast focus rings.
<details>
<summary>View Solution</summary>

```css
.actionBtn {
  background-color: #2563eb;
  color: white;
  padding: 0.5rem 1rem;
  border: none;
  border-radius: 0.375rem;
  cursor: pointer;

  &:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```jsx
// React Component Consumption
import styles from './Button.module.css';
<button className={styles.primary}>Click</button>
```

```css
/* Button.module.css */
.primary {
  background: #2563eb;
  color: white;
}

/* Composition */
.promoted {
  composes: primary;
  box-shadow: 0 4px 6px rgba(0,0,0,0.1);
}

/* Global Escape Hatch */
:global(.theme-dark) .primary {
  background: #3b82f6;
}
```

- CSS Modules hashes class names at build time to prevent global collisions.
- Zero runtime JavaScript overhead; outputs static, cacheable CSS.
- Use `composes` to share styles without duplicating CSS declarations.
- Use `:global()` for un-scoped third-party libraries.

---

## 13. Further Reading

- [GitHub: CSS Modules Specification](https://github.com/css-modules/css-modules)
- [Vite Documentation: CSS Modules](https://vitejs.dev/guide/features.html#css-modules)
- [Next.js Documentation: Built-in CSS Support](https://nextjs.org/docs/app/building-your-application/styling/css-modules)
- [Vue.js Guide: Scoped CSS](https://vuejs.org/api/sfc-css-features.html#scoped-css)

---

### Navigation
- **Previous**: [08-resets-and-normalisation.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/08-resets-and-normalisation.md)
- **Next**: [10-css-in-js-overview.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/10-css-in-js-overview.md)
