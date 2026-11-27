# Syntax Rules, Void Elements & Parser Error Recovery

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `01-fundamentals/01-document-structure.md` |
| **Time to Complete** | 35 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Universal HTML Parser Standard) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Element Categories: Normal, Void & Raw Text](#element-categories-normal-void--raw-text)
4. [Attribute Syntax & Quoting Rules](#attribute-syntax--quoting-rules)
5. [The WHATWG Parser Error Recovery Mechanics](#the-whatwg-parser-error-recovery-mechanics)
6. [The Adoption Agency Algorithm (AAA)](#the-adoption-agency-algorithm-aaa)
7. [Syntax & Usage](#syntax--usage)
8. [Practical Examples](#practical-examples)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
12. [Comparison Matrix: Element Models in HTML5](#comparison-matrix-element-models-in-html5)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In traditional compiled or interpreted programming languages (such as Java, C, or Python), a missing semicolon, unclosed parenthesis, or mismatched bracket is a **fatal syntax error**: compilation halts, an exception is thrown, and execution aborts immediately.

HTML is radically different: **the HTML parser never crashes**. 

There is no such thing as a fatal syntax error in the HTML Living Standard. If an author writes unclosed tags, misnested blocks, or completely unquoted attributes, the browser engine executes a deterministic, specification-mandated **Error Recovery Algorithm** that reconstructs the broken markup into a valid, navigable Document Object Model (DOM) tree.

Understanding these parser rules is essential: what appears as a harmless typo in your editor can trigger severe DOM re-parenting, duplicate nodes, or layout bugs that completely evade surface visual inspection.

---

## Core Concepts

### 1. Element vs Tag
In professional discourse, engineers distinguish between an **element** and a **tag**:
- A **Tag** is a syntactic token in the markup text delimited by angle brackets: `<p>` (start tag) or `</p>` (end tag).
- An **Element** is the resulting in-memory DOM object that starts with the opening tag, contains attributes and child nodes, and terminates with the closing tag.

### 2. Case Insensitivity
- HTML element names and attribute names are **ASCII case-insensitive**: `<DIV CLASS="CARD">` is syntactically identical to `<div class="CARD">`.
- **Convention**: Production engineering standards strictly require lowercase for all element and attribute names (`<div class="...">`).
- **Caveat**: Attribute *values* (such as IDs, class names, and URLs) are often strictly case-sensitive in CSS selectors and JavaScript (`id="Profile"` is different from `id="profile"`).

---

## Element Categories: Normal, Void & Raw Text

The WHATWG specification categorizes elements into five distinct parsing models:

```
+-----------------------------------------------------------------------------------------+
|                               HTML5 ELEMENT PARSING CATEGORIES                          |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. VOID ELEMENTS (Self-contained; CANNOT have children or closing tags)                |
|     <area>, <base>, <br>, <col>, <embed>, <hr>, <img>, <input>, <link>, <meta>,         |
|     <source>, <track>, <wbr>                                                            |
|                                                                                         |
|  2. RAW TEXT ELEMENTS (Parsed as plain text; HTML tags inside are ignored)               |
|     <script>, <style>                                                                   |
|                                                                                         |
|  3. ESCAPABLE RAW TEXT ELEMENTS (Entities &amp; are resolved, but tags not parsed)      |
|     <textarea>, <title>                                                                 |
|                                                                                         |
|  4. FOREIGN ELEMENTS (Parsed using XML / SVG / MathML namespace rules)                 |
|     <svg>, <math> (Self-closing slash IS significant here!)                             |
|                                                                                         |
|  5. NORMAL ELEMENTS (All other standard elements; start tag + content + end tag)        |
|     <div>, <p>, <a>, <section>, <button>, <table>, etc.                                 |
+-----------------------------------------------------------------------------------------+
```

---

## Attribute Syntax & Quoting Rules

Attributes provide configuration parameters to elements. HTML5 permits four distinct attribute styles:

1. **Empty / Boolean Attributes**: If present, the value is true; if absent, false:
   ```html
   <input type="text" disabled required>
   ```
2. **Unquoted Attribute Values**: Allowed if the value contains no spaces, quotes (`"`, `'`), `=`, `<`, `>`, or backticks:
   ```html
   <div class=container id=main> <!-- Valid, but discouraged in style guides -->
   ```
3. **Single-Quoted Values**: Useful when the attribute value contains double quotes:
   ```html
   <button aria-label='Press "Enter" to submit'>
   ```
4. **Double-Quoted Values (Industry Standard)**:
   ```html
   <button aria-label="Submit quarterly report">
   ```

---

## The WHATWG Parser Error Recovery Mechanics

When ill-formed HTML arrives, the browser applies deterministic recovery rules:

### 1. Auto-Closing of Block Elements
Certain elements have optional closing tags per specification. When the parser encounters an opening `<p>` tag while already inside an open `<p>`, it **automatically closes the first paragraph**:
```html
<p>First paragraph
<p>Second paragraph
```
*Parses into two separate siblings:*
```
p: "First paragraph"
p: "Second paragraph"
```

### 2. Foster Parenting (Misnested Tables)
Content that is not a valid table child is "foster-parented" out of the table:
```html
<table>
  <b>Warning:</b>
  <tr><td>Row 1</td></tr>
</table>
```
*Parses into:*
```
b: "Warning:" (Shifted outside and before the table!)
table
  └── tbody
        └── tr
              └── td: "Row 1"
```

---

## The Adoption Agency Algorithm (AAA)

What happens when an author overlaps formatting tags with structural tags?
```html
<p>Hello <b>bold <i>bold-italic</p> italic-only</b> outside
```

In early browsers, overlapping tags caused memory leaks and crashes. WHATWG standardized the **Adoption Agency Algorithm (AAA)** to resolve overlapping inline formatting tags (`<b>`, `<i>`, `<a>`, `<em>`, `<strong>`):
1. The parser detects that `<p>` closes while `<b>` and `<i>` are still open on the active formatting stack.
2. It closes `<p>`, but preserves the active formatting elements.
3. It **clones** the formatting elements into the subsequent container:

```
DOM Result:
p
  ├── "Hello "
  └── b
        ├── "bold "
        └── i
              └── "bold-italic"
i (Cloned!)
  └── b (Cloned!)
        └── " italic-only"
" outside"
```

The browser seamlessly duplicates the formatting nodes so visual intent is preserved without breaking the DOM hierarchy!

---

## Syntax & Usage

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Syntax & Error Recovery</title>
</head>
<body>
  <!-- Clean, standard-conforming syntax: lowercase, double-quoted, explicit closing -->
  <article class="news-item" id="item-402">
    <h2>Standard Element Syntax</h2>
    <p>All attributes are quoted with standard double quotes.</p>
    <img src="banner.webp" alt="Event banner" width="800" height="400">
  </article>
</body>
</html>
```

---

## Browser Engine Support & Baseline Status

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available (Universal across all browser engines)              |
+-------------------+--------------------------------------------------------------------------------+
| Chromium (Blink)  | Chrome 1+, Edge 1+, Android WebView                                            |
+-------------------+--------------------------------------------------------------------------------+
| Gecko (Firefox)   | Firefox 1+, Firefox Mobile                                                     |
+-------------------+--------------------------------------------------------------------------------+
| WebKit (Safari)   | Safari 1+, iOS Safari 1+                                                       |
+-------------------+--------------------------------------------------------------------------------+
| Standards Chapter | WHATWG HTML Section 13: "The HTML syntax" & "Parsing HTML documents"           |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Standard Element vs Void Element Syntax
- **Goal**: Contrast normal closing elements with void elements.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Void vs Normal</title>
</head>
<body>
  <!-- Normal element: must close explicitly -->
  <p>Text inside a normal element.</p>

  <!-- Void element: no children, no end tag permitted! -->
  <hr>
  <input type="text" name="query" value="search">
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
- **Explanation**: Writing `</hr>` or `</input>` would fail W3C validation because void elements cannot have end tags.

### Example 2: Boolean Attributes in Form Controls
- **Goal**: Demonstrate how boolean attributes are evaluated by the engine.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Boolean Attributes</title>
</head>
<body>
  <!-- All three inputs below are 100% DISABLED! -->
  <!-- The presence of the attribute represents true; value is completely ignored! -->
  <input type="text" disabled>
  <input type="text" disabled="">
  <input type="text" disabled="disabled">

  <!-- DANGEROUS MISTAKE: disabled="false" is STILL DISABLED! -->
  <input type="text" disabled="false">
</body>
</html>
```
- **Real Validator & DOM Output**:
```
DOM Property Inspection:
input[disabled="false"].disabled === true!
```
- **Explanation**: In HTML, boolean attributes evaluate to `true` simply if the attribute name is present on the tag, regardless of value. Setting `disabled="false"` does **not** enable the input! To make it false, the attribute must be omitted entirely.

### Example 3: Edge Case — Raw Text Parsing inside `<script>` and `<style>`
- **Goal**: Observe how the parser treats HTML tags inside raw text containers.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Raw Text Elements</title>
  <style>
    /* Inside <style>, HTML tags are NOT parsed! */
    body { content: "<h1>Not a real heading</h1>"; }
  </style>
</head>
<body>
  <script>
    // The tokenizer does not parse <div> here!
    const template = "<div><p>Embedded HTML string</p></div>";
    console.log(template);
  </script>
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
- **Explanation**: The parser switches to the *RAWTEXT state* inside `<style>` and `<script>`. Tags inside are treated as literal characters until the exact closing `</style>` or `</script>` tag is encountered.

### Example 4: Failure Mode — The `</script>` Substring Trap in JavaScript
- **Goal**: Demonstrate the classic parser breakdown when `</script>` appears in a string.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Script Parsing Bug</title>
</head>
<body>
  <script>
    // FATAL MISTAKE: The HTML parser scans for the literal sequence "</script>"!
    // It DOES NOT understand JavaScript quotes or string boundaries!
    const htmlString = "</script><script>alert('Broken!');</script>";
  </script>
</body>
</html>
```
- **Real Validator & DOM Output**:
```
DOM Result:
The first script closes immediately at the string "</script>"!
The remainder of the string leaks out into the HTML parser as raw page content!
```
- **Explanation**: The HTML tokenizer takes precedence over the JavaScript engine. If the sequence `</script>` appears anywhere inside an inline script (even in a string or comment), the parser closes the script immediately. Fix: escape it as `<\/script>` or `"<" + "/script>"`.

### Example 5: Production-Grade Robust Syntax
- **Goal**: Clean, strictly valid markup passing all enterprise linters.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Clean Syntax Standards</title>
</head>
<body>
  <main>
    <article>
      <header>
        <h1>Engineering Conventions</h1>
      </header>
      <p>Clean HTML5 adheres strictly to lowercase naming and double-quoted attributes.</p>
      <ul>
        <li>Deterministic tag closure</li>
        <li>No obsolete presentational elements</li>
        <li>Correct void element usage</li>
      </ul>
    </article>
  </main>
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
- **Explanation**: Flawless validation. Explicit element closures guarantee consistent parsing across all browser engines.

---

## Best Practices

1. **Always Use Lowercase for Element and Attribute Names**: While the parser is case-insensitive, lowercase is the universal web engineering standard (`<div class="box">`, never `<DIV CLASS="box">`).
2. **Always Double-Quote Attribute Values**: Never use unquoted attributes (`class=btn`). Unquoted attributes fail if a space or special character is introduced later.
3. **Never Use Trailing Slashes on HTML Void Elements**: Write `<img src="a.png" alt="">` and `<br>`, never `<img />` or `<br />`.
4. **Never Write `<div />` or `<span />`**: Slashes on non-void elements do **not** self-close the tag in HTML5. They leave an open element that swallows child nodes.
5. **Escape `</script>` Inside Inline JavaScript**: If writing HTML strings inside `<script>`, always escape the slash: `const str = "<\/script>";`.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Element remains disabled despite `disabled="false"`** | In HTML boolean attributes, the mere presence of the attribute evaluates to true. | Remove the attribute entirely to enable: `<input type="text">`. |
| **Inline script throws unexpected syntax error** | The string literal contains `</script>`, which prematurely terminates the script tag. | Escape the closing tag: `<\/script>`. |
| **Subsequent elements become children of a `<div>`** | Attempting to self-close a non-void element (`<div class="card" />`). | Always provide an explicit closing tag: `<div></div>`. |
| **Text disappears or moves above a table** | Text placed directly inside `<table>` triggering Foster Parenting. | Place all text strictly inside `<caption>`, `<th>`, or `<td>`. |
| **W3C validator throws "End tag for void element"** | Writing closing tags for void elements (e.g. `</img>` or `</input>`). | Remove the end tag; void elements cannot have closing tags. |
| **Attribute value truncated at space** | Omitting quotes around multi-word values (e.g. `<div class=card primary>`). | Always quote attribute values: `class="card primary"`. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
Writing clean, properly closed HTML avoids triggering the **Adoption Agency Algorithm (AAA)**. When the parser must reconstruct and clone misnested formatting nodes, it performs extra memory allocations and CPU iterations.

### Accessibility
Misnested headings or list items that are auto-closed by the parser create unexpected parent-child relationships in the Accessibility Tree, disrupting screen reader navigation order.

### SEO
Clean syntax guarantees that search engine bots extract metadata without parsing ambiguity.

### Security: Mutation XSS (mXSS)
When applications sanitize user input, differences between the sanitizer's parser and the browser's native parser can trigger **Mutation XSS**:
- The sanitizer parses a broken tag and deems it safe.
- When inserted into the DOM, the browser's Error Recovery Algorithm restructures the broken tags, transforming an inert attribute into an executable `<script>` or `onload` handler. Always use established sanitizers (like DOMPurify).

---

## Comparison Matrix: Element Models in HTML5

| Element Category | Self-Closing Slash Permitted? | End Tag Permitted? | Permitted Child Nodes | Examples |
|:---|:---:|:---:|:---|:---|
| **Void Elements** | Ignored (Syntactic sugar) | **Forbidden** (Invalid HTML) | **None** | `<img>`, `<input>`, `<br>`, `<meta>` |
| **Raw Text Elements** | No | **Mandatory** | Plain text only (Tags ignored) | `<script>`, `<style>` |
| **Escapable Raw Text** | No | **Mandatory** | Plain text & character entities | `<textarea>`, `<title>` |
| **Foreign Elements** | **Yes** (Valid XML self-closing) | Optional if self-closed | SVG/MathML markup | `<svg>`, `<math>`, `<circle />` |
| **Normal Elements** | No (Treated as opening tag!) | **Mandatory / Optional** | Phrasing / Flow content | `<div>`, `<p>`, `<a>`, `<section>` |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Explain the Adoption Agency Algorithm (AAA) and what problem it solves in HTML parsing.</strong></summary>

**Answer:**
The **Adoption Agency Algorithm (AAA)** is a specialized sub-procedure in the WHATWG Tree Construction specification designed to resolve **misnested inline formatting tags** (specifically tags in the formatting list: `<b>`, `<i>`, `<a>`, `<em>`, `<strong>`, `<code>`, etc.) with structural block tags.

For example, when an author writes:
```html
<p>One <b>Two <i>Three</p> Four</b> Five</i>
```
Strict XML parsers would crash with a fatal hierarchy error. 
The HTML Adoption Agency Algorithm executes a multi-step recovery:
1. It detects that the block tag `<p>` closed while formatting tags `<b>` and `<i>` were still active on the stack.
2. It closes `<p>`, but "adopts" the active formatting elements: it duplicates and re-opens `<b>` and `<i>` outside the paragraph to wrap the remaining text.
3. This ensures that the visual formatting intent is preserved without creating invalid cross-tree DOM pointer hierarchies.
</details>

<details>
<summary><strong>2. Why does writing &lt;input disabled="false"&gt; not enable the input field?</strong></summary>

**Answer:**
In HTML, `disabled` is a **Boolean Attribute**. 
The specification states that the presence of a boolean attribute on an element represents the `true` value, and the absence of the attribute represents the `false` value.

The actual value string assigned to the attribute (whether `""`, `"disabled"`, `"true"`, or `"false"`) is **completely ignored by the parser**. Because the attribute name `disabled` is physically present on the element, the browser engine sets the internal IDL property `input.disabled = true`. To make the input field enabled, the attribute must be omitted entirely from the HTML markup.
</details>

<details>
<summary><strong>3. What is Foster Parenting in HTML5 parsing? Give a concrete example.</strong></summary>

**Answer:**
**Foster Parenting** is an error-recovery mechanism triggered when the tree construction dispatcher is in table-related insertion modes (e.g. `"in table"` or `"in table body"`) and encounters content that is not legally permitted directly inside a table structure.

For example:
```html
<table>
  <div>Content outside a cell</div>
  <tr><td>Cell 1</td></tr>
</table>
```
Under strict parsing, a `<div>` cannot be a child of `<table>` (tables only permit `<caption>`, `<colgroup>`, `<thead>`, `<tbody>`, `<tfoot>`, and `<tr>`).
Instead of discarding the `<div>`, the parser "foster-parents" it: it inserts the `<div>` **as a child of the table's parent node, positioned immediately before the `<table>`** in the DOM tree.
</details>

<details>
<summary><strong>4. In inline JavaScript, why does const msg = "&lt;/script&gt;" break the entire script block? How do you fix it?</strong></summary>

**Answer:**
The HTML parser operates on a lower, earlier layer of the browser engine than the JavaScript engine. 
When the HTML tokenizer enters the *RAWTEXT state* for a `<script>` tag, it scans the raw character stream looking strictly for the character sequence `</script>` (case-insensitively).

Because the HTML tokenizer **does not understand JavaScript string syntax or quotes**, it does not know that `"</script>"` is inside a JavaScript string literal. The tokenizer encounters the characters `</script>`, immediately terminates the script element, and resumes parsing subsequent characters as standard HTML markup.

**Fix**: Escape the forward slash so the tokenizer does not see the end-tag sequence:
```javascript
const msg = "<\/script>";
// or
const msg = "<" + "/script>";
```
</details>

<details>
<summary><strong>5. What is Mutation XSS (mXSS) and how does it relate to HTML parser error recovery?</strong></summary>

**Answer:**
**Mutation XSS (mXSS)** is an advanced Cross-Site Scripting attack vector where an initially benign-looking, malformed HTML string mutates into an executable malicious payload after being parsed and re-serialized by the browser's native Error Recovery Algorithm.

For example, an application sanitizes user markup using a server-side parser, verifying no `<script>` or `onerror` attributes exist. However, if the markup contains deliberately broken tags (such as unclosed `<listing>`, `<math>`, or `<svg>` tags), the browser's tree construction dispatcher applies error recovery rules, shifting attributes and re-parenting nodes. If the application later reads `element.innerHTML`, the browser serializes the recovered DOM tree into a string that now contains an active executable vector (such as an un-escaped `<img src=x onerror=alert(1)>`).
</details>

<details>
<summary><strong>6. Why are trailing slashes valid on &lt;circle /&gt; inside &lt;svg&gt; but invalid on &lt;div /&gt; in HTML?</strong></summary>

**Answer:**
- `<circle>` is an **SVG Foreign Element**: Inside an `<svg>` container, the HTML parser switches to **Foreign Content insertion mode**, which follows XML rules. In XML/SVG, the self-closing slash (`<circle />`) is fully recognized and actively terminates the element.
- `<div>` is an **HTML Normal Element**: In standard HTML namespace, the self-closing slash is explicitly specified as having **zero effect on non-void elements**. The parser ignores the slash and treats `<div />` purely as an opening tag.
</details>

<details>
<summary><strong>7. When is omitting closing tags permitted in the HTML Living Standard? Give two examples.</strong></summary>

**Answer:**
The WHATWG specification (Section 13.1.2.4 "Optional tags") permits omitting end tags when the subsequent token makes the closure unambiguous:
1. **`<p>` element**: A `<p>` element's end tag (`</p>`) may be omitted if the `<p>` is immediately followed by an address, article, aside, blockquote, div, h1-h6, hr, ol, p, pre, section, or table tag.
2. **`<li>` element**: An `<li>` element's end tag (`</li>`) may be omitted if the `<li>` is immediately followed by another `<li>` element or if there is no more content in the parent list.
*Note*: While valid per specification, omitting tags is strongly discouraged in enterprise engineering style guides.
</details>

<details>
<summary><strong>8. What is an "escapable raw text element" in HTML5? How does it differ from a "raw text element"?</strong></summary>

**Answer:**
- **Raw Text Elements (`<script>`, `<style>`)**: The tokenizer reads all characters as plain text. Character references and HTML entities (like `&amp;` or `&lt;`) are **not resolved**; they remain literal characters.
- **Escapable Raw Text Elements (`<textarea>`, `<title>`)**: HTML tags (like `<b>` or `<div>`) are **not parsed** into element nodes; however, character references and entities **are actively resolved** into their corresponding Unicode characters (e.g. `<title>Rock &amp; Roll</title>` decodes to "Rock & Roll" in the DOM).
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identifying Void Elements</strong></summary>

**Problem:** From the following list of elements, identify which are void elements and must never have end tags: `<p>`, `<img>`, `<span>`, `<input>`, `<br>`, `<button>`, `<meta>`, `<textarea>`.

**Solution:**
- **Void Elements (No end tags allowed)**: `<img>`, `<input>`, `<br>`, `<meta>`
- **Non-Void Elements (Require end tags)**: `<p>`, `<span>`, `<button>`, `<textarea>`
</details>

<details>
<summary><strong>Exercise 2 (Easy): Correcting Boolean Attributes</strong></summary>

**Problem:** The developer wrote `<input type="checkbox" checked="false">` expecting the checkbox to be unchecked. Fix the markup.

**Solution:**
To make the checkbox unchecked, omit the attribute completely:
```html
<input type="checkbox">
```
*(In HTML, any presence of `checked` evaluates to true, even `checked="false"`).*
</details>

<details>
<summary><strong>Exercise 3 (Medium): Resolving Overlapping Formatting Tags</strong></summary>

**Problem:** Trace how the browser's Adoption Agency Algorithm (AAA) parses the following malformed markup into the DOM tree:
```html
<p>Text <b>Bold <i>Italic</p> Still Bold?</b>
```

**Solution:**
The parser automatically closes `<p>`, and clones `<b>` and `<i>` across the boundary:
```
DOM:
p
  ├── "Text "
  └── b
        ├── "Bold "
        └── i
              └── "Italic"
b (Cloned)
  └── " Still Bold?"
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Escaping Inline Scripts</strong></summary>

**Problem:** Fix the syntax bug in this inline script so the browser does not terminate parsing early:
```html
<script>
  let html = "<script src='app.js'></script>";
</script>
```

**Solution:**
Escape the closing script tag:
```html
<script>
  let html = "<script src='app.js'><\/script>";
</script>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Tracing Foster Parenting Programmatically</strong></summary>

**Problem:** Write a JavaScript test with our scratch runner that proves that text placed directly inside a `<table>` is shifted before the table in the DOM.

**Solution:**
```javascript
const { validateHtml } = require('../../scratch/html_runner.js');

const snippet = `<!DOCTYPE html>
<html lang="en">
<head><title>Foster Test</title></head>
<body>
  <table>
    Foster Parented Text
    <tr><td>Cell</td></tr>
  </table>
</body>
</html>`;

const result = validateHtml(snippet);
console.log('Valid:', result.status);
// The browser engine parses "Foster Parented Text" as a sibling preceding the table!
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Clean Modern Document Template</strong></summary>

**Problem:** Construct a clean HTML5 template demonstrating proper lowercase tags, double-quoted attributes, boolean attributes, and void element formatting without slashes.

**Solution:**
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Clean Standards</title>
</head>
<body>
  <form action="/submit" method="post">
    <p>
      <label for="username">Username:</label>
      <input type="text" id="username" name="username" required autofocus>
    </p>
    <p>
      <button type="submit">Submit</button>
    </p>
  </form>
</body>
</html>
```
</details>

---

## Summary & Further Reading

- HTML parsers are resilient state machines that implement deterministic **Error Recovery**; they never throw fatal syntax errors.
- **Void elements** (`<img>`, `<input>`, `<br>`, `<meta>`) can never have children or closing tags.
- Slashes on non-void elements (`<div />`) do **not** self-close in HTML5; they act as unclosed opening tags.
- Boolean attributes evaluate to `true` simply if present, regardless of value (`disabled="false"` is still disabled!).
- **Next Topic**: [03-text-content-and-typography.md](03-text-content-and-typography.md) covers semantic text formatting, headings, paragraphs, whitespace collapsing, and typographic tags.

---
[Previous](01-document-structure.md) | [Section Index](README.md) | [Next](03-text-content-and-typography.md)
