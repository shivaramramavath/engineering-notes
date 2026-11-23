# Web Components & Shadow DOM

> Web Components provide native, framework-agnostic component encapsulation built directly into browser standards via Custom Elements, Shadow DOM, and HTML Templates.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [The DOM](./01_dom.md), [DOM Events](./04_events.md), [Classes & OOP](../05_this-and-oop/05_classes.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | W3C Custom Elements / WHATWG DOM Shadow Trees / HTML Templates / Baseline Widely Available |

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

In traditional web development, building reusable UI widgets (such as date pickers, modal dialogs, or video players) suffers from fundamental encapsulation breakdown:
1. **Global CSS Leaks**: CSS styles declared in one part of an application bleed into widget components, or widget styles accidentally override application layouts.
2. **DOM Identifier Collisions**: Global IDs and class names collide easily across independent libraries.
3. **Framework Lock-in**: Components authored in React, Vue, or Angular cannot easily be shared across projects authored in different frameworks without shipping multiple heavy runtime runtimes.

**Web Components** solve this by introducing three native, interoperable browser standards:
- **Custom Elements**: The ability to define custom HTML tags with full lifecycle hooks.
- **Shadow DOM**: True style and DOM subtree encapsulation that shields internal markup from outside scripts and global CSS rules.
- **HTML Templates (`<template>` and `<slot>`)**: Reusable chunks of non-rendered markup with content projection capabilities.

---

## 2. Core Concepts

### The Three Pillars of Web Components

```text
+--------------------------------------------------------------------------+
|                             <user-card>                                  |
|                                                                          |
|  [ Custom Element Definition ]                                           |
|  class UserCard extends HTMLElement { ... }                              |
|  customElements.define('user-card', UserCard);                           |
|                                                                          |
|  [ Shadow DOM Boundary (#shadow-root) ]                                  |
|  +--------------------------------------------------------------------+  |
|  | <style>                                                            |  |
|  |   :host { display: block; border: 1px solid #ccc; }                |  |
|  |   h3 { color: blue; } /* Encapsulated! Does NOT leak outside! */   |  |
|  | </style>                                                           |  |
|  |                                                                    |  |
|  | <h3>User Details</h3>                                              |  |
|  | <slot name="username">Default Name</slot> <── [ Content Projected] |  |
|  +--------------------------------------------------------------------+  |
+--------------------------------------------------------------------------+
```

### Autonomous vs Customized Built-in Elements
1. **Autonomous Custom Elements**: Inherit from `HTMLElement` and define completely new HTML tags (`<ui-modal>`, `<user-avatar>`). Must contain a hyphen in their name.
2. **Customized Built-in Elements**: Inherit from specific HTML element classes (e.g., `HTMLButtonElement`), extending existing tags (`<button is="fancy-button">`). *Note: Safari/WebKit refuses to support customized built-ins, so autonomous elements are the industry standard.*

### Shadow DOM Modes: `open` vs `closed`
- **`mode: 'open'`**: Allows outside JavaScript to inspect the shadow tree via `element.shadowRoot` (recommended).
- **`mode: 'closed'`**: Hides the shadow root (`element.shadowRoot === null`). *Note: Closed mode does not provide real cryptographic security, as internal references can still be intercepted.*

---

## 3. How It Works Internally

### Custom Element Lifecycle Sequence

When a Custom Element is instantiated, mounted, mutated, or unmounted:

```
 [ Browser parses <my-element> or document.createElement('my-element') ]
                                    │
                                    v
 1. constructor()
    - Invoked when element is allocated in memory
    - Must call super()
    - Attach Shadow DOM: this.attachShadow({ mode: 'open' })
                                    │
                                    v
 2. connectedCallback()
    - Invoked when element is inserted into document DOM tree
    - Setup DOM listeners, fetch data, render template
                                    │
                                    v
 3. attributeChangedCallback(name, oldValue, newValue)
    - Invoked when an attribute listed in observedAttributes changes
    - Updates component state in response to attribute mutation
                                    │
                                    v
 4. disconnectedCallback()
    - Invoked when element is removed from DOM tree
    - Clean up timers, abort controllers, global window listeners
                                    │
                                    v
 5. adoptedCallback()
    - Invoked when element is moved to a new document (e.g., via iframe)
```

---

## 4. Syntax & API Reference

### Component Class Declaration

```javascript
class BaseCounter extends HTMLElement {
  // 1. Declare attributes to observe
  static get observedAttributes() {
    return ['count', 'disabled'];
  }

  constructor() {
    super();
    // 2. Attach encapsulated shadow root
    this.attachShadow({ mode: 'open' });
  }

  connectedCallback() {
    // 3. Render when attached to DOM
    this.render();
  }

  disconnectedCallback() {
    // 4. Cleanup on unmount
  }

  attributeChangedCallback(name, oldValue, newValue) {
    // 5. React to attribute changes
    if (oldValue !== newValue) {
      this.render();
    }
  }

  render() {
    this.shadowRoot.innerHTML = `
      <style>
        :host { display: inline-block; font-family: sans-serif; }
        :host([disabled]) { opacity: 0.5; pointer-events: none; }
        button { padding: 4px 8px; cursor: pointer; }
      </style>
      <button id="btn">Count: ${this.getAttribute('count') || 0}</button>
    `;
    this.shadowRoot.getElementById('btn').onclick = () => {
      const next = (parseInt(this.getAttribute('count') || '0', 10)) + 1;
      this.setAttribute('count', String(next));
      this.dispatchEvent(new CustomEvent('count-change', { detail: next, bubbles: true }));
    };
  }
}

// Register custom tag (MUST contain a hyphen!)
customElements.define('base-counter', BaseCounter);
```

### Shadow DOM Styling Selectors
- **`:host`**: Targets the custom element host itself from inside the shadow tree.
- **`:host([disabled])`**: Targets the host when it has the `disabled` attribute.
- **`:host-context(.dark-theme)`**: Targets the host when an ancestor matches the selector.
- **`::slotted(selector)`**: Targets elements projected into a `<slot>` from the light DOM.

---

## 5. Practical Examples

### Example 1: Fully Encapsulated Modal Dialog
```javascript
class UIModal extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          display: none;
          position: fixed;
          top: 0; left: 0; width: 100vw; height: 100vh;
          background: rgba(0,0,0,0.6);
          align-items: center; justify-content: center;
          z-index: 9999;
        }
        :host([open]) {
          display: flex;
        }
        .modal-box {
          background: white; border-radius: 8px; padding: 24px;
          min-width: 320px; max-width: 500px;
          box-shadow: 0 4px 20px rgba(0,0,0,0.3);
        }
        header { font-weight: bold; font-size: 1.25rem; margin-bottom: 12px; }
        footer { margin-top: 16px; display: flex; justify-content: flex-end; }
      </style>

      <div class="modal-box">
        <header><slot name="title">Modal Title</slot></header>
        <div class="content"><slot>Default content goes here...</slot></div>
        <footer>
          <button id="close-btn">Close</button>
        </footer>
      </div>
    `;

    this.shadowRoot.getElementById('close-btn').onclick = () => this.close();
  }

  close() {
    this.removeAttribute('open');
    this.dispatchEvent(new CustomEvent('modal-closed', { bubbles: true, composed: true }));
  }

  open() {
    this.setAttribute('open', '');
  }
}

customElements.define('ui-modal', UIModal);
```

### Example 2: Event Retargeting & Composed Events
```javascript
// Inside a Shadow Root:
// By default, events dispatched inside Shadow DOM do NOT cross the shadow boundary!
// To allow an event to bubble out to the outer document:
function emitScoreEvent(element, score) {
  element.dispatchEvent(new CustomEvent('score-update', {
    detail: { score },
    bubbles: true,   // Allow bubbling up the DOM tree
    composed: true,  // Allow event to cross the Shadow DOM boundary into Light DOM!
  }));
}

// In outer light DOM:
document.body.addEventListener('score-update', (event) => {
  // Event retargeting: event.target appears as the Custom Element (<game-widget>),
  // NOT the internal button that actually fired the dispatch!
  console.log('Event target in outer document:', event.target.tagName); // "GAME-WIDGET"
});
```

### Example 3: Reusable Template Cloning with Slots
```html
<template id="user-badge-template">
  <style>
    .badge { display: flex; align-items: center; gap: 8px; }
    ::slotted(img) { border-radius: 50%; width: 40px; height: 40px; }
  </style>
  <div class="badge">
    <slot name="avatar"></slot>
    <slot name="name">Anonymous</slot>
  </div>
</template>

<script>
class UserBadge extends HTMLElement {
  constructor() {
    super();
    const template = document.getElementById('user-badge-template');
    const shadowRoot = this.attachShadow({ mode: 'open' });
    shadowRoot.appendChild(template.content.cloneNode(true));
  }
}
customElements.define('user-badge', UserBadge);
</script>
```

### Example 4: Declarative Shadow DOM (SSR Support)
```html
<!-- Modern browsers support Declarative Shadow DOM for Server-Side Rendering! -->
<!-- No JavaScript required to render initial encapsulated shadow tree! -->
<fancy-card>
  <template shadowrootmode="open">
    <style>
      p { color: darkblue; font-style: italic; }
    </style>
    <p>This renders with encapsulated styles instantly, before JS evaluates!</p>
  </template>
</fancy-card>
```

### Example 5: Synchronizing Properties and Attributes
```javascript
class ToggleButton extends HTMLElement {
  static get observedAttributes() { return ['active']; }

  // Sync boolean property to HTML attribute
  get active() {
    return this.hasAttribute('active');
  }

  set active(val) {
    if (Boolean(val)) {
      this.setAttribute('active', '');
    } else {
      this.removeAttribute('active');
    }
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (name === 'active') {
      this.classList.toggle('btn-active', this.active);
    }
  }
}
customElements.define('toggle-button', ToggleButton);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Mandatory Hyphen in Tag Names**: The HTML standard requires all custom element names to contain at least one hyphen (e.g. `<app-header>`, `<user-card>`, not `<header>` or `<card>`). This guarantees no collision with future native HTML elements.
2. **`constructor()` DOM Access Prohibition**: You **must not** inspect attributes or children inside the `constructor()`, because during parsing, attributes and children have not yet been attached! Perform DOM queries and attribute initialization inside `connectedCallback()`.
3. **Event Retargeting across Shadow Boundaries**: When a bubbling event crosses the Shadow DOM boundary (`composed: true`), the browser **retargets** `event.target` to point to the host Custom Element rather than the internal shadow node. Use `event.composedPath()` to inspect the real internal dispatch path.
4. **CSS Inheritance Leaks**: While classes and element tag styles do not cross the Shadow DOM boundary, **inheritable CSS properties** (like `color`, `font-family`, `line-height`, `visibility`) **do inherit** from the light DOM parent into the shadow root! Use `all: initial` inside `:host` to completely reset inherited styles.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Omitting Hyphen in Name** | `customElements.define('card', Card)` | Use `'my-card'` | Custom element names must contain a hyphen; throws DOMException. |
| **DOM Queries in Constructor** | `this.getAttribute('id')` in constructor | Move to `connectedCallback()` | Element attributes and children are not parsed during construction. |
| **Missing `super()` Call** | `constructor() { this.attachShadow() }` | Always call `super()` first | Uncaught ReferenceError: Must call super constructor. |
| **Non-Composed Custom Events** | `new CustomEvent('save', { bubbles: true })` | Add `{ composed: true }` | Events without `composed: true` cannot cross the shadow boundary. |
| **Modifying Children in Constructor**| `this.appendChild(div)` in constructor | Append to `this.shadowRoot` or in connected | Violates HTML parser contract for Custom Elements. |
| **Styling Slotted Elements Deeply** | `::slotted(div > p)` | `::slotted(div)` | `::slotted()` can only style top-level slotted nodes, not nested children. |

---

## 8. Performance Characteristics

- **Zero-Runtime Framework Overhead**: Web Components run natively in C++ inside the browser rendering engine, requiring 0KB of React/Vue framework runtime overhead.
- **Style Invalidation Scoping**: Modifying CSS styles inside a Shadow Root invalidates styles **only for that shadow tree**, preventing expensive document-wide CSS rule recalculations.

---

## 9. Engine Internals

1. **Blink `ShadowRoot` Tree**: In Chromium Blink, `ShadowRoot` is a C++ node derived from `blink::DocumentFragment` with a special `Type::kOpen` or `Type::kClosed` flag.
2. **Flat Tree Resolution**: During the layout phase, the rendering engine resolves the **Flat Tree (Composed Tree)**: it merges Light DOM `<slot>` assignments with Shadow DOM markup into a single flattened layout tree for layout and paint.

---

## 10. Best Practices & Design Guidelines

1. **Keep Component API Accessible via Attributes & Properties**: Mirror public properties to attributes so consumers can configure components via markup or script.
2. **Use `composed: true` for Public Events**: Ensure public custom events emitted by the component can bubble out to parent frameworks.
3. **Use CSS Custom Properties (Variables) for Theming**: Expose styling hooks via CSS variables (`color: var(--card-bg, #fff);`) since CSS variables pierce the shadow boundary.
4. **Clean Up Listeners in `disconnectedCallback`**: Always remove window-level listeners and disconnect internal observers when the component unmounts.

---

## 11. Real-World Case Study

### Cross-Framework Design System Component Library

An enterprise with applications written in React, Angular, and vanilla HTML requires a universal, framework-agnostic design system. They build their components as native Web Components with CSS custom variable theming.

```javascript
class EnterpriseDatePicker extends HTMLElement {
  #input;
  #controller = new AbortController();

  static get observedAttributes() {
    return ['value', 'disabled', 'min', 'max'];
  }

  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          display: inline-flex;
          font-family: var(--ds-font-family, system-ui);
        }
        .wrapper {
          display: flex; align-items: center;
          border: 1px solid var(--ds-border-color, #ccc);
          border-radius: var(--ds-border-radius, 4px);
          padding: 6px 10px;
          background: var(--ds-bg-color, #fff);
        }
        :host([disabled]) .wrapper {
          background: #f5f5f5; opacity: 0.6; pointer-events: none;
        }
        input {
          border: none; outline: none;
          font-size: 1rem; color: var(--ds-text-color, #333);
        }
      </style>
      <div class="wrapper">
        <input type="date" id="date-input">
      </div>
    `;

    this.#input = this.shadowRoot.getElementById('date-input');
    this.#syncAttributes();

    const { signal } = this.#controller;
    this.#input.addEventListener('change', () => {
      this.setAttribute('value', this.#input.value);
      this.dispatchEvent(new CustomEvent('date-selected', {
        detail: { value: this.#input.value },
        bubbles: true,
        composed: true, // Crosses shadow boundary to React/Angular apps!
      }));
    }, { signal });
  }

  disconnectedCallback() {
    this.#controller.abort(); // Clean up listeners
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (this.#input && oldVal !== newVal) {
      this.#syncAttributes();
    }
  }

  #syncAttributes() {
    if (!this.#input) return;
    if (this.hasAttribute('value')) this.#input.value = this.getAttribute('value');
    if (this.hasAttribute('min')) this.#input.min = this.getAttribute('min');
    if (this.hasAttribute('max')) this.#input.max = this.getAttribute('max');
    this.#input.disabled = this.hasAttribute('disabled');
  }

  get value() { return this.getAttribute('value'); }
  set value(val) { this.setAttribute('value', val); }
}

customElements.define('ds-date-picker', EnterpriseDatePicker);
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What are the three core technologies that comprise Web Components?</strong></summary>

1. **Custom Elements**: A set of JavaScript APIs to define custom HTML tags and their lifecycle behavior.
2. **Shadow DOM**: A private, encapsulated DOM tree attached to an element that isolates styles, IDs, and markup from the rest of the document.
3. **HTML Templates (`<template>` and `<slot>`)**: Markup fragments that are not rendered until instantiated, supporting content projection.
</details>

<details>
<summary><strong>2. Why must Custom Element tag names contain a hyphen?</strong></summary>

To prevent collisions with standard HTML elements. The W3C and WHATWG reserve all single-word tag names (like `<card>`, `<button>`, `<accordion>`) for future HTML specification additions.
</details>

<details>
<summary><strong>3. What is the difference between <code>connectedCallback()</code> and the <code>constructor()</code>?</strong></summary>

The `constructor()` is called when the element is instantiated in memory. At that point, attributes and children have not yet been attached. `connectedCallback()` is called when the element is actually inserted into the active DOM tree, making it the appropriate place to perform initial rendering, fetch data, and inspect attributes.
</details>

<details>
<summary><strong>4. How do styles defined inside a Shadow Root interact with external global styles?</strong></summary>

Styles defined inside a Shadow Root cannot leak out to affect the external document, and global CSS selectors (classes, IDs, tags) cannot penetrate into the Shadow DOM. However, **inheritable CSS properties** (such as `font-family`, `color`) and **CSS Custom Properties (Variables)** cross the shadow boundary naturally.
</details>

<details>
<summary><strong>5. What is "Event Retargeting" in the Shadow DOM?</strong></summary>

When an event originates inside a Shadow DOM tree and bubbles across the shadow boundary into the light DOM (`composed: true`), the browser retargets `event.target` to refer to the host Custom Element rather than the internal shadow node, preserving internal component encapsulation.
</details>

<details>
<summary><strong>6. What does <code>::slotted()</code> do, and what is its primary limitation?</strong></summary>

`::slotted(selector)` allows styling elements that have been placed into a `<slot>` from the light DOM. Its limitation is that it can only target top-level slotted elements; it cannot style nested descendants inside slotted elements.
</details>

<details>
<summary><strong>7. What is Declarative Shadow DOM?</strong></summary>

Declarative Shadow DOM allows authoring Shadow DOM directly in static HTML using `<template shadowrootmode="open">`, enabling Server-Side Rendering (SSR) for Web Components without requiring JavaScript execution to render initial shadow trees.
</details>

<details>
<summary><strong>8. What is the difference between <code>observedAttributes</code> and standard properties?</strong></summary>

`observedAttributes` is a static getter returning an array of attribute names that trigger `attributeChangedCallback()` when modified. Standard JavaScript properties are object fields that must be explicitly synchronized with attributes if reflection is desired.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Accessible Tabs Web Component
Implement a `<ui-tabs>` custom element with encapsulated styles that projects tab headers and panels using slots and handles keyboard navigation (`ArrowRight`, `ArrowLeft`) across tabs.

<details>
<summary>View Solution</summary>

```javascript
class UITabs extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host { display: block; }
        .tab-bar { display: flex; border-bottom: 2px solid #ddd; gap: 8px; }
        ::slotted([role="tab"]) {
          cursor: pointer; padding: 8px 16px; border: none; background: none; font-size: 1rem;
        }
        ::slotted([role="tab"][aria-selected="true"]) {
          border-bottom: 2px solid blue; color: blue; font-weight: bold;
        }
        ::slotted([role="tabpanel"]:not([aria-hidden="false"])) {
          display: none;
        }
      </style>
      <div class="tab-bar" role="tablist">
        <slot name="tab"></slot>
      </div>
      <div class="panel-container">
        <slot name="panel"></slot>
      </div>
    `;

    this.shadowRoot.addEventListener('click', (e) => {
      const tab = e.target.closest('[role="tab"]');
      if (tab) this.selectTab(tab);
    });
  }

  selectTab(selectedTab) {
    const tabs = this.querySelectorAll('[role="tab"]');
    const panels = this.querySelectorAll('[role="tabpanel"]');

    tabs.forEach((tab, index) => {
      const isMatch = tab === selectedTab;
      tab.setAttribute('aria-selected', String(isMatch));
      if (panels[index]) {
        panels[index].setAttribute('aria-hidden', String(!isMatch));
      }
    });
  }
}

customElements.define('ui-tabs', UITabs);
```
</details>

### Challenge 2: Implement a Theme-Reactive Badge Component
Write a `<theme-badge>` custom element that inspects if an ancestor has class `.dark-mode` using the CSS `:host-context()` selector, automatically inverting its color palette without JavaScript polling.

<details>
<summary>View Solution</summary>

```javascript
class ThemeBadge extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          display: inline-block;
          padding: 4px 10px;
          border-radius: 12px;
          font-size: 0.85rem;
          background-color: #e0e0e0;
          color: #222;
        }
        /* Inverts colors when an ancestor has .dark-mode */
        :host-context(.dark-mode) {
          background-color: #333;
          color: #fff;
          border: 1px solid #555;
        }
      </style>
      <span><slot>Badge</slot></span>
    `;
  }
}

customElements.define('theme-badge', ThemeBadge);
```
</details>
