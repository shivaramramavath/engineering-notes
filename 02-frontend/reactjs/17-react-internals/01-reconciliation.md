# Reconciliation

**Reconciliation** is how React decides **what to change** when your components produce new output. After a render, React has two descriptions of the UI: the previous one and the new one. Comparing them to find the minimum set of DOM changes is reconciliation, commonly called "diffing".

A fully general tree diff is O(n³), which is far too slow. React gets to **O(n)** by making two assumptions that hold in practice:

1. **Elements of different types produce different trees.** React doesn't try to match them up.
2. **A `key` identifies which children are the same across renders.**

Almost everything surprising about React's state behavior follows from these two rules.

(Where this fits: it happens in `beginWork` of the [render phase](./00-fiber-architecture.md#the-work-loop); the result is flags on fibers, applied at commit.)

## Terminology

- **Render**: calling your component functions to get a new element tree.
- **Reconcile**: diffing that output against the previous fibers to decide what to create, update, or delete.
- **Commit**: applying those decisions to the DOM.

A component can render without anything being committed to the DOM, because the diff found no changes.

## Rule 1: compare by type, at the same position

React walks both trees together, level by level, comparing the element at each **position**.

### Same type → update

```tsx
// before
<div className="a" title="x" />
// after
<div className="b" title="x" />
```

Same host type (`div`), so React **keeps the DOM node** and updates only the attributes that changed (`className`). `title` is untouched.

For a **component** type, React keeps the **same fiber** (and therefore its state), calls the component with the new props, and continues diffing its output. This is the "update" path, and it's why state persists across re-renders.

### Different type → destroy and rebuild

```tsx
// before
<div><Counter /></div>
// after
<section><Counter /></section>
```

`div` → `section` is a different type. React **unmounts the entire old subtree** (running effect cleanups and discarding all state) and **mounts a brand new one**. The `Counter` inside is destroyed and recreated even though it's the "same" component, because its parent changed type, so its state resets to initial.

```tsx
// before
<Counter />
// after
<Profile />
```

Same position, different component type: `Counter` is unmounted, `Profile` is mounted fresh.

## State belongs to a position in the tree

State isn't attached to the *component definition*; it's attached to a **fiber at a position**. As long as the same type occupies the same position, the state is preserved.

```tsx
function App({ isFancy }: { isFancy: boolean }) {
  return isFancy ? <Counter fancy /> : <Counter />
}
```

Toggling `isFancy` keeps the counter's value, because it's the same type (`Counter`) at the same position. Only the prop changed.

```tsx
function App({ isFancy }: { isFancy: boolean }) {
  return isFancy ? <div><Counter /></div> : <section><Counter /></section>
}
```

Here the wrapper type changes, so the counter **resets** on toggle.

### Conditional rendering and positions

```tsx
<div>
  {showWarning && <Warning />}
  <Counter />
</div>
```

When `showWarning` is `false`, the first child is `false`, which still **occupies a slot**. `Counter` is always at position 1, so toggling the warning doesn't reset it. But if you write:

```tsx
{showWarning ? <Warning /> : null}
{showWarning && <Warning />}
```

These are equivalent for position purposes. The slot exists either way. What *does* change positions is **early returns and different JSX shapes** that move a component to a different slot or parent.

### Resetting state on purpose

Since identity = type + position (+ key), you can force a reset by changing the **key**:

```tsx
<ProfileForm key={userId} userId={userId} />      {/* new user → new fiber → fresh state */}
```

Changing `key` tells React "this is a different component", so it unmounts the old and mounts a new one. This is the idiomatic way to reset a form or editor when switching records ([state preservation and reset](../02-state-and-rendering/05-state-preservation-and-reset.md)), and often better than syncing state with `useEffect`.

## Rule 2: keys identify children in lists

For a list of children, comparing by position isn't enough. If an item is inserted at the front, every position shifts.

### Without keys (positional)

```tsx
// before: [A, B, C]
// after:  [X, A, B, C]
```

React compares position by position: position 0 (A → X), position 1 (B → A), position 2 (C → B), position 3 (nothing → C). Every item gets **updated** (and each component's *state stays at its position*, now attached to the wrong data), plus one new item is created at the end.

```text
position:   0    1    2    3
before:     A    B    C
after:      X    A    B    C
React sees: A→X  B→A  C→B  +C       ← 3 updates and a mount; component state sticks to positions
```

### With keys (by identity)

```tsx
// before: [A(key=a), B(key=b), C(key=c)]
// after:  [X(key=x), A(key=a), B(key=b), C(key=c)]
```

React matches children by key: `a`, `b`, `c` are recognized as the **same items** (just moved), `x` is new. It inserts one DOM node and leaves the rest alone, and each component keeps its own state and DOM.

```text
React sees: x is new → insert; a, b, c unchanged
```

### How the keyed diff works (conceptually)

1. Walk old and new lists while keys match in the same order (the common case: nothing moved).
2. When a mismatch occurs, build a **map of the remaining old children by key**.
3. For each remaining new child, look it up by key: reuse the matching fiber (and mark a move if needed), or create a new one.
4. Anything left in the map is deleted.

Moves are detected cheaply with a "last placed index" heuristic, which is efficient for common cases (appends, deletes, a few moves) and weaker for heavy reorders. Details aren't something you need to memorize, just that lookup is by key.

### What makes a good key

```tsx
{items.map((item) => <Row key={item.id} item={item} />)}           // ✓ stable, unique, tied to the data
{items.map((item, i) => <Row key={i} item={item} />)}              // ✗ position again: same problem as no keys on reorder
{items.map((item) => <Row key={Math.random()} item={item} />)}     // ✗✗ new identity every render → remount every time
```

- Keys must be **unique among siblings** (not globally) and **stable** across renders.
- Index keys are acceptable only for **static lists** that never reorder, insert, or delete, and whose items have no state.
- Keys aren't passed as props (`props.key` is `undefined`). Pass an `id` prop if the child needs it.

The index-key bug in the wild: a list of inputs where you delete the first row, and the *next* row's typed text appears to "move up" or the wrong row disappears, because React reused fibers by position and each fiber's input state/DOM value stayed put while the data shifted. See [lists and keys](../01-fundamentals/05-lists-and-keys.md).

## Component identity: the inner-component trap

Type comparison uses **reference equality of the function**. If you define a component **inside** another component, it's a new function on every render, so React sees a *different type* each time:

```tsx
function Parent() {
  const Child = () => <input />          // new function identity on every render of Parent
  return <Child />                        // different type every time → unmount + mount → input loses focus and state
}
```

Every re-render of `Parent` destroys and recreates `Child`, which is the reconciliation rule applied literally. Define components at module level ([rendering performance](../14-performance/01-rendering-performance.md#fix-4-keep-component-identity-stable)).

## What reconciliation does *not* do

- **It doesn't skip rendering.** Reconciliation compares outputs after a component has rendered. *Skipping* a component's render entirely is a **bailout** (same props reference and no pending update; `React.memo` is a shallow props check), which happens before diffing that subtree ([fiber architecture](./00-fiber-architecture.md#fibers-explain-behaviors-youve-seen)).
- **It doesn't move components between parents.** An element can't "move" to a different parent while keeping state; it'll be destroyed in one place and created in another.
- **It doesn't compare deeply by value.** Props are compared by reference for bailouts, and the diff only looks at element type, key, and (for host elements) attributes.
- **It doesn't guarantee minimal DOM mutations** across reorders. It's a heuristic that's good in practice.

## Host element diffing

For DOM elements, once the type matches, React compares props and updates only what changed:

- Attributes and properties: set or remove if different.
- `style`: updates only changed properties.
- Event handlers: updated on the fiber (React doesn't add/remove DOM listeners per render, see [04](./04-event-system.md)).
- Children: recursed into with the same algorithm.

**Controlled inputs** are a special case: React sets the `value` property to keep the DOM in sync with state, which is why typing without an `onChange` handler "doesn't work" ([controlled and uncontrolled inputs](../06-forms/00-controlled-and-uncontrolled-inputs.md)).

## Is the "virtual DOM" why React is fast?

Not really, and the phrase is a bit misleading. Diffing isn't free, and comparing trees is *extra* work over updating the DOM directly. What React offers is a **declarative model with acceptable performance by default**: you describe the UI, and it finds the DOM changes for you. Real-world speed comes from:

- Batching updates and applying DOM changes in one commit.
- Bailouts and memoization that skip whole subtrees ([memoization](../14-performance/02-memoization.md)).
- Scheduling and interruptibility ([03](./03-scheduler-and-lanes.md)).
- **You** structuring state well so less re-renders ([rendering performance](../14-performance/01-rendering-performance.md)).

## Practical consequences

| You want… | Because of… | Do |
|---|---|---|
| State to reset when switching records | Identity = type + position + key | Give the component a `key` of the record ID |
| State to persist across a conditional layout change | Same type at same position | Keep structure and types stable |
| Reordering/inserting/deleting list items to work | Keyed diff | Use stable, unique IDs as keys |
| An input not to lose focus on re-render | Same-type update keeps the node | Don't define components inside components; don't change keys or wrappers |
| To avoid remounting expensive subtrees | Type/position change destroys it | Keep the wrapper types and structure constant across renders |

## Common mistakes

- **Index keys** on lists that reorder, filter, insert, or delete, causing state and DOM values attached to the wrong rows.
- **`Math.random()` or unstable generated keys**, remounting every render.
- **Components defined inside components**, remounting every render.
- **Changing wrapper element types** (or adding/removing a wrapper) and accidentally resetting state.
- **Syncing state to props with effects** when a `key` change would reset it more simply.
- **Duplicate keys among siblings**, producing warnings and unpredictable updates.
- **Passing `key` as if it were a prop** (reading `props.key`).
- **Assuming a re-render means a DOM update**, or that a "wasted render" touched the DOM.
- **Treating the virtual DOM as the reason for performance** instead of structuring state and renders well.

## Quick summary

- Reconciliation diffs the new render output against the previous fibers using two heuristics: **different type = different tree**, and **keys identify list children**.
- **Same type at the same position → update** (state kept). **Different type or position → unmount and remount** (state lost).
- State belongs to a **position in the tree**; change the `key` to reset it deliberately.
- In lists, **stable unique keys** let React match, move, and reuse items; index keys fail on reorder and mutation, random keys remount everything.
- Components defined inside components get a new identity every render and remount.
- Rendering ≠ DOM update: reconcile first, commit only the diff. Bailouts skip rendering; reconciliation compares results.

## Next

[02 — How hooks work](./02-how-hooks-work.md)
