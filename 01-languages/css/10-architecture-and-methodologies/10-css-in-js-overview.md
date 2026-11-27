# CSS-in-JS and Zero-Runtime Styling

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Modern Styling Architecture, React Ecosystem & Web Performance Standards |
| **Baseline Status** | Industry Architectural Evolution (Runtime vs Zero-Runtime) |
| **Target Audience** | Staff Frontend Engineers, React Architects, Performance Engineers |
| **Tested With** | React 18/19, Next.js App Router (RSC), Vanilla Extract, StyleX, Emotion |

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

In 2014, Christopher Chedeau (Vjeux) delivered a landmark presentation titled *"React: CSS in JS"*, outlining fundamental problems with traditional CSS at scale: global namespace collisions, dead code accumulation, minification limits, non-deterministic cascade resolution, and isolation barriers.

This sparked the golden age of **Runtime CSS-in-JS** libraries:
- **Styled Components** and **Emotion** dominated the React ecosystem.
- Developers loved the ergonomics: co-located styles, component-scoped classes, and direct prop interpolation (`color: ${props => props.isPrimary ? 'blue' : 'gray'}`).

However, between 2020 and 2024, the web platform underwent major performance reckoning:
- **Heavy Runtime Overhead**: Runtime libraries parsed styles in the browser's JavaScript engine on every render cycle, serializing strings and injecting `<style>` tags dynamically. This caused high Total Blocking Time (TBT) and dropped frames during animations.
- **Incompatible with React Server Components (RSC)**: Modern server-streaming architectures (like Next.js App Router) stream HTML chunks across the network. Runtime CSS-in-JS relies on React Client Context (`React.createContext`), making it fundamentally incompatible with Server Components.

This triggered the migration to **Zero-Runtime / Build-Time CSS-in-JS** (such as **Vanilla Extract**, **StyleX** by Meta, and **Panda CSS**), combining the developer ergonomics of TypeScript styling with the raw performance of static, pre-compiled CSS files.

---

## 2. Core Concepts

### 1. Runtime CSS-in-JS (Legacy Paradigm)
- Libraries like Styled Components and Emotion execute **at runtime in the user's browser**.
- When a component renders, JavaScript parses template literals, computes dynamic values based on props, hashes a class name, and uses `document.head.appendChild(styleSheet)` or `CSSStyleSheet.insertRule()`.
- **The Cost**: Every UI state change taxes the browser's main JavaScript thread.

### 2. Zero-Runtime / Build-Time CSS-in-JS (Modern Paradigm)
- Libraries like **Vanilla Extract**, **StyleX**, and **Linaria** execute **exclusively at build time**.
- Developers write type-safe styles in TypeScript (`.css.ts`), but the compiler extracts all styles into static `.css` files, replacing JavaScript references with pre-hashed atomic class names.
- **The Benefit**: Zero JavaScript runtime payload, 100% compatible with React Server Components (RSC), and parsed by the browser's native C++ engine.

### 3. Atomic CSS Compilers (StyleX & Panda CSS)
Modern build-time tools decompose CSS declarations into atomic single-property classes:
```typescript
// Source: StyleX
const styles = stylex.create({
  card: {
    padding: 16,
    backgroundColor: 'white',
  }
});
```
At build time, StyleX extracts `.x123 { padding: 16px; }` and `.x456 { background-color: white; }`. This merges the ergonomics of CSS-in-JS with the fixed file-size ceiling of Tailwind CSS.

---

## 3. How It Works Internally

```
Runtime CSS-in-JS vs. Zero-Runtime Compilation Pipeline:

Runtime CSS-in-JS (Emotion / Styled Components):
  [Browser Downloads 150 KB JS Bundle]
     │
     ▼
  [React Component Renders in Browser]
     ├── JS String Interpolation: reads props.color
     ├── Hash Generator: computes md5 hash -> "css-1a2b3"
     ├── CSS Parser: compiles CSS in JavaScript engine (SLOW!)
     └── DOM Mutation: injects <style> tag into <head> (Triggers Reflow!)
  (Causes high Interaction to Next Paint (INP) latency and CPU spikes)

Zero-Runtime Build-Time (Vanilla Extract / StyleX):
  [Developer Writes TypeScript Styling]
     │
     ▼ (Vite / Webpack Build Step)
  [Extracts all CSS into static 'dist/bundle.css']
  [Replaces TS objects with static class strings: "card_base__8x7a"]
     │
     ▼
  [Browser Downloads Pre-Compiled Static CSS]
  (Zero runtime JS execution! 100% Native C++ browser layout at 60/120 FPS!)
```

```
Runtime vs. Zero-Runtime Architectural Comparison:
┌─────────────────────────┬─────────────────────────┬─────────────────────────┐
│ Metric                  │ Runtime (Emotion/Styled)│ Zero-Runtime (Vanilla)  │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ JavaScript Bundle Size  │ Heavy (~12–25 KB runtime│ **0 KB runtime**        │
│ Rendering Latency       │ Evaluated on main thread│ **Zero JS evaluation**  │
│ React Server Components │ **Incompatible**        │ **100% Compatible**     │
│ TypeScript Type Safety  │ Variable                │ **Strict / Autocomplete│
│ Dynamic Props Handling  │ Dynamic string template │ CSS Custom Properties   │
└─────────────────────────┴─────────────────────────┴─────────────────────────┘
```

---

## 4. Syntax & Usage

### 1. Legacy Runtime (Styled Components) Syntax
```jsx
import styled from 'styled-components';

// Runtime execution
const Button = styled.button`
  padding: 0.5rem 1rem;
  background-color: ${props => props.$primary ? '#2563eb' : '#e2e8f0'};
  color: ${props => props.$primary ? '#ffffff' : '#0f172a'};
  border-radius: 0.375rem;
`;
```

### 2. Modern Zero-Runtime (Vanilla Extract) Syntax
```typescript
// Button.css.ts (Executed strictly at build time!)
import { style } from '@vanilla-extract/css';

export const buttonBase = style({
  padding: '0.5rem 1rem',
  borderRadius: '0.375rem',
  fontWeight: 600,
  cursor: 'pointer',
  transition: 'background-color 0.15s ease',
  ':focus-visible': {
    outline: '2px solid #2563eb',
    outlineOffset: '2px',
  }
});
```

```tsx
// Button.tsx (Standard React / RSC Component)
import { buttonBase } from './Button.css';

export function Button({ children }: { children: React.ReactNode }) {
  return <button className={buttonBase}>{children}</button>;
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Theming with Zero-Runtime CSS (Vanilla Extract)
Handling dynamic props without runtime CSS evaluation by bridging TypeScript to CSS Custom Properties.

```typescript
// Card.css.ts
import { style, createVar } from '@vanilla-extract/css';

// Create a type-safe CSS custom property
export const cardColorVar = createVar();

export const cardStyle = style({
  padding: '1.5rem',
  borderRadius: '0.5rem',
  border: '1px solid #e2e8f0',
  backgroundColor: cardColorVar, // Consumes CSS variable dynamically!
});
```

```tsx
// Card.tsx (React Server Component)
import { cardStyle, cardColorVar } from './Card.css';
import { assignInlineVars } from '@vanilla-extract/dynamic';

export function Card({ customColor, children }: CardProps) {
  return (
    <div
      className={cardStyle}
      style={assignInlineVars({ [cardColorVar]: customColor })}
    >
      {children}
    </div>
  );
}
```

### Example 2: Meta's StyleX Component
Type-safe atomic styling developed by Meta for Facebook and Instagram web.

```typescript
// Card.tsx (StyleX)
import * as stylex from '@stylexjs/stylex';

const styles = stylex.create({
  card: {
    padding: 16,
    backgroundColor: '#ffffff',
    borderRadius: 8,
    boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
  },
  featured: {
    border: '2px solid #2563eb',
  }
});

export function Card({ isFeatured, children }) {
  return (
    <div {...stylex.props(styles.card, isFeatured && styles.featured)}>
      {children}
    </div>
  );
}
```

### Example 3: Static Theme Contract Definition
Defining contract tokens in TypeScript that compile directly to CSS Custom Properties.

```typescript
// theme.css.ts (Vanilla Extract)
import { createThemeContract, createTheme } from '@vanilla-extract/css';

export const vars = createThemeContract({
  color: {
    brand: null,
    background: null,
    text: null,
  },
});

export const lightTheme = createTheme(vars, {
  color: {
    brand: '#2563eb',
    background: '#ffffff',
    text: '#0f172a',
  },
});

export const darkTheme = createTheme(vars, {
  color: {
    brand: '#3b82f6',
    background: '#090d16',
    text: '#f8fafc',
  },
});
```

### Example 4: Responsive Breakpoints in Zero-Runtime Styling
Defining responsive rules in TypeScript that compile to clean CSS `@media` queries.

```typescript
// Layout.css.ts
import { style } from '@vanilla-extract/css';

export const responsiveGrid = style({
  display: 'grid',
  gridTemplateColumns: '1fr',
  gap: '1rem',
  '@media': {
    'screen and (min-width: 768px)': {
      gridTemplateColumns: 'repeat(3, 1fr)',
      gap: '2rem',
    },
  },
});
```

### Example 5: Type-Safe Variant Recipes with `@vanilla-extract/recipes`
Building multi-variant component APIs (like button sizes and colors) with full TypeScript auto-completion.

```typescript
// Button.recipe.ts
import { recipe } from '@vanilla-extract/recipes';

export const buttonRecipe = recipe({
  base: {
    borderRadius: '0.375rem',
    border: 'none',
    cursor: 'pointer',
    fontWeight: 600,
  },
  variants: {
    color: {
      primary: { backgroundColor: '#2563eb', color: 'white' },
      secondary: { backgroundColor: '#e2e8f0', color: '#1e293b' },
    },
    size: {
      small: { padding: '0.25rem 0.5rem', fontSize: '0.75rem' },
      medium: { padding: '0.5rem 1rem', fontSize: '0.875rem' },
      large: { padding: '0.75rem 1.5rem', fontSize: '1rem' },
    },
  },
  defaultVariants: {
    color: 'primary',
    size: 'medium',
  },
});
```

---

## 6. Best Practices

1. **Avoid Runtime CSS-in-JS for new greenfield projects**: The industry has decisively moved away from runtime libraries (Emotion, Styled Components) due to performance penalties and React Server Component incompatibility.
2. **Use Zero-Runtime CSS-in-JS if TypeScript integration is mandatory**: If your engineering team demands type safety and autocompletion in TypeScript, adopt **Vanilla Extract** or **StyleX**.
3. **Bridge dynamic props via CSS Custom Properties**: Instead of recalculating new CSS classes in JavaScript when props change, set a CSS custom property inline (`style={{ '--accent': propColor }}`) and let static CSS consume `var(--accent)`.
4. **Prefer CSS Modules or Native CSS for standard web applications**: Unless you require extensive compile-time TypeScript logic, standard CSS Modules or native CSS with `@layer` and `@scope` provides maximum portability with zero build plugin overhead.
5. **Always provide keyboard focus styling in TypeScript styles**: When writing styles in `.css.ts` or StyleX, always declare `:focus-visible` outlines.
6. **Ensure atomic styles are deterministic**: When using atomic compilers (StyleX/Panda), verify that property overrides resolve deterministically regardless of CSS output order.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Using Emotion / Styled Components in Next.js App Router | Breaks React Server Components; throws Context errors during SSR streaming. | Migrate to Zero-Runtime (Vanilla Extract) or CSS Modules. |
| Interpolating dynamic functions inside runtime template loops | Triggers continuous style injection on every render; degrades frame rates. | Extract static rules and use CSS variables for dynamic values. |
| Treating Zero-Runtime CSS like runtime JavaScript | Trying to access browser window objects inside `.css.ts` files crashes the build. | Keep `.css.ts` strictly declarative for build-time evaluation. |
| Missing build plugin in bundler configuration | `.css.ts` files fail to compile; bundler throws unknown token syntax errors. | Install the appropriate Vite/Webpack compiler plugin. |
| Writing duplicate styles across components | Bloats generated static CSS bundle. | Extract shared variants into reusable recipes. |
| Forgetting to export styles from `.css.ts` | Classes fail to attach to DOM nodes. | Ensure style objects are properly exported. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Interaction to Next Paint (INP)**: Replacing runtime CSS-in-JS with zero-runtime tools eliminates main-thread CPU spikes during user interactions, significantly improving Core Web Vitals.
- **Critical Path Delivery**: Zero-runtime tools output standard `.css` files that can be preloaded via `<link rel="preload">` and cached permanently by CDNs.

### Accessibility (A11y)
- Type-safe styling compilers allow teams to create reusable design system tokens that mathematically enforce minimum contrast and focus visibility across all components.

### Browser Compatibility
- Universal. Zero-runtime styling compiles to standard, static CSS classes and stylesheets compatible with all browsers.

---

## 9. Comparison / When to Use What

| Architecture | Runtime JS Engine? | React Server Components? | Type Safety | Output |
| :--- | :--- | :--- | :--- | :--- |
| **Vanilla Extract** | **No (0 KB)** | **100% Compatible** | **Full TypeScript** | Static `.css` |
| **StyleX (Meta)** | **No (0 KB)** | **100% Compatible** | **Full TypeScript** | Atomic `.css` |
| **Tailwind CSS** | No (0 KB) | 100% Compatible | Partial (Autocomplete)| Atomic `.css` |
| **CSS Modules** | No (0 KB) | 100% Compatible | Via `.d.ts` generators| Static `.css` |
| **Emotion / Styled** | **Yes (~15 KB)** | **Incompatible** | Variable | Dynamic `<style>` |

---

## 10. Interview Questions

### Q1: Why has the frontend industry largely migrated away from Runtime CSS-in-JS (Emotion, Styled Components)?
<details>
<summary>View Answer</summary>
The migration was driven by two major factors:
1. **Performance Penalties**: Runtime CSS-in-JS executes on the client's main JavaScript thread during every render cycle. Parsing strings, calculating hashes, and injecting `<style>` tags causes high Total Blocking Time (TBT), dropped animation frames, and poor Interaction to Next Paint (INP) scores.
2. **React Server Components (RSC) Incompatibility**: Modern server-streaming architectures (like Next.js App Router) stream rendered HTML without client-side React context. Runtime CSS-in-JS relies on React Client Context to inject styles, making it fundamentally incompatible with Server Components.
</details>

### Q2: What is "Zero-Runtime" CSS-in-JS, and how does it work?
<details>
<summary>View Answer</summary>
Zero-Runtime CSS-in-JS (e.g. Vanilla Extract, StyleX) allows developers to author styles using TypeScript and component-scoped objects, but evaluates and extracts all styles **at build time** using a bundler plugin. The compiler extracts all declarations into standard, static `.css` files and replaces the TypeScript references with pre-hashed static class names. The client browser receives pure CSS with zero JavaScript runtime overhead.
</details>

### Q3: How do you handle dynamic props (like user-customized colors) in Zero-Runtime CSS-in-JS?
<details>
<summary>View Answer</summary>
In runtime CSS-in-JS, developers interpolated props directly into template strings (`color: ${props => props.col}`). In zero-runtime architectures, dynamic values cannot be computed at build time. Instead, developers define static CSS rules that reference **CSS Custom Properties** (variables), and assign the dynamic value inline via the HTML `style` attribute (e.g. `style={{ '--accent-color': userColor }}`).
</details>

### Q4: How does Meta's StyleX differ from Vanilla Extract?
<details>
<summary>View Answer</summary>
While both are zero-runtime compilers, StyleX is an **atomic CSS compiler**. It decomposes declarations into single-property utility classes and guarantees deterministic property resolution (the last style applied in `stylex.props()` always wins, regardless of file order). Vanilla Extract generates scoped component stylesheets using standard CSS selectors, though it offers optional atomic plugins via `@vanilla-extract/sprinkles`.
</details>

### Q5: What is the purpose of `createThemeContract` in Vanilla Extract?
<details>
<summary>View Answer</summary>
`createThemeContract` defines a strict TypeScript contract or blueprint of required token keys without assigning actual values. When creating multiple themes (e.g., `lightTheme` and `darkTheme`), TypeScript enforces that every theme fulfills every single token defined in the contract, preventing missing variables at compile time.
</details>

### Q6: What happens when `<style>` tags are dynamically injected into the `<head>` at runtime?
<details>
<summary>View Answer</summary>
Every time a new `<style>` tag is inserted or modified in the document `<head>` via JavaScript, the browser must halt JavaScript execution, invalidate its internal CSSOM, recalculate styles across the DOM tree, and trigger a layout reflow. In pages with hundreds of components, this creates significant lag and frame drops.
</details>

### Q7: Can Zero-Runtime CSS-in-JS be used with non-React frameworks like Vue or Svelte?
<details>
<summary>View Answer</summary>
Yes. Because libraries like Vanilla Extract output standard static `.css` files and export plain string class names from TypeScript modules, they can be imported into Vue, Svelte, Solid, Astro, or vanilla JavaScript projects without React dependencies.
</details>

### Q8: What are "Recipes" in modern styling systems like Vanilla Extract and Panda CSS?
<details>
<summary>View Answer</summary>
A recipe is a type-safe multi-variant component API. It allows developers to define base styles along with variant dimensions (e.g. `size: ['sm', 'md', 'lg']`, `color: ['primary', 'danger']`). The compiler generates optimized classes and exposes a typed function that generates the correct composite class string based on passed props.
</details>

---

## 11. Practice Exercises

### Exercise 1: Basic Vanilla Extract Style Object
**Objective**: Author a `.css.ts` file creating a responsive card container with hover transitions.
<details>
<summary>View Solution</summary>

```typescript
// Card.css.ts
import { style } from '@vanilla-extract/css';

export const card = style({
  padding: '1.5rem',
  backgroundColor: '#ffffff',
  borderRadius: '0.5rem',
  border: '1px solid #e2e8f0',
  transition: 'transform 0.2s ease, box-shadow 0.2s ease',
  ':hover': {
    transform: 'translateY(-4px)',
    boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1)',
  },
});
```
</details>

### Exercise 2: Dynamic CSS Variable Bridge
**Objective**: Create a style rule that uses a CSS custom property variable for dynamic branding.
<details>
<summary>View Solution</summary>

```typescript
// BrandBox.css.ts
import { style, createVar } from '@vanilla-extract/css';

export const brandColorVar = createVar();

export const brandBox = style({
  padding: '1rem',
  backgroundColor: brandColorVar,
  color: '#ffffff',
  borderRadius: '0.375rem',
});
```
</details>

### Exercise 3: Type-Safe Style Contract Definition
**Objective**: Write a theme contract for typography and background tokens.
<details>
<summary>View Solution</summary>

```typescript
// contract.css.ts
import { createThemeContract } from '@vanilla-extract/css';

export const themeVars = createThemeContract({
  color: {
    surface: null,
    text: null,
    brand: null,
  },
  space: {
    sm: null,
    md: null,
  },
});
```
</details>

### Exercise 4: Media Query Grid Extraction
**Objective**: Create a responsive grid layout using Vanilla Extract that compiles to clean `@media` queries.
<details>
<summary>View Solution</summary>

```typescript
// Grid.css.ts
import { style } from '@vanilla-extract/css';

export const gridLayout = style({
  display: 'grid',
  gridTemplateColumns: '1fr',
  gap: '1rem',
  '@media': {
    'screen and (min-width: 64rem)': {
      gridTemplateColumns: 'repeat(4, 1fr)',
      gap: '1.5rem',
    },
  },
});
```
</details>

### Exercise 5: Button Recipe with Variants
**Objective**: Build a typed variant recipe for Primary and Danger buttons.
<details>
<summary>View Solution</summary>

```typescript
// Button.recipe.ts
import { recipe } from '@vanilla-extract/recipes';

export const buttonRecipe = recipe({
  base: {
    padding: '0.5rem 1rem',
    borderRadius: '0.375rem',
    border: 'none',
    fontWeight: 600,
    cursor: 'pointer',
  },
  variants: {
    intent: {
      primary: { backgroundColor: '#2563eb', color: 'white' },
      danger: { backgroundColor: '#ef4444', color: 'white' },
    },
  },
  defaultVariants: {
    intent: 'primary',
  },
});
```
</details>

### Exercise 6: Focus Ring Accessibility in TypeScript Styles
**Objective**: Ensure that a custom button component defined in TypeScript includes high-contrast focus rings.
<details>
<summary>View Solution</summary>

```typescript
// Interactive.css.ts
import { style } from '@vanilla-extract/css';

export const accessibleControl = style({
  ':focus-visible': {
    outline: '2px solid #2563eb',
    outlineOffset: '2px',
  },
});
```
</details>

---

## 12. Summary / Cheat Sheet

```typescript
// Zero-Runtime Pattern (Vanilla Extract)
// Button.css.ts (Build-Time Execution)
import { style } from '@vanilla-extract/css';

export const btn = style({
  padding: '8px 16px',
  backgroundColor: '#2563eb',
  color: 'white',
});

// Button.tsx (Runtime Component)
import { btn } from './Button.css';
export const Button = () => <button className={btn}>Click</button>;
```

- **Runtime CSS-in-JS** (Styled/Emotion) causes INP latency and breaks React Server Components.
- **Zero-Runtime Tools** (Vanilla Extract, StyleX) extract static `.css` files at build time.
- Bridge dynamic runtime props using **CSS Custom Properties** (`createVar()`).
- 100% compatible with modern streaming SSR and server architectures.

---

## 13. Further Reading

- [Vanilla Extract Official Documentation](https://vanilla-extract.style/)
- [Meta StyleX Documentation](https://stylexjs.com/)
- [Sam Magura: Why We're Breaking Up with CSS-in-JS](https://dev.to/srmagura/why-were-breaking-up-with-css-in-js-4g9b)
- [Josh W. Comeau: The Future of CSS-in-JS](https://www.joshwcomeau.com/css/css-in-js/)

---

### Navigation
- **Previous**: [09-css-modules-and-scoped-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/09-css-modules-and-scoped-css.md)
- **Next**: [11-sass-less-vs-native-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/11-sass-less-vs-native-css.md)
