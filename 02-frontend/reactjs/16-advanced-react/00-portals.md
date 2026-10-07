# Portals

A **portal** lets a component render its children into a **different place in the DOM**, while staying in the same place in the **React tree**.

```tsx
import { createPortal } from "react-dom"

createPortal(children, domNode)
```

```text
React tree                         DOM
<App>                              <body>
 └─ <Card>                          ├─ <div id="root">
     └─ <Modal>  ── portal ──┐      │    └─ <div class="card"> … </div>
                             └────► └─ <div class="modal"> … </div>   ← rendered here
```

The modal is a child of `Card` in React (state, props, context all flow normally), but its DOM node lives under `<body>`.

## Why you need them

CSS makes some UI impossible to render *inside* its parent:

- **`overflow: hidden`** on an ancestor clips a dropdown or tooltip that extends beyond it.
- **Stacking contexts** trap `z-index`. A modal with `z-index: 9999` inside a parent that created its own stacking context still renders *behind* a sibling with a higher-level context. (Anything with `position` + `z-index`, `transform`, `opacity < 1`, `filter`, and others creates one.)
- **`transform`/`filter` on an ancestor** turns it into the containing block for `position: fixed` children, so "fixed" stops meaning "relative to the viewport".

Rendering at the **end of `<body>`** sidesteps all of that. Typical uses: modals and dialogs, dropdowns and popovers, tooltips, toasts, and context menus. This is why [Radix and shadcn/ui](../09-ui-components/02-dialogs-and-modals.md#what-actually-happens-when-it-opens) render dialog content through a portal.

## A minimal modal

```tsx
import { useEffect } from "react"
import { createPortal } from "react-dom"

function Modal({ open, onClose, title, children }: ModalProps) {
  useEffect(() => {
    if (!open) return
    const onKeyDown = (e: KeyboardEvent) => { if (e.key === "Escape") onClose() }
    document.addEventListener("keydown", onKeyDown)
    return () => document.removeEventListener("keydown", onKeyDown)
  }, [open, onClose])

  if (!open) return null

  return createPortal(
    <div className="fixed inset-0 grid place-items-center bg-black/50" onClick={onClose}>
      <div
        role="dialog"
        aria-modal="true"
        aria-label={title}
        className="rounded bg-background p-6"
        onClick={(e) => e.stopPropagation()}
      >
        {children}
      </div>
    </div>,
    document.body
  )
}
```

This is **deliberately incomplete**. A production modal also needs to trap focus, restore focus on close, lock page scroll, and make the rest of the page inert for assistive tech ([keyboard and focus management](../08-accessibility/02-keyboard-and-focus-management.md)). That's why you should use a tested primitive (Radix `Dialog`, the native `<dialog>`) instead of hand-rolling one. Learn the portal mechanism here; don't ship this component.

## What portals preserve (and what they don't)

A portal changes **where DOM nodes go**, not how React treats the tree.

| | Behavior through a portal |
|---|---|
| **State and props** | Work normally |
| **Context** | **Preserved**: the portal content reads the same providers as its React parent |
| **Events** | **Bubble through the React tree**, not the DOM tree |
| **Refs, effects** | Work normally |
| **CSS inheritance** | Follows the **DOM** tree: inherited styles come from the portal's DOM parent (`<body>`), not the React parent |
| **CSS selectors** (`.card .modal`) | Match the DOM structure, so descendant selectors from the React parent **won't** apply |

### Events bubble through React

```tsx
function Parent() {
  return (
    <div onClick={() => console.log("Parent clicked")}>
      <Modal open onClose={close}>
        <button>Click me</button>        {/* DOM-wise it's under <body>, not under this div */}
      </Modal>
    </div>
  )
}
```

Clicking the button **does** log "Parent clicked", because React bubbles synthetic events up the *component* tree. That's usually convenient, but it surprises people when a click inside a modal triggers a handler on a distant ancestor (a clickable card, a "click outside" detector). Call `e.stopPropagation()` at the portal boundary when needed, as the modal above does for its backdrop.

### CSS and theming

Because inheritance follows the DOM, anything inherited from a wrapper (a `dark` class on `<div id="root">`, a font set on the app container, CSS variables defined on that element) **doesn't reach** portaled content that lives under `<body>`. Fixes:

- Put theme classes and CSS variables on **`<html>` or `<body>`**, which is how Tailwind's `dark` class and [theming](../07-styling/04-theming-and-dark-mode.md) normally work.
- Or portal into a container **inside** the themed element (`createPortal(…, themedRootRef.current)`).
- Tailwind utilities on the portaled elements themselves always work, since they're classes on the node, not selectors from an ancestor.

## Choosing the container

```tsx
createPortal(node, document.body)                         // simplest
createPortal(node, document.getElementById("portal-root")!) // a dedicated element in index.html
createPortal(node, someRef.current)                        // inside a specific element
```

A dedicated `<div id="portal-root">` next to `#root` keeps portaled UI separate and gives you one place to control stacking and theming. Appending directly to `<body>` is fine for most apps, and libraries often create their own container.

If you pass a container that isn't in the DOM yet (a ref's `.current` before mount), `createPortal` throws. Render the portal only after the container exists.

## Portals and server rendering

`document` doesn't exist on the server, so `createPortal(…, document.body)` during server rendering crashes. And portals aren't part of the server-rendered HTML, so they would mismatch on hydration if rendered on the first client render. The usual pattern is to render the portal **after mount**:

```tsx
function ClientOnlyPortal({ children }: { children: React.ReactNode }) {
  const [mounted, setMounted] = useState(false)
  useEffect(() => setMounted(true), [])
  return mounted ? createPortal(children, document.body) : null
}
```

Overlay libraries handle this for you. See [hydration](../15-concurrent-and-modern-react/07-server-components-and-ssr.md#hydration-mismatches). In a client-only SPA it's not a concern.

## Native alternatives

Browsers now provide the "render above everything" behavior natively, with focus and accessibility handled:

- **`<dialog>`** with `showModal()` renders in the browser's **top layer**, above all stacking contexts, traps focus, handles `Esc`, and makes the rest of the page inert. Often **no portal needed**:

```tsx
function NativeModal({ open, onClose, children }: Props) {
  const ref = useRef<HTMLDialogElement>(null)

  useEffect(() => {
    const dialog = ref.current
    if (!dialog) return
    if (open && !dialog.open) dialog.showModal()
    if (!open && dialog.open) dialog.close()
  }, [open])

  return <dialog ref={ref} onClose={onClose}>{children}</dialog>
}
```

- **The Popover API** (`popover` attribute) puts popovers, menus, and tooltips in the top layer too.

Check browser support for your targets. Even with native elements, libraries like Radix remain popular for richer behavior (positioning, typeahead, animation).

## Testing

Portaled content is in `document.body`, not inside the `container` returned by `render()`. Testing Library's `screen` queries search the whole document, so prefer `screen.getByRole("dialog")` over queries scoped to the container ([component testing](../18-testing-and-debugging/02-component-testing-with-rtl.md)).

## Common mistakes

- **Hand-rolling a modal** and skipping focus trap, focus restore, scroll lock, and inert background.
- **Forgetting events bubble through React**, so a click in the portal triggers an ancestor's handler.
- **Theme/styles not applied** because inheritance comes from the DOM parent (`<body>`), not the React parent.
- **Descendant CSS selectors** from the React parent not matching.
- **`document.body` during SSR**, causing a crash or hydration mismatch.
- **Portaling into a container that doesn't exist yet.**
- **Using a portal when `overflow`/`z-index` weren't the problem**, adding complexity for nothing.
- **Leaving portal content mounted** when closed, which leaks elements and handlers.
- **Z-index wars** between multiple portaled layers (dialog, toast, popover). Define the stacking order once.
- **Querying inside the render container in tests.**

## Quick summary

- `createPortal(children, domNode)` renders children elsewhere in the DOM while keeping their place in the React tree.
- Use it to escape `overflow: hidden`, stacking contexts, and `transform`ed ancestors: modals, popovers, tooltips, toasts.
- Context, state, and **events** follow the **React** tree; CSS inheritance and selectors follow the **DOM** tree.
- Put theme classes/variables on `<html>`/`<body>`, and stop propagation at the portal boundary if needed.
- Render portals after mount for SSR.
- Prefer a tested primitive (Radix, native `<dialog>`/Popover) over writing your own overlay.

## Next

[01 — Refs and imperative handles](./01-refs-and-imperative-handles.md)
