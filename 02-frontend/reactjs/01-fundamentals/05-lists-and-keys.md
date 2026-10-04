# Lists and Keys

Most UIs render collections: messages, products, search results. In React you turn an array of data into an array of elements with `map`, and give each element a **key** so React can track items as the list changes.

## Prerequisites

[`02-components-and-props.md`](./02-components-and-props.md), and `map`/`filter` from [`../00-setup/00-javascript-for-react.md`](../00-setup/00-javascript-for-react.md).

---

## Rendering a list with `map`

```jsx
const people = [
  { id: 1, name: "Ada" },
  { id: 2, name: "Linus" },
  { id: 3, name: "Grace" },
];

function PersonList() {
  return (
    <ul>
      {people.map((person) => (
        <li key={person.id}>{person.name}</li>
      ))}
    </ul>
  );
}
```

React renders the array of `<li>` elements in order. Every element produced by `map` needs a `key`.

---

## What keys are for

When a list changes — items added, removed, or reordered — React must work out which new element corresponds to which old one. Keys give each item a stable identity:

- With stable keys, React **moves** existing elements and preserves their state and DOM.
- Without them (or with bad keys), React may reuse the wrong element for the wrong item, causing stale input values, lost focus, wrong animations, or subtle bugs.

Keys are **not** passed to your component as a prop. If a component needs the id, pass it separately (`<Item key={item.id} id={item.id} />`).

---

## Choosing good keys

A good key is **unique among siblings** and **stable over time**.

| Source | Use as key? |
|--------|-------------|
| Database id, UUID, slug | ✅ Best choice |
| A unique, immutable field (email, SKU) | ✅ Usually fine |
| Id generated **when the item is created** (e.g., `crypto.randomUUID()` on add) | ✅ Fine |
| Array index | ⚠️ Only for static lists that never reorder, insert, or delete |
| `Math.random()` / new id on each render | ❌ Never |

```jsx
// ❌ A new key every render — React remounts every item every time
{items.map((item) => <Row key={Math.random()} item={item} />)}

// ❌ Index keys on a list the user can reorder or delete from
{items.map((item, i) => <Row key={i} item={item} />)}
```

**Why index keys break:** delete the first item and every later item's index shifts. React then thinks item 2 became item 1 and reuses its state — so an `<input>` the user typed into can end up showing the wrong row's text.

Index keys are acceptable when the list is static and never reordered or filtered.

---

## Where the key goes

Put the key on the **outermost element returned by `map`**, not on something nested inside the component:

```jsx
{items.map((item) => (
  <Item key={item.id} item={item} />   // ✅ on the element in the map
))}

function Item({ item }) {
  return <li key={item.id}>...</li>;   // ❌ has no effect here
}
```

When each item renders multiple sibling elements, use a keyed fragment:

```jsx
import { Fragment } from "react";

{items.map((item) => (
  <Fragment key={item.id}>
    <dt>{item.term}</dt>
    <dd>{item.definition}</dd>
  </Fragment>
))}
```

(The short `<>` syntax can't take a key.)

---

## Filtering and sorting

Transform the array before rendering. Don't mutate it:

```jsx
const visible = products
  .filter((p) => p.inStock)             // returns a new array
  .sort((a, b) => a.price - b.price);   // safe: sorts the new array, not `products`
```

`sort` mutates the array it's called on, so calling it directly on props or state is a bug. Sorting the result of `filter` is safe because that result is a new array. When sorting the original array, **copy first**:

```jsx
const sorted = [...products].sort((a, b) => a.price - b.price);
```

Filtered and sorted lists are **derived data** — compute them during render rather than storing them in state (see [`00-thinking-in-react.md`](./00-thinking-in-react.md)).

---

## Empty lists

Always decide what an empty list looks like:

```jsx
function ProductList({ products }) {
  if (products.length === 0) return <p>No products found.</p>;

  return (
    <ul>
      {products.map((p) => <li key={p.id}>{p.name}</li>)}
    </ul>
  );
}
```

---

## Keys can also reset state

Changing a component's key tells React it's a **different** component, so it unmounts the old one and mounts a fresh one with new state. This is a deliberate technique for resetting state — covered in [`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md).

---

## Long lists

Rendering thousands of rows is slow regardless of keys. Use pagination or windowing — see [`../14-performance/04-virtualization.md`](../14-performance/04-virtualization.md).

---

## Common mistakes

- **Missing keys** — React warns in the console; don't ignore the warning.
- **Index keys on dynamic lists** — causes state and input values to attach to the wrong item.
- **Random keys** — forces a remount of every item on every render.
- **Key on the wrong element** — it must be on the outermost element returned by `map`.
- **Mutating the array** (`push`, in-place `sort`) when it came from props or state.
- **Using non-unique keys** — duplicates among siblings cause unpredictable behavior.

## Quick summary

- Render lists with `map` and return one keyed element per item
- Keys must be unique among siblings and stable across renders
- Prefer database ids; use index only for static lists; never use random values
- Put the key on the outermost element in the `map`; use `<Fragment key>` for multi-element items
- Copy before sorting, derive filtered/sorted lists instead of storing them

## Next

**[`06-events.md`](./06-events.md)** covers responding to clicks, typing, and other user input.
