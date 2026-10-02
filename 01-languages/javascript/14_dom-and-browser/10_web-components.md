# Web Components

**Web Components** are browser-native building blocks for reusable UI elements with encapsulated markup, style and behavior. They work in any framework (or none).

Three main technologies:

| Technology | Purpose |
|------------|---------|
| **Custom Elements** | define your own HTML tags with lifecycle callbacks |
| **Shadow DOM** | encapsulated DOM and CSS scope |
| **Templates and slots** (`<template>`, `<slot>`) | reusable markup with content projection |

```html
<user-card name="Ada" role="Engineer"></user-card>
```

## Custom elements

```js
class UserCard extends HTMLElement {
  static observedAttributes = ["name", "role"];        // attributes to watch

  constructor() {
    super();                                           // always first
    this.attachShadow({ mode: "open" });               // create the shadow root
  }

  connectedCallback() { this.render(); }               // inserted into the DOM
  disconnectedCallback() { /* cleanup listeners, timers, observers */ }
  adoptedCallback() { /* moved to another document */ }
  attributeChangedCallback(name, oldValue, newValue) { this.render(); }

  render() {
    this.shadowRoot.innerHTML = `
      <style>
        :host { display: block; padding: 1rem; border: 1px solid #ccc; border-radius: 8px; }
        h2 { margin: 0; font-size: 1.1rem; }
      </style>
      <h2></h2>
      <p><slot>No description</slot></p>
    `;
    this.shadowRoot.querySelector("h2").textContent = `${this.getAttribute("name")} (${this.getAttribute("role")})`;
  }
}

customElements.define("user-card", UserCard);          // name MUST contain a hyphen
```

Rules:

- Tag names are **lowercase with a hyphen** (`user-card`, not `usercard`) so they never clash with built-in tags
- The constructor must not read attributes/children or add children: wait for `connectedCallback`
- Custom elements are **upgraded**: HTML parsed before `define()` is upgraded when the class registers (`customElements.whenDefined("user-card")`)
- Self-closing is not allowed: write `<user-card></user-card>`

## Lifecycle summary

| Callback | When | Typical use |
|----------|------|-------------|
| `constructor` | created / upgraded | set up shadow root, initial state |
| `connectedCallback` | added to the document (can run multiple times) | render, add listeners, start observers |
| `disconnectedCallback` | removed | clean up |
| `attributeChangedCallback` | an observed attribute changes | react to attributes |
| `adoptedCallback` | moved via `document.adoptNode` | rare |

## Properties, attributes and reflection

Attributes are strings in HTML; properties can be any type. Keep them in sync intentionally:

```js
class ToggleSwitch extends HTMLElement {
  static observedAttributes = ["checked"];

  get checked() { return this.hasAttribute("checked"); }
  set checked(value) { this.toggleAttribute("checked", Boolean(value)); }       // reflect property to attribute

  attributeChangedCallback() { this.#update(); }
  connectedCallback() { this.#update(); this.addEventListener("click", this.#onClick); }
  disconnectedCallback() { this.removeEventListener("click", this.#onClick); }

  #onClick = () => {
    this.checked = !this.checked;
    this.dispatchEvent(new CustomEvent("toggle", { detail: { checked: this.checked }, bubbles: true, composed: true }));
  };
  #update() { this.setAttribute("aria-checked", String(this.checked)); this.setAttribute("role", "switch"); this.tabIndex = 0; }
}
customElements.define("toggle-switch", ToggleSwitch);
```

Complex data should be passed as **properties** (`el.items = [...]`), not serialized into attributes.

## Shadow DOM

```js
const shadow = el.attachShadow({ mode: "open" });     // "open": el.shadowRoot accessible; "closed": not (rarely useful)
shadow.append(content);
```

What it gives you:

| Feature | Detail |
|---------|--------|
| **DOM encapsulation** | `document.querySelector` does not see inside |
| **Style encapsulation** | outer CSS does not leak in; inner CSS does not leak out |
| **Composition** | `<slot>` projects light DOM children |
| **Event retargeting** | listeners outside see the **host** as `event.target`; use `composedPath()` for the real origin |

### Styling

```css
/* inside the shadow root */
:host { display: block; }                    /* the host element itself */
:host([disabled]) { opacity: 0.5; }          /* host with an attribute */
:host(:hover) { outline: 2px solid; }
:host-context(.dark) { color: white; }       /* ancestor-based (limited support) */
::slotted(p) { margin: 0; }                  /* top-level slotted light DOM children */
```

Theming across the boundary:

| Mechanism | How |
|-----------|-----|
| **CSS custom properties** (inherit through shadow roots) | `--card-bg`; consumers set `user-card { --card-bg: #fff; }` |
| **`::part()`** | author marks `part="title"` inside; consumers style `user-card::part(title) { color: red; }` |
| **Inherited properties** (`font`, `color`, ...) | pass through automatically |
| **Constructable stylesheets** | share one `CSSStyleSheet` across instances |

```js
const sheet = new CSSStyleSheet();
sheet.replaceSync(`:host { display: block; } .title { font-weight: 700; }`);
shadow.adoptedStyleSheets = [sheet];                // efficient reuse, no duplicate <style> parsing
```

## Templates and slots

```html
<template id="tpl">
  <style> .box { border: 1px solid; padding: 8px; } </style>
  <div class="box">
    <header><slot name="title">Default title</slot></header>
    <slot></slot>                                    <!-- default slot -->
  </div>
</template>
```

```js
class InfoBox extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" }).append(document.getElementById("tpl").content.cloneNode(true));
  }
}
customElements.define("info-box", InfoBox);
```

```html
<info-box>
  <span slot="title">Heads up</span>
  <p>This content goes into the default slot.</p>
</info-box>
```

- Slotted nodes stay in the **light DOM** (styled by the page, and discoverable by `querySelector` on the host)
- `slot.assignedNodes()` / `assignedElements()` and the `slotchange` event let you react to content changes

```js
shadow.querySelector("slot").addEventListener("slotchange", (e) => console.log(e.target.assignedElements()));
```

## Events from components

```js
this.dispatchEvent(new CustomEvent("select", {
  detail: { id: 7 },
  bubbles: true,          // travels up through ancestors
  composed: true,         // crosses the shadow boundary
}));
```

Listeners outside: `card.addEventListener("select", (e) => e.detail.id)`. Events like `click` are already `composed`; custom events are **not** composed by default.

## Form-associated custom elements

Make a custom element participate in forms (value, validation, reset).

```js
class RatingInput extends HTMLElement {
  static formAssociated = true;
  #internals = this.attachInternals();
  #value = 0;

  get value() { return this.#value; }
  set value(v) {
    this.#value = Number(v);
    this.#internals.setFormValue(String(this.#value));                         // included in FormData
    this.#internals.setValidity(this.#value ? {} : { valueMissing: true }, "Pick a rating", this);
  }
  formResetCallback() { this.value = 0; }
}
customElements.define("rating-input", RatingInput);
```

`ElementInternals` also provides ARIA reflection (`internals.role`, `ariaLabel`) and custom states (`internals.states`, `:state(...)`, check support).

## Declarative Shadow DOM (server-side rendering)

```html
<user-card>
  <template shadowrootmode="open">
    <style>:host { display: block; }</style>
    <slot></slot>
  </template>
  Server-rendered content
</user-card>
```

Allows shadow roots to be created by the HTML parser, enabling SSR without JavaScript.

## Customized built-in elements

```js
class FancyButton extends HTMLButtonElement {}
customElements.define("fancy-button", FancyButton, { extends: "button" });
// <button is="fancy-button">
```

Not supported in Safari: prefer **autonomous** custom elements (`class extends HTMLElement`) and compose real `<button>`s inside.

## Accessibility

- Reuse **native elements** inside shadow DOM (`<button>`, `<input>`) when possible
- Set `role`, `aria-*`, `tabindex` appropriately; manage focus (`delegatesFocus: true` in `attachShadow`)
- ARIA ID references (`aria-labelledby`) **do not cross** shadow boundaries; use `ElementInternals` ARIA properties or keep related elements in the same root
- Support keyboard interaction and visible focus styles

```js
this.attachShadow({ mode: "open", delegatesFocus: true });
```

## Libraries and frameworks

| Option | Notes |
|--------|-------|
| **Lit** | tiny library with reactive properties and templates (`html`, `css`) |
| **Stencil**, **FAST**, **Shoelace/Web Awesome**, **Material Web** | compilers and component libraries built on web components |
| Frameworks | React 19+, Vue, Angular, Svelte can consume custom elements (pass complex data as properties; listen for custom events) |

```js
import { LitElement, html, css } from "lit";

class MyCounter extends LitElement {
  static properties = { count: { type: Number } };
  static styles = css`button { padding: 0.5rem 1rem; }`;
  count = 0;
  render() { return html`<button @click=${() => this.count++}>Clicked ${this.count} times</button>`; }
}
customElements.define("my-counter", MyCounter);
```

## When to use web components

| Good fit | Poor fit |
|----------|----------|
| Design systems shared across frameworks | Apps already fully built in one framework with no sharing need |
| Embeddable widgets and third-party components | Components needing heavy SSR/hydration tooling |
| Long-lived UI primitives (buttons, dialogs, inputs) | Highly dynamic data-heavy views where framework state management shines |
| Progressive enhancement of server-rendered HTML | |

## Testing and tooling

- **Web Test Runner**, **Playwright**, **Vitest browser mode**; `@open-wc/testing` helpers
- `customElements.whenDefined(name)` to await registration
- DevTools show shadow roots in the Elements panel (`#shadow-root`)

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Reading attributes/children in the constructor | Not available yet | Use `connectedCallback` |
| Forgetting a hyphen in the tag name | `DOMException` | `my-element` |
| Not cleaning up in `disconnectedCallback` | Leaks, ghost listeners | Remove listeners/timers/observers |
| `connectedCallback` re-running on move | Duplicate setup | Make setup idempotent |
| Custom events without `composed: true` | Cannot escape the shadow root | Set `bubbles` and `composed` |
| Passing objects through attributes | Stringified | Use properties |
| Rebuilding the whole `innerHTML` on every change | Slow, loses focus/state | Update targeted nodes or use Lit |
| Styling internals from the page | Blocked by encapsulation | CSS custom properties, `::part()` |
| ARIA references across shadow roots | Do not resolve | Keep references in one root, `ElementInternals` |
| Relying on customized built-ins | No Safari support | Autonomous elements |
| Defining the same element twice | Error | Guard with `customElements.get(name)` |

## Key takeaways

- Custom elements add your own tags with lifecycle callbacks; names need a hyphen
- Shadow DOM encapsulates markup and styles; slots project content; custom properties and `::part()` expose styling hooks
- Use properties for complex data, attributes for simple state, and `composed` custom events for output
- Clean up in `disconnectedCallback`, and consider Lit for ergonomics

**Next:** [Networking](../15_networking/00_README.md)