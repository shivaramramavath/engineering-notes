# Refs and Imperative Handles

React is **declarative**: you describe the UI for a given state, and React updates the DOM. But some things are inherently *imperative*: focus this input, scroll to that element, play this video, measure that box, hand this node to a charting library. **Refs** are the escape hatch for reaching the underlying DOM node or holding a value that doesn't belong in state.

(`useRef` basics: [useRef](../03-hooks/04-useRef.md). This note covers the advanced patterns: callback refs, passing refs through components, and exposing a limited imperative API.)

## Two uses of refs

```tsx
const inputRef = useRef<HTMLInputElement>(null)   // 1. a handle to a DOM node
const timerRef = useRef<number | null>(null)       // 2. a mutable box that survives renders, without causing re-renders
```

**DOM refs**: attach to an element and React sets `ref.current` to the node after mount (and back to `null` on unmount).

```tsx
function SearchBox() {
  const inputRef = useRef<HTMLInputElement>(null)
  return (
    <>
      <input ref={inputRef} />
      <button onClick={() => inputRef.current?.focus()}>Focus</button>
    </>
  )
}
```

**Value refs**: store anything mutable that must persist but shouldn't trigger a re-render: timer IDs, the previous value, "latest callback", an `IntersectionObserver` instance.

### The rules

- **Don't read or write `ref.current` during render.** Render must stay pure ([concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md#what-this-means-for-how-you-write-components)). Read and write refs in **event handlers and effects**. (The one exception is lazy initialization like `ref.current ??= new Thing()`.)
- **`ref.current` is `null` until the DOM node exists.** It's set after the commit, so it's available in effects and handlers, not on the first render pass.
- **Changing a ref doesn't re-render.** If the UI must reflect the value, it belongs in state.
- Prefer props and state. Reach for a ref only when you must call something *on* the DOM.

## Good uses

| Use | Example |
|---|---|
| Focus management | Focus the first invalid field, return focus after closing a dialog ([focus management](../08-accessibility/02-keyboard-and-focus-management.md)) |
| Scrolling | `ref.current.scrollIntoView()`, scroll a chat to the bottom |
| Media control | `video.play()`, `audio.pause()` |
| Measuring | `getBoundingClientRect()`, with [`useLayoutEffect`](../03-hooks/08-useLayoutEffect.md) to avoid flicker |
| Third-party DOM libraries | Charts, maps, editors that need a container node |
| Observers | `ResizeObserver`, `IntersectionObserver` attached to a node |
| Storing non-render values | Timers, previous values, latest-callback pattern |

**Avoid** refs for things React already models: showing/hiding, changing text, toggling classes, reading an input's value in a controlled form. If you find yourself setting `ref.current.style.x` or `ref.current.textContent`, you're fighting React. Use state.

## Callback refs

A ref can be a **function** instead of an object. React calls it with the node when it's attached, which is useful when you need to *do something* the moment the element appears, or when you manage **many** refs:

```tsx
<div ref={(node) => { if (node) node.scrollIntoView() }} />
```

### React 19: cleanup functions

A callback ref may **return a cleanup function**, run when the element is removed. That pairs setup and teardown in one place, like an effect:

```tsx
<div
  ref={(node) => {
    if (!node) return
    const observer = new ResizeObserver(([entry]) => setWidth(entry.contentRect.width))
    observer.observe(node)
    return () => observer.disconnect()            // cleanup (React 19)
  }}
/>
```

Before React 19, callback refs were called with `null` on unmount instead. When a callback returns a cleanup function, React calls the cleanup and **doesn't** call the ref with `null`. If you're typing these in TypeScript, avoid expression-bodied arrows that accidentally return a value (`ref={(n) => (cache = n)}`), because the return value is now interpreted as a cleanup.

### Refs for lists

You can't call `useRef` in a loop, so collect nodes in a `Map` with a callback ref:

```tsx
function Tabs({ tabs }: { tabs: Tab[] }) {
  const nodes = useRef(new Map<string, HTMLButtonElement>())

  function focusTab(id: string) {
    nodes.current.get(id)?.focus()
  }

  return tabs.map((tab) => (
    <button
      key={tab.id}
      ref={(node) => {
        if (node) nodes.current.set(tab.id, node)
        return () => { nodes.current.delete(tab.id) }      // React 19 cleanup
      }}
    >
      {tab.label}
    </button>
  ))
}
```

## Passing refs through components

A ref on a **DOM element** gives you the node. A ref on your **own component** gives you whatever that component chooses to pass on.

### React 19: `ref` is just a prop

```tsx
function TextInput({ ref, ...props }: React.ComponentProps<"input">) {
  return <input ref={ref} className="rounded border px-2" {...props} />
}

const inputRef = useRef<HTMLInputElement>(null)
<TextInput ref={inputRef} />
```

`React.ComponentProps<"input">` already includes `ref`, so you just forward it. No wrapper needed.

### React 18 and earlier: `forwardRef`

Function components couldn't receive `ref` as a prop. `forwardRef` was required:

```tsx
const TextInput = forwardRef<HTMLInputElement, React.ComponentPropsWithoutRef<"input">>((props, ref) => (
  <input ref={ref} {...props} />
))
```

It still works in 19 but is no longer needed ([React 19 features](../15-concurrent-and-modern-react/05-react-19-features.md#ref-is-a-regular-prop)). You'll see it throughout older code and libraries, so recognize it.

## `useImperativeHandle`: expose a *limited* API

Forwarding a ref to a DOM node hands the parent **everything** (`.style`, `.remove()`, `.innerHTML`). Sometimes you want to expose **a few deliberate methods** instead, hiding the DOM:

```tsx
import { useImperativeHandle, useRef } from "react"

export type PlayerHandle = {
  play: () => void
  pause: () => void
  seek: (seconds: number) => void
}

type PlayerProps = { src: string; ref?: React.Ref<PlayerHandle> }

export function Player({ src, ref }: PlayerProps) {
  const videoRef = useRef<HTMLVideoElement>(null)

  useImperativeHandle(ref, () => ({
    play: () => { void videoRef.current?.play() },
    pause: () => videoRef.current?.pause(),
    seek: (seconds) => { if (videoRef.current) videoRef.current.currentTime = seconds },
  }), [])

  return <video ref={videoRef} src={src} controls />
}
```

```tsx
function Lesson() {
  const playerRef = useRef<PlayerHandle>(null)
  return (
    <>
      <Player ref={playerRef} src="/lesson.mp4" />
      <button onClick={() => playerRef.current?.seek(30)}>Jump to 0:30</button>
    </>
  )
}
```

Why this is better than forwarding the `<video>` node:

- **A small, stable contract**: the parent can only `play`, `pause`, `seek`. You can change the internals (swap `<video>` for a library) without breaking callers.
- **Encapsulation**: the parent can't reach in and mutate the DOM.

On React 18, wrap the component in `forwardRef` and use its second argument as `ref`. Include a dependency array (here `[]` since the methods only touch a ref), so the handle isn't recreated every render.

### When to use it

Imperative handles suit **actions with no declarative equivalent**: `focus()`, `scrollToBottom()`, `play()`, `reset()` on a complex widget, `scrollToIndex(i)` on a virtualized list ([virtualization](../14-performance/04-virtualization.md)). They're how component libraries expose "commands".

### When *not* to use it

If the parent wants to control a **value or state**, use props instead:

```tsx
// ✗ imperative: parent pokes the child
modalRef.current?.open()

// ✓ declarative: parent owns the state
<Modal open={isOpen} onClose={() => setIsOpen(false)} />
```

Imperative APIs make data flow harder to follow and test: state changes happen "from outside". Treat them as a **last resort** for things like focus and media.

## Refs and timing

```tsx
function Example() {
  const ref = useRef<HTMLDivElement>(null)
  console.log(ref.current)                      // null on the first render

  useLayoutEffect(() => {
    console.log(ref.current)                    // the node (set before layout effects run)
  }, [])

  useEffect(() => {
    console.log(ref.current)                    // the node
  }, [])
  return <div ref={ref} />
}
```

Refs are attached during the commit, before effects run. If a ref'd element is **conditionally rendered**, `ref.current` can be `null` or change between renders, so always guard with `?.` and don't store a stale node somewhere long-lived.

### Acting on the DOM right after a state update

State updates are batched and applied after your handler returns, so this doesn't work as you'd expect:

```tsx
setItems([...items, newItem])
listRef.current?.lastElementChild?.scrollIntoView()      // runs BEFORE the new item exists in the DOM
```

Options: do the DOM work in an **effect** that depends on the data, or force a synchronous update with `flushSync` for the rare case that needs it:

```tsx
import { flushSync } from "react-dom"

flushSync(() => setItems([...items, newItem]))
listRef.current?.lastElementChild?.scrollIntoView()      // new item is in the DOM now
```

Prefer the effect approach. `flushSync` hurts performance and is an escape hatch.

## Refs with TypeScript

```tsx
const inputRef = useRef<HTMLInputElement>(null)          // RefObject<HTMLInputElement | null>
const countRef = useRef(0)                                // MutableRefObject<number>-style (React 19: just RefObject<number>)
const handleRef = useRef<PlayerHandle>(null)

function Comp({ ref }: { ref?: React.Ref<HTMLDivElement> }) { … }   // typing a ref prop (React 19)
```

React 19's types require an argument to `useRef` (`useRef(null)`, `useRef<T>(undefined)`), and `ref.current` for DOM refs is typed `T | null`, so use optional chaining. Update `@types/react` together with React when upgrading.

## Common mistakes

- **Reading or writing `ref.current` during render**, which breaks purity and gives stale or `null` values.
- **Using refs to control things state should control**, setting text, classes, or visibility through the DOM.
- **Expecting a ref change to re-render.**
- **Forgetting `null`**: `ref.current.focus()` without `?.` crashes when the element isn't mounted.
- **Passing a `ref` to a component that doesn't forward it** (React 18 without `forwardRef`), so `ref.current` stays `null`.
- **Imperative APIs where props would do** (`modalRef.current.open()`), making data flow hard to trace.
- **Exposing the raw DOM node** from a component library instead of a small handle.
- **Missing dependency array on `useImperativeHandle`**, recreating the handle every render.
- **Calling `useRef` inside loops or conditions.** Use a `Map` + callback refs instead.
- **Acting on the DOM before React commits**, then wondering why the new element isn't there (use an effect).
- **Callback refs with accidental return values** in React 19 (treated as cleanup).
- **Holding onto a DOM node in a long-lived variable** after it's unmounted.

## Quick summary

- Refs are an **escape hatch**: DOM handles (focus, scroll, media, measuring, third-party libs) and mutable values that don't trigger renders.
- Read and write `ref.current` in **handlers and effects**, never during render; it's `null` until commit.
- **Callback refs** run when a node attaches (React 19 can return a cleanup); use them with a `Map` for lists.
- React 19: **`ref` is a regular prop**; `forwardRef` is the older way.
- **`useImperativeHandle`** exposes a small, deliberate API (`focus`, `scrollToIndex`, `play`) instead of the raw DOM node.
- Prefer props and state; use imperative handles as a last resort for actions with no declarative equivalent.

## Next

[02 — External stores](./02-external-stores.md)
