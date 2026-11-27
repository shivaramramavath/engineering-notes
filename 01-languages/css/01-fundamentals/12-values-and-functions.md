# CSS Values & Math Functions

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/01-syntax-rules-and-declarations.md`, `01-fundamentals/09-units.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`calc()`, `min()`, `max()`, `clamp()` Universal; `sin()`, `cos()`, `round()` Baseline 2023) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Values and Units Level 4, Playwright css_runner.js, stylelint |

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

### What are CSS Math Functions?
CSS is no longer a static formatting sheet; it is a full **computational layout engine**. CSS Math Functions enable authors to perform mathematical expressions, fluid interpolation, geometric trigonometry, and stepped rounding directly within stylesheet declarations without JavaScript.

The core math suite includes:
- **Arithmetic**: `calc()`
- **Extrema & Clamping**: `min()`, `max()`, `clamp()`
- **Stepped Rounding**: `round()`, `mod()`, `rem()`
- **Trigonometry**: `sin()`, `cos()`, `tan()`, `asin()`, `acos()`, `atan()`, `atan2()`
- **Exponential & Roots**: `pow()`, `sqrt()`, `hypot()`, `log()`, `exp()`
- **Sign & Absolute**: `abs()`, `sign()`

### Why are Math Functions Revolutionary for Responsive Design?
Before functions like `clamp()` and `min()`, responsive design required writing dozens of rigid `@media` query breakpoints (`320px`, `480px`, `768px`, `1024px`, `1280px`). This caused typography and layout gaps to "jump" abruptly at arbitrary screen sizes.

With modern math functions, you can express **continuous, fluid relationships**:
$$\text{font-size: clamp}(1.25\text{rem}, 2\text{vw} + 1\text{rem}, 2.5\text{rem})$$
Typography scales smoothly across every fraction of a pixel, completely eliminating breakpoint jumps.

---

## 2. Core Concepts

### 1. `calc()`: Dynamic Arithmetic
Evaluates expressions mixing different units:
```css
width: calc(100% - 32px);
```
- **Operators**: Addition (`+`), subtraction (`-`), multiplication (`*`), and division (`/`).
- **The Mandatory Whitespace Rule**: You **MUST** include whitespace around the `+` and `-` operators!
  - `calc(100% - 20px)` &rarr; **VALID**.
  - `calc(100%-20px)` &rarr; **SYNTAX ERROR!** (The parser interprets `-20px` as a negative dimension identifier, not subtraction).

### 2. Extrema Functions: `min()` and `max()`
- **`min(val1, val2, ...)`**: Evaluates all arguments and applies the **smallest** computed value.
  - *Counter-intuitive mental model*: `min()` is typically used to set an **upper ceiling** (maximum width):
    ```css
    /* Never exceed 800px, but shrink on smaller screens */
    width: min(100%, 800px);
    ```
- **`max(val1, val2, ...)`**: Evaluates all arguments and applies the **largest** computed value.
  - *Mental model*: `max()` is used to enforce a **lower floor** (minimum size):
    ```css
    /* Guarantee minimum 16px font size on tiny screens */
    font-size: max(1rem, 2vw);
    ```

### 3. Fluid Clamping: `clamp(MIN, PREFERRED, MAX)`
Constrains a preferred value between an absolute minimum floor and a maximum ceiling:
$$\text{clamp}(\text{MIN}, \text{VAL}, \text{MAX}) \equiv \max(\text{MIN}, \min(\text{VAL}, \text{MAX}))$$
```css
/* Typography scales fluidly with viewport, clamped between 20px and 48px */
font-size: clamp(1.25rem, 3vw + 0.75rem, 3rem);
```

### 4. Advanced Math & Trigonometry (Baseline 2023)
- **`round(strategy, A, B)`**: Rounds value $A$ to the nearest multiple of $B$ (`nearest`, `up`, `down`, `to-zero`).
- **`sin(angle)` / `cos(angle)`**: Returns trigonometric sine and cosine of an angle, enabling pure-CSS circular positioning and orbital animations without JavaScript.
- **`hypot(A, B)`**: Calculates the Euclidean distance $\sqrt{A^2 + B^2}$.

---

## 3. How It Works Internally

### The Fluid Clamp Formula Derivation
How do engineers calculate the mathematical coefficients inside `clamp(min, slope + intercept, max)`?
To make typography scale smoothly from a minimum size ($y_1$) at a minimum viewport ($x_1$) to a maximum size ($y_2$) at a maximum viewport ($x_2$):

```
+--------------------------------------------------------------------+
| FLUID LINEAR INTERPOLATION EQUATION: y = mx + b                    |
+--------------------------------------------------------------------+
  1. Calculate Slope (m):
     m = (y2 - y1) / (x2 - x1)

  2. Convert Slope to Viewport Width (vw):
     slope_vw = m * 100

  3. Calculate Intercept (b):
     b = y1 - (m * x1)

  Example:
  - Min font: 20px (1.25rem) at 400px viewport
  - Max font: 36px (2.25rem) at 1200px viewport
  - Slope: (36 - 20) / (1200 - 400) = 16 / 800 = 0.02 (2vw)
  - Intercept: 20 - (0.02 * 400) = 20 - 8 = 12px (0.75rem)

  Resulting Fluid Formula:
  font-size: clamp(1.25rem, 2vw + 0.75rem, 2.25rem);
```

### Expression Simplification at Parse Time
The browser engine simplifies pure numerical math at parse time:
- `calc(10px + 20px)` is immediately simplified to `30px` during tokenization.
- Complex expressions combining units (like `calc(100% - 2rem)`) are stored as an expression tree node and resolved during the **Used Value** layout phase once the containing block's pixel width is determined.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| - calc(): Universal across all browsers since IE9.                      |
| - min(), max(), clamp(): Widely Available (Baseline 2020).               |
| - sin(), cos(), tan(), round(), pow(): Widely Available (Baseline 2023). |
+-------------------------------------------------------------------------+
```

### Arithmetic and Clamping Syntax
```css
.container {
  /* Dynamic subtraction */
  width: calc(100% - 4rem);

  /* Fluid container width */
  max-width: min(90vw, 1200px);

  /* Fluid padding */
  padding: clamp(1rem, 2vw + 0.5rem, 3rem);
}
```

### Modern Stepped Rounding Syntax
```css
.stepped-grid {
  /* Snap element width to discrete 50px increments */
  width: round(nearest, 32vw, 50px);
}
```

---

## 5. Practical Examples

### Example 1: Basic Responsive Layout with `min()`
Avoid writing separate `@media` queries to constrain container widths:
```css
/* Responsive container: 90% of screen on mobile, capped at 1140px on desktop */
.layout-container {
  width: min(90%, 1140px);
  margin-inline: auto;
}
```

### Example 2: Real-World Fluid Hero Typography with `clamp()`
Create an editorial title that scales seamlessly from a mobile phone to a 4K display:
```css
.hero-heading {
  /* Scales smoothly from 32px (2rem) to 72px (4.5rem) */
  font-size: clamp(2rem, 4vw + 1rem, 4.5rem);
  font-weight: 800;
  line-height: 1.1;
  letter-spacing: -0.02em;
  color: #0f172a;
}
```

### Example 3: Pure CSS Circular Menu Layout with `sin()` and `cos()`
Position 4 icons in a perfect circle around a central hub without JavaScript or manual coordinates:
```html
<nav class="circular-menu">
  <button class="menu-item" style="--angle: 0deg;">1</button>
  <button class="menu-item" style="--angle: 90deg;">2</button>
  <button class="menu-item" style="--angle: 180deg;">3</button>
  <button class="menu-item" style="--angle: 270deg;">4</button>
</nav>
```
```css
.circular-menu {
  position: relative;
  width: 200px;
  height: 200px;
  margin: 4rem auto;
}

.menu-item {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: #0284c7;
  color: white;
  border: none;
  cursor: pointer;

  /* Radius of circle = 80px */
  --radius: 80px;

  /* Trigonometric position math! */
  transform: translate(
    calc(cos(var(--angle)) * var(--radius) - 50%),
    calc(sin(var(--angle)) * var(--radius) - 50%)
  );
}

.menu-item:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

### Example 4: Failure Case — Missing Whitespace in `calc()`
```css
/* Anti-Pattern: FAILS parsing in all browsers! */
.sidebar {
  width: calc(100%-250px); /* Syntax Error! */
}
```
*Failure Analysis*: The CSS parser tokenizes `-250px` as a negative dimension token. It encounters `PERCENT(100)` followed immediately by `DIMENSION(-250px)` without an operator token. The entire declaration is discarded, collapsing the sidebar to zero width.
*Correct Pattern*:
```css
.sidebar {
  width: calc(100% - 250px); /* Explicit spaces around the minus sign */
}
```

### Example 5: Stepped Pixel-Art Scaling with `round()`
Scale an element in strict multiples of an $8\text{px}$ grid:
```css
.pixel-sprite {
  /* Snap width to exact 8px multiples */
  width: round(down, 25vw, 8px);
  aspect-ratio: 1 / 1;
  background-color: #3b82f6;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test clamp computation
const heading = document.querySelector('.hero-heading');
const computed = window.getComputedStyle(heading);
console.log('Fluid Computed Font Size:', computed.fontSize); // Computes to exact px
```

---

## 6. Best Practices

1. **Always Add Whitespace Around `+` and `-` in `calc()`**: Treat spaces around addition and subtraction as mandatory syntax requirements.
2. **Use `clamp()` for Typography Instead of Media Queries**: Replace dozens of discrete font breakpoints with a single fluid `clamp()` formula.
3. **Use `min()` for Maximum Widths**: Write `width: min(100%, 1200px);` instead of `width: 100%; max-width: 1200px;` to reduce code volume.
4. **Use `max()` for Safe Tap Targets**: Ensure touch targets satisfy WCAG $44\text{px}$ minimums: `min-height: max(44px, 2.5rem);`.
5. **Never Use Unbounded Viewport Units (`vw`) for Text**: Never write `font-size: 4vw;` alone! On mobile, text becomes microscopic; on ultra-wide screens, it becomes enormous. Always wrap in `clamp()`.
6. **Nest Math Functions Freely**: You do not need to repeat `calc()` inside `min()`, `max()`, or `clamp()`; math expressions are automatically evaluated inside them.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Missing spaces around `-` in `calc(100%-20px)` | Tokenizer grammar rules | Declaration fails parsing and is completely ignored. | Add whitespace: `calc(100% - 20px)`. |
| Inverting `min()` and `max()` logic | Confusing floor vs ceiling | Layout breaks; container grows infinitely or shrinks to zero. | Remember: `min()` sets upper cap; `max()` sets lower floor. |
| Unclamped `vw` typography (`font-size: 3vw`) | Unbounded scaling | Microscopic text on mobile phones; massive text on 4K TVs. | Wrap in `clamp(1rem, 3vw, 2.5rem)`. |
| Writing redundant `calc()` inside `clamp()` | Outdated CSS habit | Cluttered code: `clamp(1rem, calc(2vw + 1rem), 3rem)`. | Math is native: `clamp(1rem, 2vw + 1rem, 3rem)`. |
| Dividing by a unit (`calc(100px / 2px)`) | Dividing length by length emits a number | Unexpected type output in properties expecting `<length>`. | Divide length by unitless number: `calc(100px / 2)`. |
| Passing degrees without unit into `sin()` | Trigonometric syntax requires angle | Function fails; property ignored. | Always include angle unit: `sin(45deg)`. |

---

## 8. Performance, Accessibility & Browser Support

### Layout Engine Performance
All modern browser engines evaluate CSS math functions in **compiled C++ routines**:
- Expressions like `min()` and `clamp()` evaluate in **sub-nanosecond** speeds during layout calculation.
- Unlike JavaScript-driven resize listeners (`window.onresize`), native CSS `clamp()` does not cause layout thrashing or drop frames during viewport resizing.

### Accessibility Safeguards
- **WCAG 1.4.4 (Resize Text)**: When using `clamp()`, ensure the minimum value allows text to reach at least $200\%$ of default size when a user zooms in their browser settings.
- Avoid using `vw` units alone in `clamp()` without a `rem` base component (e.g., use `clamp(1rem, 2vw + 1rem, 3rem)` rather than `clamp(1rem, 4vw, 3rem)`).

---

## 9. Comparison / When to Use What

| Function | Math Evaluated | Best Use Case | Typical Example |
|:---|:---|:---|:---|
| **`calc()`** | Basic arithmetic (`+ - * /`) | Offsetting fixed widths from percentages | `width: calc(100% - 40px)` |
| **`min()`** | Chooses smallest value | Constraining maximum width | `width: min(90%, 1200px)` |
| **`max()`** | Chooses largest value | Enforcing minimum accessible touch size | `height: max(44px, 2.5rem)` |
| **`clamp()`** | Restricts value between min & max| Fluid typography & responsive padding | `font-size: clamp(1rem, 2vw + 1rem, 2.5rem)`|
| **`round()`** | Snaps to discrete step interval| Pixel-art, strict grid layouts | `width: round(down, 30vw, 16px)` |
| **`sin()` / `cos()`**| Trigonometric coordinates | Circular layouts, orbital animations | `transform: translate(cos(...), sin(...))` |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is whitespace strictly required around the `+` and `-` operators in `calc()`, but optional around `*` and `/`?</strong></summary>

In CSS syntax, the hyphen character (`-`) is used extensively in identifiers (e.g., `font-size`, `--my-var`) and negative numbers (e.g., `-10px`). If you write `calc(100%-20px)`, the tokenizer cannot distinguish between a subtraction operator and an invalid identifier containing a minus sign. To eliminate grammar ambiguity, the W3C specification strictly requires whitespace around `+` and `-`. Because `*` and `/` cannot be part of CSS identifiers, whitespace is technically optional for multiplication and division.
</details>

<details>
<summary><strong>2. Explain why `min()` acts as a maximum constraint and `max()` acts as a minimum constraint.</strong></summary>

While linguistically counter-intuitive, the mathematical logic is straightforward:
- `width: min(100%, 800px)` takes the **smallest** of the two values. When the screen is $1200\text{px}$, $800\text{px}$ is smaller, so width is capped at $800\text{px}$. The element can never exceed $800\text{px}$, effectively functioning as a `max-width`.
- `width: max(300px, 50vw)` takes the **largest** of the two values. When $50\text{vw}$ shrinks below $300\text{px}$, $300\text{px}$ is larger, preventing the element from shrinking any further. It effectively functions as a `min-width`.
</details>

<details>
<summary><strong>3. How does `clamp(MIN, VAL, MAX)` evaluate when the MIN value is greater than the MAX value?</strong></summary>

According to the W3C specification, `clamp(MIN, VAL, MAX)` is formally defined as:
$$\max(\text{MIN}, \min(\text{VAL}, \text{MAX}))$$
If an author mistakenly specifies a `MIN` value that is greater than `MAX` (e.g., `clamp(100px, 50px, 80px)`), the inner `min(50px, 80px)` evaluates to `50px`. Then the outer `max(100px, 50px)` evaluates to **`100px`**. Therefore, if `MIN > MAX`, the `MIN` value always wins.
</details>

<details>
<summary><strong>4. Do you need to wrap arithmetic inside `calc()` when using `min()`, `max()`, or `clamp()`?</strong></summary>

**No.** The W3C CSS Values 4 specification explicitly defines all arguments inside `min()`, `max()`, and `clamp()` as **math expressions**. You can write addition, subtraction, multiplication, and division directly without nesting `calc()` (e.g., `clamp(1rem, 2vw + 0.5rem, 3rem)` is completely valid).
</details>

<details>
<summary><strong>5. What units can be combined inside `calc()`? Can you divide a length by a length?</strong></summary>

You can combine any compatible units whose dimensions resolve to lengths (e.g., `px`, `rem`, `%`, `vw`).
- Multiplying a length by a number yields a length (`10px * 2 = 20px`).
- Dividing a length by a number yields a length (`10px / 2 = 5px`).
- In modern CSS Values 4, dividing a length by another length yields a **unitless number** (`100px / 20px = 5`), which can be used in dimensionless properties like `opacity` or `z-index`.
</details>

<details>
<summary><strong>6. How do the trigonometric functions `sin()` and `cos()` facilitate pure-CSS circular positioning?</strong></summary>

In polar coordinate mathematics, the Cartesian coordinates of any point along a circle's perimeter are defined by:
$$x = r \times \cos(\theta), \quad y = r \times \sin(\theta)$$
By setting an angle custom property `--angle: 45deg;` and radius `--r: 100px;`, CSS transforms can place elements along a circle using:
`transform: translate(calc(cos(var(--angle)) * var(--r)), calc(sin(var(--angle)) * var(--r)));`
This enables radial menus and planetary animations without any JavaScript trigonometry.
</details>

<details>
<summary><strong>7. What are the rounding strategies supported by the `round()` function?</strong></summary>

The `round(strategy, value, step)` function supports four rounding strategies:
1. **`nearest`** (default): Rounds to the closest multiple of step.
2. **`up`** (equivalent to `ceil`): Rounds toward positive infinity.
3. **`down`** (equivalent to `floor`): Rounds toward negative infinity.
4. **`to-zero`** (equivalent to `trunc`): Rounds toward zero, truncating fractions.
</details>

<details>
<summary><strong>8. Why is using `font-size: 5vw` without `clamp()` detrimental to accessibility?</strong></summary>

When a user with visual impairments zooms their browser text using the browser settings, the browser modifies the default root font size (`rem`). If a website uses pure viewport units (`5vw`), the font size is tied exclusively to the monitor width, ignoring the user's zoom level completely! Wrapping the viewport unit in `clamp()` with a `rem` component (`clamp(1rem, 2vw + 0.5rem, 3rem)`) ensures that browser zoom settings successfully scale the text.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Responsive Container with `min()` (Easy)
**Task**: Write the single-line width rule for `.container` that takes $92\%$ of the viewport width on mobile devices, but never expands beyond $1280\text{px}$ on desktop.

<details>
<summary><strong>View Solution</strong></summary>

```css
.container {
  width: min(92%, 1280px);
  margin-inline: auto;
}
```
*Explanation*: `min()` caps the width at $1280\text{px}$ while allowing fluid scaling on smaller viewports.
</details>

---

### Exercise 2: Accessible Touch Target Height with `max()` (Easy)
**Task**: Write the height rule for `.touch-button` ensuring it takes $2.5\text{rem}$ normally, but is guaranteed to never drop below the WCAG-recommended minimum touch target of $44\text{px}$.

<details>
<summary><strong>View Solution</strong></summary>

```css
.touch-button {
  min-height: max(44px, 2.5rem);
}
```
*Explanation*: `max()` enforces a hard minimum floor of $44\text{px}$.
</details>

---

### Exercise 3: Derive Fluid Typography with `clamp()` (Medium)
**Task**: Write a fluid `font-size` declaration using `clamp()` that starts at $1.5\text{rem}$ ($24\text{px}$) at a $400\text{px}$ viewport and scales up to $3\text{rem}$ ($48\text{px}$) at a $1200\text{px}$ viewport.

<details>
<summary><strong>View Solution</strong></summary>

*Mathematical Derivation*:
- $\Delta y = 48 - 24 = 24\text{px}$
- $\Delta x = 1200 - 400 = 800\text{px}$
- $\text{Slope } m = 24 / 800 = 0.03 \implies 3\text{vw}$
- $\text{Intercept } b = 24 - (0.03 \times 400) = 24 - 12 = 12\text{px} \implies 0.75\text{rem}$

```css
.fluid-title {
  font-size: clamp(1.5rem, 3vw + 0.75rem, 3rem);
}
```
*Explanation*: Accurately maps the linear slope between viewport endpoints.
</details>

---

### Exercise 4: Fix Subtraction Syntax Error (Medium)
**Task**: Identify why the following declaration fails to apply in Chrome and Firefox, and write the corrected CSS.
```css
.card {
  padding: calc(2rem-8px);
}
```

<details>
<summary><strong>View Solution</strong></summary>

*Explanation*: Missing whitespace around the `-` operator. The CSS parser treats `-8px` as a negative length token rather than a subtraction operator.

*Corrected Declaration*:
```css
.card {
  padding: calc(2rem - 8px);
}
```
</details>

---

### Exercise 5: Position Element in Circle using Trigonometry (Hard)
**Task**: Position an element `.satellite` along a circle centered in `.orbit-container` at an angle of $60^\circ$ and radius of $120\text{px}$ using pure CSS `sin()` and `cos()`.

<details>
<summary><strong>View Solution</strong></summary>

```css
.orbit-container {
  position: relative;
  width: 300px;
  height: 300px;
}

.satellite {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: #f59e0b;

  --angle: 60deg;
  --radius: 120px;

  transform: translate(
    calc(cos(var(--angle)) * var(--radius) - 50%),
    calc(sin(var(--angle)) * var(--radius) - 50%)
  );
}
```
*Explanation*: Applies polar-to-Cartesian trigonometric positioning.
</details>

---

### Exercise 6: Programmatic CSS Math Evaluation in JavaScript (Hard)
**Task**: Write a JavaScript function `computeCSSMath(expression)` that creates a temporary dummy element, applies a complex CSS math expression (like `clamp(1rem, 5vw, 3rem)`), and returns the resolved pixel value.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Resolves a CSS math expression to actual computed pixels.
 * @param {string} mathExpr - e.g. "clamp(16px, 4vw, 48px)"
 * @returns {number} Pixel value as floating point number
 */
function computeCSSMath(mathExpr) {
  const el = document.createElement('div');
  el.style.position = 'absolute';
  el.style.visibility = 'hidden';
  el.style.width = mathExpr;
  document.body.appendChild(el);

  const computedWidth = parseFloat(window.getComputedStyle(el).width);
  document.body.removeChild(el);

  return computedWidth;
}

console.log('Evaluated Math Width:', computeCSSMath('min(100vw, 500px)'));
```
*Explanation*: Uses the browser's native layout engine to evaluate dynamic math expressions.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS MATH FUNCTIONS MASTER CHEAT SHEET                                                           |
+-------------------+----------------------------+-----------------------+------------------------+
| Function          | Syntax Example             | Mandatory Rule        | Primary Purpose        |
+-------------------+----------------------------+-----------------------+------------------------+
| calc()            | calc(100% - 2rem)          | Spaces around + and - | Dynamic arithmetic     |
| min()             | min(90%, 1200px)           | Chooses smallest      | Maximum width ceiling  |
| max()             | max(44px, 2.5rem)          | Chooses largest       | Minimum floor / a11y   |
| clamp()           | clamp(1rem, 2vw, 3rem)     | Min, Preferred, Max   | Fluid typography/pad   |
| round()           | round(nearest, 25vw, 8px)  | Strategy, Val, Step   | Grid step snapping     |
| sin(), cos()      | cos(45deg) * 100px         | Requires angle units  | Circular coordinates   |
| hypot()           | hypot(30px, 40px)          | Euclidean distance    | Diagonal calculations  |
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [W3C CSS Values and Units Module Level 4: Mathematical Expressions](https://www.w3.org/TR/css-values-4/#calc-notation)
- [MDN Web Docs: CSS Math Functions](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Functions#math_functions)
- [web.dev: Min, max-and clamp: three logical CSS functions to use today](https://web.dev/articles/min-max-clamp)
- [Smashing Magazine: Modern Fluid Typography with clamp()](https://www.smashingmagazine.com/2022/01/modern-fluid-typography-css-clamp/)
- [Bramus: CSS Trigonometric Functions](https://www.bram.us/2023/02/08/css-trigonometric-functions/)

---

[Previous: 11 - Custom Properties (CSS Variables)](11-custom-properties.md) | [Section Index](README.md) | [Next: 13 - Fonts & Text Basics](13-fonts-and-text-basics.md)
