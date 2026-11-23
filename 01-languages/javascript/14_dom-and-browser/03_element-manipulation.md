# DOM Element Manipulation

> Dynamic modification of DOM element content, attributes, properties, styles, and classes: understanding reflows, XSS defense, the Sanitizer API, and high-performance batch mutations.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The DOM](./01_dom.md), [Selectors & Traversal](./02_selectors.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | WHATWG DOM / HTML Living Standard / CSSOM / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In interactive web applications, static HTML documents are continuously transformed into responsive interfaces through **DOM Element Manipulation**. Modifying an element encompasses altering its text content, attributes, class names, inline styles, or replacing its child nodes dynamically.

However, element manipulation is directly coupled to browser layout rendering. Naive updates—such as reading layout geometry immediately after setting style properties—trigger **Forced Synchronous Layouts (Layout Thrashing)**, causing dropped frames and sluggish UI responsiveness. Furthermore, improper insertion of dynamic strings via `innerHTML` is the primary vector for **Cross-Site Scripting (XSS)** security vulnerabilities.

---

## 2. Core Concepts

### Attributes vs Properties
One of the most frequent sources of confusion in DOM programming is the distinction between HTML **Attributes** and JavaScript DOM **Properties**:
- **Attributes**: Stored in the HTML markup as written (`<input value="default" class="btn">`). Values are always strings. Accessed via `getAttribute()` and `setAttribute()`.
- **Properties**: Stored as live properties on the underlying JavaScript DOM object (`input.value = "user input"`). Properties can be booleans, numbers, or objects.

```text
HTML Markup: <input id="age" value="25" type="text" checked>

Attribute: input.getAttribute('value')  ──> "25" (Original default string)
Property:  input.value                 ──> Current live text entered by user
Attribute: input.getAttribute('checked')──> "" (Presence indicates true)
Property:  input.checked               ──> true (Boolean property)
```

### Content Insertion Primitives
- **`textContent`**: Sets or retrieves the raw text of a node and all its descendants. Fast and completely safe against XSS injection because strings are treated as plain text, never parsed as markup.
- **`innerText`**: Renders layout-aware text. Triggers reflow to determine CSS visibility (`display: none` text is omitted).
- **`innerHTML`**: Parses a string as HTML markup and injects the resulting DOM nodes. Powerful, but dangerous if used with unsanitized user inputs.
- **`setHTMLUnsafe()` / Sanitizer API**: Modern standards for parsing HTML while stripping script tags and malicious attributes.

---

## 3. How It Works Internally

### The Reflow and Repaint Cycle

When JavaScript mutates an element, the browser marks portions of the tree as "dirty":

```
 JavaScript mutates DOM: element.style.width = '200px'
                         │
                         ▼
             [ DOM Tree Marked Dirty ]
                         │
                         ▼
        Does script immediately request geometry?
        (e.g., const w = element.offsetWidth;)
               /                     \
         YES  /                       \ NO
             v                         v
 [ FORCED SYNCHRONOUS REFLOW ]   [ Batch Reflow at next frame ]
 Browser stops JS immediately,   Recalculate Styles
 parses CSSOM, computes layout   Compute Layout (Geometry)
 (LAYOUT THRASHING HAZARD!)      Repaint & Composite Layers
```

---

## 4. Syntax & API Reference

### Attributes & Datasets

```javascript
// Attribute manipulation
element.hasAttribute('disabled');
element.getAttribute('href');
element.setAttribute('aria-expanded', 'true');
element.removeAttribute('data-pending');

// Data attributes (data-*)
// Automatically camelCased via dataset API:
element.dataset.userId = '42';          // Sets data-user-id="42"
const role = element.dataset.userRole;  // Reads data-user-role
delete element.dataset.pending;         // Removes data-pending
```

### Class Manipulation via `classList`

```javascript
element.classList.add('active', 'visible');
element.classList.remove('loading');
element.classList.toggle('collapsed');       // Inverts state
element.classList.toggle('pinned', isPinned);// Explicit force boolean
element.classList.contains('active');        // Returns boolean
element.classList.replace('old-cls', 'new-cls');
```

### Styles and CSS Variables

```javascript
// Inline styles (CamelCase properties)
element.style.backgroundColor = '#f0f0f0';
element.style.transform = 'translateY(10px)';

// CSS Variables (Custom Properties)
element.style.setProperty('--theme-color', '#ff4500');
const primaryColor = element.style.getPropertyValue('--theme-color');

// Read computed styles (including styles from external CSS sheets)
const computed = window.getComputedStyle(element);
console.log('Computed font-size:', computed.fontSize);
```

---

## 5. Practical Examples

### Example 1: Avoiding Layout Thrashing via Read-Write Batching
```javascript
// BAD (Causes Layout Thrashing):
// Interleaving reads and writes forces browser to recalculate layout repeatedly!
function badResize(elements) {
  for (const el of elements) {
    const width = el.offsetWidth; // READ (Forces reflow)
    el.style.width = `${width + 10}px`; // WRITE (Dirties layout)
  }
}

// GOOD (Batched Reads, then Batched Writes):
function optimizedResize(elements) {
  // Phase 1: Batch all geometry reads
  const widths = elements.map(el => el.offsetWidth);

  // Phase 2: Batch all style mutations
  elements.forEach((el, index) => {
    el.style.width = `${widths[index] + 10}px`;
  });
}
```

### Example 2: Safe HTML Templating with `DocumentFragment`
```javascript
function createUserCard(user) {
  const card = document.createElement('article');
  card.className = 'user-card';

  const avatar = document.createElement('img');
  avatar.src = user.avatarUrl;
  avatar.alt = `${user.name}'s profile picture`;

  const info = document.createElement('div');
  info.className = 'user-info';

  const nameHeading = document.createElement('h3');
  nameHeading.textContent = user.name; // Safe against XSS!

  const bioParagraph = document.createElement('p');
  bioParagraph.textContent = user.bio; // Safe against XSS!

  info.append(nameHeading, bioParagraph);
  card.append(avatar, info);

  return card;
}
```

### Example 3: Dynamic Theme Switching via CSS Variables
```javascript
function applyTheme(theme) {
  const root = document.documentElement;

  if (theme === 'dark') {
    root.style.setProperty('--bg-color', '#121212');
    root.style.setProperty('--text-color', '#ffffff');
    root.style.setProperty('--accent-color', '#bb86fc');
  } else {
    root.style.setProperty('--bg-color', '#ffffff');
    root.style.setProperty('--text-color', '#000000');
    root.style.setProperty('--accent-color', '#6200ee');
  }
}
```

### Example 4: Parsing Markup Safely using `DOMParser`
```javascript
function parseUntrustedHtmlSafely(rawHtmlString) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(rawHtmlString, 'text/html');

  // Strip script tags and dangerous event attributes
  doc.querySelectorAll('script').forEach(el => el.remove());
  doc.querySelectorAll('*').forEach(el => {
    for (const attr of Array.from(el.attributes)) {
      if (attr.name.startsWith('on') || attr.value.startsWith('javascript:')) {
        el.removeAttribute(attr.name);
      }
    }
  });

  return doc.body.children;
}
```

### Example 5: Toggle Switch Component with Accessible Attributes
```javascript
class ToggleSwitch {
  constructor(element) {
    this.el = element;
    this.el.setAttribute('role', 'switch');
    this.el.setAttribute('tabindex', '0');
    this.el.addEventListener('click', () => this.toggle());
    this.el.addEventListener('keydown', (e) => {
      if (e.key === ' ' || e.key === 'Enter') {
        e.preventDefault();
        this.toggle();
      }
    });
  }

  toggle() {
    const isChecked = this.el.getAttribute('aria-checked') === 'true';
    const newState = !isChecked;

    this.el.setAttribute('aria-checked', String(newState));
    this.el.classList.toggle('switch-on', newState);
    this.el.dispatchEvent(new CustomEvent('change', { detail: { checked: newState } }));
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Boolean Attributes in HTML**: Boolean attributes (`disabled`, `checked`, `hidden`, `required`) are considered `true` if present, regardless of their string value! Writing `el.setAttribute('disabled', 'false')` still disables the button! To enable it, you must invoke `el.removeAttribute('disabled')` or set the boolean property `el.disabled = false`.
2. **`style` Property vs `getComputedStyle`**: `element.style` contains **only** styles set inline directly on that element. Styles applied via external CSS stylesheets or `<style>` blocks return empty strings when queried through `element.style`. Always use `window.getComputedStyle(element)` to read active styles.
3. **`innerHTML` Script Execution**: Scripts inserted via `innerHTML` are not executed by modern browsers. However, malicious markup can still execute JavaScript via `onerror` attributes (`<img src="x" onerror="stealTokens()">`).
4. **Data Attribute Hyphenation**: Attributes named `data-app-user-id` are automatically camelCased to `dataset.appUserId`. Setting `dataset.AppUserId` creates `data--app-user-id` with extra hyphens.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **`setAttribute('disabled', 'false')`**| `btn.setAttribute('disabled', 'false')` | `btn.disabled = false` or `btn.removeAttribute('disabled')` | In HTML, the mere presence of `disabled` means true. |
| **Reading Styles from `element.style`** | `const color = el.style.color` | `window.getComputedStyle(el).color` | `el.style` only reads inline styles; misses stylesheet rules. |
| **Layout Thrashing in Loops** | Reading `.offsetHeight` then setting `.height` in loop | Batch all reads before executing writes | Triggers repeated forced synchronous reflows. |
| **Concatenating User Input into `innerHTML`**| `el.innerHTML = '<b>' + input + '</b>'` | Use `textContent` or template with DOM nodes | Creates critical Cross-Site Scripting (XSS) vulnerabilities. |
| **Direct String Class Replacement** | `el.className = el.className + ' active'` | `el.classList.add('active')` | Prone to whitespace bugs and accidental duplicates. |
| **Setting Inline Styles with Strings** | `el.setAttribute('style', 'color: red')`| `el.style.color = 'red'` | Overwrites all existing inline styles on the element. |

---

## 8. Performance Characteristics

- **Reflow Triggers**: Accessing properties like `offsetWidth`, `clientHeight`, `scrollLeft`, `getBoundingClientRect()`, or calling `getComputedStyle()` after a DOM mutation forces synchronous layout calculation.
- **`classList` vs `className`**: `classList.add()` uses compiled C++ hash sets to avoid duplicate entries and is substantially faster than string splitting/regex operations on `className`.
- **CSS Variable Cascade**: Setting a CSS variable on `:root` recalculates styles for the entire document tree; setting it on a localized component element scopes recalculations strictly to that subtree.

---

## 9. Engine Internals

1. **CSSStyleDeclaration Wrapper**: Accessing `element.style` allocates a lightweight V8 wrapper around Blink's native `blink::MutableCSSPropertyValueSet`.
2. **Computed Style Caching**: Blink caches computed styles in the `Element::ComputedStyle` structure. When an element or ancestor style mutates, the cache is invalidated, and styles are re-resolved during the next style calculation pass.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `textContent` for User-Generated Strings**: Eliminate XSS injection risks by default.
2. **Batch Layout Reads and Writes**: Separate DOM read phases from DOM mutation phases to eliminate layout thrashing.
3. **Use CSS Classes for Style Mutations**: Avoid hardcoding multiple inline style declarations; toggle semantic CSS classes via `classList`.
4. **Use `dataset` for Data Attributes**: Leverage `dataset` to attach metadata cleanly without polluting element property prototypes.

---

## 11. Real-World Case Study

### High-Performance Drag-and-Drop Card Reordering

A Trello-like kanban board moves task cards across columns. Smooth 60 FPS performance requires updating card positions without triggering document-wide reflows.

```javascript
class DraggableCardManager {
  #draggedEl = null;
  #placeholder = null;

  constructor(boardElement) {
    this.board = boardElement;
    this.#init();
  }

  #init() {
    this.board.addEventListener('dragstart', (e) => this.#onDragStart(e));
    this.board.addEventListener('dragover', (e) => this.#onDragOver(e));
    this.board.addEventListener('dragend', () => this.#onDragEnd());
  }

  #onDragStart(e) {
    const card = e.target.closest('.kanban-card');
    if (!card) return;

    this.#draggedEl = card;
    card.classList.add('is-dragging');

    // Create lightweight placeholder node
    this.#placeholder = document.createElement('div');
    this.#placeholder.className = 'kanban-placeholder';
    this.#placeholder.style.height = `${card.offsetHeight}px`;

    // Defer DOM modification so drag image snapshot captures original card
    requestAnimationFrame(() => {
      card.before(this.#placeholder);
      card.style.display = 'none';
    });
  }

  #onDragOver(e) {
    e.preventDefault();
    if (!this.#draggedEl || !this.#placeholder) return;

    const column = e.target.closest('.kanban-column');
    if (!column) return;

    const cardContainer = column.querySelector('.card-list');
    const targetCard = e.target.closest('.kanban-card:not(.is-dragging)');

    if (targetCard) {
      const rect = targetCard.getBoundingClientRect();
      const isAfter = e.clientY > rect.top + rect.height / 2;
      targetCard[isAfter ? 'after' : 'before'](this.#placeholder);
    } else if (cardContainer && !cardContainer.contains(this.#placeholder)) {
      cardContainer.appendChild(this.#placeholder);
    }
  }

  #onDragEnd() {
    if (!this.#draggedEl || !this.#placeholder) return;

    // Move real card to final placeholder position
    this.#placeholder.replaceWith(this.#draggedEl);
    this.#draggedEl.style.display = '';
    this.#draggedEl.classList.remove('is-dragging');

    this.#draggedEl = null;
    this.#placeholder = null;
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between an HTML attribute and a DOM property?</strong></summary>

An attribute represents initial markup declared in HTML; its value is always a string and is read via `getAttribute()`. A DOM property is a live, typed field on the JavaScript DOM object (e.g. `input.checked` is a boolean). Some properties reflect their attributes (like `id`), while others diverge once mutated by the user (like `input.value`).
</details>

<details>
<summary><strong>2. What is "Layout Thrashing" (Forced Synchronous Layout)?</strong></summary>

Layout Thrashing occurs when JavaScript repeatedly alternates between mutating the DOM (which dirties the layout) and reading layout geometry properties (like `offsetWidth`, `clientHeight`), forcing the browser to pause JavaScript execution and recalculate styles and layout synchronously on every iteration.
</details>

<details>
<summary><strong>3. Why is <code>element.style.color</code> empty if a color was defined in an external stylesheet?</strong></summary>

`element.style` accesses only the element's inline style declarations. To inspect styles calculated from external stylesheets or `<style>` rules, you must use `window.getComputedStyle(element)`.
</details>

<details>
<summary><strong>4. How do you properly disable a button element using attributes vs properties?</strong></summary>

Using properties: `btn.disabled = true;` (or `false` to enable).
Using attributes: `btn.setAttribute('disabled', '');` to disable, and `btn.removeAttribute('disabled');` to enable. Setting `setAttribute('disabled', 'false')` still disables the button because boolean attributes in HTML evaluate to true by their mere presence.
</details>

<details>
<summary><strong>5. Why is <code>textContent</code> preferred over <code>innerHTML</code> for inserting plain text?</strong></summary>

`textContent` treats input as literal text, preventing cross-site scripting (XSS) attacks. Additionally, `textContent` avoids the performance cost of invoking the HTML parser.
</details>

<details>
<summary><strong>6. What is the difference between <code>outerHTML</code> and <code>innerHTML</code>?</strong></summary>

`innerHTML` represents the serialized HTML markup of the element's children. `outerHTML` represents the serialized markup of the element itself, including its opening tag, attributes, child contents, and closing tag.
</details>

<details>
<summary><strong>7. How does <code>classList.toggle(className, force)</code> work when the force argument is provided?</strong></summary>

If the optional boolean `force` argument is provided, `classList.toggle` does not invert the state. If `force === true`, it adds the class; if `force === false`, it removes the class.
</details>

<details>
<summary><strong>8. What is the modern standard replacement for unsafe <code>innerHTML</code> assignments?</strong></summary>

The emerging `Element.prototype.setHTMLUnsafe()` method, and the W3C Sanitizer API (`setHTML(input, { sanitizer })`), which parses HTML markup while automatically stripping dangerous scripts, event handlers, and protocol URIs.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an XSS-Safe HTML String Tagged Template
Create a tagged template function `safeHtml` that sanitizes all interpolated values, converting special characters (`<`, `>`, `&`, `"`, `'`) into HTML entities while preserving literal template strings.

<details>
<summary>View Solution</summary>

```javascript
function safeHtml(strings, ...values) {
  const escapeChar = (str) => {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  };

  return strings.reduce((acc, str, i) => {
    const val = i < values.length ? escapeChar(values[i]) : '';
    return acc + str + val;
  }, '');
}

// Verification:
const untrusted = '<script>alert("hack")</script>';
const cleanHtml = safeHtml`<div class="comment">${untrusted}</div>`;
// Result: '<div class="comment">&lt;script&gt;alert(&quot;hack&quot;)&lt;/script&gt;</div>'
```
</details>

### Challenge 2: Implement a Fast Batch Style Applicator
Write a function `applyStyles(element, styleObject)` that sets multiple style properties on an element efficiently, supporting both camelCase property names, CSS custom variables (`--var`), and `!important` priorities.

<details>
<summary>View Solution</summary>

```javascript
function applyStyles(element, styleObject) {
  for (const [prop, value] of Object.entries(styleObject)) {
    if (prop.startsWith('--')) {
      element.style.setProperty(prop, value);
    } else if (typeof value === 'string' && value.includes('!important')) {
      const cleanVal = value.replace('!important', '').trim();
      const cssProp = prop.replace(/([A-Z])/g, '-$1').toLowerCase();
      element.style.setProperty(cssProp, cleanVal, 'important');
    } else {
      element.style[prop] = value;
    }
  }
}
```
</details>
