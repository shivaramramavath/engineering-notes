# Common Flexbox UI Patterns

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Production Patterns |
| **Prerequisites** | `03-flexbox/01-flexbox-mental-model.md` through `06-flexbox-sizing-algorithm-deep-dive.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+ |
| **Tested With** | Automated CSS Validation Runner (`css_runner.js`) |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

Theory without practice is useless. While understanding axes and free space algorithms is foundational, real-world frontend engineering requires recognizing recurring UI layout archetypes and implementing them cleanly with Flexbox.

Over the last decade, the web industry has consolidated around several standard **Flexbox Component Patterns**:
1. **The Classic Media Object** (image + fluid content).
2. **The Sticky Footer App Shell** (header, dynamic scrollable body, footer pinned to bottom).
3. **The Split Navigation Bar** (brand on left, centered links, user avatar pushed to far right via auto margins).
4. **Equal Height Cards with Bottom-Pinned CTAs** (vertical flex with `margin-top: auto`).
5. **The Fluid Input Group** (fixed-size icon/button addons flanking a fluid text input).
6. **The Wrapping Tag Cloud / Filter Bar** (dynamic badges flowing naturally across rows).

---

## 2 Core Concepts

Each of these classic patterns leverages a specific strength of the Flexbox model:
- **Auto Margins**: Used in Split Navbars and Card CTAs to sponge available free space without creating empty wrapper elements.
- **`flex: 1` + `min-width: 0`**: Used in Media Objects and Input Groups to ensure fluid text expansion while preserving clean truncation.
- **`flex-direction: column` + `min-height: 100vh`**: Used in Sticky Footers to anchor page layouts.
- **`flex-wrap: wrap` + `gap`**: Used in Chip Clouds to handle multi-line tag wrapping without margin leak.

---

## 3 How It Works Internally

```
1. Split Navigation Bar Architecture
+-------------------------------------------------------------------------+
| [Brand Logo] [Link 1] [Link 2]            (margin-left: auto) [Profile] |
+-------------------------------------------------------------------------+

2. Sticky Footer Layout Architecture
+-------------------------------------------------------------------------+
| Header (flex: 0 0 auto)                                                 |
+-------------------------------------------------------------------------+
| Main Content Area (flex: 1 0 auto - expands to push footer down)        |
|                                                                         |
|                                                                         |
+-------------------------------------------------------------------------+
| Footer (flex: 0 0 auto - always sits at or below viewport fold)         |
+-------------------------------------------------------------------------+

3. Equal Height Card with Pinned Bottom Button
+-----------------------------+
| Card Title                  |
| Card Description text...    |
| (Variable length paragraph) |
|                             |
| [margin-top: auto]          |
| [Action Button Pinned Here] |
+-----------------------------+
```

---

## 4 Syntax & Usage

```css
/* Pattern 1: Media Object */
.media-object {
  display: flex;
  align-items: flex-start;
  gap: 16px;
}
.media-object-img { flex-shrink: 0; }
.media-object-body { flex: 1; min-width: 0; }

/* Pattern 2: Sticky Footer */
.page-shell {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
}
.page-main { flex: 1; }

/* Pattern 3: Split Nav */
.nav-split-end { margin-left: auto; }

/* Pattern 4: Bottom Pinned Button */
.card-cta { margin-top: auto; }
```

---

## 5 Practical Examples

### Pattern 1: Production-Grade Media Object (Comments / Feed Item)

```css
.comment-item {
  display: flex;
  align-items: flex-start;
  gap: 16px;
  padding: 16px;
  border-bottom: 1px solid #e2e8f0;
}

.comment-avatar {
  flex: 0 0 48px;
  width: 48px;
  height: 48px;
  border-radius: 50%;
  object-fit: cover;
}

.comment-body {
  flex: 1;
  min-width: 0; /* Prevents long URLs in comments from breaking layout */
}

.comment-author {
  font-weight: 700;
  margin-bottom: 4px;
}

.comment-text {
  color: #475569;
  line-height: 1.5;
}
```

### Pattern 2: Application Shell with Sticky Footer

```css
.app-viewport {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  background-color: #f8fafc;
}

.app-header {
  flex: 0 0 64px;
  background-color: #0f172a;
  color: #ffffff;
  display: flex;
  align-items: center;
  padding: 0 24px;
}

.app-main {
  flex: 1 0 auto; /* Expands to fill viewport, pushing footer down */
  max-width: 1200px;
  width: 100%;
  margin: 0 auto;
  padding: 32px 16px;
}

.app-footer {
  flex: 0 0 auto;
  background-color: #ffffff;
  border-top: 1px solid #e2e8f0;
  padding: 24px;
  text-align: center;
  color: #64748b;
}
```

### Pattern 3: Three-Section Responsive Split Navbar

```css
.navbar {
  display: flex;
  align-items: center;
  padding: 12px 24px;
  background-color: #ffffff;
  border-bottom: 1px solid #e2e8f0;
  gap: 16px;
}

.nav-brand {
  font-size: 1.25rem;
  font-weight: 800;
  color: #0f172a;
  text-decoration: none;
}

.nav-links {
  display: flex;
  gap: 16px;
  list-style: none;
  margin: 0;
  padding: 0;
}

.nav-user-actions {
  margin-left: auto; /* Consumes all spare horizontal space */
  display: flex;
  align-items: center;
  gap: 12px;
}
```

### Pattern 4: Equal Height Cards with Pinned CTA Button

```css
.pricing-deck {
  display: flex;
  gap: 24px;
}

.pricing-card {
  flex: 1;
  display: flex;
  flex-direction: column; /* Internal vertical flex */
  background: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  padding: 32px 24px;
}

.pricing-features {
  list-style: none;
  padding: 0;
  margin: 24px 0;
}

.pricing-button {
  margin-top: auto; /* Pushed cleanly to bottom regardless of feature list length */
  background-color: #2563eb;
  color: #ffffff;
  padding: 12px;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  text-align: center;
}
```

### Pattern 5: Dynamic Search Input Addon Group

```css
.search-input-group {
  display: flex;
  max-width: 420px;
  width: 100%;
}

.search-input-prefix {
  flex: 0 0 44px;
  background-color: #f1f5f9;
  border: 1px solid #cbd5e1;
  border-right: none;
  border-start-start-radius: 6px;
  border-end-start-radius: 6px;
  display: grid;
  place-items: center;
}

.search-input-field {
  flex: 1;
  min-width: 0;
  padding: 10px 12px;
  border: 1px solid #cbd5e1;
  font-size: 1rem;
}

.search-input-field:focus {
  outline: 2px solid #2563eb;
  outline-offset: -1px;
  z-index: 1; /* Elevates border above adjacent buttons */
}

.search-input-btn {
  flex: 0 0 auto;
  padding: 0 20px;
  background-color: #2563eb;
  color: #ffffff;
  border: 1px solid #2563eb;
  border-start-end-radius: 6px;
  border-end-end-radius: 6px;
  font-weight: 600;
  cursor: pointer;
}
```

---

## 6 Best Practices

1. **Always use `margin-left: auto` for split navbars**: Avoids creating artificial nested flex wrapper containers.
2. **Pair `flex: 1` with `min-width: 0` in fluid flex children**: Prevents long unhyphenated text strings from blowing out container widths.
3. **Use `margin-top: auto` on card buttons**: Ensures clean, uniform button alignment across cards with differing paragraph heights.
4. **Apply `flex-shrink: 0` to avatars and icons**: Protects graphical elements from being compressed when container space contracts.
5. **Use `gap` instead of margin resets**: Produces cleaner, more maintainable code without `:last-child` overrides.
6. **Set `min-height: 100vh` on sticky footer wrappers**: Ensures the footer stays pinned to the bottom of the viewport even on blank or short pages.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Breaks | Solution |
| :--- | :--- | :--- |
| Sticky footer floats in middle of empty page | Wrapper used `height: 100%` without explicit parent height | Use `min-height: 100vh` on the wrapper |
| Card button floating immediately under short text | Forgot `margin-top: auto` or container is not `display: flex; flex-direction: column` | Apply vertical flexbox and auto margin |
| Split navbar wraps unexpectedly | Inner elements have `flex-shrink: 0` or default `nowrap` | Add `min-width: 0` or responsive media queries |
| Avatar crushed into oval | Avatar lacks `flex-shrink: 0` and is squeezed by text | Declare `flex-shrink: 0` |
| Input field overlapping button border | Focus outline hidden behind sibling buttons | Add `z-index: 1` on `:focus` |
| Nested flex child overflowing horizontally | Default `min-width: auto` prevents shrinking | Add `min-width: 0` to all intermediate flex containers |

---

## 8 Performance, Accessibility & Browser Support

### Rendering Performance

- These standard Flexbox patterns trigger zero layout thrashing and compute in single-pass browser rendering cycles.
- Avoid placing hundreds of deeply nested flex containers inside real-time animation loops; use simple transforms for motion.

### Accessibility (a11y) Standards

- **DOM Sequence Consistency**: In split navbars, ensure that the "Profile / Sign In" button (pushed right via `margin-left: auto`) follows navigation links in logical DOM order so keyboard users tab through navigation before encountering user account actions.
- **Form Addons**: Icon prefixes in input groups should either be purely decorative (`aria-hidden="true"`) or properly labeled if interactive.

---

## 9 Comparison / When to Use What

| Pattern | Primary Mechanism | Why It Beats Alternatives |
| :--- | :--- | :--- |
| **Split Nav** | `margin-left: auto` | No empty spacer `<div>` required |
| **Card Bottom Button** | `margin-top: auto` | Zero hardcoded pixel heights needed |
| **Sticky Footer** | `flex: 1` on `<main>` | Eliminates brittle negative margin calculations |
| **Media Object** | `flex: 1; min-width: 0;` | Avoids clearfix float hacks |
| **Input Group** | `flex: 1` on `<input>` | Works seamlessly with any button padding |

---

## 10 Interview Questions

<details>
<summary><strong>1. How does <code>margin-left: auto</code> work to create a split navbar in Flexbox?</strong></summary>

In Flexbox, auto margins absorb all positive free space remaining along the active axis. When `margin-left: auto` is placed on a flex item in a horizontal row, it claims all unused horizontal space between itself and its preceding sibling, effectively pushing itself (and all subsequent siblings) flush against the far right edge of the container.
</details>

<details>
<summary><strong>2. How does the modern Flexbox Sticky Footer pattern work?</strong></summary>

The outer page shell is styled with `display: flex; flex-direction: column; min-height: 100vh;`. The `<header>` and `<footer>` have default flex sizing (`flex: 0 0 auto`). The `<main>` element has `flex: 1 0 auto;`, which instructs it to expand and absorb all remaining vertical space in the viewport. When page content is short, `<main>` stretches to push the `<footer>` to the very bottom of the screen; when content is tall, normal document expansion occurs.
</details>

<details>
<summary><strong>3. Why is <code>margin-top: auto</code> preferred over <code>position: absolute; bottom: 0;</code> for aligning buttons in cards?</strong></summary>

Using `position: absolute; bottom: 0;` removes the button from normal flow, meaning the card cannot calculate its height dynamically based on the button. If the description text grows long, it will overlap and collide with the absolute button. `margin-top: auto` inside a vertical flex column preserves the button in normal flow, expanding the card safely while pinning the button cleanly to the bottom.
</details>

<details>
<summary><strong>4. Why is <code>flex-shrink: 0</code> essential on avatars inside media objects?</strong></summary>

Flex items default to `flex-shrink: 1`. When neighboring body text contains long content or when the viewport narrows, the container contracts and forces shrinkable items to compress. Without `flex-shrink: 0`, the circular avatar's width will be squished, turning it into an oval.
</details>

<details>
<summary><strong>5. What is the Media Object pattern, and who originated it?</strong></summary>

The Media Object was coined by Nicole Sullivan (author of OOCSS) in 2010. It consists of a fixed media element (image, video, avatar) on one side and fluid content on the other, designed to be repeated hundreds of times (e.g. Facebook feed, Twitter tweets, comment threads) without breaking layout. Flexbox implements this pattern cleanly in 4 lines of CSS.
</details>

<details>
<summary><strong>6. How do you prevent borders from doubling up in an Input Group pattern?</strong></summary>

By setting `border-right: none;` on the leading prefix addon, and styling the middle input's borders so they share boundaries. Additionally, setting `z-index: 1;` on `:focus` ensures that the active focus ring sits on top of neighboring button borders.
</details>

<details>
<summary><strong>7. How does <code>gap</code> simplify the Tag Cloud pattern compared to older techniques?</strong></summary>

Previously, developers had to apply `margin: 0 8px 8px 0;` to tags and negative margins (`margin: 0 -8px -8px 0;`) to the container to offset outer edge spacing. `gap: 8px;` natively applies space only between tags, avoiding negative margin overflow hacks.
</details>

<details>
<summary><strong>8. Can Flexbox create a full-page Holy Grail layout?</strong></summary>

Yes, by nesting a horizontal flex row (`<aside>`, `<main>`, `<aside>`) inside a vertical flex column (`<header>`, `<div class="middle">`, `<footer>`). However, for entire page layouts with complex sidebar spans, CSS Grid is often preferred for cleaner two-dimensional layout definitions.
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Basic Media Object
- **Task**: Style a notification card with an unshrinking icon on the left and title + timestamp on the right.

<details>
<summary>View Solution</summary>

```css
.notification-card {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  padding: 16px;
  background: #ffffff;
  border-radius: 8px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.notification-icon {
  flex-shrink: 0;
  width: 24px;
  height: 24px;
}

.notification-body {
  flex: 1;
  min-width: 0;
}
```
</details>

### Exercise 2 (Easy): Card Bottom CTA Pin
- **Task**: Style a product card that pins the "Add to Cart" button to the bottom, regardless of title length.

<details>
<summary>View Solution</summary>

```css
.product-card {
  display: flex;
  flex-direction: column;
  height: 320px;
  padding: 20px;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
}

.product-cta-btn {
  margin-top: auto;
  padding: 10px;
  background-color: #16a34a;
  color: #ffffff;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}
```
</details>

### Exercise 3 (Medium): Sticky Footer Page Shell
- **Task**: Construct a full viewport HTML page layout where the footer is guaranteed to stay at the bottom of the screen.

<details>
<summary>View Solution</summary>

```css
body {
  margin: 0;
  display: flex;
  flex-direction: column;
  min-height: 100vh;
}

header {
  height: 60px;
  background-color: #0f172a;
}

main {
  flex: 1;
  padding: 24px;
}

footer {
  height: 80px;
  background-color: #1e293b;
  color: #ffffff;
}
```
</details>

### Exercise 4 (Medium): Split Navbar with Center Links
- **Task**: Create a navbar with a logo on the left, 3 centered navigation links, and a login button on the right.

<details>
<summary>View Solution</summary>

```css
.navbar {
  display: flex;
  align-items: center;
  padding: 12px 24px;
}

.nav-links {
  display: flex;
  gap: 16px;
  margin-left: auto;
  margin-right: auto; /* Centers the navigation links */
}

.nav-login {
  /* Remains on the right edge */
}
```
</details>

### Exercise 5 (Hard): Input Group with Icon Prefix and Clear Button
- **Task**: Build an input group containing a magnifying glass prefix, fluid input field, and clear button suffix.

<details>
<summary>View Solution</summary>

```css
.input-addon-group {
  display: flex;
  align-items: center;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
  padding: 4px 8px;
  max-width: 360px;
}

.input-addon-group:focus-within {
  border-color: #2563eb;
  box-shadow: 0 0 0 2px rgba(37, 99, 235, 0.2);
}

.input-icon-prefix {
  flex-shrink: 0;
  margin-right: 8px;
}

.input-search-text {
  flex: 1;
  min-width: 0;
  border: none;
  outline: none;
}

.input-search-text:focus-visible {
  outline: none; /* Container .input-addon-group:focus-within provides high-contrast ring */
}

.input-clear-btn {
  flex-shrink: 0;
  background: transparent;
  border: none;
  cursor: pointer;
}
```
</details>

### Exercise 6 (Hard): Wrapping Chip Filter Cloud with Clear-All Button
- **Task**: Create a filter container that wraps active filter chips and anchors a "Clear All" link at the end of the line.

<details>
<summary>View Solution</summary>

```css
.filter-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  padding: 12px;
  background: #f8fafc;
  border-radius: 8px;
}

.filter-chip {
  padding: 4px 10px;
  background: #e2e8f0;
  border-radius: 9999px;
  font-size: 0.875rem;
}

.clear-all-link {
  margin-left: auto; /* Pushes to end of line when space permits */
  color: #2563eb;
  font-weight: 600;
  font-size: 0.875rem;
  text-decoration: none;
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Pattern Recipes */

/* 1. Media Object */
.media-container { display: flex; gap: 16px; }
.media-img       { flex-shrink: 0; }
.media-body      { flex: 1; min-width: 0; }

/* 2. Sticky Footer */
.page-shell { display: flex; flex-direction: column; min-height: 100vh; }
.page-main  { flex: 1; }

/* 3. Split Navbar */
.nav-end-item { margin-left: auto; }

/* 4. Bottom Button in Card */
.card-btn { margin-top: auto; }

/* 5. Fluid Input Group */
.input-group       { display: flex; }
.input-group input { flex: 1; min-width: 0; }
```

---

## 13 Further Reading

- [MDN Web Docs: Typical use cases of Flexbox](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_flexible_box_layout/Typical_use_cases_of_flexbox)
- [Nicole Sullivan: The Media Object Pattern](http://www.stubbornella.org/content/2010/06/25/the-media-object-saves-hundreds-of-lines-of-code/)
- [web.dev: Modern CSS Solutions for Common Patterns](https://web.dev/)
- [CSS-Tricks: Solved by Flexbox](https://philipwalton.github.io/solved-by-flexbox/)

---

[Previous: 06-flexbox-sizing-algorithm-deep-dive.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/06-flexbox-sizing-algorithm-deep-dive.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/README.md) | [Next: 08-flexbox-pitfalls.md](file:///e:/Learning/engineering-notes/01-languages/css/03-flexbox/08-flexbox-pitfalls.md)
