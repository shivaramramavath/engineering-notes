# Custom Properties (CSS Variables)

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/08-inheritance-initial-unset-revert.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`var()` Universal; `@property` Baseline 2024 - Chrome 85, Safari 16.4, Firefox 128) |
| **Browsers Tested** | Chrome 120+, Firefox 128+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Custom Properties Level 1, CSS Properties & Values API Level 1, Playwright css_runner.js |

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

### What are CSS Custom Properties?
**CSS Custom Properties** (often called **CSS Variables**) are author-defined entities whose values can be declared once and reused throughout an entire stylesheet via the `var()` function. Custom property names always begin with a double-hyphen (`--`):

```css
:root {
  --brand-primary: #0284c7;
}

.button {
  background-color: var(--brand-primary);
}
```

### Why are CSS Variables Superior to Preprocessor Variables (Sass `$var`)?
Before native custom properties, developers used preprocessors like Sass or Less (`$primary-color: #0284c7`). However, preprocessor variables have a fatal architectural flaw:
- **Sass variables are static and compile-time only**: They compile into hardcoded hex values during your build step. They do not exist in the browser DOM, cannot inherit down the document tree, and cannot react dynamically to media queries, user interactions, or JavaScript DOM mutations.
- **CSS Custom Properties are dynamic and runtime-aware**: They participate fully in the CSS **Cascade**, **Inherit** down DOM subtrees, re-evaluate automatically within `@media` blocks, and can be read or written by JavaScript at 120 FPS (`element.style.setProperty('--x', e.clientX)`).

---

## 2. Core Concepts

### 1. Declaration and Consumption
- **Declaration**: Declared using `--identifier: value;`. Custom properties can store any sequence of valid CSS tokens (lengths, colors, strings, numbers, even partial expressions).
- **Consumption**: Retrieved via `var(--identifier, fallback)`.
- **Fallback Chaining**: You can supply fallbacks, including nested custom properties:
  ```css
  color: var(--theme-text, var(--fallback-text, #0f172a));
  ```

### 2. Scoping and the Cascade
Custom properties follow standard CSS inheritance rules:
- **Global Scope (`:root`)**: Declaring variables on `:root` makes them accessible to every element in the document.
- **Local / Component Scope**: Declaring variables on a component selector (`.card`) confines them to that element and its descendants.
- **Contextual Overrides**: Overriding a custom property inside a media query or modifier class automatically cascades down to all children without re-declaring properties on each child!

### 3. The Typed Revolution: `@property` (Houdini API)
Historically, custom properties were parsed purely as **untyped strings**. Because the browser engine did not know whether `--my-angle` was a number, length, or color, **it was impossible to animate or transition CSS custom properties** (e.g., rotating a gradient).

The **CSS Properties and Values API (`@property`)** solves this permanently by registering explicit types with the browser engine:
```css
@property --gradient-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}
```
Now, animating `--gradient-angle` smoothly transitions from `0deg` to `360deg`!

### 4. Invalid at Computed-Value Time
If a custom property provides a value that is syntactically invalid for the property consuming it (e.g., `--size: 20px; color: var(--size);`):
- CSS does **NOT** fall back to earlier declarations in the stylesheet!
- Instead, the property treats the value as **invalid at computed-value time** and resets the property to its **`unset`** value (`inherit` if naturally inherited, otherwise `initial`).

---

## 3. How It Works Internally

### Token Stream Preservation & Variable Substitution
In the browser engine's C++ parser (e.g., Blink's `CSSVariableData`):

```
+--------------------------------------------------------------------+
| PARSER STAGE: Encounter custom property "--primary: #0284c7;"      |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| PRESERVE TOKEN STREAM IN CSSOM RECORD                              |
| Stored as raw sequence of CSS tokens: [ HASH("#0284c7") ]          |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| STYLE RESOLVER STAGE: Encounter "color: var(--primary);"           |
| 1. Look up "--primary" in current element's Inherited Variable Map |
| 2. Splice tokens into destination property's token stream          |
| 3. Parse newly spliced stream into ComputedStyle (RGB color)       |
+--------------------------------------------------------------------+
```

- When an element mutates a custom property (`el.style.setProperty('--theme', 'dark')`), the engine only invalidates elements in that specific subtree that actually consume `var(--theme)`, avoiding global layout reflows.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| - var() and custom property inheritance: Universal (Baseline 2016)      |
| - @property at-rule: Widely Available (Baseline 2024 - All engines)     |
+-------------------------------------------------------------------------+
```

### Standard Usage with Fallbacks
```css
:root {
  --font-base: 1rem;
  --color-primary: #0284c7;
}

.card {
  /* Consume with fallback in case variable is undeclared */
  padding: var(--card-spacing, 1.5rem);
  color: var(--color-text, #1e293b);
  border: 1px solid var(--color-border, #e2e8f0);
}
```

### `@property` Definition Syntax
```css
@property --card-rotation {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

@property --brand-lightness {
  syntax: "<percentage>";
  inherits: true;
  initial-value: 60%;
}
```

---

## 5. Practical Examples

### Example 1: Basic Responsive Design System Tokens
```css
:root {
  --container-padding: 1rem;
  --brand-color: #2563eb;
  --title-size: 1.75rem;
}

/* Dynamically update design tokens inside media query */
@media (min-width: 768px) {
  :root {
    --container-padding: 2.5rem;
    --title-size: 2.5rem;
  }
}

.page-container {
  padding: var(--container-padding);
}

.page-title {
  font-size: var(--title-size);
  color: var(--brand-color);
}
```
*Notice*: Neither `.page-container` nor `.page-title` need their own media query! They automatically adapt as the root custom properties shift.

### Example 2: Real-World Animated Gradient Border via `@property`
Historically, CSS linear gradients could not be animated. By typing the angle token, it transitions smoothly:
```css
/* Register typed angle property */
@property --spin-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

.animated-border-card {
  position: relative;
  padding: 2rem;
  background-color: #0f172a;
  border-radius: 12px;
  color: #f8fafc;
}

/* Pseudo-element with rotating conical gradient */
.animated-border-card::before {
  content: "";
  position: absolute;
  inset: -2px;
  border-radius: 14px;
  background: conic-gradient(
    from var(--spin-angle),
    #38bdf8,
    #818cf8,
    #c084fc,
    #38bdf8
  );
  z-index: -1;
  animation: rotateBorder 4s linear infinite;
}

@keyframes rotateBorder {
  to {
    --spin-angle: 360deg;
  }
}
```

### Example 3: Edge Case — Circular Reference Resolution
What happens if two variables reference each other?
```css
:root {
  --color-a: var(--color-b, red);
  --color-b: var(--color-a, blue);
}

.box {
  background-color: var(--color-a);
}
```
*Engine Handling*: The browser engine detects the circular dependency loop during substitution. It treats both variables as **invalid at computed-value time**. The fallback values (`red`, `blue`) are ignored because the loop was formed before fallback substitution could complete! `.box` receives `background-color: transparent` (its `initial` value).

### Example 4: Failure Case — Invalid at Computed-Value Time
```css
.card {
  color: #2563eb; /* Normal rule */
}

.card {
  --bad-color: 42px; /* A length, NOT a color! */
  color: var(--bad-color); /* Invalid at computed value time */
}
```
*Failure Analysis*:
- Developers mistakenly assume that because `42px` is not a valid color, the browser will fall back to `color: #2563eb`.
- **It does not!** The second declaration won the cascade tie-breaker. Once substituted, `color: 42px` is invalid. Because the cascade has already executed, the browser resets `color` to its `unset` value (which inherits from parent, or becomes black).

### Example 5: High-Performance Mouse Tracking via JavaScript
```html
<div class="spotlight-card" id="card">
  <h3>Interactive Spotlight</h3>
  <p>Move your cursor over this card.</p>
</div>
```
```css
.spotlight-card {
  position: relative;
  padding: 3rem;
  background-color: #1e293b;
  border-radius: 12px;
  color: #ffffff;
  overflow: hidden;

  /* Default mouse coordinates */
  --mouse-x: 50%;
  --mouse-y: 50%;
}

.spotlight-card::before {
  content: "";
  position: absolute;
  inset: 0;
  background: radial-gradient(
    400px circle at var(--mouse-x) var(--mouse-y),
    rgba(56, 189, 248, 0.25),
    transparent 80%
  );
  pointer-events: none;
}
```
```javascript
// High performance 120 FPS runtime mutation
const card = document.querySelector('#card');
card.addEventListener('pointermove', (e) => {
  const rect = card.getBoundingClientRect();
  card.style.setProperty('--mouse-x', `${e.clientX - rect.left}px`);
  card.style.setProperty('--mouse-y', `${e.clientY - rect.top}px`);
});
```

---

## 6. Best Practices

1. **Standardize on Lowercase Kebab-Case**: Always name custom properties with lowercase words separated by hyphens (`--brand-primary`, never `--brandPrimary`). Custom property names are strictly case-sensitive.
2. **Define Default Tokens on `:root`**: Place baseline typography, spacing, and brand tokens on `:root` to ensure global inheritance.
3. **Register Typed Properties with `@property` for Animations**: Never attempt to animate raw custom properties without defining their syntax via `@property`.
4. **Supply Fallbacks for Critical Layout Properties**: When consuming variables in third-party or reusable component libraries, always provide fallbacks: `var(--gap, 1rem)`.
5. **Use Local Variables to Create Clean Component APIs**: Allow consumers to customize a component by exposing tokens (`--btn-bg`, `--btn-padding`) rather than forcing them to write complex CSS overrides.
6. **Mutate Variables in JavaScript Instead of Inline Styles**: Rather than updating multiple styles imperatively in JS (`el.style.left`, `el.style.top`), update custom properties (`el.style.setProperty('--x', ...)`) to keep layout logic in CSS.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Expecting CSS to fall back to earlier declarations on invalid value | Misunderstanding "Invalid at Computed-Value Time" | Property resets to `initial`/`inherit` instead of earlier CSS rule. | Ensure custom property contains valid syntax. |
| Inconsistent casing (`--brandColor` vs `--brandcolor`) | Forgetting that custom properties are case-sensitive | Variable fails to resolve; falls back to default. | Enforce lowercase kebab-case in Stylelint. |
| Animating custom properties without `@property` | Untyped strings cannot be interpolated | Values jump abruptly from start to finish with zero animation. | Register property type with `@property`. |
| Creating circular variable references | `--a: var(--b); --b: var(--a);` | Both variables fail; property becomes invalid at computed time. | Maintain a unidirectional token flow. |
| Concatenating unitless numbers with `px` via string | Attempting `var(--size)px` | Parser error; `10px` becomes `10 px` (invalid token sequence). | Use math multiplication: `calc(var(--size) * 1px)`. |
| Overloading `:root` with 1,000s of unused variables | Performance memory bloat | Slight memory overhead in style recalculation. | Scope component-specific tokens to components. |

---

## 8. Performance, Accessibility & Browser Support

### Performance Profile
- Reading and writing custom properties via JavaScript (`element.style.setProperty()`) is dramatically faster than mutating style classes because it does not trigger global CSSOM selector re-matching.
- Mutating custom properties consumed purely by `transform` or `opacity` runs on the **GPU Compositor Thread** at 120 FPS.

### Accessibility Safeguards
- Theme tokens managed via custom properties (`--text-color: light-dark(#000, #fff);`) ensure that contrast ratios remain accessible without needing duplicate stylesheets.

---

## 9. Comparison / When to Use What

| Feature | CSS Custom Properties | Sass Variables (`$var`) | Data Attributes (`data-*`) |
|:---|:---|:---|:---|
| **Execution Environment** | Client Browser DOM (Runtime) | Build-time Compiler (Node/Dart) | HTML DOM Tree |
| **Inherits Down DOM?** | **YES** | No | No |
| **Dynamic JS Mutation?** | **YES** (`setProperty()`) | No | Yes (`dataset.*`) |
| **Media Query Reactive?**| **YES** | No | No |
| **Animatable?** | **YES** (with `@property`)| No | No |
| **Primary Use Case** | Design tokens, dynamic themes | Compile-time constants | Storing element metadata |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does "Invalid at Computed-Value Time" mean in the CSS Custom Properties specification?</strong></summary>

When a custom property is substituted into a standard CSS property via `var()`, the browser validates whether the substituted value is valid for that property. If it is invalid (e.g., `color: var(--width)` where `--width: 20px`), the cascade has already finished selecting the winning rule. The browser **cannot** go back in time to apply an earlier CSS rule in the stylesheet. Instead, it declares the declaration "invalid at computed-value time" and sets the property to its **`unset`** value (which inherits if naturally inherited, or becomes `initial`).
</details>

<details>
<summary><strong>2. How does the `@property` at-rule enable CSS gradient animations that were previously impossible?</strong></summary>

By default, CSS custom properties are untyped token streams. When you write `background: conic-gradient(from var(--angle), ...)`, the browser treats `--angle` as plain text; it has no mathematical understanding of angles, so `transition: --angle 1s` cannot calculate intermediate values (interpolation). By defining `@property --angle { syntax: '<angle>'; inherits: false; initial-value: 0deg; }`, the browser registers `--angle` as a real mathematical angle, allowing the animation engine to interpolate degrees frame-by-frame.
</details>

<details>
<summary><strong>3. Why can't you concatenate units onto custom properties like `var(--size)px`? How do you fix it?</strong></summary>

In CSS tokenization, `var(--size)px` does not produce a single `<dimension-token>`. Instead, the tokenizer produces two distinct tokens: the evaluated value token followed by an identifier token `px`. Because property grammars expect a single length token, this sequence is syntactically invalid. The correct way to attach a unit to a unitless custom property is using `calc()` multiplication:
```css
width: calc(var(--size) * 1px);
```
</details>

<details>
<summary><strong>4. Are CSS Custom Property names case-sensitive?</strong></summary>

**Yes, strictly.** Unlike standard CSS property names (such as `color` or `DISPLAY`, which are case-insensitive), CSS Custom Properties are user-defined identifiers. `--brand-color` and `--Brand-Color` are treated as two distinct custom properties in memory.
</details>

<details>
<summary><strong>5. What is the difference between fallback behavior in `var(--a, var(--b, red))` vs a circular reference?</strong></summary>

In fallback chaining (`var(--a, var(--b, red))`), if `--a` is undefined, the engine evaluates `--b`. If `--b` is also undefined, it successfully applies `red`. However, if `--a` references `--b` and `--b` references `--a`, the engine detects a **circular dependency loop**. The specification mandates that all variables participating in the loop are instantly declared invalid at computed-value time; fallbacks are discarded.
</details>

<details>
<summary><strong>6. How can you register a typed custom property using JavaScript?</strong></summary>

Using the Houdini `CSS.registerProperty()` API:
```javascript
window.CSS.registerProperty({
  name: '--accent-color',
  syntax: '<color>',
  inherits: true,
  initialValue: '#0284c7'
});
```
This is the JavaScript programmatic equivalent of the CSS `@property` at-rule.
</details>

<details>
<summary><strong>7. Why are CSS Custom Properties particularly effective for dark mode theming?</strong></summary>

With custom properties, components are styled against abstract tokens (`background: var(--surface); color: var(--text);`) rather than hardcoded hex values. To implement dark mode, you do not need to re-declare background and text rules for hundreds of individual classes. You only need to redefine the root variables once inside a `prefers-color-scheme` media query or `.dark-theme` class, and the entire document tree updates instantly.
</details>

<details>
<summary><strong>8. What is the performance impact of updating a custom property on `:root` versus on a local component?</strong></summary>

Updating a custom property on `:root` forces the browser style resolver to check the entire DOM tree to determine which descendant elements consume that variable. Updating a custom property on a local component element (e.g., `#card.style.setProperty('--x', ...)`) confines style recalculation strictly to that component's subtree, yielding vastly superior rendering performance.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Cascading Button Token API (Easy)
**Task**: Build a `.btn` class that uses internal custom properties `--btn-bg`, `--btn-color`, and `--btn-padding` with sensible defaults. Then create a `.btn--primary` modifier that overrides only `--btn-bg` without repeating padding or border declarations.

<details>
<summary><strong>View Solution</strong></summary>

```css
.btn {
  /* Internal token defaults */
  --btn-bg: #e2e8f0;
  --btn-color: #0f172a;
  --btn-padding: 0.5rem 1rem;

  background-color: var(--btn-bg);
  color: var(--btn-color);
  padding: var(--btn-padding);
  border-radius: 6px;
  border: none;
  cursor: pointer;
  font-weight: 600;
}

.btn:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}

/* Modifier simply updates the variable */
.btn--primary {
  --btn-bg: #0284c7;
  --btn-color: #ffffff;
}
```
*Explanation*: Modifiers only need to alter custom properties; base styles remain untouched.
</details>

---

### Exercise 2: Unit Conversion with `calc()` (Easy)
**Task**: An API returns a unitless integer `--elevation: 8;`. Write a CSS declaration for `box-shadow` that uses this number to generate an 8px vertical blur.

<details>
<summary><strong>View Solution</strong></summary>

```css
.card {
  --elevation: 8;
  box-shadow: 0 calc(var(--elevation) * 1px) calc(var(--elevation) * 2px) rgba(0, 0, 0, 0.15);
}
```
*Explanation*: Multiplying a unitless number by `1px` or `2px` inside `calc()` converts it into a valid CSS length.
</details>

---

### Exercise 3: Register a Typed Color Property (Medium)
**Task**: Write the `@property` definition for `--highlight-color` that ensures it is treated strictly as a `<color>`, does not inherit to child elements, and has a default initial value of `#38bdf8`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@property --highlight-color {
  syntax: "<color>";
  inherits: false;
  initial-value: #38bdf8;
}
```
*Explanation*: Registers the property with the browser engine, enabling type validation and color interpolation.
</details>

---

### Exercise 4: Dynamic Theme Overrides via Media Queries (Medium)
**Task**: Define `--page-bg` and `--page-text` on `:root`. Invert their values inside `@media (prefers-color-scheme: dark)` and apply them to `body`.

<details>
<summary><strong>View Solution</strong></summary>

```css
:root {
  --page-bg: #ffffff;
  --page-text: #0f172a;
}

@media (prefers-color-scheme: dark) {
  :root {
    --page-bg: #0f172a;
    --page-text: #f8fafc;
  }
}

body {
  background-color: var(--page-bg);
  color: var(--page-text);
  font-family: system-ui, sans-serif;
  transition: background-color 0.3s ease, color 0.3s ease;
}
```
*Explanation*: Updating custom properties on `:root` cascades theme values across the document.
</details>

---

### Exercise 5: Animated Gradient Angle Transition (Hard)
**Task**: Create an interactive badge `.pulse-badge` that smoothly rotates its linear-gradient angle from `0deg` to `180deg` on hover using `@property` and `transition`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@property --gradient-angle {
  syntax: "<angle>";
  inherits: false;
  initial-value: 0deg;
}

.pulse-badge {
  display: inline-block;
  padding: 0.5rem 1rem;
  border-radius: 9999px;
  color: #ffffff;
  font-weight: 700;
  background-image: linear-gradient(var(--gradient-angle), #ec4899, #8b5cf6);
  transition: --gradient-angle 0.5s ease;
  cursor: pointer;
}

.pulse-badge:hover {
  --gradient-angle: 180deg;
}
```
*Explanation*: Because `--gradient-angle` is registered as `<angle>`, the transition engine interpolates degrees smoothly on hover.
</details>

---

### Exercise 6: Programmatic Variable Mutation in JavaScript (Hard)
**Task**: Write a JavaScript function `setElementTheme(element, themeObject)` that iterates over a key-value object of tokens and sets them as custom properties on the target element using the CSSOM API.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Applies custom properties to a DOM element dynamically.
 * @param {HTMLElement} element
 * @param {Record<string, string>} tokens
 */
function setElementTheme(element, tokens) {
  if (!element || !element.style) return;

  for (const [key, value] of Object.entries(tokens)) {
    // Ensure key starts with double hyphen
    const propName = key.startsWith('--') ? key : `--${key}`;
    element.style.setProperty(propName, value);
  }
}

// Example usage:
const card = document.querySelector('.card');
setElementTheme(card, {
  'surface-bg': '#f1f5f9',
  '--elevation': '12px'
});
```
*Explanation*: Uses `element.style.setProperty()` to modify the element's CSSOM variable map.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS CUSTOM PROPERTIES MASTER CHEAT SHEET                                                        |
+--------------------------+-------------------------------------+--------------------------------+
| Syntax                   | Example                             | Core Behavior                  |
+--------------------------+-------------------------------------+--------------------------------+
| Declaration              | --brand-color: #0284c7;             | Double-hyphen, CASE-SENSITIVE  |
| Consumption              | var(--brand-color, #000)            | Fallback used if var missing   |
| Global Scope             | :root { --token: val; }             | Accessible to all elements     |
| Local Scope              | .card { --token: val; }             | Accessible only in subtree     |
| Typed Definition         | @property --name { syntax: ... }    | Enables animation/transition   |
| Unit Conversion          | calc(var(--num) * 1px)              | Multiply to attach units       |
| Invalid Value Handling   | Invalid at computed time            | Resets to unset (inherit/init) |
| JS Read / Write          | el.style.setProperty('--x', '10px') | Dynamic runtime DOM updates    |
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [W3C CSS Custom Properties for Cascading Variables Module Level 1](https://www.w3.org/TR/css-variables-1/)
- [W3C CSS Properties and Values API Level 1 (@property)](https://www.w3.org/TR/css-properties-values-api-1/)
- [MDN Web Docs: Using CSS custom properties](https://developer.mozilla.org/en-US/docs/Web/CSS/Using_CSS_custom_properties)
- [MDN Web Docs: @property](https://developer.mozilla.org/en-US/docs/Web/CSS/@property)
- [web.dev: @property - Next-gen CSS custom properties](https://web.dev/articles/at-property)

---

[Previous: 10 - Colours & Color Spaces](10-colours.md) | [Section Index](README.md) | [Next: 12 - CSS Values & Math Functions](12-values-and-functions.md)
