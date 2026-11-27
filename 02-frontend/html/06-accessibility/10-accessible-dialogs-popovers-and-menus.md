# Accessible Dialogs, Popovers & Menus

| Metadata | Details |
|:---|:---|
| **Level** | Advanced |
| **Prerequisites** | `06-accessibility/05-keyboard-navigation-and-focus.md`, `06-accessibility/04-accessible-names-and-the-a11y-tree.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (`<dialog>` & Popover API Baseline 2024) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, Keyboard Only |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Challenge of Overlays in Web Architecture
Overlays—such as modal confirmation prompts, contextual popovers, dropdown disclosures, and action menus—are among the most complex UI components to make accessible. Sighted mouse users see a floating card appearing on top of the page. However, for a screen reader or keyboard user:
- What happens to keyboard focus?
- Can the user accidentally tab into elements hidden behind the overlay?
- Does the screen reader announce that an overlay has opened?
- Can the user dismiss the overlay using standard keys (`Escape`)?
- When closed, does focus return cleanly to the opening trigger?

### The Modern HTML Revolution: `<dialog>` and Popover API
Historically, developers had to build custom modal systems using dozens of lines of fragile JavaScript to trap focus, manage `aria-hidden` across background elements, and fight `z-index` stacking context wars. 

With modern HTML standards (Baseline 2024):
1. **The Native `<dialog>` Element**: Provides browser-native modal dialogs with automatic focus trapping, `Escape` key dismissal, automatic background inertness, and focus restoration.
2. **The Popover API (`popover="auto"`)**: Renders non-modal floating overlays into the browser's **Top Layer** above all other DOM elements with built-in "light dismiss" (dismiss on click outside or `Escape`).

---

## 2. Core Concepts

### The Four Overlay Paradigms
Understanding the precise functional difference between overlay types is essential:

```
+--------------------------------------------------------------------------+
|                         FOUR OVERLAY PARADIGMS                           |
+--------------------------------------------------------------------------+
| Type            | Modal? | Background Inert? | Dismiss Mechanism | Tech  |
+-----------------+--------+-------------------+-------------------+-------+
| 1. Modal Dialog | YES    | YES (inert)       | Esc, Close Button | <dialog>
|                 |        |                   |                   | showModal()
+-----------------+--------+-------------------+-------------------+-------+
| 2. Popover      | NO     | NO (interactive)  | Esc, Click outside| popover
|                 |        |                   |                   | attribute
+-----------------+--------+-------------------+-------------------+-------+
| 3. Disclosure   | NO     | NO                | Toggle button     | <details>
|                 |        |                   |                   | or aria-expanded
+-----------------+--------+-------------------+-------------------+-------+
| 4. App Menu     | NO     | NO                | Esc, Arrows, Tab  | role="menu"
|                 |        |                   |                   | (WAI-ARIA APG)
+--------------------------------------------------------------------------+
```

### The "Do Not Use `role="menu"` for Navigation" Rule
A pervasive and destructive anti-pattern on the web is adding `role="menu"` and `role="menuitem"` to website navigation bars.
- In the WAI-ARIA specification, `role="menu"` is intended **strictly for desktop-style application menus** (like the *File &rarr; Save As* menu in Google Docs or Photoshop).
- Elements with `role="menu"` change screen reader behavior, suppress link announcements, and **require complex Arrow key navigation while disabling standard `Tab` key progression**.
- Website navigation links must use semantic `<nav>` and `<a>` elements, **never** `role="menu"`.

---

## 3. How It Works Internally

### The Browser Top Layer & Inert Subtree
When an author opens a native modal dialog via `dialog.showModal()`:

```
JavaScript executes: dialog.showModal();
                        │
                        ▼
         Browser Rendering Engine (Blink/Gecko/WebKit):
                        │
  ├─► 1. Promotes <dialog> to the Browser TOP LAYER
  │      (Renders above all z-index stacking contexts)
  │
  ├─► 2. Generates ::backdrop pseudo-element covering full viewport
  │
  ├─► 3. Automatically marks the entire preceding DOM document as INERT!
  │      - Elements cannot be clicked or focused
  │      - Accessibility Tree removes background nodes from AT view
  │
  ├─► 4. Traps keyboard Tab focus natively within <dialog> boundaries
  │
  ├─► 5. Binds browser native [Escape] key handler to close dialog
  │
  └─► 6. Captures document.activeElement before opening and
         restores focus automatically when dialog.close() fires!
```

---

## 4. Syntax & Usage

### Native Modal Dialog Pattern

```html
<!-- Trigger Button -->
<button type="button" onclick="openDeleteDialog()">Delete Repository</button>

<!-- Native Modal Dialog -->
<dialog 
  id="delete-dialog" 
  aria-labelledby="dialog-title" 
  aria-describedby="dialog-desc"
>
  <form method="dialog">
    <h2 id="dialog-title">Confirm Deletion</h2>
    <p id="dialog-desc">
      Are you sure you want to permanently delete this repository? 
      This action cannot be undone.
    </p>

    <div class="dialog-actions">
      <!-- Form method="dialog" automatically closes dialog on click! -->
      <button value="cancel">Cancel</button>
      <button value="confirm" class="btn-danger">Confirm Delete</button>
    </div>
  </form>
</dialog>

<script>
  const dialog = document.getElementById('delete-dialog');

  function openDeleteDialog() {
    dialog.showModal(); // Opens modal in top layer with focus trap
  }

  dialog.addEventListener('close', () => {
    if (dialog.returnValue === 'confirm') {
      executeDeletion();
    }
  });
</script>
```

---

## 5. Practical Examples

### Example 1: Basic — Native Popover API (Baseline 2024)
Using the Popover API to create an accessible non-modal info tooltip with zero JavaScript:

```html
<!-- Popover Trigger: connects to popover ID -->
<button type="button" popovertarget="pricing-help">
  <span>Pricing Breakdown Info</span>
  <svg aria-hidden="true" width="16" height="16"><circle cx="8" cy="8" r="7"/></svg>
</button>

<!-- Popover Surface: renders in top layer, automatically supports Escape & Light Dismiss -->
<div id="pricing-help" popover="auto" role="tooltip">
  <h3>Enterprise Billing Tiers</h3>
  <p>Annual billing receives a 20% discount applied at invoice generation.</p>
</div>
```

### Example 2: Real-World — Accessible Disclosure Navigation Submenu
The correct way to build a dropdown navigation submenu (without `role="menu"`):

```html
<nav aria-label="Main">
  <ul>
    <li><a href="/">Home</a></li>
    
    <!-- Collapsible Submenu Item -->
    <li class="nav-item-dropdown">
      <button 
        type="button" 
        id="solutions-btn"
        aria-expanded="false" 
        aria-controls="solutions-menu"
        onclick="toggleSubmenu(this)"
      >
        <span>Solutions</span>
        <svg aria-hidden="true" width="12" height="12"><path d="M2 4l4 4 4-4"/></svg>
      </button>
      
      <ul id="solutions-menu" hidden>
        <li><a href="/solutions/cloud">Cloud Infrastructure</a></li>
        <li><a href="/solutions/security">Cybersecurity</a></li>
        <li><a href="/solutions/ai">Machine Learning</a></li>
      </ul>
    </li>

    <li><a href="/pricing">Pricing</a></li>
  </ul>
</nav>

<script>
  function toggleSubmenu(button) {
    const isExpanded = button.getAttribute('aria-expanded') === 'true';
    const menu = document.getElementById(button.getAttribute('aria-controls'));
    
    button.setAttribute('aria-expanded', String(!isExpanded));
    menu.hidden = isExpanded;
  }
</script>
```

### Example 3: Edge Case — Focus Restoration in Custom Non-Dialog Overlays
If you must build a custom overlay without native `<dialog>`, you must manually implement focus restoration:

```html
<button 
  type="button" 
  id="filter-trigger" 
  aria-expanded="false" 
  aria-controls="filter-drawer"
  onclick="openFilterDrawer()"
>
  Filter Results
</button>

<div id="filter-drawer" hidden>
  <h3>Filter Catalog</h3>
  <button type="button" onclick="closeFilterDrawer()">Apply & Close</button>
</div>

<script>
  let lastFocusedTrigger = null;

  function openFilterDrawer() {
    lastFocusedTrigger = document.activeElement; // 1. Save active trigger
    const drawer = document.getElementById('filter-drawer');
    const trigger = document.getElementById('filter-trigger');
    
    drawer.hidden = false;
    trigger.setAttribute('aria-expanded', 'true');
    drawer.querySelector('button, [href]').focus(); // 2. Shift focus inside
  }

  function closeFilterDrawer() {
    const drawer = document.getElementById('filter-drawer');
    const trigger = document.getElementById('filter-trigger');
    
    drawer.hidden = true;
    trigger.setAttribute('aria-expanded', 'false');
    
    if (lastFocusedTrigger) {
      lastFocusedTrigger.focus(); // 3. Restore focus cleanly!
    }
  }
</script>
```

### Example 4: Failure Case — Broken Modal Anti-Patterns
Common mistakes that break accessibility:

```html
<!-- BROKEN: Calling dialog.show() instead of dialog.showModal() -->
<!-- show() does NOT trap focus and does NOT make background inert! -->
<script>
  dialog.show(); // INACCESSIBLE FOR MODALS!
</script>

<!-- BROKEN: role="dialog" without an accessible name -->
<!-- Screen reader speaks: "Dialog", leaving user blind to its purpose -->
<div role="dialog" aria-modal="true">
  <!-- Missing aria-labelledby or aria-label -->
  <button type="button">X</button>
</div>

<!-- BROKEN: Using role="menu" for standard website links -->
<ul role="menu">
  <li role="menuitem"><a href="/about">About Us</a></li>
</ul>
<!-- Destroys standard Tab navigation and confuses screen readers -->
```

### Example 5: Accessible Production-Grade — `<dialog>` with Accessible Backdrop and Form
Demonstrating styling the native `::backdrop` and using `method="dialog"` for zero-JS dismissal:

```html
<style>
  dialog {
    border: none;
    border-radius: 8px;
    padding: 24px;
    box-shadow: 0 10px 25px rgba(0, 0, 0, 0.2);
    max-width: 500px;
  }
  
  /* Styling the native top-layer backdrop */
  dialog::backdrop {
    background-color: rgba(0, 0, 0, 0.6);
    backdrop-filter: blur(2px);
  }

  dialog:focus-visible {
    outline: none;
  }
</style>

<button type="button" onclick="document.getElementById('invite-modal').showModal()">
  Invite Team Member
</button>

<dialog id="invite-modal" aria-labelledby="invite-title">
  <form method="dialog">
    <h2 id="invite-title">Invite New Collaborator</h2>
    
    <div class="field" style="margin: 16px 0;">
      <label for="collab-email">Email address</label>
      <input type="email" id="collab-email" required>
    </div>

    <div class="modal-buttons" style="display: flex; justify-content: flex-end; gap: 8px;">
      <button value="cancel" formnovalidate>Cancel</button>
      <button value="submit" class="btn-primary">Send Invite</button>
    </div>
  </form>
</dialog>
```

---

## 6. Best Practices

1. **Use `dialog.showModal()` for Modals**: Always call `.showModal()`, never `.show()`, for true modal dialogs.
2. **Always Provide `aria-labelledby` on Dialogs**: Tie the dialog to its primary heading so screen readers announce its title upon entry.
3. **Never Use `role="menu"` for Navigation Links**: Reserve `role="menu"` exclusively for desktop OS application action menus. Use `<nav>` and `<button aria-expanded="...">` for website navigation dropdowns.
4. **Use Popover API for Non-Modal Floating Overlays**: Popovers automatically handle top-layer promotion and light dismissal without `z-index` bugs.
5. **Guarantee Focus Restoration**: When closing any overlay, restore focus to the button that triggered it.
6. **Support the `Escape` Key Everywhere**: Any overlay that opens must close when the user presses `Escape`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Calling `dialog.show()` instead of `showModal()` | Critical | Fails to trap focus; background remains active | Call `dialog.showModal()` |
| Applying `role="menu"` to website navigation links | High | Destroys keyboard Tab order; suppresses link semantics | Use `<nav>` and `<button aria-expanded="...">` |
| Missing accessible name on `<dialog>` | High | Screen reader announces empty "dialog" | Add `aria-labelledby="heading-id"` |
| Forgetting focus restoration on close | High | Focus resets to top of document (`<body>`) | Save `document.activeElement` and restore focus |
| Un-dismissible overlays on mobile | High | Blocks entire viewport with no escape | Provide visible, high-contrast Close button |
| Trapping focus inside a non-modal popover | Medium | Prevents user from interacting with rest of page | Do not trap focus unless modal |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Top Layer Elimination of Stacking Contexts
Prior to `<dialog>` and the Popover API, developers battled CSS `z-index` bugs where an ancestor with `overflow: hidden` or `transform` clipped modals. The browser's **Top Layer** bypasses the standard CSS render tree entirely, rendering directly in a separate display plane, eliminating costly DOM reparenting hacks.

### SEO Considerations
Content inside a closed `<dialog>` (without `open`) or a closed Popover is not displayed visually. However, modern Googlebot executes JavaScript and indexes text content inside `<dialog>` tags if it represents crawlable document data.

### Security Notes
Modal dialogs are frequently used for sensitive authentication or confirmation actions. Ensure that forms inside dialogs use standard anti-CSRF tokens and that the dialog cannot be hijacked via clickjacking by enforcing `Content-Security-Policy: frame-ancestors 'none'`.

---

## 9. Comparison / When to Use What

### Overlay Architectural Decision Matrix

| Requirement | Recommended Element | API Method | Focus Trapped? | Background Active? |
|:---|:---|:---|:---:|:---:|
| **Action Confirmation** | `<dialog>` | `showModal()` | YES | NO (inert) |
| **Contextual Tooltip** | `<div>` with `popover="auto"` | Popover API | NO | YES |
| **Website Nav Dropdown** | `<nav>` + `<ul>` | `aria-expanded` | NO | YES |
| **Desktop App Menu** | `<div>` with `role="menu"` | WAI-ARIA APG | Arrows only | YES |
| **Collapsible FAQ** | `<details>` + `<summary>` | Native HTML | NO | YES |

---

## 10. Interview Questions

### Q1: What is the difference between `dialog.showModal()` and `dialog.show()`?
<details>
<summary>View Answer</summary>

- `dialog.showModal()`: Opens the dialog as a **true modal dialog**. It is rendered in the browser's Top Layer, generates a `::backdrop` overlay, makes all background DOM elements inert, traps keyboard focus inside the dialog, and natively dismisses on `Escape`.
- `dialog.show()`: Opens the dialog as a **non-modal element**. It stays within normal document flow (subject to `z-index` and overflow clipping), does not generate a backdrop, does not trap focus, does not make background content inert, and does not close on `Escape` automatically.
</details>

### Q2: Why is it an accessibility violation to use `role="menu"` and `role="menuitem"` for a website's header navigation?
<details>
<summary>View Answer</summary>

In the WAI-ARIA specification, `role="menu"` represents an **application action menu** (similar to desktop software menus like File, Edit, View in Excel or VS Code). 

When a screen reader encounters `role="menu"`:
1. It enters Application Navigation Mode.
2. It disables standard `Tab` key navigation (users are expected to navigate items using `Up` and `Down` arrow keys).
3. It suppresses standard link announcements.
Standard website navigation links should always use semantic `<nav>` and `<a>` tags with `<button aria-expanded="...">` for disclosures.
</details>

### Q3: What is "Light Dismiss" in the context of the Popover API?
<details>
<summary>View Answer</summary>

"Light Dismiss" is a native browser behavior where an open popover or overlay automatically closes when:
1. The user clicks or taps anywhere outside the popover surface.
2. The user presses the `Escape` key on their keyboard.
3. The user moves keyboard focus outside the popover subtree.
The HTML Popover API (`popover="auto"`) provides light dismiss natively without requiring custom document-level click event listeners.
</details>

### Q4: How does the browser's "Top Layer" solve legacy CSS `z-index` bugs?
<details>
<summary>View Answer</summary>

In standard CSS, an element's `z-index` is constrained by its parent's stacking context (e.g., if a parent has `opacity < 1`, `transform`, or `overflow: hidden`, a child cannot render above sibling stacking contexts).

The browser's **Top Layer** is an independent rendering layer positioned in front of the entire document. Elements promoted to the Top Layer (via `<dialog showModal()>` or `popover`) exist outside the normal page stacking context hierarchy, guaranteeing they render on top of all other elements without requiring `z-index: 999999`.
</details>

### Q5: What is the purpose of `form method="dialog"`?
<details>
<summary>View Answer</summary>

When a `<form method="dialog">` is submitted inside a `<dialog>` element:
1. The browser prevents an HTTP navigation request.
2. It closes the `<dialog>` automatically.
3. It sets the dialog's `returnValue` property to the `value` of the button that was clicked to submit the form (e.g., `<button value="confirm">` sets `dialog.returnValue = "confirm"`).
This enables zero-JavaScript dismissal and response handling for modals.
</details>

### Q6: How should an author handle focus management when a modal dialog is opened?
<details>
<summary>View Answer</summary>

1. **Before opening**: Save a reference to the active element (`lastActive = document.activeElement`).
2. **Upon opening**: Focus should land on the first interactive element inside the dialog (or on a specific primary action button, or on the dialog container itself if it contains long reading text).
3. **During interaction**: Focus must be constrained inside the modal.
4. **Upon closing**: Programmatically restore focus back to `lastActive`. (Note: Native `dialog.showModal()` handles steps 2, 3, and 4 automatically).
</details>

### Q7: Why must a modal `<dialog>` always have an `aria-labelledby` attribute?
<details>
<summary>View Answer</summary>

Without `aria-labelledby`, the dialog lacks an Accessible Name. When the dialog opens and receives focus, the screen reader announces only *"Dialog"* or *"Window"*, giving the visually impaired user no indication of what the modal is for. 

Wiring `aria-labelledby="title-id"` ensures the screen reader announces the title immediately: *"Delete Account, dialog"*.
</details>

### Q8: What is the difference between a modal dialog and a non-modal popover?
<details>
<summary>View Answer</summary>

- **Modal Dialog**: Requires immediate user attention. The background content is made completely inert (cannot be clicked or focused), and keyboard focus is strictly trapped inside the modal until dismissed.
- **Non-Modal Popover**: Supplementary information or actions that allow the user to continue interacting with the background document without trapping focus.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Native Popover Tooltip
**Problem**: Write the HTML for an accessible button that toggles a non-modal popover containing help text using the Popover API.

<details>
<summary>View Solution</summary>

```html
<button type="button" popovertarget="help-popover">
  Help & Guidance
</button>

<div id="help-popover" popover="auto" role="tooltip">
  <p>Password must be at least 12 characters long.</p>
</div>
```
</details>

### Exercise 2: Easy — Modal Dialog Trigger & Close
**Problem**: Write the HTML for a native `<dialog>` that opens with `showModal()` and closes using a native `method="dialog"` button.

<details>
<summary>View Solution</summary>

```html
<button type="button" onclick="document.getElementById('my-dialog').showModal()">
  Open Terms
</button>

<dialog id="my-dialog" aria-labelledby="terms-title">
  <form method="dialog">
    <h2 id="terms-title">Terms of Service</h2>
    <p>Please read our terms before proceeding...</p>
    <button value="close">Accept & Close</button>
  </form>
</dialog>
```
</details>

### Exercise 3: Medium — Correcting an Inaccessible Navigation Dropdown
**Problem**: Refactor this broken navigation snippet that erroneously uses `role="menu"`:

```html
<nav>
  <div role="menu">
    <div role="menuitem"><a href="/docs">Docs</a></div>
  </div>
</nav>
```

<details>
<summary>View Solution</summary>

```html
<nav aria-label="Documentation">
  <ul>
    <li><a href="/docs">Docs</a></li>
  </ul>
</nav>
```
</details>

### Exercise 4: Medium — Custom Disclosure Widget with ARIA
**Problem**: Write the HTML and JavaScript for a collapsible FAQ panel using `aria-expanded` and `aria-controls`.

<details>
<summary>View Solution</summary>

```html
<h3>
  <button 
    type="button" 
    id="faq-btn" 
    aria-expanded="false" 
    aria-controls="faq-ans"
    onclick="toggleFaq(this)"
  >
    What is your return policy?
  </button>
</h3>
<div id="faq-ans" role="region" aria-labelledby="faq-btn" hidden>
  <p>Items can be returned within 30 days of purchase.</p>
</div>

<script>
  function toggleFaq(btn) {
    const isExpanded = btn.getAttribute('aria-expanded') === 'true';
    btn.setAttribute('aria-expanded', String(!isExpanded));
    document.getElementById(btn.getAttribute('aria-controls')).hidden = isExpanded;
  }
</script>
```
</details>

### Exercise 5: Hard — Implementing Manual Focus Restoration
**Problem**: Write the complete JavaScript functions to open and close a custom off-canvas modal drawer, capturing the trigger element and restoring focus upon closure.

<details>
<summary>View Solution</summary>

```javascript
let modalTrigger = null;

function openCustomModal() {
  modalTrigger = document.activeElement; // Save currently focused trigger
  const modal = document.getElementById('custom-modal');
  modal.hidden = false;
  
  // Shift focus to close button inside modal
  const closeBtn = modal.querySelector('.modal-close');
  if (closeBtn) closeBtn.focus();
}

function closeCustomModal() {
  const modal = document.getElementById('custom-modal');
  modal.hidden = true;
  
  // Restore focus to original trigger
  if (modalTrigger) {
    modalTrigger.focus();
  }
}
```
</details>

### Exercise 6: Hard — Automated Trap Listener for Custom Overlays
**Problem**: Write a keydown event listener function that traps the `Tab` and `Shift+Tab` keys inside a given modal element `#app-modal`.

<details>
<summary>View Solution</summary>

```javascript
function attachModalFocusTrap(modalElement) {
  modalElement.addEventListener('keydown', function(e) {
    if (e.key !== 'Tab') return;

    const focusableSelectors = 'button, [href], input, select, textarea, [tabindex="0"]';
    const focusableElements = Array.from(modalElement.querySelectorAll(focusableSelectors))
      .filter(el => !el.hasAttribute('disabled') && el.offsetParent !== null);

    if (focusableElements.length === 0) return;

    const firstElement = focusableElements[0];
    const lastElement = focusableElements[focusableElements.length - 1];

    if (e.shiftKey) { // Shift + Tab
      if (document.activeElement === firstElement) {
        e.preventDefault();
        lastElement.focus();
      }
    } else { // Tab
      if (document.activeElement === lastElement) {
        e.preventDefault();
        firstElement.focus();
      }
    }
  });
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Native `<dialog>`**: Use `.showModal()` for modal overlays. Automatically provides focus trapping, background inertness, `::backdrop`, and `Escape` key dismissal.
- **Popover API (`popover="auto"`)**: Renders non-modal content in the Top Layer with native light dismiss (click outside / Esc).
- **Never Use `role="menu"` for Navigation**: Website navigation menus must use `<nav>`, `<ul>`, and `<a>`.
- **Dialog Naming**: Always supply `aria-labelledby="heading-id"` on `<dialog>`.
- **Form Method Dialog**: `<form method="dialog">` closes the dialog on submit and sets `dialog.returnValue`.
- **Focus Restoration**: Always restore focus to the trigger element when an overlay closes.

---

## 13. Further Reading

- [MDN Web Docs: The Dialog Element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/dialog)
- [MDN Web Docs: Popover API](https://developer.mozilla.org/en-US/docs/Web/API/Popover_API)
- [Adrian Roselli: Don't Use role="menu"](https://adrianroselli.com/2017/10/dont-use-aria-menu-roles-for-site-nav.html)
- [W3C WAI-ARIA Authoring Practices Guide: Dialog (Modal) Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/dialog-modal/)

---

[Previous: 09 - Accessible Tables & Lists (09-accessible-tables-and-lists.md)](09-accessible-tables-and-lists.md) | [Section Index](README.md) | [Next: 11 - Live Regions & Status Messages ->](11-live-regions-and-status-messages.md)
