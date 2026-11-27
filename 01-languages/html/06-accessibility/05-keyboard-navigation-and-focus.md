# Keyboard Navigation & Focus Management

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `06-accessibility/01-accessibility-fundamentals-and-wcag.md`, `01-fundamentals/09-global-attributes.md` |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Universal HTML5 & CSS Focus Specs) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA, JAWS, VoiceOver, Physical Keyboard Navigation Only |

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

### What is Keyboard Accessibility?
Keyboard accessibility is the foundational prerequisite of digital usability. A web application is keyboard accessible when **every interactive function can be discovered, reached, navigated, operated, and dismissed using solely a keyboard**, without requiring a mouse, trackpad, or touchscreen.

Keyboard navigation is utilized not only by individuals who are blind or low-vision, but also by people with motor disabilities, tremors, repetitive strain injuries (RSI), cerebral palsy, switch access hardware users, power users, and anyone experiencing situational hardware limitations.

### Relevant WCAG Success Criteria
Under WCAG 2.2, keyboard accessibility is enforced across multiple strict criteria:
- **2.1.1 Keyboard (Level A)**: All functionality must be operable through a keyboard interface without requiring specific timings for individual keystrokes.
- **2.1.2 No Keyboard Trap (Level A)**: Keyboard focus must never become trapped in any sub-component. If focus enters a component, standard exit keys (like `Esc` or `Tab`) must allow the user to leave.
- **2.4.3 Focus Order (Level A)**: The sequential tab order must follow a logical, meaningful reading path.
- **2.4.7 Focus Visible (Level AA)**: Any keyboard-operable interface must provide a visually discernible focus indicator.
- **2.4.11 Focus Not Obscured (Minimum) (Level AA)**: Focused elements must not be entirely hidden behind author sticky headers, banners, or floating footers.

---

## 2. Core Concepts

### Sequential Navigation & The Tab Order
By default, the browser allows users to traverse focusable elements sequentially:
- Pressing `Tab` moves focus forward to the next focusable element.
- Pressing `Shift + Tab` moves focus backward to the previous focusable element.
- Pressing `Enter` or `Space` activates buttons.
- Pressing `Enter` follows hyperlinks or submits forms.
- Pressing `Arrow` keys navigates radio groups, select options, and composite widgets (like tabs or sliders).
- Pressing `Escape` closes modals, menus, and popovers.

### The `tabindex` Trinity
The global `tabindex` attribute controls whether and how an element participates in sequential keyboard navigation:

```
+--------------------------------------------------------------------------+
|                            TABINDEX TRINITY                              |
+--------------------------------------------------------------------------+
| Value           | In Tab Order? | Programmatic Focus? | Intended Usage   |
+-----------------+---------------+---------------------+------------------+
| None (Default)  | YES (if native)| YES                | Native buttons,  |
|                 | NO (if div)   | NO                  | links, inputs    |
+-----------------+---------------+---------------------+------------------+
| tabindex="0"    | YES           | YES                 | Custom interactive|
|                 |               |                     | widgets (Tabs)   |
+-----------------+---------------+---------------------+------------------+
| tabindex="-1"   | NO            | YES (.focus())      | Modals, error    |
|                 |               |                     | summaries, panels|
+-----------------+---------------+---------------------+------------------+
| tabindex=">0"   | OUT OF ORDER  | YES                 | ANTI-PATTERN!    |
| (1, 2, 3...)    | (DISRUPTIVE)  |                     | NEVER USE.       |
+--------------------------------------------------------------------------+
```

### Focus Rings: `:focus`, `:focus-visible`, and `:focus-within`
In modern CSS:
- `:focus`: Matches whenever an element receives focus (via mouse click or keyboard).
- `:focus-visible`: Matches only when the browser determines focus should be visually emphasized (e.g., keyboard `Tab` navigation).
- `:focus-within`: Matches a parent container whenever any of its descendant elements receives focus.

---

## 3. How It Works Internally

### The Browser Focus Cycle & Event Flow
When focus moves from Element A to Element B, the browser engine executes a strict, synchronous lifecycle:

```
User presses [Tab] key while on Element A
                  │
                  ▼
         Element A loses focus:
    1. Fires 'focusout' event (bubbles)
    2. Fires 'blur' event (does not bubble)
                  │
                  ▼
         Element B gains focus:
    1. Updates document.activeElement to Element B
    2. Fires 'focusin' event (bubbles)
    3. Fires 'focus' event (does not bubble)
                  │
                  ▼
   Accessibility Engine (AOM) notifies OS:
   Windows: UIA_AutomationFocusChangedEventId
   macOS:   AXFocusedUIElementChangedNotification
                  │
                  ▼
  Screen Reader announces new element to user
```

### Why Positive Tabindex Breaks Focus Architecture
When the HTML parser encounters elements with positive `tabindex` values (`tabindex="1"`, `tabindex="2"`), it separates them into a **priority bucket**. When the user presses `Tab`:
1. The browser visits all positive `tabindex` elements in ascending numerical order (`1`, then `2`, then `3`), completely ignoring DOM hierarchy and visual layout.
2. Only after exhausting all positive elements does the browser begin sequential navigation through normal elements (`tabindex="0"` and native interactive controls).

This causes jarring, unpredictable visual jumps across the viewport, completely disorienting keyboard users.

---

## 4. Syntax & Usage

### Styling Focus with `:focus-visible`
To satisfy WCAG 2.4.7 (Focus Visible) without displaying focus outlines when mouse users click buttons, use the modern `:focus-visible` pattern (Widely Available across all engines):

```css
/* Suppress default browser outline only on pointer clicks */
button:focus:not(:focus-visible),
a:focus:not(:focus-visible) {
  outline: none;
}

/* Guarantee prominent, high-contrast focus indicator for keyboard users */
button:focus-visible,
a:focus-visible,
input:focus-visible,
[tabindex="0"]:focus-visible {
  outline: 3px solid #005a9c;
  outline-offset: 2px;
  border-radius: 2px;
}
```

---

## 5. Practical Examples

### Example 1: Basic — Making a Non-interactive Container Programmatically Focusable
Using `tabindex="-1"` to shift user focus to an Error Summary or modal container via JavaScript without injecting it into the sequential tab sequence.

```html
<!-- Error summary banner placed at top of form -->
<div 
  id="error-summary" 
  class="error-banner" 
  tabindex="-1" 
  role="alert" 
  aria-labelledby="error-heading"
  hidden
>
  <h2 id="error-heading">There are 2 errors in your application</h2>
  <ul>
    <li><a href="#card-number">Card number must be 16 digits</a></li>
    <li><a href="#expiry-date">Expiration date is in the past</a></li>
  </ul>
</div>

<script>
  function handleFormValidationFailure() {
    const errorBanner = document.getElementById('error-summary');
    errorBanner.hidden = false;
    // Programmatically move focus to error banner
    errorBanner.focus();
  }
</script>
```

### Example 2: Real-World — Focus Restoration Pattern
When closing a dialog, menu, or modal, focus **must be restored to the exact trigger element that opened it**, ensuring the user does not lose their place in the document.

```html
<button 
  type="button" 
  id="open-settings-btn" 
  onclick="openSettingsDialog()"
>
  Open Settings
</button>

<dialog id="settings-dialog">
  <h2>Account Settings</h2>
  <button type="button" onclick="closeSettingsDialog()">Save & Close</button>
</dialog>

<script>
  let previouslyFocusedElement = null;

  function openSettingsDialog() {
    // 1. Capture currently focused trigger element
    previouslyFocusedElement = document.activeElement;
    
    const dialog = document.getElementById('settings-dialog');
    dialog.showModal(); // Native <dialog> traps focus automatically!
  }

  function closeSettingsDialog() {
    const dialog = document.getElementById('settings-dialog');
    dialog.close();
    
    // 2. Restore focus back to original trigger button
    if (previouslyFocusedElement) {
      previouslyFocusedElement.focus();
    }
  }
</script>
```

### Example 3: Edge Case — Keyboard Trap Prevention in Custom Overlays
If you must build a custom overlay without native `<dialog>`, you are legally and technically obligated to trap focus inside the modal and allow exit via `Escape`.

```html
<div id="custom-modal" role="dialog" aria-modal="true" aria-labelledby="modal-title" hidden>
  <h2 id="modal-title">Delete Project</h2>
  <button type="button" class="btn-cancel" onclick="closeCustomModal()">Cancel</button>
  <button type="button" class="btn-confirm" onclick="deleteProject()">Confirm</button>
</div>

<script>
  const modal = document.getElementById('custom-modal');

  modal.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      closeCustomModal();
      return;
    }

    if (e.key === 'Tab') {
      const focusable = modal.querySelectorAll('button, [href], input, select, textarea, [tabindex="0"]');
      const first = focusable[0];
      const last = focusable[focusable.length - 1];

      if (e.shiftKey) { // Shift + Tab
        if (document.activeElement === first) {
          e.preventDefault();
          last.focus();
        }
      } else { // Tab
        if (document.activeElement === last) {
          e.preventDefault();
          first.focus();
        }
      }
    }
  });
</script>
```

### Example 4: Failure Case — The Disastrous `outline: none` Anti-Pattern
This code strips away all focus visibility, rendering the site completely unusable for keyboard navigators:

```html
<!-- CATASTROPHIC ACCESSIBILITY BREACH (Fails WCAG 2.4.7) -->
<style>
  * {
    outline: none !important;
  }
  button:focus {
    outline: none;
  }
</style>

<!-- Keyboard users tabbing across these links have ZERO visual feedback -->
<nav>
  <a href="/home">Home</a>
  <a href="/about">About</a>
  <a href="/contact">Contact</a>
</nav>
```

### Example 5: Accessible Production-Grade — Styling Parent Cards with `:focus-within`
Using `:focus-within` to provide a subtle, high-contrast visual focus boundary around an entire card whenever an internal interactive link receives focus.

```html
<style>
  .article-card {
    border: 1px solid #ddd;
    padding: 16px;
    border-radius: 8px;
    transition: box-shadow 0.2s ease, border-color 0.2s ease;
  }

  /* Highlight card container when child link is focused */
  .article-card:focus-within {
    border-color: #005a9c;
    box-shadow: 0 0 0 3px rgba(0, 90, 156, 0.25);
  }

  .article-card h3 a:focus-visible {
    outline: 2px solid #005a9c;
    outline-offset: 4px;
  }
</style>

<article class="article-card">
  <img src="post-thumb.jpg" alt="" width="300" height="180">
  <h3>
    <a href="/blog/semantic-focus">Mastering Focus Management</a>
  </h3>
  <p>Learn how browser engines process focus rings and sequential keyboard order.</p>
</article>
```

---

## 6. Best Practices

1. **Never Use Positive `tabindex`**: Rely entirely on natural DOM source order. Use only `tabindex="0"` or `tabindex="-1"`.
2. **Never Remove Focus Outlines Without High-Contrast Replacements**: Always define a `:focus-visible` rule with at least 3:1 contrast against adjacent background colors.
3. **Always Restore Focus Upon Dismissal**: When closing dialogs, drawers, or dropdowns, return focus to the triggering element.
4. **Use Native `<dialog>` for Modals**: Native `<dialog>` via `.showModal()` handles modal focus trapping and `Escape` key dismissal automatically in C++ engine code.
5. **Manage Focus Shift on In-Page Routing (SPAs)**: In Single Page Applications, when a route changes, programmatically move focus to the new page's `<h1>` (`<h1 tabindex="-1">`) so screen readers announce the new view.
6. **Ensure Touch Targets and Focus Rings Don't Overlap**: Use `outline-offset` to keep focus indicators separate from element borders and text.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | WCAG Success Criterion | Correction |
|:---|:---|:---|:---|
| Removing focus indicators (`outline: none`) | Critical | 2.4.7 Focus Visible (AA) | Replace with custom `:focus-visible` style |
| Positive `tabindex` (`tabindex="5"`) | High | 2.4.3 Focus Order (A) | Reorder elements in HTML source; use `tabindex="0"` |
| Forgetting focus restoration after closing modal | High | 2.4.3 Focus Order (A) | Capture `document.activeElement` before opening and call `.focus()` on close |
| Un-trapped modal allowing background focus | High | 2.1.2 No Keyboard Trap (A) | Use native `<dialog>` or trap `Tab`/`Shift+Tab` |
| Hiding interactive elements with `opacity: 0` | High | 2.4.7 Focus Visible (AA) | Use `display: none`, `hidden`, or `inert` to prevent hidden focus |
| Missing keyboard triggers on custom controls | High | 2.1.1 Keyboard (A) | Listen for `Enter` and `Space` keydown events |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Focus Shift Reflows
Calling `element.focus()` causes the browser layout engine to scroll the focused element into view if it is offscreen (`scrollIntoViewIfNeeded`). In heavy SPAs, calling `.focus()` repeatedly during rendering can trigger multiple synchronous reflows. Ensure focus is only shifted once when an interactive flow completes.

### SEO Considerations
Search bots do not press `Tab` or activate focusable elements. However, keeping the HTML source in a logical reading order directly improves search engine readability and semantic indexation.

### Security: Focus Stealing / Focus Jacking
Malicious scripts can constantly steal focus by invoking `input.focus()` in an interval loop, preventing users from interacting with browser controls, address bars, or password managers. Browsers enforce security boundaries preventing programmatic `.focus()` calls across cross-origin `<iframe>` contexts without user activation.

---

## 9. Comparison / When to Use What

### Focus Management Approaches

| Scenario | Recommended HTML/JS Approach | `tabindex` Value |
|:---|:---|:---:|
| **Standard Link / Button** | Native `<button>` or `<a href="...">` | Omit (Default) |
| **Custom Component in Tab Order** | Custom widget (`role="tab"`) | `tabindex="0"` |
| **Inactive Item in Roving Tabindex** | Unselected tab in a tablist | `tabindex="-1"` |
| **Target of Skip Link / Anchor** | `<main id="main-content" tabindex="-1">` | `tabindex="-1"` |
| **Error Banner / Alert Notification** | Container focused via `.focus()` on error | `tabindex="-1"` |
| **Reordering Visual Layout** | CSS Flexbox/Grid `order` (Use with caution!) | Never use `tabindex=">0"` |

---

## 10. Interview Questions

### Q1: What is the difference between `tabindex="0"`, `tabindex="-1"`, and positive `tabindex` values?
<details>
<summary>View Answer</summary>

- `tabindex="0"`: Inserts the element into the natural sequential keyboard navigation order based on its position in the DOM. It can be reached via `Tab` and focused programmatically.
- `tabindex="-1"`: Removes the element from the sequential keyboard navigation order (cannot be reached via `Tab`), but allows the element to be focused programmatically via JavaScript (`element.focus()`).
- Positive `tabindex` (`tabindex="1"`, `tabindex="2"`): Creates a disjointed, custom tab order that the browser navigates before any normal elements. It is an industry anti-pattern that violates WCAG 2.4.3.
</details>

### Q2: Why is `:focus-visible` preferred over `:focus` for styling focus rings?
<details>
<summary>View Answer</summary>

`:focus` triggers on both mouse clicks and keyboard navigation. Because designers often object to focus rings appearing when a mouse user clicks a button, they historically removed outlines with `outline: none`, completely breaking accessibility for keyboard users.

`:focus-visible` solves this by applying focus styles heuristically only when the browser detects keyboard or assistive device interaction, allowing designers to keep clean visuals for mouse users while providing prominent indicators for keyboard users.
</details>

### Q3: What is a "Keyboard Trap" (WCAG 2.1.2), and how does a proper modal focus trap differ from an inaccessible trap?
<details>
<summary>View Answer</summary>

A **Keyboard Trap** occurs when a keyboard user navigates into an element or component and becomes stuck, unable to move focus away using standard keyboard keys (`Tab`, `Shift+Tab`, `Esc`, `Arrow` keys). This is a catastrophic Level A violation.

In contrast, a **Modal Focus Trap** is an intentional accessibility pattern where focus is constrained within an active modal dialog so the user cannot tab into obscured background elements. To remain compliant, a modal trap **must provide a clear exit mechanism** (such as pressing `Escape` or activating a Close button) that cleanly releases the trap and restores focus.
</details>

### Q4: What is "Focus Restoration", and why is it mandatory when closing dynamic overlays?
<details>
<summary>View Answer</summary>

Focus Restoration is the practice of saving a reference to the element that was focused before an overlay opened (`previouslyFocused = document.activeElement`), and returning focus to that exact element when the overlay closes.

Without focus restoration, closing a modal or dropdown resets focus to the top of the `<body>` element. A screen reader or keyboard user is forced to re-tab through the entire document from the beginning just to get back to where they were.
</details>

### Q5: How does the native HTML `<dialog>` element handle focus trapping and restoration automatically?
<details>
<summary>View Answer</summary>

When opened using `dialog.showModal()`:
1. It automatically traps keyboard focus within the dialog without requiring custom JavaScript listeners.
2. It listens for the `Escape` key and closes the dialog automatically.
3. Upon closing, the browser natively restores focus to the element that had focus when `showModal()` was called.
4. It renders in the browser's top layer and marks background DOM elements inert.
</details>

### Q6: What is the purpose of placing `tabindex="-1"` on `<main>` or the target of a skip link?
<details>
<summary>View Answer</summary>

When a user activates an in-page skip link (`<a href="#main-content">`), browsers scroll to the `#main-content` container. However, some browsers and screen readers do not move programmatic focus unless the target container is focusable.

Adding `tabindex="-1"` to `<main id="main-content" tabindex="-1">` allows the skip link to transfer keyboard focus directly to `<main>`, ensuring the very next `Tab` press focuses the first interactive item inside the main content area.
</details>

### Q7: Explain WCAG 2.2 Success Criterion 2.4.11 (Focus Not Obscured). How does sticky header/footer design impact this?
<details>
<summary>View Answer</summary>

WCAG 2.4.11 (Level AA) requires that when an item receives keyboard focus, it must not be completely obscured by author-created content (such as fixed headers, sticky cookies banners, or floating chat widgets).

If a user tabs through a long page and the focused input scrolls beneath a fixed 80px header, the focus ring is invisible to the user. This can be resolved using CSS `scroll-padding-top` and `scroll-padding-bottom` on the `html` root, or `scroll-margin` on focusable elements.
</details>

### Q8: What does `:focus-within` do, and how can it improve form accessibility?
<details>
<summary>View Answer</summary>

`:focus-within` is a CSS pseudo-class that matches any element if that element or any of its descendants currently matches `:focus` or `:focus-visible`.

In forms, it allows developers to visually highlight an entire fieldset, card, or composite input group (e.g., highlighting a search box container with border and glow) whenever the user focuses the inner `<input>` or clear `<button>`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Restoring an Accessible Focus Outline
**Problem**: Write the CSS rule using `:focus-visible` to give all interactive elements a 3px solid blue outline with a 2px offset, while removing default outlines on mouse clicks.

<details>
<summary>View Solution</summary>

```css
:focus:not(:focus-visible) {
  outline: none;
}

:focus-visible {
  outline: 3px solid #005a9c;
  outline-offset: 2px;
}
```
</details>

### Exercise 2: Easy — Making a Skip Target Focusable
**Problem**: Write the semantic HTML for a skip link and its target `<main>` container, ensuring keyboard focus lands properly on `<main>`.

<details>
<summary>View Solution</summary>

```html
<a href="#main-content" class="skip-link">Skip to main content</a>

<!-- tabindex="-1" enables programmatic focus without adding to regular tab sequence -->
<main id="main-content" tabindex="-1">
  <h1>Dashboard</h1>
</main>
```
</details>

### Exercise 3: Medium — Cleaning Up Positive Tabindex
**Problem**: Refactor this form snippet to remove all positive `tabindex` attributes while preserving a natural keyboard navigation flow.

```html
<form>
  <button type="submit" tabindex="3">Submit</button>
  <input type="text" id="name" tabindex="1">
  <input type="email" id="email" tabindex="2">
</form>
```

<details>
<summary>View Solution</summary>

```html
<!-- Reorder HTML into logical reading order and remove positive tabindex -->
<form>
  <label for="name">Name</label>
  <input type="text" id="name">

  <label for="email">Email</label>
  <input type="email" id="email">

  <button type="submit">Submit</button>
</form>
```
</details>

### Exercise 4: Medium — Implementing Focus Restoration in Vanilla JavaScript
**Problem**: Complete the script so that when `closeDrawer()` is executed, focus returns to the button that opened it.

<details>
<summary>View Solution</summary>

```javascript
let triggerButton = null;

function openDrawer() {
  triggerButton = document.activeElement;
  const drawer = document.getElementById('drawer');
  drawer.hidden = false;
  drawer.querySelector('button, [href]').focus();
}

function closeDrawer() {
  const drawer = document.getElementById('drawer');
  drawer.hidden = true;
  
  if (triggerButton) {
    triggerButton.focus();
  }
}
```
</details>

### Exercise 5: Hard — Preventing Obscured Focus via CSS Scroll Padding
**Problem**: A site has a fixed top navigation bar of 70px height and a floating live-chat widget at the bottom right. Write the CSS rules to guarantee keyboard focused elements are never obscured when scrolled into view.

<details>
<summary>View Solution</summary>

```css
/* Tells browser scrolling mechanisms to offset focused items from viewport edges */
html {
  scroll-padding-top: 80px;    /* 70px bar + 10px breathing room */
  scroll-padding-bottom: 30px;
  scroll-behavior: smooth;
}

:focus-visible {
  scroll-margin-top: 80px;
  scroll-margin-bottom: 30px;
}
```
</details>

### Exercise 6: Hard — Roving Tabindex for an Accessible Toolbar
**Problem**: Write the HTML and JavaScript for a 3-button formatting toolbar implementing the roving `tabindex` pattern (only active button has `tabindex="0"`; Arrow Left/Right navigates between them).

<details>
<summary>View Solution</summary>

```html
<div role="toolbar" aria-label="Text Formatting" id="editor-toolbar">
  <button type="button" tabindex="0">Bold</button>
  <button type="button" tabindex="-1">Italic</button>
  <button type="button" tabindex="-1">Underline</button>
</div>

<script>
  const toolbar = document.getElementById('editor-toolbar');
  const buttons = toolbar.querySelectorAll('button');

  toolbar.addEventListener('keydown', (e) => {
    let index = Array.from(buttons).indexOf(document.activeElement);
    if (index === -1) return;

    if (e.key === 'ArrowRight') {
      e.preventDefault();
      index = (index + 1) % buttons.length;
      shiftFocus(buttons[index]);
    } else if (e.key === 'ArrowLeft') {
      e.preventDefault();
      index = (index - 1 + buttons.length) % buttons.length;
      shiftFocus(buttons[index]);
    }
  });

  function shiftFocus(btn) {
    buttons.forEach(b => b.setAttribute('tabindex', '-1'));
    btn.setAttribute('tabindex', '0');
    btn.focus();
  }
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Keyboard Keys**: `Tab` (forward), `Shift+Tab` (backward), `Enter`/`Space` (activate), `Esc` (dismiss), `Arrows` (widgets).
- **Tabindex Rules**:
  - Omit `tabindex` for native controls.
  - `tabindex="0"`: Adds custom widget into natural tab order.
  - `tabindex="-1"`: Enables programmatic `.focus()` only.
  - Never use positive `tabindex` (`tabindex="1"`).
- **Focus Indicators**: Always use `:focus-visible` with >= 3:1 contrast against adjacent background.
- **Focus Restoration**: Save `document.activeElement` before opening dialogs and restore focus on close.
- **Focus Trapping**: Use native `<dialog>` or trap `Tab`/`Shift+Tab` within custom overlays.
- **WCAG 2.2 2.4.11**: Use `scroll-padding-top` to prevent sticky headers from hiding focused controls.

---

## 13. Further Reading

- [W3C WCAG 2.2: Guideline 2.1 Keyboard Accessible](https://www.w3.org/TR/WCAG22/#keyboard-accessible)
- [W3C WAI-ARIA Authoring Practices Guide: Keyboard Interface Design](https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/)
- [MDN Web Docs: Keyboard-navigable JavaScript Widgets](https://developer.mozilla.org/en-US/docs/Web/Accessibility/Keyboard-navigable_JavaScript_widgets)
- [CSS-Tricks: The Focus-Visible Trick](https://css-tricks.com/almanac/selectors/f/focus-visible/)

---

[Previous: 04 - Accessible Names & The A11y Tree (04-accessible-names-and-the-a11y-tree.md)](04-accessible-names-and-the-a11y-tree.md) | [Section Index](README.md) | [Next: 06 - Skip Links & Landmarks ->](06-skip-links-and-landmarks.md)
