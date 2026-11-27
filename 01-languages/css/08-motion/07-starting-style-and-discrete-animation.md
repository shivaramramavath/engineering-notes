# Starting Style and Discrete Animation

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | CSS Transitions Level 2 / CSS Display Module Level 4 (W3C Working Draft / Candidate Recommendation) |
| **Baseline Status** | Baseline 2024 (Newly Available across Chrome 117+, Firefox 129+, Safari 17.5+) |
| **Browsers Tested** | Chrome 117+, Edge 117+, Firefox 129+, Safari 17.5+ |
| **Tested With** | W3C Web Platform Tests, Playwright, Chromium DevTools Rendering Inspector |

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

Historically in CSS, transitioning elements into and out of the Document Object Model (DOM) or transitioning elements switching between `display: none` and `display: block` was impossible without complex JavaScript choreography. Because `display` is a discrete, non-interpolatable property, changing an element from `display: none` to `display: block` immediately applied target styles without rendering intermediate frames. Developers were forced to use `requestAnimationFrame()`, double `setTimeout()` callbacks, or CSS class toggling with delay hacks to achieve smooth entry and exit transitions for modals, dropdowns, dialogs, and popovers.

The modern CSS Motion specification introduces two revolutionary features to solve this problem natively:
1. **`@starting-style`**: An at-rule that defines an element's initial style properties *before* its first style calculation or when changing from `display: none` to an active display type.
2. **`transition-behavior: allow-discrete`** (and `overlay` property): Enables transitions on discrete properties (such as `display`, `content-visibility`, and `overlay`), allowing an element to remain in the active render tree during its exit transition until all continuous animations (such as `opacity` or `transform`) finish.

With Baseline 2024 cross-browser availability, developers can animate native `<dialog>` and `popover` elements smoothly into and out of the top layer entirely in declarative CSS.

---

## 2. Core Concepts

### Discrete vs. Continuous Properties
- **Continuous Properties**: Values interpolate smoothly along a mathematical continuum (e.g., `opacity` from `0` to `1`, `transform` from `scale(0.8)` to `scale(1)`).
- **Discrete Properties**: Values flip instantaneously at a specific transition threshold (e.g., `display: none` to `display: block`, `visibility: hidden` to `visible`). Under standard transitions (`transition-behavior: normal`), discrete properties switch instantly at $t = 0$ (or not at all). Under `allow-discrete`, the property switches at $t = 0$ when transitioning forward, and switches at $t = 1$ (the very end of the duration) when transitioning backward (exit transition).

### The `@starting-style` Block
The `@starting-style` rule establishes an artificial initial state for an element when it is first inserted into the DOM or when its `display` flips from `none` to any other value. Without `@starting-style`, a browser sets computed values immediately upon insertion, leaving no "before" state to interpolate from.

```css
.card {
  opacity: 1;
  transform: translateY(0);
  transition: opacity 0.4s ease, transform 0.4s ease;

  @starting-style {
    opacity: 0;
    transform: translateY(20px);
  }
}
```

### The Top Layer and `overlay` Property
Native `<dialog>` elements and `[popover]` elements live in the browser's **Top Layer** when opened. Animating an element out of the top layer requires delaying the element's removal from the top layer until the exit transition finishes. The CSS property `overlay: auto` (transitionable with `allow-discrete`) keeps the element in the top layer during exit animation.

---

## 3. How It Works Internally

```
Classic DOM Insert / Display Change:
  [display: none] ──(JS Class Added)──> [display: block, opacity: 1]
                                          └─ No intermediate frames! Instantly appears.

Modern @starting-style + allow-discrete Pipeline:
  1. Element Mounted or display changed from 'none' to 'block'
  2. Browser detects @starting-style declaration
  3. Frame 0 (Synthetic Initial State):
     - Computed styles set to @starting-style values (opacity: 0, transform: scale(0.9))
  4. Frame 1 (Transition Initiation):
     - Transition starts toward target styles (opacity: 1, transform: scale(1.0))
     - Display switches immediately to 'block'
  5. Exit Sequence (Closing / Dismissing):
     - Target styles set back to hidden (opacity: 0)
     - display: none & overlay: none transitions delayed via allow-discrete
     - Compositor renders frames 0% -> 100% of duration
     - At duration completion (100%), display flips to 'none' and drops from Top Layer
```

```
Top Layer Entry / Exit Transition State Table:
┌─────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Stage           │ display              │ overlay              │ opacity              │
├─────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Closed (Idle)   │ none                 │ none                 │ 0                    │
│ Entry Frame 0   │ @starting-style:     │ @starting-style:     │ @starting-style: 0   │
│                 │ (evaluates as block) │ (evaluates as auto)  │                      │
│ Entry 0% -> 100%│ block (instant)      │ auto (instant)       │ 0 -> 1 (interpolates)│
│ Open (Active)   │ block                │ auto                 │ 1                    │
│ Exit 0% -> 100% │ block (held active!) │ auto (held in top!)  │ 1 -> 0 (interpolates)│
│ Closed (Final)  │ none (flips at 100%) │ none (flips at 100%) │ 0                    │
└─────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2024 Support Box:
├── Chrome / Edge: 117+ (Full support for @starting-style, transition-behavior, overlay)
├── Firefox: 129+ (Full support for @starting-style and discrete transitions)
└── Safari: 17.5+ (macOS / iOS support for @starting-style and allow-discrete)
```

### Transition Behavior Syntax
The `transition-behavior` property can be declared standalone or shorthand:

```css
/* Longhand declaration */
.dialog-box {
  transition-property: display, opacity, transform;
  transition-duration: 0.3s;
  transition-behavior: allow-discrete;
}

/* Shorthand declaration: append 'allow-discrete' after timing function */
.modal-window {
  transition:
    opacity 0.3s ease,
    transform 0.3s cubic-bezier(0.16, 1, 0.3, 1),
    display 0.3s allow-discrete,
    overlay 0.3s allow-discrete;
}
```

---

## 5. Practical Examples

### Example 1: Basic Element Entry on DOM Mount
When a list item is appended to a list via JavaScript, it smoothly fades and slides into view without manual JavaScript timeout choreography.

```html
<ul id="notification-list" class="notif-feed">
  <li class="notif-item">Item #1: Payment received</li>
</ul>
```

```css
.notif-item {
  display: flex;
  align-items: center;
  padding: 1rem 1.25rem;
  background-color: #ffffff;
  border-left: 4px solid #10b981;
  border-radius: 0.5rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  opacity: 1;
  transform: translateX(0);
  transition: opacity 0.35s ease-out, transform 0.35s cubic-bezier(0.16, 1, 0.3, 1);

  @starting-style {
    opacity: 0;
    transform: translateX(40px);
  }
}
```

### Example 2: Pure CSS Accordion with `display: none` and Discrete Transition
Toggle an accordion panel between `display: none` and `display: block` with smooth height and opacity transitions.

```html
<div class="accordion-item">
  <button class="accordion-trigger" aria-expanded="false">Account Settings</button>
  <div class="accordion-content">
    <p>Manage your login credentials, multi-factor keys, and notification preferences.</p>
  </div>
</div>
```

```css
.accordion-content {
  display: none;
  opacity: 0;
  transform: translateY(-8px);
  transition:
    opacity 0.25s ease-out,
    transform 0.25s ease-out,
    display 0.25s allow-discrete;
}

.accordion-content.is-open {
  display: block;
  opacity: 1;
  transform: translateY(0);

  @starting-style {
    opacity: 0;
    transform: translateY(-8px);
  }
}
```

### Example 3: Native HTML `<dialog>` with Full Entry and Exit Transitions
Animating both open (showModal) and close transitions including `::backdrop`.

```html
<dialog id="confirm-modal" class="native-dialog">
  <h2>Confirm Order Cancellation</h2>
  <p>Are you sure you want to cancel order #98214? This action cannot be reversed.</p>
  <form method="dialog">
    <button value="cancel">Keep Order</button>
    <button value="confirm" class="btn-danger">Cancel Order</button>
  </form>
</dialog>
```

```css
/* Dialog Base Styling (Closed State) */
.native-dialog {
  border: none;
  border-radius: 0.75rem;
  padding: 2rem;
  max-width: 32rem;
  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);
  background-color: #ffffff;
  color: #1e293b;

  /* Exit animation values */
  opacity: 0;
  transform: scale(0.92) translateY(12px);

  /* Transition declaration for all active properties */
  transition:
    opacity 0.3s ease-out,
    transform 0.3s cubic-bezier(0.16, 1, 0.3, 1),
    overlay 0.3s allow-discrete,
    display 0.3s allow-discrete;
}

/* Open State */
.native-dialog[open] {
  opacity: 1;
  transform: scale(1) translateY(0);
}

/* Starting style for entry animation */
@starting-style {
  .native-dialog[open] {
    opacity: 0;
    transform: scale(0.92) translateY(12px);
  }
}

/* Backdrop Transition */
.native-dialog::backdrop {
  background-color: rgba(15, 23, 42, 0);
  backdrop-filter: blur(0px);
  transition:
    background-color 0.3s ease-out,
    backdrop-filter 0.3s ease-out,
    overlay 0.3s allow-discrete,
    display 0.3s allow-discrete;
}

.native-dialog[open]::backdrop {
  background-color: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(4px);
}

@starting-style {
  .native-dialog[open]::backdrop {
    background-color: rgba(15, 23, 42, 0);
    backdrop-filter: blur(0px);
  }
}
```

### Example 4: Popover API with Anchored Dropdown Menu
Smooth dropdown menu opening and closing using native HTML `popover="auto"` attributes.

```html
<button popovertarget="nav-user-menu" class="user-avatar-btn">
  Profile Menu
</button>

<div id="nav-user-menu" popover class="user-popover-menu">
  <a href="/profile">My Account</a>
  <a href="/billing">Billing</a>
  <hr>
  <button class="logout-btn">Sign Out</button>
</div>
```

```css
.user-popover-menu {
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  padding: 0.5rem;
  background: #ffffff;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  margin: 0;
  inset: auto;
  top: 4rem;
  right: 1.5rem;

  /* Closed state */
  opacity: 0;
  transform: translateY(-6px);
  transition:
    opacity 0.2s ease-out,
    transform 0.2s cubic-bezier(0.16, 1, 0.3, 1),
    display 0.2s allow-discrete,
    overlay 0.2s allow-discrete;
}

/* Open state target */
.user-popover-menu:popover-open {
  opacity: 1;
  transform: translateY(0);
}

@starting-style {
  .user-popover-menu:popover-open {
    opacity: 0;
    transform: translateY(-6px);
  }
}
```

### Example 5: Toast Notification Dismissal with Exit Transition
A notification banner that auto-dismisses after 5 seconds with an animated slide-out before removing itself from display.

```html
<aside id="toast-banner" class="toast-card" role="status">
  <span>Saved changes to cloud storage.</span>
</aside>
```

```css
.toast-card {
  position: fixed;
  bottom: 2rem;
  right: 2rem;
  display: flex;
  align-items: center;
  padding: 0.875rem 1.25rem;
  background-color: #0f172a;
  color: #f8fafc;
  border-radius: 0.5rem;
  box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3);

  /* Open default */
  opacity: 1;
  transform: translateY(0);
  transition:
    opacity 0.3s ease,
    transform 0.3s ease,
    display 0.3s allow-discrete;

  @starting-style {
    opacity: 0;
    transform: translateY(20px);
  }
}

.toast-card.is-dismissed {
  opacity: 0;
  transform: translateY(16px);
  display: none;
}
```

---

## 6. Best Practices

1. **Always pair `display` with `overlay` for top-layer elements**: When transitioning `<dialog>` or `popover`, animate both `display 0.3s allow-discrete` and `overlay 0.3s allow-discrete`. Omitting `overlay` causes the element to fall out of the top layer immediately during exit, producing clipping artifacts.
2. **Nest `@starting-style` directly inside the selector**: Use CSS nesting syntax (`@starting-style { ... }` inside `.element`) to keep styles maintainable and scoped.
3. **Keep durations short for discrete exit transitions**: Exit transitions should range between 150ms and 250ms to ensure the user interface feels responsive and snappy.
4. **Ensure `transition-behavior: allow-discrete` is applied to both entry and exit selectors**: Applying `transition-behavior` only on the `:open` state will cause the exit animation to break because the closed state will revert to `transition-behavior: normal`.
5. **Never animate layout properties alongside discrete displays**: Only animate compositor-friendly properties (`opacity`, `transform`) during discrete state changes to avoid layout thrashing during display switches.
6. **Support `prefers-reduced-motion` cleanly**: For users who prefer reduced motion, remove the `transform` transitions while leaving the `opacity` or instant discrete switches intact.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `transition-behavior: allow-discrete` | Element disappears instantly upon closing without any fading or shrinking transition. | Include `allow-discrete` either shorthand (`transition: display 0.3s allow-discrete`) or via longhand property. |
| Forgetting `overlay` on `<dialog>` and `popover` | Modals clip under neighboring elements or drop behind backdrops during exit animation. | Add `overlay 0.3s allow-discrete` to the transition list. |
| Placing `@starting-style` on the closed state instead of open state | Initial entry animation fails to trigger when the element opens. | Declare `@starting-style` on the open selector (`dialog[open]`, `.item.is-open`). |
| Declaring different transition durations on `display` vs. `opacity` | If `display` duration is shorter than `opacity`, the element snaps out of existence before the fade finishes. | Ensure `display` transition duration matches or exceeds the longest continuous property duration. |
| Transitioning `display` without defining target styles | No animation occurs; browser ignores discrete transition without an interpolating visual property. | Always pair `display` transitions with continuous properties such as `opacity` or `transform`. |
| Overwriting `transition` in child states without repeating `allow-discrete` | Exit animations silently break due to shorthand reset. | Use individual CSS properties or ensure helper classes maintain `transition-behavior`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- Discrete transitions on `display` and `overlay` incur zero animation computation overhead; they simply act as binary time-delayed triggers.
- When paired exclusively with `opacity` and `transform`, the entire transition executes on the GPU Compositor thread.

### Accessibility (A11y)
- Elements with `display: none` are completely excluded from the Accessibility Tree. Using discrete transitions ensures that while an element is visible during its exit transition, screen readers can cleanly recognize when the element has been completely deactivated.
- Always respect `prefers-reduced-motion`:

```css
@media (prefers-reduced-motion: reduce) {
  .native-dialog,
  .native-dialog::backdrop {
    transition-duration: 0.01ms !important;
  }
}
```

### Browser Support Matrix
- **Chrome / Edge**: Supported since version 117 (September 2023).
- **Safari**: Supported since iOS / macOS Safari 17.5 (May 2024).
- **Firefox**: Supported since version 129 (August 2024).
- **Feature Detection**:
```css
@supports (transition-behavior: allow-discrete) {
  /* Modern starting style & discrete logic */
}
```

---

## 9. Comparison / When to Use What

| Technique | Entry Animation | Exit Animation | Top Layer Support | JavaScript Required? |
| :--- | :--- | :--- | :--- | :--- |
| **`@starting-style` + `allow-discrete`** | Native CSS | Native CSS | Full (`overlay`) | None |
| **CSS Keyframe Animations (`@keyframes`)** | Native CSS (`animation`) | Requires JS delay to remove class | Complex z-index hacking | Yes (listen to `animationend`) |
| **JavaScript Web Animations API (WAAPI)** | Programmatic | Programmatic | Manual `.close()` invocation | Yes (heavy JS) |
| **Legacy `visibility: hidden` Hacking** | Hacky opacity transition | Immediate collapse unless delayed | Fails in native Top Layer | Minimal |

---

## 10. Interview Questions

### Q1: What problem does `@starting-style` solve that previous CSS versions could not handle?
<details>
<summary>View Answer</summary>
When an element transitions from `display: none` to `display: block` or is first inserted into the DOM, the browser performs an initial style calculation. Previously, the browser established these initial styles as the baseline without any prior state to interpolate from, making CSS transitions fail on initial render. `@starting-style` enables developers to explicitly declare a synthetic "before-render" state, allowing smooth entry transitions without JavaScript layout re-triggering hacks.
</details>

### Q2: Why is `transition-behavior: allow-discrete` necessary when transitioning `display`?
<details>
<summary>View Answer</summary>
By default, CSS transition specifications define discrete properties (such as `display` or `content-visibility`) as non-animatable, meaning value changes happen immediately at $t = 0$. `transition-behavior: allow-discrete` instructs the browser transition engine to treat the discrete property as active throughout the entire transition duration. On exit transitions, `display: block` is preserved until all companion transitions (`opacity`, `transform`) reach 100% completion.
</details>

### Q3: Why must the `overlay` property be animated when closing a native `<dialog>` element?
<details>
<summary>View Answer</summary>
When a `<dialog>` closes or a popover is dismissed, the browser immediately removes the element from the browser's native **Top Layer**. If the element is removed from the Top Layer at frame 0 of the exit animation, it will immediately drop behind normal document elements or become clipped by overflow containers, ruining the exit animation. Transitioning `overlay 0.3s allow-discrete` instructs the browser to retain the element in the Top Layer until the animation duration completes.
</details>

### Q4: What happens if an element has an `@starting-style` block with `transform: scale(0.5)`, but no `transition` property is defined on the element?
<details>
<summary>View Answer</summary>
The element will immediately render at its normal computed styles (e.g. `transform: scale(1)`). `@starting-style` only establishes the starting values for an interpolation curve. Without a declared `transition` property that includes `transform`, no transition is initiated, and the browser displays the target computed style immediately on the first rendered frame.
</details>

### Q5: Can `@starting-style` be nested within pseudo-classes and media queries?
<details>
<summary>View Answer</summary>
Yes. `@starting-style` can be placed at the top level referencing a selector (e.g., `@starting-style { .modal[open] { opacity: 0; } }`) or nested directly inside a CSS rule set using native CSS nesting (e.g., `.dialog { &[open] { @starting-style { opacity: 0; } } }`). It also operates cleanly inside `@media` and `@container` queries.
</details>

### Q6: How does the browser determine the duration of a discrete exit transition when multiple properties have different durations?
<details>
<summary>View Answer</summary>
A discrete property with `allow-discrete` transitions until its own declared duration finishes. However, if companion continuous properties (such as `opacity`) have longer durations, closing the element prematurely by setting a shorter duration on `display` will snap the element out of view. Developers should synchronize `display` duration with the maximum duration of the active visual transitions.
</details>

### Q7: Does `@starting-style` re-trigger if an element remains in the DOM but changes an attribute?
<details>
<summary>View Answer</summary>
Yes, provided that attribute change transitions the element from an inactive display state (`display: none`) to an active state, or if the selector matching the `@starting-style` changes state (such as the native `[open]` attribute on `<dialog>` or the `:popover-open` pseudo-class).
</details>

### Q8: What is the fallback behavior in browsers that do not support `@starting-style`?
<details>
<summary>View Answer</summary>
Browsers that do not recognize `@starting-style` simply ignore the block as an unknown at-rule. The element opens and closes instantly with standard discrete display changes, preserving functionality without throwing CSS parsing errors.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Pure CSS Toast Popup with Mount Animation
**Objective**: Create a notification toast element that smoothly scales and fades into view when rendered in the DOM.
<details>
<summary>View Solution</summary>

```css
.toast-msg {
  display: block;
  padding: 1rem 1.5rem;
  background-color: #1e293b;
  color: #f8fafc;
  border-radius: 0.5rem;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  opacity: 1;
  transform: translateY(0);
  transition: opacity 0.3s ease-out, transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);

  @starting-style {
    opacity: 0;
    transform: translateY(1.5rem);
  }
}
```
</details>

### Exercise 2: Animate a Mobile Flyout Drawer with `display: none`
**Objective**: Animate a sliding navigation drawer from the left edge of the screen, supporting discrete display toggling.
<details>
<summary>View Solution</summary>

```css
.mobile-drawer {
  position: fixed;
  top: 0;
  left: 0;
  bottom: 0;
  width: 20rem;
  background-color: #ffffff;
  display: none;
  opacity: 0;
  transform: translateX(-100%);
  transition:
    transform 0.35s ease,
    opacity 0.35s ease,
    display 0.35s allow-discrete;
}

.mobile-drawer.is-active {
  display: block;
  opacity: 1;
  transform: translateX(0);

  @starting-style {
    opacity: 0;
    transform: translateX(-100%);
  }
}
```
</details>

### Exercise 3: Full Dialog and Backdrop Styling
**Objective**: Style a native `<dialog>` element with custom backdrop blur and smooth exit animations.
<details>
<summary>View Solution</summary>

```css
dialog.app-modal {
  border: none;
  padding: 2rem;
  border-radius: 1rem;
  opacity: 0;
  transform: scale(0.9);
  transition:
    opacity 0.25s ease,
    transform 0.25s ease,
    display 0.25s allow-discrete,
    overlay 0.25s allow-discrete;
}

dialog.app-modal[open] {
  opacity: 1;
  transform: scale(1);
}

@starting-style {
  dialog.app-modal[open] {
    opacity: 0;
    transform: scale(0.9);
  }
}

dialog.app-modal::backdrop {
  background-color: rgba(0, 0, 0, 0);
  transition:
    background-color 0.25s ease,
    display 0.25s allow-discrete,
    overlay 0.25s allow-discrete;
}

dialog.app-modal[open]::backdrop {
  background-color: rgba(0, 0, 0, 0.5);
}

@starting-style {
  dialog.app-modal[open]::backdrop {
    background-color: rgba(0, 0, 0, 0);
  }
}
```
</details>

### Exercise 4: Popover Tooltip with Scale Transition
**Objective**: Create a tooltip using the HTML `popover` attribute with an animated directional bubble transition.
<details>
<summary>View Solution</summary>

```css
[popover].bubble-tooltip {
  border: none;
  background-color: #334155;
  color: #ffffff;
  padding: 0.5rem 0.75rem;
  border-radius: 0.375rem;
  font-size: 0.875rem;
  opacity: 0;
  transform: scale(0.85);
  transition:
    opacity 0.15s ease-out,
    transform 0.15s ease-out,
    display 0.15s allow-discrete,
    overlay 0.15s allow-discrete;
}

[popover].bubble-tooltip:popover-open {
  opacity: 1;
  transform: scale(1);
}

@starting-style {
  [popover].bubble-tooltip:popover-open {
    opacity: 0;
    transform: scale(0.85);
  }
}
```
</details>

### Exercise 5: Collapsible Dropdown with `content-visibility`
**Objective**: Optimize layout calculation for off-screen hidden panels by transitioning `content-visibility: hidden` using `allow-discrete`.
<details>
<summary>View Solution</summary>

```css
.collapsible-card {
  content-visibility: hidden;
  opacity: 0;
  transition:
    opacity 0.3s ease,
    content-visibility 0.3s allow-discrete;
}

.collapsible-card.expanded {
  content-visibility: visible;
  opacity: 1;

  @starting-style {
    opacity: 0;
  }
}
```
</details>

### Exercise 6: Accessible Animated Banner with Reduced Motion Check
**Objective**: Create an animated dismissible banner that gracefully downgrades for users who prefer reduced motion.
<details>
<summary>View Solution</summary>

```css
.notice-bar {
  display: flex;
  justify-content: space-between;
  padding: 1rem;
  background-color: #fef3c7;
  color: #92400e;
  opacity: 1;
  transform: translateY(0);
  transition:
    opacity 0.3s ease,
    transform 0.3s ease,
    display 0.3s allow-discrete;

  @starting-style {
    opacity: 0;
    transform: translateY(-100%);
  }
}

.notice-bar.closed {
  display: none;
  opacity: 0;
  transform: translateY(-100%);
}

@media (prefers-reduced-motion: reduce) {
  .notice-bar {
    transform: none !important;
    transition-duration: 0.01ms !important;
  }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* Core Shorthand Syntax */
.element {
  /* Exit state */
  display: none;
  opacity: 0;
  transform: scale(0.95);

  /* Transition declaration: note 'allow-discrete' keywords */
  transition:
    opacity 0.25s ease,
    transform 0.25s ease,
    display 0.25s allow-discrete,
    overlay 0.25s allow-discrete;
}

.element.is-visible {
  /* Open state */
  display: block;
  opacity: 1;
  transform: scale(1);

  /* Entry starting values */
  @starting-style {
    opacity: 0;
    transform: scale(0.95);
  }
}
```

- `@starting-style` defines properties before initial DOM paint or transition from `display: none`.
- `transition-behavior: allow-discrete` delays setting `display: none` until exit animations finish.
- `overlay: auto` (with `allow-discrete`) keeps modals inside the Top Layer during exit animations.
- Supported cross-browser in all modern engines (Baseline 2024).

---

## 13. Further Reading

- [MDN Web Docs: @starting-style](https://developer.mozilla.org/en-US/docs/Web/CSS/@starting-style)
- [MDN Web Docs: transition-behavior](https://developer.mozilla.org/en-US/docs/Web/CSS/transition-behavior)
- [Chrome for Developers: Four new CSS features for smooth entry and exit animations](https://developer.chrome.com/blog/entry-exit-animations)
- [W3C CSS Transitions Level 2 Specification](https://drafts.csswg.org/css-transitions-2/)
- [W3C CSS Display Module Level 4](https://drafts.csswg.org/css-display-4/)

---

### Navigation
- **Previous**: [06-view-transitions-css.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/06-view-transitions-css.md)
- **Next**: [08-reduced-motion-and-accessible-animation.md](file:///e:/Learning/engineering-notes/01-languages/css/08-motion/08-reduced-motion-and-accessible-animation.md)
