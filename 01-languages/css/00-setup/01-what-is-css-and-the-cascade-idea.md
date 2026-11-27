# What is CSS & The Cascade Idea

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `01-languages/html/01-fundamentals/01-document-structure.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Foundation) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | Playwright css_runner.js, getComputedStyle(), stylelint |

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

### What is CSS?
**Cascading Style Sheets (CSS)** is a declarative, domain-specific stylesheet language used to describe the visual presentation, layout formatting, typography, color science, and dynamic motion of documents written in markup languages such as HTML, SVG, and MathML.

Unlike imperative programming languages (like JavaScript or Python) that execute sequential instructions to mutate state, CSS allows authors to declare *desired visual outcomes* based on document selectors. The browser engine's layout algorithm resolves conflicting declarations, calculates geometric coordinates, and renders the interface across any screen geometry.

### Why Does the "Cascade" Exist?
In October 1994, **Håkon Wium Lie** published the original proposal for Cascading Style Sheets at CERN. At the time, web architects debated whether presentation should be controlled strictly by the author (like desktop publishing) or by the reader (to accommodate visual disabilities and screen sizes).

Lie’s breakthrough proposal was the **Cascade**: an intentional resolution algorithm where **multiple competing entities contribute styles simultaneously**:
1. **The Browser (User Agent)**: Provides baseline defaults (e.g., links are blue and underlined, `<h1>` is large, `<button>` has a border).
2. **The User**: Applies personal preferences (e.g., default font size, dark mode, high contrast modes).
3. **The Author (Web Developer)**: Designs the bespoke interface of the web application.

The cascade is the deterministic algorithm that merges these competing streams into one final, computed visual state.

---

## 2. Core Concepts

### Declarative vs Imperative Styling
In an imperative model, you tell the computer *how* to draw:
`ctx.fillRect(10, 10, 100, 50);`
In CSS's declarative model, you declare *relationships and constraints*:
```css
.card {
  display: flex;
  justify-content: space-between;
}
```
The browser's layout engine automatically adapts that rule whether the viewport is 320px wide on an iPhone or 3840px wide on a 4K monitor.

### The Three Pillars of CSS Mechanics
CSS execution rests on three interrelated concepts:
1. **The Cascade**: Resolving which declaration wins when multiple rules target the exact same element property.
2. **Specificity**: A weighting algorithm that scores selectors based on ID, class/attribute/pseudo-class, and type elements.
3. **Inheritance**: The mechanism by which certain properties (like `color` and `font-family`) automatically pass down from parent elements to their descendants unless overridden.

```
+--------------------------------------------------------------------------+
|                        THE THREE PILLARS OF CSS                          |
+--------------------------------------------------------------------------+
| 1. THE CASCADE    | Merges User Agent, User, and Author styles based on  |
|                   | origin, importance (!important), layers, and order.  |
+-------------------+------------------------------------------------------+
| 2. SPECIFICITY    | A 3-component score (A, B, C) that breaks ties       |
|                   | between competing selectors within the same layer.   |
+-------------------+------------------------------------------------------+
| 3. INHERITANCE    | Passes computed typographic and color values down    |
|                   | the DOM tree to child nodes (e.g. body -> p -> span).|
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The CSS Engine Resolution Lifecycle
When the browser's CSS parser encounters stylesheets, it executes an internal sequence to calculate the final pixel values:

```
                  HTML Stream & CSS Stylesheets
                               │
                               ▼
                   CSS Parser Tokenization
                               │
                               ▼
                  CSSOM (CSS Object Model) Tree
                               │
            ┌──────────────────┴──────────────────┐
            ▼                                     ▼
   Declared Values                       User Agent Defaults
   (Author Styles)                       (Browser Stylesheet)
            │                                     │
            └──────────────────┬──────────────────┘
                               ▼
                       CASCADE RESOLUTION
   1. Origin & Importance (UA, User, Author, !important, Transitions)
   2. Cascade Layers (@layer definition order)
   3. Specificity ((A, B, C) tuple score)
   4. Order of Appearance (Last declared wins)
                               │
                               ▼
                        Specified Value
                               │
                               ▼
                     INHERITANCE EVALUATION
     (If no cascade winner: does property inherit? Yes -> parent value; No -> initial value)
                               │
                               ▼
                         Computed Value
           (Resolves relative units: rem -> px, urls -> absolute)
                               │
                               ▼
                           Used Value
           (Resolves layout dependencies: width: 50% -> 400px)
                               │
                               ▼
                           Actual Value
           (Rasterization rounding: 400.33px -> 400px display pixel)
```

### The 4 Value Stages of Every CSS Property
Every single CSS property on every DOM element progresses through four distinct value phases:
1. **Specified Value**: The value from the winning cascade rule, or inherited from parent, or initial default.
2. **Computed Value**: Resolved during style calculation before layout (e.g., relative units like `2rem` resolved to `32px`, keyword `red` resolved to `rgb(255, 0, 0)`).
3. **Used Value**: Resolved after the layout engine runs (e.g., percentage widths like `width: 50%` resolved to an absolute pixel number based on the parent's box).
4. **Actual Value**: The final pixel value after device hardware constraints and display pixel grid snapping (e.g., fractional pixels like `14.66px` rounded to `15px`).

---

## 4. Syntax & Usage

### Baseline Conformance Matrix

```
+--------------------------------------------------------------------------+
| CAPABILITY / CONCEPT       | BASELINE STATUS | ENGINE SUPPORT             |
+----------------------------+-----------------+----------------------------+
| Core CSS Syntax & Cascade  | Widely Available| Universal (All Engines)    |
| Cascade Layers (@layer)    | Widely Available| Chrome 99+, Safari 15.4+,  |
|                            |                 | Firefox 97+ (Baseline 2022)|
| CSS Custom Properties      | Widely Available| Universal (All Engines)    |
+--------------------------------------------------------------------------+
```

### The CSS Rule Set Structure
A CSS rule set consists of a **Selector** and a **Declaration Block**:

```css
/* Selector: targets specific DOM nodes */
.hero-title {
  /* Declaration Block: property-value pairs */
  color: #005a9c;           /* Property: Value; */
  font-size: 2rem;
  line-height: 1.25;
}
```

---

## 5. Practical Examples

### Example 1: Basic — The Cascade Order of Appearance
**Goal**: Demonstrate how equal-specificity rules resolve based on order of appearance in the stylesheet.

```html
<!-- HTML Structure -->
<p class="alert-message">System maintenance scheduled at midnight.</p>
```

```css
/* CSS Stylesheet */
.alert-message {
  color: #005a9c; /* Blue */
  background-color: #e1f5fe;
  padding: 12px;
}

/* Identical specificity (0, 1, 0), declared later in stylesheet */
.alert-message {
  color: #d84315; /* Rust Red */
}
```

**Measured / Computed Output**:
```
getComputedStyle(.alert-message):
- color: rgb(216, 67, 21) [Winning value: #d84315]
- background-color: rgb(225, 245, 254) [Merged from first rule]
- padding: 12px
```
**Explanation**: Because both rules target `.alert-message` with identical specificity `(0, 1, 0)`, the cascade breaks the tie using **Order of Appearance**. The second rule overrides `color`, while non-competing properties (`background-color`, `padding`) from the first rule are preserved.

### Example 2: Real-World — User Agent vs Author Reset
**Goal**: Override browser default user-agent styles with an accessible, predictable box model.

```html
<button type="button" class="btn-primary">Deploy Cluster</button>
```

```css
/* User Agent stylesheet provides: border, grey background, padding */

/* Author stylesheet overrides specific declarations */
.btn-primary {
  appearance: none;
  border: 2px solid transparent;
  background-color: #005a9c;
  color: #ffffff;
  padding: 10px 20px;
  border-radius: 4px;
  font-family: inherit;
  font-size: 1rem;
  cursor: pointer;
}

.btn-primary:focus-visible {
  outline: 3px solid #ffbf47;
  outline-offset: 2px;
}
```

**Measured / Computed Output**:
```
getComputedStyle(.btn-primary):
- background-color: rgb(0, 90, 156) [Author overrides UA button face]
- color: rgb(255, 255, 255)
- border-top-width: 2px
- cursor: pointer
```
**Explanation**: The Author stylesheet has higher precedence than the User Agent origin in the cascade, cleanly replacing default browser button styles.

### Example 3: Edge Case — Inherited vs Non-Inherited Properties
**Goal**: Prove why `color` passes down to child elements automatically, but `border` does not.

```html
<div class="card-parent">
  <p>Parent card paragraph with <strong>emphasized text</strong>.</p>
</div>
```

```css
.card-parent {
  color: #005a9c;           /* Inherited property */
  border: 2px solid #005a9c;/* Non-inherited property */
  padding: 16px;
}
```

**Measured / Computed Output**:
```
getComputedStyle(.card-parent p):
- color: rgb(0, 90, 156) [Inherited from .card-parent]
- border-top-width: 0px [Not inherited; defaults to initial value: none]

getComputedStyle(.card-parent strong):
- color: rgb(0, 90, 156) [Inherited through p from .card-parent]
- font-weight: 700 [From User Agent strong default]
```
**Explanation**: Typographic properties (`color`, `font-family`, `line-height`) naturally inherit down the DOM tree to prevent authors from having to style every nested tag. Layout and geometric properties (`border`, `margin`, `padding`, `width`) do **not** inherit, preventing destructive recursive borders.

### Example 4: Failure Case — Specificity Escalation War (The Anti-Pattern)
**Goal**: Illustrate how developers break stylesheets using high-specificity selectors and `!important`.

```html
<header id="main-header">
  <nav class="site-nav">
    <a href="/login" class="btn-login">Sign In</a>
  </nav>
</header>
```

```css
/* ANTI-PATTERN: Overly specific selector (1, 1, 1) */
#main-header .site-nav a {
  color: #333333;
}

/* Fails to take effect because (0, 1, 0) loses to (1, 1, 1) */
.btn-login {
  color: #005a9c; /* LOSES! */
}

/* DESPERATION ANTI-PATTERN: Escalating to !important */
.btn-login {
  color: #005a9c !important; /* Disastrous for maintenance */
}
```

**Measured / Computed Output**:
```
Without !important:
getComputedStyle(.btn-login): color: rgb(51, 51, 51) [#333333 wins due to ID selector]

With !important:
getComputedStyle(.btn-login): color: rgb(0, 90, 156) [!important overrides specificity, breaking future changes]
```
**Explanation**: Using ID selectors in stylesheets causes specificity wars. Idiomatic modern CSS uses flat class names (`.btn-login`) or Cascade Layers (`@layer`).

### Example 5: Accessible / Production-Grade — Cascade Layers Isolation
**Goal**: Use modern `@layer` to allow utility classes to override component styles cleanly without `!important`.

```html
<div class="component-card text-muted">
  System status operational.
</div>
```

```css
/* Explicitly define layer precedence order upfront */
@layer reset, components, utilities;

@layer components {
  /* High selector specificity (0, 2, 0) inside component layer */
  .component-card.component-card {
    color: #121212;
    background: #f5f5f5;
    padding: 16px;
  }
}

@layer utilities {
  /* Low selector specificity (0, 1, 0) inside higher layer */
  .text-muted {
    color: #767676;
  }
}
```

**Measured / Computed Output**:
```
getComputedStyle(.component-card.text-muted):
- color: rgb(118, 118, 118) [#767676 WINS!]
- background-color: rgb(245, 245, 245)
```
**Explanation**: In modern CSS (Baseline 2022+), **Layer Order takes precedence over Selector Specificity**. Because `@layer utilities` was declared after `@layer components`, `.text-muted` wins the cascade despite having lower selector specificity.

---

## 6. Best Practices

1. **Keep Specificity Flat**: Avoid ID selectors (`#nav`) and deep descendant combinators (`div ul li a`). Rely on single class selectors.
2. **Embrace the Cascade**: Do not fight the cascade; organize styles into logical layers using `@layer reset, base, components, utilities;`.
3. **Leverage Natural Inheritance**: Define global typography (`font-family`, `line-height`, `color`) on `:root` or `body` and let inheritance propagate it down the tree.
4. **Never Use `!important` as a Quick Fix**: `!important` alters the origin of the declaration, creating severe maintenance barriers.
5. **Always Set `box-sizing: border-box`**: Apply border-box globally to ensure padding does not break layout math.
6. **Design for Fallbacks**: Understand user-agent defaults so you only write declarations that actively change state.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
|:---|:---|:---|:---|
| Using ID selectors for styling (`#header`) | High | High specificity `(1, 0, 0)` blocks component class overrides | Use class selectors (`.header`) |
| Sprinkling `!important` across stylesheets | High | Escalates specificity wars; impossible to override later | Use `@layer` or refactor selectors |
| Assuming all properties inherit | Medium | Expecting `border` or `margin` to pass down to children | Explicitly declare properties or use `inherit` |
| Misunderstanding Order of Appearance | Medium | Confusing HTML class attribute order with stylesheet rule order | Reorder rules inside CSS file |
| Fighting browser User Agent defaults procedurally | Medium | Bloats stylesheet with useless declarations | Use a minimal CSS reset |
| Hardcoding pixel typography everywhere | High | Breaks accessibility zoom and browser user font scaling | Use relative units (`rem`, `em`, `clamp()`) |

---

## 8. Performance, Accessibility & Browser Support

### Performance: The CSSOM Construction
The browser cannot render any pixels on the screen until all linked CSS stylesheets are downloaded and parsed into the **CSSOM**. Complex, deeply nested selectors (e.g., `body div.container > ul li.item a.link`) force the style engine to evaluate right-to-left matches across hundreds of DOM nodes. Keep selectors shallow for sub-millisecond style resolution.

### Accessibility: Respecting User Font Settings
Users with low vision frequently configure their browser defaults to a base font size of 24px or 32px instead of the default 16px. If an author defines body text using hardcoded pixels (`font-size: 16px;`), the browser **completely ignores the user's accessibility setting**. Always use `rem` for typography.

### Browser Support
The cascade, inheritance, and core selector matching are universally supported across 100% of web browsers. Cascade Layers (`@layer`) are **Baseline Widely Available** since early 2022 across all engines.

---

## 9. Comparison / When to Use What

### Declarative CSS vs Inline Styles vs JavaScript Styling

| Criterion | External CSS (`.css`) | Inline Style (`style=""`) | CSS-in-JS (Runtime) |
|:---|:---|:---|:---|
| **Cascade & `@layer` Support** | Full Support | No `@layer`, pseudo-classes, or media queries | Varies (often strips layers) |
| **Caching & Performance** | Cached via HTTP headers | Bloats HTML payload | JavaScript runtime CPU overhead |
| **Specificity Score** | Normal `(A, B, C)` | High Precedence (`1, 0, 0, 0`) | Injects inline or generated classes |
| **Best Used For** | 99% of web styling | Dynamic JS-calculated values (e.g., coordinates) | Component-scoped design systems |

---

## 10. Interview Questions

### Q1: What are the primary inputs to the CSS Cascade algorithm, and in what order are they evaluated?
<details>
<summary>View Answer</summary>

Under the CSS Cascading and Inheritance specification, the cascade evaluates competing declarations in the following strict order:
1. **Origin & Importance**: Compares User Agent, User, and Author origins, plus active `!important` flags and CSS transitions/animations.
2. **Context**: Evaluates whether styles belong to Shadow DOM or Light DOM.
3. **Cascade Layers**: Evaluates `@layer` order (styles in later declared layers win over earlier layers).
4. **Specificity**: Compares selector weight tuples `(A, B, C)`.
5. **Order of Appearance**: If specificity is identical, the last declared rule in the stylesheet wins.
</details>

### Q2: What is the difference between a property's "Specified Value", "Computed Value", "Used Value", and "Actual Value"?
<details>
<summary>View Answer</summary>

1. **Specified Value**: The raw value assigned by the winning cascade rule, or inherited from parent, or the property's initial default.
2. **Computed Value**: The value resolved during style calculation before layout (relative units like `2rem` resolved to `32px`, named colors resolved to `rgb()`).
3. **Used Value**: The value resolved after the layout engine runs and calculates geometric dependencies (e.g., `width: 50%` resolved to an absolute pixel dimension like `450px`).
4. **Actual Value**: The final pixel value applied after rounding to match the display hardware's physical pixel grid (e.g., `450.33px` rounded to `450px`).
</details>

### Q3: Why does `color` inherit by default in CSS, while `margin` and `border` do not?
<details>
<summary>View Answer</summary>

The CSS Working Group intentionally designed inheritance based on human usability:
- Typographic and readability properties (`color`, `font-family`, `font-size`, `line-height`) inherit so that authors can establish cohesive typography on the `<body>` element without needing to manually restyle every paragraph, list item, and link.
- Box model and layout properties (`border`, `padding`, `margin`, `width`, `background`) do not inherit because inheriting them would produce catastrophic visual chaos (e.g., giving a parent card a 2px border would cause every child, span, and button inside it to also render a 2px border).
</details>

### Q4: Does the class attribute order in HTML matter for cascade resolution?
<details>
<summary>View Answer</summary>

**No**. The order of class names inside the HTML attribute (`<div class="blue red">` vs `<div class="red blue">`) has **zero effect** on cascade resolution. 

The cascade breaks ties using the **order of appearance of the rules within the CSS stylesheet**, not the HTML markup. Whichever class rule appears later in the CSS file wins.
</details>

### Q5: How do Cascade Layers (`@layer`) fundamentally change specificity?
<details>
<summary>View Answer</summary>

Historically, selector specificity always trumped rule location. With `@layer`:
- **Layer order is evaluated BEFORE selector specificity**.
- Declarations in a higher layer (declared later in `@layer a, b;`) **always beat declarations in an earlier layer**, regardless of how high the selector specificity is in the earlier layer.
- Unlayered styles have higher precedence than all layered normal styles.
</details>

### Q6: What does the global CSS keyword `revert` do compared to `unset`?
<details>
<summary>View Answer</summary>

- `unset`: Resets the property to `inherit` if it is naturally an inherited property, or to `initial` (spec default) if it does not naturally inherit.
- `revert`: Rolls the cascade back to the value defined in the **previous cascade origin**. For author styles, `revert` rolls back the property to the value defined by the User Agent (browser default) or User stylesheet.
</details>

### Q7: Why is setting `font-size: 16px` on `html` or `body` considered an accessibility anti-pattern?
<details>
<summary>View Answer</summary>

Setting `font-size: 16px` hardcodes the base font size to a fixed pixel value. If a visually impaired user has configured their browser settings to use a larger default font size (e.g., 24px), the hardcoded 16px declaration overrides and silences their preference.

Authors should leave the root font size alone or use `font-size: 100%`, and define all subordinate font sizes using relative `rem` units.
</details>

### Q8: What role did Håkon Wium Lie envision for user stylesheets in his original CSS proposal?
<details>
<summary>View Answer</summary>

Lie envisioned the web as a democratized medium where presentation was shared equally between author and reader. If a reader had visual impairments or preferred a specific typeface, their user stylesheet could negotiate with or override the author's styles. The cascade was explicitly created to mediate between user agent, author, and reader preferences.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Tracing the Cascade Winner
**Problem**: For the following markup and CSS, what is the computed `color` of the paragraph, and why?

```html
<p class="text-lead text-highlight">System online.</p>
```

```css
.text-highlight { color: orange; }
.text-lead { color: blue; }
```

<details>
<summary>View Solution</summary>

The computed color is **`blue`**.
**Reason**: Both selectors target single class names with identical specificity `(0, 1, 0)`. The cascade resolves the tie using Order of Appearance: `.text-lead` appears after `.text-highlight` in the CSS stylesheet. The order in the HTML `class` attribute is completely ignored.
</details>

### Exercise 2: Easy — Forcing Property Inheritance
**Problem**: By default, form `<button>` elements do not inherit the `font-family` of their parent container. Write the single CSS declaration to force buttons to inherit their parent's typeface.

<details>
<summary>View Solution</summary>

```css
button {
  font-family: inherit;
}
```
</details>

### Exercise 3: Medium — Resetting to User Agent Defaults with `revert`
**Problem**: An author stylesheet applied `list-style: none` to all `<ul>` tags. Write the class `.prose-list` that restores the native browser bulleted list appearance using the `revert` keyword.

<details>
<summary>View Solution</summary>

```css
ul.prose-list {
  list-style: revert;
  padding-inline-start: revert;
}
```
</details>

### Exercise 4: Medium — Overriding Specificity with Cascade Layers
**Problem**: Without modifying selector specificity or using `!important`, structure the following rules using `@layer` so that `.btn` wins over `.sidebar .nav .btn`.

<details>
<summary>View Solution</summary>

```css
@layer components, overrides;

@layer components {
  /* Specificity (0, 3, 0) */
  .sidebar .nav .btn {
    background-color: grey;
  }
}

@layer overrides {
  /* Specificity (0, 1, 0) - WINS because 'overrides' layer is declared after 'components' */
  .btn {
    background-color: blue;
  }
}
```
</details>

### Exercise 5: Hard — Tracing `!important` Inversion in Cascade Layers
**Problem**: In the following code, which color wins on `.box`, and why?

```css
@layer base, theme;

@layer base {
  .box { color: red !important; }
}

@layer theme {
  .box { color: green !important; }
}
```

<details>
<summary>View Solution</summary>

The computed color is **`red`**!
**Reason (The Inverted Importance Rule)**: In the CSS Cascade specification, while normal declarations in later layers override earlier layers, **`!important` declarations invert the layer order of precedence**. 

Declarations marked `!important` in earlier layers (`@layer base`) take precedence over `!important` declarations in later layers (`@layer theme`). This allows base reset and accessibility layers to protect their critical styles against theme overrides.
</details>

### Exercise 6: Hard — Automated Verification of Cascade Precedence
**Problem**: Write a vanilla JavaScript function that inspects an element and returns its computed `color` alongside the winning stylesheet rule.

<details>
<summary>View Solution</summary>

```javascript
function inspectWinningColor(element) {
  const computedColor = window.getComputedStyle(element).color;
  console.log('Resolved Computed Color:', computedColor);
  
  // Trace matching rules in stylesheets
  const matchedRules = [];
  for (const sheet of document.styleSheets) {
    try {
      for (const rule of sheet.cssRules) {
        if (rule.selectorText && element.matches(rule.selectorText)) {
          if (rule.style.color) {
            matchedRules.push({
              selector: rule.selectorText,
              color: rule.style.color,
              cssText: rule.cssText
            });
          }
        }
      }
    } catch (e) {
      // Cross-origin stylesheet access restriction guard
    }
  }
  return { computedColor, matchedRules };
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **CSS is Declarative**: Authors declare constraints and outcomes; the browser engine resolves geometry.
- **The Cascade Triad**: Cascade (precedence), Specificity (weight), and Inheritance (tree propagation).
- **Cascade Tie-Breaking**:
  1. Origin & Importance (Transitions &rarr; `!important` &rarr; Normal Author &rarr; UA).
  2. Cascade Layers (`@layer` definition order).
  3. Specificity (`A, B, C` score).
  4. Order of Appearance (Last declared wins).
- **Layer Inversion**: For normal styles, later layers win; for `!important` styles, earlier layers win!
- **Value Stages**: Specified &rarr; Computed (before layout) &rarr; Used (after layout) &rarr; Actual (pixel snapped).
- **Inheritance**: Typographic properties inherit; box-model/layout properties do not.

---

## 13. Further Reading

- [W3C CSS Cascading and Inheritance Level 5 Specification](https://www.w3.org/TR/css-cascade-5/)
- [MDN Web Docs: Introducing the CSS Cascade](https://developer.mozilla.org/en-US/docs/Web/CSS/Cascade)
- [W3C Historical Note: Cascading HTML Style Sheets Proposal (Håkon Wium Lie, 1994)](https://www.w3.org/People/howcome/p/cascade.html)
- [web.dev: Learn CSS — The Cascade](https://web.dev/learn/css/the-cascade/)

---

[Previous: Section Overview (README.md)](README.md) | [Section Index](README.md) | [Next: 02 - How the Browser Renders CSS ->](02-how-the-browser-renders-css.md)
