# Element Manipulation

Creating, inserting, removing and updating elements, attributes, classes and styles.

## Creating elements

```js
const card = document.createElement("article");
card.className = "card";
card.id = "post-1";
card.textContent = "Hello";

const text = document.createTextNode("plain text");
const clone = card.cloneNode(true);       // true = deep copy (children included); listeners are NOT copied
```

## Inserting

| Method | Position |
|--------|----------|
| `parent.append(...nodesOrStrings)` | after the last child |
| `parent.prepend(...)` | before the first child |
| `el.before(...)` / `el.after(...)` | siblings |
| `el.replaceWith(...)` | replace the element |
| `parent.replaceChildren(...nodes)` | replace all children (no args clears) |
| `parent.appendChild(node)` / `insertBefore(node, ref)` | older single-node versions |
| `el.insertAdjacentElement(pos, el2)` | `"beforebegin"`, `"afterbegin"`, `"beforeend"`, `"afterend"` |
| `el.insertAdjacentHTML(pos, html)` | parse HTML string (XSS risk) |

```js
const list = document.querySelector("ul");
const li = document.createElement("li");
li.textContent = "New item";
list.append(li);                         // strings are inserted as text, not HTML
list.prepend("plain text", li2);
list.replaceChildren();                  // clear
```

Inserting an **existing** node **moves** it.

## Removing

```js
el.remove();                             // modern
parent.removeChild(el);                  // older
el.replaceChildren();                    // empty the element (fast)
el.innerHTML = "";                       // also works, parses nothing
```

Remove listeners or use `AbortController` signals if you manually attached them to nodes you discard (nodes themselves become garbage once unreferenced).

## Text vs HTML

```js
el.textContent = userInput;              // SAFE: always text
el.innerHTML = userInput;                // DANGEROUS: parses markup, scripts in attributes (XSS)
```

```js
const name = `<img src=x onerror=alert(1)>`;
el.innerHTML = `<p>${name}</p>`;         // executes the handler!
el.textContent = name;                   // shows the literal text
```

Safe alternatives:

| Need | Approach |
|------|----------|
| Plain text | `textContent` |
| Structured markup from data | build with `createElement` / templates |
| Trusted static HTML | `innerHTML` is fine |
| Untrusted HTML | sanitize (DOMPurify) or the browser **Sanitizer API** (`el.setHTML(html)`, check support) |
| Enforce at scale | Content Security Policy, Trusted Types |

## `<template>` and `DocumentFragment`

```html
<template id="row-tpl">
  <tr><td class="name"></td><td class="age"></td></tr>
</template>
```

```js
const tpl = document.getElementById("row-tpl");
const fragment = document.createDocumentFragment();

for (const person of people) {
  const row = tpl.content.cloneNode(true);          // inert content, cloned
  row.querySelector(".name").textContent = person.name;
  row.querySelector(".age").textContent = person.age;
  fragment.append(row);
}
tbody.append(fragment);                              // one insertion, one reflow
```

A fragment is a lightweight, off-DOM container: its children move into the target on insertion.

## Attributes vs properties

| | Attribute | Property |
|---|-----------|----------|
| What | what is written in HTML (`getAttribute`) | the live JS object field |
| Values | strings | typed (boolean, number, object) |
| Example | `<input value="a">` initial value | `input.value` current value |

```js
el.getAttribute("href");  el.setAttribute("href", "/x");
el.hasAttribute("hidden");  el.removeAttribute("hidden");
el.toggleAttribute("disabled", true);
el.attributes;                           // NamedNodeMap

input.value = "typed";                   // property reflects what the user sees
input.getAttribute("value");             // still the original markup value
checkbox.checked = true;                 // property; the "checked" attribute is only the default
a.href;                                  // absolute URL (property) vs a.getAttribute("href") raw text
```

Prefer **properties** for state (`value`, `checked`, `disabled`, `hidden`, `selected`), **attributes** for custom data and ARIA.

## Classes

```js
el.classList.add("a", "b");
el.classList.remove("a");
el.classList.toggle("open");             // returns new state
el.classList.toggle("open", isOpen);     // force on/off
el.classList.contains("open");
el.classList.replace("old", "new");
el.className = "x y";                    // replaces all classes (avoid unless intended)
```

## Data attributes

```js
el.dataset.userId = "42";                // data-user-id="42"
delete el.dataset.userId;
Object.entries(el.dataset);
```

Store state in classes/attributes for styling, and in JS variables for logic.

## Styles

```js
el.style.color = "tomato";
el.style.backgroundColor = "#fff";       // camelCase
el.style.setProperty("--accent", "teal");// CSS custom properties
el.style.cssText = "color: red; margin: 0";
el.style.removeProperty("color");

getComputedStyle(el).fontSize;           // resolved value (read-only, triggers style calc)
getComputedStyle(el).getPropertyValue("--accent");
```

Prefer **toggling classes** over writing inline styles; use inline styles only for dynamic values (positions, custom properties).

```js
el.style.setProperty("--progress", `${percent}%`);   // CSS: width: var(--progress)
```

## Dimensions and position

| API | Gives |
|-----|-------|
| `el.getBoundingClientRect()` | size and position relative to the viewport (`x, y, width, height, top, left, ...`) |
| `offsetWidth/Height` | layout size including padding and border |
| `clientWidth/Height` | inner size (without border/scrollbar) |
| `scrollWidth/Height`, `scrollTop/Left` | scrollable size and offset |
| `window.innerWidth/innerHeight` | viewport size |
| `window.scrollY`, `scrollX` | page scroll offset |

```js
el.scrollIntoView({ behavior: "smooth", block: "center" });
window.scrollTo({ top: 0, behavior: "smooth" });
el.focus({ preventScroll: true });
```

## Layout thrashing

Interleaving reads of layout with writes forces repeated reflows.

```js
// slow: each iteration reads (forces layout) then writes (invalidates layout)
for (const el of items) {
  el.style.width = `${el.offsetWidth + 10}px`;
}

// fast: batch reads, then writes
const widths = items.map((el) => el.offsetWidth);
items.forEach((el, i) => { el.style.width = `${widths[i] + 10}px`; });
```

Other tips:

- Build detached trees/fragments and insert once
- Animate with `transform` and `opacity` (composite-only), not `top/left/width`
- Use `requestAnimationFrame` for visual updates
- Use `content-visibility`, `contain`, and virtualization for very long lists
- Avoid `innerHTML +=` in loops (reparses everything and drops listeners)

## Visibility and modern elements

```js
el.hidden = true;                        // hidden attribute (display: none by default)
el.inert = true;                         // not focusable/clickable, hidden from assistive tech
dialog.showModal(); dialog.close();      // <dialog> with built-in focus trap and backdrop
popoverEl.showPopover();                 // popover attribute API (modern browsers)
details.open = true;
```

## Animations

```js
el.animate(
  [{ opacity: 0, transform: "translateY(10px)" }, { opacity: 1, transform: "none" }],
  { duration: 200, easing: "ease-out", fill: "forwards" },
);

document.startViewTransition?.(() => updateDOM());    // View Transitions API (check support)
```

CSS transitions and the Web Animations API run off the main thread when possible.

## Cloning, moving and swapping

```js
const copy = el.cloneNode(true);         // listeners and internal state like JS properties are not cloned
a.replaceWith(b);                        // moves b
parent.insertBefore(newNode, parent.children[2]);
[a, b] = [b, a];                         // only swaps variables; to swap nodes: a.after(b)
```

## Example: render a list safely

```js
function renderTodos(container, todos) {
  const items = todos.map((todo) => {
    const li = document.createElement("li");
    li.dataset.id = todo.id;
    li.classList.toggle("done", todo.done);

    const label = document.createElement("span");
    label.textContent = todo.title;             // safe

    const del = document.createElement("button");
    del.type = "button";
    del.dataset.action = "delete";
    del.setAttribute("aria-label", `Delete ${todo.title}`);
    del.textContent = "×";

    li.append(label, del);
    return li;
  });
  container.replaceChildren(...items);
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `innerHTML` with untrusted data | XSS | `textContent`, DOM APIs, sanitizer |
| `innerHTML +=` | Reparses and destroys listeners/state | `append`, `insertAdjacentHTML` |
| Reading layout between writes | Layout thrashing | Batch reads then writes |
| Setting many inline styles | Hard to maintain, specificity issues | Classes and CSS variables |
| Confusing attribute and property | Stale values (`value`, `checked`) | Use properties for state |
| `el.className = ...` clobbering classes | Lost classes | `classList` |
| Appending nodes one by one into the live DOM | Many reflows | Fragment or `replaceChildren` |
| Cloning and expecting event listeners | They do not copy | Re-attach or use delegation |
| Animating layout properties | Jank | `transform`/`opacity` |
| Forgetting accessibility on dynamic UI | Unusable for screen readers | Roles, labels, focus management, live regions |

## Key takeaways

- Build elements with `createElement`, insert with `append`/`before`/`replaceChildren`
- Use `textContent`, never raw `innerHTML` with untrusted data
- Use properties for state, `classList` for classes, `dataset` for custom data
- Batch DOM writes (fragments, templates) and avoid mixing layout reads with writes

**Next:** [Events](./04_events.md)
