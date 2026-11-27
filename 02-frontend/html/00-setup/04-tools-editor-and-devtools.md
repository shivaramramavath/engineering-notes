# Developer Tools, Editor Setup & DOM Inspection

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md`, `00-setup/02-how-browsers-turn-html-into-pixels.md` |
| **Time to Complete** | 30 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal Toolchain) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The Difference Between View-Source & The Live DOM](#the-difference-between-view-source--the-live-dom)
4. [Mastering the DevTools Elements Panel](#mastering-the-devtools-elements-panel)
5. [The DevTools Accessibility Tree Inspector](#the-devtools-accessibility-tree-inspector)
6. [VS Code Configuration & Emmet Productivity](#vs-code-configuration--emmet-productivity)
7. [Syntax & Usage](#syntax--usage)
8. [Practical Examples](#practical-examples)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
12. [Comparison Matrix: Browser DevTools Capabilities](#comparison-matrix-browser-devtools-capabilities)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

Writing modern HTML requires more than a simple text editor. Modern web documents are dynamic, responsive, and deeply integrated with browser-level accessibility APIs and graphics hardware.

A professional frontend engineer does not guess how an HTML document behaves; they inspect and verify its runtime state using **Browser Developer Tools (DevTools)**:
- Inspecting how the browser's parser resolved malformed tags into physical DOM nodes.
- Verifying whether a heading or form input was assigned an accessible name in the **Accessibility Object Model (AOM)**.
- Debugging unexpected margins or padding injected by the browser's internal **User Agent Stylesheet**.
- Detecting layout shifts and paint bottlenecks in real-time.

Configuring an optimal editor environment (such as VS Code with Emmet and accessibility linters) and mastering DevTools turns HTML development from error-prone guesswork into deterministic engineering.

---

## Core Concepts

### 1. The Triad of DOM State
When diagnosing HTML, an engineer must distinguish between three distinct representations:
1. **Raw Source Text (`view-source:`)**: The literal bytes transmitted over the network before the parser touched them.
2. **The Live DOM Tree (Elements Panel)**: The living, parsed in-memory tree of nodes, including parser-corrected tags and JavaScript mutations.
3. **The Accessibility Tree (Accessibility Tab)**: The filtered semantic tree exposed to platform assistive technologies (screen readers).

```
+-----------------------------------------------------------------------------------------+
|                                  THE THREE VIEWS OF A WEBPAGE                           |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. RAW SOURCE (Network)                                                                |
|     <table><span>Hello</span><tr><td>1</td></tr>                                        |
|                                                                                         |
|  2. LIVE DOM (Elements Panel)                                                           |
|     body                                                                                |
|       ├── span "Hello" (Foster parented!)                                               |
|       └── table                                                                         |
|             └── tbody (Synthesized!)                                                    |
|                   └── tr                                                                |
|                         └── td "1"                                                      |
|                                                                                         |
|  3. ACCESSIBILITY TREE (A11y Inspector)                                                 |
|     table (Role: table)                                                                 |
|       └── row (Role: row)                                                               |
|             └── cell: "1" (Role: gridcell)                                              |
+-----------------------------------------------------------------------------------------+
```

---

## The Difference Between View-Source & The Live DOM

A common beginner misconception is using **"View Page Source"** (`Ctrl+U` / `Cmd+Option+U`) to debug HTML:
- **View Source shows only the original static string** sent by the web server. It does not reflect parser error recovery, normalized attributes, client-side JavaScript rendering (React, Vue, Web Components), or dynamic DOM insertions.
- **The Elements Panel (`F12` / `Cmd+Option+I`) shows the true, live DOM tree** as it currently exists in browser memory. Always inspect the live DOM in DevTools when debugging markup.

---

## Mastering the DevTools Elements Panel

The Elements Panel provides advanced debugging capabilities beyond simple inspection:
1. **Live DOM Mutation**: Double-click any tag, attribute, or text node to edit it in place. Drag and drop nodes to reorder the tree.
2. **Force Element State (`:hov`)**: Force pseudo-classes (`:hover`, `:active`, `:focus`, `:focus-visible`, `:focus-within`, `:target`) on interactive elements.
3. **DOM Breakpoints**: Right-click any element $\rightarrow$ *Break on*:
   - *Subtree modifications*: Pauses JavaScript execution when children are added or removed.
   - *Attribute modifications*: Pauses when classes, styles, or data attributes mutate.
   - *Node removal*: Pauses when the element is deleted from the tree.
4. **Scroll into View**: Right-click any node $\rightarrow$ *Scroll into view* to immediately locate off-screen or hidden elements.

---

## The DevTools Accessibility Tree Inspector

In modern Chrome, Firefox, and Edge DevTools, you can inspect the full Accessibility Tree:
- Click the **Accessibility** sub-tab in the Elements panel.
- Or click the **Full-Page Accessibility Tree** toggle button in the Elements panel toolbar.

### What the A11y Inspector Exposes:
- **Computed Role**: The explicit or implicit ARIA role (e.g. `button`, `heading`, `link`, `region`).
- **Computed Name**: The accessible name announcing the element, showing the exact cascade of properties (`aria-labelledby` vs `aria-label` vs element text vs `title`).
- **Focusability**: Whether the element has `focusable: true` and its current `tabindex`.
- **States & Properties**: `aria-expanded`, `aria-hidden`, `aria-invalid`, `aria-required`.

---

## VS Code Configuration & Emmet Productivity

VS Code natively includes **Emmet**, a shorthand expansion syntax that expands CSS-like abbreviations into production-grade HTML snippets.

### Essential Emmet Expansions:
- `!` $\rightarrow$ Expands to a complete, valid HTML5 document skeleton.
- `header>nav>ul>li*3>a` $\rightarrow$ Generates a nested navigation tree with 3 links.
- `main>article.card#post-$*2` $\rightarrow$ Generates two article elements with IDs `post-1` and `post-2`.
- `input:email` $\rightarrow$ `<input type="email" name="" id="">`.
- `form:post` $\rightarrow$ `<form action="" method="post"></form>`.

### Recommended VS Code Settings (`settings.json`):
```json
{
  "html.format.enable": true,
  "html.format.wrapLineLength": 120,
  "html.format.unformatted": "wbr",
  "editor.formatOnSave": true,
  "emmet.includeLanguages": {
    "javascript": "javascriptreact"
  }
}
```

---

## Syntax & Usage

```html
<!-- Canonical markup with accessible attributes ready for DevTools inspection -->
<button type="button" 
        id="theme-toggle" 
        aria-pressed="false" 
        aria-label="Toggle dark mode theme">
  <svg aria-hidden="true" width="20" height="20" viewBox="0 0 24 24">
    <!-- SVG graphic hidden from screen readers -->
    <path d="M12 3v1m0 16v1m9-9h-1M4 12H3..."/>
  </svg>
</button>
```

---

## Practical Examples

### Example 1: Inspecting User Agent Default Styles in DevTools
- **Goal**: Observe the browser's default styling applied to semantic elements.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>User Agent Styles</title>
</head>
<body>
  <h1>Heading 1</h1>
  <p>Paragraph text</p>
  <button>Click Me</button>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: In DevTools Styles panel, inspecting `<h1>` reveals `display: block; font-size: 2em; margin-block-start: 0.67em; margin-block-end: 0.67em; font-weight: bold;` marked as `user agent stylesheet`.

### Example 2: Inspecting Accessible Name Computation in DevTools
- **Goal**: Verify how DevTools resolves conflicting accessible names.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>AccName Resolution</title>
</head>
<body>
  <!-- aria-label overrides inner text content in the Accessibility Tree! -->
  <button aria-label="Submit quarterly financial report">
    Send Report
  </button>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
Accessibility Inspector Output:
Node: button
  Role: button
  Name: "Submit quarterly financial report"
  Sources:
    * aria-label: "Submit quarterly financial report" (Active / Highest Priority)
    * Contents: "Send Report" (Ignored due to aria-label override)
```
- **Explanation**: DevTools explicitly illustrates the AccName computation cascade, showing that `aria-label` suppressed the visible text "Send Report" for screen reader output.

### Example 3: Edge Case — Debugging DOM Clobbering in DevTools
- **Goal**: Inspect how named elements pollute the global JavaScript scope.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>DOM Clobbering Demo</title>
</head>
<body>
  <!-- An element with id or name creates a global variable in window! -->
  <form id="userSettings" action="/save">
    <input name="role" value="admin">
  </form>
  <script>
    console.log('Clobbered variable:', window.userSettings);
  </script>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
Console Output:
Clobbered variable: <form id="userSettings" action="/save">...</form>
```
- **Explanation**: In the DevTools Console, typing `window.userSettings` returns the HTMLFormElement node. Naming forms or inputs can accidentally overwrite native JavaScript variables or API methods.

### Example 4: Failure Mode — Tracking Focus Trap Failure with Rendering Emulation
- **Goal**: Identify an inaccessible modal where focus escapes into the background.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Focus Leak</title>
</head>
<body>
  <div class="page-background">
    <a href="/dashboard">Dashboard Link (Accidentally focusable behind modal!)</a>
  </div>
  <div class="custom-modal" role="dialog" aria-modal="true">
    <h2>Notice</h2>
    <button>Close</button>
  </div>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: In DevTools, pressing `Tab` shows the focus ring jumping to the link behind the modal dialog because the background lacks `inert`. Adding the native `inert` attribute to `.page-background` completely blocks focus traversal.

### Example 5: Production Setup with Rendering Emulation & Vision Checks
- **Goal**: Test high-contrast and reduced motion media queries directly in DevTools Rendering tab.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Media Query Emulation</title>
  <style>
    .banner { background: #0044cc; color: #fff; padding: 1rem; }
    @media (prefers-reduced-motion: reduce) {
      .animated { animation: none !important; }
    }
  </style>
</head>
<body>
  <div class="banner">
    <h1>Accessible Animated Banner</h1>
  </div>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: Open DevTools $\rightarrow$ *More tools* $\rightarrow$ *Rendering* $\rightarrow$ Emulate CSS media feature `prefers-reduced-motion: reduce` to verify that animations disable automatically.

---

## Best Practices

1. **Inspect the Accessibility Tree on Every Component**: Never deploy an interactive component (dropdown, modal, accordion) without checking its role and computed name in the DevTools Accessibility tab.
2. **Check the User Agent Stylesheet**: When an element has mysterious default margins (e.g. `<blockquote>`, `<h1>`, `<ul>`), inspect the user agent stylesheet rules in the Styles tab before writing override CSS.
3. **Use DOM Mutation Breakpoints**: When debugging complex JavaScript single-page applications where an element unexpectedly disappears, place a *Break on node removal* breakpoint on the element.
4. **Use Emmet for Rapid, Error-Free Markup**: Expand complete structures with Emmet to guarantee that tags, IDs, and attributes are closed and quoted properly.
5. **Test in Multiple Rendering Engines**: Always verify layout and accessibility across at least Chromium (Chrome/Edge), Gecko (Firefox), and WebKit (Safari).

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Debugging "View Page Source" instead of live DOM** | Looking at static server HTML; missing client JavaScript mutations. | Use the DevTools Elements panel (`F12`) to inspect the live in-memory DOM. |
| **Hover styles cannot be inspected** | Hover menu disappears as soon as cursor moves to DevTools. | Use DevTools `:hov` panel to force the `:hover` pseudo-class permanently. |
| **Screen reader name missing on icon button** | Icon `<button><svg>...</svg></button>` has no visible text or `aria-label`. | Inspect button in A11y tree; add `aria-label="Search"` to the button. |
| **Unexpected margin around entire page** | Browser's user agent stylesheet injects `body { margin: 8px; }` by default. | Add a CSS reset (`body { margin: 0; }`). |
| **Accidental global variable collisions** | Using common IDs like `id="header"` or `id="main"` which become global properties in `window`. | Scope variables cleanly in JavaScript modules; avoid relying on global window variables. |
| **Cannot find which script modified an element** | Manually searching thousands of lines of JavaScript code. | Right-click the element in DevTools $\rightarrow$ *Break on Subtree modifications*. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
Use the DevTools **Performance Panel** to record a trace: look for long red bars indicating "Recalculate Style" or "Layout" to diagnose layout thrashing.

### Accessibility
Use the DevTools **Lighthouse Panel** or the **axe DevTools Extension** to run an automated WCAG 2.2 Level AA audit in 3 seconds directly against the rendered page.

### SEO
Inspect `<head>` tags in the Elements panel to verify that Open Graph and canonical links have rendered their final attribute values accurately after server-side hydration.

### Security
In the DevTools Console, inspect `window.__proto__` and check for DOM Clobbering vulnerabilities by testing whether untrusted HTML inputs can shadow native browser APIs.

---

## Comparison Matrix: Browser DevTools Capabilities

| Feature | Chrome / Edge DevTools (Blink) | Firefox Developer Tools (Gecko) | Safari Web Inspector (WebKit) |
|:---|:---:|:---:|:---:|
| **Full Accessibility Tree View** | **Yes** (Dedicated button) | **Yes** (Full A11y panel) | **Yes** (Node inspection) |
| **DOM Mutation Breakpoints** | Yes | Yes | Yes |
| **Force Pseudo-Classes (:hover, :focus)**| Yes (`:hov` button) | Yes (`:hov` checkboxes) | Yes |
| **Emulate Vision Deficiencies** | **Yes** (Blur, Protanopia, etc.) | **Yes** (Simulator panel) | No (OS-level only) |
| **Scroll into View** | Yes | Yes | Yes |
| **Preload Scanner Visualization** | Yes (Performance track) | Limited | Limited |

---

## Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between "View Page Source" and inspecting an element in the DevTools Elements panel?</strong></summary>

**Answer:**
- **"View Page Source"** displays the exact, raw text stream received from the web server over the network connection before any parsing or script execution took place.
- **The Elements Panel** displays the live, in-memory **Document Object Model (DOM)** tree. It reflects:
  1. The browser's deterministic HTML parser error recovery (e.g. automatically inserted `<tbody>` tags, foster-parented table nodes, self-closing corrections).
  2. All dynamic mutations made by JavaScript (DOM nodes created by React, Vue, or vanilla scripts).
  3. Live attribute and state changes (e.g. `open` on `<dialog>`, `hidden`, class toggles).
</details>

<details>
<summary><strong>2. How do you inspect the computed Accessible Name of an element in Chrome DevTools?</strong></summary>

**Answer:**
1. Open DevTools (`F12` or `Cmd+Option+I`) and switch to the **Elements** panel.
2. Select the target element in the DOM tree.
3. In the right-hand inspection drawer, click the **Accessibility** sub-tab (next to Styles, Computed, Event Listeners).
4. Under the **Computed Properties** section, inspect the **Name** field. 
5. Expand the Name row to inspect the source cascade: DevTools displays which attribute provided the name (`aria-labelledby`, `aria-label`, native label, or inner text) and which properties were discarded.
</details>

<details>
<summary><strong>3. What are DOM Mutation Breakpoints in DevTools and what are their three types?</strong></summary>

**Answer:**
DOM Mutation Breakpoints allow developers to pause JavaScript execution at the exact line of code that mutates a specific DOM node. 
The three types are:
1. **Subtree modifications**: Pauses execution whenever a child node is appended, removed, or reordered within the selected element's subtree.
2. **Attribute modifications**: Pauses execution whenever an attribute (`class`, `style`, `id`, `data-*`) on the selected element is added, changed, or removed.
3. **Node removal**: Pauses execution at the exact moment the selected element itself is removed from the DOM.
</details>

<details>
<summary><strong>4. What is a User Agent Stylesheet, and how can you view it in DevTools?</strong></summary>

**Answer:**
The **User Agent Stylesheet** is the internal, default CSS stylesheet built into the browser rendering engine that provides baseline visual styles for all HTML elements (e.g. making `<h1>` large and bold, `<ul>` indented with bullet points, and `display: none` on `<head>`).

In DevTools, select any element and view the **Styles** tab. Scroll past your author styles: rules labeled with `user agent stylesheet` represent the browser's default styles. They have the lowest priority in the CSS cascade and are easily overridden by author CSS.
</details>

<details>
<summary><strong>5. How can you debug an element whose styles only appear on :hover if moving the mouse to DevTools closes it?</strong></summary>

**Answer:**
In the DevTools Elements panel:
1. Select the target element in the DOM tree.
2. In the Styles tab, click the **`:hov`** button in the top-right corner.
3. Check the **`:hover`** checkbox (or other states like `:focus-visible` or `:active`).
4. DevTools forces the element into the `:hover` pseudo-class state permanently, allowing you to freely inspect and edit its hover styles and child menus without touching the mouse.
</details>

<details>
<summary><strong>6. What is DOM Clobbering, and how can you test for it in the DevTools Console?</strong></summary>

**Answer:**
DOM Clobbering is an injection vulnerability where an attacker uses HTML elements with specific `id` or `name` attributes to overwrite global JavaScript variables or DOM API properties on the `window` or `document` object.

For example, `<form id="config"><input name="url" value="//evil.com"></form>` creates `window.config.url`.
In the DevTools Console, you test for this by evaluating variables: if an application variable `myVar` resolves to an `HTMLFormElement` or `HTMLImageElement` rather than a JavaScript object, DOM Clobbering has occurred.
</details>

<details>
<summary><strong>7. How do you simulate color vision deficiencies (like deuteranopia) using DevTools?</strong></summary>

**Answer:**
1. Open DevTools and press `Ctrl+Shift+P` (or `Cmd+Shift+P` on macOS) to open the Command Menu.
2. Type `Show Rendering` and press Enter to open the **Rendering** tab.
3. Scroll down to the dropdown labeled **"Emulate vision deficiencies"**.
4. Select from available options: *Blurred vision*, *Protanopia* (red blindness), *Deuteranopia* (green blindness), *Tritanopia* (blue blindness), or *Achromatopsia* (complete color blindness). The viewport updates immediately to reflect the simulation.
</details>

<details>
<summary><strong>8. What is the difference between console.log(element) and console.dir(element)?</strong></summary>

**Answer:**
- **`console.log(element)`**: In browser consoles, logging a DOM element outputs its **HTML string representation** as an expandable XML/HTML tree node.
- **`console.dir(element)`**: Outputs the element as a full **JavaScript Object**, displaying all its internal properties, prototype chain (`HTMLElement` $\rightarrow$ `Element` $\rightarrow$ `Node` $\rightarrow$ `EventTarget`), event listeners, and IDL attributes.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Fast Markup Generation with Emmet</strong></summary>

**Problem:** Write the exact Emmet abbreviation to generate a `<nav>` containing an unordered list of 4 items, each containing a link with class `nav-link`.

**Solution:**
```text
nav>ul>li*4>a.nav-link
```
*Expands to:*
```html
<nav>
  <ul>
    <li><a href="" class="nav-link"></a></li>
    <li><a href="" class="nav-link"></a></li>
    <li><a href="" class="nav-link"></a></li>
    <li><a href="" class="nav-link"></a></li>
  </ul>
</nav>
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Forcing Element States in DevTools</strong></summary>

**Problem:** You have a button `<button class="primary-btn">Submit</button>`. Describe the steps in Chrome or Firefox DevTools to inspect its `:focus-visible` styling without pressing the Tab key.

**Solution:**
1. Right-click the button on the page and select **Inspect**.
2. In the Elements panel (Styles pane), click the **`:hov`** toggle button.
3. Check the **`:focus-visible`** checkbox.
4. The button displays its focus ring in the browser window, and the Styles pane shows the active `:focus-visible` CSS rules.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Inspecting Accessible Name Precedence</strong></summary>

**Problem:** In the following markup, what will the DevTools Accessibility tab report as the computed accessible name?
```html
<a href="/cart" aria-label="Shopping Cart (3 items)" title="View Cart">
  Cart (3)
</a>
```

**Solution:**
The computed name will be **`"Shopping Cart (3 items)"`**.
Per the W3C AccName algorithm, `aria-label` takes precedence over both visible inner text (`"Cart (3)"`) and the fallback `title` attribute (`"View Cart"`).
</details>

<details>
<summary><strong>Exercise 4 (Medium): Setting a DOM Breakpoint to Catch Bad Scripts</strong></summary>

**Problem:** A third-party script is maliciously removing your `<header>` element from the page. How do you pinpoint the exact line of JavaScript responsible using DevTools?

**Solution:**
1. In the DevTools Elements panel, locate the `<header>` element.
2. Right-click the `<header>` node $\rightarrow$ hover over **Break on** $\rightarrow$ select **Node removal**.
3. Trigger the page action.
4. The DevTools **Sources** panel immediately pauses JavaScript execution at the exact line of script attempting to remove the header (e.g. `header.remove()` or `parentNode.removeChild()`), revealing the call stack and offending file.
</details>

<details>
<summary><strong>Exercise 5 (Hard): Creating an Automated A11y Verification Script</strong></summary>

**Problem:** Write a Node.js script using the scratch runner to validate an HTML snippet and ensure all `<button>` elements have accessible names.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const snippet = `<!DOCTYPE html>
<html lang="en">
<head><title>A11y Test</title></head>
<body>
  <button aria-label="Close dialog">X</button>
</body>
</html>`;

const result = validateHtml(snippet);
console.log('Result:', result.status, 'Issues:', result.issueCount);
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Simulating User Agent Default Resets</strong></summary>

**Problem:** Write an HTML page comparing default unstyled browser elements (`h1`, `ul`, `fieldset`) side-by-side with modern CSS reset styles.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>User Agent vs Reset</title>
  <style>
    .reset-container * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font: inherit;
    }
  </style>
</head>
<body>
  <div>
    <h2>User Agent Default Styles</h2>
    <ul><li>Default indented item</li></ul>
  </div>
  <div class="reset-container">
    <h2>Reset Styles</h2>
    <ul><li>Clean zero-margin item</li></ul>
  </div>
</body>
</html>
```
</details>

---

## Summary & Further Reading

- The **Live DOM** (Elements panel) reflects real in-memory nodes after parser recovery and JavaScript execution, while **View Source** is merely static raw bytes.
- The **Accessibility Inspector** displays the computed AOM: Role, Name, States, and focusability.
- Use **DOM Mutation Breakpoints** to catch rogue scripts modifying the tree.
- Leverage **Emmet** abbreviations in VS Code for rapid, syntactically perfect markup authoring.
- **Next Topic**: [05-your-first-page.md](05-your-first-page.md) guides you through constructing a complete, responsive, and standards-compliant HTML5 web page from scratch.

---
[Previous](03-html-history-and-the-living-standard.md) | [Section Index](README.md) | [Next](05-your-first-page.md)
