# Events

An **event** is a signal that something happened: a click, a key press, a finished load, a custom message. You register a **listener** (callback), and the browser calls it with an **event object**.

```js
button.addEventListener("click", (event) => {
  console.log("clicked", event.target);
});
```

## Registering listeners

```js
target.addEventListener(type, listener, options);
target.removeEventListener(type, listener, options);   // must pass the SAME function reference
```

| Option | Meaning |
|--------|---------|
| `capture: true` | listen during the capture phase instead of bubbling |
| `once: true` | run once, then remove automatically |
| `passive: true` | promise not to call `preventDefault()` (lets scrolling stay smooth) |
| `signal: abortSignal` | remove the listener when the signal aborts |

```js
const ac = new AbortController();
window.addEventListener("resize", onResize, { signal: ac.signal });
window.addEventListener("keydown", onKey, { signal: ac.signal });
ac.abort();                                            // removes both

button.addEventListener("click", handler, { once: true });
```

Other ways to attach handlers:

```html
<button onclick="doThing()">Inline attribute</button>   <!-- avoid: mixes markup and logic, CSP-unfriendly -->
```

```js
button.onclick = () => {};            // single handler slot, overwritten by the next assignment
```

Prefer `addEventListener`: multiple listeners, options, easy cleanup.

A listener can also be an **object** with a `handleEvent` method (useful for classes, keeps `this`):

```js
class Menu {
  constructor(el) { el.addEventListener("click", this); }
  handleEvent(event) { if (event.type === "click") this.onClick(event); }
  onClick() {}
}
```

## Event flow: capture, target, bubble

```
window → document → html → body → section → button   (capture, top down)
                                          ▲ target
window ← document ← html ← body ← section ← button   (bubble, bottom up)
```

1. **Capture phase**: from `window` down to the target's parent
2. **Target phase**: on the element itself
3. **Bubble phase**: back up to `window`

Most listeners run in the **bubble** phase (default). Most events bubble; some do not (see below).

```js
outer.addEventListener("click", () => console.log("outer capture"), { capture: true });
inner.addEventListener("click", () => console.log("inner"));
outer.addEventListener("click", () => console.log("outer bubble"));
// click inner: outer capture, inner, outer bubble
```

## The event object

| Property / method | Meaning |
|-------------------|---------|
| `type` | event name (`"click"`) |
| `target` | the element where the event **originated** |
| `currentTarget` | the element whose listener is running now (`this` for non-arrow handlers) |
| `eventPhase` | 1 capture, 2 target, 3 bubble |
| `bubbles`, `cancelable` | whether it bubbles / can be canceled |
| `preventDefault()` | stop the browser's default action (follow link, submit form) |
| `stopPropagation()` | stop moving to other nodes along the path |
| `stopImmediatePropagation()` | also stop other listeners on the same node |
| `defaultPrevented` | whether `preventDefault` was called |
| `composedPath()` | array of nodes the event passes through (works with shadow DOM) |
| `isTrusted` | `true` for real user/browser events, `false` for script-dispatched |
| `timeStamp` | high-resolution time |

```js
link.addEventListener("click", (e) => {
  e.preventDefault();                     // do not navigate
  loadInPlace(e.currentTarget.href);
});
```

`stopPropagation` is rarely the right fix: it breaks analytics, delegation and other code. Prefer checking `event.target`.

## Common events

| Category | Events |
|----------|--------|
| Mouse | `click`, `dblclick`, `contextmenu`, `mousedown`, `mouseup`, `mousemove`, `mouseenter`/`mouseleave` (no bubble), `mouseover`/`mouseout` (bubble), `wheel` |
| Pointer (unified mouse/touch/pen) | `pointerdown`, `pointermove`, `pointerup`, `pointercancel`, `pointerenter`, `pointerleave`, `gotpointercapture` |
| Touch | `touchstart`, `touchmove`, `touchend` (prefer pointer events) |
| Keyboard | `keydown`, `keyup` (`keypress` is deprecated) |
| Focus | `focus`/`blur` (no bubble), `focusin`/`focusout` (bubble) |
| Form | `input`, `change`, `submit`, `reset`, `invalid`, `select` |
| Drag and drop | `dragstart`, `dragover`, `drop`, `dragend` |
| Document / window | `DOMContentLoaded`, `load`, `beforeunload`, `pagehide`, `pageshow`, `visibilitychange`, `resize`, `scroll`, `hashchange`, `popstate`, `online`, `offline` |
| Media | `play`, `pause`, `ended`, `timeupdate`, `loadedmetadata` |
| Resources | `load`, `error` on `img`, `script`, `link` |
| Clipboard | `copy`, `cut`, `paste` |
| Animation | `animationend`, `transitionend` |
| Messaging | `message` (postMessage, workers, BroadcastChannel), `storage` |

## Keyboard events

```js
document.addEventListener("keydown", (e) => {
  e.key;            // "a", "A", "Enter", "Escape", "ArrowLeft" (the character/meaning)
  e.code;           // "KeyA", "Enter", "ArrowLeft" (physical key, layout independent)
  e.ctrlKey; e.shiftKey; e.altKey; e.metaKey;
  e.repeat;         // true while held down
  e.isComposing;    // IME composition in progress (ignore shortcuts)

  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
    e.preventDefault();
    save();
  }
});
```

Use `key` for text/meaning, `code` for game controls (WASD positions).

## Pointer and mouse

```js
canvas.addEventListener("pointerdown", (e) => {
  canvas.setPointerCapture(e.pointerId);                 // keep receiving moves even outside
  start(e.clientX, e.clientY);
});
canvas.addEventListener("pointermove", (e) => draw(e.offsetX, e.offsetY));
canvas.addEventListener("pointerup", end);
```

| Coordinates | Relative to |
|-------------|-------------|
| `clientX/Y` | viewport |
| `pageX/Y` | document (includes scroll) |
| `screenX/Y` | screen |
| `offsetX/Y` | target element's padding edge |

Set `touch-action: none` in CSS on elements that handle their own touch gestures.

## Events that do not bubble

`focus`, `blur`, `mouseenter`, `mouseleave`, `load`, `unload`, `scroll` (on elements), `error` (on resources), `pointerenter/leave`.

Alternatives for delegation: `focusin`/`focusout`, `mouseover`/`mouseout`, or capture-phase listeners (`{ capture: true }`).

## Passive listeners and scrolling

Touch and wheel listeners that might `preventDefault` make the browser **wait** before scrolling. Declare them passive when you do not need to cancel:

```js
window.addEventListener("scroll", onScroll, { passive: true });
window.addEventListener("touchstart", onTouch, { passive: true });
```

`wheel`, `touchstart`, `touchmove` are passive by default on `window`, `document`, `body` in modern browsers.

## `this` in handlers

```js
button.addEventListener("click", function () { this === button; });      // currentTarget
button.addEventListener("click", () => { /* this is the outer scope */ });
button.addEventListener("click", obj.method.bind(obj));                  // keep object context
```

Class methods need an arrow field or `bind` (see `05_this-and-oop/01_this.md`).

## Custom events

```js
const event = new CustomEvent("cart:add", {
  detail: { id: 42, qty: 2 },
  bubbles: true,
  composed: true,                         // crosses shadow DOM boundaries
  cancelable: true,
});

element.dispatchEvent(event);              // returns false if a listener called preventDefault

document.addEventListener("cart:add", (e) => console.log(e.detail.id));
```

Events can be used as a decoupled messaging system between components. `EventTarget` can be extended or instantiated for non-DOM objects:

```js
class Store extends EventTarget {
  set(value) { this.value = value; this.dispatchEvent(new CustomEvent("change", { detail: value })); }
}
const store = new Store();
store.addEventListener("change", (e) => render(e.detail));
```

## Programmatic and synthetic events

```js
button.click();                            // fires a click (untrusted)
input.dispatchEvent(new Event("input", { bubbles: true }));
```

Untrusted events cannot trigger certain actions (opening popups, clipboard access in many cases).

## Page lifecycle events

| Event | Use |
|-------|-----|
| `DOMContentLoaded` | DOM ready |
| `visibilitychange` | tab hidden/visible: pause work, **send analytics** (`navigator.sendBeacon`) |
| `pagehide` | page is being unloaded or entering the back/forward cache: preferred over `unload` |
| `beforeunload` | prompt on unsaved changes only (`e.preventDefault()`); avoid for anything else |
| `unload` | deprecated; breaks bfcache |

```js
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") navigator.sendBeacon("/analytics", JSON.stringify(data));
});
```

## Event timing and the loop

Each user event runs as a task; microtasks drain between listeners for real user events but not for script-dispatched ones (see `12_event-loop/04_task-ordering.md`). Long handlers delay the next paint and hurt INP: keep them short and defer heavy work.

## Debouncing and throttling

```js
input.addEventListener("input", debounce(onSearch, 300));
window.addEventListener("resize", throttle(onResize, 100));
window.addEventListener("scroll", () => requestAnimationFrame(update), { passive: true });
```

## Cleaning up

Listeners keep their closures alive. Remove them when elements or components go away:

- `removeEventListener` with the same function reference (anonymous functions cannot be removed)
- `{ signal }` with an `AbortController` (best for groups)
- Listeners on `window`, `document`, timers and observers are the usual leak sources
- Listeners on an element are collected with the element, unless something else references the element

## Debugging

DevTools: Elements → **Event Listeners** panel; Console `getEventListeners(el)` (Chrome only), `monitorEvents(el, "click")`; Sources → **Event Listener Breakpoints**.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Inline `onclick="..."` | CSP issues, global functions | `addEventListener` |
| Removing an anonymous listener | Cannot (different reference) | Named function or `signal` |
| Using `target` instead of `currentTarget` | Wrong element in nested markup | `currentTarget` / `closest` |
| Overusing `stopPropagation` | Breaks other listeners and analytics | Check `event.target` |
| Heavy work in `scroll`/`mousemove` | Jank | Throttle, rAF, passive |
| Using `keyCode` / `keypress` | Deprecated | `key` / `code` with `keydown` |
| `unload`/`beforeunload` for saving | Unreliable, blocks bfcache | `pagehide`, `visibilitychange`, `sendBeacon` |
| Ignoring IME (`isComposing`) | Shortcuts fire during text composition | Check `isComposing` |
| Not passing `bubbles: true` to custom events | Delegated listeners never hear it | Set `bubbles` (and `composed` for shadow DOM) |
| Leaking `window`/`document` listeners | Memory and ghost handlers | `AbortController` cleanup |
| Assuming `mouse*` events cover touch | Misses touch users | Pointer events |

## Key takeaways

- Use `addEventListener` with options (`once`, `passive`, `signal`) and keep references for removal
- Events capture down, hit the target, then bubble up; `target` is the origin, `currentTarget` the listener's element
- `preventDefault` cancels default behavior; avoid `stopPropagation` unless necessary
- Use pointer events, `key`/`code`, `CustomEvent` for messaging, and clean up listeners

**Next:** [Event Delegation](./05_event-delegation.md)
