# Selectors

Finding elements is the first step of any DOM work. Modern code mostly uses **CSS selectors**.

## The main methods

| Method | Returns | Live? | Notes |
|--------|---------|-------|-------|
| `document.getElementById("id")` | element or `null` | n/a | fastest, id must be unique |
| `document.querySelector(css)` | first match or `null` | n/a | any CSS selector |
| `document.querySelectorAll(css)` | `NodeList` | static | all matches (possibly empty) |
| `getElementsByClassName("a b")` | `HTMLCollection` | live | |
| `getElementsByTagName("div")` | `HTMLCollection` | live | |
| `getElementsByName("name")` | `NodeList` | live | form controls by `name` |
| `element.closest(css)` | nearest ancestor **or itself**, or `null` | n/a | great for delegation |
| `element.matches(css)` | boolean | n/a | test an element against a selector |

```js
const title = document.querySelector("h1.title");
const items = document.querySelectorAll("ul.menu > li");
const form = document.getElementById("signup");
```

`querySelector` and `querySelectorAll` also exist on **elements** and **document fragments**, searching their descendants:

```js
const list = document.querySelector("#todos");
const done = list.querySelectorAll(".done");       // only inside #todos
```

## Selector cheat sheet

| Selector | Matches |
|----------|---------|
| `div`, `.card`, `#main` | tag, class, id |
| `a, b` | either |
| `ul li` | descendant |
| `ul > li` | direct child |
| `h2 + p` | immediately following sibling |
| `h2 ~ p` | any following sibling |
| `[data-id]` | has attribute |
| `[type="text"]`, `[href^="https"]`, `[href$=".pdf"]`, `[class*="btn"]` | exact, starts with, ends with, contains |
| `[lang|="en"]`, `[class~="x"]` | dash-prefixed, word in list |
| `:first-child`, `:last-child`, `:nth-child(2n+1)`, `:nth-of-type(2)` | structural |
| `:not(.x)`, `:is(h1, h2)`, `:where(...)` | logical |
| `:has(> img)` | parent selector (modern browsers) |
| `:checked`, `:disabled`, `:enabled`, `:required`, `:invalid`, `:valid` | form state |
| `:focus`, `:focus-visible`, `:hover` | interaction state |
| `::before`, `::after` | pseudo-elements (not selectable via `querySelector`) |
| `:scope > li` | relative to the element you query from |

```js
document.querySelectorAll("a[href^='http']:not([href*='mysite.com'])");   // external links
document.querySelectorAll("input:checked");
document.querySelector("article:has(h2) .byline");
list.querySelectorAll(":scope > li");                                     // direct children only
```

## Working with results

```js
const el = document.querySelector(".missing");     // null if not found
el?.classList.add("x");                             // guard

const nodes = document.querySelectorAll(".item");
nodes.length;
nodes.forEach((n) => n.classList.add("seen"));      // NodeList has forEach
const arr = Array.from(nodes);                      // or [...nodes]
arr.filter((n) => n.dataset.active === "true").map((n) => n.id);

for (const n of nodes) { /* iterable */ }
```

`querySelector` never throws for "not found", but **throws** `SyntaxError` for invalid selectors.

## Escaping special characters

```js
const id = "user:42";
document.querySelector(`#${CSS.escape(id)}`);              // "#user\:42"
document.querySelector(`[data-id="${CSS.escape(id)}"]`);
document.getElementById(id);                               // no escaping needed with getElementById
```

Always escape values that come from users or data.

## `closest` and `matches`

```js
document.addEventListener("click", (event) => {
  const button = event.target.closest("button[data-action]");
  if (!button) return;                          // click was not inside such a button
  console.log(button.dataset.action);
});

element.matches(".card.active");                // boolean
```

`closest` starts with the element itself and walks up.

## Data attributes

```html
<li data-id="42" data-user-name="Ada">Ada</li>
```

```js
document.querySelector("[data-id='42']");
const li = document.querySelector("li");
li.dataset.id;            // "42"
li.dataset.userName;      // "Ada"  (kebab-case becomes camelCase)
li.dataset.role = "admin";// sets data-role="admin"
```

## Forms and special collections

```js
document.forms.signup;                     // <form name="signup"> or id
document.forms[0].elements.email;          // control named "email"
form.elements["email"];
document.images, document.links, document.scripts;
document.body, document.head, document.documentElement;
```

## Shadow DOM and iframes

Selectors **do not cross** shadow boundaries or iframes:

```js
const host = document.querySelector("my-widget");
host.shadowRoot.querySelector(".inner");                  // open shadow root
iframe.contentDocument?.querySelector("h1");              // same-origin only
```

## Performance

- `getElementById` is the fastest lookup, but `querySelector` is fast enough for almost everything
- Narrow the search scope: `container.querySelector` rather than `document.querySelector`
- **Cache** results you use repeatedly
- Avoid querying inside tight loops; query once, then work with the nodes
- Overly complex selectors are rarely the bottleneck: layout and rendering are

```js
const rows = table.querySelectorAll("tbody tr");      // once
rows.forEach((row) => { /* work with row */ });
```

## Practical helpers

```js
const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];

const closestData = (el, name) => el.closest(`[data-${name}]`)?.dataset[name];

function requireEl(selector, root = document) {
  const el = root.querySelector(selector);
  if (!el) throw new Error(`Missing element: ${selector}`);
  return el;
}
```

DevTools console already provides `$` and `$$` (query shortcuts) and `$0` (selected node).

## Waiting for elements

```js
function waitFor(selector, { root = document, timeout = 5000 } = {}) {
  return new Promise((resolve, reject) => {
    const found = root.querySelector(selector);
    if (found) return resolve(found);
    const observer = new MutationObserver(() => {
      const el = root.querySelector(selector);
      if (el) { observer.disconnect(); clearTimeout(timer); resolve(el); }
    });
    observer.observe(root, { childList: true, subtree: true });
    const timer = setTimeout(() => { observer.disconnect(); reject(new Error(`Timeout waiting for ${selector}`)); }, timeout);
  });
}
```

## Selecting by text

CSS cannot match text content. Filter in JavaScript:

```js
const button = [...document.querySelectorAll("button")].find((b) => b.textContent.trim() === "Save");
```

(XPath `document.evaluate` can match text, but is rarely needed.)

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Not handling `null` from `querySelector` | `TypeError` | Guard or `?.` |
| Assuming `querySelectorAll` is live | Stale after DOM changes | Query again when needed |
| Unescaped dynamic values in selectors | Wrong matches or `SyntaxError` | `CSS.escape` |
| Duplicate IDs | `getElementById` returns the first only | Unique IDs, use classes for groups |
| Selecting by fragile structure (`div > div > div`) | Breaks on markup changes | `data-*` attributes or semantic classes |
| Using classes meant for styling as JS hooks | Style refactors break behavior | `data-js-*` or `data-action` hooks |
| Querying before the element exists | `null` | `defer`/module scripts, `DOMContentLoaded` |
| Expecting selectors to pierce shadow DOM | Not found | Query the `shadowRoot` |
| Calling `Array` methods on `NodeList` | `TypeError` | `Array.from` |

## Key takeaways

- `querySelector`/`querySelectorAll` handle nearly everything; `getElementById` is the fastest
- `closest` and `matches` power delegation and context checks
- Results can be `null`; `querySelectorAll` is a static `NodeList`
- Escape dynamic values with `CSS.escape` and prefer `data-*` hooks

**Next:** [Element Manipulation](./03_element-manipulation.md)
