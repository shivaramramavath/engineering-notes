# CSS `@function` and Conditional `if()`

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Functions and Mixins Module Level 1 / CSS Values and Units Module Level 5 (W3C Working Draft) |
| **Baseline Status** | Emerging / Experimental (Active prototype in Chrome Canary behind Experimental Web Platform features flag) |
| **Browsers Tested** | Chrome Canary 131+ (Flag: Experimental Web Platform Features) |
| **Tested With** | W3C CSSWG Specification Drafts, Chrome Platform Status, CSSWG GitHub Resolutions |

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

Historically, Cascading Style Sheets lacked procedural abstractions. When developers needed to calculate modular typography scales, convert units (e.g. `rem` to `px`), compute fluid clamp values, or apply conditional logic based on variables, they had only two choices:
1. **Preprocessors (Sass / Less)**: Allowed `@function` and `@if`, but executed strictly at **build time**. They could not react to runtime browser states, container queries, or dynamic CSS custom properties.
2. **CSS Variable Hacks ("Space Toggles")**: Abused CSS variable fallback mechanics (e.g. `--is-active: ; --color: var(--is-active, red) var(--is-inactive, blue);`) to emulate binary boolean conditionals. These hacks were unreadable, fragile, and prone to breaking across browser versions.

To complete the evolution of CSS into a fully expressive, declarative design system, the CSS Working Group (CSSWG) approved two major specifications:
1. **The CSS `@function` At-Rule** (CSS Functions and Mixins Module Level 1): Allows developers to define custom, reusable, parametrized functions directly in CSS that execute at runtime.
2. **The Inline Conditional `if()` Function** (CSS Values and Units Level 5): Brings native branching logic—`if(condition: value; else: value)`—directly into property declarations.

---

## 2. Core Concepts

### The `@function` At-Rule
Defines a reusable mathematical or value-transformation routine directly in the stylesheet:
- Declared with a dashed-ident name (e.g. `@function --fluid-type(...)`).
- Accepts typed parameters with optional default fallback values.
- Uses `@return` to output the computed CSS value.

```css
@function --fluid-type(--min-size <length>, --max-size <length>) {
  @return clamp(var(--min-size), 2vw + 1rem, var(--max-size));
}

h1 {
  font-size: --fluid-type(1.5rem, 3rem);
}
```

### The Inline Conditional `if()`
Provides native inline branching without needing multiple class overrides:
- **Syntax**: `if(condition ? true-value : false-value)` or `if(condition: value; else: value)`.
- Supports style queries, media queries, supports checks, or variable comparisons directly inside property values.

```css
.card {
  /* Branch on container or variable state */
  padding: if(style(--dense: true): 0.5rem; else: 1.5rem);
}
```

### Scope and Parameter Shadowing
Inside an `@function` body:
- Parameters are accessible via `var(--param-name)`.
- Arguments are scoped exclusively to that function execution and do not leak into the global custom property cascade.
- Functions can call other standard CSS functions (`calc()`, `clamp()`, `oklch()`, `color-mix()`) or other custom `@function` declarations.

---

## 3. How It Works Internally

```
Build-Time (Sass) vs. Runtime Native @function:

Sass @function:
  [Sass CLI in Node.js] ──> Calculates value at build time
                        ──> Emits hardcoded 'font-size: 24px;' into CSS file
                        * Cannot adapt to runtime CSS variables, container queries, or viewport!

Native CSS @function:
  [Browser Style Engine]
     │ reads h1 { font-size: --fluid-type(1.5rem, 3rem); }
     ▼
  [CSS Function Evaluator (C++)]
     ├── Binds argument tokens: --min-size: 1.5rem, --max-size: 3rem
     ├── Evaluates body expressions: clamp(1.5rem, 2vw + 1rem, 3rem)
     └── Computes final value against live viewport!
```

```
Inline if() Evaluation Matrix:
┌─────────────────────────┬────────────────────────────────────────────────────────┐
│ Syntax Pattern          │ Evaluated Output                                       │
├─────────────────────────┼────────────────────────────────────────────────────────┤
│ `if(style(--theme: dark)│ If parent defines `--theme: dark`, outputs `#fff`.     │
│   : #fff; else: #000)`  │ Otherwise, outputs `#000`.                             │
├─────────────────────────┼────────────────────────────────────────────────────────┤
│ `if(media(max-width: 480│ If viewport $\le 480\text{px}$, outputs `100%`.        │
│   px): 100%; else: 50%)`│ Otherwise, outputs `50%`.                              │
└─────────────────────────┴────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Specification & Availability Status:
├── CSSWG Status: First Public Working Draft / Active Candidate Development (2024–2026)
├── Chrome Canary: Experimental implementation under flag (--enable-experimental-web-platform-features)
├── Safari / WebKit: Actively collaborating on specification in CSSWG
└── Firefox / Gecko: Standard tracking underway
```

### Proposed `@function` Grammar
```css
@function --function-name(--param-1 <type>, --param-2 <type>: default-value) {
  --intermediate-step: calc(var(--param-1) * 2);
  @return var(--intermediate-step);
}
```

### Proposed `if()` Grammar
```css
.element {
  property: if(
    test-condition-1: value-1;
    test-condition-2: value-2;
    else: default-value
  );
}
```

---

## 5. Practical Examples

### Example 1: Procedural Fluid Clamp Generator
Eliminate repetitive `clamp()` mathematical formulas by defining a fluid typography helper.

```css
@function --fluid-clamp(
  --min-font <length>,
  --max-font <length>,
  --min-vp <length>: 320px,
  --max-vp <length>: 1200px
) {
  --slope: calc((var(--max-font) - var(--min-font)) / (var(--max-vp) - var(--min-vp)));
  --intercept: calc(var(--min-font) - var(--slope) * var(--min-vp));
  @return clamp(
    var(--min-font),
    calc(var(--intercept) + var(--slope) * 100vw),
    var(--max-font)
  );
}

/* Consumption across typography elements */
h1 {
  font-size: --fluid-clamp(2rem, 4.5rem);
}

h2 {
  font-size: --fluid-clamp(1.5rem, 2.75rem);
}
```

### Example 2: Negative Half-Margin Centerer
A utility function that calculates negative offsets for absolute centering.

```css
@function --half-negative(--dimension <length>) {
  @return calc(var(--dimension) / -2);
}

.modal-center {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 24rem;
  margin-left: --half-negative(24rem); /* Evaluates to -12rem */
}
```

### Example 3: Conditional Inline Component Theming via `if()`
Selecting colors dynamically based on parent container variables.

```css
.badge {
  /* Dynamic color based on custom property status */
  background-color: if(
    style(--status: "success"): #10b981;
    style(--status: "warning"): #f59e0b;
    style(--status: "error"):   #ef4444;
    else:                       #64748b
  );
  color: #ffffff;
}
```

### Example 4: Automatic Contrast Text Picker
A custom function that calculates high-contrast black or white text depending on background lightness.

```css
@function --contrast-text(--bg <color>) {
  --lightness: oklch(from var(--bg) l 0 0);
  @return if(
    calc(var(--lightness) > 0.6): #0f172a;
    else:                         #ffffff
  );
}

.dynamic-card {
  background-color: var(--card-accent);
  color: --contrast-text(var(--card-accent));
}
```

### Example 5: Unit Conversion Function (`rem` to `px`)
A function that converts REM units to pixel equivalents based on a base scale.

```css
@function --rem-to-px(--rem-val <length>) {
  @return calc(var(--rem-val) / 1rem * 16px);
}
```

---

## 6. Best Practices

1. **Always type-annotate `@function` parameters**: Specifying `<length>`, `<color>`, `<number>`, or `<percentage>` guarantees that invalid argument types fail early during style parsing.
2. **Provide default values for optional parameters**: When defining helper functions, supply sensible defaults (e.g. `--base-size: 16px`) so callers can omit boilerplate parameters.
3. **Keep function bodies side-effect-free**: Custom functions must strictly compute and return values via `@return`; they cannot declare style properties or mutate external stylesheets.
4. **Use `if()` to eliminate fragile space-toggle hacks**: Replace complex binary custom property fallback tricks with explicit declarative `if()` statements.
5. **Always provide an `else:` branch in `if()` statements**: Omitting the `else:` branch can cause the property to resolve to `guaranteed-invalid` at computed-value time if none of the test conditions match.
6. **Use progressive enhancement**: Guard experimental custom functions with preprocessor compilation or standard CSS fallbacks until Baseline adoption is universal.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `--` prefix on function name | Invalid at-rule grammar; parser discards `@function`. | All custom functions must begin with `--`: `@function --name()`. |
| Writing style declarations inside `@function` | Functions cannot declare properties; parser errors out. | Only write internal variable math and `@return`. |
| Omitting `else:` branch in `if()` | Property becomes invalid at computed-value time when conditions fail. | Always supply a safe fallback: `else: <fallback>`. |
| Passing wrong data types into typed parameters | Function computation fails and returns initial/invalid value. | Ensure arguments match the parameter syntax type. |
| Attempting infinite recursive function calls | Browser detects recursion and halts with stack overflow error. | Keep functions acyclic and bounded. |
| Relying on `@function` in production today without polyfills | Fails in standard consumer browsers (currently experimental draft). | Use PostCSS plugins or standard `calc()` formulas for production fallbacks. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero Runtime Scripting**: Native `@function` computations occur directly in the browser's C++ rendering engine, avoiding JavaScript thread serialization overhead.
- **Result Caching**: Browser engines cache pure function evaluations for identical argument signatures across DOM nodes.

### Accessibility (A11y)
- Algorithmic color contrast functions (like `--contrast-text()`) ensure that user-generated or dynamic themes mathematically maintain WCAG AA/AAA contrast ratios under all conditions.

### Browser Compatibility
- **Status**: Emerging Specification (CSSWG Working Draft).
- **Prototyping**: Available in Chrome Canary behind the "Experimental Web Platform features" flag.
- **Feature Detection**:
```css
@supports (@function --test() { @return 1; }) {
  /* @function supported */
}
```

---

## 9. Comparison / When to Use What

| Capability | CSS `@function` | Sass `@function` | CSS Custom Properties (`calc()`) |
| :--- | :--- | :--- | :--- |
| **Execution Time** | **Runtime (Browser)** | Build Time (Node/Dart) | Runtime (Browser) |
| **Parametrized Arguments** | **Yes** | Yes | No (relies on variable cascade) |
| **Dynamic Viewport Access**| **Yes (`100vw`, `cqw`)**| No (static only) | Yes |
| **Tooling Dependency** | None (Native) | Requires Sass compiler | None (Native) |
| **Browser Availability** | Experimental Draft | N/A (Compiles to CSS) | Widely Available |

---

## 10. Interview Questions

### Q1: What is the primary difference between Sass `@function` and the proposed native CSS `@function`?
<details>
<summary>View Answer</summary>
Sass `@function` is a build-time preprocessor macro evaluated in Node.js or Dart before the CSS is served. It can only compute values from static constants and cannot access runtime browser states. Native CSS `@function` executes directly within the browser's style engine at runtime, allowing functions to accept dynamic CSS custom properties, container query units (`cqw`), viewport units (`vw`), and perform live mathematical transformations in response to DOM mutations.
</details>

### Q2: How does the proposed inline `if()` function replace legacy CSS "space toggles"?
<details>
<summary>View Answer</summary>
Space toggles relied on setting custom properties to a single whitespace character or `initial`, exploiting the CSS variable fallback mechanism (`var(--toggle, fallback)`) to emulate binary on/off switches. Space toggles were difficult to read, unmaintainable, and fragile. The `if()` function provides clean, expressive, first-class conditional branching syntax: `if(style(--dense: true): 4px; else: 12px)`.
</details>

### Q3: Why must custom function names begin with a double hyphen (`--`)?
<details>
<summary>View Answer</summary>
Following the established design pattern for CSS Custom Properties (`--var`) and `@property`, custom function identifiers are prefixed with `--` to ensure they never collide with future native CSS functions (such as `calc()`, `min()`, or `oklch()`) introduced by the W3C.
</details>

### Q4: Can a custom `@function` declare CSS style rules like `margin: 10px` inside its body?
<details>
<summary>View Answer</summary>
No. `@function` is strictly a value-returning mathematical routine, designed to output a single CSS value via `@return`. Defining style rules, selectors, or component properties inside an `@function` is invalid. For reusing blocks of declarations, the CSSWG is developing the companion `@mixin` specification.
</details>

### Q5: What happens if an argument passed to a typed `@function` parameter does not match its type syntax?
<details>
<summary>View Answer</summary>
If an argument fails type validation against the parameter's syntax descriptor (for example, passing `#ff0000` to a parameter typed as `<length>`), the function call is considered invalid at computed-value time, and the property invoking it will resolve to its initial or inherited value.
</details>

### Q6: Can a custom function call another custom function?
<details>
<summary>View Answer</summary>
Yes. Custom functions are composable. A function can invoke other standard CSS mathematical functions (`calc()`, `clamp()`) as well as other custom `@function` routines, provided the execution call graph does not form a circular or recursive loop.
</details>

### Q7: What is the benefit of parameter type annotation in `@function`?
<details>
<summary>View Answer</summary>
Type annotations (e.g. `--size <length>`) allow the browser's CSS parser to validate arguments upfront, ensure proper unit arithmetic during `calc()` execution, and enable developer tooling (IDEs, linters) to provide autocomplete and type diagnostics.
</details>

### Q8: What is the current standard status of CSS `@function` and `if()`?
<details>
<summary>View Answer</summary>
Both features are published as official W3C Working Drafts under the CSS Functions and Mixins Module Level 1 and CSS Values and Units Module Level 5. They are actively being prototyped in Chromium (Chrome Canary) and discussed among browser vendors for future Baseline inclusion.
</details>

---

## 11. Practice Exercises

### Exercise 1: Golden Ratio Typography Scaler
**Objective**: Write a proposed `@function --golden-step` that multiplies a base font size by powers of the Golden Ratio ($1.618$).
<details>
<summary>View Solution</summary>

```css
@function --golden-step(--base <length>, --exponent <integer>: 1) {
  --ratio: 1.618;
  @return calc(var(--base) * pow(var(--ratio), var(--exponent)));
}

h1 {
  font-size: --golden-step(1rem, 3); /* ~4.23rem */
}

h2 {
  font-size: --golden-step(1rem, 2); /* ~2.62rem */
}
```
</details>

### Exercise 2: Fluid Line Height Calculator
**Objective**: Create a custom function that shrinks line-height as font size grows to maintain typographic harmony.
<details>
<summary>View Solution</summary>

```css
@function --proportional-leading(--font-size <length>) {
  @return clamp(1.2, calc(1.8 - (var(--font-size) / 50px)), 1.6);
}

.headline {
  font-size: 2.5rem;
  line-height: --proportional-leading(2.5rem);
}
```
</details>

### Exercise 3: Conditional Density Padding with `if()`
**Objective**: Apply conditional component padding using the proposed inline `if()` function based on `--density`.
<details>
<summary>View Solution</summary>

```css
.table-cell {
  padding: if(
    style(--density: "compact"): 0.25rem 0.5rem;
    style(--density: "relaxed"): 1rem 1.5rem;
    else: 0.5rem 1rem
  );
}
```
</details>

### Exercise 4: Dynamic Aspect Ratio Shifter
**Objective**: Write an inline `if()` rule that switches video aspect ratio between `16 / 9` and `9 / 16` based on viewport orientation.
<details>
<summary>View Solution</summary>

```css
.responsive-video {
  width: 100%;
  aspect-ratio: if(
    media(orientation: portrait): 9 / 16;
    else: 16 / 9
  );
  object-fit: cover;
}
```
</details>

### Exercise 5: Linear RGB to OKLCH Illuminance Normalizer
**Objective**: Design a custom function that boosts the lightness of a base color by a specified percentage.
<details>
<summary>View Solution</summary>

```css
@function --boost-lightness(--base-col <color>, --boost <number>: 0.1) {
  @return oklch(from var(--base-col) calc(l + var(--boost)) c h);
}
```
</details>

### Exercise 6: Progressive Fallback Strategy for Experimental `@function`
**Objective**: Write production-safe CSS that uses standard `clamp()` math by default and enhances with `--fluid-type()` when supported.
<details>
<summary>View Solution</summary>

```css
/* Standard production baseline */
.title {
  font-size: clamp(1.5rem, 4vw, 3rem);
}

/* Progressive enhancement check */
@supports (font-size: --fluid-type(1rem, 2rem)) {
  .title {
    font-size: --fluid-type(1.5rem, 3rem);
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Custom Function Definition */
@function --double-size(--size <length>) {
  @return calc(var(--size) * 2);
}

/* Custom Function Invocation */
.box {
  width: --double-size(100px); /* 200px */
}

/* Inline if() Conditional */
.button {
  background: if(
    style(--primary: true): #2563eb;
    style(--danger: true):  #ef4444;
    else:                   #94a3b8
  );
}
```

- `@function --name()` defines reusable, typed runtime calculation routines.
- Evaluates dynamically inside the browser's CSS engine (unlike Sass).
- `if(condition: val; else: fallback)` replaces fragile space-toggle variable hacks.
- Active W3C Working Draft in development across browser vendor engines.

---

## 13. Further Reading

- [W3C CSS Functions and Mixins Module Level 1](https://drafts.csswg.org/css-mixins-1/)
- [W3C CSS Values and Units Module Level 5 (Inline if)](https://drafts.csswg.org/css-values-5/#if-notation)
- [CSSWG GitHub Discussions: Custom Functions in CSS](https://github.com/w3c/csswg-drafts/issues/9350)
- [Bramus: CSS Functions and Mixins Explained](https://www.bram.us/2024/06/18/css-functions-and-mixins/)

---

### Navigation
- **Previous**: [13-content-visibility-and-containment.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/13-content-visibility-and-containment.md)
- **Next**: [15-masonry-and-grid-lanes.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/15-masonry-and-grid-lanes.md)
