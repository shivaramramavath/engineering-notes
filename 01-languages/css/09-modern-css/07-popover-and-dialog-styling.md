# Popover API and `<dialog>` Styling

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | HTML Living Standard / CSS Display Level 4 / CSS Positioning (W3C / WHATWG) |
| **Baseline Status** | Baseline 2024 (Widely Available across Chrome 114+, Safari 17+, Firefox 125+, Edge 114+) |
| **Browsers Tested** | Chrome 114+, Firefox 125+, Safari 17+, Edge 114+ |
| **Tested With** | W3C Web Platform Tests, Axe A11y, VoiceOver, NVDA, Playwright |

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

Creating modals, non-modal dialogs, floating tooltips, dropdown menus, and toast notifications has historically required hundreds of lines of fragile JavaScript:
- Managing `z-index` wars so popups aren't hidden behind subsequent elements.
- Hacking around `overflow: hidden` on parent containers that unexpectedly clip floating menus.
- Implementing custom "light dismiss" handlers (clicking outside the menu or pressing `Escape` to close).
- Enforcing keyboard focus traps so tab navigation doesn't wander into background content.

The modern web platform provides two dedicated, native browser primitives to solve this:
1. **The HTML `<dialog>` Element**: Designed for modal confirmation dialogs and critical prompts, featuring built-in modal focus trapping and standard form integration.
2. **The HTML Popover API (`popover`)**: Designed for non-modal overlays (dropdowns, tooltips, flyout menus, action sheets) with native light-dismiss, automatic dismissal of unrelated popovers, and zero `z-index` management.

Both primitives render into the browser's **Top Layer**, completely rendering outside the standard document flow, immune to parent clipping, and easily styled using CSS.

---

## 2. Core Concepts

### The Top Layer
The **Top Layer** is an internal rendering layer managed by the browser engine that sits above all other elements in the document:
- Elements in the top layer ignore `z-index`; they stack strictly in the order they were promoted into the top layer.
- Elements in the top layer are **never clipped** by ancestor containers that have `overflow: hidden`, `clip-path`, or `transform` applied.
- The `::backdrop` pseudo-element renders directly behind the top-layer element, spanning the entire viewport.

### Popover: `auto` vs. `manual`
The HTML `popover` attribute accepts two values:
- `popover="auto"`: Default behavior. Features automatic "light dismiss" (clicking outside closes the popover, pressing `Escape` closes it) and closes all other unrelated open popovers.
- `popover="manual"`: Disables light dismiss. The popover remains open until explicitly dismissed via script or a dedicated dismiss button. Ideal for toasts and persistent banners.

### The `<dialog>` Element: Modal vs. Non-Modal
- **Modal (`dialog.showModal()`)**: Promoted to the Top Layer. Renders `::backdrop`. Traps focus inside the dialog; inert background.
- **Non-Modal (`dialog.show()`)**: Renders within the standard document layer. No `::backdrop`. Focus is not trapped.

### The `::backdrop` Pseudo-Element
Renders a full-viewport canvas immediately beneath any element residing in the Top Layer:
```css
dialog::backdrop,
[popover]::backdrop {
  background-color: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(4px);
}
```

### The `:popover-open` Pseudo-Class
Matches any popover element that is currently visible in the Top Layer:
```css
[popover]:popover-open {
  opacity: 1;
  transform: translateY(0);
}
```

---

## 3. How It Works Internally

```
Top Layer Stacking Architecture:

┌────────────────────────────────────────────────────────┐
│ TOP LAYER (Independent of DOM hierarchy or z-index)    │
│                                                        │
│  [Top Layer Item 2]: <div popover> (Submenu)          │
│  [Top Layer Item 1]: <dialog open> (Active Modal)      │
│  [::backdrop]: rgba(0, 0, 0, 0.5) (Blurs document)     │
└────────────────────────────────────────────────────────┘
                           ▲
                           │ Elevated by showModal() or popovertarget
┌──────────────────────────┴─────────────────────────────┐
│ REGULAR DOCUMENT STACKING CONTEXTS                     │
│                                                        │
│  Root stacking context (z-index: 999999 fails here!)   │
│  Container with overflow: hidden; (Does NOT clip!)    │
│  Normal DOM Elements (div, section, footer)            │
└────────────────────────────────────────────────────────┘
```

```
<dialog> vs. Popover Comparison Matrix:
┌──────────────────────────────┬──────────────────────────────┬──────────────────────────────┐
│ Feature                      │ HTML `<dialog>`              │ HTML `popover`               │
├──────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
│ Primary Role                 │ Modal prompts & confirmations│ Non-modal menus, tooltips    │
│ Built-in Focus Trap          │ YES (on `showModal()`)       │ NO (free navigation)         │
│ Automatic Light Dismiss      │ NO (requires custom JS)      │ YES (built-in on `auto`)     │
│ Inerts Background DOM        │ YES (on `showModal()`)       │ NO (background interactive)  │
│ HTML Trigger Attribute       │ `<button formmethod=dialog>` │ `<button popovertarget="...">│
│ Top Layer Support            │ YES (modal mode)             │ YES (always)                 │
└──────────────────────────────┴──────────────────────────────┴──────────────────────────────┘
```

---

## 4. Syntax & Usage

```
Baseline 2024 Support Box:
├── Chrome / Edge: 114+ (May 2023)
├── Safari: 17+ (September 2023)
└── Firefox: 125+ (April 2024 - Complete cross-browser Baseline)
```

### Basic HTML Declarations
```html
<!-- Popover Trigger & Container -->
<button popovertarget="my-menu">Open Menu</button>
<div id="my-menu" popover="auto">
  <p>Menu content</p>
</div>

<!-- Native Dialog -->
<dialog id="my-dialog">
  <p>Modal content</p>
  <button onclick="this.closest('dialog').close()">Close</button>
</dialog>
```

### CSS Styling & Transitions
```css
/* Base Popover Styling */
[popover] {
  border: 1px solid #cbd5e1;
  border-radius: 0.5rem;
  padding: 1.5rem;
  box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.2);

  /* Modern Exit & Entry Transitions */
  opacity: 0;
  transform: scale(0.95);
  transition:
    opacity 0.2s ease,
    transform 0.2s ease,
    display 0.2s allow-discrete,
    overlay 0.2s allow-discrete;
}

[popover]:popover-open {
  opacity: 1;
  transform: scale(1);
}

@starting-style {
  [popover]:popover-open {
    opacity: 0;
    transform: scale(0.95);
  }
}
```

---

## 5. Practical Examples

### Example 1: Full-Featured Accessible Modal Dialog
A confirmation modal with custom backdrop blur, responsive sizing, and entry/exit transitions.

```html
<dialog id="delete-modal" class="modal-window">
  <h2>Delete Project?</h2>
  <p>This action is irreversible. All associated database tables will be purged.</p>
  <form method="dialog" class="dialog-actions">
    <button value="cancel" class="btn-secondary">Cancel</button>
    <button value="confirm" class="btn-danger">Yes, Delete</button>
  </form>
</dialog>
```

```css
.modal-window {
  border: none;
  border-radius: 0.75rem;
  padding: 2rem;
  max-width: 28rem;
  width: 90%;
  background: #ffffff;
  color: #0f172a;
  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25);

  /* Closed State */
  opacity: 0;
  transform: scale(0.9);
  transition:
    opacity 0.25s ease-out,
    transform 0.25s cubic-bezier(0.16, 1, 0.3, 1),
    display 0.25s allow-discrete,
    overlay 0.25s allow-discrete;

  h2 {
    margin-top: 0;
    font-size: 1.25rem;
  }

  p {
    color: #475569;
    line-height: 1.5;
  }

  .dialog-actions {
    display: flex;
    justify-content: flex-end;
    gap: 0.75rem;
    margin-top: 1.5rem;
  }
}

.modal-window[open] {
  opacity: 1;
  transform: scale(1);
}

@starting-style {
  .modal-window[open] {
    opacity: 0;
    transform: scale(0.9);
  }
}

/* Backdrop Transition */
.modal-window::backdrop {
  background-color: rgba(15, 23, 42, 0);
  backdrop-filter: blur(0px);
  transition:
    background-color 0.25s ease,
    backdrop-filter 0.25s ease,
    display 0.25s allow-discrete,
    overlay 0.25s allow-discrete;
}

.modal-window[open]::backdrop {
  background-color: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(4px);
}

@starting-style {
  .modal-window[open]::backdrop {
    background-color: rgba(15, 23, 42, 0);
    backdrop-filter: blur(0px);
  }
}
```

### Example 2: Popover Dropdown Action Menu
A clean profile menu anchored directly under an avatar button with native light dismiss.

```html
<button popovertarget="profile-menu" class="avatar-btn">
  <img src="avatar.png" alt="User avatar">
</button>

<div id="profile-menu" popover="auto" class="action-dropdown">
  <a href="/profile">Your Profile</a>
  <a href="/settings">Settings</a>
  <hr>
  <button class="logout-btn">Sign Out</button>
</div>
```

```css
.avatar-btn {
  background: none;
  border: none;
  cursor: pointer;
  padding: 0;
  border-radius: 50%;
}

.avatar-btn:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}

.action-dropdown {
  border: 1px solid #e2e8f0;
  border-radius: 0.5rem;
  background: white;
  padding: 0.5rem;
  min-width: 12rem;
  box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  margin: 0;
  inset: auto;
  top: 4.5rem;
  right: 1.5rem;

  opacity: 0;
  transform: translateY(-8px);
  transition:
    opacity 0.15s ease-out,
    transform 0.15s ease-out,
    display 0.15s allow-discrete,
    overlay 0.15s allow-discrete;

  a, button {
    display: block;
    width: 100%;
    padding: 0.5rem 0.75rem;
    text-align: left;
    background: none;
    border: none;
    color: #334155;
    text-decoration: none;
    font-size: 0.875rem;
    border-radius: 0.25rem;
    cursor: pointer;

    &:hover {
      background-color: #f1f5f9;
      color: #0f172a;
    }

    &:focus-visible {
      outline: 2px solid #2563eb;
      outline-offset: -1px;
    }
  }

  hr {
    margin: 0.5rem 0;
    border: none;
    border-top: 1px solid #e2e8f0;
  }
}

.action-dropdown:popover-open {
  opacity: 1;
  transform: translateY(0);
}

@starting-style {
  .action-dropdown:popover-open {
    opacity: 0;
    transform: translateY(-8px);
  }
}
```

### Example 3: Persistent Notification Toast using `popover="manual"`
A toast message that floats in the top layer and does not dismiss when users click elsewhere.

```html
<div id="cloud-toast" popover="manual" class="toast-card">
  <span>System backup completed successfully.</span>
  <button popovertarget="cloud-toast" popovertargetaction="hide" aria-label="Dismiss">✕</button>
</div>
```

```css
.toast-card {
  position: fixed;
  inset: auto;
  bottom: 1.5rem;
  right: 1.5rem;
  margin: 0;
  display: flex;
  align-items: center;
  gap: 1rem;
  padding: 0.75rem 1.25rem;
  background-color: #0f172a;
  color: #f8fafc;
  border-radius: 0.5rem;
  border: 1px solid #334155;
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.3);

  opacity: 0;
  transform: translateY(1rem);
  transition:
    opacity 0.3s ease,
    transform 0.3s cubic-bezier(0.16, 1, 0.3, 1),
    display 0.3s allow-discrete,
    overlay 0.3s allow-discrete;

  button {
    background: none;
    border: none;
    color: #94a3b8;
    cursor: pointer;
    font-size: 1rem;

    &:hover {
      color: white;
    }
  }
}

.toast-card:popover-open {
  opacity: 1;
  transform: translateY(0);
}

@starting-style {
  .toast-card:popover-open {
    opacity: 0;
    transform: translateY(1rem);
  }
}
```

### Example 4: Mobile Fullscreen Bottom-Sheet Dialog
A modal sheet that slides up from the bottom of the screen on mobile devices.

```css
@media (max-width: 48rem) {
  .modal-window {
    width: 100%;
    max-width: 100%;
    margin-bottom: 0;
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
    transform: translateY(100%);
  }

  .modal-window[open] {
    transform: translateY(0);
  }

  @starting-style {
    .modal-window[open] {
      transform: translateY(100%);
    }
  }
}
```

### Example 5: Custom Multi-Level Popover Menu
Submenu popover triggered from inside an existing popover menu without closing the parent menu.

```html
<div id="main-popover" popover="auto">
  <button popovertarget="sub-popover">Open Nested Submenu</button>
</div>

<div id="sub-popover" popover="auto">
  <p>Submenu content stays open alongside parent!</p>
</div>
```

---

## 6. Best Practices

1. **Use `<dialog>` for modal tasks; use `popover` for lightweight menus**: If you need to block the background and trap keyboard focus, choose `<dialog>`. For non-blocking tooltips, dropdowns, and flyouts, choose `popover`.
2. **Always pair `display` and `overlay` with `allow-discrete`**: When animating dialogs or popovers into/out of the Top Layer, always include `overlay 0.25s allow-discrete` and `display 0.25s allow-discrete`.
3. **Reset browser default user-agent dialog styles**: Browsers apply default borders, paddings, and centering to `<dialog>`. Reset `border: none; padding: 0;` and apply design system tokens explicitly.
4. **Use declarative HTML triggers**: Prefer `<button popovertarget="id">` over writing custom JavaScript `addEventListener('click')` handlers.
5. **Set `autofocus` thoughtfully in modals**: Place `autofocus` on the primary interactive control (or close button) inside `<dialog>` so keyboard users immediately know where their cursor landed.
6. **Ensure accessible backdrop contrast**: When using `::backdrop`, ensure the background dimming provides at least $50\%$ opacity so the modal content clearly separates from background noise.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Using `dialog.show()` instead of `dialog.showModal()` | Dialog renders as non-modal, lacks `::backdrop`, and does not trap focus. | Call `.showModal()` for true modal dialogs. |
| Forgetting `overlay` during exit transitions | The dialog falls out of the Top Layer immediately at frame 0, clipping under other elements. | Add `overlay: 0.25s allow-discrete` to the transition list. |
| Using `popover="auto"` for toast notifications | Clicking anywhere on the page instantly dismisses the toast before the user can read it. | Use `popover="manual"` for toasts. |
| Attempting to manage `z-index` on Top Layer elements | `z-index` has zero effect in the Top Layer; elements stack purely by open order. | Let the browser engine manage Top Layer stacking naturally. |
| Placing `<dialog>` inside an `overflow: hidden` container | Before top layer promotion, styles can look odd. Once `.showModal()` is called, it promotes to top layer cleanly. | Rest easy knowing `showModal()` breaks free of parent overflow! |
| Missing keyboard close handler for custom `<dialog>` close buttons | Keyboard-only users may struggle to dismiss custom dialogs. | Add `<form method="dialog">` or listen for `Escape`. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **Zero DOM Re-parenting**: No need for React Portals (`ReactDOM.createPortal()`) to move popups to `document.body` to avoid clipping. Elements remain in their natural logical DOM position while rendering visually in the Top Layer.
- **Hardware Acceleration**: The Top Layer is composited directly by the GPU.

### Accessibility (A11y)
- **`<dialog>.showModal()` Built-in A11y**: Automatically marks all outside DOM elements `inert`, locks keyboard focus inside the modal, sets focus to the first focusable element, and dispatches the native `cancel` event on `Escape`.
- **Popover Built-in A11y**: Links trigger to target via `popovertarget` without requiring manual `aria-controls` or `aria-expanded` synchronization in modern browsers.

### Browser Compatibility
- **Baseline 2024**: Full support across Chromium (114+), WebKit / Safari (17+), and Gecko / Firefox (125+).
- **Feature Detection**:
```css
@supports selector([popover]) {
  /* Popover supported */
}
```

---

## 9. Comparison / When to Use What

| Feature / Behavior | Native `<dialog>` | HTML Popover (`auto`) | JavaScript Modal Libraries |
| :--- | :--- | :--- | :--- |
| **Top Layer Rendering** | Yes (via `showModal()`) | Yes (always) | No (hacked with z-index) |
| **Focus Trapping** | Native | No (allows typing) | JavaScript event listener |
| **Light Dismiss (Click Outside)** | Manual JS | **Built-in Native** | JavaScript event listener |
| **Parent Overflow Clipping** | **Immune** | **Immune** | Vulnerable without portals |
| **Backdrop Filter Support** | Native `::backdrop` | Native `::backdrop` | Extra DOM overlay element |

---

## 10. Interview Questions

### Q1: What is the Top Layer in browser rendering engines?
<details>
<summary>View Answer</summary>
The Top Layer is a specialized rendering plane managed directly by the browser that sits in front of all other stacking contexts on the web page. Elements promoted to the Top Layer (such as active modals via `showModal()`, fullscreen elements, or open popovers) are completely immune to parent container clipping (`overflow: hidden`), parent CSS transforms, or document-level `z-index` constraints.
</details>

### Q2: How do `popover="auto"` and `popover="manual"` differ?
<details>
<summary>View Answer</summary>
`popover="auto"` enforces automatic light dismissal: clicking outside the popover, pressing the `Escape` key, or opening another unrelated `auto` popover immediately closes it. `popover="manual"` disables light dismiss, keeping the element open until explicitly dismissed via user interaction or JavaScript, making it ideal for toasts or interactive tooltips.
</details>

### Q3: Why is `overlay` needed when transitioning `<dialog>` out of view?
<details>
<summary>View Answer</summary>
When a modal is closed, the browser's default action is to immediately remove the element from the Top Layer at frame 0. If continuous transitions (`opacity`, `transform`) take 300ms, the modal would drop behind other page elements during its exit fade. Declaring `overlay 0.3s allow-discrete` instructs the browser to keep the modal elevated in the Top Layer until all transitions finish.
</details>

### Q4: What does `<form method="dialog">` achieve inside a `<dialog>`?
<details>
<summary>View Answer</summary>
`<form method="dialog">` is a native HTML feature. When a submit button inside this form is clicked, the browser closes the dialog automatically without submitting form data across the network. The button's `value` attribute is passed directly to `dialog.returnValue`, allowing developers to handle confirm/cancel decisions without custom click handlers.
</details>

### Q5: Can a popover element also be a `<dialog>` element?
<details>
<summary>View Answer</summary>
Yes. You can declare `<dialog popover="auto">`. In this hybrid configuration, the element gains popover light-dismiss capabilities while retaining the semantic role of a dialog. However, opening it via `popovertarget` will open it as a non-modal popover rather than invoking `showModal()`.
</details>

### Q6: How does the browser determine focus trapping in native `<dialog>`?
<details>
<summary>View Answer</summary>
When `.showModal()` is invoked, the browser marks all sibling and ancestor elements outside the `<dialog>` subtree as `inert`. Assistive technologies and keyboard tab events are physically barred from reaching outside content, creating a foolproof native focus trap.
</details>

### Q7: What is the purpose of `@starting-style` when animating popovers?
<details>
<summary>View Answer</summary>
Popovers change from `display: none` to `display: block` upon opening. Because browsers cannot interpolate from an unrendered state, `@starting-style` provides the initial synthetic frame values (e.g., `opacity: 0; transform: translateY(10px);`), allowing the browser to smoothly transition the popover into its open state.
</details>

### Q8: Does `z-index: 9999999` beat an element in the Top Layer?
<details>
<summary>View Answer</summary>
No. The Top Layer exists entirely outside the document's CSS stacking context hierarchy. Even an element with the highest integer `z-index` in the normal DOM will always render underneath any element currently residing in the Top Layer.
</details>

---

## 11. Practice Exercises

### Exercise 1: Basic Confirm Modal with Backdrop Blur
**Objective**: Build a native modal dialog with blurred backdrop and cancel/confirm buttons using `<form method="dialog">`.
<details>
<summary>View Solution</summary>

```css
dialog.confirm-box {
  border: none;
  border-radius: 0.5rem;
  padding: 1.5rem;
  max-width: 24rem;
  box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.2);
}

dialog.confirm-box::backdrop {
  background-color: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(5px);
}
```
</details>

### Exercise 2: Declarative Popover Menu
**Objective**: Create a pure HTML and CSS dropdown menu using `popover="auto"` and `popovertarget`.
<details>
<summary>View Solution</summary>

```html
<button popovertarget="dropdown-panel">Options</button>
<div id="dropdown-panel" popover class="options-menu">
  <button>Rename</button>
  <button>Duplicate</button>
</div>
```

```css
.options-menu {
  border: 1px solid #cbd5e1;
  border-radius: 0.375rem;
  padding: 0.5rem;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
  margin-top: 0.25rem;
}
```
</details>

### Exercise 3: Smooth Slide-In Popover Drawer
**Objective**: Animate a sidebar drawer that smoothly slides in from the right edge when opened as a popover.
<details>
<summary>View Solution</summary>

```css
[popover].side-drawer {
  position: fixed;
  inset: 0 0 0 auto;
  width: 20rem;
  height: 100%;
  max-height: 100%;
  border: none;
  background: white;
  box-shadow: -10px 0 25px rgba(0, 0, 0, 0.15);

  transform: translateX(100%);
  transition:
    transform 0.3s ease-out,
    display 0.3s allow-discrete,
    overlay 0.3s allow-discrete;
}

[popover].side-drawer:popover-open {
  transform: translateX(0);
}

@starting-style {
  [popover].side-drawer:popover-open {
    transform: translateX(100%);
  }
}
```
</details>

### Exercise 4: Dismissible Notification Bubble with `popover="manual"`
**Objective**: Style a persistent floating alert badge with a manual dismiss button.
<details>
<summary>View Solution</summary>

```html
<aside id="alert-bubble" popover="manual" class="alert-toast">
  <span>System update available.</span>
  <button popovertarget="alert-bubble" popovertargetaction="hide">Dismiss</button>
</aside>
```

```css
.alert-toast {
  position: fixed;
  inset: auto 1.5rem 1.5rem auto;
  background-color: #2563eb;
  color: white;
  padding: 0.75rem 1.25rem;
  border-radius: 0.375rem;
  border: none;
}
```
</details>

### Exercise 5: Accessible Dialog Focus Ring
**Objective**: Ensure all interactive form controls within a modal receive high-contrast focus rings when focused via keyboard.
<details>
<summary>View Solution</summary>

```css
dialog.app-modal button:focus-visible,
dialog.app-modal input:focus-visible {
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}
```
</details>

### Exercise 6: Top Layer Clean Reset
**Objective**: Write a reset snippet that normalizes `<dialog>` and `[popover]` browser defaults for custom design systems.
<details>
<summary>View Solution</summary>

```css
:where(dialog, [popover]) {
  border: none;
  padding: 0;
  margin: auto;
  background: transparent;
  color: inherit;
  max-width: initial;
  max-height: initial;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```html
<!-- Native Modal -->
<dialog id="modal">
  <form method="dialog"><button>Close</button></form>
</dialog>
<button onclick="document.getElementById('modal').showModal()">Open</button>

<!-- Native Popover -->
<button popovertarget="menu">Toggle Popover</button>
<div id="menu" popover="auto">Dropdown content</div>
```

```css
/* Animated Entry & Exit */
[popover], dialog {
  opacity: 0;
  transform: scale(0.95);
  transition: opacity 0.2s ease, transform 0.2s ease, display 0.2s allow-discrete, overlay 0.2s allow-discrete;
}
[popover]:popover-open, dialog[open] { opacity: 1; transform: scale(1); }
@starting-style { [popover]:popover-open, dialog[open] { opacity: 0; transform: scale(0.95); } }
```

- `<dialog>` + `showModal()` = Modal + Focus Trap + Inert background.
- `popover="auto"` = Non-modal + Native Light Dismiss.
- Top Layer bypasses all `overflow: hidden` and `z-index` limits.
- Baseline 2024 (Widely available across all modern engines).

---

## 13. Further Reading

- [MDN Web Docs: Popover API](https://developer.mozilla.org/en-US/docs/Web/API/Popover_API)
- [MDN Web Docs: The Dialog Element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/dialog)
- [WHATWG HTML Living Standard: Popover attribute](https://html.spec.whatwg.org/multipage/popover.html)
- [Chrome for Developers: Introducing the popover API](https://developer.chrome.com/blog/introducing-popover-api)

---

### Navigation
- **Previous**: [06-anchor-positioning.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/06-anchor-positioning.md)
- **Next**: [08-color-mix-oklch-and-relative-colors.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/08-color-mix-oklch-and-relative-colors.md)
