# Refs

A **ref** is a box React keeps for you across renders. It does two different jobs: it holds a **reference to a DOM element** (to focus an input, measure a node, scroll something into view), and it holds a **mutable value that does not trigger re-renders** (a timer id, the previous value, an instance of a third-party library). The two uses have different types, and mixing them up is the source of most ref-related TypeScript errors.

> **Version note.** React 19 made `ref` an ordinary prop for function components, so `forwardRef` is no longer needed there, and its `@types/react` changed the shape of `RefObject` and `useRef`. This note shows both the React 18 and 19 forms where they differ. Check which `@types/react` you have installed.

**Prerequisites:**
- [Hooks](./02-hooks.md)
- [Component props and children](./00-component-props-and-children.md)
- [Event types](./01-event-types.md) (DOM element types)

---

## DOM refs

```tsx
import { useRef } from "react";

function SearchBox() {
  const inputRef = useRef<HTMLInputElement>(null);

  function focusInput() {
    inputRef.current?.focus();          // current is HTMLInputElement | null
  }

  return (
    <>
      <input ref={inputRef} />
      <button onClick={focusInput}>Focus</button>
    </>
  );
}
```

- The **type argument is the element type**: `HTMLInputElement`, `HTMLDivElement`, `HTMLCanvasElement`, `HTMLFormElement`, and so on. Passing a `ref` whose element type does not match the JSX element is an error (a `useRef<HTMLDivElement>` on an `<input>`).
- The initial value is `null`, and React sets `current` when the element mounts and back to `null` when it unmounts. So `current` is `T | null` and you must handle `null` (`?.`, or an explicit check).
- Avoid `inputRef.current!`. It silences the check without guaranteeing the element exists. In an event handler or effect after mount it is usually present, but a guard costs nothing.

```tsx
function focusInput() {
  const el = inputRef.current;
  if (!el) return;
  el.focus();
}
```

### Typing differences by version

In older `@types/react`, `useRef<T>(null)` returned `RefObject<T>` with a **read-only** `current`. In React 19's types, `useRef<T>(null)` returns `RefObject<T | null>` and `current` is writable, and `MutableRefObject` is deprecated. In both, **writing your own code the same way works**: `useRef<HTMLInputElement>(null)`, then null-check `current`. Also, React 19's types require an argument: `useRef<T>(null)` or `useRef<T | undefined>(undefined)`, where older types allowed `useRef<T>()`.

## Mutable value refs

A ref can store anything, with no connection to the DOM. Changing `current` does **not** cause a re-render.

```tsx
function Stopwatch() {
  const [elapsed, setElapsed] = useState(0);
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  function start() {
    if (timerRef.current !== null) return;
    timerRef.current = setInterval(() => setElapsed((e) => e + 1), 1000);
  }

  function stop() {
    if (timerRef.current !== null) {
      clearInterval(timerRef.current);
      timerRef.current = null;
    }
  }

  useEffect(() => stop, []);            // clean up on unmount

  return (
    <>
      <p>{elapsed}s</p>
      <button onClick={start}>Start</button>
      <button onClick={stop}>Stop</button>
    </>
  );
}
```

`ReturnType<typeof setInterval>` works for both browser (`number`) and Node (`NodeJS.Timeout`) typings ([event loop](../12-async-and-iteration/00-event-loop.md)).

Common uses: timer and animation frame ids, the previous value of a prop, a flag such as `isMounted`, a latest-callback holder, an instance of an imperative library (a chart, a map), a cache that should survive renders.

```tsx
function usePrevious<T>(value: T): T | undefined {
  const ref = useRef<T | undefined>(undefined);
  useEffect(() => {
    ref.current = value;
  });
  return ref.current;                   // the value from the previous render
}
```

## Ref vs state

| | `useState` | `useRef` |
|---|---|---|
| Changing it re-renders | yes | **no** |
| Value visible in the UI | yes | not directly |
| Safe to read during render | yes | **avoid**: reading or writing `current` during render makes behavior unpredictable |
| Use for | anything the UI displays | things the UI does not display, and DOM access |

If a value affects what is on screen, it belongs in state. If you find yourself forcing re-renders after changing a ref, you wanted state.

## Callback refs

Instead of a ref object, you can pass a **function** that React calls with the element on mount and with `null` on unmount. This lets you react to an element appearing, or hold several elements:

```tsx
function List({ items }: { items: string[] }) {
  const nodes = useRef(new Map<string, HTMLLIElement>());

  return (
    <ul>
      {items.map((item) => (
        <li
          key={item}
          ref={(el) => {
            if (el) nodes.current.set(item, el);
            else nodes.current.delete(item);
          }}
        >
          {item}
        </li>
      ))}
    </ul>
  );
}
```

The callback receives `HTMLLIElement | null`. In React 19, a callback ref may also **return a cleanup function**, which runs when the element is removed. Since the cleanup replaces the `null` call, do not mix both styles in the same callback.

## Passing refs to child components

### React 19: `ref` is a prop

```tsx
import type { ComponentPropsWithRef } from "react";

function FancyInput({ ref, ...props }: ComponentPropsWithRef<"input">) {
  return <input ref={ref} className="fancy" {...props} />;
}

const inputRef = useRef<HTMLInputElement>(null);
<FancyInput ref={inputRef} placeholder="Name" />
```

`ComponentPropsWithRef<"input">` includes `ref`. For a component that takes the ref but not the other props:

```tsx
function Field({ ref }: { ref?: React.Ref<HTMLInputElement> }) {
  return <input ref={ref} />;
}
```

### React 18: `forwardRef`

A function component does not receive `ref` as a prop. Wrap it:

```tsx
import { forwardRef } from "react";

const FancyInput = forwardRef<HTMLInputElement, ComponentPropsWithoutRef<"input">>(
  function FancyInput(props, ref) {
    return <input ref={ref} className="fancy" {...props} />;
  },
);
```

The type arguments are `<RefElementType, PropsType>` in that order. `forwardRef` does not preserve generics on the wrapped component, which matters for generic components ([generic and polymorphic components](./06-generic-and-polymorphic-components.md)). Give the inner function a name so it appears in DevTools. React 19 deprecates `forwardRef` in favor of the prop form, though it still works.

### `useImperativeHandle`: exposing a limited API

Sometimes a parent should call a few methods on a child, not reach into its DOM. Define the handle's shape as an interface, and expose exactly that:

```tsx
export interface DialogHandle {
  open(): void;
  close(): void;
}

function Dialog({ ref }: { ref?: React.Ref<DialogHandle> }) {      // React 19 form
  const [isOpen, setIsOpen] = useState(false);

  useImperativeHandle(ref, () => ({
    open: () => setIsOpen(true),
    close: () => setIsOpen(false),
  }), []);

  return isOpen ? <div role="dialog">...</div> : null;
}

// parent
const dialogRef = useRef<DialogHandle>(null);
dialogRef.current?.open();
```

With React 18, wrap in `forwardRef<DialogHandle, DialogProps>`. The handle interface is the contract: parents see only `open` and `close`. Use this sparingly. Props and state are usually clearer than imperative calls.

## Useful DOM patterns

```tsx
// focus on mount
useEffect(() => { inputRef.current?.focus(); }, []);

// scroll into view
sectionRef.current?.scrollIntoView({ behavior: "smooth" });

// measure after layout, before paint
useLayoutEffect(() => {
  const rect = boxRef.current?.getBoundingClientRect();
  if (rect) setHeight(rect.height);
}, []);

// canvas
const ctx = canvasRef.current?.getContext("2d");      // CanvasRenderingContext2D | null | undefined
```

Every DOM API that can fail returns `null` or `undefined` in the types, such as `getContext`, so handle those before using the result.

## Refs and server rendering

Refs are `null` until the component mounts in the browser. During server rendering, and during the first render, `current` is `null`, so never read DOM through a ref in the render body. Do it in effects or event handlers ([Next.js](./09-nextjs.md)).

## Important rules and misconceptions

- **A ref does not cause a re-render when `current` changes.**
- **Do not read or write `ref.current` during rendering.** It makes components impure. Do it in effects and handlers.
- **`useRef` does not track whether an element exists.** It is `null` before mount and after unmount.
- **A ref on a component is not a DOM node.** In React 18 you need `forwardRef` to receive one. In React 19, it is a prop you must pass on yourself.
- **Class components** give refs to the **instance**, not a DOM element.
- **The element type must match the JSX tag.** `HTMLElement` is too loose for specific properties, so use the specific type.

## Common mistakes

- `useRef<HTMLInputElement>()` without `null` in React 19 types, or forgetting `| null` handling.
- `ref.current!.focus()` everywhere instead of a guard.
- Using a ref where state is needed, so the UI does not update.
- Reading `ref.current` during render.
- Forgetting to forward the `ref` in a wrapper component, so the parent's ref stays `null`.
- Passing `ref` as a prop to a React 18 function component and getting a warning.
- Typing a ref with the wrong element (`HTMLDivElement` on an `<input>`).
- Storing derived UI data in a ref and expecting it to display.
- Not clearing timers and listeners stored in refs on unmount.

## Debugging

- If `ref.current` is `null` in an effect, check that the element is actually rendered (not conditionally absent) and that the ref is attached to the element, not a component that does not forward it.
- If TypeScript rejects `ref={myRef}`, compare the ref's element type with the JSX element's type.
- If `forwardRef` types look wrong, check the order of the generics (`<Element, Props>`).
- Log `ref.current` inside a `useEffect` after mount to inspect what you actually have.
- In React DevTools, select the element and inspect `ref` on the component to confirm it is wired up.

## Quick summary

- A DOM ref is `useRef<ElementType>(null)`, with `current` being `T | null`: guard it. A value ref is `useRef<T>(initial)` and does not re-render.
- Use state for anything shown on screen, and refs for DOM access and values the UI does not display.
- Pass refs to children with the `ref` prop (React 19) or `forwardRef` (React 18). Expose a limited API with `useImperativeHandle` and a handle interface.
- Callback refs run on mount and unmount and support lists of elements.
- Never read or write `current` during render.

**Next:** [Forms](./05-forms.md)
