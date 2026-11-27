# Interaction Media Features

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Interaction Architecture |
| **Prerequisites** | `05-responsive-design/01-responsive-principles-and-mobile-first.md`, `05-responsive-design/03-media-queries.md` |
| **Estimated Reading Time** | 20 minutes |
| **Status** | Complete |
| **Baseline Status** | Widely Available across all modern browsers (Baseline 2020) |
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

Historically, web developers assumed a dangerous equation:
$$\text{Small Screen} = \text{Touchscreen Phone}$$
$$\text{Large Screen} = \text{Desktop with Mouse}$$

In the modern device landscape, this assumption is completely broken:
- High-end tablets (iPad Pro, Surface Pro) have massive $12.9\text{inch}$ screens ($1366\text{px}$ wide) but are operated with **coarse human fingers**!
- Touchscreen laptops (MacBook + iPad Sidecar, Lenovo Yoga) feature large displays where users alternate between a physical mouse and touch input.
- Compact hand-held gaming consoles and netbooks have small screens operated with fine analog pointers or styluses.

If you style hover effects purely based on screen width (`@media (min-width: 1024px)`), tablet users will suffer from the infamous **"Sticky Hover Bug"**: tapping a button triggers `:hover`, which gets stuck permanently on the screen until they tap elsewhere!

**CSS Interaction Media Features** (`hover`, `pointer`, `any-hover`, `any-pointer`) interrogate the device's actual input hardware rather than screen pixel dimensions.

---

## 2 Core Concepts

### 1. The `pointer` Media Feature

Queries the precision of the device's primary pointing mechanism:
- **`none`**: No pointing mechanism (e.g. Apple TV, game console with D-pad).
- **`coarse`**: Inaccurate pointing mechanism (e.g. human finger on a smartphone or touchscreen tablet).
- **`fine`**: Highly accurate pointing mechanism (e.g. precision optical mouse, trackpad, digital stylus).

### 2. The `hover` Media Feature

Queries whether the device's primary pointing mechanism can physically hover over elements without activating them:
- **`none`**: Primary input cannot hover (e.g. human fingers on smartphones/tablets).
- **`hover`**: Primary input can hover freely (e.g. mouse cursor, trackpad).

### 3. Primary vs. Any (`any-pointer` / `any-hover`)

Many modern devices have **multiple input mechanisms** (e.g. a touchscreen laptop with an external USB mouse):
- **`pointer` / `hover`**: Inspects only the **primary** pointing device.
- **`any-pointer` / `any-hover`**: Inspects **all connected pointing devices**. If *any* connected hardware can hover (like an external mouse plugged into an iPad), `@media (any-hover: hover)` evaluates to `true`!

---

## 3 How It Works Internally

### The "Sticky Hover on Touch" Bug Deconstructed

```
On Desktop (Mouse Input):
User moves cursor over button ──► :hover styles activate.
User moves cursor away        ──► :hover styles deactivate immediately.

On Touch Device (Finger Input):
User taps button with finger  ──► Browser sends 'touchstart'.
                              ──► Browser engine synthesizes a 'mousemove' and activates :hover!
User lifts finger away        ──► Button REMAINS STUCK in :hover state!
                                  (Tooltips stay visible, button color stays highlighted!)
```

### The Clean Defensive Solution

Never declare hover states globally. Gate hover micro-interactions inside `@media (hover: hover)`:

```css
/* Base: Clean static button */
.action-button {
  background-color: #2563eb;
  color: #ffffff;
  padding: 12px 24px;
  border-radius: 6px;
  transition: background-color 0.2s ease;
}

/* Enhanced: ONLY apply hover highlighting if device CAN physically hover */
@media (hover: hover) {
  .action-button:hover {
    background-color: #1d4ed8;
  }
}
```

---

## 4 Syntax & Usage

```css
/* Touch-Optimized Hit Targets (Fingers) */
@media (pointer: coarse) {
  .nav-link, button, input {
    min-height: 44px; /* Ensure 44x44px finger target */
    min-width: 44px;
    padding: 12px 16px;
  }
}

/* Mouse-Optimized Precision (Fine Pointer) */
@media (pointer: fine) {
  .table-row-action-btn {
    padding: 4px 8px; /* Compact desktop sizing */
  }
}

/* Hover Capabilities */
@media (hover: hover) {
  .card:hover {
    transform: translateY(-4px);
    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  }
}
```

> [!NOTE]
> **Baseline Status: Widely Available**
> `hover`, `pointer`, `any-hover`, and `any-pointer` are Baseline Widely Available across all major modern browsers since 2020 (Chrome 41+, Safari 9+, Firefox 64+).

---

## 5 Practical Examples

### Example 1: Eliminating Sticky Tooltips on Mobile

```css
.tooltip-trigger {
  position: relative;
}

.tooltip-bubble {
  display: none;
  position: absolute;
  bottom: 100%;
  left: 50%;
  transform: translateX(-50%);
  background-color: #0f172a;
  color: #ffffff;
  padding: 6px 12px;
  border-radius: 4px;
  font-size: 0.75rem;
  white-space: nowrap;
}

/* ONLY display tooltip on hover if the device actually supports true hover */
@media (hover: hover) {
  .tooltip-trigger:hover .tooltip-bubble {
    display: block;
  }
}
```

### Example 2: WCAG 2.5.8 Touch Target Sizing with `pointer: coarse`

```css
.icon-action-btn {
  background: transparent;
  border: none;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
}

/* Fine pointer: Compact 32x32px desktop button */
@media (pointer: fine) {
  .icon-action-btn {
    width: 32px;
    height: 32px;
  }
}

/* Coarse touch pointer: Expand to 44x44px minimum touch target */
@media (pointer: coarse) {
  .icon-action-btn {
    width: 48px;
    height: 48px;
  }
}
```

### Example 3: Edge Case — Hybrid Touchscreen Laptops (`any-hover`)

On laptops with both a touchscreen and a trackpad (like Microsoft Surface or Lenovo ThinkPad):
- `hover` may evaluate to `none` or `hover` depending on whether tablet mode is active.
- `any-hover: hover` guarantees detection that a mouse is available:

```css
@media (any-hover: hover) {
  /* Provides subtle hover hints if ANY mouse is connected */
  .list-item:hover {
    background-color: #f8fafc;
  }
}
```

### Example 4: Failure Case — Hiding Critical Controls Behind Hover

```css
/* FAILURE PATTERN: ACCESSIBILITY DISASTER */
.card-actions {
  opacity: 0; /* Hidden by default! */
  transition: opacity 0.2s ease;
}

.card:hover .card-actions {
  opacity: 1; /* Appears on hover */
}
/* PROBLEM: On an iPad or smartphone, the user has NO mouse to hover with!
   The user can NEVER see or tap the action buttons! */

/* FIXED PATTERN */
.card-actions-fixed {
  opacity: 1; /* Always visible by default on touch screens */
}

@media (hover: hover) {
  .card-actions-fixed {
    opacity: 0;
  }
  .card:hover .card-actions-fixed {
    opacity: 1; /* Only hide behind hover if true hover is supported */
  }
}
```

### Example 5: Custom Dropdown Toggle Behavior

```css
.dropdown-menu {
  display: none;
  position: absolute;
  top: 100%;
  left: 0;
  background: #ffffff;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}

/* Mouse users: Open on hover */
@media (hover: hover) {
  .dropdown-parent:hover .dropdown-menu {
    display: block;
  }
}

/* Touch users: Rely on explicit click / tap states */
.dropdown-parent.active .dropdown-menu {
  display: block;
}
```

---

## 6 Best Practices

1. **Gate all visual hover micro-interactions inside `@media (hover: hover)`**: Prevents the sticky hover bug on smartphones and tablets.
2. **Never hide primary functionality or critical actions exclusively behind hover**: Ensure buttons and controls are visible or accessible via click/tap on touch devices.
3. **Use `pointer: coarse` to enforce minimum touch targets**: Guarantee interactive buttons are at least $44\text{px} \times 44\text{px}$ (or $48\text{px} \times 48\text{px}$ for Android).
4. **Use `pointer: fine` to densify desktop tables and data grids**: Allows compact rows on desktop while maintaining large tap targets on touchscreens.
5. **Always provide `:focus-visible` styling alongside `:hover`**: Keyboard users rely on focus outlines, not mouse hover states.
6. **Remember that iPad Pro is a coarse pointer device**: Do not assume high resolution or wide screen equals a mouse pointer.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Why It Fails | Solution |
| :--- | :--- | :--- |
| Sticky hover state on iOS Safari | Touch event triggers fake `:hover` that stays stuck | Wrap `:hover` rules inside `@media (hover: hover)` |
| Tiny 24px buttons on touch tablets | Hard to hit with human fingers; high tap error rate | Use `@media (pointer: coarse)` to expand targets |
| Delete/Edit buttons hidden on mobile | Controls hidden behind `:hover` can never be triggered on touch | Make controls permanently visible on touch screens |
| Assuming `width >= 1024px` has a mouse | Tablets and touch laptops have wide screens with touch input | Query `(hover: hover)` instead of screen width |
| Forgetting `:focus-visible` | Keyboard users cannot navigate elements styled only on `:hover` | Always pair `:hover` with `:focus-visible` |
| Overriding touch sizes with desktop small text | Small font sizes reduce clickable area on buttons | Add `min-height: 44px; min-width: 44px;` |

---

## 8 Performance, Accessibility & Browser Support

### Input Hardware Query Performance

- Interaction media features evaluate instantly against the browser's hardware input driver registry with zero overhead.
- They completely replace fragile JavaScript touch-detection libraries (like `Modernizr.touchevents`) that tested for `'ontouchstart' in window` (which caused false positives on touch laptops).

### Accessibility Standards (WCAG 2.5.5 & 2.5.8 Target Size)

- **WCAG 2.5.8 Target Size (Minimum - Level AA)**: The size of the target for pointer inputs must be at least **$24\text{px} \times 24\text{px}$** (with adequate spacing).
- **WCAG 2.5.5 Target Size (Enhanced - Level AAA)**: Pointer target size must be at least **$44\text{px} \times 44\text{px}$**.
- Utilizing `@media (pointer: coarse)` allows you to effortlessly achieve Level AAA compliance on touch devices while preserving elegant, compact information density on desktop workstations.

---

## 9 Comparison / When to Use What

| Feature Query | Matches What Hardware? | Typical Component Application |
| :--- | :--- | :--- |
| `hover: hover` | Desktop mouse, trackpad | Hover lift cards, tooltips, animated underline |
| `hover: none` | Smartphone, tablet, touchscreen | Permanently visible action buttons, tap menus |
| `pointer: coarse` | Human finger, touchscreens | $44\text{px}+$ touch targets, large form inputs |
| `pointer: fine` | Optical mouse, stylus, trackpad | Compact data tables, dense toolbars |
| `any-hover: hover` | Device has *at least one* mouse | Surface laptops, iPad with Magic Keyboard |

---

## 10 Interview Questions

<details>
<summary><strong>1. What causes the "Sticky Hover Bug" on mobile touch devices, and how do you resolve it?</strong></summary>

When a user taps an element on a touchscreen, the browser synthesizes mouse events (`mousemove`, `mouseover`, `mousedown`) to maintain backwards compatibility with legacy web applications. This activates `:hover` styling. Because the user's finger is removed and no cursor moves away, the element remains stuck in the `:hover` state until another element is tapped. You resolve it by enclosing all hover-specific CSS inside `@media (hover: hover)`.
</details>

<details>
<summary><strong>2. Why is using screen width to detect mobile devices an anti-pattern for interaction design?</strong></summary>

Modern hardware decouples screen dimensions from input methods. An iPad Pro has a screen width of $1024\text{px}$ to $1366\text{px}$ (larger than many laptops) but is operated primarily with coarse finger touch. Conversely, some netbooks or handheld consoles have small screens operated with fine mice or precision styluses. Querying `@media (hover: hover)` or `@media (pointer: coarse)` tests actual input capabilities directly.
</details>

<details>
<summary><strong>3. What is the difference between <code>pointer: coarse</code> and <code>any-pointer: coarse</code>?</strong></summary>

`pointer: coarse` checks only the primary pointing mechanism of the device. `any-pointer: coarse` checks all connected pointing mechanisms. On a desktop touchscreen monitor where a physical mouse is the primary pointer, `pointer` is `fine`, but `any-pointer` will match both `fine` and `coarse`.
</details>

<details>
<summary><strong>4. What is the minimum recommended touch target size under WCAG 2.5.5 and WCAG 2.5.8?</strong></summary>

- WCAG 2.5.8 (Level AA): At least $24\text{px} \times 24\text{px}$ (or sufficient spacing).
- WCAG 2.5.5 (Level AAA): At least $44\text{px} \times 44\text{px}$.
- Apple iOS Human Interface Guidelines recommends $44\text{pt} \times 44\text{pt}$, and Google Android Material Design recommends $48\text{dp} \times 48\text{dp}$.
</details>

<details>
<summary><strong>5. Why should critical actions (like Edit or Delete buttons) never be hidden behind <code>:hover</code>?</strong></summary>

Because users on touchscreens without hovering capabilities will have no visual indication that those controls exist, and cannot trigger them without accidental taps. Critical actions must always be visibly accessible on touch interfaces.
</details>

<details>
<summary><strong>6. Can a device match both <code>hover: none</code> and <code>pointer: fine</code>?</strong></summary>

Yes. An example is a graphics tablet without a cursor display, or a high-precision stylus where the screen only registers contact upon physical touch rather than proximity hover.
</details>

<details>
<summary><strong>7. How does <code>hover: hover</code> interact with keyboard-only users?</strong></summary>

Keyboard-only users navigate using `Tab` and arrow keys, which triggers `:focus` and `:focus-visible`, not `:hover`. Enclosing hover styles inside `(hover: hover)` has no adverse impact on keyboard users as long as authors provide proper `:focus-visible` styling alongside hover effects.
</details>

<details>
<summary><strong>8. What is the browser support baseline for Interaction Media Features?</strong></summary>

They are part of **Web Platform Baseline 2020** (Widely Available across Chrome 41+, Safari 9+, Firefox 64+, Edge 79+).
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Guarding Hover Micro-Interactions
- **Task**: Style a card to elevate with a shadow and transform on hover, but ensure touch devices never get stuck in the hover state.

<details>
<summary>View Solution</summary>

```css
.card {
  background-color: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 8px;
  padding: 20px;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}

@media (hover: hover) {
  .card:hover {
    transform: translateY(-4px);
    box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  }
}
```
</details>

### Exercise 2 (Easy): WCAG AAA Touch Target Sizing
- **Task**: Ensure close buttons on modal dialogs are at least 44x44px on coarse pointer touch devices.

<details>
<summary>View Solution</summary>

```css
.modal-close-btn {
  background: transparent;
  border: none;
  cursor: pointer;
  display: grid;
  place-items: center;
}

@media (pointer: coarse) {
  .modal-close-btn {
    min-width: 44px;
    min-height: 44px;
  }
}
```
</details>

### Exercise 3 (Medium): Adaptive Data Grid Row Height
- **Task**: Style a table row to have compact 32px height on desktop mouse displays, and comfortable 48px height on touchscreens.

<details>
<summary>View Solution</summary>

```css
.data-table td {
  padding: 6px 12px;
}

@media (pointer: coarse) {
  .data-table td {
    padding: 14px 16px; /* Expanded tap area for mobile/tablet */
  }
}
```
</details>

### Exercise 4 (Medium): Touch-Safe Tooltip Preview
- **Task**: Show an informational tooltip on hover for mouse users, but ensure the tooltip text is permanently visible as helper text on touchscreens.

<details>
<summary>View Solution</summary>

```css
.helper-text {
  display: block;
  font-size: 0.8rem;
  color: #64748b;
  margin-top: 4px;
}

/* On desktop with mouse, collapse into a hover tooltip */
@media (hover: hover) {
  .helper-text {
    display: none;
    position: absolute;
    bottom: 100%;
    background: #0f172a;
    color: #ffffff;
    padding: 4px 8px;
    border-radius: 4px;
  }
  
  .input-wrapper:hover .helper-text {
    display: block;
  }
}
```
</details>

### Exercise 5 (Hard): Dual Pointer Hybrid Detection
- **Task**: Detect if a device has ANY mouse connected and show custom cursor helpers.

<details>
<summary>View Solution</summary>

```css
@media (any-hover: hover) {
  .draggable-item {
    cursor: grab;
  }
  
  .draggable-item:active {
    cursor: grabbing;
  }
}
```
</details>

### Exercise 6 (Hard): Accessible Touch Action Buttons in Feed
- **Task**: Ensure action buttons on a social media card are permanently visible on touch screens, but fade in on hover for desktop mouse users.

<details>
<summary>View Solution</summary>

```css
.card-action-bar {
  display: flex;
  gap: 12px;
  opacity: 1; /* Always visible on touchscreens */
}

@media (hover: hover) {
  .card-action-bar {
    opacity: 0;
    transition: opacity 0.2s ease;
  }
  
  .card:hover .card-action-bar,
  .card:focus-within .card-action-bar {
    opacity: 1; /* Visible on hover and keyboard focus */
  }
}
```
</details>

---

## 12 Summary / Cheat Sheet

```css
/* Capability Queries */
@media (hover: hover)     /* True hover (Mouse / Trackpad) */
@media (hover: none)      /* Touchscreen (Phone / Tablet) */

@media (pointer: fine)    /* Mouse / Precision stylus */
@media (pointer: coarse)  /* Finger touch (Expand target to >= 44px) */

/* Golden Rule */
/* Wrap all :hover styles inside @media (hover: hover) to kill sticky mobile hover! */
```

---

## 13 Further Reading

- [MDN Web Docs: hover media feature](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/hover)
- [MDN Web Docs: pointer media feature](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/pointer)
- [W3C Media Queries Level 4: Pointer Interaction](https://www.w3.org/TR/mediaqueries-4/#mf-interaction)
- [web.dev: Adapting to input devices](https://web.dev/learn/design/interaction/)

---

[Previous: 07-user-preference-media-features.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/07-user-preference-media-features.md) | [Section Index](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/README.md) | [Next: 09-responsive-navigation-patterns.md](file:///e:/Learning/engineering-notes/01-languages/css/05-responsive-design/09-responsive-navigation-patterns.md)
