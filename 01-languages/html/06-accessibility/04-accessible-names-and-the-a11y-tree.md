# Accessible Names & The Accessibility Tree

| Metadata | Details |
|:---|:---|
| **Level** | Advanced |
| **Prerequisites** | `06-accessibility/02-semantic-first-and-the-aria-rules.md`, `06-accessibility/03-aria-roles-states-and-properties.md` |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (W3C Accessible Name and Description Computation 1.2) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, Chrome DevTools AOM Inspector |

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

### What is an Accessible Name?
Every interactive user interface element exposed to assistive technologies—such as buttons, links, inputs, dialogs, and landmarks—must have an **Accessible Name**. The accessible name is the primary human-readable string by which assistive technology identifies and refers to that node (e.g., when a screen reader focuses a control, it speaks the accessible name first, followed by its role and state: *"Submit Application, button"*).

Without an accessible name, a button is announced merely as *"button"*, leaving visually impaired users with no indication of what clicking it will do.

### What is the Accessibility Object Model (AOM) / Accessibility Tree?
The **Accessibility Tree** is an in-memory data structure constructed by the web browser alongside the DOM and CSSOM. Assistive technologies (screen readers like NVDA, JAWS, VoiceOver; speech input software like Dragon NaturallySpeaking; braille displays) do not query the raw DOM. Instead, they interact with the host operating system's platform accessibility APIs, which are populated directly from the browser's Accessibility Tree.

### The AccName Specification
Because multiple competing attributes and text sources can define an element's name (e.g., inner text, `<label>`, `aria-label`, `aria-labelledby`, `title`, `placeholder`), the W3C published the **Accessible Name and Description Computation (AccName)** specification. AccName defines an unambiguous, deterministic priority algorithm that every browser engine must follow.

---

## 2. Core Concepts

### The Hierarchy of Accessible Name Resolution (AccName 1.2)
When calculating an element's accessible name, the browser evaluates candidate text sources in a strict cascade. As soon as a step yields a non-empty string, computation ceases:

```
+--------------------------------------------------------------------------+
|                  ACCNAME 1.2 RESOLUTION CASCADE ORDER                    |
+--------------------------------------------------------------------------+
| 1. aria-labelledby | Highest priority. Resolves space-separated IDs.     |
|                    | Can reference hidden nodes. Prevents recursive loops|
+--------------------+-----------------------------------------------------+
| 2. aria-label      | Overrides all subtree text and native HTML labels.  |
|                    | Must NOT be used when visible text is present.      |
+--------------------+-----------------------------------------------------+
| 3. Native Labels   | Element-specific HTML mechanisms:                   |
|                    | - <label for="id"> on form inputs                   |
|                    | - <caption> on <table>                              |
|                    | - <legend> on <fieldset>                            |
|                    | - alt="..." on <img>                                |
|                    | - <summary> on <details>                            |
+--------------------+-----------------------------------------------------+
| 4. Subtree Text    | Text content of the element and its descendants     |
|                    | (e.g., innerText of <button> or <a>).               |
+--------------------+-----------------------------------------------------+
| 5. Fallback Attrs  | Last resort (Poor practice!):                       |
|                    | - title attribute                                   |
|                    | - placeholder attribute (Inputs only)               |
+--------------------------------------------------------------------------+
```

### Accessible Name vs Accessible Description
An accessibility node has two primary textual channels:
1. **Name**: The concise identity of the element (*"Search"*).
2. **Description**: Secondary explanatory details, constraints, or hints (*"Must contain at least 8 characters including one number"*).

The description is computed via `aria-describedby` or native elements like `<table summary="...">`. Assistive technologies speak the Name first, pause briefly, and then speak the Description.

```html
<!-- Name: "Username" | Description: "Your public handle" -->
<label for="user-field">Username</label>
<input 
  type="text" 
  id="user-field" 
  aria-describedby="user-hint"
>
<span id="user-hint">Your public handle</span>
```

---

## 3. How It Works Internally

### AccName 1.2 Computation Algorithm Walkthrough
The AccName algorithm contains sophisticated edge-case handling to resolve complex references while preventing infinite recursion:

```
                         Start AccName Computation for Target Node
                                            │
                                            ▼
                    Is target hidden? (display: none / visibility: hidden)
                                            │
                             ┌──────────────┴──────────────┐
                            YES                            NO
                             │                             │
             Was node referenced by                        ▼
             aria-labelledby?               Does target have aria-labelledby?
                             │                             │
                      ┌──────┴──────┐               ┌──────┴──────┐
                     YES            NO             YES            NO
                      │              │              │              │
                      │         Return empty        │              ▼
                      ▼                             ▼       Does target have aria-label?
                Compute text                  Traverse IDs         │
                from node's                   in attribute  ┌──────┴──────┐
                text content                  list and join YES           NO
                                              results        │             │
                                                             ▼             ▼
                                                        Return label  Check Native HTML
                                                                      label (<label>, <caption>,
                                                                      alt, innerText, etc.)
```

### Preventing Infinite Loops in `aria-labelledby`
Consider an accidental self-reference:
`<button id="btn" aria-labelledby="btn">Click</button>`
The AccName specification mandates a recursion guard: during `aria-labelledby` traversal, a node cannot be referenced by an `aria-labelledby` that itself originated from the traversal chain. When recursion is detected, the engine drops to Step 4 (Subtree Text) and reads the inner text `"Click"`.

### Hidden Content Resolution Exception
A unique property of `aria-labelledby` is that **it can extract text from elements styled with `display: none` or `visibility: hidden`**. 

```html
<!-- The span is visually hidden, but aria-labelledby still extracts its text! -->
<span id="secret-label" style="display: none;">Delete User Record</span>
<button type="button" aria-labelledby="secret-label">
  <svg aria-hidden="true">...</svg>
</button>
<!-- Accessible Name computed: "Delete User Record" -->
```

---

## 4. Syntax & Usage

### Inspecting the Accessibility Tree in Chrome DevTools
Modern browsers allow full inspection of the Accessibility Tree:
1. Open DevTools (`F12` or `Ctrl+Shift+I`).
2. Navigate to the **Elements** panel.
3. In the right-hand inspection drawer, select the **Accessibility** tab.
4. Expand **Computed Properties** to see:
   - **Name**: The exact string computed by AccName.
   - **Source**: Which attribute or node provided the name (e.g., `aria-labelledby`, `innerText`, `Contents`).
   - **Role**: The resolved platform accessibility role.
5. In Chromium, click the **"Enable full-page accessibility tree"** icon in the top right of the Elements DOM tree to switch the entire DOM view into a live screen-reader AOM hierarchy!

---

## 5. Practical Examples

### Example 1: Basic — AccName Precedence Demonstration
Observing how `aria-label` completely overrides native subtree text:

```html
<!-- Subtree text is "Previous Page", but aria-label overrides it -->
<!-- Screen reader speaks: "Go back one step, link" -->
<a href="/page-1" aria-label="Go back one step">
  Previous Page
</a>

<!-- WARNING: This violates WCAG 2.5.3 (Label in Name)! -->
<!-- Speech-to-text users saying "Click Previous Page" will FAIL because
     the accessible name is "Go back one step". -->
```

### Example 2: Real-World — Labeled By Multiple Element IDs
`aria-labelledby` can concatenate multiple strings across distinct DOM nodes to construct a complete, contextual accessible name:

```html
<div class="user-row">
  <span id="user-name-101">Sarah Connor</span>
  <span id="user-role-101">Systems Administrator</span>
  
  <!-- Concatenates two nodes: "Delete Sarah Connor Systems Administrator" -->
  <button 
    type="button" 
    aria-labelledby="btn-action-101 user-name-101"
  >
    <span id="btn-action-101">Delete</span>
    <svg aria-hidden="true" width="16" height="16"><path d="..."/></svg>
  </button>
</div>
```

### Example 3: Edge Case — Dialog Labeling via `aria-labelledby` and `aria-describedby`
Modal dialogs must have their accessible name derived from their primary heading, and their accessible description derived from their introductory paragraph.

```html
<dialog 
  id="confirm-modal" 
  aria-labelledby="dialog-title" 
  aria-describedby="dialog-desc"
>
  <h2 id="dialog-title">Permanently Delete Repository?</h2>
  <p id="dialog-desc">
    This action cannot be undone. All branches, pull requests, and commit 
    history will be permanently deleted from the database.
  </p>
  
  <div class="dialog-actions">
    <button type="button" onclick="closeModal()">Cancel</button>
    <button type="button" class="btn-danger" onclick="confirmDelete()">Confirm Deletion</button>
  </div>
</dialog>
```

### Example 4: Failure Case — Broken Accessible Name Anti-Patterns
Common mistakes leading to unnamed elements or broken screen reader announcements:

```html
<!-- BROKEN: Empty button with non-text child and no label -->
<!-- Screen reader announces: "Button" or "Unlabeled 0" -->
<button type="button">
  <i class="fa fa-trash"></i>
</button>

<!-- BROKEN: Using placeholder as the sole accessible name -->
<!-- Placeholders disappear on input and fail contrast requirements -->
<input type="text" placeholder="Search orders...">

<!-- BROKEN: Broken aria-labelledby pointing to non-existent ID -->
<button type="button" aria-labelledby="non-existent-id">
  <span></span>
</button>
```

### Example 5: Accessible Production-Grade — WCAG 2.5.3 (Label in Name) Compliance
WCAG Success Criterion 2.5.3 mandates that if an element contains visible text, its Accessible Name **must contain that visible text verbatim**, preferably at the start of the accessible name.

```html
<!-- ACCESSIBLE & WCAG 2.5.3 COMPLIANT -->
<!-- Visible text: "View Details" -->
<!-- Accessible Name: "View Details for Order #4920" -->
<!-- Voice users saying "Click View Details" succeed instantly! -->
<a 
  href="/orders/4920" 
  aria-label="View Details for Order #4920"
>
  View Details
</a>

<!-- ALTERNATIVE (Pure HTML without aria-label): Screen Reader Only Span -->
<a href="/orders/4920">
  View Details <span class="sr-only">for Order #4920</span>
</a>

<style>
  .sr-only {
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
</style>
```

---

## 6. Best Practices

1. **Adhere to WCAG 2.5.3 (Label in Name)**: If visible text is rendered inside a button or link, that exact text must be present inside the computed accessible name.
2. **Prefer Visible Native Labels**: Use `<label for="id">` for forms and visible inner text for buttons. Visible labels benefit all users, including those with cognitive impairments.
3. **Use `aria-label` Only for Icon-Only Controls**: Reserve `aria-label` for controls that legitimately have no visible text (e.g., search icon buttons, hamburger menus, audio controls).
4. **Never Use Placeholder as a Replacement for an Accessible Name**: Placeholders disappear once text is typed and have poor contrast by default.
5. **Concatenate Context with `aria-labelledby`**: When buttons repeat across a table (e.g., "Edit", "Edit", "Edit"), use `aria-labelledby` to append the row's entity name.
6. **Verify Computed Names in DevTools**: Always inspect the Accessibility tab in browser DevTools to ensure the computed name matches your mental model.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Impact | Correction |
|:---|:---|:---|:---|
| Icon-only button with no `aria-label` or text | Critical | Announced as empty "button" | Add `aria-label="Descriptive action"` |
| `aria-label` completely replaces visible text | High | Breaks speech-to-text control (WCAG 2.5.3) | Ensure `aria-label` begins with the visible text |
| Misspelled ID in `aria-labelledby` | High | Element loses its accessible name | Verify referenced IDs exist in the DOM |
| Using `title` attribute for accessible name | Medium | Mobile users cannot hover; poor screen reader support | Use visible text or `aria-label` |
| Overriding `<label>` with an incorrect `aria-label` | Medium | Screen reader ignores the visible `<label>` text | Remove `aria-label` and let native `<label>` resolve |
| Hidden label without ID association | Medium | Screen readers cannot locate the text source | Wire with `aria-labelledby="id"` |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility Tree Mutation Costs
Every time an author updates `aria-label` or `aria-labelledby`, the browser must synchronously recompute the AccName algorithm for that node and its subtree, and notify the OS platform accessibility bridge. Batch label mutations and avoid updating labels in animation loops.

### SEO Implications
Search engine bots (Googlebot) read visible text content and standard HTML attributes (`alt`, `title`). While Googlebot parses some ARIA attributes, standard visible anchor text and semantic headings carry exponentially higher SEO weight than `aria-label`.

### Security Considerations
Ensure that dynamic variables injected into `aria-label` or `aria-description` are properly sanitized. Injecting raw user input into attribute strings can lead to attribute breakout and Reflected/Stored Cross-Site Scripting (XSS).

---

## 9. Comparison / When to Use What

### Methods for Providing Accessible Names

| Method | Visible on Screen? | Precedence in AccName | Best Used For |
|:---|:---:|:---:|:---|
| **Visible Subtree Text** | Yes | 4 | Standard buttons, links, table headers |
| **`<label for="id">`** | Yes | 3 | All form inputs (`<input>`, `<select>`, `<textarea>`) |
| **`aria-label`** | No | 2 | Icon-only buttons (close, menu, search) |
| **`aria-labelledby`** | Yes (References ID) | 1 (Highest) | Dialogs, complex tables, multi-node names |
| **`alt` attribute** | No (Unless image fails)| 3 | `<img>`, `<area>`, `<input type="image">` |
| **`title` attribute** | Hover tooltip only | 5 (Lowest fallback) | Desktop advisory tooltips (Never as primary name) |

---

## 10. Interview Questions

### Q1: What is the AccName specification, and why is it necessary?
<details>
<summary>View Answer</summary>

The **Accessible Name and Description Computation (AccName)** specification is a W3C standard that defines how browser engines calculate the text string that identifies a user interface element in the Accessibility Tree.

It is necessary because HTML provides numerous overlapping mechanisms for naming elements (e.g., inner text, `<label>`, `aria-label`, `aria-labelledby`, `alt`, `title`, `placeholder`). AccName provides a deterministic, standardized precedence order so that all browsers and screen readers compute identical accessible names for any given DOM structure.
</details>

### Q2: What is the exact precedence order of the AccName 1.2 resolution algorithm?
<details>
<summary>View Answer</summary>

The resolution order is:
1. `aria-labelledby` (Iterates through IDs, concatenating computed text).
2. `aria-label` (Direct string override).
3. Native HTML label mechanisms (`<label for>`, `<caption>`, `<legend>`, `alt`, `<summary>`).
4. Subtree text content (Recursive concatenation of child text nodes and pseudo-elements).
5. Fallback attributes (`title`, or `placeholder` on input controls).
</details>

### Q3: Explain WCAG 2.2 Success Criterion 2.5.3 (Label in Name). Why is it critical for voice-control users?
<details>
<summary>View Answer</summary>

WCAG 2.5.3 mandates that for user interface components with labels that include text or images of text, the Accessible Name must contain the text that is presented visually.

This is critical for users of speech-recognition software (e.g., Dragon NaturallySpeaking, Apple Voice Control). When a voice user sees a button labeled "Submit", they speak the command *"Click Submit"*. If the developer wrote `<button aria-label="Send Application">Submit</button>`, the browser's accessible name is "Send Application". The speech engine cannot find an element named "Submit", and the voice command fails.
</details>

### Q4: Can `aria-labelledby` reference an element that is visually hidden with `display: none`?
<details>
<summary>View Answer</summary>

**Yes**. The AccName specification explicitly carves out an exception for `aria-labelledby`: when a node is referenced as a label source via `aria-labelledby`, the browser engine will parse and extract its text content even if that element has `display: none` or `visibility: hidden`.
</details>

### Q5: How do browser engines prevent infinite recursive loops in `aria-labelledby`?
<details>
<summary>View Answer</summary>

The AccName algorithm maintains a traversal history stack during calculation. If node A references node B via `aria-labelledby`, and node B references node A, or if node A references itself (`<div id="x" aria-labelledby="x">`), the algorithm detects the circular reference, halts recursive attribute evaluation, and falls back to computing the node's inner text or next available candidate in the cascade.
</details>

### Q6: What is the difference between `aria-labelledby` and `aria-describedby`?
<details>
<summary>View Answer</summary>

- `aria-labelledby`: Computes the **Accessible Name** (the concise identity of the element, spoken first alongside its role).
- `aria-describedby`: Computes the **Accessible Description** (supplementary instructional text or error messages, spoken after the name and role following a brief pause).
</details>

### Q7: Why is relying on the `placeholder` attribute as an accessible name considered an anti-pattern?
<details>
<summary>View Answer</summary>

1. Once the user types any text into the input, the placeholder disappears visually, leaving users with no visual reference for what the field represents.
2. Most browsers render placeholders in low-contrast light grey text that fails WCAG 1.4.3 (4.5:1 contrast ratio).
3. Some assistive technologies do not treat placeholders as valid accessible names unless all other naming candidates are absent, leading to inconsistent announcements.
</details>

### Q8: How can a developer inspect the live computed Accessible Name and Accessibility Tree in modern browsers?
<details>
<summary>View Answer</summary>

1. In **Chrome / Edge**: Open DevTools, select the element in the DOM tree, open the **Accessibility** pane in the right-hand panel, and view the **Computed Properties** section showing "Name" and its "Source". You can also click the accessibility tree icon to inspect the full-page AOM.
2. In **Firefox**: Open DevTools, switch to the dedicated **Accessibility** tab, inspect the full tree hierarchy, and view the properties panel showing Name, Role, and Relations.
3. In **Safari**: Open Web Inspector, select the Elements tab, and view the **Node** panel under the Accessibility section.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Providing an Accessible Name to an Icon Button
**Problem**: An icon button displays a magnifying glass SVG. Write the semantic HTML providing an accessible name of "Search documents".

<details>
<summary>View Solution</summary>

```html
<button type="button" aria-label="Search documents">
  <svg aria-hidden="true" width="20" height="20" viewBox="0 0 24 24">
    <circle cx="11" cy="11" r="8" stroke="currentColor" fill="none"/>
    <line x1="21" y1="21" x2="16.65" y2="16.65" stroke="currentColor"/>
  </svg>
</button>
```
</details>

### Exercise 2: Easy — Tracing the AccName Winner
**Problem**: For the following markup, what will be the computed Accessible Name?
`<button type="button" id="b1" aria-label="Upload Files" title="Choose Files">Submit</button>`

<details>
<summary>View Solution</summary>

The computed Accessible Name is **"Upload Files"**.
**Reason**: According to the AccName 1.2 resolution cascade, `aria-label` (Step 2) takes absolute precedence over inner subtree text ("Submit", Step 4) and the `title` attribute ("Choose Files", Step 5).
</details>

### Exercise 3: Medium — Fixing a WCAG 2.5.3 (Label in Name) Violation
**Problem**: The following button violates WCAG 2.5.3. Correct it so voice users can activate it by speaking "Add".
`<button type="button" aria-label="Create New Project Record">Add</button>`

<details>
<summary>View Solution</summary>

```html
<!-- The Accessible Name MUST contain the visual label ("Add") verbatim, ideally at the start -->
<button type="button" aria-label="Add new project record">
  Add
</button>
```
</details>

### Exercise 4: Medium — Concatenating Multi-Part Labels via `aria-labelledby`
**Problem**: Write a table row where an "Edit" button derives its accessible name by combining the button's own text with the customer's name from another cell.

<details>
<summary>View Solution</summary>

```html
<table>
  <tr>
    <td id="cust-42">Acme Industrial Logistics</td>
    <td>
      <button 
        type="button" 
        id="btn-edit-42"
        aria-labelledby="btn-edit-42 cust-42"
      >
        Edit
      </button>
    </td>
  </tr>
</table>
<!-- Computed Accessible Name: "Edit Acme Industrial Logistics" -->
```
</details>

### Exercise 5: Hard — Dialog Naming and Description Architecture
**Problem**: Write the complete HTML for an accessible confirmation modal dialog using `<dialog>`, properly wired with `aria-labelledby` and `aria-describedby`.

<details>
<summary>View Solution</summary>

```html
<dialog id="deactivate-dialog" aria-labelledby="dialog-heading" aria-describedby="dialog-info">
  <div class="dialog-content">
    <h2 id="dialog-heading">Deactivate Enterprise Account?</h2>
    
    <p id="dialog-info">
      Deactivating your account will instantly suspend API access for all 
      associated developer keys and revoke organization-wide SSO credentials.
    </p>

    <div class="dialog-footer">
      <button type="button" onclick="closeDialog()">Cancel</button>
      <button type="button" class="btn-danger" onclick="confirmDeactivation()">Deactivate</button>
    </div>
  </div>
</dialog>
```
</details>

### Exercise 6: Hard — Tracing AccName with Hidden Fallback Spans
**Problem**: Given the following markup, what is the computed Accessible Name of the button, and why?

```html
<div style="display: none;">
  <span id="label-prefix">Download</span>
  <span id="label-entity">Annual Tax Audit</span>
</div>
<button type="button" aria-labelledby="label-prefix label-entity">
  <span aria-hidden="true">PDF</span>
</button>
```

<details>
<summary>View Solution</summary>

The computed Accessible Name is **"Download Annual Tax Audit"**.
**Explanation**:
1. `aria-labelledby` is evaluated first in the AccName cascade.
2. Even though the parent `<div>` has `display: none`, the AccName spec mandates that nodes referenced by `aria-labelledby` are traversed and their text content is extracted.
3. The browser extracts `"Download"` from `#label-prefix` and `"Annual Tax Audit"` from `#label-entity`, concatenating them into `"Download Annual Tax Audit"`.
4. The inner `<span aria-hidden="true">PDF</span>` is ignored because `aria-labelledby` took precedence.
</details>

---

## 12. Summary / Cheat Sheet

- **Accessible Name**: The primary identity string spoken by screen readers for any element.
- **AccName Precedence Order**:
  1. `aria-labelledby`
  2. `aria-label`
  3. Native HTML labels (`<label>`, `<caption>`, `<legend>`, `alt`, `<summary>`)
  4. Subtree text (`innerText`)
  5. Fallback attributes (`title`, `placeholder`)
- **WCAG 2.5.3 (Label in Name)**: If an element has visible text, that exact text must be included in the accessible name for speech-to-text voice control compatibility.
- **`aria-labelledby`**: Can reference multiple space-separated IDs and can read text from `display: none` elements.
- **`aria-describedby`**: Provides supplementary hints or error messages spoken after the accessible name.
- **DevTools Inspection**: Use the Accessibility tab in Chrome/Firefox/Safari to inspect computed names and sources.

---

## 13. Further Reading

- [W3C Accessible Name and Description Computation (AccName) 1.2](https://www.w3.org/TR/accname-1.2/)
- [W3C WAI: Understanding WCAG 2.5.3 Label in Name](https://www.w3.org/WAI/WCAG22/Understanding/label-in-name.html)
- [Chrome DevTools: Inspect the Accessibility Tree](https://developer.chrome.com/docs/devtools/accessibility/reference)
- [TPGi: What is an Accessible Name?](https://www.tpgi.com/what-is-an-accessible-name/)

---

[Previous: 03 - ARIA Roles, States & Properties (03-aria-roles-states-and-properties.md)](03-aria-roles-states-and-properties.md) | [Section Index](README.md) | [Next: 05 - Keyboard Navigation & Focus ->](05-keyboard-navigation-and-focus.md)
