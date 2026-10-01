# Event Delegation

**Event delegation** attaches **one listener to a common ancestor** instead of one listener per child. Because most events **bubble**, the ancestor hears about events from any descendant and uses `event.target` to find which one was involved.

```
ul  ◄── one listener here
├─ li  (click)
├─ li
└─ li   (added later, still works)
```

## Basic example

```html
<ul id="todos">
  <li data-id="1">Buy milk <button data-action="delete">×</button></li>
  <li data-id="2">Walk dog <button data-action="delete">×</button></li>
</ul>
```

```js
const list = document.getElementById("todos");

list.addEventListener("click", (event) => {
  const button = event.target.closest("[data-action='delete']");
  if (!button || !list.contains(button)) return;       // ignore clicks elsewhere

  const item = button.closest("li");
  deleteTodo(item.dataset.id);
  item.remove();
});
```

New `<li>` elements added later work automatically, with no re-binding.

## Why delegate

| Benefit | Explanation |
|---------|-------------|
| **Fewer listeners** | one per container, not one per row: less memory and setup time |
| **Dynamic content** | elements added or replaced later are covered |
| **Simpler cleanup** | one `removeEventListener` / abort signal |
| **Consistency** | behavior defined in one place |

When **not** to delegate: events that do not bubble, handlers needing per-element capture phase logic, or a single element.

## `target` vs `currentTarget`

```js
list.addEventListener("click", (event) => {
  event.target;          // the deepest element that was clicked (could be a <span> inside the <button>)
  event.currentTarget;   // always `list`
});
```

Clicks often land on **child elements** (an icon inside a button). Use `closest()` to climb to the element you care about instead of comparing `target` directly:

```js
// fragile
if (event.target.tagName === "BUTTON") { ... }              // fails when clicking <svg> or <span> inside the button
if (event.target.classList.contains("btn")) { ... }

// robust
const btn = event.target.closest("button.btn");
if (btn && list.contains(btn)) { ... }
```

`list.contains(btn)` guards against matching an ancestor **outside** your container.

## Action routing pattern

Use `data-action` attributes and a handler map.

```html
<div id="toolbar">
  <button data-action="bold">B</button>
  <button data-action="italic">I</button>
  <button data-action="link">Link</button>
</div>
```

```js
const actions = {
  bold: () => format("bold"),
  italic: () => format("italic"),
  link: () => insertLink(),
};

toolbar.addEventListener("click", (event) => {
  const el = event.target.closest("[data-action]");
  if (!el || !toolbar.contains(el)) return;
  actions[el.dataset.action]?.(event, el);
});
```

Prefer `Object.hasOwn(actions, name)` when the action name could come from untrusted data (avoids `constructor`, `toString`).

## A reusable `delegate` helper

```js
function delegate(root, type, selector, handler, options) {
  const listener = (event) => {
    const match = event.target.closest(selector);
    if (match && root.contains(match)) handler.call(match, event, match);
  };
  root.addEventListener(type, listener, options);
  return () => root.removeEventListener(type, listener, options);   // unsubscribe function
}

const off = delegate(document, "click", "a[data-track]", (event, link) => track(link.dataset.track));
// later: off();
```

## Delegation for non-bubbling events

| Event | Alternative that bubbles |
|-------|--------------------------|
| `focus` / `blur` | `focusin` / `focusout` |
| `mouseenter` / `mouseleave` | `mouseover` / `mouseout` (check `relatedTarget`) |
| `load`, `error` on images | capture phase listener: `addEventListener("error", fn, true)` |
| `scroll` on elements | capture phase |

```js
form.addEventListener("focusin", (e) => e.target.closest(".field")?.classList.add("focused"));
form.addEventListener("focusout", (e) => e.target.closest(".field")?.classList.remove("focused"));

document.addEventListener("error", (e) => {
  if (e.target instanceof HTMLImageElement) e.target.src = "/fallback.png";
}, true);
```

## Input and change delegation

```js
form.addEventListener("input", (event) => {
  const input = event.target;
  if (input.matches("input[type='number']")) updateTotal();
});

form.addEventListener("change", (event) => {
  if (event.target.matches("select[name='country']")) loadRegions(event.target.value);
});
```

## Tabs example

```html
<div class="tabs" role="tablist">
  <button role="tab" data-tab="a" aria-selected="true">A</button>
  <button role="tab" data-tab="b" aria-selected="false">B</button>
</div>
<section id="panel-a"></section>
<section id="panel-b" hidden></section>
```

```js
tabs.addEventListener("click", (event) => {
  const tab = event.target.closest("[role='tab']");
  if (!tab || !tabs.contains(tab)) return;

  tabs.querySelectorAll("[role='tab']").forEach((t) => t.setAttribute("aria-selected", String(t === tab)));
  document.querySelectorAll("section[id^='panel-']").forEach((p) => { p.hidden = p.id !== `panel-${tab.dataset.tab}`; });
});
```

## Dropdowns and outside clicks

```js
document.addEventListener("click", (event) => {
  document.querySelectorAll(".dropdown.open").forEach((dd) => {
    if (!dd.contains(event.target)) dd.classList.remove("open");      // click outside closes
  });
});
```

Prefer `composedPath().includes(el)` when the click may come from inside a shadow root or an element removed during the same event:

```js
if (!event.composedPath().includes(dropdown)) close();
```

## Where to attach the listener

| Location | Notes |
|----------|-------|
| The closest stable container (`ul`, `table`, `form`) | best: narrow scope, easy cleanup |
| `document` / `body` | fine for global actions (`data-action` routing, analytics), but every click passes through it |
| `window` | for global keyboard/resize/etc. |

## Keyboard accessibility

Delegation works with `keydown` too; ensure interactive items are **focusable** and operable by keyboard:

```js
list.addEventListener("keydown", (event) => {
  const item = event.target.closest("[role='option']");
  if (!item) return;
  if (event.key === "Enter" || event.key === " ") { event.preventDefault(); select(item); }
  if (event.key === "ArrowDown") { event.preventDefault(); item.nextElementSibling?.focus(); }
});
```

Use real `<button>` and `<a>` elements whenever possible: they give you keyboard and screen reader behavior for free.

## Shadow DOM and delegation

Events crossing a shadow boundary are **retargeted**: outside listeners see the **host** as `event.target`. Use `event.composedPath()[0]` for the real origin, and note that non-`composed` events do not leave the shadow root.

## Performance considerations

- Handler runs for **every** matching event bubbling through the container: keep the first check cheap (`closest` + early return)
- Delegation on thousands of rows is far cheaper than thousands of listeners
- Frequently firing events (`mousemove`, `scroll`) should still be throttled

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Comparing `event.target` directly to a selector | Child elements (icons/spans) break it | `event.target.closest(selector)` |
| Not checking `container.contains(match)` | Matches elements outside the container | Add the contains check |
| `stopPropagation()` on descendants | Delegated handler never fires | Avoid, or use capture phase |
| Delegating non-bubbling events without alternatives | Never fires | `focusin`/`focusout`, capture |
| Using `mouseover` for hover effects without checking `relatedTarget` | Flicker/repeated triggers | Compare `relatedTarget`, or `mouseenter` on elements |
| Using `data-action` names as object keys unchecked | `__proto__`, `constructor` lookups | `Object.hasOwn`, `Map` |
| One giant handler for everything on `document` | Hard to maintain | Per-feature containers and small handlers |
| Assuming `target` is always an Element | Text nodes in some older cases, `window` for some events | Guard with `instanceof Element` |
| Forgetting accessibility | Mouse-only UIs | Semantic elements and keyboard handling |

## Key takeaways

- Attach one listener to a stable ancestor and use `event.target.closest(selector)` to find the target
- Delegation handles dynamically added elements and saves memory
- Verify the match is inside the container (`contains`)
- For non-bubbling events use `focusin`/`focusout` or capture
- Route actions with `data-action` attributes and keep handlers small

**Next:** [Forms](./06_forms.md)
