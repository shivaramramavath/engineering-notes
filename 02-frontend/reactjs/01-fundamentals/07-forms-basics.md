# Forms Basics

Forms are where events, state, and conditional rendering meet. This file covers the essential pattern — the **controlled input** — plus form submission. It's an introduction; validation, libraries, and larger forms are in [`../06-forms/README.md`](../06-forms/README.md).

## Prerequisites

[`06-events.md`](./06-events.md). This file uses `useState` in its simplest form: `const [value, setValue] = useState(initial)` gives you a value and a function to change it. Details in [`../03-hooks/01-useState.md`](../03-hooks/01-useState.md).

---

## Controlled inputs

In a **controlled input**, React state is the single source of truth for the input's value:

```jsx
import { useState } from "react";

function NameField() {
  const [name, setName] = useState("");

  return (
    <label>
      Name
      <input value={name} onChange={(e) => setName(e.target.value)} />
      <p>Hello, {name || "stranger"}</p>
    </label>
  );
}
```

The loop:

1. The input displays `value={name}`.
2. The user types; `onChange` fires.
3. The handler calls `setName(e.target.value)`.
4. React re-renders, and the input shows the new value.

If you set `value` without `onChange`, the input becomes read-only. If you want an input that starts with a value but isn't controlled, use `defaultValue` (see [`../06-forms/00-controlled-and-uncontrolled-inputs.md`](../06-forms/00-controlled-and-uncontrolled-inputs.md)).

---

## Other input types

Different inputs use different props:

```jsx
// textarea — also uses value
<textarea value={bio} onChange={(e) => setBio(e.target.value)} />

// select — value on the <select>, not on <option>
<select value={role} onChange={(e) => setRole(e.target.value)}>
  <option value="user">User</option>
  <option value="admin">Admin</option>
</select>

// checkbox — uses `checked`, and reads e.target.checked
<input
  type="checkbox"
  checked={agreed}
  onChange={(e) => setAgreed(e.target.checked)}
/>

// radio — compare each option to the current value
<input
  type="radio"
  name="plan"
  value="pro"
  checked={plan === "pro"}
  onChange={(e) => setPlan(e.target.value)}
/>
```

Numeric inputs still give you **strings** in `e.target.value`; convert with `Number(...)` when you need a number.

---

## Handling submission

Use `onSubmit` on the `<form>` (not `onClick` on the button) so pressing Enter works too, and call `preventDefault` to stop the page reload:

```jsx
function SignupForm() {
  const [email, setEmail] = useState("");
  const [agreed, setAgreed] = useState(false);

  function handleSubmit(e) {
    e.preventDefault();
    console.log({ email, agreed });
  }

  return (
    <form onSubmit={handleSubmit}>
      <label>
        Email
        <input
          type="email"
          value={email}
          onChange={(e) => setEmail(e.target.value)}
          required
        />
      </label>

      <label>
        <input
          type="checkbox"
          checked={agreed}
          onChange={(e) => setAgreed(e.target.checked)}
        />
        I agree to the terms
      </label>

      <button type="submit" disabled={!agreed}>Sign up</button>
    </form>
  );
}
```

Notes:

- `disabled={!agreed}` is derived from state — no extra state needed.
- Browser validation attributes (`required`, `type="email"`, `minLength`) work for free and run before `onSubmit` fires.

---

## Many fields, one state object

With several inputs, keep them in one object and update by field name:

```jsx
const [form, setForm] = useState({ name: "", email: "", role: "user" });

function handleChange(e) {
  const { name, value } = e.target;
  setForm((prev) => ({ ...prev, [name]: value }));
}

<input name="name"  value={form.name}  onChange={handleChange} />
<input name="email" value={form.email} onChange={handleChange} />
<select name="role" value={form.role} onChange={handleChange}>...</select>
```

- Each input's `name` matches a key in the state object.
- `[name]: value` is a **computed property** that updates just that field.
- `(prev) => ({ ...prev, ... })` copies the old object instead of mutating it. This is the functional update form, explained in [`../02-state-and-rendering/01-state-updates-and-batching.md`](../02-state-and-rendering/01-state-updates-and-batching.md). Checkboxes need `e.target.checked` instead of `value`.

---

## Resetting a form

Set state back to the initial values after a successful submit:

```jsx
const initialForm = { name: "", email: "", role: "user" };

function handleSubmit(e) {
  e.preventDefault();
  save(form);
  setForm(initialForm);
}
```

---

## Always label your inputs

Every input needs an accessible name. Wrap it in a `<label>` or connect them with `htmlFor` and `id`:

```jsx
<label htmlFor="email">Email</label>
<input id="email" type="email" />
```

Placeholders are **not** labels — they disappear on typing and are unreliable for screen readers. Details in [`../08-accessibility/04-accessibility-checklist.md`](../08-accessibility/04-accessibility-checklist.md).

---

## When to reach for more

This pattern works for small forms. For validation rules, error messages, many fields, dynamic fields, or performance on large forms, move on to [`../06-forms/README.md`](../06-forms/README.md), which covers validation, React Hook Form, and submission handling.

---

## Common mistakes

- **`value` without `onChange`** — the input can't be edited and React warns.
- **Switching between `undefined` and a string** — initialize state with `""`, not `undefined`, or React warns about switching from uncontrolled to controlled.
- **Using `value` on a checkbox** — use `checked`.
- **Forgetting `e.preventDefault()`** — the page reloads on submit.
- **Mutating the form state object** (`form.name = x`) — always create a new object.
- **Handling submit on the button's `onClick`** — Enter-key submission breaks; use `onSubmit` on the form.
- **Treating number input values as numbers** — they're strings.

## Quick summary

- A controlled input sets `value` from state and updates it in `onChange`
- Checkboxes use `checked`; selects and textareas use `value`
- Handle submission with `onSubmit` and `preventDefault`
- Keep related fields in one state object, updating immutably by `name`
- Label every input; go to `06-forms` for validation and larger forms

## Next

You've finished the fundamentals. Continue to **[`../02-state-and-rendering/README.md`](../02-state-and-rendering/README.md)** to learn exactly how state works and how React decides when to re-render.
