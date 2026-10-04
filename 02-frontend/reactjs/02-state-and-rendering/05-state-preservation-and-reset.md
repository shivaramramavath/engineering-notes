# State Preservation and Reset

State isn't stored inside your component function. React stores it, and associates it with a **position in the render tree**. That one fact explains why some components keep their state when you expect them to reset, and why others lose it when you expect them to stay. This file gives the rules and the tools (`key`, lifting, CSS hiding) to control it.

## Prerequisites

[`04-component-lifecycle.md`](./04-component-lifecycle.md) and keys from [`../01-fundamentals/05-lists-and-keys.md`](../01-fundamentals/05-lists-and-keys.md).

---

## The core rule

> React keeps a component's state as long as **the same component type** is rendered **at the same position** in the tree. If either changes, the state is destroyed.

The "position" is its place in the tree structure — not in your JSX file.

---

## State is tied to the tree position

```jsx
function App() {
  return (
    <div>
      <Counter />   {/* position 0 */}
      <Counter />   {/* position 1 */}
    </div>
  );
}
```

These are two separate counters because they sit at different positions. Each has its own state.

### Same component, same position → state preserved

```jsx
function App() {
  const [isFancy, setIsFancy] = useState(false);

  return (
    <div>
      {isFancy ? <Counter isFancy={true} /> : <Counter isFancy={false} />}
      <button onClick={() => setIsFancy(!isFancy)}>Toggle</button>
    </div>
  );
}
```

Even though the ternary produces two different JSX elements, both are `<Counter />` at the same position. Toggling keeps the count — React is just passing new props to the **same** instance.

### Different component type at the same position → state reset

```jsx
{isPaused ? <p>Paused</p> : <Counter />}
```

When `isPaused` becomes `true`, `Counter` is **removed** and its state is gone. When it flips back, a **brand-new** `Counter` mounts, starting from its initial state.

Same idea applies to different element types:

```jsx
{isFancy ? <div><Counter /></div> : <section><Counter /></section>}
```

`<div>` vs `<section>` are different types at the same position, so the whole subtree (including `Counter`'s state) resets.

### Conditionally rendering `null` or `false`

```jsx
{showHint && <Hint />}
```

Falsy values still **occupy a slot** in the tree, so siblings keep their positions. But the `Hint` itself unmounts and loses its state whenever `showHint` is `false`.

---

## Resetting state deliberately: `key`

Sometimes you want a fresh component even though the type and position are the same — for example, a chat or profile form shown for a different person. Give the component a `key` that identifies what it represents:

```jsx
function Messenger() {
  const [to, setTo] = useState(contacts[0]);

  return (
    <>
      <ContactList contacts={contacts} onSelect={setTo} />
      <Chat key={to.id} contact={to} />   {/* different id → fresh Chat */}
    </>
  );
}
```

Without the `key`, switching contacts would **keep** the half-typed draft message in `Chat`. With `key={to.id}`, React treats each contact's `Chat` as a distinct component, so changing the key **unmounts the old one and mounts a new one** with fresh state.

Keys aren't only for lists. They are a general **identity** marker: same key means same component; different key means different component.

### Resetting a form

```jsx
<ProfileForm key={user.id} user={user} />
```

This is usually better than writing code that manually clears each field with effects when `user` changes, which is bug-prone. For the "adjusting state when a prop changes" cases, see [`../03-hooks/03-you-might-not-need-an-effect.md`](../03-hooks/03-you-might-not-need-an-effect.md).

---

## Preserving state when something is hidden

If you want a component to keep its state while not visible, you have several options:

1. **Hide with CSS** — the component stays mounted, so state is kept:

   ```jsx
   <Panel style={{ display: isVisible ? "block" : "none" }} />
   ```

   Good for small, cheap content (a tab with a half-filled form). The cost: hidden components still render, run effects, and stay in memory.

2. **Lift state up** — hold the state in a parent that stays mounted, and pass it down. The child can unmount freely without losing data. This is often the right answer ([`02-state-structure-and-lifting.md`](./02-state-structure-and-lifting.md)).

3. **Keep it elsewhere** — URL parameters, context, `localStorage`, or a state library (see [`../13-state-management/00-choosing-state-management.md`](../13-state-management/00-choosing-state-management.md)). This also survives page reloads and navigation.

React also offers an `Activity` component for hiding UI while preserving state; check the React docs for its current status before relying on it.

---

## The nested component definition bug

Defining a component **inside** another component creates a *new function every render*, so React sees a **different component type** each time and resets its state:

```jsx
function Parent() {
  const [count, setCount] = useState(0);

  // ❌ A new MyTextField function on every Parent render
  function MyTextField() {
    const [text, setText] = useState("");
    return <input value={text} onChange={(e) => setText(e.target.value)} />;
  }

  return (
    <>
      <MyTextField />
      <button onClick={() => setCount(count + 1)}>Clicked {count}</button>
    </>
  );
}
```

Each click re-renders `Parent`, creating a new `MyTextField` type, so the input is wiped. Typing then clicking the button clears the text.

**Fix:** declare components at the **top level** of the module:

```jsx
function MyTextField() { /* ... */ }

function Parent() { /* uses <MyTextField /> */ }
```

---

## Lists and state

In lists, **keys are the identity** that preserves state per item (see [`../01-fundamentals/05-lists-and-keys.md`](../01-fundamentals/05-lists-and-keys.md)). With index keys, reordering or deleting makes React attach state to the wrong item. With stable ids, state follows its item.

---

## Summary of what resets state

| Situation | State |
|-----------|-------|
| Same type, same position, same key | **Preserved** |
| Same type and position, different `key` | **Reset** |
| Different component type at the same position | **Reset** |
| Component no longer rendered (conditional `false`) | **Destroyed** |
| Component moved to a different position | **Reset** (it's a new instance) |
| Hidden with CSS only | **Preserved** |

---

## Common mistakes

- **Expecting state to reset when props change** — same type and position means state persists; use a `key` to reset.
- **Expecting state to persist across a conditional** — unmounting destroys it; lift the state or hide with CSS.
- **Declaring components inside components** — state resets on every parent render; define components at module scope.
- **Index keys on dynamic lists** — state attaches to the wrong items.
- **Writing effects to reset state on prop change** — a `key` is simpler and more reliable.
- **Using `Math.random()` as a key** — remounts everything on every render, destroying all state.

## Quick summary

- State is tied to a component's **type and position** in the tree, not to the function
- Same type + position keeps state; different type, position, or key resets it
- Use `key` to intentionally reset a component (a form or chat per user)
- To keep state while hiding: hide with CSS, lift state up, or store it elsewhere
- Never define components inside other components
- Stable keys let list items keep their own state

## Next

You've finished the mental model. Continue to **[`../03-hooks/README.md`](../03-hooks/README.md)** for the hooks themselves, beginning with the rules that keep them predictable. For how React implements all of this, see [`../17-react-internals/01-reconciliation.md`](../17-react-internals/01-reconciliation.md).
