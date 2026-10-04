# Components and Props

A component is a JavaScript function that returns JSX. Props are the inputs you pass to it. Together they're how you split a UI into reusable pieces and move data between them.

## Prerequisites

[`01-jsx.md`](./01-jsx.md)

---

## Defining a component

```jsx
function Greeting() {
  return <h1>Hello!</h1>;
}

export default Greeting;
```

Requirements:

- The name starts with a **capital letter**.
- It **returns** JSX (or `null` to render nothing).
- It's defined at the **top level** of a module — never inside another component, or it's recreated on every render and loses its state.

Use it like an HTML tag:

```jsx
function App() {
  return (
    <>
      <Greeting />
      <Greeting />
    </>
  );
}
```

Each use creates an independent instance.

---

## Props

Props are passed as attributes and received as a single object:

```jsx
function Greeting({ name, excited = false }) {
  return <h1>Hello, {name}{excited ? "!" : "."}</h1>;
}

<Greeting name="Ada" excited />
```

- **Destructure** in the parameter list for readability.
- Provide **default values** with `=` in the destructuring (applies when the prop is `undefined`).
- A bare attribute like `excited` means `excited={true}`.

### Passing non-string values

Use braces for anything that isn't a plain string:

```jsx
<Profile
  user={{ name: "Ada", role: "admin" }}
  tags={["math", "computing"]}
  age={36}
  onSelect={handleSelect}
/>
```

### Passing functions as props

Props can be functions, which is how children communicate **up** to parents:

```jsx
function Counter({ onIncrement }) {
  return <button onClick={onIncrement}>+1</button>;
}
```

Naming convention: props that receive handlers start with `on` (`onSelect`, `onChange`); the handler functions you define start with `handle` (`handleSelect`).

---

## Props are read-only

A component must **never modify its own props**:

```jsx
function Bad({ user }) {
  user.name = "Changed"; // ❌ mutating props
}
```

If something needs to change, it's **state** — owned by this component or by a parent that passes new props down.

---

## One-way data flow

Data moves **down** the tree: parent → child. A child can't reach up and change a parent's data directly; it calls a function the parent gave it.

```
App (owns user)
 └─ Header (receives user)
     └─ Avatar (receives user.avatarUrl)
```

This predictability is the point: to find where a value comes from, look at the parent.

---

## Components should be pure

During rendering, a component should behave like a math function: **same props (and state) in, same JSX out**, with no side effects.

```jsx
// ❌ Impure: depends on and changes something outside the component
let guestCount = 0;
function Guest() {
  guestCount += 1;
  return <p>Guest #{guestCount}</p>;
}

// ✅ Pure: everything it needs comes from props
function Guest({ number }) {
  return <p>Guest #{number}</p>;
}
```

Side effects (network requests, subscriptions) belong in event handlers or in effects — see [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md). Purity is also what lets React render components in any order, skip renders, and (with StrictMode) detect bugs by rendering twice.

---

## Props vs state

| | Props | State |
|---|-------|-------|
| Owned by | The parent | The component itself |
| Can the component change it? | No (read-only) | Yes, via its setter |
| Purpose | Configure a component | Remember data that changes over time |

State is covered in [`../02-state-and-rendering/00-state-and-snapshots.md`](../02-state-and-rendering/00-state-and-snapshots.md).

---

## Organizing components

- **One component per file** is the usual convention for anything non-trivial; tiny helper components used only by one parent can live in the same file.
- Use a **default export** for the main component of a file, or **named exports** consistently across a project (named exports make renames and auto-imports more reliable).
- Extract a new component when a chunk of JSX has its own responsibility, is repeated, or makes the parent hard to read.

Typing props with TypeScript: [`../04-typescript-with-react/00-typing-components-and-props.md`](../04-typescript-with-react/00-typing-components-and-props.md).

---

## Common mistakes

- **Lowercase component name** — `<greeting />` is treated as an HTML tag.
- **Defining a component inside another component** — it's re-created every render, resetting its state and hurting performance.
- **Mutating props** — treat them as immutable.
- **Copying a prop into state** — the state won't follow later prop changes; use the prop directly or derive from it.
- **Calling a component like a function** (`Greeting()`) — render it as `<Greeting />` so React can manage it.
- **Forgetting braces for non-strings** — `count="5"` passes a string, `count={5}` passes a number.

## Quick summary

- A component is a capitalized function that returns JSX
- Props are inputs, received as one object; destructure and set defaults
- Props are read-only; data flows down, and functions passed as props send events up
- Keep rendering pure: same inputs, same output, no side effects
- Define components at module top level, never inside other components

## Next

**[`03-children-and-composition.md`](./03-children-and-composition.md)** shows how components can wrap and contain other content.
