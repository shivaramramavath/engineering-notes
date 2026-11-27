# Syntax, Rules & Declarations

| Metadata | Details |
|:---|:---|
| **Level** | Foundational |
| **Prerequisites** | `00-setup/01-what-is-css-and-the-cascade-idea.md`, `00-setup/02-how-the-browser-renders-css.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal Web Foundation) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Syntax 3 Validator, Playwright css_runner.js, stylelint |

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

### What is a CSS Rule?
A CSS stylesheet is an ordered sequence of **statements**. Statements are categorized into two primary forms:
1. **At-rules**: Directives that begin with the `@` symbol (e.g., `@media`, `@keyframes`, `@layer`, `@supports`, `@property`).
2. **Qualified Rules (Style Rules)**: The primary mechanism of visual styling, comprising a **selector** followed by a **declaration block**.

A declaration block contains one or more **declarations**, where each declaration binds an author-chosen **property** to an evaluated **value**.

```
                  +-------------------- Selector
                  |
             .btn-primary {
Property --->  background-color: #0284c7;  <--- Value
               padding: 8px 16px;
             } \_______________________/
                           |
                           +----------- Declaration Block
```

### Why is CSS Syntax Designed for Resilience?
Unlike programming languages like JavaScript or Python—where a single syntax error halts script execution completely—CSS is engineered around **forward compatibility and fault tolerance**:
- If a browser encounters a property name it does not recognize (e.g., a newly drafted CSS feature), it **silently ignores that specific declaration** and proceeds to parse the rest of the file.
- This design ensures that modern websites can deliver bleeding-edge CSS to capable browsers without breaking visual presentation on older or alternative devices.

---

## 2. Core Concepts

### 1. Anatomy of a Declaration
A declaration is composed of:
- **Property**: An ASCII identifier naming the stylistic attribute (e.g., `margin-top`, `font-size`, `display`).
- **Colon (`:`)**: The delimiter separating property and value.
- **Value**: The component values conforming to the property's formal grammar (e.g., lengths, keywords, color functions).
- **Important Flag (`!important`)**: An optional modifier hoisting the declaration into the Author Important cascade origin.
- **Semicolon (`;`)**: The mandatory terminator separating adjacent declarations.

### 2. Case Sensitivity Rules in CSS
Case sensitivity in CSS is nuanced and source-dependent:
- **Properties & Keywords are Case-Insensitive**: `color: RED;` is functionally identical to `COLOR: red;`.
- **Custom Properties (`--*`) are Strictly Case-Sensitive**: `--brand-color: #0284c7;` and `--Brand-Color: #10b981;` are two entirely distinct variables in memory!
- **Class and ID Selectors are Case-Sensitive**: `.button` will not match `<div class="Button">` in standard HTML documents.
- **Attribute Selectors**: Can be made case-insensitive using the `i` flag: `[data-status="active" i]`.

### 3. The Selector List Invalidation Trap
In standard CSS comma-separated selector lists:
```css
/* DANGEROUS PATTERN */
.card, :unsupported-pseudo-element {
  border-radius: 8px;
}
```
If **any single selector** in a comma-separated list is unrecognized or syntactically invalid to the browser's parser, **the entire rule block is discarded**! This is why modern CSS introduces the forgiving selector `:is()`:
```css
/* FORGIVING PATTERN */
:is(.card, :unsupported-pseudo-element) {
  border-radius: 8px; /* Safe! .card matches even if the pseudo-element fails */
}
```

### 4. Legacy Vendor Prefixes
Historically, experimental features used engine prefixes:
- `-webkit-` (Chrome, Safari, newer Edge, iOS WebKit)
- `-moz-` (Firefox)
- `-ms-` (Internet Explorer, legacy Edge)
- `-o-` (Presto-era Opera)

> [!NOTE]
> Modern browser vendors have officially deprecated vendor prefixes. New CSS specifications now remain behind experimental browser flags until they achieve consensus, shipping directly as un-prefixed standards.

---

## 3. How It Works Internally

### The CSS Syntax 3 Tokenizer State Machine
The browser engine parses CSS using a formal state machine defined in the **W3C CSS Syntax Module Level 3**:

```
+--------------------------------------------------------------------+
| RAW INPUT STREAM: "button { color: red; }"                         |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| TOKENIZER STATE MACHINE                                            |
| Emits:                                                             |
| - IDENT-token ("button")                                           |
| - WHITESPACE-token (" ")                                           |
| - <{-token                                                         |
| - IDENT-token ("color")                                            |
| - COLON-token                                                      |
| - IDENT-token ("red")                                              |
| - SEMICOLON-token                                                  |
| - >}-token                                                         |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| PARSER: Consume a Qualified Rule                                   |
| 1. Collect prelude until <{-token -> Preludes = [ "button" ]       |
| 2. Consume Simple Block -> Declarations:                           |
|    - Property: "color"                                             |
|    - Value: [ IDENT("red") ]                                       |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| ERROR RECOVERY MECHANISM                                           |
| If error encountered:                                              |
| Fast-forward token stream until matching ';' or '}' is found;      |
| Resume normal parsing without throwing an exception.               |
+--------------------------------------------------------------------+
```

1. **Tokenization**: Transforms characters into typed tokens, recognizing strings, identifiers, numbers, functions, and delimiters.
2. **Prelude Consumption**: Everything before the opening `{` is consumed as the selector prelude.
3. **Declaration Parsing**: Within `{ ... }`, tokens are partitioned by colons and semicolons.
4. **Panic Mode Recovery**: If a syntax error occurs (such as an illegal character or missing value), the parser enters "panic mode": it skips tokens until it finds a semicolon (`;`) or closing brace (`}`), discards the malformed declaration, and resumes cleanly on the next token.

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| Basic CSS rule syntax, declaration grammar, and error recovery have      |
| been standardized across all web browsers since CSS Level 1 (1996).     |
+-------------------------------------------------------------------------+
```

### Standard Style Rule Anatomy
```css
/* Selector prelude matches elements */
.alert-banner {
  /* Declarations: property: value; */
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 1rem 1.5rem;
  border-left: 4px solid #ef4444;
  background-color: #fef2f2;
  color: #991b1b;
}
```

### At-Rules Structure
```css
/* Conditional Group Rule */
@media (min-width: 768px) {
  .alert-banner {
    padding: 1.5rem 2rem;
  }
}

/* Definition Rule */
@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}
```

---

## 5. Practical Examples

### Example 1: Basic Syntactic Rule
```css
/* Standard Button Rule */
.button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding: 0.5rem 1rem;
  font-family: inherit;
  font-size: 1rem;
  font-weight: 600;
  line-height: 1.5;
  border: 1px solid transparent;
  border-radius: 0.375rem;
  cursor: pointer;
}
```

### Example 2: Real-World Resilient Fallbacks
Supplying progressive fallbacks for advanced properties:
```css
.hero-gradient {
  /* Fallback 1: Solid color for very old engines */
  background-color: #0284c7;

  /* Fallback 2: Standard sRGB linear gradient */
  background-image: linear-gradient(135deg, #0284c7, #38bdf8);

  /* Modern: Perceptual OKLCH gradient with smooth hue interpolation */
  background-image: linear-gradient(135deg in oklch, oklch(0.6 0.2 240), oklch(0.8 0.15 200));
}
```
*How Engines Parse This*:
- An older engine parses `background-color`, then overwrites `background-image` with the standard sRGB gradient. When it reaches the `in oklch` syntax, it fails parsing and ignores it, safely keeping the sRGB gradient.
- A modern engine parses all three, retaining the final OKLCH gradient.

### Example 3: Edge Case — Case-Sensitivity in Custom Properties
```css
:root {
  --theme-color: #0284c7;
  --Theme-Color: #f59e0b; /* Case difference creates a completely new token! */
}

.box-a {
  background-color: var(--theme-color); /* Resolves to #0284c7 (Blue) */
}

.box-b {
  background-color: var(--Theme-Color); /* Resolves to #f59e0b (Amber) */
}
```
*Verification*: Both variables coexist simultaneously in CSSOM memory because CSS custom property names are case-sensitive identifiers.

### Example 4: Failure Case — Broken Selector List Invalidation
```html
<div class="card">Card One</div>
```
```css
/* Anti-pattern: Old vendor-specific pseudo-element invalidates the entire block! */
.card, ::-webkit-broken-experimental-spec {
  background: #ffffff;
  border: 1px solid #cbd5e1;
  padding: 1.5rem;
}
```
*Failure Analysis*: In Firefox, `::-webkit-broken-experimental-spec` is unrecognized. Instead of styling `.card` and ignoring the unknown pseudo-element, the browser discards the **entire rule**. `.card` remains completely unstyled!
*Production Solution*:
```css
/* Correct: Use :is() for forgiving selector parsing */
:is(.card, ::-webkit-broken-experimental-spec) {
  background: #ffffff;
  border: 1px solid #cbd5e1;
  padding: 1.5rem;
}
```

### Example 5: Accessible Production Rule with Focus-Visible Indicator
```css
/* Interactive button with robust keyboard accessibility */
.action-btn {
  background-color: #0284c7;
  color: #ffffff;
  border: 2px solid transparent;
  padding: 0.5rem 1.25rem;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.15s ease-in-out;
}

.action-btn:hover {
  background-color: #0369a1;
}

/* Explicit high-contrast focus indicator for keyboard users */
.action-btn:focus-visible {
  outline: 3px solid #0284c7;
  outline-offset: 3px;
}
```
*Playwright Test Runner Verification*:
```javascript
// Test computed styles
const btn = document.querySelector('.action-btn');
const computed = window.getComputedStyle(btn);
console.log('Padding:', computed.padding); // "8px 20px"
console.log('Border Radius:', computed.borderRadius); // "4px"
```

---

## 6. Best Practices

1. **Always Terminate Declarations with Semicolons**: Although the CSS specification allows omitting the semicolon on the final declaration before `}`, omitting it causes syntax breakage whenever another rule is appended later.
2. **Keep Custom Properties Strictly Lowercase**: To avoid catastrophic case-sensitivity bugs, enforce lowercase kebab-case for all CSS custom properties (`--primary-color`, never `--PrimaryColor`).
3. **Never Chain Vendor Pseudo-Elements in Comma Lists**: Always isolate experimental or browser-specific selectors into separate rules or wrap them inside `:is()`.
4. **Order Declarations with Fallbacks First**: Declare generic fallbacks immediately prior to modern functional variants (e.g., `background-color: #2563eb;` followed by `background-color: oklch(...);`).
5. **Use Comments for Complex Math or Z-Index Stacks**: Document why unusual values exist (e.g., `/* z-index: 50 -> must sit above fixed navigation (z: 40) */`).
6. **Enforce Formatting Consistency via Stylelint**: Prevent missing semicolons, malformed properties, and formatting deviations in automated CI checks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Missing semicolon on declaration line | Typo / carelessness | The subsequent declaration is treated as part of the previous value and ignored. | Terminate every declaration with a `;`. |
| Comma-separated selector list with invalid pseudo-element | Selector parsing rules | The entire rule block is rejected; all matching elements lose styling. | Wrap selectors inside `:is()`, or author separate rules. |
| Mismatched custom property casing | Assuming CSS is universally case-insensitive | `var(--textColor)` returns empty fallback because `--textcolor` was defined. | Standardize on lowercase kebab-case for custom properties. |
| Using CSS comments with `//` | Familiarity with JavaScript / SCSS | Parser error; entire subsequent declarations may be discarded. | Always use block comment syntax: `/* ... */`. |
| Writing units with a space between number and unit | Typing `10 px` instead of `10px` | Parser interprets `10` as a bare number and `px` as an identifier; value is invalid. | Never insert whitespace between number and unit (`10px`). |
| Forgetting curly braces around at-rules | Syntax confusion | Malformed stylesheet; subsequent rules fail to parse. | Ensure all `@media`, `@supports`, and `@layer` blocks are balanced. |

---

## 8. Performance, Accessibility & Browser Support

### Parser Performance
Modern browser tokenizers (such as Blink's `CSSTokenizer`) are implemented in highly optimized C++ with SIMD optimizations. A typical 200 KB CSS file is tokenized and parsed into CSSOM rules in **under 2 milliseconds** on modern devices.

### Accessibility Safeguards
Syntactic errors that invalidate a declaration can directly harm accessibility:
- If an invalid color syntax causes `color: invalid-token;` to fail while `background-color: #000;` succeeds, text may inherit white from a parent, creating an invisible white-on-white accessibility violation.
- Always verify computed contrast in DevTools when using cutting-edge color syntax.

---

## 9. Comparison / When to Use What

| Statement Type | Syntax Example | Purpose | Scope |
|:---|:---|:---|:---|
| **Style Rule** | `.header { color: red; }` | Applies visual properties to matching DOM nodes | Document or Scoped Subtree |
| **Conditional At-Rule** | `@media (...) { ... }` | Evaluates device, screen, or capability conditions | Nested rules within block |
| **Definition At-Rule** | `@keyframes spin { ... }` | Defines reusable animation or font descriptors | Global across document |
| **Architecture At-Rule** | `@layer framework, app;` | Declares cascade layer priority order | Global across document |
| **Typing At-Rule** | `@property --accent { ... }` | Declares data type, inheritance, and initial value | Global CSSOM register |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain CSS parser error recovery and why it is fundamental to the web platform.</strong></summary>

CSS error recovery is governed by the principle of **graceful degradation and forward compatibility**. When a CSS parser encounters an unrecognized token, invalid property name, or malformed value, it does not throw an exception or halt rendering. Instead, it enters a recovery mode: it discards tokens until it reaches the next statement delimiter (`;` or `}`), discarding only the malformed declaration. This allows browsers to add new CSS capabilities without breaking existing sites in older browsers.
</details>

<details>
<summary><strong>2. Why are CSS custom properties case-sensitive when standard CSS properties are case-insensitive?</strong></summary>

Standard CSS properties (`font-size`, `display`) and CSS keyword values (`flex`, `block`, `red`) are defined by the W3C specification and belong to the browser's fixed dictionary; the spec authors chose ASCII case-insensitivity for developer convenience. In contrast, CSS Custom Properties (`--my-var`) are **user-defined identifiers**. To maximize developer flexibility and maintain parity with programming language variables, the CSSWG explicitly specified that custom property names are case-sensitive.
</details>

<details>
<summary><strong>3. What happens when an invalid selector is placed inside a comma-separated selector list, and how does `:is()` solve it?</strong></summary>

According to the CSS Selectors specification, a comma-separated list of selectors is evaluated as a single unit during parsing. If any single selector in the list is invalid or unsupported, **the entire selector list is declared invalid**, causing the browser to discard the entire style rule. The `:is()` and `:where()` pseudo-classes are explicitly defined as **forgiving selector lists**: the parser evaluates each argument independently, discarding invalid selectors while preserving valid ones.
</details>

<details>
<summary><strong>4. Why is `// single line comments` invalid in native CSS?</strong></summary>

The W3C CSS specification only standardizes block comments (`/* comment */`). In native CSS, `//` is not a comment token. If you write `// color: red;`, the parser treats the two forward slashes as part of the selector or property name, resulting in a syntax error that can cause the parser to drop adjacent declarations.
</details>

<details>
<summary><strong>5. What is the syntactic difference between an At-rule and a Qualified rule?</strong></summary>

An **At-rule** begins with the "commercial at" character (`@`) followed by an identifier keyword (e.g., `@media`, `@keyframes`, `@layer`). It may end with a semicolon (like `@import url("...");`) or a block enclosed in curly braces. A **Qualified rule** (commonly called a style rule) begins with a prelude (a selector list) and always ends with a simple block enclosed in curly braces containing declarations.
</details>

<details>
<summary><strong>6. Explain how the browser parses a declaration where a value is separated from its unit by whitespace (e.g., `margin: 10 px`).</strong></summary>

The CSS tokenizer specifies that a dimension token (like `<length>`) must consist of a `<number-token>` immediately followed by a `<ident-token>` with **zero whitespace between them**. If whitespace is inserted (`10 px`), the tokenizer emits two separate tokens: `NUMBER(10)` and `IDENT("px")`. Because the `margin` property grammar expects a single `<length>`, this sequence fails grammar validation, and the entire declaration is discarded.
</details>

<details>
<summary><strong>7. How do modern browsers handle vendor prefixes like `-webkit-`?</strong></summary>

Historically, vendors created proprietary prefixes for unfinalized specs. However, developers deployed code using only `-webkit-` prefixes, which broke websites on Firefox and Edge. Today, browser engines implement a standard set of "compat" `-webkit-` properties (e.g., `-webkit-user-select`, `-webkit-appearance`) purely for legacy web compatibility. For new CSS features, browser engines no longer introduce prefixes; features are developed behind experimental flags.
</details>

<details>
<summary><strong>8. What is the difference between an empty declaration block and a missing declaration block?</strong></summary>

An empty declaration block (`.card {}`) is valid CSS syntax: the parser consumes the prelude, creates a rule node in the CSSOM with an empty style declaration list, and continues. A missing declaration block (`.card;` or `.card`) is a syntax error: the parser treats it as an invalid statement prelude without a block, discarding it during error recovery.
</details>

---

## 11. Practice Exercises

### Exercise 1: Identify and Fix Syntax Errors (Easy)
**Task**: Identify the three syntax errors in the following CSS snippet and correct them.
```css
.card {
  background color: #ffffff;
  padding: 16 px;
  color: #1e293b
  border-radius: 8px;
}
```

<details>
<summary><strong>View Solution</strong></summary>

```css
.card {
  background-color: #ffffff; /* Error 1: Property was missing a hyphen */
  padding: 16px;             /* Error 2: Whitespace between number and unit */
  color: #1e293b;            /* Error 3: Missing semicolon on color declaration */
  border-radius: 8px;
}
```
*Explanation*: Properties are hyphenated (`background-color`), dimensions cannot have whitespace (`16px`), and statements must terminate with a semicolon.
</details>

---

### Exercise 2: Prevent Selector List Invalidation (Easy)
**Task**: Refactor the rule below so that `.button` still renders correctly in browsers that do not recognize `:something-unsupported`.
```css
.button, :something-unsupported {
  background-color: #3b82f6;
  color: white;
}
```

<details>
<summary><strong>View Solution</strong></summary>

```css
/* Option A: Using modern forgiving selector list */
:is(.button, :something-unsupported) {
  background-color: #3b82f6;
  color: white;
}

/* Option B: Separate rules */
.button {
  background-color: #3b82f6;
  color: white;
}

:something-unsupported {
  background-color: #3b82f6;
  color: white;
}
```
*Explanation*: `:is()` forgives invalid selectors within its argument list, allowing `.button` to apply regardless of engine support for the second selector.
</details>

---

### Exercise 3: Case-Sensitive Custom Property Debugging (Medium)
**Task**: An engineer complains that their custom property isn't working:
```css
:root {
  --primaryColor: #10b981;
}

.badge {
  background-color: var(--primarycolor, #64748b);
}
```
What is the computed background color of `.badge`? Fix the bug.

<details>
<summary><strong>View Solution</strong></summary>

*Computed Color*: `#64748b` (the fallback gray color), because custom properties are case-sensitive and `--primarycolor` does not exist.

*Fix*:
```css
:root {
  --primary-color: #10b981; /* Adopt consistent lowercase kebab-case */
}

.badge {
  background-color: var(--primary-color, #64748b);
}
```
*Explanation*: Standardizing on lowercase kebab-case eliminates casing mismatch bugs.
</details>

---

### Exercise 4: Constructing Progressive Gradient Fallbacks (Medium)
**Task**: Write a CSS class `.surface` that defines a solid fallback color `#3b82f6`, an sRGB linear gradient fallback, and an advanced OKLCH gradient with perceptual interpolation.

<details>
<summary><strong>View Solution</strong></summary>

```css
.surface {
  /* 1. Solid fallback */
  background-color: #3b82f6;

  /* 2. Standard linear gradient */
  background-image: linear-gradient(to right, #3b82f6, #60a5fa);

  /* 3. Modern OKLCH interpolation */
  background-image: linear-gradient(to right in oklch, oklch(0.6 0.2 250), oklch(0.75 0.15 230));
}
```
*Explanation*: Declaring properties sequentially allows the CSS parser's error recovery mechanism to retain the highest-fidelity gradient supported by the client engine.
</details>

---

### Exercise 5: Case-Insensitive Attribute Selectors (Hard)
**Task**: An HTML document contains legacy markup where data attributes have mixed casing: `<input data-role="Admin">` and `<input data-role="admin">`. Write a single CSS selector that styles both inputs identically without editing HTML.

<details>
<summary><strong>View Solution</strong></summary>

```css
input[data-role="admin" i] {
  border: 2px solid #ef4444;
  background-color: #fef2f2;
}
```
*Explanation*: Appending the `i` (or `I`) flag before the closing bracket of an attribute selector forces the browser engine to perform case-insensitive ASCII matching on the attribute value.
</details>

---

### Exercise 6: Parse State Inspection via JavaScript (Hard)
**Task**: Write a JavaScript function `isValidDeclaration(property, value)` that uses the browser's native parser to test whether a given CSS property and value are syntactically valid without applying them to any real DOM element.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Tests whether a CSS property and value are valid according to the browser's parser.
 * @param {string} property
 * @param {string} value
 * @returns {boolean}
 */
function isValidDeclaration(property, value) {
  if (typeof CSS !== 'undefined' && typeof CSS.supports === 'function') {
    return CSS.supports(property, value);
  }
  
  // DOM fallback
  const element = document.createElement('div');
  element.style[property] = value;
  return element.style[property] !== '';
}

// Tests:
console.log(isValidDeclaration('padding', '10px'));      // true
console.log(isValidDeclaration('padding', '10 px'));     // false (invalid unit spacing)
console.log(isValidDeclaration('color', 'oklch(0.6 0.2 150)')); // true in modern engines
```
*Explanation*: `CSS.supports()` interfaces directly with the browser's internal CSS parser to test validity.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CSS SYNTAX & DECLARATIONS CHEAT SHEET                                                           |
+--------------------------+-------------------------------------+--------------------------------+
| Element                  | Syntax Example                      | Core Rule / Behavior           |
+--------------------------+-------------------------------------+--------------------------------+
| Style Rule               | .selector { prop: val; }            | Qualified rule with block      |
| Declaration              | color: #0284c7;                     | Must end with semicolon        |
| Custom Property          | --token-name: 1rem;                 | Strictly CASE-SENSITIVE        |
| Standard Properties      | COLOR: RED;                         | ASCII Case-Insensitive         |
| Unit Spacing             | 16px (Valid), 16 px (INVALID)       | Zero space between num & unit  |
| Selector Invalidation    | .a, :invalid { }                    | Whole list drops if 1 fails    |
| Forgiving Selector       | :is(.a, :invalid) { }               | Preserves valid selectors      |
| Attribute Case Flag      | [data-status="active" i]            | Forces case-insensitive match  |
+--------------------------+-------------------------------------+--------------------------------+
```

---

## 13. Further Reading

- [W3C CSS Syntax Module Level 3](https://www.w3.org/TR/css-syntax-3/)
- [MDN Web Docs: CSS Syntax](https://developer.mozilla.org/en-US/docs/Web/CSS/Syntax)
- [MDN Web Docs: CSS Error Handling](https://developer.mozilla.org/en-US/docs/Web/CSS/Error_handling)
- [W3C CSS Cascading and Inheritance Level 5](https://www.w3.org/TR/css-cascade-5/)
- [web.dev: Learn CSS Syntax](https://web.dev/learn/css/syntax)

---

[Previous: Module 00 Setup](../00-setup/06-css-modules-levels-and-baseline.md) | [Section Index](README.md) | [Next: 02 - Selectors](02-selectors.md)
