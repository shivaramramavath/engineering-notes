# The `:has()` Relational Pseudo-Class

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | Selectors Level 4 (W3C Working Draft / Candidate Recommendation) |
| **Baseline Status** | Baseline 2023 (Widely Available across Chrome 105+, Safari 15.4+, Firefox 121+, Edge 105+) |
| **Browsers Tested** | Chrome 105+, Firefox 121+, Safari 15.4+, Edge 105+ |
| **Tested With** | W3C Web Platform Tests, Chrome DevTools Rendering, Playwright |

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

For more than two decades, the inability of CSS to select parent elements based on child conditions or style preceding siblings based on subsequent siblings was one of the most frustrating limitations in web development. Developers routinely called it "The Holy Grail of CSS Selectors".

Historically, solving simple relational UI requirements required JavaScript:
- Highlighting a form card when an input inside it became invalid.
- Toggling body scroll when a modal was opened inside a deep component subtree.
- Styling an article card differently if it contained an image or video hero.
- Emulating a previous-sibling combinator (e.g., hovering an item to dim previous siblings).

The `:has()` relational pseudo-class, finalized in **Selectors Level 4** and achieving **Baseline 2023** status, fundamentally resolves this problem. It allows any element to be styled based on its descendants, direct children, or adjacent siblings, transforming how developers structure declarative UI components without JavaScript event listeners.

---

## 2. Core Concepts

### What is `:has()`?
`:has()` is a functional pseudo-class that takes a relative selector list as its argument. It represents an element if any of the relative selectors would match at least one element when evaluated relative to the anchor element:

```css
/* Select any .card that contains an <img> element */
.card:has(img) {
  grid-template-columns: 200px 1fr;
}
```

### Relational Combinators Inside `:has()`
You can pass any relative selector combinator inside `:has()`:
- **Child Combinator (`>`)**: `article:has(> h2)` (matches an `article` whose direct child is an `h2`).
- **Next-Sibling Combinator (`+`)**: `h1:has(+ p.lead)` (matches an `h1` immediately followed by a lead paragraph).
- **Subsequent-Sibling Combinator (`~`)**: `label:has(~ input:checked)` (matches a `label` preceding a checked input).
- **Descendant (Space)**: `form:has(button:focus)` (matches a `form` containing a focused button).

### Previous-Sibling Emulation
CSS has a next-sibling combinator (`+`) and subsequent-sibling combinator (`~`), but has never had a "previous-sibling" combinator. Using `:has()`, you can select preceding siblings:

```css
/* Select a label immediately PRECEDING an invalid input */
label:has(+ input:invalid) {
  color: #ef4444;
}

/* Select any card PRECEDING a hovered card */
.item:has(~ .item:hover) {
  opacity: 0.5;
}
```

### Specificity of `:has()`
Like `:is()` and `:not()`, the specificity of `:has()` is determined by the **most specific selector in its argument list**:
- `.card:has(h2)` has specificity `(0, 1, 1)` (class `.card` + element `h2`).
- `.card:has(#hero)` has specificity `(1, 1, 0)` (class `.card` + ID `#hero`).
- If `:has()` matches, the pseudo-class itself contributes zero base specificity; only its argument's maximum specificity is added.

---

## 3. How It Works Internally

```
Classic CSS Selector Matching Engine:
  - Traversed Right-to-Left (Bottom-Up) through DOM tree.
  - Matches child -> verifies parent -> verifies ancestor.
  - O(N) evaluation complexity.

Modern :has() Execution Engine:
  1. Browser encounters '.card:has(img)'
  2. Traverses candidate '.card' DOM node.
  3. Executes sub-query rooted at candidate node:
     - Scans descendant subtree for 'img'.
  4. Caching & Fast-Reject Bloom Filters:
     - Browser builds ancestor/descendant Bloom filters.
     - Quickly rejects '.card' nodes lacking 'img' in their subtree.
  5. Invalidation Tracking on DOM Mutation:
     - When an <img> is inserted/removed, browser marks ancestor '.card' dirty.
     - Modern engines restrict invalidation strictly to matching subtrees.
```

```
Relative Selector Evaluation Matrix:
┌──────────────────────────────┬────────────────────────────────────────────────────────┐
│ Selector Pattern             │ Semantic Interpretation                                │
├──────────────────────────────┼────────────────────────────────────────────────────────┤
│ `section:has(> h2)`          │ Direct Parent: Section whose direct child is H2.       │
│ `figure:has(figcaption)`     │ Deep Ancestor: Figure containing Figcaption somewhere. │
│ `dt:has(+ dd:empty)`         │ Preceding Sibling: DT immediately followed by empty DD.│
│ `li:has(~ li:hover)`         │ Previous Siblings: Any LI preceding a hovered LI.      │
│ `body:has(dialog[open])`     │ Global Page State: Body containing an open dialog.     │
└──────────────────────────────┴────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2023 Support Box:
├── Safari: 15.4+ (March 2022)
├── Chrome / Edge: 105+ (August 2022)
└── Firefox: 121+ (December 2023 - Complete Baseline cross-browser availability)
```

### Basic Syntax
```css
/* Target parent if it contains a matching descendant */
selector:has(relative-selector) {
  /* Declarations */
}

/* Negation: target elements that DO NOT contain child */
selector:not(:has(relative-selector)) {
  /* Declarations */
}

/* Logical OR in argument list */
selector:has(img, video) {
  /* Matches if it has an image OR video */
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Card Layouts (Image vs. Text-Only)
Automatically adjust a card layout from a 2-column media grid to a single-column text card based on the presence of media.

```html
<!-- Card with image: 2 columns -->
<div class="card">
  <img src="thumb.jpg" alt="Preview">
  <div class="body">
    <h3>Design Systems</h3>
    <p>Creating consistent enterprise UI libraries.</p>
  </div>
</div>

<!-- Text-only card: centered 1 column -->
<div class="card">
  <div class="body">
    <h3>Pure Typography</h3>
    <p>Minimalist layout with no imagery.</p>
  </div>
</div>
```

```css
.card {
  display: block;
  padding: 1.5rem;
  border-radius: 0.5rem;
  background: #ffffff;
  border: 1px solid #e2e8f0;
}

/* When the card contains an <img>, switch to CSS Grid layout */
.card:has(img) {
  display: grid;
  grid-template-columns: 8rem 1fr;
  gap: 1.25rem;
  align-items: center;
}

.card img {
  width: 100%;
  height: 8rem;
  object-fit: cover;
  border-radius: 0.375rem;
}
```

### Example 2: Form Container Validation & Submit Button Activation
Highlight an entire form field group in red if the input inside it is invalid and has been touched.

```html
<div class="form-group">
  <label for="user-email">Work Email</label>
  <input type="email" id="user-email" required placeholder="name@company.com">
  <span class="error-msg">Please enter a valid company email address.</span>
</div>
```

```css
.form-group {
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
  padding: 1rem;
  border: 1px solid #cbd5e1;
  border-radius: 0.5rem;
  transition: border-color 0.2s ease, background-color 0.2s ease;

  .error-msg {
    display: none;
    font-size: 0.8125rem;
    color: #dc2626;
  }
}

/* Style parent group when input is invalid AND user has blurred or typed */
.form-group:has(input:user-invalid) {
  border-color: #ef4444;
  background-color: #fef2f2;

  label {
    color: #b91c1c;
    font-weight: 600;
  }

  .error-msg {
    display: block;
  }
}

/* Glow border on focus */
.form-group:has(input:focus-visible) {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
}
```

### Example 3: Global Body Scroll Lock on Modal Open
Prevent background scrolling when a native `<dialog>` or custom modal is open without writing JavaScript `document.body.style.overflow = 'hidden'`.

```html
<body>
  <main class="page-content">
    <h1>Dashboard</h1>
  </main>
  <dialog id="settings-dialog" class="modal">
    <h2>Account Preferences</h2>
  </dialog>
</body>
```

```css
/* Freeze body scroll whenever any dialog in the page is open */
body:has(dialog[open]) {
  overflow: hidden;
  user-select: none;
}

/* Dim page content behind modal */
body:has(dialog[open]) .page-content {
  filter: blur(2px);
  pointer-events: none;
}
```

### Example 4: Sibling Focus Dimming / Spotlight Navigation
When hovering or focusing any link in a navigation list, dim all sibling links to highlight the selected item.

```html
<nav class="spotlight-nav">
  <ul class="nav-list">
    <li><a href="#overview">Overview</a></li>
    <li><a href="#features">Features</a></li>
    <li><a href="#pricing">Pricing</a></li>
    <li><a href="#contact">Contact</a></li>
  </ul>
</nav>
```

```css
.nav-list {
  display: flex;
  gap: 2rem;
  list-style: none;
  padding: 0;

  a {
    font-size: 1.125rem;
    font-weight: 500;
    color: #0f172a;
    text-decoration: none;
    transition: opacity 0.2s ease, transform 0.2s ease;

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: 4px;
      border-radius: 2px;
    }
  }
}

/* When the list has ANY link hovered or focused, dim all links */
.nav-list:has(a:hover, a:focus-visible) a {
  opacity: 0.35;
}

/* Keep the actively hovered/focused link at full opacity and scale up */
.nav-list a:hover,
.nav-list a:focus-visible {
  opacity: 1 !important;
  transform: translateY(-2px);
}
```

### Example 5: Star Rating Widget (Previous Siblings Highlight)
Implement an interactive 5-star rating control purely in CSS where hovering over any star illuminates all preceding stars.

```html
<div class="star-rating">
  <button class="star" aria-label="1 star">★</button>
  <button class="star" aria-label="2 stars">★</button>
  <button class="star" aria-label="3 stars">★</button>
  <button class="star" aria-label="4 stars">★</button>
  <button class="star" aria-label="5 stars">★</button>
</div>
```

```css
.star-rating {
  display: inline-flex;
  gap: 0.25rem;
}

.star {
  background: none;
  border: none;
  font-size: 1.75rem;
  color: #cbd5e1;
  cursor: pointer;
  padding: 0;
  transition: color 0.15s ease, transform 0.15s ease;

  &:focus-visible {
    outline: 2px solid #f59e0b;
    outline-offset: 2px;
  }
}

/* 1. Highlight the actively hovered star */
.star:hover {
  color: #f59e0b;
  transform: scale(1.2);
}

/* 2. Highlight any star that precedes a hovered star! */
.star:has(~ .star:hover) {
  color: #fbbf24;
}
```

---

## 6. Best Practices

1. **Scope `:has()` selectors narrowly**: Avoid sweeping global selectors like `*:has(p)`. Always anchor `:has()` to a specific class or container: `.card:has(img)` instead of `:has(img)`.
2. **Combine with `:not()` for empty or missing state styling**: Use `.list:not(:has(li))` to display a clean empty state card without writing conditional rendering scripts.
3. **Use `:user-invalid` rather than `:invalid` with `:has()`**: `:invalid` triggers immediately on pristine unsubmitted forms, causing forms to render with alarming error styles before users type. `:has(:user-invalid)` triggers only after user interaction.
4. **Leverage `:has()` to eliminate DOM-state tracking JavaScript**: Replace event listeners that toggle class names on parents (e.g. `is-active`, `has-modal-open`) with declarative `:has()` queries.
5. **Keep relative selectors shallow where possible**: Prefer direct child matching (`.parent:has(> .badge)`) over deep descendant matching when applicable to allow browser fast-rejection filters to optimize style passes.
6. **Ensure keyboard accessibility remains intact**: When using `:has(:hover)` to create hover spotlights, always pair it with `:focus-visible` (e.g., `:has(:hover, :focus-visible)`).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Nesting `:has()` inside `:has()` (`:has(:has(...))`) | Invalid syntax. The CSS specification strictly forbids nesting `:has()`. | Flatten into compound sibling or descendant selectors. |
| Using pseudo-elements inside `:has()` (`:has(::before)`) | Invalid selector. Pseudo-elements cannot be targeted inside `:has()`. | Target real DOM elements or pseudo-classes. |
| Writing unbounded universal queries: `body:has(*) ` | Causes severe style recalculation penalties during animations or scroll events. | Scope to specific component containers or state attributes. |
| Forgetting `:focus-visible` when pairing with `:hover` | Keyboard-only users miss out on interactive states. | Always write `:has(:hover, :focus-visible)`. |
| Expecting `:has()` to match across Shadow DOM boundaries | Selectors cannot penetrate shadow trees into slotted content. | Style shadow hosts using `:host-context()` or CSS custom properties. |
| Specificity escalation from complex arguments | Passing `#id` inside `:has()` escalates the parent selector specificity to ID level. | Use classes or wrap arguments in `:where()` to drop specificity. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- Early browser drafts struggled with `:has()` performance due to exponential DOM tree scanning.
- Modern Blink, Gecko, and WebKit engines utilize **ancestor-descendant Bloom filters** and dedicated dependency graphs. When an element mutates, only nodes whose selector footprints match the mutated node trigger invalidation.
- Avoid using `:has()` inside frequently animating or scrolling loops where geometry changes every 16ms.

### Accessibility (A11y)
- `:has()` dramatically simplifies accessible UI development by enabling visual states to respond directly to semantic HTML states (such as `aria-expanded="true"`, `dialog[open]`, and `:checked`) rather than custom JavaScript classes.

### Browser Compatibility
- **Baseline 2023**: Supported in all major desktop and mobile browsers (Chrome 105+, Safari 15.4+, Firefox 121+).
- **Feature Detection**:
```css
@supports selector(:has(p)) {
  /* Supported */
}
```

---

## 9. Comparison / When to Use What

| Technique | Parent Styling | Preceding Sibling Styling | Runtime Engine | JS Required? |
| :--- | :--- | :--- | :--- | :--- |
| **CSS `:has()`** | Native | Native (`:has(~ .next)`) | CSSOM (Fast C++) | No |
| **JavaScript Class Toggling** | Manual | Manual | Main Thread JS | Yes |
| **MutationObserver** | Manual | Manual | Main Thread JS | Yes |
| **CSS `:focus-within`** | Limited to focus | None | CSSOM | No |

---

## 10. Interview Questions

### Q1: Why was `:has()` notoriously absent from CSS for more than 20 years?
<details>
<summary>View Answer</summary>
CSS selector engines evaluate selectors from right-to-left to quickly reject non-matching nodes without walking whole subtrees. Introducing a parent selector meant that a DOM mutation on a leaf node could invalidate styles on any of its ancestors or preceding siblings, risking severe $O(N^2)$ layout performance degradation. It required breakthroughs in invalidation graph caching and bloom filters before browser vendors agreed it was safe to implement.
</details>

### Q2: What is the specificity of `.menu:has(a:hover, #special-link)`?
<details>
<summary>View Answer</summary>
The specificity of `:has()` equals the highest specificity among the selectors in its argument list. Here, `#special-link` has specificity `(1, 0, 0)` while `a:hover` has `(0, 1, 1)`. Added to `.menu` `(0, 1, 0)`, the total specificity of the rule is `(1, 1, 0)`.
</details>

### Q3: Can you nest a `:has()` pseudo-class inside another `:has()` pseudo-class?
<details>
<summary>View Answer</summary>
No. The W3C Selectors Level 4 specification explicitly forbids nesting `:has()` inside another `:has()` (e.g. `div:has(p:has(span))` is invalid). Attempting to do so causes the entire selector to be discarded as invalid.
</details>

### Q4: Can pseudo-elements like `::before` or `::after` be used inside `:has()`?
<details>
<summary>View Answer</summary>
No. Pseudo-elements represent generated content rather than real nodes in the Document Object Model (DOM) tree. The `:has()` argument grammar only accepts relative selector syntax composed of real DOM element selectors and pseudo-classes.
</details>

### Q5: How can `:has()` be used to emulate a "previous sibling" selector in CSS?
<details>
<summary>View Answer</summary>
By combining `:has()` with the subsequent sibling combinator (`~`) or next sibling combinator (`+`). For example, `li:has(+ li:hover)` selects the `<li>` immediately preceding a hovered `<li>`. Similarly, `li:has(~ li:hover)` selects all `<li>` elements that precede a hovered `<li>`.
</details>

### Q6: How does `:has()` interact with `:not()`?
<details>
<summary>View Answer</summary>
`:has()` can be combined with `:not()` in two distinct ways:
1. `:not(:has(.child))`: Selects parents that do NOT contain `.child` (e.g. an empty container).
2. `:has(:not(.child))`: Selects parents that contain at least one descendant that is NOT `.child`.
Developers frequently confuse these two forms; targeting empty containers requires `:not(:has(...))`.
</details>

### Q7: What is the difference between `.card:has(> .badge)` and `.card:has(.badge)`?
<details>
<summary>View Answer</summary>
`.card:has(> .badge)` uses the direct child combinator (`>`), meaning the badge must be an immediate child of `.card`. In contrast, `.card:has(.badge)` uses the descendant space combinator, matching if `.badge` exists anywhere in the subtree of `.card`, no matter how deeply nested.
</details>

### Q8: What happens if an unsupported selector is passed into `:has()`?
<details>
<summary>View Answer</summary>
According to the forgiving selector list rules adopted for `:has()`, `:is()`, and `:where()`, an invalid or unsupported selector in the argument list is discarded, allowing the remaining valid relative selectors to continue matching without invalidating the entire selector rule.
</details>

---

## 11. Practice Exercises

### Exercise 1: Empty State Container
**Objective**: Style a shopping cart container with a dashed border and centered "Your cart is empty" message when it contains no `.cart-item` elements.
<details>
<summary>View Solution</summary>

```css
.cart-container {
  padding: 1.5rem;
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
}

/* Empty State: Cart has NO items */
.cart-container:not(:has(.cart-item)) {
  border: 2px dashed #cbd5e1;
  background-color: #f8fafc;
  text-align: center;
  padding: 3rem 1.5rem;

  &::after {
    content: 'Your shopping cart is currently empty.';
    color: #64748b;
    font-size: 1rem;
    font-weight: 500;
  }
}
```
</details>

### Exercise 2: Grid Hero Card Auto-Spanning
**Objective**: If an article in a grid contains an `.is-hero` element, make the entire card span two columns.
<details>
<summary>View Solution</summary>

```css
.news-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(18rem, 1fr));
  gap: 1.5rem;
}

.news-card {
  background: white;
  padding: 1rem;
  border-radius: 0.5rem;
  border: 1px solid #e2e8f0;
}

/* Card expands across 2 columns when housing a hero banner */
.news-card:has(.is-hero) {
  grid-column: span 2;
  background-color: #0f172a;
  color: #f8fafc;
}
```
</details>

### Exercise 3: Sibling Row Highlighting in Data Tables
**Objective**: In a data table, highlight the row containing a checked checkbox with a distinct background color.
<details>
<summary>View Solution</summary>

```css
.data-table {
  width: 100%;
  border-collapse: collapse;

  tr {
    border-bottom: 1px solid #e2e8f0;
    transition: background-color 0.15s ease;
  }

  /* Highlight entire table row when its checkbox is checked */
  tr:has(input[type="checkbox"]:checked) {
    background-color: #eff6ff;
  }
}
```
</details>

### Exercise 4: Mandatory Agreement Gate
**Objective**: Disable a form's submit button styling unless the terms and conditions checkbox inside the form is checked.
<details>
<summary>View Solution</summary>

```css
.signup-form {
  display: flex;
  flex-direction: column;
  gap: 1rem;

  .btn-submit {
    padding: 0.75rem 1.5rem;
    background-color: #2563eb;
    color: white;
    border: none;
    border-radius: 0.375rem;
    cursor: pointer;
    transition: opacity 0.2s ease;

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: 2px;
    }
  }

  /* Dim submit button when terms checkbox is NOT checked */
  &:not(:has(#agree-terms:checked)) .btn-submit {
    opacity: 0.4;
    pointer-events: none;
  }
}
```
</details>

### Exercise 5: Previous Header Decoration
**Objective**: Style an `<h3>` with an underline decoration only if it is immediately followed by a code block (`pre`).
<details>
<summary>View Solution</summary>

```css
.doc-article {
  h3:has(+ pre) {
    text-decoration: underline;
    text-decoration-color: #3b82f6;
    text-underline-offset: 6px;
  }
}
```
</details>

### Exercise 6: Focus Trap Visual Shield
**Objective**: When an input inside an open search drawer is focused, apply a dimming overlay across the rest of the application header.
<details>
<summary>View Solution</summary>

```css
.app-header {
  position: relative;
  display: flex;
  justify-content: space-between;
  padding: 1rem;
  background: white;

  .branding, .user-actions {
    transition: opacity 0.2s ease;
  }

  /* When search input is focused, fade surrounding header controls */
  &:has(.search-input:focus) {
    .branding, .user-actions {
      opacity: 0.2;
      pointer-events: none;
    }
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Parent contains child */
.card:has(img) { ... }

/* 2. Parent contains direct child */
section:has(> h2) { ... }

/* 3. Preceding sibling (element preceding checked box) */
label:has(+ input:checked) { ... }

/* 4. Previous siblings (all items before hovered item) */
li:has(~ li:hover) { ... }

/* 5. Negation (empty container) */
.list:not(:has(li)) { ... }

/* 6. Form state parent reaction */
.form-card:has(input:user-invalid) { border-color: red; }
```

- Specificity = max specificity of argument selector list.
- Invalid syntax: nesting `:has()` or using pseudo-elements inside `:has()`.
- Supported cross-browser in all major engines (Baseline 2023).

---

## 13. Further Reading

- [MDN Web Docs: :has() pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:has)
- [W3C Selectors Level 4: The Relational Pseudo-class :has()](https://drafts.csswg.org/selectors-4/#relational)
- [web.dev: :has(): the family selector](https://web.dev/has-m105/)
- [WebKit: Using :has() as a CSS Parent Selector and much more](https://webkit.org/blog/13096/css-has-pseudo-class/)

---

### Navigation
- **Previous**: [01-nesting.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/01-nesting.md)
- **Next**: [03-is-where-and-not.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/03-is-where-and-not.md)
