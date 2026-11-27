# Output-Based and Debugging Interview Questions

| Topic | Module | Level | Target Roles | Date | Prerequisites |
|---|---|---|---|---|---|
| CSS Output Prediction & Real-World Debugging | 15: Interview Questions | Senior to Staff / Principal | Principal Engineer, Senior Frontend Lead | 2026-03-31 | Modules 01–13 |

---

## Overview
This guide tests practical mastery through concrete "What does this render?" code challenges, specificity collision traps, margin collapse calculations, and real-world layout debugging scenarios.

---

## Technical Puzzles & Detailed Answers

### Q1: [Output Prediction] What is the computed text color of the button in this markup?
```html
<div id="app">
  <button class="btn btn-primary" id="submit-btn">Click Me</button>
</div>
```
```css
@layer components {
  #submit-btn { color: red; }
}

:where(#app) .btn.btn-primary {
  color: green;
}

button {
  color: blue !important;
}
```
<details>
<summary>View Output & Verification</summary>

**Computed Color: `blue`**

**Cascade Analysis**:
1. `#submit-btn` has `color: red`, but it is inside `@layer components` and is a *normal* (non-important) declaration.
2. `:where(#app) .btn.btn-primary` has specificity `[0, 2, 0]` because `:where(#app)` contributes `[0, 0, 0]`. It is unlayered, so it would beat the normal layered style (`#submit-btn`).
3. However, `button` declares `color: blue !important;`. In the cascade origins hierarchy, **Author `!important` declarations always defeat all Author normal declarations**, regardless of layers, IDs, or class specificity.
4. Therefore, `color: blue` wins unconditionally.
</details>

---

### Q2: [Output Prediction] What is the rendered height of `.child` in this snippet?
```html
<div class="parent">
  <div class="child">Content</div>
</div>
```
```css
.parent {
  padding: 20px;
}
.child {
  height: 50%;
  background: blue;
}
```
<details>
<summary>View Output & Verification</summary>

**Computed Height: Intrinsic content height (approx. 24px, NOT 50%)**

**Explanation**:
In CSS, a percentage height (`height: 50%`) is calculated relative to the **explicitly computed height of its parent**.
- `.parent` has `height: auto` (its height is determined by its children).
- Because `.parent` has no definite height declared, the child's `50%` cannot be resolved.
- Per W3C specification, when a percentage height cannot resolve against a parent's indefinite height, it falls back to **`height: auto`** (the intrinsic height of the child's content).
- *Solution to make percentage height work*: Either declare an explicit height on `.parent` (`height: 200px` or `min-height: 100vh`), or make `.parent` a Flexbox/Grid container with stretched alignment.
</details>

---

### Q3: [Output Prediction] What is the vertical distance between Box A and Box B?
```html
<div class="box-a">Box A</div>
<div class="box-b">Box B</div>
```
```css
.box-a {
  margin-bottom: 30px;
}
.box-b {
  margin-top: -10px;
}
```
<details>
<summary>View Output & Verification</summary>

**Computed Vertical Distance: `20px`**

**Explanation**:
This is a **Margin Collapse with Negative Margins** calculation:
- When a positive margin collapses with a negative margin, the formula is:
  $$\text{Effective Margin} = M_{\text{positive}} + M_{\text{negative}}$$
- Here: $30\text{px} + (-10\text{px}) = 20\text{px}$.
- Box B is pulled 10px upward into Box A's 30px margin zone, leaving exactly 20px of visible whitespace separation.
</details>

---

### Q4: [Debugging] Why is there a mysterious 3px–4px whitespace gap underneath an image inside a container?
```html
<div class="card">
  <img src="banner.webp" alt="Banner">
</div>
```
```css
.card {
  background: red;
  padding: 0;
}
img {
  width: 100%;
}
```
<details>
<summary>View Diagnosis & Fix</summary>

**Root Cause**:
By default in HTML, `<img>` is an **inline-level element** (`display: inline`). Inline elements are aligned to the typographic baseline of the parent font (`vertical-align: baseline`).

The browser reserves vertical space below the baseline for **typographic descenders** (the tail on letters like `g`, `j`, `p`, `q`, `y`). Even though the image contains no text, the browser reserves that descender space below the image, exposing a sliver of the red `.card` background.

**The Fixes (Any of the following)**:
1. `img { display: block; }` (Removes the image from the typographic inline flow entirely - Best Practice).
2. `img { vertical-align: middle; }` or `vertical-align: bottom;` (Realigns the image away from the text baseline).
3. `.card { font-size: 0; line-height: 0; }` (Eliminates the line box height).
</details>

---

### Q5: [Debugging] Why does `width: 100vw` cause an unwanted horizontal scrollbar on desktop browsers?
<details>
<summary>View Diagnosis & Fix</summary>

**Root Cause**:
`100vw` represents 100% of the **entire viewport window**, *including* the width of the operating system's vertical scrollbar.

On desktop systems (Windows, Linux, macOS with permanent scrollbars enabled), the vertical scrollbar typically occupies 15px–17px of width. 
- If a page has content that scrolls vertically, the document content area is `Viewport Width - Scrollbar Width`.
- If an element declares `width: 100vw`, it expands to the *full* viewport, exceeding the document content area by exactly the width of the vertical scrollbar.
- This creates an annoying horizontal scrollbar at the bottom of the screen.

**The Fix**:
1. Use `width: 100%;` instead of `100vw`. `100%` calculates relative to the containing block, which excludes the scrollbar.
2. Use modern CSS `scrollbar-gutter: stable;` to reserve space permanently.
</details>

---

### Q6: [Output Prediction] What is the color of the text matched by `:is(.alert, #toast) span`?
```html
<div class="alert">
  <span>System Message</span>
</div>
```
```css
.alert span {
  color: blue;
}

:is(.alert, #toast) span {
  color: green;
}
```
<details>
<summary>View Output & Verification</summary>

**Computed Color: `green`**

**Explanation**:
- `.alert span` has a specificity of `[0, 1, 1]` (1 Class, 1 Element).
- `:is(.alert, #toast) span` takes the specificity of its **most specific argument**.
- In the list `(.alert, #toast)`, `#toast` has 1 ID (`[1, 0, 0]`).
- Therefore, the `:is(...)` selector evaluates to `[1, 0, 0] + [0, 0, 1] = [1, 0, 1]`.
- Because `[1, 0, 1] > [0, 1, 1]`, the `:is()` selector wins, rendering the text **`green`**, even though the matching HTML element only has the class `.alert` and `#toast` does not exist in the DOM!
</details>

---

### Q7: [Debugging] Why does a modal dialog render behind a sibling card despite having `z-index: 9999`?
```html
<div class="card-container" style="transform: translate(0, 0);">
  <div class="modal" style="position: fixed; z-index: 9999;">Modal Content</div>
</div>
<div class="hero-banner" style="position: relative; z-index: 10;">Banner Content</div>
```
<details>
<summary>View Diagnosis & Fix</summary>

**Root Cause**:
The parent `.card-container` has `transform: translate(0, 0);`. 
By W3C specification, any value other than `none` on `transform`, `filter`, or `perspective` creates two things:
1. A new **Stacking Context**.
2. A new **Containing Block for `position: fixed` descendants**!

Because `.card-container` creates a containing block, the `.modal` is trapped inside `.card-container`. Furthermore, `.card-container` has no declared `z-index` (evaluating to `auto`/0), which is lower than `.hero-banner` (`z-index: 10`). As a result, `.hero-banner` paints on top of `.card-container` and its trapped modal.

**The Fix**:
1. Move the modal outside `.card-container` to the root `<body>` level.
2. **Modern Best Practice**: Use the native HTML `<dialog>` element and open it via `.showModal()`. Native `<dialog>` elevates to the browser's **Top Layer**, rendering above all parent transforms, filters, and page stacking contexts immune to this bug.
</details>

---

### Q8: [Output Prediction] What are the computed column widths in this CSS Grid layout?
```html
<div class="grid">
  <div class="item">Item 1</div>
  <div class="item">Item 2</div>
  <div class="item">Item 3</div>
</div>
```
```css
.grid {
  display: grid;
  width: 900px;
  grid-template-columns: 100px 1fr 2fr;
  gap: 30px;
}
```
<details>
<summary>View Output & Verification</summary>

**Computed Column Widths**:
- **Column 1**: `100px`
- **Column 2**: `240px`
- **Column 3**: `480px`

**Mathematical Proof**:
1. Total Grid Width = `900px`.
2. Total Gap Space = 2 gaps × 30px = `60px`.
3. Non-flexible tracks = `100px`.
4. Available Free Space:
   $$\text{Free Space} = 900\text{px} - 60\text{px} - 100\text{px} = 740\text{px}$$
   *(Correction)*: Total $\sum \text{fr} = 1 + 2 = 3\text{fr}$.
   $$\text{Width of } 1\text{fr} = \frac{740\text{px}}{3} = 246.67\text{px}$$
   - Column 1: `100px`
   - Column 2 ($1\text{fr}$): `246.67px`
   - Column 3 ($2\text{fr}$): `493.33px`
   - Sum: $100 + 246.67 + 493.33 + 60\text{ (gaps)} = 900.00\text{px}$.
</details>

---

## Navigation
- **Previous Guide**: [Modern CSS & Platform Baseline Q&A](./07-modern-css-qa.md)
- **Next Guide**: [Practical Layout Challenges Q&A](./09-practical-layout-challenges.md)
