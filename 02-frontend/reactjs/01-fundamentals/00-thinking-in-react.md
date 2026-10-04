# Thinking in React

React isn't hard because of its API; it's hard because it asks you to think about UIs differently. Instead of writing instructions that change the page step by step, you describe **what the UI should look like for a given set of data**, and React works out the changes. This file builds that mindset and gives a repeatable process for turning a design into components.

## Prerequisites

[`../00-setup/`](../00-setup/README.md) — a running project.

---

## UI as a function of state

The core idea:

```
UI = f(state)
```

Given the same data, a component always produces the same UI. You don't reach into the DOM to hide a button; you change the data, and the component describes the new result.

**Imperative** (what you do *not* do in React):

```js
button.disabled = true;
spinner.style.display = "block";
```

**Declarative** (what you do):

```jsx
<button disabled={isLoading}>Save</button>
{isLoading && <Spinner />}
```

You describe each visual state; React handles the transitions between them.

---

## The five-step process

Imagine a searchable product table: a search box, a "only show in stock" checkbox, and a table grouped by category.

### 1. Break the UI into a component hierarchy

Draw boxes around parts of the design and name them. A useful guide: **one component, one responsibility**.

```
FilterableProductTable
├── SearchBar
└── ProductTable
    ├── ProductCategoryRow
    └── ProductRow
```

Rules of thumb for where to cut: follow your data model's structure, follow the design's visual sections, and split when a piece would be reused or gets hard to read.

### 2. Build a static version first

Render the UI from hard-coded data using props only. No state, no interactivity. It's a lot of typing and little thinking, which is why you do it separately from the hard part.

```jsx
function ProductRow({ product }) {
  return (
    <tr>
      <td style={{ color: product.stocked ? "inherit" : "red" }}>
        {product.name}
      </td>
      <td>{product.price}</td>
    </tr>
  );
}
```

### 3. Find the minimal state

State is the smallest set of data that changes over time and that the UI can't compute from other data. For each piece of data, ask:

- Does it change over time? If not, it's not state.
- Is it passed in from a parent via props? Then it's not state here.
- Can you compute it from existing state or props? Then it's **not** state — derive it.

In the example, state is the search text and the checkbox value. The filtered product list is **derived**, so it isn't stored.

### 4. Decide where state lives

Find every component that renders something based on that state, then find their closest common parent. State goes there. Here, `SearchBar` shows the text and `ProductTable` filters by it, so both live under `FilterableProductTable`, which owns the state. This is called **lifting state up** (see `../02-state-and-rendering/02-state-structure-and-lifting.md`).

### 5. Add inverse data flow

Data flows **down** through props. To let a child change parent state, pass a function down:

```jsx
function FilterableProductTable({ products }) {
  const [query, setQuery] = useState("");

  return (
    <>
      <SearchBar query={query} onQueryChange={setQuery} />
      <ProductTable products={products} query={query} />
    </>
  );
}
```

`SearchBar` calls `onQueryChange` when the user types; the parent updates; React re-renders everything that depends on `query`.

---

## Key principles

- **Data flows one way**, from parent to child. This makes bugs traceable: to find where a value came from, look up the tree.
- **Components are pure during rendering** — same props and state in, same JSX out, with no side effects while rendering.
- **Keep state minimal** and derive everything else.
- **Small, focused components** are easier to read, test, and reuse.

---

## Common mistakes

- **Storing derived data in state** — a filtered list or a total price stored in state falls out of sync. Compute it during render.
- **Duplicating props into state** — the copy won't update when the prop changes.
- **Putting state too low** — siblings that need the same data can't share it; lift it to the common parent.
- **Putting all state at the top** — every component re-renders and props get passed through many layers; keep state close to where it's used.
- **Skipping the static step** — mixing layout and interactivity makes both harder.

## Quick summary

- Describe the UI for each state; let React handle the updates
- Process: split into components → build static → find minimal state → place state → add inverse data flow
- State is only what can't be computed from other data
- Data flows down via props; changes flow up via function props
- Prefer small, single-purpose components

## Next

**[`01-jsx.md`](./01-jsx.md)** covers the syntax you use to write these components.
