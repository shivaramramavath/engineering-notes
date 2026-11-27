# 06. Touch Targets and Hit Areas

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | WCAG 2.2 Level AA (SC 2.5.8 Target Size) & Level AAA (SC 2.5.5) / Platform Guidelines |
| **Browsers Tested** | Chrome 120+, Firefox 125+, Safari 17.4+ (Mobile & Desktop) |
| **Tested With** | iOS Safari Touch, Android Chrome Pointer, DevTools Touch Emulation |
| **Prerequisites** | `02-box-model-and-layout-basics/01-box-model.md`, `11-accessibility-and-ux-in-css/01-focus-visible-and-focus-styles.md` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. Practical Examples
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

### The Problem
Touchscreens and mobile pointer devices lack the single-pixel precision of a desktop hardware mouse cursor. A human fingertip contacting a touchscreen glass surface creates an elliptical contact patch ranging between **8mm to 12mm** in diameter.

When interactive controls (such as close icons, pagination links, checkboxes, or tag filters) are rendered with tiny bounding boxes (e.g. $16\times 16\text{px}$), users with motor tremors, Parkinson's disease, arthritis, large fingers, or users walking while operating mobile phones experience high error rates. They accidentally tap adjacent links or fail to trigger targets altogether, causing immense frustration and task abandonment.

### The Solution
Web accessibility standards and mobile platform design guidelines establish strict mathematical minimums for interactive hit areas:
- **WCAG 2.2 SC 2.5.8 Target Size (Minimum) (Level AA)**: Targets must have a minimum interactive bounding box of **$24\times 24$ CSS pixels**, or provide adequate spacing offsets.
- **WCAG 2.1 SC 2.5.5 Target Size (Enhanced) (Level AAA)**: Targets must measure at least **$44\times 44$ CSS pixels**.
- **Platform Guidelines**: Apple iOS requires $44\times 44\text{pt}$; Google Android Material Design recommends $48\times 48\text{dp}$.
- **Decoupling Visual Size from Hit Area**: In modern CSS, an element's *visual appearance* (e.g. a 16px pencil icon) can remain sleek and compact while its *interactive touch target* expands to $44\times 44\text{px}$ or $48\times 48\text{px}$ using pseudo-elements (`::before`) or transparent padding.

---

## 2. Core Concepts

### 1. Visual Target vs. Interactive Hit Area

```
VISUAL TARGET (16px x 16px Close Icon)
            ┌──┐
            │✕ │ ◄── Visual Icon (16px)
            └──┘

EXPANDED INTERACTIVE HIT AREA (44px x 44px)
   ┌─────────────────────────────────────────┐
   │                                         │
   │               ┌──┐                      │
   │               │✕ │                      │ ◄── 44px x 44px Hit Target
   │               └──┘                      │     (Created via ::before)
   │                                         │
   └─────────────────────────────────────────┘
```

An interactive target does not need to look large; it only needs to *act* large. By separating visual geometry from click/touch hit detection, designers preserve delicate UI aesthetics while engineering robust accessibility.

### 2. WCAG Target Size Hierarchy

| Standard / Guideline | Minimum Target Dimensions | Conformance Level | Key Exceptions |
| :--- | :--- | :--- | :--- |
| **WCAG 2.2 SC 2.5.8** | **$24 \times 24$ CSS px** | Level AA | Inline links in a sentence, browser native controls, spacing offset |
| **WCAG 2.1 SC 2.5.5** | **$44 \times 44$ CSS px** | Level AAA | Essential presentation, inline text links |
| **Apple iOS HIG** | **$44 \times 44$ pt** | Platform Guideline | System icons, tab bars, navigation bars |
| **Google Material 3** | **$48 \times 48$ dp** | Platform Guideline | Buttons, chips, icon buttons, checkboxes |

### 3. Pointer Media Queries
CSS Media Queries Level 4 allows querying the precision of the primary pointing device:
- `@media (pointer: coarse)`: Matches touchscreens, stylus interfaces, and game controllers.
- `@media (pointer: fine)`: Matches mice, trackpads, and precision drawing tablets.

---

## 3. How It Works Internally

### Hit Testing and the Event Dispatch Pipeline
When a user touches a glass screen:
1. **Contact Ellipse Detection**: The hardware digitizer calculates the center coordinates $(X, Y)$ and radius of the contact patch.
2. **Hit Testing (Blink / WebKit / Gecko)**: The browser executes a hit-test traversal down the Render Tree from top to bottom (highest `z-index` first).
3. **Bounding Box Intersection**: Any node whose hit area (including pseudo-elements like `::before` and `::after` that have `pointer-events: auto`) encompasses the touch coordinates is registered as a hit candidate.
4. **Pointer Event Dispatch**: The browser fires `pointerdown`, `touchstart`, `pointerup`, `touchend`, and `click` to the matched target.

---

## 4. Syntax & Usage

### 1. The Pseudo-Element Touch Target Expansion Pattern
The most robust modern CSS technique to expand hit areas without disrupting parent Flexbox or Grid layouts:

```css
.icon-button {
  position: relative; /* Establishes coordinate anchor */
  width: 1.5rem;      /* Visual width: 24px */
  height: 1.5rem;     /* Visual height: 24px */
  padding: 0;
  border: none;
  background: transparent;
  cursor: pointer;
}

/* Invisible hit area expansion */
.icon-button::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  min-width: 44px;    /* Expands touch target to WCAG AAA minimum */
  min-height: 44px;
  width: 100%;
  height: 100%;
}
```

### 2. Targeting Touchscreens with Pointer Media Queries
Provide compact padding for desktop mouse users, and automatically expand to comfortable touch targets on mobile touchscreens:

```css
/* Base desktop compact size */
.nav-tab {
  padding: 0.375rem 0.75rem;
  font-size: 0.875rem;
}

/* Coarse touch devices: expand hit areas */
@media (pointer: coarse) {
  .nav-tab {
    padding: 0.75rem 1.25rem;
    min-height: 48px; /* Google Material recommended */
    display: flex;
    align-items: center;
  }
}
```

---

## 5. Practical Examples

### Example 1: Accessible Close Button in Modal Dialog
Modal close buttons (`✕`) are frequently placed in the top right corner. Developers often style them as tiny 16px text nodes, resulting in users accidentally clicking the modal backdrop or content underneath:

```html
<div class="modal-card">
  <button type="button" class="modal-close" aria-label="Close dialog">
    <svg class="modal-close__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">
      <path stroke-linecap="round" stroke-linejoin="round" d="M6 18L18 6M6 6l12 12" />
    </svg>
  </button>
  <h3>Account Settings</h3>
</div>
```

```css
.modal-card {
  position: relative;
  background: #ffffff;
  padding: 1.5rem;
  border-radius: 0.75rem;
}

.modal-close {
  position: absolute;
  top: 0.75rem;
  right: 0.75rem;
  width: 2rem;      /* 32px visual width */
  height: 2rem;     /* 32px visual height */
  display: flex;
  align-items: center;
  justify-content: center;
  background: #f1f5f9;
  border: 1px solid transparent;
  border-radius: 50%;
  color: #475569;
  cursor: pointer;
}

.modal-close__icon {
  width: 1.125rem;
  height: 1.125rem;
}

/* Expand touch target to 48px x 48px */
.modal-close::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 48px;
  height: 48px;
}

.modal-close:hover {
  background: #e2e8f0;
  color: #0f172a;
}

.modal-close:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```

### Example 2: Checkboxes and Radio Buttons with Expanded Hit Areas
Native checkboxes are notoriously tiny ($13\times 13\text{px}$ to $16\times 16\text{px}$). Enclosing the `<input>` and label inside an accessible container expands the hit area to the full label row:

```html
<label class="touch-checkbox">
  <input type="checkbox" class="touch-checkbox__input">
  <span class="touch-checkbox__control" aria-hidden="true"></span>
  <span class="touch-checkbox__text">Enable Two-Factor Authentication</span>
</label>
```

```css
.touch-checkbox {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  /* Entire row is clickable, achieving > 44px vertical touch target */
  min-height: 44px;
  padding: 0.25rem 0.5rem;
  cursor: pointer;
  border-radius: 0.375rem;
  transition: background-color 0.15s ease;
}

.touch-checkbox:hover {
  background-color: #f8fafc;
}

.touch-checkbox__input {
  /* Visually hide input while keeping it keyboard accessible */
  position: absolute;
  opacity: 0;
  width: 0;
  height: 0;
}

.touch-checkbox__control {
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid #64748b;
  border-radius: 0.25rem;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}

.touch-checkbox__input:checked + .touch-checkbox__control {
  background-color: #2563eb;
  border-color: #2563eb;
}

.touch-checkbox__input:focus-visible ~ .touch-checkbox__control {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```

### Example 3: Mobile Bottom Navigation Bar
Bottom tabs on mobile devices must meet the $48\times 48\text{px}$ minimum to prevent fat-finger selection errors:

```html
<nav class="bottom-nav" aria-label="Mobile Navigation">
  <a href="#feed" class="bottom-nav__item is-active">
    <svg class="bottom-nav__icon" viewBox="0 0 24 24" fill="currentColor"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg>
    <span class="bottom-nav__label">Home</span>
  </a>
  <a href="#search" class="bottom-nav__item">
    <svg class="bottom-nav__icon" viewBox="0 0 24 24" fill="currentColor"><path d="M15.5 14h-.79l-.28-.27A6.471 6.471 0 0 0 16 9.5 6.5 6.5 0 1 0 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z"/></svg>
    <span class="bottom-nav__label">Search</span>
  </a>
</nav>
```

```css
.bottom-nav {
  position: fixed;
  bottom: 0;
  left: 0;
  right: 0;
  display: flex;
  background-color: #ffffff;
  border-top: 1px solid #e2e8f0;
  /* Support iOS safe area home indicator */
  padding-bottom: env(safe-area-inset-bottom, 0);
  z-index: 100;
}

.bottom-nav__item {
  flex: 1;
  /* Guarantees at least 48px height for touch target */
  min-height: 52px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-decoration: none;
  color: #64748b;
  gap: 0.25rem;
}

.bottom-nav__item.is-active {
  color: #0284c7;
}

.bottom-nav__icon {
  width: 1.5rem;
  height: 1.5rem;
}

.bottom-nav__label {
  font-size: 0.6875rem;
  font-weight: 600;
}

.bottom-nav__item:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: -2px;
}
```

### Example 4: Pagination Links Spacing Guard (Preventing Overlap)
When targets expand via pseudo-elements, adjacent targets must not overlap:

```css
.pagination {
  display: flex;
  gap: 0.5rem; /* Ensures expanded touch areas do not collide */
  list-style: none;
  padding: 0;
}

.pagination__link {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 2rem;
  height: 2rem;
  border-radius: 0.375rem;
  color: #334155;
  text-decoration: none;
  font-weight: 600;
  font-size: 0.875rem;
}

/* 44px touch target via pseudo-element */
.pagination__link::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 44px;
  height: 44px;
}

.pagination__link.is-active {
  background-color: #2563eb;
  color: #ffffff;
}
```

---

## 6. Best Practices

1. **Target 44px to 48px Minimums**: Strive for $44\times 44\text{px}$ (WCAG AAA / iOS) or $48\times 48\text{px}$ (Android) on all primary touch buttons, tabs, and form controls.
2. **Never Settle for Under 24px (WCAG 2.2 Level AA)**: Ensure no interactive control has a bounding box smaller than $24\times 24\text{px}$ unless it is an inline link within a text sentence.
3. **Use Pseudo-Elements to Decouple Visuals from Targets**: Apply `::before` with `min-width: 44px; min-height: 44px;` so small visual icons (like 16px stars or arrows) have generous touch areas.
4. **Enforce Spacing Between Interactive Elements**: Always provide at least $8\text{px}$ of `gap` or `margin` between adjacent targets to prevent accidental touches.
5. **Honor `env(safe-area-inset-bottom)` on Mobile**: Ensure bottom navigation bars and floating buttons account for the iPhone home indicator bar so touch areas are not obstructed.
6. **Leverage `@media (pointer: coarse)`**: Adapt padding and hit areas specifically for touch-first environments without bloating desktop mouse layouts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **Clustering tiny action buttons without gaps** | Tapping "Delete" accidentally triggers "Edit" due to fat-finger contact. | Provide minimum 8px gap and 44px touch targets. |
| **Overlapping expanded `::before` pseudo-elements** | Top target steals pointer clicks from bottom target. | Use parent container `gap` or check bounding box layout in DevTools. |
| **Applying touch expansion to inline paragraph links** | Expanded pseudo-elements on multiple inline links cover each other and break text selection. | Inline links inside running sentences are exempt from target size minimums. |
| **Using `pointer-events: none` on interactive parents** | Disables all hit-testing for the button and child elements. | Only use `pointer-events: none` on decorative overlay badges. |
| **Relying solely on visual size instead of hit testing** | Designing a 44px button where only the 14px inner text is clickable. | Ensure the `<button>` or `<a>` itself occupies the full 44px dimension. |
| **Ignoring mobile viewport zoom restrictions** | Small targets are unclickable and users cannot zoom in if `user-scalable=no` was set. | Never disable viewport zoom; allow pinch-to-zoom. |

---

## 8. Performance, Accessibility & Browser Support

### Universal Support
- Pseudo-element target expansion (`::before`, `::after`) is universally supported in all browsers dating back to IE8.
- `@media (pointer: coarse)` is supported across all modern mobile and desktop browsers (Chrome 41+, Safari 9+, Firefox 64+).

### DevTools Touch Testing
1. In Chrome / Firefox DevTools, click the **Toggle Device Toolbar** icon (`Cmd+Shift+M` / `Ctrl+Shift+M`).
2. Select a mobile device preset (e.g. iPhone 14 or Pixel 7).
3. The cursor converts into a circular touch patch simulator. Inspect elements to measure their computed `clientWidth` and `clientHeight`.

---

## 9. Comparison / When to Use What

| Technique | Layout Disruption | Visual Footprint | Best Suited For |
| :--- | :--- | :--- | :--- |
| **`::before` Pseudo-Element** | Zero (Floats above/behind) | Unchanged (Sleek visual icon) | Icon buttons, close icons, table actions |
| **Direct CSS `padding`** | Expands container box | Visually expands button | Standard primary & secondary buttons |
| **`min-height: 48px`** | Enforces vertical bounds | Larger physical control | Mobile navigation bars, list items |
| **Pointer Media Queries** | Responsive to hardware | Compact on desktop, large on touch | Multi-platform design systems |

---

## 10. Interview Questions

### Q1: What is the primary difference between WCAG 2.1 SC 2.5.5 (Target Size) and WCAG 2.2 SC 2.5.8 (Target Size Minimum)?
<details>
<summary>Answer</summary>
- **WCAG 2.1 SC 2.5.5 (Level AAA)**: Established the enhanced target size standard of at least **$44 \times 44$ CSS pixels** for pointer inputs, with exceptions for inline links and essential presentation.
- **WCAG 2.2 SC 2.5.8 (Level AA)**: Introduced a mandatory, lower-threshold criteria at Level AA requiring targets to be at least **$24 \times 24$ CSS pixels**, or satisfy an spacing offset condition (where a 24px diameter circle centered on each target does not intersect with any adjacent target or its spacing circle).
</details>

### Q2: How can an author ensure an icon button with a visual size of $18\times 18\text{px}$ passes WCAG Level AAA ($44\times 44\text{px}$) without changing its visual appearance?
<details>
<summary>Answer</summary>
By utilizing an invisible pseudo-element (`::before` or `::after`) to expand the interactive hit area:
```css
.icon-btn {
  position: relative;
  width: 18px;
  height: 18px;
}

.icon-btn::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 44px;
  height: 44px;
}
```
The icon appears visually as an 18px graphic, but the browser's hit-test engine detects clicks anywhere within the $44\times 44\text{px}$ invisible pseudo-element bounding box.
</details>

### Q3: Why are inline links within a sentence of running text exempt from WCAG Target Size criteria?
<details>
<summary>Answer</summary>
If inline links were forced to have a $44\times 44\text{px}$ or $24\times 24\text{px}$ bounding box, the line height of paragraphs containing links would explode, creating giant vertical gaps between lines and severely degrading readability for all users. Furthermore, if two inline links appeared adjacent to each other in a sentence, their expanded hit areas would overlap, making it impossible to click the intended link. Consequently, WCAG explicitly exempts targets that are inline in a sentence or block of text.
</details>

### Q4: How does the CSS `@media (pointer: coarse)` media query help optimize touch targets?
<details>
<summary>Answer</summary>
`@media (pointer: coarse)` queries whether the user's primary pointing device is of limited accuracy (such as a finger on a touchscreen or a remote control).

Developers can design compact, high-density interfaces for desktop mouse users (`padding: 4px 8px; font-size: 13px`), while using `@media (pointer: coarse)` to automatically inflate padding and enforce $44\text{px}$ or $48\text{px}$ minimum heights for mobile touch users without maintaining separate stylesheets or user-agent sniffing scripts.
</details>

### Q5: What is the "Spacing Exception" in WCAG 2.2 Success Criterion 2.5.8?
<details>
<summary>Answer</summary>
SC 2.5.8 allows targets smaller than $24\times 24\text{px}$ if the target provides sufficient spacing to ensure it cannot be accidentally triggered alongside an adjacent target.

Specifically: if a 24px diameter circle is drawn around the bounding box of target A, and a 24px diameter circle is drawn around target B, neither circle may intersect the other target's bounding box. This prevents accidental multi-activation even if the visual targets are under 24px.
</details>

### Q6: Why should bottom-anchored mobile action buttons account for `env(safe-area-inset-bottom)`?
<details>
<summary>Answer</summary>
On modern mobile devices (such as iPhones without physical home buttons and modern Android devices), the operating system places a persistent gesture navigation bar (home indicator) at the bottom of the display.

If a web application fixes a navigation bar or button directly to `bottom: 0`, the hit target overlaps with the OS gesture indicator. When users tap the button, the OS often intercepts the touch as a "swipe to home" gesture, causing high failure rates. Adding `padding-bottom: env(safe-area-inset-bottom)` elevates the interactive area above the system gesture zone.
</details>

### Q7: What issue occurs when multiple adjacent targets use pseudo-element touch expansions without adequate layout gaps?
<details>
<summary>Answer</summary>
If two adjacent buttons (e.g. Edit and Delete in a table row) each use an expanded `::before` pseudo-element of $44\times 44\text{px}$ but are visually separated by only $4\text{px}$, their invisible pseudo-elements will physically overlap in the DOM layout.

Because the element later in source order (or with higher `z-index`) stacks above the earlier element, clicking near the edge of the first button will actually trigger the second button, causing catastrophic user errors (such as accidentally clicking Delete when trying to click Edit). Authors must enforce a layout `gap` equal to or greater than the expanded perimeter.
</details>

### Q8: How can entire card elements be made clickable while maintaining accessible keyboard navigation and text selection?
<details>
<summary>Answer</summary>
Use the "Stretched Link" pattern:
1. Place a semantic `<a>` link around the card's heading text.
2. Set `position: relative` on the card container.
3. Apply `position: absolute; inset: 0;` to `a::after`.
```css
.card { position: relative; }
.card__title a::after {
  content: "";
  position: absolute;
  inset: 0;
}
```
The entire card becomes a giant, easy-to-tap hit target for touch users, while screen readers and keyboard users encounter clean, semantic heading links.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Sleek 16px Bookmark Icon with a 44px Hit Area
**Objective**: Construct an icon button that renders visually as a 16px SVG bookmark, but provides a 44px by 44px hit target centered on the icon, with visible `:focus-visible` styling.

<details>
<summary>Solution</summary>

```html
<button type="button" class="bookmark-btn" aria-label="Bookmark article">
  <svg class="bookmark-btn__icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">
    <path stroke-linecap="round" stroke-linejoin="round" d="M5 5a2 2 0 012-2h10a2 2 0 012 2v16l-7-3.5L5 21V5z" />
  </svg>
</button>
```

```css
.bookmark-btn {
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 1.5rem;    /* 24px visual bounds */
  height: 1.5rem;
  padding: 0;
  background: transparent;
  border: none;
  color: #4b5563;
  cursor: pointer;
  border-radius: 4px;
}

.bookmark-btn__icon {
  width: 1rem;      /* 16px visual icon */
  height: 1rem;
}

/* 44px x 44px Touch Target */
.bookmark-btn::before {
  content: "";
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 44px;
  height: 44px;
}

.bookmark-btn:hover {
  color: #111827;
}

.bookmark-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
}
```
</details>

### Exercise 2: Responsive Floating Action Button with Safe-Area Padding
**Objective**: Build a mobile Floating Action Button (FAB) that provides a minimum $56\times 56\text{px}$ touch target and respects mobile safe-area insets.

<details>
<summary>Solution</summary>

```html
<button type="button" class="app-fab" aria-label="Compose new message">
  <svg class="app-fab__icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
    <path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34a.9959.9959 0 00-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/>
  </svg>
</button>
```

```css
.app-fab {
  position: fixed;
  bottom: calc(1rem + env(safe-area-inset-bottom, 0px));
  right: calc(1rem + env(safe-area-inset-right, 0px));
  width: 3.5rem;     /* 56px - Material Design FAB standard */
  height: 3.5rem;
  border-radius: 50%;
  background-color: #0284c7;
  color: #ffffff;
  border: none;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  cursor: pointer;
  z-index: 99;
}

.app-fab__icon {
  width: 1.5rem;
  height: 1.5rem;
}

.app-fab:focus-visible {
  outline: 3px solid #0284c7;
  outline-offset: 3px;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 2.2 SC 2.5.8 (Level AA)**: Minimum target size of **$24 \times 24$ CSS px**.
- **WCAG 2.1 SC 2.5.5 (Level AAA)**: Enhanced target size of **$44 \times 44$ CSS px**.
- **Mobile Guidelines**: Apple iOS $44\times 44\text{pt}$; Android Material $48\times 48\text{dp}$.
- **Pseudo-Element Pattern**: Expand hit areas invisibly using `::before` (`min-width: 44px; min-height: 44px; position: absolute; transform: translate(-50%, -50%);`).
- **Gaps**: Always maintain at least $8\text{px}$ gap between targets to prevent pseudo-element collisions.
- **Pointer Queries**: Use `@media (pointer: coarse)` to expand hit areas selectively on touch devices.
- **Safe Areas**: Use `env(safe-area-inset-bottom)` to keep mobile hit targets clear of system navigation bars.

---

## 13. Further Reading
- [W3C WCAG 2.2: Understanding Success Criterion 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
- [W3C WCAG 2.1: Understanding Success Criterion 2.5.5 Target Size](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Apple Human Interface Guidelines: Layout & Target Sizes](https://developer.apple.com/design/human-interface-guidelines/layout)
- [Google Material Design 3: Touch Targets](https://m3.material.io/foundations/accessible-design/accessibility-basics#054366b5-0219-4f7f-aa4e-c90d0b0059e0)
- [Ahmad Shadeed: Enhancing Touch Targets with CSS](https://ishadeed.com/article/target-size/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [05. Zoom, Reflow and Text Spacing](05-zoom-reflow-and-text-spacing.md) | [07. Visually Hidden Content Patterns](07-visually-hidden-content-patterns.md) |
