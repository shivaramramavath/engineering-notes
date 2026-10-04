# Events

Events are how users act on your UI: clicking, typing, submitting, hovering. In React you attach handler functions to elements as props, and React calls them when the event happens.

## Prerequisites

[`02-components-and-props.md`](./02-components-and-props.md)

---

## Adding an event handler

```jsx
function LikeButton() {
  function handleClick() {
    alert("Liked!");
  }

  return <button onClick={handleClick}>Like</button>;
}
```

Event props are **camelCase** (`onClick`, `onChange`, `onSubmit`, `onKeyDown`, `onMouseEnter`), and their value is a **function**.

### Pass the function, don't call it

```jsx
<button onClick={handleClick}>OK</button>     // ✅ passes the function
<button onClick={handleClick()}>Oops</button> // ❌ calls it during render
```

`handleClick()` runs immediately while rendering and passes its *return value* to `onClick`. This is one of the most common beginner mistakes.

### Inline handlers

For short logic, an inline arrow function is fine:

```jsx
<button onClick={() => setCount(count + 1)}>+1</button>
```

Use a named function when the logic is longer or reused. Don't worry about the arrow function being "recreated" each render; that's rarely a real performance problem (see [`../14-performance/02-memoization.md`](../14-performance/02-memoization.md)).

---

## Passing arguments

Wrap the call in an arrow function so it runs on click, not on render:

```jsx
{items.map((item) => (
  <button key={item.id} onClick={() => handleDelete(item.id)}>
    Delete
  </button>
))}
```

---

## The event object

React passes an event object as the first argument:

```jsx
function handleChange(e) {
  console.log(e.target.value);   // the input's current text
}

<input onChange={handleChange} />
```

Useful properties:

| Property | Meaning |
|----------|---------|
| `e.target` | The element that triggered the event |
| `e.currentTarget` | The element the handler is attached to |
| `e.key` | The key pressed (`"Enter"`, `"Escape"`) on keyboard events |
| `e.preventDefault()` | Stop the browser's default action |
| `e.stopPropagation()` | Stop the event bubbling to parents |

React wraps native events in a cross-browser event object, so you can use these consistently.

---

## `preventDefault`

Some elements have default browser behavior: forms reload the page on submit, links navigate. Prevent it when you handle the action yourself:

```jsx
function handleSubmit(e) {
  e.preventDefault();   // don't reload the page
  save();
}

<form onSubmit={handleSubmit}>...</form>
```

---

## Propagation (bubbling)

Most events **bubble**: they fire on the target first, then on each ancestor.

```jsx
<div onClick={() => console.log("div")}>
  <button onClick={() => console.log("button")}>Click</button>
</div>
// Clicking the button logs "button", then "div"
```

Stop it with `e.stopPropagation()` when a child action shouldn't trigger the parent's handler:

```jsx
<button onClick={(e) => { e.stopPropagation(); handleDelete(); }}>
  Delete
</button>
```

Use it sparingly — overusing it makes event flow hard to follow. Add `Capture` to a prop name (`onClickCapture`) to handle the event on the way *down* instead.

---

## Handlers as props

Events travel **up** the tree through function props:

```jsx
function SearchBar({ onSearch }) {
  const [text, setText] = useState("");

  return (
    <form
      onSubmit={(e) => {
        e.preventDefault();
        onSearch(text);
      }}
    >
      <input value={text} onChange={(e) => setText(e.target.value)} />
      <button>Search</button>
    </form>
  );
}
```

Naming convention: props that take handlers are named `onSomething`; the functions you write are named `handleSomething`.

---

## Handlers that update state

Handlers are where state usually changes in response to user actions:

```jsx
function Counter() {
  const [count, setCount] = useState(0);
  return <button onClick={() => setCount(count + 1)}>Clicked {count}</button>;
}
```

State is covered in [`../02-state-and-rendering/00-state-and-snapshots.md`](../02-state-and-rendering/00-state-and-snapshots.md). Handlers are also the right place for side effects caused by a user action, such as sending a request when a button is clicked.

---

## Keyboard accessibility

Use real interactive elements. A `<button>` is focusable and works with Enter and Space automatically; a clickable `<div>` is not:

```jsx
<button onClick={handleClick}>Save</button>   // ✅ accessible by default
<div onClick={handleClick}>Save</div>         // ❌ no keyboard or screen reader support
```

See [`../08-accessibility/00-semantic-html.md`](../08-accessibility/00-semantic-html.md) and [`../08-accessibility/02-keyboard-and-focus-management.md`](../08-accessibility/02-keyboard-and-focus-management.md).

---

## TypeScript

Handler and event types (`React.MouseEvent`, `React.ChangeEvent<HTMLInputElement>`) are in [`../04-typescript-with-react/01-typing-events.md`](../04-typescript-with-react/01-typing-events.md).

---

## Common mistakes

- **Calling the handler** — `onClick={fn()}` runs during render; use `onClick={fn}` or `onClick={() => fn(arg)}`.
- **Forgetting `preventDefault`** on form submit — the page reloads.
- **Clickable `<div>`s** — use `<button>` or `<a>`.
- **Wrong casing** — `onclick` is ignored; use `onClick`.
- **Overusing `stopPropagation`** — breaks other handlers (analytics, outside-click detection) that rely on bubbling.
- **Expecting state to update immediately** after calling a setter inside a handler — the new value appears on the next render.

## Quick summary

- Attach handlers with camelCase props and pass a function (not a call)
- Use `() => fn(arg)` to pass arguments
- The event object gives `target`, `key`, `preventDefault`, and `stopPropagation`
- Events bubble up; stop propagation only when necessary
- Children notify parents by calling function props named `onSomething`

## Next

**[`07-forms-basics.md`](./07-forms-basics.md)** puts events to work in controlled inputs and form submission.
