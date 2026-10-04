# Children and Composition

Components often need to wrap other content — a card, a modal, a page layout. React handles this with the special `children` prop and with composition: building complex UIs by combining simple components rather than extending them.

## Prerequisites

[`02-components-and-props.md`](./02-components-and-props.md)

---

## The `children` prop

Whatever you put between a component's opening and closing tags arrives as `props.children`:

```jsx
function Card({ title, children }) {
  return (
    <section className="card">
      <h2>{title}</h2>
      <div className="card-body">{children}</div>
    </section>
  );
}

<Card title="Welcome">
  <p>Thanks for signing up.</p>
  <button>Get started</button>
</Card>
```

`Card` doesn't know or care what's inside. It provides the frame; the caller provides the content. This is **containment**.

`children` can be anything renderable: text, elements, arrays, even `null`.

---

## Layout components

The most common use of `children` is layout and visual wrappers:

```jsx
function PageLayout({ children }) {
  return (
    <div className="page">
      <Header />
      <main>{children}</main>
      <Footer />
    </div>
  );
}

<PageLayout>
  <Dashboard />
</PageLayout>
```

The same layout wraps any page, and the pages don't repeat header or footer markup.

---

## Multiple "slots" with named props

`children` is a single slot. When a component needs several distinct areas, pass elements through **named props**:

```jsx
function SplitPane({ left, right }) {
  return (
    <div className="split">
      <div className="split-left">{left}</div>
      <div className="split-right">{right}</div>
    </div>
  );
}

<SplitPane
  left={<Sidebar />}
  right={<Content />}
/>
```

JSX elements are just values, so they can be passed through any prop. You can combine both: named props for fixed regions, `children` for the main body.

---

## Specialization: a specific version of a general component

Instead of inheritance, create a specific component by **rendering a general one with particular props**:

```jsx
function Dialog({ title, message, children }) {
  return (
    <div role="dialog">
      <h1>{title}</h1>
      <p>{message}</p>
      {children}
    </div>
  );
}

function WelcomeDialog() {
  return (
    <Dialog title="Welcome" message="Thanks for visiting!">
      <button>Close</button>
    </Dialog>
  );
}
```

---

## Composition over inheritance

React doesn't use class inheritance to share UI behavior. Everything you'd do with inheritance — customization, reuse, specialization — is done with **props and composition**:

- Different content → `children` or named props
- Different behavior → function props
- Different look → props (variants) or CSS classes
- Shared logic (not UI) → custom hooks (see [`../03-hooks/09-custom-hooks.md`](../03-hooks/09-custom-hooks.md))

---

## Avoiding "prop drilling" with composition

Before reaching for global state, try passing **already-built elements** down. If a deep component needs `user`, you can create the element high in the tree and pass the finished element as a prop:

```jsx
// Instead of passing `user` through Layout → Header → Nav → Avatar
<Layout header={<Header avatar={<Avatar user={user} />} />} />
```

`Layout` and `Header` no longer need to know about `user` at all.

---

## Rendering `children` safely

- `children` may be **undefined** if nothing was passed. Rendering `{children}` is fine (it renders nothing).
- Don't mutate or inspect `children` directly; the `React.Children` utilities and `cloneElement` exist but are rarely needed and make components fragile. Prefer explicit props, context, or the patterns in [`../05-component-design/01-composition-patterns.md`](../05-component-design/01-composition-patterns.md) and [`../05-component-design/02-compound-components.md`](../05-component-design/02-compound-components.md).

TypeScript note: `children` is typed as `React.ReactNode` — see [`../04-typescript-with-react/00-typing-components-and-props.md`](../04-typescript-with-react/00-typing-components-and-props.md).

---

## Common mistakes

- **Forgetting to render `{children}`** — the content you passed silently disappears.
- **Passing content as a string prop when `children` fits better** — less flexible than JSX children.
- **Over-using `cloneElement` / `React.Children`** — creates implicit coupling between parent and children.
- **Creating a wrapper that accepts too many props** — if you keep adding props for each variation, consider slots or composition instead.
- **Prop drilling through many layers** — try passing elements first; use context if it's truly shared (see `../03-hooks/05-useContext.md`).

## Quick summary

- Content between a component's tags arrives as `children`
- Use `children` for one content slot and named props for multiple slots
- Build specific components by composing general ones with props
- React favors composition over inheritance for all UI reuse
- Passing finished elements down can avoid prop drilling

## Next

**[`04-conditional-rendering.md`](./04-conditional-rendering.md)** covers showing different UI depending on data.
