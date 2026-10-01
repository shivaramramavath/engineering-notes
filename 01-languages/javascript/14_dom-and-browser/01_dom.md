# The DOM

The **Document Object Model (DOM)** is the browser's in-memory, object representation of an HTML document. JavaScript reads and modifies the page through it.

```html
<body>
  <h1 class="title">Hello</h1>
  <p>Some <b>bold</b> text</p>
</body>
```

```
document
└── html
    └── body
        ├── h1.title
        │   └── "Hello"          (text node)
        └── p
            ├── "Some "
            ├── b
            │   └── "bold"
            └── " text"
```

## Node types

| `nodeType` | Interface | Examples |
|-----------|-----------|----------|
| 1 | `Element` | `<div>`, `<p>` |
| 3 | `Text` | text between tags (whitespace counts) |
| 8 | `Comment` | `<!-- note -->` |
| 9 | `Document` | `document` |
| 10 | `DocumentType` | `<!doctype html>` |
| 11 | `DocumentFragment` | lightweight container (also shadow roots) |

Class hierarchy (simplified): `EventTarget` → `Node` → `Element` → `HTMLElement` → `HTMLDivElement`, `HTMLInputElement`, ...

```js
document.body instanceof HTMLElement;      // true
document.body.nodeType;                     // 1
document.body.tagName;                      // "BODY"
```

## Global entry points

| Object | Role |
|--------|------|
| `window` / `globalThis` | the global object and browsing context |
| `document` | the page: `documentElement` (`<html>`), `head`, `body`, `title`, `readyState` |
| `location`, `history`, `navigator` | URL, session history, browser info |
| `screen`, `visualViewport` | display and viewport information |

## Traversal

| Need | Element-only (prefer) | Includes text/comment nodes |
|------|----------------------|----------------------------|
| Parent | `parentElement` | `parentNode` |
| Children | `children` | `childNodes` |
| First / last child | `firstElementChild`, `lastElementChild` | `firstChild`, `lastChild` |
| Siblings | `previousElementSibling`, `nextElementSibling` | `previousSibling`, `nextSibling` |
| Ancestor match | `closest(selector)` | |
| Containment | `contains(node)` | |

```js
const p = document.querySelector("p");
p.parentElement;                 // <body>
p.children.length;               // 1 (only <b>)
p.childNodes.length;             // 3 (text, b, text)
p.nextElementSibling;
p.closest("body");
document.body.contains(p);       // true
```

Whitespace between tags becomes **text nodes**, which is why `firstChild` often returns `"\n  "`. Use the `*Element*` properties.

## Live vs static collections

| Collection | Type | Live? |
|-----------|------|-------|
| `getElementsByClassName`, `getElementsByTagName`, `element.children` | `HTMLCollection` | **live** (updates automatically) |
| `childNodes` | `NodeList` | **live** |
| `querySelectorAll` | `NodeList` | **static** snapshot |
| `form.elements`, `document.forms`, `document.images` | `HTMLCollection` | live |

```js
const divs = document.getElementsByTagName("div");   // live
const copy = document.querySelectorAll("div");       // static
document.body.append(document.createElement("div"));
divs.length;     // increased
copy.length;     // unchanged
```

Looping over a live collection while adding or removing nodes can skip or loop forever: convert first.

```js
for (const el of Array.from(live)) el.remove();
[...document.querySelectorAll(".item")].forEach((el) => el.remove());
```

`NodeList` has `forEach`, `entries`, `keys`, `values`, but not `map`/`filter`: use `Array.from(list)` or `[...list]`.

## Reading content

| Property | Returns | Notes |
|----------|---------|-------|
| `textContent` | all text including hidden, no layout | fast, safe |
| `innerText` | rendered text (respects CSS, line breaks) | triggers layout, slower |
| `innerHTML` | markup string | **XSS risk** when setting with untrusted input |
| `outerHTML` | element markup including itself | |
| `value` | form control value | |

## From HTML to pixels (the rendering pipeline)

```text
HTML  ─► DOM ─┐
CSS   ─► CSSOM ┴─► render tree ─► layout (reflow) ─► paint ─► composite
```

| Stage | Triggered by | Cost |
|-------|--------------|------|
| **Style recalculation** | class/attribute/style changes | moderate |
| **Layout (reflow)** | geometry changes (size, position, text, font) | high, can cascade to descendants/ancestors |
| **Paint** | visual changes (color, shadow) | moderate |
| **Composite** | `transform`, `opacity` (GPU layers) | cheapest |

Reading layout properties (`offsetWidth`, `getBoundingClientRect()`, `scrollTop`) after writes can force a synchronous layout. See [Element Manipulation](./03_element-manipulation.md) for avoiding thrashing.

## Document lifecycle

| Event / state | When |
|---------------|------|
| `document.readyState === "loading"` | still parsing |
| `DOMContentLoaded` | HTML parsed and deferred/module scripts run; DOM ready (images may still load) |
| `"interactive"` / `"complete"` | parsing done / all resources loaded |
| `window.load` | everything loaded (images, styles, iframes) |
| `visibilitychange`, `pagehide` | tab hidden, page being unloaded |

```js
function ready(fn) {
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", fn, { once: true });
  else fn();
}
```

Script loading:

```html
<script src="a.js"></script>             <!-- blocks parsing -->
<script src="b.js" defer></script>       <!-- runs after parsing, in order -->
<script src="c.js" async></script>       <!-- runs when ready, any order -->
<script type="module" src="d.js"></script>   <!-- deferred by default -->
```

With `defer`/`module`, you can query the DOM directly without waiting.

## Document-level helpers

```js
document.title = "New title";
document.documentElement.lang;           // "en"
document.activeElement;                  // focused element
document.elementFromPoint(x, y);
document.createElement("div");
document.createTextNode("text");
document.createDocumentFragment();
document.head.append(styleOrMetaElement);
document.getSelection();
document.cookie;                         // see storage chapter
```

## Walking the tree

```js
function walk(node, visit) {
  visit(node);
  for (const child of node.children) walk(child, visit);
}

const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
while (walker.nextNode()) console.log(walker.currentNode.nodeValue);
```

## Shadow DOM boundary

Elements inside a **shadow root** (web components) are hidden from `document.querySelector` and global CSS. Reach them via `element.shadowRoot` (open mode). See [Web Components](./10_web-components.md).

## Browser DevTools

| Panel | Use |
|-------|-----|
| Elements | live DOM, edit nodes/attributes, break on subtree modifications |
| Console | `$0` (selected element), `$$("css")`, `inspect(el)`, `monitorEvents(el)` |
| Performance | layout/paint timings, layout shifts |
| Rendering | paint flashing, layout shift regions |

## DOM vs framework virtual DOMs

Frameworks (React, Vue, Svelte) manage the DOM for you and batch updates. Direct DOM code is perfect for small enhancements, widgets and libraries; understanding it explains what frameworks do under the hood. Do not mix manual DOM mutation with framework-managed nodes.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `childNodes`/`firstChild` expecting elements | Whitespace text nodes | `children`, `firstElementChild` |
| Looping a live collection while mutating | Skipped items, infinite loops | `Array.from` first |
| Running script before the element exists | `null` errors | `defer`, `type="module"`, or `DOMContentLoaded` |
| Treating `NodeList` as an array | No `map`/`filter` | `Array.from` / spread |
| `innerHTML` with user data | XSS | `textContent`, DOM APIs, sanitization |
| Reading layout after every write | Layout thrashing | Batch reads, then writes |
| Assuming `load` equals "DOM ready" | Waits for images | `DOMContentLoaded` |
| Querying inside shadow roots from the document | Not found | Query `shadowRoot` |

## Key takeaways

- The DOM is a tree of nodes; prefer element-specific traversal properties
- `querySelectorAll` is static; `getElementsBy*` and `children` are live
- Use `textContent` for text and avoid untrusted `innerHTML`
- Layout is expensive: batch DOM reads and writes
- Load scripts with `defer` or as modules so the DOM is ready

**Next:** [Selectors](./02_selectors.md)
