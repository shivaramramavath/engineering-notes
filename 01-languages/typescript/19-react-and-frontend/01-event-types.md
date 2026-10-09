# Event Types

React wraps browser events in its own **synthetic event** objects, and `@types/react` has a matching type for each: `MouseEvent`, `ChangeEvent`, `KeyboardEvent`, and so on, each generic over the element the handler is attached to. Getting these right gives you typed `currentTarget`, correct property names, and autocomplete. Getting them wrong produces some of the most annoying TypeScript errors in React code. This note covers the types you need, why `target` and `currentTarget` differ, and how to type handlers and native listeners.

**Prerequisites:**
- [Component props and children](./00-component-props-and-children.md)
- [Callbacks](../02-functions/02-callbacks.md)
- [Type narrowing](../03-unions-and-narrowing/03-type-narrowing.md) (`instanceof` on DOM elements)

---

## Inline handlers need no annotation

When you write the handler **inline**, TypeScript infers the event type from the element and the event name:

```tsx
<button onClick={(e) => console.log(e.clientX)} />       // e: React.MouseEvent<HTMLButtonElement>
<input onChange={(e) => console.log(e.target.value)} />  // e: React.ChangeEvent<HTMLInputElement>
<form onSubmit={(e) => e.preventDefault()} />            // e: React.FormEvent<HTMLFormElement>
```

This is contextual typing: the expected type of the `onClick` prop supplies the parameter type ([type widening and inference](../14-type-system-internals/03-type-widening-and-inference.md)). Prefer inline handlers when they are short.

## Extracted handlers need an annotation

If you define the handler separately, there is no context, so annotate the parameter:

```tsx
import type { ChangeEvent, FormEvent, MouseEvent, KeyboardEvent } from "react";

function handleChange(e: ChangeEvent<HTMLInputElement>) {
  setName(e.target.value);
}

function handleSubmit(e: FormEvent<HTMLFormElement>) {
  e.preventDefault();
  save();
}

function handleClick(e: MouseEvent<HTMLButtonElement>) {
  console.log(e.currentTarget.name);
}

function handleKeyDown(e: KeyboardEvent<HTMLInputElement>) {
  if (e.key === "Enter") submit();
}
```

The type argument is the **element the handler is attached to**. Using the wrong element (say `HTMLInputElement` for a `<select>`) gives an error when you attach the handler.

Alternatively, annotate the **handler type** instead of the parameter, using the `*Handler` aliases, so the parameter is inferred:

```tsx
import type { ChangeEventHandler } from "react";

const handleChange: ChangeEventHandler<HTMLInputElement> = (e) => {
  setName(e.target.value);
};
```

## Common event types

| Event | Type | Element generic examples |
|---|---|---|
| click, mouse down/up, move | `MouseEvent<T>` | `HTMLButtonElement`, `HTMLDivElement`, `HTMLAnchorElement` |
| input, select, textarea change | `ChangeEvent<T>` | `HTMLInputElement`, `HTMLSelectElement`, `HTMLTextAreaElement` |
| form submit | `FormEvent<T>` | `HTMLFormElement` |
| key down/up | `KeyboardEvent<T>` | `HTMLInputElement`, `HTMLDivElement` |
| focus / blur | `FocusEvent<T>` | `HTMLInputElement` |
| drag and drop | `DragEvent<T>` | `HTMLDivElement` |
| clipboard | `ClipboardEvent<T>` | `HTMLInputElement` |
| pointer | `PointerEvent<T>` | `HTMLCanvasElement` |
| touch | `TouchEvent<T>` | `HTMLDivElement` |
| wheel | `WheelEvent<T>` | `HTMLDivElement` |
| generic fallback | `SyntheticEvent<T>` | any |

Each has a matching `*EventHandler<T>` alias (`MouseEventHandler`, `ChangeEventHandler`, ...) for typing handler props and variables.

## `target` vs `currentTarget`

This is the most common source of confusion.

- **`e.currentTarget`** is the element **the handler is attached to**. React types it as the generic argument you provided, so it is precisely typed (`HTMLButtonElement`, `HTMLFormElement`).
- **`e.target`** is the element that **originally triggered** the event. With bubbling it can be a descendant (a `<span>` inside a button), so TypeScript types it as the broad `EventTarget`.

```tsx
function handleClick(e: MouseEvent<HTMLButtonElement>) {
  e.currentTarget.disabled = true;     // ok: HTMLButtonElement
  e.target.disabled = true;            // error: 'disabled' does not exist on type 'EventTarget'
}
```

For `ChangeEvent`, React narrows `target` for you, because for an input's own change event the target is the input:

```tsx
function handleChange(e: ChangeEvent<HTMLInputElement>) {
  e.target.value;           // ok: string
  e.currentTarget.value;    // also fine
}
```

When you do need to inspect `target` on another event (event delegation), **narrow it** rather than asserting:

```tsx
function handleClick(e: MouseEvent<HTMLUListElement>) {
  const target = e.target;
  if (target instanceof HTMLElement && target.dataset.id) {
    select(target.dataset.id);          // runtime check, then typed access
  }
}
```

A cast (`e.target as HTMLInputElement`) works but is unchecked ([soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)).

## Reading values from form controls

All DOM values are strings, so convert deliberately:

```tsx
// text
<input onChange={(e) => setName(e.target.value)} />

// number: use valueAsNumber (NaN when empty or invalid), not Number(value) blindly
<input type="number" onChange={(e) => setAge(e.target.valueAsNumber)} />

// checkbox: use checked, not value
<input type="checkbox" onChange={(e) => setAccepted(e.target.checked)} />

// select: the value is a string, so narrow it if your state is a union
<select onChange={(e) => setSize(e.target.value as Size)}>

// file input: files is FileList | null
<input type="file" onChange={(e) => {
  const file = e.target.files?.[0];
  if (file) upload(file);
}} />
```

The `as Size` on a select value is the one assertion that is hard to avoid, since a DOM value is just `string`. If the options come from your own list, wrap it in a small guard (`isSize(value)`) to validate it ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)).

## Typing event props on your own components

Choose what your component's callback receives:

```tsx
interface SearchBoxProps {
  onChange: (value: string) => void;                       // value callback: simplest for callers
  onKeyDown?: (e: KeyboardEvent<HTMLInputElement>) => void; // raw event when callers need it
}

function SearchBox({ onChange, onKeyDown }: SearchBoxProps) {
  return <input onChange={(e) => onChange(e.target.value)} onKeyDown={onKeyDown} />;
}
```

If the component is a thin wrapper around a native element, extend that element's props instead of declaring handlers by hand ([component props](./00-component-props-and-children.md)):

```tsx
interface ButtonProps extends ComponentPropsWithoutRef<"button"> {}
// onClick is already typed as MouseEventHandler<HTMLButtonElement>
```

## Native DOM events (outside JSX)

Listeners added with `addEventListener` (in `useEffect`) use the **native** DOM event types, not React's:

```tsx
useEffect(() => {
  function onResize(e: UIEvent) {
    setWidth(window.innerWidth);
  }
  window.addEventListener("resize", onResize);
  return () => window.removeEventListener("resize", onResize);
}, []);

useEffect(() => {
  function onKey(e: KeyboardEvent) {            // DOM KeyboardEvent, not React's
    if (e.key === "Escape") close();
  }
  document.addEventListener("keydown", onKey);
  return () => document.removeEventListener("keydown", onKey);
}, [close]);
```

Name collision to watch for: `KeyboardEvent` and `MouseEvent` exist both as **global DOM types** and as **React types** (`React.KeyboardEvent`). If you import the React ones by name (`import type { KeyboardEvent } from "react"`), the DOM global is shadowed in that file. Use `React.KeyboardEvent` (via `import type React from "react"`) or alias imports to keep both clear.

`addEventListener` is overloaded by event name, so `"keydown"` gives a `KeyboardEvent` and `"resize"` gives a `UIEvent` automatically when you write the handler inline. Always remove listeners in the effect cleanup. For custom events and typed emitters, see [typed event emitter](../17-design-patterns/07-typed-event-emitter.md).

## Behavior worth knowing

- **Synthetic events:** React wraps native events for cross-browser consistency. Since React 17, events are no longer pooled, so you can read properties asynchronously. The underlying native event is `e.nativeEvent`.
- **Propagation:** `e.stopPropagation()` and `e.preventDefault()` work as expected. Forms need `e.preventDefault()` in `onSubmit` to avoid a page reload.
- **Capture handlers:** `onClickCapture` fires during the capture phase.
- **Passive listeners:** touch and wheel handlers may be passive, in which case `preventDefault()` has no effect. Use native listeners with `{ passive: false }` when needed.
- **Async handlers:** `onClick={async () => { ... }}` is allowed (the returned promise is ignored). Handle errors inside it, because an unhandled rejection will not be caught by React ([async/await](../12-async-and-iteration/02-async-await.md)).

## Common mistakes

- Annotating the event with the wrong element (`ChangeEvent<HTMLInputElement>` on a `<select>`).
- Accessing `e.target.value` on a non-change event, where `target` is `EventTarget`.
- Casting `e.target as HTMLInputElement` everywhere instead of using `currentTarget` or narrowing.
- Using `any` for events.
- Mixing up React and DOM event types in `useEffect` listeners, or shadowing the global type by importing by name.
- Using `Number(e.target.value)` for number inputs and getting `0` from empty strings (use `valueAsNumber` and handle `NaN`).
- Using `value` instead of `checked` for checkboxes.
- Forgetting `e.preventDefault()` in submit handlers.
- Forgetting to remove native listeners in effect cleanup.
- Passing `() => handler(e)` patterns that create stale closures when the handler depends on changing state.

## Debugging

- Hover the event parameter in an inline handler to see the inferred type, then copy it when extracting the handler.
- If a property "does not exist on type EventTarget", you are on `e.target`. Switch to `e.currentTarget` or narrow with `instanceof`.
- If an extracted handler will not attach ("not assignable to type MouseEventHandler"), compare its element generic with the element it is attached to.
- Log `e.nativeEvent` to inspect the browser's own event.
- If a handler never fires, check for an overlay element capturing clicks, a missing `type="button"` that is submitting a form, or a listener attached to the wrong element.

## Quick summary

- Use inline handlers and let TypeScript infer the event type. Annotate extracted handlers with `*Event<Element>` or `*EventHandler<Element>`.
- The generic argument is the element the handler is attached to. `currentTarget` is typed to it, while `target` is a broad `EventTarget` unless React narrows it (`ChangeEvent`).
- Narrow `target` with `instanceof` for event delegation rather than casting.
- Convert form values deliberately: `valueAsNumber`, `checked`, `files?.[0]`, and validate select values against your union.
- Native `addEventListener` uses DOM event types. Watch for name collisions with React's types, and always clean up listeners.

**Next:** [Hooks](./02-hooks.md)