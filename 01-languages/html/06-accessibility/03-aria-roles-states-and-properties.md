# ARIA Roles, States & Properties

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `06-accessibility/02-semantic-first-and-the-aria-rules.md` |
| **Estimated Reading Time** | 29 minutes |
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

### What are ARIA Roles, States, and Properties?
WAI-ARIA decomposes the user interface into three primary semantic dimensions:
1. **Roles (`role="..."`)**: Define *what an element is* or what purpose it serves within the document hierarchy (e.g., `role="dialog"`, `role="tab"`, `role="progressbar"`). Once set on an element, its role rarely changes during its lifecycle.
2. **States (`aria-*`)**: Define *the dynamic condition* of an element that frequently changes in response to user interaction or asynchronous data updates (e.g., `aria-expanded="false"`, `aria-checked="true"`, `aria-busy="true"`).
3. **Properties (`aria-*`)**: Define *intrinsic characteristics or architectural relationships* between elements that are less prone to change during runtime (e.g., `aria-haspopup="dialog"`, `aria-controls="panel-1"`, `aria-describedby="hint-text"`).

### Why Understanding the Taxonomy is Essential
In complex web applications—such as dashboards, cloud consoles, data grids, and rich text editors—native HTML elements alone are often insufficient to express intricate component relationships. Understanding the precise role taxonomy and the behavioral contract of each state and property prevents developers from producing "Frankenstein components" (e.g., an element with `role="tab"` that behaves like a link, or an `aria-expanded` placed on an un-focusable container).

---

## 2. Core Concepts

### The ARIA Role Taxonomy
WAI-ARIA classifies roles into six formal categories:

```
+--------------------------------------------------------------------------+
|                           ARIA ROLE CATEGORIES                           |
+--------------------------------------------------------------------------+
| 1. WIDGET ROLES      | Interactive standalone or composite controls       |
|                      | (button, checkbox, tab, treeitem, slider, grid)    |
+----------------------+----------------------------------------------------+
| 2. LANDMARK ROLES    | Navigational waypoints for quick rotor access      |
|                      | (banner, main, navigation, search, contentinfo)   |
+----------------------+----------------------------------------------------+
| 3. DOCUMENT ROLES    | Structural content containers (non-interactive)    |
|                      | (article, document, feed, heading, table, list)    |
+----------------------+----------------------------------------------------+
| 4. LIVE REGION ROLES | Dynamic notification emitters                      |
|                      | (alert, status, log, timer, marquee)              |
+----------------------+----------------------------------------------------+
| 5. WINDOW ROLES      | Overlay surfaces floating above normal flow        |
|                      | (dialog, alertdialog)                              |
+----------------------+----------------------------------------------------+
| 6. ABSTRACT ROLES    | Architectural base types in the spec definition    |
|                      | (widget, command, input - NEVER used in HTML!)     |
+--------------------------------------------------------------------------+
```

### States vs Properties
While both are declared as `aria-*` attributes in HTML, their conceptual distinction is critical:

- **States** reflect transient conditions modified by JavaScript as users interact with the page:
  - `aria-expanded`: `true` | `false`
  - `aria-checked`: `true` | `false` | `mixed`
  - `aria-selected`: `true` | `false`
  - `aria-disabled`: `true` | `false`
  - `aria-invalid`: `true` | `false` | `grammar` | `spelling`
  - `aria-busy`: `true` | `false`
  - `aria-current`: `page` | `step` | `location` | `date` | `time` | `true` | `false`

- **Properties** describe persistent configurations, references, and limits:
  - `aria-labelledby`: Space-separated list of element IDs providing the accessible name.
  - `aria-describedby`: Space-separated list of element IDs providing descriptive hint text.
  - `aria-controls`: Space-separated list of element IDs manipulated by this trigger.
  - `aria-haspopup`: `menu` | `listbox` | `tree` | `grid` | `dialog` | `true` | `false`
  - `aria-valuemin` / `aria-valuemax` / `aria-valuenow` / `aria-valuetext`: Range boundaries.
  - `aria-live`: `off` | `polite` | `assertive`

---

## 3. How It Works Internally

### Role Resolution and State Change Propagation
When an author alters an ARIA attribute via JavaScript (`element.setAttribute('aria-expanded', 'true')`), the browser engine executes an internal dispatch sequence:

```
          JavaScript Mutates Attribute:
          button.setAttribute('aria-expanded', 'true');
                          │
                          ▼
             DOM Node Mutation Observer
                          │
                          ▼
            Accessibility Engine (AOM)
      Recalculates state flags for that node
                          │
                          ▼
         Platform Accessibility API Event Fired
      ┌───────────────────┴───────────────────┐
      ▼                                       ▼
Windows (UI Automation):               macOS (NSAccessibility):
UIA_AutomationPropertyChanged          AXValueChangedNotification
      │                                       │
      ▼                                       ▼
NVDA / JAWS Screen Reader:             VoiceOver:
"Expanded" spoken immediately to user  "Expanded" spoken via speech synth
```

### The Role Inheritance Hierarchy
ARIA roles inherit capabilities in an object-oriented taxonomy defined by the W3C. For example:
- `roletype` (Root abstract role)
  - `structure`
    - `section`
      - `landmark`
        - `navigation`
  - `widget`
    - `command`
      - `button`

Because `button` inherits from `command` and `widget`, it supports global ARIA states (`aria-disabled`, `aria-hidden`), but does *not* support properties reserved exclusively for grid or table structures (such as `aria-rowspan`).

---

## 4. Syntax & Usage

### Core ARIA Attributes Reference Table

```
+--------------------------------------------------------------------------+
| ATTRIBUTE          | ALLOWED VALUES                 | PRIMARY USE CASES  |
+--------------------+--------------------------------+--------------------+
| aria-expanded      | true, false, undefined         | Accordions, menus  |
| aria-selected      | true, false, undefined         | Tabs, list options |
| aria-checked       | true, false, mixed, undefined  | Checkbox, switches |
| aria-current       | page, step, location, date, ...| Active nav links   |
| aria-controls      | ID reference string            | Triggers & panels  |
| aria-haspopup      | menu, listbox, tree, dialog... | Dropdowns, dialogs |
| aria-invalid       | true, false, grammar, spelling | Form validation    |
| aria-live          | off, polite, assertive         | Toast messages     |
+--------------------------------------------------------------------------+
```

### The `aria-current` Attribute
`aria-current` informs assistive technologies of the current item within a set of pagination links, breadcrumbs, step flows, or navigation bars:

```html
<nav aria-label="Breadcrumb">
  <ol>
    <li><a href="/">Home</a></li>
    <li><a href="/products">Products</a></li>
    <!-- aria-current="page" announces "Current page: Laptops" -->
    <li><a href="/products/laptops" aria-current="page">Laptops</a></li>
  </ol>
</nav>
```

---

## 5. Practical Examples

### Example 1: Basic — ARIA Switch (Toggle)
A binary toggle switch using `role="switch"` and `aria-checked`.

```html
<button 
  type="button" 
  role="switch" 
  id="notifications-toggle"
  aria-checked="true"
  onclick="toggleNotifications(this)"
>
  <span class="label">Push Notifications</span>
  <span class="track" aria-hidden="true">
    <span class="thumb"></span>
  </span>
</button>

<script>
  function toggleNotifications(button) {
    const isChecked = button.getAttribute('aria-checked') === 'true';
    button.setAttribute('aria-checked', String(!isChecked));
  }
</script>
```

### Example 2: Real-World — Accordion with `aria-expanded` and `aria-controls`
When native `<details>` cannot be used due to multi-container design systems:

```html
<div class="accordion-item">
  <h3>
    <button 
      type="button" 
      id="acc-btn-billing"
      aria-expanded="false" 
      aria-controls="acc-panel-billing"
      onclick="toggleAccordion(this)"
    >
      Billing History & Invoices
    </button>
  </h3>
  <div 
    id="acc-panel-billing" 
    role="region" 
    aria-labelledby="acc-btn-billing" 
    hidden
  >
    <p>View your past monthly billing statements and download PDF invoices.</p>
  </div>
</div>

<script>
  function toggleAccordion(btn) {
    const expanded = btn.getAttribute('aria-expanded') === 'true';
    btn.setAttribute('aria-expanded', String(!expanded));
    document.getElementById(btn.getAttribute('aria-controls')).hidden = expanded;
  }
</script>
```

### Example 3: Edge Case — Tri-State Checkbox (`aria-checked="mixed"`)
A parent checkbox managing multiple sub-options displays a indeterminate/mixed state when only some children are selected.

```html
<div class="checkbox-group">
  <div 
    role="checkbox" 
    id="select-all" 
    tabindex="0" 
    aria-checked="mixed"
    aria-labelledby="select-all-label"
    onkeydown="handleKey(event)"
    onclick="toggleSelectAll()"
  >
    <span class="box">[-]</span>
    <span id="select-all-label">Select All Permissions</span>
  </div>

  <div class="sub-checkboxes" style="margin-left: 20px;">
    <label><input type="checkbox" checked onchange="updateParent()"> Read Access</label>
    <label><input type="checkbox" onchange="updateParent()"> Write Access</label>
    <label><input type="checkbox" onchange="updateParent()"> Admin Access</label>
  </div>
</div>
```

### Example 4: Failure Case — Broken ARIA Attributes
Common blunders that confuse screen readers:

```html
<!-- BROKEN: Using an abstract role in HTML (Abstract roles are forbidden!) -->
<div role="input">Enter Name</div>

<!-- BROKEN: aria-expanded on a non-interactive element -->
<div aria-expanded="false">
  Click here to open
</div>

<!-- BROKEN: Missing companion role for state -->
<!-- aria-selected is only valid on roles: tab, option, gridcell, row, columnheader -->
<button type="button" aria-selected="true">Filter Option</button>
<!-- Correction: Use aria-pressed="true" for toggle buttons! -->
```

### Example 5: Accessible Production-Grade — Accessible Progress Bar with Range Attributes
A complete accessible progress bar adhering to WAI-ARIA guidelines:

```html
<div class="progress-wrapper">
  <div id="upload-status" class="progress-label">Uploading Database Backup...</div>
  
  <div 
    role="progressbar" 
    id="upload-progress"
    aria-labelledby="upload-status"
    aria-valuemin="0" 
    aria-valuemax="100" 
    aria-valuenow="65"
    aria-valuetext="65% complete (Estimated 4 minutes remaining)"
    tabindex="0"
  >
    <div class="progress-fill" style="width: 65%;"></div>
  </div>
</div>

<script>
  function updateProgress(percent, minutesRemaining) {
    const bar = document.getElementById('upload-progress');
    bar.setAttribute('aria-valuenow', percent);
    bar.setAttribute('aria-valuetext', `${percent}% complete (Estimated ${minutesRemaining} minutes remaining)`);
    bar.querySelector('.progress-fill').style.width = `${percent}%`;
  }
</script>
```

---

## 6. Best Practices

1. **Keep Roles Static**: Never change an element's `role` dynamically at runtime. Instead of swapping `role="button"` to `role="link"`, render distinct elements or toggle visibility.
2. **Use `aria-pressed` for Toggle Buttons**: If a button toggles an active state on and off (like a Mute button or Bold formatting button), use `aria-pressed="true|false"`, not `aria-selected` or `aria-expanded`.
3. **Always Supply `aria-controls` for Disclosures**: Tie the controlling button to the target content container using its unique `id`.
4. **Leverage `aria-valuetext` for Human-Readable Ranges**: When displaying complex values (e.g., star ratings, temperature, upload duration), use `aria-valuetext` so screen readers speak "3.5 out of 5 stars" instead of raw numbers.
5. **Never Use Abstract Roles**: Roles like `roletype`, `widget`, `structure`, `command`, and `composite` are spec definitions only; browsers will treat them as invalid.
6. **Synchronize Visual Styles with ARIA Selectors**: Use CSS selectors like `button[aria-expanded="true"]` to drive visual presentation, ensuring your code cannot visually expand without updating the accessibility tree.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
|:---|:---|:---|
| Using `aria-selected` on regular buttons | Invalid ARIA attribute combination; ignored by screen readers | Use `aria-pressed="true|false"` |
| Assigning abstract roles (`role="widget"`) | Browsers ignore the role and mark it generic | Use concrete roles like `role="button"` or `role="dialog"` |
| Forgetting to toggle `aria-expanded` in JS | Screen reader announces "collapsed" even when content is visible | Update attribute immediately upon click |
| Hardcoding `aria-controls` to non-existent ID | Dead reference in accessibility tree | Verify that the target element has the exact corresponding `id` |
| Using `aria-haspopup="true"` for dialogs | Historically mapped to `menu`; confuses screen readers | Use `aria-haspopup="dialog"` explicitly |
| Using `aria-disabled="true"` without disabling clicks | Screen reader announces disabled, but mouse clicks still execute | Add `pointer-events: none` and guard click handlers |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility Engine Tree Sync
When managing large virtualized lists or grids (e.g., 10,000 items), updating thousands of `aria-selected` attributes synchronously will cause noticeable main-thread stuttering. Maintain focus and selection states via roving `tabindex` and update only the previously selected and newly selected nodes.

### SEO Implications
Search engine bots do not index or rank pages based on `aria-expanded`, `aria-selected`, or `aria-checked`. Rely on semantic HTML structure and crawlable anchor tags (`<a href="...">`) to ensure all deep content is discoverable by search spiders.

### Security Notes
Avoid populating `aria-valuetext` or `aria-label` with unescaped user-generated content from query strings or database fields. Injected single or double quotes can lead to HTML attribute injection or XSS vulnerabilities.

---

## 9. Comparison / When to Use What

### Toggle Button vs Checkbox vs Switch

| Component Pattern | Intended Semantics | ARIA Role & State | Typical Interaction |
|:---|:---|:---|:---|
| **Toggle Button** | Pressable state (e.g., Bold, Pin) | `button` + `aria-pressed="true|false"` | Immediate visual effect |
| **Form Checkbox** | Form option selection | `role="checkbox"` + `aria-checked="true|false|mixed"` | Part of submitted form data |
| **Toggle Switch** | Instant on/off preference | `role="switch"` + `aria-checked="true|false"` | Instant setting change (no submit required) |
| **Accordion Trigger** | Shows/hides collapsible content | `button` + `aria-expanded="true|false"` | Expands panel referenced by `aria-controls` |

---

## 10. Interview Questions

### Q1: What is the fundamental difference between an ARIA State and an ARIA Property?
<details>
<summary>View Answer</summary>

- **ARIA States**: Represent dynamic, transient characteristics that change frequently in response to user actions or real-time application updates (e.g., `aria-expanded`, `aria-checked`, `aria-selected`, `aria-busy`, `aria-current`).
- **ARIA Properties**: Represent static or architectural characteristics that describe relationships, configurations, or boundaries, and rarely change during the element's lifecycle (e.g., `aria-labelledby`, `aria-describedby`, `aria-controls`, `aria-haspopup`, `aria-valuemin`, `aria-valuemax`).
</details>

### Q2: What are ARIA Abstract Roles, and why must they never be used in HTML?
<details>
<summary>View Answer</summary>

Abstract roles (such as `roletype`, `structure`, `widget`, `command`, `input`, `composite`, and `range`) are organizational building blocks in the W3C WAI-ARIA specification. They exist solely for the specification authors to define role inheritance and attribute requirements.

Browsers do not map abstract roles to any platform accessibility API. If an author writes `<div role="widget">` or `<div role="command">`, the browser treats the role as completely invalid and falls back to a generic, non-semantic container.
</details>

### Q3: When should an author use `aria-pressed` versus `aria-selected` versus `aria-checked`?
<details>
<summary>View Answer</summary>

- `aria-pressed`: Used on buttons (`<button>` or `role="button"`) to indicate a toggle state that turns an option on or off (e.g., a "Mute Audio" or "Pin Item" button).
- `aria-selected`: Used exclusively on items within selection containers: tabs in a tablist (`role="tab"`), options in a listbox (`role="option"`), or cells in a data grid (`role="gridcell"`).
- `aria-checked`: Used on checkboxes (`role="checkbox"`), radio buttons (`role="radio"`), or switches (`role="switch"`). Unlike `aria-pressed`, `aria-checked` also supports the `mixed` value for indeterminate multi-select states.
</details>

### Q4: What values can `aria-current` take, and what does it convey to assistive technology?
<details>
<summary>View Answer</summary>

`aria-current` indicates that the element represents the current item within a container or set of related elements.
Allowed values include:
- `page`: Current page within a set of navigation links or pagination.
- `step`: Current step within a multi-step checkout or wizard flow.
- `location`: Current visual location highlighted in an architecture diagram or map.
- `date`: Current date within a calendar picker.
- `time`: Current time slot within a timetable.
- `true`: Generic current item.
- `false`: Not current (default).
</details>

### Q5: What values can `aria-haspopup` accept, and why is `aria-haspopup="true"` often misunderstood?
<details>
<summary>View Answer</summary>

Allowed values in ARIA 1.2+ are: `false`, `true`, `menu`, `listbox`, `tree`, `grid`, and `dialog`.

Historically in ARIA 1.0, `aria-haspopup="true"` was synonymous with `aria-haspopup="menu"`. When authors used `aria-haspopup="true"` on a button opening a modal dialog, screen readers announced "Menu button", leading blind users to expect a desktop-style application menu. In modern applications, authors should explicitly specify `aria-haspopup="dialog"` when triggering dialogs.
</details>

### Q6: Why should CSS developers style components using ARIA attribute selectors instead of class names?
<details>
<summary>View Answer</summary>

Using ARIA attributes as CSS selectors (e.g., `button[aria-expanded="true"] .icon { transform: rotate(180deg); }`):
1. **Single Source of Truth**: Guarantees that the visual state cannot change unless the underlying accessibility state is updated in JavaScript.
2. **Prevents Silent Desync**: If a developer forgets to toggle `aria-expanded`, the visual UI will fail to update during QA testing, instantly exposing the bug before reaching production.
</details>

### Q7: What is the purpose of `aria-busy="true"`?
<details>
<summary>View Answer</summary>

`aria-busy="true"` tells assistive technologies that an element is currently undergoing live DOM updates (e.g., loading new feed items, re-rendering a table).

While an element has `aria-busy="true"`, screen readers temporarily suppress reading or firing change notifications on that subtree until `aria-busy="false"` is set, preventing confusing half-rendered announcements.
</details>

### Q8: What is the difference between `aria-disabled="true"` and the native HTML `disabled` attribute?
<details>
<summary>View Answer</summary>

- Native `disabled`: Completely disables the element. The element is removed from the keyboard tab sequence, cannot receive mouse clicks, and does not submit data with forms.
- `aria-disabled="true"`: Communicates to the accessibility tree that the element is disabled, **but the element remains focusable and interactive unless prevented with custom JavaScript and CSS**. This is useful when you want keyboard users to be able to focus a disabled button to hear an explanation tooltip via `aria-describedby` explaining why the button is unavailable.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Implementing `aria-current` on Navigation
**Problem**: Add the correct ARIA attribute to mark the active "Documentation" link in a header navigation bar.

<details>
<summary>View Solution</summary>

```html
<nav aria-label="Main Navigation">
  <ul>
    <li><a href="/">Home</a></li>
    <li><a href="/docs" aria-current="page">Documentation</a></li>
    <li><a href="/pricing">Pricing</a></li>
  </ul>
</nav>
```
</details>

### Exercise 2: Easy — Authoring an Accessible Toggle Button
**Problem**: Author a "Mute Sound" button that uses `aria-pressed` to indicate whether audio is muted.

<details>
<summary>View Solution</summary>

```html
<button 
  type="button" 
  id="mute-btn" 
  aria-pressed="false" 
  onclick="toggleMute(this)"
>
  Mute Sound
</button>

<script>
  function toggleMute(btn) {
    const isMuted = btn.getAttribute('aria-pressed') === 'true';
    btn.setAttribute('aria-pressed', String(!isMuted));
    btn.textContent = !isMuted ? "Unmute Sound" : "Mute Sound";
  }
</script>
```
</details>

### Exercise 3: Medium — Accessible Multi-Step Wizard Progress
**Problem**: Create a 4-step wizard breadcrumb where Step 3 is currently active, using `aria-current="step"`.

<details>
<summary>View Solution</summary>

```html
<nav aria-label="Checkout Progress">
  <ol class="step-wizard">
    <li><a href="/step-1">1. Account Details</a></li>
    <li><a href="/step-2">2. Shipping Address</a></li>
    <li><a href="/step-3" aria-current="step">3. Payment Information</a></li>
    <li aria-disabled="true"><span>4. Order Review</span></li>
  </ol>
</nav>
```
</details>

### Exercise 4: Medium — Fixing a Broken Multi-Menu Trigger
**Problem**: A dropdown button opens a modal dialog, but currently specifies `aria-haspopup="true"`. Fix the markup so screen readers announce a dialog rather than an application menu.

<details>
<summary>View Solution</summary>

```html
<!-- Explicitly set aria-haspopup="dialog" and reference the dialog ID with aria-controls -->
<button 
  type="button" 
  aria-haspopup="dialog" 
  aria-expanded="false" 
  aria-controls="export-dialog"
  onclick="openExportModal()"
>
  Export Data
</button>

<dialog id="export-dialog">
  <h2>Export Options</h2>
  <button type="button" onclick="closeExportModal()">Close</button>
</dialog>
```
</details>

### Exercise 5: Hard — Implementing an Accessible Slider Control
**Problem**: Build a custom slider widget using `role="slider"` with complete range attributes (`aria-valuemin`, `aria-valuemax`, `aria-valuenow`, `aria-valuetext`) and keyboard arrow key handling.

<details>
<summary>View Solution</summary>

```html
<div class="slider-group">
  <label id="volume-label">Audio Volume</label>
  
  <div 
    role="slider" 
    tabindex="0"
    aria-labelledby="volume-label"
    aria-valuemin="0" 
    aria-valuemax="10" 
    aria-valuenow="4" 
    aria-valuetext="Volume level 4 out of 10"
    id="custom-slider"
    onkeydown="handleSliderKey(event)"
  >
    <div class="slider-thumb" style="left: 40%;"></div>
  </div>
</div>

<script>
  function handleSliderKey(e) {
    const slider = e.currentTarget;
    let val = parseInt(slider.getAttribute('aria-valuenow'), 10);
    const min = parseInt(slider.getAttribute('aria-valuemin'), 10);
    const max = parseInt(slider.getAttribute('aria-valuemax'), 10);

    if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {
      e.preventDefault();
      if (val < max) setSliderValue(slider, val + 1);
    } else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {
      e.preventDefault();
      if (val > min) setSliderValue(slider, val - 1);
    }
  }

  function setSliderValue(slider, newVal) {
    slider.setAttribute('aria-valuenow', newVal);
    slider.setAttribute('aria-valuetext', `Volume level ${newVal} out of 10`);
    slider.querySelector('.slider-thumb').style.left = `${newVal * 10}%`;
  }
</script>
```
</details>

### Exercise 6: Hard — Managing `aria-busy` during Asynchronous Table Filtering
**Problem**: A data table updates via `fetch()`. Write the JavaScript logic to apply `aria-busy="true"` to the table container during network latency and remove it when rendering completes.

<details>
<summary>View Solution</summary>

```html
<div id="table-region" aria-live="polite" aria-busy="false">
  <table id="data-table">
    <thead>
      <tr><th>Customer</th><th>Status</th></tr>
    </thead>
    <tbody id="table-body">
      <tr><td>Alice Cooper</td><td>Active</td></tr>
    </tbody>
  </table>
</div>

<script>
  async function reloadTableData() {
    const region = document.getElementById('table-region');
    const tbody = document.getElementById('table-body');
    
    // Suppress screen reader announcements during partial DOM mutations
    region.setAttribute('aria-busy', 'true');
    
    try {
      const response = await fetch('/api/customers');
      const data = await response.json();
      
      tbody.innerHTML = data.map(item => `
        <tr><td>${item.name}</td><td>${item.status}</td></tr>
      `).join('');
    } finally {
      // Re-enable screen reader announcements once DOM tree is stable
      region.setAttribute('aria-busy', 'false');
    }
  }
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

- **Role**: Defines *what* an element is (`role="dialog"`, `role="tab"`). Do not use abstract roles.
- **State**: Dynamic condition that changes frequently (`aria-expanded`, `aria-checked`, `aria-selected`, `aria-busy`).
- **Property**: Structural characteristic or reference (`aria-controls`, `aria-labelledby`, `aria-describedby`, `aria-haspopup`).
- **`aria-current`**: Marks current item in a set (`page`, `step`, `location`).
- **`aria-pressed` vs `aria-checked` vs `aria-selected`**:
  - `aria-pressed`: Toggle buttons (`<button>`).
  - `aria-checked`: Checkboxes and switches.
  - `aria-selected`: Tabs, listbox options, tree items.
- **Drive CSS via ARIA**: Style with `button[aria-expanded="true"]` to avoid desync between visuals and accessibility tree.

---

## 13. Further Reading

- [W3C WAI-ARIA 1.2 Specification: Categorization of Roles](https://www.w3.org/TR/wai-aria-1.2/#roles_categorization)
- [W3C WAI-ARIA Authoring Practices Guide (APG) Patterns](https://www.w3.org/WAI/ARIA/apg/patterns/)
- [MDN Web Docs: ARIA States and Properties Reference](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Attributes)
- [Adrian Roselli: Brief Note on aria-haspopup](https://adrianroselli.com/)

---

[Previous: 02 - Semantic-First Architecture & ARIA Rules (02-semantic-first-and-the-aria-rules.md)](02-semantic-first-and-the-aria-rules.md) | [Section Index](README.md) | [Next: 04 - Accessible Names & The A11y Tree ->](04-accessible-names-and-the-a11y-tree.md)
