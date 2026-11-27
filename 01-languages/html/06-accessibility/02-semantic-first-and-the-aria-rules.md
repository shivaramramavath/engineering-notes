# Semantic-First Architecture & The ARIA Rules

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `06-accessibility/01-accessibility-fundamentals-and-wcag.md`, `02-semantics-and-structure/01-semantic-html-principles.md` |
| **Estimated Reading Time** | 27 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (W3C WAI-ARIA 1.2 / 1.3 Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, axe-core 4.8 |

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

### What is Semantic-First HTML?
**Semantic-First Architecture** is the engineering philosophy where developers exhaust native HTML elements (`<main>`, `<article>`, `<button>`, `<dialog>`, `<details>`, `<table>`, `<input>`) before considering custom markup or accessibility annotations. Native elements carry intrinsic browser behaviors: focusability, keyboard event listeners (`Enter`, `Space`, arrow keys), form association, and automatic translation into platform accessibility APIs.

### What is ARIA and Why Does It Exist?
**WAI-ARIA** (Web Accessibility Initiative – Accessible Rich Internet Applications) is a W3C specification that provides a suite of HTML attributes (`role`, `aria-*`) designed to define user interface roles, states, and properties when native HTML lacks an appropriate semantic element.

ARIA was conceived in the mid-2000s when complex, dynamic Ajax applications emerged, but HTML4 offered only rudimentary form controls and no native landmarks, dialogs, or accordions. However, **ARIA does not add functionality, keyboard behaviors, or focusability to elements**. ARIA only alters how an element is described in the browser's Accessibility Object Model (AOM).

Because improper ARIA actively misleads assistive technologies—creating confusion or making interfaces completely unusable—the W3C codified **The Five Rules of ARIA**.

---

## 2. Core Concepts

### The Five Rules of ARIA
The W3C WAI-ARIA specification formally defines five cardinal rules for authors:

```
+--------------------------------------------------------------------------+
|                          THE FIVE RULES OF ARIA                          |
+--------------------------------------------------------------------------+
| 1. FIRST RULE    | If you can use a native HTML element with built-in    |
|                  | semantics and behavior, DO SO. Do not reinvent it.    |
+------------------+-------------------------------------------------------+
| 2. SECOND RULE   | Do not change native semantics unless truly necessary.|
|                  | (<h2 role="button"> is an anti-pattern).              |
+------------------+-------------------------------------------------------+
| 3. THIRD RULE    | All interactive ARIA controls MUST be fully operable  |
|                  | via keyboard (Tab, Enter, Space, Arrow keys).         |
+------------------+-------------------------------------------------------+
| 4. FOURTH RULE   | Do not use role="presentation" or aria-hidden="true"  |
|                  | on a focusable element (creates "ghost" tab stops).   |
+------------------+-------------------------------------------------------+
| 5. FIFTH RULE    | All interactive elements MUST have an accessible name |
|                  | (via text content, aria-label, or aria-labelledby).  |
+--------------------------------------------------------------------------+
```

### The "No ARIA is Better Than Bad ARIA" Axiom
The W3C WAI-ARIA authoring practices prominently display the principle:
> *"No ARIA is better than Bad ARIA."*

The WebAIM Million annual accessibility analysis of the top 1,000,000 homepages consistently reports that **pages with ARIA attributes present have significantly more detected accessibility errors on average than pages without any ARIA**. Developers frequently sprinkle ARIA attributes like magic incantations, erroneously believing that adding `role="button"` or `role="navigation"` automatically makes broken markup accessible.

### Implicit vs Explicit Roles
Modern HTML5 elements possess **implicit roles** mapped directly to the WAI-ARIA taxonomy by the browser parser:
- `<nav>` has the implicit role `navigation`.
- `<main>` has the implicit role `main`.
- `<button>` has the implicit role `button`.
- `<a>` (with `href`) has the implicit role `link`.
- `<ul>` has the implicit role `list`.
- `<li>` has the implicit role `listitem`.

Explicitly declaring redundant roles—such as `<nav role="navigation">` or `<button role="button">`—is unnecessary noise in modern browsers (Baseline 2024+) and clutters DOM inspection.

---

## 3. How It Works Internally

### Native HTML vs ARIA Pipeline in the Browser Engine
Understanding why native HTML is superior requires observing the pipeline inside browser engines (Chromium/Blink, Gecko, WebKit):

```
Scenario A: Native <button>
[<button type="button">]
       │
       ├─► Parser assigns native button C++ class (HTMLButtonElement)
       ├─► Keyboard Focus Manager adds element to Tab Ring automatically
       ├─► Event Loop binds native click synthesis for [Enter] and [Space]
       ├─► Style Engine applies default user-agent styles & focus indicators
       └─► Accessibility Engine maps node directly to OS Button Primitive
           (UIA: Button, NSAccessibility: NSAccessibilityButtonRole)

Scenario B: Custom ARIA [<div role="button">]
[<div role="button">]
       │
       ├─► Parser assigns generic container class (HTMLDivElement)
       ├─► Accessibility Engine modifies node role in AOM to "Button"
       ├─► NO keyboard focus added! (Requires author to add tabindex="0")
       ├─► NO keyboard events added! (Requires author to write keydown JS for Enter & Space)
       ├─► NO form submission hooks!
       └─► Result: Screen reader announces "Button", but blind user pressing
           Space/Enter experiences complete silence; nothing happens.
```

### ARIA Only Modifies Accessibility Semantics
ARIA attributes act strictly as metadata modifiers for the Accessibility Tree (AOM). ARIA:
- **Does NOT** listen for or emit DOM events.
- **Does NOT** change CSS rendering (unless targeted by attribute selectors like `[aria-expanded="true"]`).
- **Does NOT** make non-interactive elements focusable.
- **Does NOT** prevent mouse or touch interactions.

When you write `<span role="link" href="/page">`, the browser AOM exposes it as a link, but pressing `Enter` will do nothing, and the element cannot be focused with `Tab` unless `tabindex="0"` and custom JavaScript are manually wired.

---

## 4. Syntax & Usage

### Redundant vs Legitimate ARIA Matrix

```
+--------------------------------------------------------------------------+
| ELEMENT / PATTERN           | REDUNDANT / BAD PRACTICE | VALID ARIA USE  |
+-----------------------------+--------------------------+-----------------+
| `<button role="button">`    | Redundant (implicit)     | None needed     |
| `<header role="banner">`    | Redundant at root level  | None needed     |
| `<div role="button">`       | Anti-pattern (use native)| Only if legacy  |
| Collapsible Disclosure      | Native `<details>` preferred| `aria-expanded`|
| Live status announcements   | No native HTML equivalent| `aria-live`     |
| Complex tree / combobox     | No native standard (yet) | WAI-ARIA APG    |
+--------------------------------------------------------------------------+
```

### The Second Rule: Respecting Native Semantics
Never override native element semantics to make them represent completely conflicting concepts:

```html
<!-- CATASTROPHIC ANTI-PATTERN (Violates Rule 2) -->
<h2 role="button" onclick="doSomething()">
  Clickable Heading
</h2>
<!-- Screen readers announce this as a Button, stripping heading rank (H2) -->
<!-- Blind users navigating via the Headings List (H key) cannot find this section -->

<!-- CORRECT PATTERN: Nest the native interactive element inside the heading -->
<h2>
  <button type="button" class="disclosure-btn" onclick="doSomething()">
    Clickable Heading
  </button>
</h2>
```

---

## 5. Practical Examples

### Example 1: Basic — Native Button vs Custom ARIA Re-implementation
Comparing the code volume and fragility of reinventing the wheel:

```html
<!-- 1. Native HTML: 1 line, fully accessible, 0 JavaScript needed for wiring -->
<button type="button" onclick="handleAction()">Perform Action</button>

<!-- 2. Re-inventing the button via ARIA: 15+ lines of fragile JS required -->
<div 
  class="custom-button" 
  role="button" 
  tabindex="0" 
  onclick="handleAction()"
  onkeydown="handleKeyAction(event)"
>
  Perform Action
</div>

<script>
  function handleKeyAction(event) {
    // Manually replicating browser native keyboard behavior
    if (event.key === ' ' || event.key === 'Enter') {
      event.preventDefault(); // Prevent page scroll on Space
      handleAction();
    }
  }
</script>
```

### Example 2: Real-World — Accessible Disclosure Widget (When ARIA is Appropriate)
When dynamic controls are not covered cleanly by native `<details>`, the WAI-ARIA disclosure pattern with `aria-expanded` and `aria-controls` is the industry standard.

```html
<div class="faq-item">
  <h3>
    <button 
      type="button" 
      id="faq-btn-1" 
      aria-expanded="false" 
      aria-controls="faq-panel-1"
      onclick="toggleDisclosure(this)"
    >
      <span>What is the European Accessibility Act?</span>
      <svg class="chevron" aria-hidden="true" width="16" height="16">
        <path d="M4 6l4 4 4-4" stroke="currentColor" fill="none"/>
      </svg>
    </button>
  </h3>
  <div 
    id="faq-panel-1" 
    role="region" 
    aria-labelledby="faq-btn-1" 
    hidden
  >
    <p>
      The European Accessibility Act (EAA) is an EU directive mandating accessibility 
      standards for commercial hardware and digital services by June 2025.
    </p>
  </div>
</div>

<script>
  function toggleDisclosure(button) {
    const isExpanded = button.getAttribute('aria-expanded') === 'true';
    const panel = document.getElementById(button.getAttribute('aria-controls'));
    
    button.setAttribute('aria-expanded', !isExpanded);
    panel.hidden = isExpanded;
  }
</script>
```

### Example 3: Edge Case — Fourth Rule Violation: Hidden Focusable Elements
Violating the Fourth Rule creates invisible "ghost" focus stops where keyboard users tab into an interactive element that assistive technology is explicitly told does not exist.

```html
<!-- SEVERE BUG (Violates Fourth Rule of ARIA) -->
<!-- The parent has aria-hidden="true", but contains focusable children! -->
<div class="promo-modal" aria-hidden="true">
  <h3>Limited Time Offer</h3>
  <!-- Screen reader is completely blinded to this button, yet keyboard user tabs into it! -->
  <button type="button">Claim Discount</button>
</div>

<!-- REMEDIATION: Use the HTML5 inert attribute or native hidden -->
<div class="promo-modal" inert>
  <h3>Limited Time Offer</h3>
  <button type="button">Claim Discount</button>
</div>
```

### Example 4: Failure Case — Broken Custom Dropdown Menu
A common disaster where custom DIVs impersonate a menu without adhering to keyboard specifications:

```html
<!-- BROKEN: Fails Rule 3 (Keyboard Operability) and Rule 5 (Accessible Name) -->
<div class="dropdown-trigger" onclick="openMenu()">
  <span class="icon"></span>
</div>

<div class="dropdown-list" role="menu">
  <div role="menuitem" onclick="navigate('/profile')">Profile</div>
  <div role="menuitem" onclick="navigate('/settings')">Settings</div>
  <div role="menuitem" onclick="logout()">Logout</div>
</div>
<!-- Problems:
  1. Trigger is not focusable (no tabindex or native button).
  2. Role="menu" requires complex ArrowUp / ArrowDown roving tabindex JS.
  3. No Esc key handler to close.
  4. Screen readers get trapped or lost.
-->
```

### Example 5: Accessible Production-Grade — Custom Tabs Component Following WAI-ARIA APG
When authoring custom widgets without native HTML equivalents (like a tabbed interface), strict adherence to the **WAI-ARIA Authoring Practices Guide (APG)** pattern is mandatory.

```html
<div class="tabs-container">
  <!-- Tablist container with role -->
  <div role="tablist" aria-label="Account Settings Sections">
    <button 
      type="button" 
      role="tab" 
      id="tab-profile" 
      aria-selected="true" 
      aria-controls="panel-profile" 
      tabindex="0"
    >
      Profile
    </button>
    <button 
      type="button" 
      role="tab" 
      id="tab-security" 
      aria-selected="false" 
      aria-controls="panel-security" 
      tabindex="-1"
    >
      Security
    </button>
  </div>

  <!-- Tabpanel 1 -->
  <div 
    role="tabpanel" 
    id="panel-profile" 
    aria-labelledby="tab-profile" 
    tabindex="0"
  >
    <h4>Public Profile Details</h4>
    <p>Manage your public username and avatar.</p>
  </div>

  <!-- Tabpanel 2 -->
  <div 
    role="tabpanel" 
    id="panel-security" 
    aria-labelledby="tab-security" 
    tabindex="0" 
    hidden
  >
    <h4>Security Settings</h4>
    <p>Configure two-factor authentication and active sessions.</p>
  </div>
</div>

<script>
  // Roving tabindex and Arrow Key navigation according to APG
  const tablist = document.querySelector('[role="tablist"]');
  const tabs = tablist.querySelectorAll('[role="tab"]');

  tablist.addEventListener('keydown', (e) => {
    let currentIdx = Array.from(tabs).indexOf(document.activeElement);
    if (currentIdx === -1) return;

    if (e.key === 'ArrowRight') {
      currentIdx = (currentIdx + 1) % tabs.length;
      activateTab(tabs[currentIdx]);
    } else if (e.key === 'ArrowLeft') {
      currentIdx = (currentIdx - 1 + tabs.length) % tabs.length;
      activateTab(tabs[currentIdx]);
    }
  });

  function activateTab(tab) {
    tabs.forEach(t => {
      t.setAttribute('aria-selected', 'false');
      t.setAttribute('tabindex', '-1');
      document.getElementById(t.getAttribute('aria-controls')).hidden = true;
    });
    tab.setAttribute('aria-selected', 'true');
    tab.setAttribute('tabindex', '0');
    document.getElementById(tab.getAttribute('aria-controls')).hidden = false;
    tab.focus();
  }
</script>
```

---

## 6. Best Practices

1. **Prioritize Native Semantic Elements**: Always use `<button>`, `<a>`, `<dialog>`, `<details>`, and `<select>` before creating custom widgets with ARIA.
2. **Never Duplicate Implicit Roles**: Avoid `<header role="banner">`, `<nav role="navigation">`, or `<button role="button">`.
3. **Keep ARIA States Synchronized with DOM State**: If you toggle an element with CSS or JavaScript, ensure `aria-expanded`, `aria-checked`, or `aria-selected` is immediately updated.
4. **Use `inert` Instead of Multiple `aria-hidden="true"` Tags**: When masking background content behind a modal or off-canvas drawer, apply the native HTML `inert` attribute to disable both the AOM and keyboard focus simultaneously.
5. **Always Test with a Real Screen Reader**: Automated linters (ESLint JSX-a11y, axe) only catch 30-40% of ARIA misuse. Verifying with NVDA or VoiceOver reveals whether custom ARIA patterns announce coherently.
6. **Implement Keyboard Handlers for Every Custom Role**: If you specify `role="button"`, you are legally and technically obligated to handle `Enter` and `Space` key events.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
|:---|:---|:---|
| Using `role="button"` on a `<div>` without keydown handling | Keyboard users cannot activate the button with Enter or Space | Use native `<button type="button">` |
| Applying `role="presentation"` to a `<button>` | Button role is stripped from the AOM, leaving an unlabelled focusable element | Remove `role="presentation"` from interactive controls |
| Redundant explicit roles (`<main role="main">`) | Clutters markup and flags warnings in strict enterprise audits | Remove explicit `role` attribute |
| Applying `aria-hidden="true"` to a container with focusable buttons | Creates ghost keyboard tab stops completely invisible to screen readers | Use the native `inert` attribute |
| Modifying semantic headings with `role="button"` (`<h3 role="button">`) | Destroys the heading outline for screen reader users | Nest `<button>` inside `<h3>` |
| Inverting `aria-expanded` logic (setting `true` when collapsed) | Screen reader announces false state, confusing visually impaired users | Synchronize attribute with component visibility |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility Tree Invalidation
Dynamically changing ARIA attributes (`aria-expanded`, `aria-valuenow`, `aria-selected`) forces the browser engine to invalidate that node in the Accessibility Object Model and fire an OS-level accessibility event (such as `UIA_AutomationPropertyChangedEventId` on Windows). Avoid firing ARIA property mutations inside high-frequency animation loops or `requestAnimationFrame` callbacks, as this will freeze screen reader speech synthesis.

### SEO Implications
Search engine web bots (Googlebot, Bingbot) primarily ignore ARIA roles and states when determining page indexing and rank. Google's search algorithms rely strictly on the **DOM and native HTML structure** (`<h1>`, `<nav>`, `<article>`, `<a>`). Replacing semantic HTML with ARIA-decorated DIVs damages SEO performance.

### Security: DOM XSS via ARIA Attributes
Developers often dynamically inject user-supplied values into ARIA labels (`button.setAttribute('aria-label', userInput)`). If these labels are rendered server-side without proper HTML entity escaping (`<button aria-label="<%= userInput %>">`), attackers can break out of the attribute quotes and execute arbitrary JavaScript (Cross-Site Scripting). Always treat ARIA attributes with the same strict escaping rigor as text nodes.

---

## 9. Comparison / When to Use What

### HTML vs ARIA vs CSS for State Representation

| State / Behavior | Native HTML | ARIA Attribute | CSS Class |
|:---|:---|:---|:---|
| **Collapsible Open/Closed** | `<details open>` | `aria-expanded="true/false"` | `.is-open` (Invisible to AT) |
| **Disabled Control** | `<button disabled>` | `aria-disabled="true"` | `.disabled` (Focusable, broken) |
| **Selected Tab** | No native equivalent | `aria-selected="true/false"` | `.active` (Invisible to AT) |
| **Required Input** | `<input required>` | `aria-required="true"` | `.is-required` (Ignored by AT) |
| **Inactive Background** | `<div inert>` | `aria-hidden="true"` (Leaves focus) | `pointer-events: none` (Leaves AT) |

---

## 10. Interview Questions

### Q1: State the Five Rules of ARIA and explain the significance of the first rule.
<details>
<summary>View Answer</summary>

The Five Rules of ARIA are:
1. **First Rule**: If you can use a native HTML element with built-in semantics and behavior, do so.
2. **Second Rule**: Do not change native semantics unless truly necessary (e.g., avoid `<h2 role="button">`).
3. **Third Rule**: All interactive ARIA controls must be fully operable via keyboard.
4. **Fourth Rule**: Do not use `role="presentation"` or `aria-hidden="true"` on a focusable element.
5. **Fifth Rule**: All interactive elements must have an accessible name.

The **First Rule** is the cornerstone of web accessibility. Native elements (`<button>`, `<a>`, `<dialog>`) provide focus management, keyboard handlers (`Enter`/`Space`), form submission hooks, and native OS accessibility mappings automatically across all operating systems. Recreating this via custom ARIA requires extensive, error-prone custom JavaScript.
</details>

### Q2: Why is "No ARIA better than Bad ARIA"?
<details>
<summary>View Answer</summary>

Screen readers rely on ARIA to convey an accurate mental model of the interface to users who cannot see the screen. When ARIA is applied incorrectly (e.g., mismatched roles, wrong states, or missing keyboard handlers):
- It actively lies to assistive technology users.
- A button marked with `role="button"` that lacks keydown handlers announces itself as a button, but pressing Enter or Space does nothing, leading users to believe the site is broken or their computer has frozen.
- WebAIM Million data shows pages with ARIA have more accessibility errors than pages without ARIA due to developer misuse.
</details>

### Q3: What is the effect of applying `role="presentation"` or `role="none"` to an element?
<details>
<summary>View Answer</summary>

`role="presentation"` (and its synonym `role="none"`) strips an element's implicit semantic meaning from the Accessibility Tree, treating it as if it were a plain `<div>` or `<span>`. 

For example, `<table role="presentation">` removes table semantics (rows, columns, headers), preventing screen readers from entering data-table navigation mode. However, if `role="presentation"` is placed on a focusable element (like a button or link), the browser will ignore the presentation role to preserve user access.
</details>

### Q4: Why should you avoid `<header role="banner">` or `<button role="button">` in modern web applications?
<details>
<summary>View Answer</summary>

These are redundant ARIA declarations. In modern HTML5 (Baseline 2024+):
- A top-level `<header>` already has the implicit ARIA role of `banner`.
- `<button>` already has the implicit ARIA role of `button`.

Declaring redundant roles bloats markup, clutters developer inspection, and indicates a lack of understanding of modern HTML semantics.
</details>

### Q5: How does `aria-hidden="true"` differ from the HTML5 `hidden` and `inert` attributes?
<details>
<summary>View Answer</summary>

- `aria-hidden="true"`: Removes the element and all its children from the Accessibility Tree, making them invisible to screen readers. **However, it does NOT hide elements visually, does NOT prevent mouse clicks, and does NOT remove elements from the keyboard Tab ring**.
- `hidden` (or `display: none`): Hides elements visually, prevents clicks, removes them from the tab ring, and removes them from the Accessibility Tree.
- `inert`: Leaves elements visually rendered (unless styled otherwise) but completely disables user interaction: elements cannot be clicked, focused via keyboard, found via text search, or perceived by assistive technology.
</details>

### Q6: What happens when an author puts `role="heading" aria-level="2"` on a `<div>` instead of using `<h2>`?
<details>
<summary>View Answer</summary>

While modern screen readers will announce the `<div>` as an "H2 Heading", this approach has significant downsides:
1. It requires extra attributes (`role="heading" aria-level="2"`).
2. It loses all default browser styling (requires custom CSS font size and margins).
3. Search engine crawlers (SEO) may not weight the heading correctly compared to a real `<h2>`.
4. It violates the First Rule of ARIA.
</details>

### Q7: Explain the concept of "Roving Tabindex" and when it must be used with ARIA widgets.
<details>
<summary>View Answer</summary>

**Roving Tabindex** is a keyboard navigation design pattern used in composite widgets like tablists (`role="tablist"`), toolbars (`role="toolbar"`), and menus (`role="menu"`).

Instead of making every child item a separate `Tab` stop:
1. Only the currently active item has `tabindex="0"`.
2. All inactive items have `tabindex="-1"`.
3. Pressing `Tab` moves into and out of the entire widget as a single stop.
4. Once inside the widget, the user uses the `Arrow` keys to navigate between items, dynamically shifting `tabindex="0"` to the focused element.
</details>

### Q8: Can ARIA make an inaccessible JavaScript application accessible by itself?
<details>
<summary>View Answer</summary>

No. ARIA only provides semantic labels, roles, and states for the accessibility tree. It cannot:
- Inject keyboard focusability.
- Add keyboard event listeners.
- Fix color contrast violations.
- Remove pointer-specific traps.
Accessibility requires a holistic engineering approach combining semantic markup, keyboard interaction design, and CSS compliance.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Eliminating Redundant ARIA
**Problem**: Clean up the following markup by removing all redundant or improper ARIA roles while preserving accessibility.

```html
<header role="banner">
  <nav role="navigation">
    <ul role="list">
      <li role="listitem"><a href="/" role="link">Home</a></li>
      <li role="listitem"><a href="/about" role="link">About</a></li>
    </ul>
  </nav>
</header>
```

<details>
<summary>View Solution</summary>

```html
<!-- Native HTML5 elements have implicit roles; all ARIA declarations were redundant -->
<header>
  <nav aria-label="Main">
    <ul>
      <li><a href="/">Home</a></li>
      <li><a href="/about">About</a></li>
    </ul>
  </nav>
</header>
```
</details>

### Exercise 2: Easy — Fixing an Inaccessible Icon Button
**Problem**: An icon-only button uses SVG. Ensure it satisfies Rule 5 (Must have an accessible name).

<details>
<summary>View Solution</summary>

```html
<button type="button" aria-label="Close settings dialog">
  <svg aria-hidden="true" width="24" height="24" viewBox="0 0 24 24">
    <line x1="18" y1="6" x2="6" y2="18" stroke="currentColor" stroke-width="2"/>
    <line x1="6" y1="6" x2="18" y2="18" stroke="currentColor" stroke-width="2"/>
  </svg>
</button>
```
</details>

### Exercise 3: Medium — Correcting a Second Rule Violation
**Problem**: A developer coded an interactive FAQ heading as `<h3 role="button" tabindex="0" onclick="expand()">`. Refactor this to adhere to Rule 2 without losing heading structure or keyboard accessibility.

<details>
<summary>View Solution</summary>

```html
<!-- Retain heading rank for outline navigation, embed native button for interaction -->
<h3>
  <button type="button" aria-expanded="false" onclick="expand()">
    Frequently Asked Question Title
  </button>
</h3>
```
</details>

### Exercise 4: Medium — Fixing a Fourth Rule Ghost Tab Stop
**Problem**: A sidebar drawer slides offscreen using CSS transforms. Screen reader users can still tab into its buttons. Fix the bug using the modern `inert` attribute.

<details>
<summary>View Solution</summary>

```html
<!-- Applying inert simultaneously removes the drawer from the AOM and disables keyboard tab stops -->
<aside class="sidebar-drawer" inert>
  <h2>Navigation Drawer</h2>
  <ul>
    <li><a href="/inbox">Inbox</a></li>
    <li><a href="/drafts">Drafts</a></li>
  </ul>
  <button type="button" onclick="closeDrawer()">Close Drawer</button>
</aside>
```
</details>

### Exercise 5: Hard — Authoring an Accessible Rating Widget (Radio Group)
**Problem**: Create a 5-star rating widget using native semantic `<input type="radio">` elements wrapped in a `<fieldset>`, styled to look like stars, requiring 0 lines of JavaScript for accessibility.

<details>
<summary>View Solution</summary>

```html
<fieldset class="star-rating">
  <legend>Product Rating (out of 5 stars)</legend>
  
  <input type="radio" id="rate-1" name="rating" value="1">
  <label for="rate-1"><span class="sr-only">1 star</span>★</label>

  <input type="radio" id="rate-2" name="rating" value="2">
  <label for="rate-2"><span class="sr-only">2 stars</span>★</label>

  <input type="radio" id="rate-3" name="rating" value="3">
  <label for="rate-3"><span class="sr-only">3 stars</span>★</label>

  <input type="radio" id="rate-4" name="rating" value="4">
  <label for="rate-4"><span class="sr-only">4 stars</span>★</label>

  <input type="radio" id="rate-5" name="rating" value="5">
  <label for="rate-5"><span class="sr-only">5 stars</span>★</label>
</fieldset>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0, 0, 0, 0);
    border: 0;
  }
</style>
```
</details>

### Exercise 6: Hard — Refactoring Custom Toggle Switch to APG Standard
**Problem**: Build a custom accessible switch component using `role="switch"` and `aria-checked` that supports keyboard activation via `Enter` and `Space`.

<details>
<summary>View Solution</summary>

```html
<button 
  type="button" 
  role="switch" 
  id="dark-mode-toggle"
  aria-checked="false" 
  onclick="toggleSwitch(this)"
>
  <span class="switch-label">Enable Dark Mode</span>
  <span class="switch-track" aria-hidden="true">
    <span class="switch-thumb"></span>
  </span>
</button>

<script>
  function toggleSwitch(btn) {
    const isChecked = btn.getAttribute('aria-checked') === 'true';
    btn.setAttribute('aria-checked', String(!isChecked));
    document.body.classList.toggle('dark-theme', !isChecked);
  }
  // Note: Because a native <button> is used, the browser fires click
  // on both Enter and Space keys automatically, satisfying Rule 3!
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

- **Semantic-First Rule**: Always prefer `<button>`, `<a>`, `<dialog>`, `<details>` over ARIA-decorated DIVs.
- **The 5 Rules of ARIA**:
  1. Use native HTML first.
  2. Do not change native semantics.
  3. All interactive ARIA controls must work with the keyboard.
  4. Do not use `role="presentation"` or `aria-hidden="true"` on focusable elements.
  5. All interactive elements must have an accessible name.
- **ARIA is Presentation for the AOM**: ARIA does not grant focus, does not handle keypresses, and does not alter visual styles.
- **Redundant ARIA**: Do not add `<nav role="navigation">` or `<button role="button">`.
- **Ghost Tab Stops**: Never put `aria-hidden="true"` on a container with focusable buttons. Use `inert` instead.

---

## 13. Further Reading

- [W3C WAI-ARIA 1.2 Specification](https://www.w3.org/TR/wai-aria-1.2/)
- [W3C WAI-ARIA Authoring Practices Guide (APG)](https://www.w3.org/WAI/ARIA/apg/)
- [W3C: Using ARIA in HTML](https://www.w3.org/TR/html-aria/)
- [WebAIM: WebAIM Million ARIA Analysis](https://webaim.org/projects/million/)
- [MDN Web Docs: ARIA Introduction](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA)

---

[Previous: 01 - Accessibility Fundamentals & WCAG (01-accessibility-fundamentals-and-wcag.md)](01-accessibility-fundamentals-and-wcag.md) | [Section Index](README.md) | [Next: 03 - ARIA Roles, States & Properties ->](03-aria-roles-states-and-properties.md)
