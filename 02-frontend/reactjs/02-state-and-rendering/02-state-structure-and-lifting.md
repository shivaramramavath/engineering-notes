# State Structure and Lifting

How you shape state decides whether a component is easy to change or full of bugs. Well-structured state is minimal, has one source of truth, and lives in the right component. This file gives concrete rules for the shape of state and for deciding **where** it lives.

## Prerequisites

[`01-state-updates-and-batching.md`](./01-state-updates-and-batching.md) and the process in [`../01-fundamentals/00-thinking-in-react.md`](../01-fundamentals/00-thinking-in-react.md).

---

## Principles for structuring state

### 1. Group related state

If two values always change together, keep them as one object (or one variable) rather than separate pieces that can drift apart:

```jsx
// Two variables that must be updated together
const [x, setX] = useState(0);
const [y, setY] = useState(0);

// One object
const [position, setPosition] = useState({ x: 0, y: 0 });
```

Rule of thumb: if you forget to update one of them, would you get a bug? Then group them. For unrelated values, separate variables are simpler.

### 2. Avoid contradictory state

Multiple booleans can combine into impossible situations:

```jsx
// ❌ What does isSending = true and isSent = true mean?
const [isSending, setIsSending] = useState(false);
const [isSent, setIsSent] = useState(false);

// ✅ One variable that can only be one thing at a time
const [status, setStatus] = useState("idle");
// "idle" | "sending" | "sent" | "error"
```

Derive the booleans you need from the single source:

```jsx
const isSending = status === "sending";
```

For richer cases (transitions between states, side effects), see `useReducer` in [`../03-hooks/06-useReducer.md`](../03-hooks/06-useReducer.md).

### 3. Avoid redundant state

If a value can be **calculated** from props or other state, don't store it:

```jsx
// ❌ fullName can always be computed
const [firstName, setFirstName] = useState("");
const [lastName, setLastName] = useState("");
const [fullName, setFullName] = useState("");

// ✅ compute it during render
const fullName = `${firstName} ${lastName}`;
```

Redundant state falls out of sync the moment you forget one `setFullName(...)`. This is called **derived state**: calculate it in the render body, no effect or extra state needed. (If the calculation is genuinely expensive, see [`../03-hooks/07-useMemo-and-useCallback.md`](../03-hooks/07-useMemo-and-useCallback.md).)

### 4. Don't mirror props in state

```jsx
// ❌ Copies the prop once; ignores later prop changes
function Message({ color }) {
  const [messageColor, setMessageColor] = useState(color);
}

// ✅ Use the prop directly
function Message({ color }) { /* use `color` */ }
```

Mirroring is only appropriate when you intentionally want an *initial value* (name the prop `initialColor`) and are happy to ignore updates.

### 5. Avoid duplication

Don't store the same data in two places:

```jsx
// ❌ selectedItem is a copy; editing items doesn't update it
const [items, setItems] = useState(initialItems);
const [selectedItem, setSelectedItem] = useState(items[0]);

// ✅ Store the id; look up the item
const [selectedId, setSelectedId] = useState(items[0].id);
const selectedItem = items.find((i) => i.id === selectedId);
```

### 6. Avoid deeply nested state

Deep trees are painful to update immutably. Prefer a **flat** structure, referencing by id:

```jsx
// ❌ Nested
{ id: 0, title: "Root", children: [{ id: 1, title: "A", children: [...] }] }

// ✅ Flat (normalized)
{
  0: { id: 0, title: "Root", childIds: [1, 2] },
  1: { id: 1, title: "A", childIds: [] },
  2: { id: 2, title: "B", childIds: [] },
}
```

Updating one node then touches one entry, not a path through a tree.

---

## Lifting state up

When two components need to reflect the same changing data, **move the state to their closest common parent** and pass it down as props. The parent becomes the **single source of truth**.

### Example: two panels, one open at a time

Each panel keeping its own `isOpen` state can't coordinate. Lift it:

```jsx
function Accordion() {
  const [activeIndex, setActiveIndex] = useState(0);

  return (
    <>
      <Panel
        title="About"
        isActive={activeIndex === 0}
        onShow={() => setActiveIndex(0)}
      >
        About content
      </Panel>
      <Panel
        title="Etymology"
        isActive={activeIndex === 1}
        onShow={() => setActiveIndex(1)}
      >
        Etymology content
      </Panel>
    </>
  );
}

function Panel({ title, children, isActive, onShow }) {
  return (
    <section>
      <h3>{title}</h3>
      {isActive ? <p>{children}</p> : <button onClick={onShow}>Show</button>}
    </section>
  );
}
```

`Panel` has no state of its own; it is **controlled** by the parent through props. The three steps:

1. **Remove** state from the child components.
2. **Pass** the data and handlers down from the common parent.
3. **Add** state to the common parent.

### Controlled vs uncontrolled

- **Controlled component:** its important information comes from props; the parent drives it (`Panel` above, an `<input value={...}>`).
- **Uncontrolled component:** it keeps its own local state (`<input>` with no `value`).

Neither is wrong. Let components own state they alone care about; lift it when others need it. For component API design see [`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md); for inputs see [`../06-forms/00-controlled-and-uncontrolled-inputs.md`](../06-forms/00-controlled-and-uncontrolled-inputs.md).

---

## Where should state live?

1. Find every component that **uses** the state.
2. Find their **closest common parent**.
3. Put the state there (or above if it makes sense).

Keep state **as low as possible**: state that only one component uses should stay in that component. Lifting too high causes unnecessary re-renders of large subtrees and lots of prop passing.

When many distant components need the data or props pass through many layers ("prop drilling"), alternatives include composition ([`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md)), context ([`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md)), or a state library. The full decision guide is in [`../13-state-management/00-choosing-state-management.md`](../13-state-management/00-choosing-state-management.md).

---

## Common mistakes

- **Multiple booleans for one process** — leads to impossible combinations; use a status value.
- **Storing derived data in state** — compute it during render.
- **Copying props or other state into new state** — creates a second source of truth that goes stale.
- **Storing an object copy instead of an id** — updates to the original don't reach the copy.
- **Deeply nested state** — flatten it.
- **Lifting state too high "just in case"** — keep it as close as possible to where it's used.
- **Two components each holding their own copy of shared data** — lift it to the common parent.

## Quick summary

- Group related values, avoid contradictions, redundancy, duplication, and deep nesting
- Derive what you can instead of storing it
- Store ids rather than copies of objects
- Lift state to the closest common parent when components must stay in sync
- Keep state as low in the tree as it can be while still serving everyone who uses it

## Next

**[`03-rendering.md`](./03-rendering.md)** explains what actually happens when state changes: how React renders and updates the screen.
