# Cascade Layers (`@layer`)

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/05-specificity.md`, `01-fundamentals/06-cascade-origins-importance-and-order.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Baseline 2022 - Chrome 99, Firefox 97, Safari 15.4, Edge 99) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | W3C CSS Cascading & Inheritance Level 5, Playwright css_runner.js, stylelint |

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

### What are Cascade Layers?
**Cascade Layers (`@layer`)** are an official CSS at-rule standardized in CSS Cascading and Inheritance Level 5. They allow developers to structure stylesheets into explicit, ordered tiers of priority (such as `reset`, `base`, `components`, and `utilities`).

Within the Cascade algorithm, **Layer Precedence is evaluated BEFORE Selector Specificity**. This means a selector in a higher-priority layer will always override a selector in a lower-priority layer, **regardless of how high the lower-layer selector's specificity is**.

### Why Were Cascade Layers Created?
Historically, integrating third-party component libraries (like Bootstrap, Tailwind, or date-pickers) caused immense specificity headaches:
- If a vendor library styled buttons with `#widget-root button.primary-btn` (`[1, 1, 1]`), overriding that button in author styles required matching or escalating that specificity (`body #widget-root button.primary-btn` or `!important`).
- Developers engaged in endless specificity escalation.
- With `@layer`, you place the entire vendor library into `@layer vendor` and your styles into `@layer app`. Your simple `.btn` (`[0, 1, 0]`) now overrides the vendor's `#id` selector effortlessly!

---

## 2. Core Concepts

### 1. Declaring Layer Order
Layers are ordered based on the **first time their names appear** in stylesheet execution. The industry-standard pattern is to declare the complete layer sequence at the very top of your root CSS file:
```css
/* Establish explicit priority order from lowest to highest */
@layer reset, base, vendor, components, utilities;
```
In normal cascade resolution:
$$\text{reset} < \text{base} < \text{vendor} < \text{components} < \text{utilities} < \text{Unlayered Styles}$$

### 2. Block Syntax & Anonymous Layers
You can declare styles within layers using block notation:
```css
@layer components {
  .card {
    padding: 1.5rem;
    border-radius: 8px;
  }
}

/* Anonymous layer: Cannot be appended to later; assigned priority at definition */
@layer {
  .temporary-fix { opacity: 0.9; }
}
```

### 3. Nested Layers
Layers can be nested to create hierarchical namespaces:
```css
@layer framework {
  @layer buttons {
    .btn { padding: 8px 16px; }
  }
}

/* Equivalent dot-notation syntax */
@layer framework.buttons {
  .btn { border-radius: 4px; }
}
```

### 4. Importing into Layers
External stylesheets can be imported directly into a specific layer:
```css
/* Load third-party CSS into the vendor layer */
@import url("normalize.css") layer(reset);
@import url("leaflet.css") layer(vendor);
```
HTML `<link>` tags also support layer assignment:
```html
<link rel="stylesheet" href="bootstrap.css" layer="vendor">
```

### 5. The `revert-layer` Keyword
The `revert-layer` keyword rolls back the property's computed value to the value it would have held in the **previous cascade layer**:
```css
@layer base {
  button { background-color: #e2e8f0; }
}

@layer components {
  button.cancel {
    /* Discards component-layer styling and reverts to base layer */
    background-color: revert-layer;
  }
}
```

---

## 3. How It Works Internally

### The Layer Hierarchy & Inversion Matrix
The browser engine maps rules into discrete layer buckets inside the Author origin:

```
+--------------------------------------------------------------------+
| NORMAL CASCADE ORIGIN (Unflagged Styles)                          |
| HIGHEST: Unlayered Author Styles (Styles outside any @layer)       |
|          ^                                                         |
|          |-- @layer utilities (Later layer)                        |
|          |-- @layer components                                     |
|          |-- @layer vendor                                         |
|          |-- @layer base                                           |
| LOWEST:  \-- @layer reset     (Earlier layer)                      |
+--------------------------------------------------------------------+

                          VS

+--------------------------------------------------------------------+
| IMPORTANT CASCADE ORIGIN (!important Styles)                      |
| HIGHEST: @layer reset !important     (INVERTED: Earlier layer!)    |
|          ^                                                         |
|          |-- @layer base !important                                |
|          |-- @layer vendor !important                              |
|          |-- @layer components !important                          |
|          |-- @layer utilities !important                           |
| LOWEST:  \-- Unlayered Author !important (Inverted to the bottom!)  |
+--------------------------------------------------------------------+
```

### Why Does `!important` Invert Layer Order?
This intentional inversion protects design system boundaries:
- In normal styling, you want utilities and application code to override resets and framework defaults.
- But if a core design system layer declares an essential safety constraint with `!important` (e.g., in `@layer reset { [hidden] { display: none !important; } }`), that declaration **cannot be broken** by a careless `!important` declaration in a downstream utility layer!

---

## 4. Syntax & Usage

```
+-------------------------------------------------------------------------+
| Web Platform Baseline: Widely Available                                  |
| @layer, layer(), <link layer>, and revert-layer were shipped across      |
| all major browser engines in early 2022 (Chrome 99, FF 97, Safari 15.4).|
+-------------------------------------------------------------------------+
```

### Establishing Layer Architecture
```css
/* 1. Define explicit hierarchy at the entry point */
@layer reset, tokens, components, utilities;

/* 2. Assign rules to layers anywhere in code */
@layer reset {
  *, *::before, *::after {
    box-sizing: border-box;
    margin: 0;
  }
}

@layer tokens {
  :root {
    --brand-primary: #0284c7;
    --text-main: #0f172a;
  }
}

@layer components {
  .btn {
    background-color: var(--brand-primary);
    color: #ffffff;
    padding: 0.5rem 1rem;
    border-radius: 4px;
    border: none;
    cursor: pointer;
  }
}

@layer utilities {
  .text-center { text-align: center; }
  .p-4 { padding: 1rem; }
}
```

---

## 5. Practical Examples

### Example 1: Taming High-Specificity Vendor Styles
```html
<button id="vendor-submit-btn" class="btn">Click Me</button>
```
```css
/* Establish layer order: vendor sits lower than app */
@layer vendor, app;

/* Inside third-party vendor CSS (high specificity!) */
@layer vendor {
  #vendor-submit-btn {
    background-color: #ef4444; /* Specificity [1, 0, 0] */
    padding: 12px 24px;
  }
}

/* Inside your application CSS (low specificity!) */
@layer app {
  .btn {
    background-color: #0284c7; /* Specificity [0, 1, 0] */
  }
}
```
*Result*: The button background is **`#0284c7` (Blue)**!
*Why?*: `@layer app` has higher layer precedence than `@layer vendor`. The browser resolves the layer winner first, completely ignoring the fact that the vendor used an ID selector.

### Example 2: Real-World Multi-Layer Component Design
```css
@layer reset, base, components, overrides;

@layer base {
  body {
    font-family: system-ui, sans-serif;
    color: #1e293b;
    line-height: 1.5;
  }

  a {
    color: #2563eb;
    text-decoration: underline;
  }
}

@layer components {
  .nav-link {
    text-decoration: none;
    font-weight: 600;
  }
}

@layer overrides {
  /* Safely override navigation link for special promotions */
  .promo-link {
    color: #d97706;
  }
}
```

### Example 3: Edge Case — Rolling Back with `revert-layer`
```html
<button class="btn btn-ghost">Ghost Button</button>
```
```css
@layer base, components;

@layer base {
  button {
    background-color: transparent;
    border: 1px solid #cbd5e1;
    color: #0f172a;
  }
}

@layer components {
  .btn {
    background-color: #0284c7;
    color: #ffffff;
    border: 1px solid transparent;
  }

  /* Discard component styling and revert back to base button styling */
  .btn-ghost {
    background-color: revert-layer;
    color: revert-layer;
    border: revert-layer;
  }
}
```
*Verification*: `.btn-ghost` inherits transparent background and the slate border from the `base` layer without re-declaring them.

### Example 4: Failure Case — Unlayered Styles Overriding Layered Architecture
```css
@layer components {
  /* High specificity button inside components layer */
  button#primary-checkout-btn.active {
    background-color: #0284c7; /* Specificity: [1, 1, 1] */
  }
}

/* Rogue unlayered style outside any @layer block */
button {
  background-color: #dc2626; /* Specificity: [0, 0, 1] */
}
```
*Failure Analysis*: The button turns **`#dc2626` (Red)**!
*Why?*: In the W3C Cascade, **unlayered styles sit above all layered styles in the normal origin**. Even a bare tag selector `button` will override an ID-qualified selector inside `@layer components`.
*Remedy*: Always wrap application styles inside defined layers.

### Example 5: Accessible Reset with Protected `[hidden]` Attribute
```css
@layer reset, components;

@layer reset {
  /* Ensure hidden elements are NEVER displayed, even if a component declares display: flex */
  [hidden] {
    display: none !important;
  }
}

@layer components {
  .modal-dialog {
    display: flex; /* Normal style cannot override [hidden] */
  }

  .banner {
    display: block !important; /* Inverted: reset !important beats components !important! */
  }
}
```
*Result*: `<div class="banner" hidden></div>` remains completely hidden, guaranteeing accessibility compliance.

---

## 6. Best Practices

1. **Declare Layer Order Upfront**: Always define your complete `@layer` list at the top of your main stylesheet entry file (`@layer reset, base, components, utilities;`).
2. **Put Third-Party Vendor CSS into Lower Layers**: Wrap imported vendor stylesheets (e.g., Bootstrap, Normalize) in a low-priority `vendor` or `reset` layer.
3. **Avoid Unlayered Author Styles in Mature Codebases**: Once you adopt `@layer`, place all application code inside layers so unlayered styles do not accidentally override your architecture.
4. **Never Use `!important` to Fight Layer Precedence**: If a style inside a lower layer isn't applying, move it to an appropriate higher layer rather than tacking on `!important`.
5. **Use `revert-layer` for Variant Fallbacks**: Instead of overriding properties with `initial` or manual values, use `revert-layer` to step back cleanly through the layer stack.
6. **Inspect Layers in DevTools**: Check Chrome/Firefox DevTools Styles pane; each rule is decorated with its containing `@layer` badge to trace precedence.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Mixing unlayered styles with layered styles | Unaware of unlayered precedence | A low-specificity unlayered rule overrides high-specificity layered rules. | Wrap all application styles inside `@layer`. |
| Adding `!important` inside an early layer | Unaware of layer importance inversion | The early layer's `!important` rule overrides later layers and cannot be defeated. | Keep early layers free of `!important`. |
| Splitting layer declarations across files without master order | Relying on file download order | Layers reorder unpredictably depending on network load timing. | Define `@layer name1, name2;` in the entry file `<head>`. |
| Writing duplicate layer names with different orders | Typo in configuration | The first encountered `@layer` statement permanently establishes order. | Maintain a single canonical `@layer` statement. |
| Expecting higher specificity to beat a higher layer | Specificity vs Layer confusion | An ID selector in `@layer base` fails to override a class in `@layer components`. | Move rule to higher layer or adjust layer order. |
| Using `revert` instead of `revert-layer` | Keyword misunderstanding | `revert` rolls back to User Agent origin; `revert-layer` rolls back to previous layer. | Use `revert-layer` for inter-layer rollbacks. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Engine Architecture
Inside Chromium's Blink and WebKit's style engines:
- Each cascade layer is assigned an integer index during stylesheet parsing.
- When resolving matching rules, the engine performs a simple integer comparison of layer indices (`ruleA.layerIndex > ruleB.layerIndex`).
- Because layers simplify cascade sorting, using `@layer` actually **improves CSSOM matching performance** by allowing the engine to short-circuit specificity calculations for rules in mismatched layers.

### Accessibility Safeguards
- Protect critical accessibility states (like `[hidden]`, `[aria-hidden="true"]`, and `:focus-visible`) by placing them in early layers with `!important` or high-priority utility layers.

---

## 9. Comparison / When to Use What

| Feature | Primary Purpose | How Order is Determined | Specificity Impact |
|:---|:---|:---|:---|
| **Cascade Layers (`@layer`)** | Taming specificity across code modules | First declaration statement | Layer precedence beats specificity |
| **Specificity (`[A, B, C]`)** | Resolving conflicts within the same layer | Selector token count | Lexicographical compare |
| **Source Order** | Final tie-breaker for identical specificity | Position in stylesheet text | Later declaration wins |
| **Cascade Origins** | Resolving User Agent vs User vs Author | Browser architecture | Dictated by W3C specification |
| **`revert-layer`** | Rolling back values to previous layer | Traverses layer stack down | Re-evaluates lower layer |

---

## 10. Interview Questions

<details>
<summary><strong>1. What happens when a declaration in `@layer base` has an ID selector `#header`, and a declaration in `@layer components` has a class selector `.header`?</strong></summary>

The declaration in **`@layer components` wins**. In normal cascade resolution, layer precedence is evaluated *before* selector specificity. Because `@layer components` sits higher in the layer order than `@layer base`, the browser chooses the winner from `components` and never compares the specificity of the two selectors.
</details>

<details>
<summary><strong>2. How do unlayered styles interact with layered styles in normal cascade origin?</strong></summary>

In normal (unflagged) declarations, **unlayered styles always beat layered styles**. The W3C Cascade specification places unlayered author styles at the very top of the author normal origin. This ensures that legacy stylesheets and quick overrides written outside `@layer` continue to work without breaking.
</details>

<details>
<summary><strong>3. Explain why `!important` declarations invert the priority order of Cascade Layers.</strong></summary>

In normal cascade resolution, later layers override earlier layers (`base < components < utilities`). However, for declarations marked `!important`, **earlier layers override later layers** (`utilities !important < components !important < base !important`). This was designed to protect foundational design contracts: if a base reset layer specifies `[hidden] { display: none !important; }`, it cannot be overridden by an accidental `!important` declaration in a downstream utility or component layer.
</details>

<details>
<summary><strong>4. How do you import an external stylesheet into a specific cascade layer using `@import`?</strong></summary>

You append the `layer(layer-name)` functional notation to the `@import` statement:
```css
@import url("sanitize.css") layer(reset);
@import url("bootstrap.css") layer(framework);
```
Alternatively, in HTML, you can write:
```html
<link rel="stylesheet" href="framework.css" layer="framework">
```
</details>

<details>
<summary><strong>5. What is the difference between `revert` and `revert-layer`?</strong></summary>

- **`revert`**: Rolls back the property to the value it would have had in the previous **Cascade Origin** (e.g., rolling back an Author style to the User or User Agent default stylesheet).
- **`revert-layer`**: Rolls back the property to the value it would have had in the previous **Cascade Layer within the same origin**. If no previous layer styled the property, it rolls back to the previous origin.
</details>

<details>
<summary><strong>6. What is an anonymous layer, and when is it useful?</strong></summary>

An anonymous layer is created by declaring `@layer { ... }` without specifying a name. It establishes an isolated layer that cannot be referenced or appended to from other stylesheets. It is useful for third-party scripts or standalone widgets that want to isolate their styles without polluting the global layer namespace.
</details>

<details>
<summary><strong>7. How do nested layers work in Cascade Layer syntax?</strong></summary>

Nested layers allow hierarchical namespacing. They can be authored by nesting `@layer` blocks inside one another, or by using dot notation:
```css
@layer framework.components {
  .btn { border-radius: 4px; }
}
```
The internal priority of nested layers is evaluated within the context of their parent layer.
</details>

<details>
<summary><strong>8. Can you reorder cascade layers after they have been initially defined?</strong></summary>

**No.** The priority order of cascade layers is determined permanently by the **first statement** that mentions them. Any subsequent `@layer` statements merely append rules to the already-ordered layers. For example, if you declare `@layer a, b;`, writing `@layer b, a;` later in the file has zero effect on layer priority.
</details>

---

## 11. Practice Exercises

### Exercise 1: Establish Standard Layer Architecture (Easy)
**Task**: Write the single-line CSS statement that establishes an explicit priority hierarchy with the following layers: `reset`, `tokens`, `vendor`, `components`, and `utilities`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@layer reset, tokens, vendor, components, utilities;
```
*Explanation*: The order of comma-separated identifiers in the initial `@layer` statement establishes the ascending priority order.
</details>

---

### Exercise 2: Override Vendor CSS Without Specificity Hacks (Easy)
**Task**: A third-party stylesheet inside `@layer vendor` styles alert messages with `.alert.alert-danger[role="alert"]` (color: red). In `@layer app`, write a clean rule that changes the text color to `#dc2626` using only a single class name.

<details>
<summary><strong>View Solution</strong></summary>

```css
@layer vendor, app;

/* In your app stylesheet */
@layer app {
  .alert {
    color: #dc2626;
  }
}
```
*Explanation*: Because `@layer app` outranks `@layer vendor`, the single class `.alert` beats the vendor's high-specificity selector without requiring ID chaining or `!important`.
</details>

---

### Exercise 3: Isolate CSS Reset with Low Priority (Medium)
**Task**: Write a CSS snippet that loads `reset.css` into a `reset` layer and styles `h1` headings with `font-size: 2.5rem` inside a `typography` layer. Confirm that `h1` gets 2.5rem even if `reset.css` has `h1 { font-size: 1rem; }`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@layer reset, typography;

@import url("reset.css") layer(reset);

@layer typography {
  h1 {
    font-size: 2.5rem;
  }
}
```
*Explanation*: Because `typography` sits above `reset` in the layer definition, its `2.5rem` declaration wins over any `h1` rules declared inside `reset.css`.
</details>

---

### Exercise 4: Roll Back Style to Base Layer (Medium)
**Task**: A component library defines `.btn` in `@layer components`. For `.btn--plain`, discard the background color and border styles declared in `components`, reverting them to the base browser/theme styles in `@layer base`.

<details>
<summary><strong>View Solution</strong></summary>

```css
@layer base, components;

@layer base {
  button {
    background-color: transparent;
    border: 1px solid #94a3b8;
  }
}

@layer components {
  .btn {
    background-color: #0284c7;
    border: 1px solid transparent;
  }

  .btn--plain {
    background-color: revert-layer;
    border: revert-layer;
  }
}
```
*Explanation*: `revert-layer` steps down the layer stack to retrieve the values declared in `@layer base`.
</details>

---

### Exercise 5: Predict the Outcome of Layered `!important` (Hard)
**Task**: What color is rendered for `<div class="box">`? Explain why.
```css
@layer alpha, beta;

@layer alpha {
  .box {
    background-color: red !important;
  }
}

@layer beta {
  .box {
    background-color: blue !important;
  }
}
```

<details>
<summary><strong>View Solution</strong></summary>

*Winner*: **`red`** (from `@layer alpha`).
*Explanation*: For `!important` declarations, layer precedence inverts: earlier layers defeat later layers (`alpha !important > beta !important`). Because `alpha` was declared first in `@layer alpha, beta;`, its `!important` declaration wins over `beta`.
</details>

---

### Exercise 6: Inspect Active Layer Support in JavaScript (Hard)
**Task**: Write a JavaScript function `isCascadeLayersSupported()` that checks whether the client's browser engine natively supports CSS Cascade Layers.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
/**
 * Detects native browser engine support for CSS Cascade Layers.
 * @returns {boolean}
 */
function isCascadeLayersSupported() {
  if (typeof window === 'undefined') return false;
  return (
    'CSSLayerBlockRule' in window ||
    (window.CSS && CSS.supports('@layer { * { color: red; } }'))
  );
}

console.log('Cascade Layers Supported:', isCascadeLayersSupported()); // true in modern engines
```
*Explanation*: Modern browsers that support Cascade Layers expose the `CSSLayerBlockRule` interface on the global `window` object.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| CASCADE LAYERS (@layer) MASTER CHEAT SHEET                                                      |
+--------------------------+-----------------------------+--------------------+-------------------+
| Syntax                   | Example                     | Purpose            | Precedence Rule   |
+--------------------------+-----------------------------+--------------------+-------------------+
| Definition Statement     | @layer reset, app;          | Set order upfront  | First one wins    |
| Block Rule               | @layer app { .card { } }    | Encapsulate styles | Later layer wins  |
| Anonymous Layer          | @layer { .fix { } }         | One-off isolation  | Defined at point  |
| Nested Layer             | @layer app.components { }   | Namespaced layers  | Sub-layer order   |
| Import into Layer        | @import url() layer(name);  | Vendor isolation   | Bound to layer    |
| Revert Layer Keyword     | color: revert-layer;        | Step down stack    | Previous layer val|
| Unlayered Styles         | p { color: black; }         | Author top tier    | Beats all layers  |
| Important Inversion      | prop: val !important;       | Safety protection  | Earlier layer wins|
+--------------------------+-----------------------------+--------------------+-------------------+
```

---

## 13. Further Reading

- [W3C CSS Cascading and Inheritance Level 5: Cascade Layers](https://www.w3.org/TR/css-cascade-5/#layering)
- [MDN Web Docs: @layer](https://developer.mozilla.org/en-US/docs/Web/CSS/@layer)
- [web.dev: Cascade Layers are coming to your browser](https://web.dev/articles/cascade-layers)
- [CSSWG Explainer: Cascade Layers](https://github.com/w3c/csswg-drafts/issues/5840)
- [Bramus: Cascade Layers Guide](https://www.bram.us/2022/01/29/the-cascade-layers-and-important/)

---

[Previous: 06 - Cascade: Origins, Importance & Order](06-cascade-origins-importance-and-order.md) | [Section Index](README.md) | [Next: 08 - Inheritance: initial, inherit, unset, revert](08-inheritance-initial-unset-revert.md)
