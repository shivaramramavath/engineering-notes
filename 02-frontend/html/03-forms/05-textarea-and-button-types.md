# Multi-Line Textarea, Button Modalities, and Form Actions

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Fundamental |
| **Prerequisites** | `01-form-basics-and-methods.md`, `03-labels-fieldsets-and-legends.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Baseline Status** | Widely Available (`field-sizing` Newly Available 2024) |
| **Browsers Tested** | Chrome 120+, Firefox 120+, Safari 17+, Edge 120+ |
| **Tested With** | W3C Nu HTML Checker, Axe-core v4.8, NVDA 2023.3, VoiceOver macOS Sonoma |

---

## Table of Contents
- [1 What & Why](#1-what--why)
- [2 Core Concepts](#2-core-concepts)
- [3 How It Works Internally](#3-how-it-works-internally)
- [4 Syntax & Usage](#4-syntax--usage)
- [5 Practical Examples](#5-practical-examples)
- [6 Best Practices](#6-best-practices)
- [7 Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8 Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
- [9 Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10 Interview Questions](#10-interview-questions)
- [11 Practice Exercises](#11-practice-exercises)
- [12 Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13 Further Reading](#13-further-reading)

---

## 1 What & Why

Single-line text inputs (`<input type="text">`) are optimized for concise, discrete tokens: usernames, phone numbers, postal codes, and email addresses. However, modern hypermedia applications constantly collect expansive, unstructured text: issue descriptions, code snippets, support tickets, and Markdown documents.

For multi-line text input, HTML provides the **`<textarea>`** element. Unlike `<input>`, which relies on the `value` attribute, `<textarea>` is a normal non-void element whose default text is enclosed directly between its tags.

Simultaneously, the mechanisms that trigger form submissions, modal dismissals, or client-side UI mutations depend on the **`<button>`** element. Misunderstanding button types is the single most common cause of "phantom form submissions" in frontend engineering: because `<button>` defaults to `type="submit"`, omitting an explicit `type="button"` causes innocent modal triggers to inadvertently submit forms and reload pages!

This guide covers production patterns for `<textarea>` configuration, line wrapping algorithms, button types, button-level attribute overrides, and modern auto-sizing capabilities (`field-sizing`).

---

## 2 Core Concepts

### The `<textarea>` Element
The `<textarea>` element represents a multi-line plain-text editing control.
Key attributes:
- `rows`: The number of visible text lines (User-Agent default is typically 2).
- `cols`: The visible width of the text control in average character widths (default is typically 20).
- `wrap`: Governs how the browser handles line breaks during submission:
  - `soft` (Default): The browser displays wrapped text visually on screen, but transmits text **without line breaks** (only explicit user `Enter` keystrokes generate `\r\n`).
  - `hard`: The browser displays wrapped text visually **and automatically injects carriage return / line feeds (`\r\n`)** at each wrap boundary in the submitted payload. *(Note: Mandates the `cols` attribute be specified!)*.
- `maxlength` and `minlength`: Enforce character count constraints.
- `placeholder`: Temporary hint text visible when empty.

### The `<textarea>` Initial Value Quirk
Unlike `<input type="text" value="Default">`, **`<textarea>` does NOT accept a `value` attribute in HTML**. 
Its initial value must be placed between the opening and closing tags:
```html
<textarea name="notes">Initial default prose goes here.</textarea>
```
*Parser Rule*: If an immediate newline character follows the opening `<textarea>` tag, the parser automatically discards that first newline, preventing unintended blank lines at the top of the editor.

### Button Types and Modalities
The `<button>` element represents an interactive button control. It accepts the **`type` attribute**, which completely governs its operational behavior:

1. **`type="submit"` (DEFAULT)**:
   If the `type` attribute is omitted, the browser defaults to `type="submit"`. Activating this button validates the enclosing form and initiates form submission.
2. **`type="button"`**:
   The button has **no default behavior**. It does not submit the form, does not reset inputs, and does not navigate. It serves exclusively as a hook for JavaScript click listeners, modal toggles, or popover triggers.
3. **`type="reset"`**:
   Resets all form controls within the enclosing `<form>` back to their original initial values. *(Generally considered a UX anti-pattern in modern software because users frequently click it accidentally, destroying their hard work).*

---

### Button vs. `<input type="submit">`
Historically in HTML 4, form buttons were authored using `<input type="submit" value="Save">`.
Modern engineering universally favors `<button type="submit">`:
- `<input type="submit">` is a void element; its visual label is tied directly to its plain-text `value` attribute. It cannot contain child HTML elements.
- `<button>` is a container element that can hold rich HTML phrasing: SVG icons, loading spinners, flexbox layouts, badges, and hidden screen reader spans.

---

## 3 How It Works Internally

### The Parser Whitespace Rule for `<textarea>`
Because `<textarea>` contains raw text, the HTML tokenizer treats its contents in the **RCDATA state** (entities like `&amp;` and `&lt;` are decoded, but tags like `<div>` are parsed as literal text).

```
Author Markup:
<textarea name="desc">
Line 1
Line 2</textarea>

Parser Tokenizer Action:
1. Encounter <textarea>. Enter RCDATA state.
2. First character is \n (Line Feed).
3. PARSER RULE: If first character in <textarea> is \n, IGNORE IT.
4. Next characters: "Line 1\nLine 2".
5. Encounter </textarea>. Exit RCDATA state.
6. Resulting DOM value: "Line 1\nLine 2" (Zero leading blank line!)
```

### The `wrap="hard"` vs `wrap="soft"` Transmission Matrix

```
User types in a 20-column textarea:
"The quick brown fox jumps over the lazy dog"

Under wrap="soft" (Default):
HTTP Payload Body: "The quick brown fox jumps over the lazy dog" (Single uninterrupted string)

Under wrap="hard" (with cols="20"):
HTTP Payload Body: "The quick brown fox\r\njumps over the\r\nlazy dog" (Hard breaks inserted!)
```

---

## 4 Syntax & Usage

```html
<!-- 1. Semantic Textarea with Character Limits and Resizing -->
<div class="field-group">
  <label for="incident-description">Incident Impact Description (Max 1000 characters):</label>
  <textarea id="incident-description" 
            name="impact_description" 
            rows="6" 
            cols="60" 
            maxlength="1000" 
            required 
            placeholder="Describe cluster symptoms, affected microservices, and error rates..."></textarea>
</div>

<!-- 2. Rich Submit Button with SVG Icon and Loading State -->
<button type="submit" class="btn-primary">
  <svg width="18" height="18" viewBox="0 0 24 24" aria-hidden="true">
    <path d="M2.01 21L23 12 2.01 3 2 10l15 2-15 2z"/>
  </svg>
  <span>Deploy Service Mesh</span>
</button>

<!-- 3. Safe Action Button (Will NOT submit the form!) -->
<button type="button" class="btn-secondary" onclick="openDiagnosticDrawer()">
  Inspect Live Socket Metrics
</button>

<!-- 4. Submit Button with Action and Validation Overrides -->
<button type="submit" 
        formaction="/drafts/save" 
        formnovalidate 
        class="btn-draft">
  Save Incomplete Draft
</button>
```

> [!NOTE]
> **Baseline Status: Widely Available**
> The `<textarea>` element, `rows`, `cols`, `wrap`, and all `<button>` types (`submit`, `button`, `reset`) are foundational baseline standards supported across every web browser engine.

---

## 5 Practical Examples

### Example 1: Accessible Code Submission Textarea with Monospace Formatting
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Terraform HCL Validator</title>
  <style>
    .editor-wrapper { max-width: 700px; display: flex; flex-direction: column; gap: 0.5rem; }
    label { font-weight: 600; font-family: sans-serif; }
    textarea.code-editor {
      font-family: ui-monospace, Menlo, Monaco, Consolas, monospace;
      font-size: 0.875rem;
      line-height: 1.5;
      padding: 0.75rem;
      background: #0f172a;
      color: #38bdf8;
      border: 1px solid #334155;
      border-radius: 6px;
      resize: vertical; /* Prevents horizontal layout blowout */
      tab-size: 2;
    }
  </style>
</head>
<body>
  <main>
    <h1>Infrastructure as Code Deployment</h1>

    <form action="/infrastructure/plan" method="post">
      <div class="editor-wrapper">
        <label for="hcl-input">Terraform Configuration Manifest (HCL):</label>
        <textarea id="hcl-input" 
                  name="hcl_manifest" 
                  class="code-editor" 
                  rows="8" 
                  spellcheck="false" 
                  required>resource "aws_vpc" "production" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
}</textarea>
      </div>

      <div class="button-group" style="margin-top: 1rem;">
        <button type="submit" class="btn-primary">Generate Execution Plan</button>
        <button type="button" onclick="formatHclCode()">Format Syntax</button>
      </div>
    </form>
  </main>
</body>
</html>
```

### Example 2: Modern Pure-CSS Auto-Sizing Textarea (`field-sizing`)
```html
<!-- Modern 2024 Baseline Feature: field-sizing: content -->
<!-- Replaces 30 lines of JavaScript that calculated scrollHeight on input! -->
<!-- As the user types multiple lines, the textarea smoothly expands automatically -->

<style>
  .auto-expanding-input {
    width: 100%;
    min-height: 2.5rem;
    padding: 0.5rem;
    font-size: 1rem;
    border: 1px solid #d1d5db;
    border-radius: 6px;
    
    /* Native auto-grow property */
    field-sizing: content;
    resize: none; /* Disable manual drag handle */
  }
</style>

<label for="chat-msg">Post System Status Update:</label>
<textarea id="chat-msg" 
          name="message" 
          class="auto-expanding-input" 
          rows="1" 
          placeholder="Type update (auto-expands as you type)..."></textarea>
```

### Example 3: Edge Case — Button-Level Form Overrides in Complex Workflow
```html
<form action="/workflow/submit" method="post" id="approval-form">
  <label for="comments">Reviewer Audit Comments:</label>
  <textarea id="comments" name="audit_notes" rows="3" required></textarea>

  <div class="action-bar">
    <!-- Primary Action: Submits to standard action (/workflow/submit) -->
    <button type="submit" name="action_type" value="approve" class="btn-success">
      Approve &amp; Deploy
    </button>

    <!-- Reject Action: Overrides formaction to hit rejection endpoint -->
    <button type="submit" 
            formaction="/workflow/reject" 
            name="action_type" 
            value="reject" 
            class="btn-danger">
      Reject Submission
    </button>

    <!-- Save Draft Action: Overrides action AND disables validation requirements -->
    <button type="submit" 
            formaction="/workflow/draft" 
            formnovalidate 
            class="btn-neutral">
      Save Draft (Skip Validation)
    </button>
  </div>
</form>
```

### Example 4: Failure Case — Phantom Submissions and Textarea Value Bugs
```html
<!-- ANTI-PATTERN: DO NOT COPY -->

<!-- Error 1: Missing type="button" on modal trigger inside a form -->
<!-- Clicking "Open Help" ACCIDENTALLY SUBMITS THE FORM AND RELOADS THE PAGE! -->
<form action="/save" method="post">
  <input type="text" name="data">
  <button onclick="openHelpModal()">Open Help</button> <!-- DEFAULT TYPE IS SUBMIT! -->
</form>

<!-- Error 2: Using the value attribute on <textarea> -->
<!-- The value attribute is COMPLETELY IGNORED by the HTML parser! The box remains empty! -->
<textarea name="bio" value="This will never render!"></textarea>

<!-- Error 3: Accidental leading whitespace in textarea -->
<textarea name="notes">
      Indented text creates 6 unwanted spaces in the user's document!
</textarea>

<!-- Error 4: Unconstrained horizontal resize blowing out page layout -->
<textarea style="resize: horizontal;"></textarea> <!-- Drags box offscreen! -->
```

### Example 5: Production Incident Ticketing Form with Character Counter
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Production Issue Escalation</title>
  <style>
    .counter-text { font-size: 0.85rem; color: #64748b; text-align: right; margin-top: 0.25rem; }
    textarea { width: 100%; box-sizing: border-box; padding: 0.75rem; border: 1px solid #cbd5e1; border-radius: 6px; }
  </style>
</head>
<body>
  <main>
    <h1>Escalate Production Incident</h1>

    <form action="/incidents/escalate" method="post">
      <div>
        <label for="rca-summary">Root Cause Analysis (RCA) Summary:</label>
        <textarea id="rca-summary" 
                  name="rca" 
                  rows="5" 
                  maxlength="500" 
                  aria-describedby="char-counter" 
                  oninput="updateCount(this)" 
                  required></textarea>
        <div id="char-counter" class="counter-text" aria-live="polite">
          <span id="chars-used">0</span> / 500 characters
        </div>
      </div>

      <div style="margin-top: 1.5rem; display: flex; gap: 1rem;">
        <button type="submit" class="btn-primary">File Emergency Ticket</button>
        <button type="button" onclick="history.back()">Discard Changes</button>
      </div>
    </form>
  </main>

  <script>
    function updateCount(el) {
      document.getElementById("chars-used").textContent = el.value.length;
    }
  </script>
</body>
</html>
```

---

## 6 Best Practices

1. **Always Explicitly Declare `type="button"` on Non-Submitting Buttons**: Never write `<button>` without a `type` attribute unless you intentionally desire form submission. Always declare `type="button"` for modal triggers, toolbars, and custom controls.
2. **Never Use the `value` Attribute on `<textarea>`**: Place default text directly between `<textarea>` and `</textarea>`.
3. **Restrict CSS Resizing to Vertical**: Apply `textarea { resize: vertical; }` in your global CSS to allow users to expand the editor downward without dragging it horizontally and destroying responsive page boundaries.
4. **Link Character Counters via `aria-describedby`**: When implementing character counters, associate the counter element with the textarea using `aria-describedby` and add `aria-live="polite"` so screen readers speak count updates.
5. **Avoid `<button type="reset">`**: Reset buttons frequently cause accidental data loss when users mistakenly click them instead of "Submit". If a reset option is genuinely required, hide it behind an explicit confirmation step.
6. **Prefer `<button>` Over `<input type="submit">`**: Always use `<button type="submit">` to maintain semantic flexibility, allowing you to embed SVG icons and accessible text spans within the button trigger.

---

## 7 Common Mistakes & Pitfalls

| Mistake | Bad Code | Good Code | Technical Explanation |
| :--- | :--- | :--- | :--- |
| **Omitting Button Type** | `<button onclick="close()">X</button>` | `<button type="button" onclick="close()">X</button>` | Default type is `submit`. Clicking it inside a form unintentionally submits the form and reloads the page. |
| **`value` Attribute on Textarea** | `<textarea value="Default text"></textarea>` | `<textarea>Default text</textarea>` | `<textarea>` ignores the `value` HTML attribute; text must be placed between the tags. |
| **Accidental Indentation Whitespace** | `<textarea>\n    Text\n</textarea>` | `<textarea>Text</textarea>` | Textarea preserves all internal indentation spaces verbatim, corrupting user data payloads. |
| **Disabling Textarea Resizing Entirely** | `textarea { resize: none; }` | `textarea { resize: vertical; }` | Completely disabling resize frustrates users on desktop displays who need to inspect large blocks of text. |
| **Using `<div>` Instead of `<button>`** | `<div class="btn" onclick="save()">Save</div>` | `<button type="button" class="btn" onclick="save()">Save</button>` | Custom `div` buttons cannot be focused via keyboard `Tab` and cannot be activated via `Enter` or `Spacebar`. |
| **Missing `cols` with `wrap="hard"`** | `<textarea wrap="hard"></textarea>` | `<textarea wrap="hard" cols="60"></textarea>` | The WHATWG specification dictates that `wrap="hard"` requires an explicit `cols` attribute to calculate wrap boundaries. |

---

## 8 Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Criterion 2.1.1 (Keyboard - Level A)**: Native `<button>` elements receive keyboard focus automatically and activate via both `Enter` and `Spacebar`.
- **Criterion 1.3.1 (Info and Relationships)**: Sighted users can visually discern that `<textarea>` is a multi-line input; screen readers announce: *"Edit text, multi-line"*, informing blind users that pressing `Enter` will insert a line break rather than submit the form.

### Search Engine Optimization (SEO)
- Search engines parse initial content pre-rendered inside `<textarea>` tags for contextual relevance, but give it low weighting since textareas represent interactive input rather than static editorial prose.

### Performance & Memory
- Utilizing modern CSS `field-sizing: content` completely eliminates expensive JavaScript `input` event listeners and `element.scrollHeight` layout thrashing passes, executing height calculations directly on the GPU/layout thread.

### Security Notes
- **XSS Sanitization**: Because `<textarea>` parses in the RCDATA state, writing `<textarea><script>alert(1)</script></textarea>` does **not** execute JavaScript; the script tag is parsed as inert text. However, when server-side templates render user input into a textarea, characters like `</textarea>` must be entity-escaped (`&lt;/textarea&gt;`) to prevent **Tag Injection / Attribute Breakout XSS**.

---

## 9 Comparison / When to Use What

| Element / Type | Multi-line? | Default Form Behavior | Permitted Children | Ideal Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `<textarea>` | **Yes** | Input field (submits value) | Plain text (RCDATA) | Long prose, code manifests, ticket descriptions. |
| `<input type="text">` | No (Single line)| Input field (submits value) | Void (No children) | Single tokens: usernames, phone numbers, codes. |
| `<button type="submit">`| N/A | Submits enclosing form | Phrasing content (SVG, text) | Primary form submission, buy now, sign in. |
| `<button type="button">`| N/A | **No default behavior** | Phrasing content (SVG, text) | Modal triggers, client-side UI toggles, tabs. |
| `<button type="reset">` | N/A | Resets form to initial state | Phrasing content (SVG, text) | Form clearing (use with extreme caution). |
| `<input type="submit">` | N/A | Submits enclosing form | Void (Plain text value only) | Legacy legacy forms (deprecated pattern). |

---

## 10 Interview Questions

<details>
<summary><strong>1. What happens if a developer omits the `type` attribute on a `<button>` element inside a `<form>`? Why does this cause subtle bugs?</strong></summary>

**Answer:**
According to the WHATWG HTML Living Standard, if the `type` attribute is omitted from a `<button>` element, the browser engine **defaults to `type="submit"`**.

**Why This Causes Subtle Bugs**:
If a developer builds a form containing auxiliary action buttons (such as an "Add Another Row" button, a "Toggle Password Visibility" button, or an "Open Help Modal" button) and writes:
```html
<button onclick="toggleModal()">Help</button>
```
When a user clicks that button, the browser triggers `toggleModal()`, but immediately following the click handler, the browser treats the button as a form submission trigger:
1. It runs native constraint validation on all inputs.
2. If valid, it dispatches an HTTP request, reloading the entire page!
The developer is left wondering why their modal appears for a split-second before the page mysteriously refreshes. 

To prevent this, all non-submitting buttons must explicitly declare:
```html
<button type="button" onclick="toggleModal()">Help</button>
```
</details>

<details>
<summary><strong>2. Why can you not set the initial value of a `<textarea>` using the `value` attribute in HTML?</strong></summary>

**Answer:**
Unlike `<input>`, which is an empty (void) element that uses attributes for its payload, `<textarea>` was designed in early HTML as a **container element** capable of holding vast multi-line blocks of text without escaping quotes.

In the HTML parser state machine, the `<textarea>` element parses in the **RCDATA state**. Its initial value is derived strictly from its **child text nodes**:
```html
<textarea name="desc">Initial content goes here</textarea>
```
The HTML parser completely ignores any `value="..."` attribute declared on `<textarea>`. 

*(Note: In JavaScript DOM manipulation, the `HTMLTextAreaElement` interface DOES provide a `.value` property for reading and writing text dynamically: `textareaEl.value = "New text"`).*
</details>

<details>
<summary><strong>3. What is the difference between `wrap="soft"` and `wrap="hard"` on a `<textarea>`?</strong></summary>

**Answer:**
Both wrap text visually within the browser viewport so lines do not overflow horizontally. The difference occurs during **form submission**:
- **`wrap="soft"` (Default)**: Visual wrapping occurs purely for the user's viewing convenience. The submitted HTTP payload contains **no automatic line breaks**; lines only break where the user explicitly pressed the `Enter` key.
- **`wrap="hard"`**: The browser layout engine calculates the exact column width specified by the mandatory `cols` attribute. Upon form submission, the browser **automatically inserts CRLF (`\r\n`) line break characters** into the payload at every point where the text visually wrapped!
`wrap="hard"` is used when submitting text to legacy mainframes or automated email formatters that require strict 72-character line limits.
</details>

<details>
<summary><strong>4. How does the modern CSS `field-sizing: content` property revolutionize `<textarea>` engineering?</strong></summary>

**Answer:**
Historically, making a `<textarea>` automatically grow and shrink in height as the user typed required complex JavaScript:
1. Listening to the `input` event on every keystroke.
2. Setting `textarea.style.height = "auto"`.
3. Reading `textarea.scrollHeight` (which forced an expensive synchronous layout reflow).
4. Assigning `textarea.style.height = textarea.scrollHeight + "px"`.

The modern CSS property **`field-sizing: content`** (Newly Available 2024 across Chrome 123+) allows form controls to calculate their intrinsic dimensional box based directly on their textual content, exactly like a standard `<div>`. The `<textarea>` grows vertically as new lines are added and shrinks when lines are deleted, running entirely on the layout thread with zero JavaScript!
</details>

<details>
<summary><strong>5. What is the parser's "initial newline stripping rule" for `<textarea>`?</strong></summary>

**Answer:**
When authors write HTML templates, they frequently place the opening `<textarea>` tag on its own line and indent the content on the next line:
```html
<textarea name="notes">
Content starts here
</textarea>
```
If the parser treated all whitespace literally, the initial value would begin with an invisible `\n` carriage return, causing the cursor to start on line 2.

To avoid this, the WHATWG specification mandates:
*If the very first character token inside a `<textarea>` element is a newline (`U+000A` or `U+000D`), the parser must automatically discard that single character*. All subsequent newlines and spaces are preserved verbatim.
</details>

<details>
<summary><strong>6. Why should developers apply `textarea { resize: vertical; }` instead of leaving `resize` at its default value?</strong></summary>

**Answer:**
By default in desktop browsers, `<textarea>` elements have CSS `resize: both`. This allows users to click and drag the bottom-right corner to expand the editor both vertically and horizontally.

If a user drags the handle horizontally, the textarea expands across its container boundaries, blowing out CSS Flexbox, Grid, and mobile viewport widths, causing horizontal scrollbars and breaking the site's layout. Applying `resize: vertical;` allows users to expand the height as much as they need to read long prose while locking horizontal width strictly to the design grid.
</details>

<details>
<summary><strong>7. How do button-level form attributes (`formaction`, `formnovalidate`) provide progressive enhancement?</strong></summary>

**Answer:**
Button-level attributes allow a single HTML `<form>` to support multiple diverse workflows without writing client-side JavaScript routing:
```html
<form action="/publish" method="post">
  <textarea name="content" required></textarea>
  <button type="submit">Publish</button>
  <button type="submit" formaction="/save-draft" formnovalidate>Save Draft</button>
</form>
```
If the user clicks "Save Draft":
1. The browser overrides the form's `action` to submit directly to `/save-draft`.
2. The `formnovalidate` attribute bypasses the `required` constraint on `<textarea>`, allowing users to save incomplete drafts.
This works natively over pure HTTP even if the client's network connection drops JavaScript asset loading.
</details>

<details>
<summary><strong>8. Why is `<button>` preferred over `<input type="button">` or `<input type="submit">`?</strong></summary>

**Answer:**
- `<input type="submit">` is a void element. It cannot contain child elements. Its visible text is governed solely by its `value` attribute, preventing developers from including SVG icons, badges, loading spinners, or styling spans.
- `<button>` is a full container element. It can encompass any valid phrasing content (SVGs, `<span>`, `<strong>`, `<img>`). Furthermore, `<button>` cleanly separates the **submitted form payload value** (declared in `value="..."`) from its **visible visual label** (declared in its inner HTML).
</details>

---

## 11 Practice Exercises

### Exercise 1 (Easy): Prevent Phantom Form Submission
**Task**: In the following form, fix the bug that causes clicking the "Toggle Preview" button to prematurely submit the form.

```html
<!-- Broken Source -->
<form action="/posts/create" method="post">
  <textarea name="body"></textarea>
  <button onclick="toggleMarkdownPreview()">Toggle Preview</button>
  <button type="submit">Publish Post</button>
</form>
```

<details>
<summary><strong>Solution</strong></summary>

```html
<form action="/posts/create" method="post">
  <textarea name="body"></textarea>
  <!-- Fix: Explicitly declare type="button" -->
  <button type="button" onclick="toggleMarkdownPreview()">Toggle Preview</button>
  <button type="submit">Publish Post</button>
</form>
```
*Explanation*: Without `type="button"`, the browser defaults to `type="submit"`, executing form submission on click.
</details>

### Exercise 2 (Easy): Set Default Textarea Content Correctly
**Task**: Correct this invalid textarea to ensure default text "SELECT * FROM telemetry;" appears on screen when rendered.

```html
<!-- Broken Source -->
<textarea name="query" value="SELECT * FROM telemetry;"></textarea>
```

<details>
<summary><strong>Solution</strong></summary>

```html
<textarea name="query">SELECT * FROM telemetry;</textarea>
```
*Explanation*: Textarea content must be placed between the opening and closing tags; the `value` attribute in HTML is ignored.
</details>

### Exercise 3 (Medium): Dual-Submission Workflow via Formaction
**Task**: Author a form for creating a database backup. Provide a "Standard Backup" button that submits via POST to `/backup/standard` and an "Urgent Immediate Backup" button that overrides the action to submit to `/backup/urgent` with high priority.

<details>
<summary><strong>Solution</strong></summary>

```html
<form action="/backup/standard" method="post">
  <label for="db-name">Database Target:</label>
  <input type="text" id="db-name" name="database" required>

  <div class="actions" style="margin-top: 1rem;">
    <button type="submit">Standard Backup</button>
    <button type="submit" formaction="/backup/urgent" class="btn-danger">
      Urgent Immediate Backup
    </button>
  </div>
</form>
```
</details>

### Exercise 4 (Medium): Accessible Character-Counted Textarea
**Task**: Create an accessible textarea for filing an error report with a maximum of 250 characters. Connect an accessible helper message and character counter using `aria-describedby`.

<details>
<summary><strong>Solution</strong></summary>

```html
<div class="form-field">
  <label for="error-report">Bug Reproduction Steps (Max 250 characters):</label>
  <textarea id="error-report" 
            name="reproduction_steps" 
            rows="4" 
            maxlength="250" 
            aria-describedby="counter-info" 
            required></textarea>
  <div id="counter-info" class="hint-text">
    Limit 250 characters. Markdown code blocks supported.
  </div>
</div>
```
*Explanation*: `aria-describedby="counter-info"` ensures screen readers announce the character limit instruction upon focusing the textarea.
</details>

### Exercise 5 (Hard): Identify and Fix 5 Textarea & Button Bugs
**Task**: Remediate all 5 bugs in this cloud configuration editor:

```html
<!-- Flawed Source -->
<form action="/config" method="post">
  <label>Environment Variables</label>
  <textarea name="env" value="PORT=8080" style="resize: horizontal;">
      DEBUG=true
  </textarea>
  <button class="btn">Save</button>
  <button onclick="revert()">Reset</button>
</form>
```

<details>
<summary><strong>Solution</strong></summary>

```html
<!-- Fully Remediated Source -->
<form action="/config" method="post">
  <div>
    <!-- 1. Explicit label connection -->
    <label for="env-editor">Environment Variables</label>
    <!-- 2. Removed useless value attribute -->
    <!-- 3. Fixed resize to vertical to prevent layout blowout -->
    <!-- 4. Removed unintended indentation whitespace -->
    <textarea id="env-editor" 
              name="env" 
              rows="4" 
              style="resize: vertical;">PORT=8080
DEBUG=true</textarea>
  </div>

  <div style="margin-top: 1rem;">
    <!-- 5. Explicitly declare button types -->
    <button type="submit" class="btn-primary">Save Configuration</button>
    <button type="button" class="btn-secondary" onclick="revert()">Revert Changes</button>
  </div>
</form>
```
</details>

### Exercise 6 (Hard): Build an Accessible Rich Action Button with SVG and State
**Task**: Author a semantic `<button type="submit">` that contains an SVG checkmark icon, visible button text "Deploy Cluster", and an accessible hidden badge indicating "(Production Environment)".

<details>
<summary><strong>Solution</strong></summary>

```html
<button type="submit" class="deploy-button" name="env_target" value="production">
  <!-- Decorative SVG hidden from screen reader -->
  <svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
    <path fill="currentColor" d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/>
  </svg>

  <span class="btn-label">Deploy Cluster</span>
  <span class="sr-only">(Target: Production Environment)</span>
</button>

<style>
  .deploy-button {
    display: inline-flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.75rem 1.5rem;
    background: #16a34a;
    color: #fff;
    border: none;
    border-radius: 6px;
    font-size: 1rem;
    font-weight: 600;
    cursor: pointer;
  }
  .deploy-button:hover { background: #15803d; }
  .deploy-button:focus-visible { outline: 3px solid #facc15; outline-offset: 2px; }
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
</details>

---

## 12 Summary / Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                        TEXTAREA & BUTTON CHEAT SHEET                   │
├───────────────────┬────────────────────────────────────────────────────┤
│ Pattern           │ Syntax / Rule                                      │
├───────────────────┼────────────────────────────────────────────────────┤
│ Textarea Initial  │ <textarea>Initial Text</textarea> (NO value attr!) │
│ Textarea Resize   │ CSS: resize: vertical; (Never allow horizontal)    │
│ Modern Auto-Grow  │ CSS: field-sizing: content;                        │
│ Default Button    │ <button> defaults to type="submit"!                │
│ Safe JS Button    │ ALWAYS write <button type="button">                │
│ Button Override   │ formaction="/url" formnovalidate formmethod="post" │
│ Wrap Modes        │ wrap="soft" (standard) vs wrap="hard" (adds \r\n)  │
│ Rich Content      │ <button> can contain SVGs and spans; <input> cannot│
└───────────────────┴────────────────────────────────────────────────────┘
```

---

## 13 Further Reading

- [WHATWG HTML Living Standard: Section 4.10.11 The &lt;textarea&gt; Element](https://html.spec.whatwg.org/multipage/form-elements.html#the-textarea-element)
- [WHATWG HTML Living Standard: Section 4.10.6 The &lt;button&gt; Element](https://html.spec.whatwg.org/multipage/form-elements.html#the-button-element)
- [MDN Web Docs: The HTML &lt;textarea&gt; Element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/textarea)
- [web.dev: CSS field-sizing is Here](https://web.dev/articles/css-field-sizing)
- [CSS-Tricks: Auto-Growing Inputs and Textareas](https://css-tricks.com/auto-growing-inputs-textareas/)

---

| [Previous: 04 Select, Datalist & Optgroup](04-select-datalist-and-optgroup.md) | [Section Index](README.md) | [Next: 06 Output, Progress & Meter](06-output-progress-and-meter.md) |
