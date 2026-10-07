# Event System

When you write `<button onClick={handler}>`, React does **not** call `addEventListener("click", handler)` on that button. It has its own event system layered on top of the browser's, built on **event delegation**. Knowing how it works explains event order, `stopPropagation` quirks, why events bubble through portals, and why some handlers can't `preventDefault`.

> Based on React 17+ behavior. Older versions attached listeners to `document` and pooled event objects; those differences are noted where they matter.

## Delegation: one listener at the root

```tsx
createRoot(document.getElementById("root")!).render(<App />)
```

When you create a root, React attaches native listeners for supported event types **once, on the root container** (`#root`), for both the capture and bubble phases. Your handlers live on **fibers** as props, not on DOM nodes.

```text
user clicks <button>
   │
   ▼  native event bubbles up the DOM:  button → div → … → #root
                                                        │
                                                        ▼ React's single listener fires
                                       React finds the target fiber from the DOM node,
                                       walks UP the fiber tree collecting onClick handlers
                                       and runs them in order, creating a SyntheticEvent
```

Practical evidence: inspect a page in DevTools and the individual `<button>` has no click listener. The listener is on the root.

Why delegate: fewer listeners (cheap for large lists), handlers added and removed by updating a fiber prop rather than touching the DOM, and **React controls dispatch**, so it can set event priority for scheduling and keep behavior consistent across browsers.

## SyntheticEvent

Handlers receive a **SyntheticEvent**, a cross-browser wrapper around the native event with the same interface (`target`, `currentTarget`, `preventDefault()`, `stopPropagation()`, `key`, `clientX`, …). The native event is available as `e.nativeEvent`.

```tsx
function onClick(e: React.MouseEvent<HTMLButtonElement>) {
  e.preventDefault()
  console.log(e.currentTarget)     // the element the handler is attached to
  console.log(e.nativeEvent)       // the underlying DOM event
}
```

- **Event pooling is gone** (React 17+). In old code you needed `e.persist()` to use an event asynchronously. Now it's safe to read `e` after an `await`, and `persist()` is a no-op.
- `e.target` is where the event **started**; `e.currentTarget` is where the handler **is attached**. Mixing these up is a classic bug.
- TypeScript types the event and element: `React.ChangeEvent<HTMLInputElement>`, `React.FormEvent<HTMLFormElement>`, `React.KeyboardEvent` ([typing events](../04-typescript-with-react/01-typing-events.md)).

## Propagation follows the React tree

React simulates capture and bubble phases **over the fiber tree**, not the DOM tree:

```tsx
<div onClickCapture={() => log("1 capture")} onClick={() => log("4 bubble")}>
  <button onClickCapture={() => log("2 capture")} onClick={() => log("3 bubble")}>Go</button>
</div>
// click → 1 capture, 2 capture, 3 bubble, 4 bubble
```

- `onClick` runs in the **bubble** phase; `onClickCapture` in the **capture** phase.
- `e.stopPropagation()` stops React from calling handlers on *ancestor fibers*.

### Portals

Because propagation walks fibers, an event inside a [portal](../16-advanced-react/00-portals.md) **bubbles to its React parent**, even though the DOM nodes are elsewhere:

```tsx
<div onClick={() => log("parent")}>
  <Modal>{/* portaled to <body> */}<button>OK</button></Modal>
</div>
// clicking OK logs "parent"
```

Convenient for composition, but surprising for "click outside" logic. Stop propagation at the boundary when needed.

## Mixing React and native listeners

Since React handles events at the **root**, the ordering relative to your own native listeners follows from DOM bubbling:

```text
native listener on the button itself  → runs FIRST (event hasn't reached the root yet)
native listener on an ancestor inside #root → runs next
React's handlers (dispatched from the root listener) → run when the event reaches #root
native listener on #root (added after React's) → same node, registration order
native listener on document / window → runs LAST (after React)
```

- A native `addEventListener` on a specific element runs **before** that element's React `onClick`.
- **`e.stopPropagation()` in a React handler calls the native `stopPropagation`** on an event that is already at the root, so it prevents it from reaching `document` and `window` listeners (React 17+). It **cannot** stop native listeners that already ran lower in the tree, or other listeners on the root itself. For those, use `e.nativeEvent.stopImmediatePropagation()`.
- In React 16 and earlier, React listened on `document`, so `stopPropagation` couldn't block other `document` listeners. This changed in 17 so multiple React versions (or React and non-React code) can coexist on one page.

### Click-outside, correctly

Use a native listener and compare targets, rather than fighting React's propagation:

```tsx
useEffect(() => {
  function onPointerDown(e: PointerEvent) {
    if (ref.current && !ref.current.contains(e.target as Node)) onClose()
  }
  document.addEventListener("pointerdown", onPointerDown)
  return () => document.removeEventListener("pointerdown", onPointerDown)
}, [onClose])
```

A native `document` listener runs after React's handlers. With portaled content, `ref.current.contains(e.target)` is **false** for nodes in the portal (DOM-wise they're elsewhere), so check each relevant container or use a library primitive ([dropdowns and menus](../09-ui-components/03-dropdowns-and-menus.md)).

## Events that don't behave like their DOM namesakes

React normalizes or reimplements several events:

| React event | Behavior |
|---|---|
| **`onChange`** (inputs, textareas, selects) | Fires on **every change as the user types** (like native `input`), not only on blur like native `change` |
| **`onFocus` / `onBlur`** | **Bubble** (implemented via `focusin`/`focusout`), unlike native `focus`/`blur` |
| **`onMouseEnter` / `onMouseLeave`** | Don't bubble; derived from `mouseover`/`mouseout` |
| **`onScroll`** | **Doesn't bubble** (React 17+), matching browser behavior, so a child's scroll doesn't trigger a parent's `onScroll` |
| **`onSelect`, `onBeforeInput`, `onCompositionEnd`** | Normalized across browsers via React's own handling |
| **Non-delegable events** (`load`, `error` on media, `toggle`) | Attached directly to the elements rather than the root |

The `onChange` difference is why controlled inputs update per keystroke ([controlled and uncontrolled inputs](../06-forms/00-controlled-and-uncontrolled-inputs.md)).

## Passive listeners and `preventDefault`

Browsers treat `touchstart`, `touchmove`, and `wheel` listeners on the **root, `window`, and `document`** as **passive by default**, meaning the handler promises not to call `preventDefault()`, which lets scrolling stay smooth. React registers these events at the root, so:

```tsx
<div onWheel={(e) => e.preventDefault()} />   // ✗ doesn't prevent scrolling; browser logs a passive-listener warning
```

To prevent default scrolling for these, attach a native non-passive listener to the element itself:

```tsx
useEffect(() => {
  const el = ref.current!
  const handler = (e: WheelEvent) => e.preventDefault()
  el.addEventListener("wheel", handler, { passive: false })
  return () => el.removeEventListener("wheel", handler)
}, [])
```

(CSS often avoids the need: `overscroll-behavior` and `touch-action`.)

## Event priority → scheduling

React classifies events by urgency, and that classification is the **lane** for any state updates inside the handler ([03](./03-scheduler-and-lanes.md#how-an-update-gets-its-lane)):

- **Discrete** events (`click`, `keydown`, `input`, `submit`, `focusin`): Sync lane, flushed promptly at the end of the event.
- **Continuous** events (`scroll`, `mousemove`, `pointermove`, `drag`): input-continuous priority.
- Everything else (timers, network): default priority.

Updates in a handler are **batched**: all `setState` calls run to the end of the handler before React renders. Handlers see the state of the render they were created in (a closure over that render's snapshot), not the latest queued state ([hooks](./02-how-hooks-work.md#closures-why-values-go-stale)).

## What event handlers are *not*

- **Not part of rendering.** Handlers run outside the render phase, so they may have side effects, call APIs, and mutate refs.
- **Not covered by error boundaries.** An exception in a handler isn't a render error, so handle it with `try/catch` ([error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md#what-boundaries-catch-and-dont)).
- **Not awaited.** An `async` handler's returned promise is ignored by React, so a rejection becomes an unhandled promise rejection unless you catch it.
- **Not re-attached on each render.** Updating `onClick` replaces the handler stored on the fiber; no DOM listener churn.

## Testing

- `fireEvent.click(el)` dispatches a single native event, which bubbles to the React root as a real one would.
- `userEvent` (from Testing Library) simulates the *sequence* of real interactions (pointer down, focus, mouse up, click, key events), which exercises behaviors like focus and `onChange` per keystroke more faithfully ([component testing](../18-testing-and-debugging/02-component-testing-with-rtl.md)).
- Events dispatched on **detached** DOM nodes never reach the root listener, so they won't trigger React handlers.

## Common mistakes

- **Confusing `target` and `currentTarget`**, especially when children exist inside the element.
- **Expecting `stopPropagation` to stop native listeners** that already ran lower in the tree, or those on the root.
- **Click-outside handlers broken by portals**, because `contains` follows the DOM and React events follow the fiber tree.
- **`preventDefault()` in `onWheel`/`onTouchMove`** with no effect (passive listeners).
- **Expecting `onChange` to fire on blur** (it fires per keystroke) or `onFocus` not to bubble.
- **Forgetting events bubble through portals** to React ancestors.
- **Using `e.persist()` or worrying about pooled events** in modern React (no longer needed).
- **Throwing in handlers expecting an error boundary to catch it.**
- **Not catching rejections in `async` handlers.**
- **Using `onClick` on non-interactive elements** (`div`), losing keyboard and screen reader support. Use `<button>` or `<a>` ([accessibility](../08-accessibility/00-semantic-html.md)).
- **Stale state in handlers** from closures captured in an old render.

## Quick summary

- React uses **event delegation**: listeners are attached once at the **root container**, and handlers live as props on fibers.
- Handlers receive a **SyntheticEvent** (cross-browser wrapper; `nativeEvent` underneath), with no pooling in React 17+.
- Capture/bubble is simulated over the **fiber tree**, so events bubble through portals to React parents.
- Native listeners on elements below the root run **before** React handlers; `document`/`window` listeners run **after**, and `e.stopPropagation()` in React stops them from firing.
- Some events differ from the DOM: `onChange` fires per keystroke, `onFocus`/`onBlur` bubble, `onScroll` doesn't, and `wheel`/`touch` handlers are passive.
- Event type determines **update priority** ([lanes](./03-scheduler-and-lanes.md)); handler updates are batched.
- Handlers aren't covered by error boundaries and aren't part of render.

## Next

Continue to [18 — Testing and debugging](../18-testing-and-debugging/README.md).
