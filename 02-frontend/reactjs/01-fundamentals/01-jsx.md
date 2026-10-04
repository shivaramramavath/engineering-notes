# JSX

JSX is the HTML-like syntax you write inside JavaScript to describe UI. It isn't HTML and it isn't a template language — it's syntax that compiles into plain JavaScript function calls. Knowing that explains all of its rules.

## Prerequisites

[`00-thinking-in-react.md`](./00-thinking-in-react.md) and the destructuring and expression basics in [`../00-setup/00-javascript-for-react.md`](../00-setup/00-javascript-for-react.md).

---

## What JSX compiles to

```jsx
const element = <h1 className="title">Hello</h1>;
```

becomes roughly:

```js
const element = jsx("h1", { className: "title", children: "Hello" });
```

The result is a plain JavaScript object describing what to render (an "element"). Your build tool (Vite) does this transformation; you never write the `jsx(...)` calls yourself.

TSX is the same thing in TypeScript files. Typing details are in [`../04-typescript-with-react/00-typing-components-and-props.md`](../04-typescript-with-react/00-typing-components-and-props.md).

---

## The rules

### 1. Return a single root

A component returns one element. Wrap siblings in a parent or a **fragment**, which adds nothing to the DOM:

```jsx
return (
  <>
    <h1>Title</h1>
    <p>Body</p>
  </>
);
```

Use `<Fragment key={...}>` (imported from React) when a fragment needs a key.

### 2. Close every tag

Self-closing tags need the slash:

```jsx
<img src="logo.png" alt="Logo" />
<input type="text" />
```

### 3. Use camelCase for most attributes

| HTML | JSX |
|------|-----|
| `class` | `className` |
| `for` | `htmlFor` |
| `onclick` | `onClick` |
| `tabindex` | `tabIndex` |
| `stroke-width` (SVG) | `strokeWidth` |

`aria-*` and `data-*` attributes keep their hyphenated names.

---

## Expressions in curly braces

Curly braces let you drop any **JavaScript expression** into JSX:

```jsx
const user = { name: "Ada", age: 36 };

<h1>Hello, {user.name}</h1>
<p>Next year: {user.age + 1}</p>
<img src={user.avatar} alt={`${user.name}'s avatar`} />
```

You can use braces in two places: as **text content** (`<p>{value}</p>`) and as **attribute values** (`src={value}`). You cannot use statements (`if`, `for`) inside braces — use expressions like ternaries, or compute values before the `return`. See [`04-conditional-rendering.md`](./04-conditional-rendering.md) and [`05-lists-and-keys.md`](./05-lists-and-keys.md).

### Inline styles

`style` takes an **object**, with camelCase properties — hence double braces (braces for the expression, braces for the object):

```jsx
<div style={{ backgroundColor: "tomato", padding: 8 }} />
```

Numbers become pixels for most properties. Prefer CSS classes for anything beyond dynamic values.

### Comments

```jsx
<div>
  {/* a comment inside JSX */}
</div>
```

---

## What renders and what doesn't

| Value | Result |
|-------|--------|
| strings, numbers | Rendered as text |
| `true`, `false`, `null`, `undefined` | Render nothing |
| arrays | Each item rendered in order |
| objects (plain) | **Error** — "Objects are not valid as a React child" |

Notice `0` **does** render (it's a number), which matters for `&&` — see [`04-conditional-rendering.md`](./04-conditional-rendering.md).

---

## Components vs HTML elements

Names starting with a **lowercase** letter are treated as HTML elements; **uppercase** names are components:

```jsx
<button />     // DOM element
<Button />     // your component
```

Forgetting the capital letter is a common reason a component "renders nothing" or produces an unknown-tag warning.

---

## Spreading props

```jsx
const props = { id: "email", type: "email", required: true };
<input {...props} />
```

Handy for forwarding props, but use it deliberately — spreading everything makes it unclear what a component actually accepts.

---

## Escaping and security

React escapes values embedded in JSX before rendering them, which protects against most cross-site scripting (XSS) from text content:

```jsx
const input = "<img src=x onerror=alert(1)>";
<p>{input}</p>  {/* rendered as harmless text */}
```

The exception is `dangerouslySetInnerHTML`, which inserts raw HTML. Avoid it unless the HTML is sanitized. More in `../19-production/05-security.md`.

---

## Common mistakes

- **Using `class` instead of `className`** — React warns, and styling may not apply.
- **Returning multiple root elements** — wrap in a fragment.
- **Putting a statement in braces** — `{if (x) ...}` is a syntax error; use a ternary or compute beforehand.
- **Rendering an object** — `{user}` throws; render its fields (`{user.name}`).
- **`style="color: red"`** — style must be an object: `style={{ color: "red" }}`.
- **Lowercase component names** — `<card />` is treated as an HTML tag.

## Quick summary

- JSX compiles to JavaScript function calls that produce element objects
- One root per return; close all tags; use `className`, `htmlFor`, camelCase events
- `{}` embeds expressions, not statements
- Booleans, `null`, and `undefined` render nothing; plain objects are an error
- Capitalized names are components; React escapes embedded text by default

## Next

**[`02-components-and-props.md`](./02-components-and-props.md)** shows how to turn JSX into reusable components and pass data into them.
