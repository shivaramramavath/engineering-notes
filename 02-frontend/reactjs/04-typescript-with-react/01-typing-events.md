# Typing Events

Event handlers receive an event object whose type depends on the event and the element. Getting those types right gives you accurate autocomplete for `e.target.value`, `e.key`, and friends, and catches mistakes like reading `.value` from a button. In most code you won't write event types at all — inference does it — but you need them the moment a handler is defined separately from the element.

## Prerequisites

[`00-typing-components-and-props.md`](./00-typing-components-and-props.md) and [`../01-fundamentals/06-events.md`](../01-fundamentals/06-events.md)

---

## Inline handlers are inferred

When you write the handler inline, TypeScript knows the event type from the element and event name:

```tsx
<input onChange={(e) => console.log(e.target.value)} />   // e: ChangeEvent<HTMLInputElement>
<button onClick={(e) => e.currentTarget.blur()} />        // e: MouseEvent<HTMLButtonElement>
```

No annotation needed. This is the easiest way to get correct types.

---

## Annotating extracted handlers

Once you pull the handler out into its own function, there's no context to infer from, so annotate the parameter with React's event types:

```tsx
import type { ChangeEvent, MouseEvent, FormEvent, KeyboardEvent } from "react";

function handleChange(e: ChangeEvent<HTMLInputElement>) {
  setName(e.target.value);
}

function handleClick(e: MouseEvent<HTMLButtonElement>) {
  console.log(e.currentTarget.name);
}

function handleSubmit(e: FormEvent<HTMLFormElement>) {
  e.preventDefault();
  // ...
}

function handleKeyDown(e: KeyboardEvent<HTMLInputElement>) {
  if (e.key === "Enter") submit();
}
```

The pattern is always `XxxEvent<ElementType>`:

| Event | Type |
|-------|------|
| `onClick`, `onMouseEnter`… | `React.MouseEvent<HTMLButtonElement>` (use the element's type) |
| `onChange` on `<input>` | `React.ChangeEvent<HTMLInputElement>` |
| `onChange` on `<textarea>` | `React.ChangeEvent<HTMLTextAreaElement>` |
| `onChange` on `<select>` | `React.ChangeEvent<HTMLSelectElement>` |
| `onSubmit` | `React.FormEvent<HTMLFormElement>` (or `SubmitEvent`) |
| `onKeyDown`, `onKeyUp` | `React.KeyboardEvent<HTMLInputElement>` |
| `onFocus`, `onBlur` | `React.FocusEvent<HTMLInputElement>` |
| `onDrag`, `onDrop` | `React.DragEvent<HTMLDivElement>` |
| `onPointerDown`… | `React.PointerEvent<HTMLElement>` |
| `onCopy`, `onPaste` | `React.ClipboardEvent<HTMLInputElement>` |
| `onScroll` | `React.UIEvent<HTMLDivElement>` |

Not sure which type? **Hover over the `e` in an inline handler** in your editor — it shows the exact type to copy.

### Handler type aliases

Each event type has a matching handler alias, useful for typing a whole function or a prop:

```tsx
import type { ChangeEventHandler, MouseEventHandler } from "react";

const handleChange: ChangeEventHandler<HTMLInputElement> = (e) => {
  setName(e.target.value);    // e is inferred here
};

type ButtonProps = {
  onClick: MouseEventHandler<HTMLButtonElement>;
};
```

Use whichever style reads better; they're equivalent.

---

## `target` vs `currentTarget`

The two have different types, and the difference trips people up:

- **`e.currentTarget`** — the element the handler is **attached to**. Fully typed as the element you specified (`HTMLInputElement`).
- **`e.target`** — the element that **triggered** the event, which could be a descendant. Typed as the broader `EventTarget` on generic events.

On `ChangeEvent<HTMLInputElement>`, React types `e.target` as the input, so `e.target.value` works. On mouse and other events, prefer `e.currentTarget` for anything element-specific:

```tsx
function handleClick(e: MouseEvent<HTMLButtonElement>) {
  e.currentTarget.disabled = true;    // ✅ typed as HTMLButtonElement
  // e.target.disabled                // ❌ EventTarget has no "disabled"
}
```

If you really need `target` as a specific element (event delegation on a parent), narrow it:

```tsx
if (e.target instanceof HTMLButtonElement) {
  console.log(e.target.name);
}
```

---

## Form handling

### Reading the value from a change event

Different controls expose different properties:

```tsx
// text input / textarea / select
(e: ChangeEvent<HTMLInputElement>) => setValue(e.target.value);

// checkbox
(e: ChangeEvent<HTMLInputElement>) => setChecked(e.target.checked);

// number input — value is still a string
(e: ChangeEvent<HTMLInputElement>) => setAge(Number(e.target.value));
```

### One handler for many inputs

```tsx
type FormState = { name: string; email: string; role: "user" | "admin" };

function handleChange(
  e: ChangeEvent<HTMLInputElement | HTMLSelectElement>
) {
  const { name, value } = e.target;
  setForm((prev) => ({ ...prev, [name]: value }));
}
```

Computed keys lose some type safety (`name` is just `string`). For stricter code, use separate handlers or a form library ([`../06-forms/02-react-hook-form.md`](../06-forms/02-react-hook-form.md)).

### Submit and `FormData`

```tsx
function handleSubmit(e: FormEvent<HTMLFormElement>) {
  e.preventDefault();
  const data = new FormData(e.currentTarget);
  const email = data.get("email");               // FormDataEntryValue | null
  if (typeof email !== "string") return;         // narrow before using
  save({ email });
}
```

`FormData.get` returns `string | File | null`, so narrow it. Use `e.currentTarget` (the form), not `e.target`.

---

## Passing values instead of events

Components usually shouldn't leak DOM events into their public API. Prefer callbacks that receive the **value**:

```tsx
type SearchBarProps = {
  value: string;
  onValueChange: (value: string) => void;
};

function SearchBar({ value, onValueChange }: SearchBarProps) {
  return <input value={value} onChange={(e) => onValueChange(e.target.value)} />;
}
```

The parent doesn't need to know how the value was produced, and the component is easier to test and reuse. Reserve event-typed props for wrappers that intentionally mirror native elements (for example, `ComponentProps<"input">` already includes `onChange`).

---

## Custom events and generic handlers

For your own events, define the payload type:

```tsx
type TabChange = { index: number; label: string };

type TabsProps = {
  onTabChange: (change: TabChange) => void;
};
```

A handler shared across element types can use the broad base types: `React.SyntheticEvent` (any React event) and `React.UIEvent`.

---

## Native DOM events in effects

Listeners added with `addEventListener` use the **DOM** event types, not React's:

```tsx
useEffect(() => {
  function onKey(e: KeyboardEvent) {   // global DOM KeyboardEvent (not React's)
    if (e.key === "Escape") close();
  }
  window.addEventListener("keydown", onKey);
  return () => window.removeEventListener("keydown", onKey);
}, []);
```

Watch the name clash: `KeyboardEvent` without importing from React is the DOM type; `React.KeyboardEvent<T>` is the React one. Import React's types with `import type { KeyboardEvent } from "react"` only when you want those.

---

## Common mistakes

- **Annotating inline handlers** — unnecessary; they're inferred.
- **Using `any` for `e`** — you lose `target`/`key` checking; use the right event type.
- **Reading `e.target.value` on non-input events** — use `ChangeEvent<HTMLInputElement>`, or `currentTarget`.
- **Using `e.target` where `currentTarget` is meant** — gives the wrong (broader) type.
- **Using the wrong element type** (`HTMLElement`) — you lose properties like `value`.
- **Treating `FormData.get()` results as strings** — it can be `File` or `null`; narrow first.
- **Mixing up DOM and React event types** in `addEventListener` callbacks.
- **Exposing raw DOM events in component props** — pass values when the parent doesn't need the event.

## Quick summary

- Inline handlers need no annotations; extracted ones use `XxxEvent<HTMLElementType>`
- Hover an inline `e` to discover the right type
- `currentTarget` is the handler's element (well typed); `target` may be a descendant
- Read `value`, `checked`, or `FormData` from the right place and narrow where needed
- Prefer `(value: T) => void` callbacks over event props for component APIs
- DOM listeners in effects use the DOM event types

## Next

**[`02-typing-hooks.md`](./02-typing-hooks.md)** covers typing `useState`, `useReducer`, `useRef`, `useContext`, and custom hooks.
