# JavaScript for React

React is "just JavaScript," which means most confusion early on isn't about React at all — it's about JavaScript features that React code leans on in nearly every line. This file covers exactly those, each tied to how it shows up in components.

## Prerequisites

Basic JavaScript: variables, functions, objects, arrays, `if`/loops. If those are unfamiliar, learn them first and come back.

---

## `const`, `let`, and arrow functions

Use `const` by default, `let` only when you'll reassign, and never `var`. Components and handlers are almost always arrow functions or function declarations:

```js
const double = (n) => n * 2;

function Greeting({ name }) {
  return <h1>Hello, {name}</h1>;
}
```

An arrow function with no braces **returns its expression implicitly**. With braces you need an explicit `return`.

---

## Destructuring

Pulls values out of objects and arrays. React uses it for props and for hooks:

```js
// Object destructuring — props
function Card({ title, subtitle = "No subtitle" }) {}

// Array destructuring — useState returns [value, setter]
const [count, setCount] = useState(0);

// Renaming
const { data: users, isLoading } = useQuery(/* ... */);
```

Array destructuring is **positional** (names are up to you); object destructuring is **by key** (names must match unless you rename).

---

## Spread and rest

```js
// Spread: copy and extend without mutating
const user = { name: "Ada", role: "admin" };
const updated = { ...user, role: "owner" };

const items = [1, 2, 3];
const more = [...items, 4];

// Rest: collect the remaining properties
function Button({ children, ...rest }) {
  return <button {...rest}>{children}</button>;
}
```

Spread is how you **update state without mutating it** — a rule you'll meet in `02-state-and-rendering/01-state-updates-and-batching.md`.

---

## Template literals, optional chaining, nullish coalescing

```js
const label = `Hello, ${user.name}`;

const city = user.address?.city; // undefined instead of throwing
const pageSize = options.pageSize ?? 20; // only falls back on null/undefined
```

`??` differs from `||`: `0 || 20` gives `20`, but `0 ?? 20` gives `0`. That matters for counts, prices, and flags.

---

## Array methods

Rendering lists means transforming arrays. Learn these four well:

```js
const users = [
  { id: 1, name: "Ada", active: true },
  { id: 2, name: "Linus", active: false },
];

users.map((u) => u.name); // ["Ada", "Linus"]
users.filter((u) => u.active); // [{ id: 1, ... }]
users.find((u) => u.id === 2); // { id: 2, ... }
users.reduce((sum, u) => sum + u.id, 0); // 3
```

`map`, `filter`, and `find` are **non-mutating** — they return new values. Avoid `push`, `splice`, and `sort` on state arrays (`sort` mutates; copy first with `[...arr].sort()`).

---

## Modules: `import` and `export`

```js
// math.js
export const add = (a, b) => a + b; // named export
export default function multiply(a, b) {
  return a * b;
} // default export

// app.js
import multiply, { add } from "./math.js";
```

Components are typically a **default export per file** (or named exports in larger codebases). Mixing them up is a very common source of "undefined component" errors.

---

## Closures

A function remembers the variables from where it was created:

```js
function makeCounter() {
  let count = 0;
  return () => ++count;
}
const next = makeCounter();
next(); // 1
next(); // 2
```

In React, every render creates new functions that close over **that render's** props and state. This is why event handlers and effects can show "stale" values — covered in `02-state-and-rendering/00-state-and-snapshots.md`.

---

## Promises and `async`/`await`

```js
async function loadUser(id) {
  try {
    const res = await fetch(`/api/users/${id}`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.error(err);
    throw err;
  }
}
```

`fetch` does **not** reject on HTTP errors like 404 or 500 — you must check `res.ok` yourself.

---

## Truthiness

Falsy values: `false`, `0`, `""`, `null`, `undefined`, `NaN`. Everything else is truthy — including `"0"`, `[]`, and `{}`.

This is behind a classic React bug: `{count && <Badge />}` renders a literal `0` when `count` is `0`. See `01-fundamentals/04-conditional-rendering.md`.

---

## Common mistakes

- **Mutating instead of copying** — `state.push(x)` or `obj.field = y` on state; use spread, `map`, `filter`.
- **Using `||` where `??` was meant** — silently replaces `0` and `""`.
- **Forgetting `return` in an arrow function with braces** — the function returns `undefined`.
- **Assuming `fetch` throws on 404/500** — it only throws on network failure.
- **Mixing default and named imports** — `import Button from` vs `import { Button } from`.

## Quick summary

- Default to `const`; use arrow functions for handlers and small helpers
- Destructuring and spread are everywhere in props, hooks, and state updates
- `map`, `filter`, `find` render and transform lists without mutating
- Closures explain most "stale value" surprises later on
- Check `res.ok` after `fetch`; use `??` rather than `||` for defaults

## Next

**[`01-node-and-npm.md`](./01-node-and-npm.md)** gets Node.js and npm installed so you can create a project.
