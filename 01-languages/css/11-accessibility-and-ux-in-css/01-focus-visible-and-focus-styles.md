# 01. Focus Visible and Focus Styles

| Metadata | Details |
| :--- | :--- |
| **Spec Level & Status** | Selectors Level 4 / W3C Recommendation / WCAG 2.2 AA & AAA |
| **Browsers Tested** | Chrome 86+, Firefox 85+, Safari 15.4+ (Universal Baseline) |
| **Tested With** | Modern DevTools, NVDA, VoiceOver, Keyboard Navigation |
| **Prerequisites** | `01-fundamentals/03-pseudo-classes.md`, `10-architecture-and-methodologies/01-css-architecture-principles.md` |

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
For keyboard-only users, individuals with motor disabilities, switch-device operators, and power users navigating via the `Tab` key, the browser's visual focus indicator is their only visual compass on the screen. If an element receives programmatic or keyboard focus without a prominent visual indicator, the user becomes instantly disoriented, unable to determine where interactive commands (e.g. `Enter` or `Space`) will be executed.

Historically, designers disliked the default browser focus ring appearing on mouse clicks, leading developers to implement the catastrophic anti-pattern:
```css
/* ANTI-PATTERN: Stripping focus blindly (NEVER DO THIS ALONE) */
*:focus {
  outline: none;
}

/* CORRECT PATTERN: Always pair with a high-contrast :focus-visible replacement */
*:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
}
```
This stripped focus indicators for keyboard navigators, causing critical accessibility failures and locking millions of users out of web applications.

### The Solution
The modern CSS pseudo-class `:focus-visible` solves this problem at the browser engine level:
- It selectively applies focus styling **only when the user is operating via keyboard or assistive modalities**, or when interacting with elements where focus is always needed (like text input fields).
- When a user clicks a button with a mouse or taps with a touchscreen, `:focus-visible` does not match, eliminating unwanted click rings without sacrificing keyboard accessibility.
- Pair `:focus-visible` with WCAG 2.2 compliant focus indicators (`outline`, `outline-offset`, double-ring contrast technique) to ensure focus rings are visible against any background.

---

## 2. Core Concepts

### 1. `:focus` vs `:focus-visible` Heuristic Engine

```
USER ACTION                        ELEMENT TYPE       :focus MATCHES?   :focus-visible MATCHES?
─────────────────────────────────────────────────────────────────────────────────────────────────
Click with Mouse                   <button>           YES               NO (Clean UI!)
Tab via Keyboard                   <button>           YES               YES (Accessible!)
Click with Mouse                   <input type="text">YES               YES (Typing caret needed)
Tap on Touchscreen                 <a>                YES               NO
Programmatic focus() via JS        <dialog>           YES               YES (Usually keyboard intent)
```

1. **`:focus`**: Matches whenever an element holds focus, regardless of the user's input modality (mouse, keyboard, stylus, touch).
2. **`:focus-visible`**: Matches only when the browser engine's heuristic determines that a visual indicator is necessary to assist the user.
3. **`:focus-within`**: Matches a container element if the container itself *or any of its descendants* currently matches `:focus` or `:focus-visible`.

### 2. WCAG 2.2 Focus Criteria
WCAG 2.2 introduced strict criteria governing focus visibility:
- **SC 2.4.7 Focus Visible (Level A)**: Any keyboard-operable interface must have a visible focus indicator.
- **SC 2.4.11 Focus Not Obscured (Minimum) (Level AA)**: When an item receives focus, it must not be entirely hidden behind author-created sticky headers, banners, or floating overlays.
- **SC 2.4.13 Focus Appearance (Level AAA)**: The focus indicator must:
  1. Have an area at least as large as a **2-pixel solid perimeter** of the unfocused component.
  2. Have a **contrast ratio of at least 3:1** between the focused and unfocused states, AND at least 3:1 against adjacent background colors.

---

## 3. How It Works Internally

### Browser Focus Heuristics
Modern browser rendering engines (Chromium Blink, Mozilla Gecko, Apple WebKit) implement an internal input tracking state machine:

```
┌────────────────────────────────────────────────────────┐
│             BROWSER MODALITY ENGINE                    │
└────────────────────────────────────────────────────────┘
                       │
        User interacts with page
                       │
       ┌───────────────┴───────────────┐
       ▼                               ▼
  [Keydown Event]             [Pointerdown Event]
  (Tab, Arrow keys)           (Mouse, Touch, Stylus)
       │                               │
       ▼                               ▼
  Internal Flag:              Internal Flag:
  hadKeyboardEvent = true     hadKeyboardEvent = false
       │                               │
       └───────────────┬───────────────┘
                       │
               Element focused
                       │
         Is element a text-editing field?
                       │
        ┌──────────────┴──────────────┐
       YES                            NO
        │                              │
        ▼                              ▼
  :focus-visible = true       Did hadKeyboardEvent == true?
                                       │
                                ┌──────┴──────┐
                               YES            NO
                                │              │
                                ▼              ▼
                      :focus-visible = true  :focus-visible = false
```

---

## 4. Syntax & Usage

### 1. The Universal Modern Reset Pattern
Safely remove mouse click rings while enforcing high-contrast keyboard focus indicators across all interactive elements:

```css
/* 1. Remove default mouse outline only when :focus-visible is supported */
:where(a, button, input, textarea, select, [tabindex]):focus:not(:focus-visible) {
  outline: none;
}

/* 2. Enforce standard, high-contrast focus indicator on keyboard focus */
:where(a, button, input, textarea, select, [tabindex]):focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 3px;
  border-radius: 4px;
}
```

### 2. The Infallible Double-Ring Technique
A single-color focus ring can vanish if an element sits on a background of the exact same hue (e.g. a blue outline on a blue banner). The **double-ring technique** uses an outline combined with an inner `box-shadow` (or multi-layered outline) to guarantee a 3:1 contrast ratio against *any* background (white, black, or brand color).

```css
.c-interactive:focus-visible {
  /* Outer outline: Bright white / light tone */
  outline: 2px solid #ffffff;
  outline-offset: 2px;
  /* Inner shadow: Pitch black ring */
  box-shadow: 0 0 0 4px #0f172a;
}
```

### 3. Container Highlight with `:focus-within`
Highlight an entire card, form fieldset, or search box when any child input receives focus:

```css
.search-form {
  border: 1px solid #d1d5db;
  border-radius: 0.5rem;
  padding: 0.25rem 0.5rem;
  transition: border-color 0.15s ease, box-shadow 0.15s ease;
}

/* When the inner <input> or <button> is focused */
.search-form:focus-within {
  border-color: #2563eb;
  box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.25);
}
```

---

## 5. Practical Examples

### Example 1: Accessible Custom Checkbox with Focus Ring
Styling a hidden native checkbox without destroying keyboard focus visibility:

```html
<label class="custom-checkbox">
  <input type="checkbox" class="custom-checkbox__input">
  <span class="custom-checkbox__box" aria-hidden="true"></span>
  <span class="custom-checkbox__label">Receive security alerts via email</span>
</label>
```

```css
.custom-checkbox {
  display: inline-flex;
  align-items: center;
  gap: 0.75rem;
  cursor: pointer;
  user-select: none;
}

/* Hide native input visually but keep it fully accessible in DOM */
.custom-checkbox__input {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}

.custom-checkbox__box {
  width: 1.25rem;
  height: 1.25rem;
  border: 2px solid #6b7280;
  border-radius: 0.25rem;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background-color 0.15s ease, border-color 0.15s ease;
}

/* Checked state */
.custom-checkbox__input:checked + .custom-checkbox__box {
  background-color: #2563eb;
  border-color: #2563eb;
}

/* KEYBOARD FOCUS: When native input receives focus-visible, ring the custom box */
.custom-checkbox__input:focus-visible + .custom-checkbox__box {
  outline: 2px solid #2563eb;
  outline-offset: 3px;
  box-shadow: 0 0 0 5px rgba(37, 99, 235, 0.25);
}
```

### Example 2: Preventing Focus Obscuration (WCAG 2.2 SC 2.4.11)
When users navigate down a page with a sticky header, focusing an anchor link can cause the focused element to be completely obscured underneath the sticky header:

```css
/* Ensure the browser scrolls elements past sticky headers when focused */
html {
  /* Set scroll padding equal to the sticky header height + breathing room */
  scroll-padding-top: 5rem;
  scroll-behavior: smooth;
}

.site-header {
  position: sticky;
  top: 0;
  height: 4rem;
  z-index: 100;
  background: #ffffff;
  border-bottom: 1px solid #e5e7eb;
}

/* Focused elements will never be obscured beneath the sticky header */
:target, :focus-visible {
  scroll-margin-top: 5rem;
}
```

### Example 3: Expanding Animated Focus Indicator for Buttons
```css
.action-button {
  padding: 0.75rem 1.5rem;
  background-color: #10b981;
  color: #ffffff;
  border: none;
  border-radius: 0.5rem;
  font-weight: 600;
  cursor: pointer;
  position: relative;
}

.action-button:focus-visible {
  outline: 3px solid #059669;
  outline-offset: 4px;
  animation: pulse-ring 1.5s cubic-bezier(0.24, 0, 0.38, 1) infinite;
}

@keyframes pulse-ring {
  0% {
    outline-color: #059669;
    outline-offset: 2px;
  }
  50% {
    outline-color: #34d399;
    outline-offset: 6px;
  }
  100% {
    outline-color: #059669;
    outline-offset: 2px;
  }
}
```

### Example 4: Focus Indicators in Dark Mode via `light-dark()`
Ensuring focus rings remain high contrast across color scheme shifts:

```css
:root {
  color-scheme: light dark;
  --focus-ring-color: light-dark(#0284c7, #38bdf8);
  --focus-ring-bg: light-dark(#ffffff, #0f172a);
}

.interactive-element:focus-visible {
  outline: 2px solid var(--focus-ring-color);
  outline-offset: 2px;
}
```

### Example 5: High-Contrast Mode (`forced-colors`) Adaptation
In Windows Forced Colors Mode (High Contrast), custom shadows and background colors are removed by the OS. The focus indicator must leverage native `outline` or system colors:

```css
@media (forced-colors: active) {
  .custom-checkbox__input:focus-visible + .custom-checkbox__box,
  .action-button:focus-visible {
    /* Use Highlight system color to guarantee visibility in Windows High Contrast */
    outline: 3px solid Highlight;
    outline-offset: 3px;
  }
}
```

---

## 6. Best Practices

1. **Never Strip Outlines Globally**: Never write `outline: none` or `outline: 0` without immediately supplying a `:focus-visible` replacement rule with at least 3:1 contrast.
2. **Prefer `outline` Over `box-shadow` or `border` for Focus**: `outline` does not take up box-model space and will never cause layout shifts (reflow) when toggled.
3. **Always Set `outline-offset`**: Adding 2px to 4px of `outline-offset` separates the focus indicator from the element's border, preventing it from blending into the element's background color.
4. **Use Dual-Contrast Focus Rings**: Combine a light outer ring with a dark inner ring (or vice versa) to guarantee 3:1 contrast against both light and dark backgrounds.
5. **Set `scroll-padding-top` on Sticky Headers**: Prevent keyboard-focused elements from slipping behind fixed or sticky headers (WCAG 2.2 SC 2.4.11).
6. **Preserve Focus on Text Inputs on Click**: Never disable `:focus` or `:focus-visible` on `<input type="text">`, `<textarea>`, or `[contenteditable]`; users always need to see the active insertion point.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correct Pattern |
| :--- | :--- | :--- |
| **`button:focus { outline: none; }` without replacement** | Completely blinds keyboard navigators; severe WCAG Level A violation. | Use `button:focus:not(:focus-visible) { outline: none; }` paired with `:focus-visible`. |
| **Using `border` changes on focus (`border: 2px solid blue`)** | Element box model expands by 2px, pushing all adjacent sibling content (layout thrashing/CLS). | Use `outline` or `box-shadow`, which render outside document flow. |
| **Using low-contrast brand colors for focus rings** | Yellow outline on white or dark-grey outline on black fails the 3:1 contrast rule. | Test contrast ratio against both component and background; use double rings. |
| **Placing `overflow: hidden` on focusable parent containers** | Crops the child's `outline-offset`, cutting off the focus ring. | Use `overflow: clip` or ensure parent padding accommodates the focus ring offset. |
| **Forgetting `:focus-within` on custom composite widgets** | Parent container looks dead while user tabs through child radio or checkbox inputs. | Apply `:focus-within` to composite form cards or search containers. |
| **Ignoring Windows High Contrast Mode (`forced-colors`)** | Box-shadow based focus rings are discarded by the OS, leaving elements with no focus indicator. | Always include an `outline` property alongside `box-shadow`. |

---

## 8. Performance, Accessibility & Browser Support

### Browser Support Matrix
- **`:focus-visible`**: Universally supported across all modern browsers (Chrome 86+, Edge 86+, Firefox 85+, Safari 15.4+).
- **`:focus-within`**: Supported across all browsers since Chrome 60+, Safari 10.1+, Firefox 52+.
- **`outline-offset`**: Baseline widely available across all modern browsers.

### Performance Impact
- **Zero Reflow**: Because `outline` and `box-shadow` do not alter the element's geometry (unlike `margin` or `border`), activating a focus ring executes entirely in the **Paint** or **Composite** phase, guaranteeing 60/120 FPS buttery smooth keyboard tabbing.

---

## 9. Comparison / When to Use What

| Selector | Matches on Mouse Click? | Matches on Keyboard Tab? | Reflow Cost | Primary Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **`:focus`** | Yes | Yes | 0 (with outline) | Text inputs, forms, universal state |
| **`:focus-visible`** | No (except inputs) | Yes | 0 (with outline) | Buttons, links, custom toggles |
| **`:focus-within`** | Yes (if child has focus)| Yes (if child has focus)| 0 (with outline) | Form cards, search bars, navigation flyouts |
| **`:active`** | While mouse held down | While Space/Enter pressed | 0 | Depressed button micro-interaction |

---

## 10. Interview Questions

### Q1: What is the architectural difference between `:focus` and `:focus-visible` in modern browsers?
<details>
<summary>Answer</summary>
`:focus` is triggered unconditionally whenever an element receives focus in the DOM, regardless of the input device used (mouse, keyboard, touch, script).

`:focus-visible` incorporates an internal browser heuristic that evaluates the user's input modality. If the user interacted via keyboard navigation (e.g. `Tab` key) or if the element is inherently text-editable (like `<input type="text">`), `:focus-visible` matches. If the user clicked a non-text element (like `<button>` or `<a>`) with a pointer device, `:focus-visible` remains unmatched, preventing unwanted aesthetic focus outlines while preserving accessibility for keyboard users.
</details>

### Q2: Why does changing `border` on `:focus` cause performance and visual issues, and what should be used instead?
<details>
<summary>Answer</summary>
In the CSS box model, `border` is part of the element's layout geometry. If an element has no border (or a 1px border) and `:focus` introduces a 3px border, the element's outer dimensions expand.

This expansion forces the browser engine to perform an expensive **Layout (Reflow)** pass, shifting all adjacent sibling and descendant elements across the screen, which degrades Interaction to Next Paint (INP) and introduces visual jarring.
Instead, developers should use `outline` (with `outline-offset`) or `box-shadow`. Neither property affects box-model layout geometry; both paint directly in the Paint/Composite stage with zero reflow.
</details>

### Q3: How does WCAG 2.2 Success Criterion 2.4.13 (Focus Appearance) define compliance for focus indicators?
<details>
<summary>Answer</summary>
WCAG 2.2 SC 2.4.13 (Level AAA) establishes measurable geometric and contrast thresholds:
1. **Minimum Area**: The focus indicator's area must be at least as large as a 2-pixel thick solid perimeter of the unfocused component (or 4 times the shortest dimension).
2. **Contrast Ratio**:
   - The focused state must have a contrast ratio of at least **3:1** against the unfocused state.
   - The indicator must have a contrast ratio of at least **3:1** against adjacent colors/backgrounds.
</details>

### Q4: Explain the "Double-Ring Focus Technique" and why single-color focus indicators fail accessibility audits.
<details>
<summary>Answer</summary>
A single-color focus ring (e.g. solid blue `outline: 2px solid #2563eb`) relies on the surrounding background having adequate contrast with that specific color. If that element sits on a blue header or brand banner, the outline disappears completely, violating WCAG 2.4.7 (Level A).

The **Double-Ring Technique** combines two contrasting colors—typically a light tone (white) and a dark tone (black or deep blue)—either via an outline with `box-shadow` or layered multi-stop rings:
```css
.btn:focus-visible {
  outline: 2px solid #ffffff;
  outline-offset: 2px;
  box-shadow: 0 0 0 4px #0f172a;
}
```
Regardless of whether the element sits on a pure white, pitch black, or vibrant blue background, at least one of the two rings will exceed the mandatory 3:1 contrast ratio.
</details>

### Q5: How does WCAG 2.2 Success Criterion 2.4.11 (Focus Not Obscured) affect CSS layout architecture?
<details>
<summary>Answer</summary>
SC 2.4.11 (Level AA) requires that when an item receives keyboard focus, it must not be completely obscured by author-created sticky headers, floating footers, or non-modal overlays.

In CSS, if a page has a `position: sticky` or `position: fixed` header of 80px height, tabbing down the page will cause focused headings or links to scroll directly beneath the sticky header, blinding the user.
To solve this in CSS, developers must set `scroll-padding-top` on the root container (`html { scroll-padding-top: 5rem; }`) or set `scroll-margin-top: 5rem;` on focusable targets, directing the browser's viewport scroll positioning to stop above the element.
</details>

### Q6: What happens to `box-shadow` focus rings in Windows High Contrast Mode (`forced-colors`), and how do you safeguard against it?
<details>
<summary>Answer</summary>
In Windows High Contrast Mode (Forced Colors), the operating system strips author-defined `box-shadow`, `background-color`, and background images to enforce an accessible, restricted color palette.
If a developer implements focus rings *solely* using `box-shadow: 0 0 0 3px blue`, the user in High Contrast Mode will see zero focus indication.

To safeguard against this, always pair `box-shadow` with a transparent native `outline` or test in `@media (forced-colors: active)`:
```css
.btn:focus-visible {
  box-shadow: 0 0 0 3px #2563eb;
  outline: 2px solid transparent; /* Becomes visible Highlight in HCM */
}
```
The browser preserves `outline` in Forced Colors mode, automatically remapping `transparent` to the system's `Highlight` color.
</details>

### Q7: How can `:focus-within` be used to improve accessibility on complex data tables?
<details>
<summary>Answer</summary>
In large data tables with action buttons or editable inputs in each row, keyboard users can easily lose track of which row they are editing as they tab across columns.
Applying `:focus-within` to the `<tr>` element:
```css
tr:focus-within {
  background-color: #f1f5f9;
  outline: 2px solid #3b82f6;
  outline-offset: -2px;
}
```
provides an immediate, high-contrast visual container highlight that persists across every cell within that row, giving the user continuous context.
</details>

### Q8: What is the correct way to hide an interactive element from mouse users while keeping it accessible to keyboard users?
<details>
<summary>Answer</summary>
The "Skip to Main Content" link is the canonical example. It should remain visually off-screen by default, but snap immediately into full view when it receives keyboard focus:
```css
.skip-link {
  position: absolute;
  top: -9999px;
  left: 1rem;
  z-index: 9999;
  padding: 0.75rem 1.5rem;
  background: #000;
  color: #fff;
}

.skip-link:focus-visible {
  top: 1rem;
  outline: 3px solid #ffcc00;
}
```
Never use `display: none` or `visibility: hidden`, because those properties remove the element from the accessibility tree entirely, preventing it from ever receiving keyboard focus.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build an Infallible Multi-Theme Focus Ring Token
**Objective**: Construct a set of CSS Custom Properties that automatically configure a compliant 3:1 focus ring across light mode, dark mode, and high-contrast environments.

<details>
<summary>Solution</summary>

```css
:root {
  color-scheme: light dark;
  --focus-ring-width: 2px;
  --focus-ring-offset: 3px;
  --focus-ring-color: light-dark(#0284c7, #38bdf8);
  --focus-ring-contrast-shadow: light-dark(#ffffff, #0f172a);
}

.a11y-focus:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
  box-shadow: 0 0 0 calc(var(--focus-ring-offset) + var(--focus-ring-width)) var(--focus-ring-contrast-shadow);
  border-radius: 4px;
}

@media (forced-colors: active) {
  .a11y-focus:focus-visible {
    outline: 3px solid Highlight;
    outline-offset: 3px;
    box-shadow: none;
  }
}
```
</details>

### Exercise 2: Accessible Floating Action Button with Obscuration Guard
**Objective**: Build a fixed Floating Action Button (FAB) that does not obscure other focused interactive elements and displays an accessible focus ring when reached via Tab.

<details>
<summary>Solution</summary>

```html
<main id="main-content">
  <p>Interactive page content...</p>
  <button type="button" class="btn-primary">Standard Action</button>
</main>

<button type="button" class="fab" aria-label="Create New Document">
  <span aria-hidden="true">+</span>
</button>
```

```css
html {
  /* Guard against fixed elements obscuring targets */
  scroll-padding-bottom: 6rem;
}

.fab {
  position: fixed;
  bottom: 1.5rem;
  right: 1.5rem;
  width: 3.5rem;
  height: 3.5rem;
  border-radius: 9999px;
  background-color: #2563eb;
  color: #ffffff;
  border: none;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.75rem;
  cursor: pointer;
  z-index: 50;
  transition: transform 0.15s ease, background-color 0.15s ease;
}

.fab:hover {
  background-color: #1d4ed8;
  transform: scale(1.05);
}

.fab:focus-visible {
  outline: 3px solid #2563eb;
  outline-offset: 4px;
  box-shadow: 0 0 0 6px #ffffff, 0 4px 6px -1px rgba(0, 0, 0, 0.2);
}

.fab:focus:not(:focus-visible) {
  outline: none;
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Golden Rule**: Never write `outline: none` without providing a replacement `:focus-visible` rule.
- **`:focus-visible`**: The gold standard for modern focus rings; only matches on keyboard navigation and text inputs.
- **`:focus-within`**: Highlights parent containers when any child element receives focus.
- **No Reflow**: Always use `outline` and `outline-offset` rather than `border` to prevent layout reflow during navigation.
- **Double Ring**: Combine light outline + dark box-shadow to guarantee 3:1 contrast against any background color.
- **Scroll Padding**: Use `scroll-padding-top` on `html` to prevent sticky headers from obscuring focused elements (WCAG 2.2 SC 2.4.11).
- **Forced Colors**: Always include a native `outline` (can be `transparent`) so Windows High Contrast Mode can render `Highlight` outlines.

---

## 13. Further Reading
- [W3C Selectors Level 4: The :focus-visible Pseudo-class](https://www.w3.org/TR/selectors-4/#the-focus-visible-pseudo)
- [W3C WCAG 2.2: Understanding Success Criterion 2.4.13 Focus Appearance](https://www.w3.org/WAI/WCAG22/Understanding/focus-appearance.html)
- [W3C WCAG 2.2: Understanding Success Criterion 2.4.11 Focus Not Obscured (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/focus-not-obscured-minimum.html)
- [web.dev: Focus visible guide](https://web.dev/focus-visible-guide/)
- [Sara Soueidan: A Guide To Designing Accessible, WCAG-Compliant Focus Indicators](https://www.sarasoueidan.com/blog/focus-indicators/)

---

| Previous Note | Next Note |
| :--- | :--- |
| [README.md](README.md) | [02. Colour Contrast and Colour Independence](02-colour-contrast-and-colour-independence.md) |
