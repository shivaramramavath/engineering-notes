# Cascade, Specificity, and Inheritance Interview Questions

| Topic | Module | Level | Target Roles | Date | Prerequisites |
|---|---|---|---|---|---|
| Cascade, Specificity & Layers | 15: Interview Questions | Mid to Senior / Lead | Senior Frontend, Design Systems Engineer | 2026-03-31 | Modules 01, 10 |

---

## Overview
This guide addresses the mathematical mechanics of the CSS Cascade: specificity tuple vectors, functional pseudo-class specificity calculations, origin priority hierarchy, Cascade Layers (`@layer`), and computed value inheritance keywords.

---

## Technical Questions & Detailed Answers

### Q1: Calculate the exact specificity of `:is(.card, #hero) p:hover` and explain the mathematical rule for `:is()`.
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
The specificity is **`[1, 1, 1]`** (1 ID, 1 Pseudo-class, 1 Element).

**Rule for `:is()` (and `:not()`, `:has()`)**:
The `:is()` pseudo-class itself contributes zero specificity; instead, it adopts the **specificity of its most specific argument** in the selector list.
- In the list `(:card, #hero)`, `.card` is `[0, 1, 0]`, while `#hero` is `[1, 0, 0]`.
- Therefore, `:is(.card, #hero)` evaluates with the specificity of `#hero`: `[1, 0, 0]`.
- Adding `p` (element: `[0, 0, 1]`) and `:hover` (pseudo-class: `[0, 1, 0]`) yields:
  `[1, 0, 0] + [0, 0, 1] + [0, 1, 0] = [1, 1, 1]`.

Even if the matched element is `.card p`, the browser uses `[1, 1, 1]` because `#hero` exists in the argument list.
</details>

---

### Q2: Why is the "base-10" specificity myth (e.g. 10 classes equal 1 ID) scientifically false in modern browser engines?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
Historically, some amateur tutorials simplified specificity into decimal numbers: Elements = 1, Classes = 10, IDs = 100. This led to the false belief that stacking 10 classes (`.c1.c2.c3...`) would equal or defeat an ID (`#header`).

In modern browser engines (and W3C specifications), specificity is evaluated as a **lexicographical tuple `[A, B, C]`**:
- Column A (IDs) is compared first. If Rule 1 has 1 ID and Rule 2 has 0 IDs, Rule 1 wins unconditionally. Column B is never even inspected.
- Stacking 1,000 class names yields `[0, 1000, 0]`. An ID selector yields `[1, 0, 0]`.
- Because `1 > 0` in Column A, the single ID selector defeats 1,000 classes every time. No carry-over or overflow exists between columns.
</details>

---

### Q3: What is the complete cascade priority resolution order across origins, layers, and importance?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
When multiple declarations compete for the same CSS property, modern browser engines resolve precedence in the following strict order (from lowest to highest):

1. **User Agent normal** (browser default stylesheet)
2. **User normal** (user custom browser styles)
3. **Author normal unlayered** (developer CSS)
4. **Author normal layered** (ordered from first declared `@layer` to last `@layer`)
5. **Author normal unlayered** (unlayered styles win over layered styles!)
6. **Active CSS Animations** (CSS `@keyframes` interpolate here)
7. **Author `!important` layered** (INVERTED: First `@layer !important` beats last `@layer !important`!)
8. **Author `!important` unlayered**
9. **User `!important`**
10. **User Agent `!important`**
11. **Active CSS Transitions** (interpolating CSS transitions beat all author `!important` declarations)
</details>

---

### Q4: Explain the "`!important` Layer Inversion" paradox in CSS Cascade Layers (`@layer`).
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
In normal (non-important) declarations, later layers override earlier layers:
```css
@layer reset, framework, custom;
/* Normal: 'custom' beats 'framework', which beats 'reset' */
```

However, when `!important` is added to a declaration inside an earlier layer, the priority order is **completely inverted**:
```css
@layer reset {
  button { color: red !important; } /* WINS! */
}
@layer custom {
  button { color: blue !important; }
}
```
In this scenario, `color: red !important` inside `@layer reset` **defeats** `color: blue !important` in `@layer custom`.

**The Architectural Rationale**:
Cascade Layers are designed to empower consumers over libraries. A reset or utility layer needs the ability to guarantee fundamental constraints (e.g. accessibility high-contrast overrides or layout protection) without allowing higher component layers to break them with rogue `!important` statements.
</details>

---

### Q5: What is the exact difference between `revert` and `revert-layer`?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
- **`revert`**: Rolls back the property value to the value it would have had in the **previous cascade origin**. For author styles, `revert` rolls back past the entire Author origin, restoring the User or User Agent (browser default) style.
- **`revert-layer`**: Rolls back the property value to the value it would have had in the **previous cascade layer** within the *current* Author origin. If no prior layer defines a value, it falls back to the previous origin.

```css
@layer base {
  button { color: green; }
}
@layer components {
  button {
    color: red;
    /* Reverting here: */
    /* color: revert;       -> Resolves to User Agent default (black/system) */
    /* color: revert-layer; -> Resolves to 'green' from @layer base! */
  }
}
```
</details>

---

### Q6: How does `:where()` enable zero-specificity CSS components in design systems?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
The `:where()` pseudo-class has a fixed specificity of strictly **`[0, 0, 0]`**, regardless of the complexity of the selectors inside its arguments.

In design systems, wrapping base component styles in `:where()` guarantees they can be overridden by *any* consumer selector, even a bare element tag:
```css
/* Inside Design System Library */
:where(.btn.btn-primary.btn-large) {
  padding: 1rem 2rem;
  background-color: blue;
}

/* Consumer Code in Application */
button {
  background-color: purple; /* WINS! Specificity [0, 0, 1] beats [0, 0, 0] */
}
```
This eliminates the need for consumers to write high-specificity hacks or `!important` to customize library components.
</details>

---

### Q7: Explain computed value inheritance and distinguish between `initial`, `inherit`, and `unset`.
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
- **Inherited Properties**: Properties that by default pass their computed value from parent to child (e.g. `color`, `font-family`, `line-height`, `visibility`).
- **Non-Inherited Properties**: Properties that default to their initial value if not explicitly declared on the element (e.g. `margin`, `padding`, `border`, `display`, `background`).

The universal CSS keywords behave as follows:
- **`initial`**: Sets the property to the value defined in the CSS specification (e.g. `color: initial` resolves to black, `display: initial` resolves to inline).
- **`inherit`**: Explicitly forces the element to adopt the computed value of its direct parent, even for non-inherited properties (e.g. `border: inherit`).
- **`unset`**: Contextual reset: acts like `inherit` if the property is naturally inherited, and acts like `initial` if the property is naturally non-inherited.
- **`all: unset`**: Resets all CSS properties on an element to their natural initial/inherited state, commonly used to build clean unstyled button resets.
</details>

---

### Q8: What is "Invalid at Computed-Value Time" (IACVT) in CSS Custom Properties?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
When a CSS custom property resolves to a value that is syntactically invalid for the target property (for example, setting `color: var(--my-width)` where `--my-width` is `100px`), the browser cannot catch the error during initial CSSOM parsing because custom property substitution happens during the **Computed Value** stage.

When IACVT occurs:
1. The browser does NOT fall back to an earlier declared rule in the stylesheet (the cascade has already resolved!).
2. Instead, the engine treats the property as if it were declared as **`unset`**.
3. If the property is naturally inherited (like `color`), it inherits from its parent. If non-inherited (like `background-color`), it resets to its `initial` value (transparent).
</details>

---

## Navigation
- **Previous Guide**: [Core Syntax & Platform Fundamentals Q&A](./01-css-basics-qa.md)
- **Next Guide**: [Box Model and Positioning Q&A](./03-box-model-and-positioning-qa.md)
