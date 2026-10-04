# Controlled and Uncontrolled Inputs

React gives you two ways to handle form input. In a **controlled** input, React state is the source of truth for the value. In an **uncontrolled** input, the DOM keeps the value and you read it when you need it. Neither is "right" — each fits different problems, and form libraries are built on the second one. This file covers both and how to choose.

## Prerequisites

[`../01-fundamentals/07-forms-basics.md`](../01-fundamentals/07-forms-basics.md), [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md), and [`../04-typescript-with-react/01-typing-events.md`](../04-typescript-with-react/01-typing-events.md)

---

## Controlled inputs

The input's displayed value comes from state; every change flows through React:

```tsx
function NameField() {
  const [name, setName] = useState("");

  return (
    <input
      value={name}
      onChange={(e) => setName(e.target.value)}
    />
  );
}
```

The loop: user types → `onChange` fires → state updates → React re-renders → the input shows the new state. React's `onChange` fires on **every keystroke** (like the native `input` event).

### What controlled inputs make easy

- **Live reactions**: character counters, formatting as you type, enabling a button as soon as input is valid.
- **Transforming input**: force uppercase, strip non-digits, mask phone numbers.
- **Dependent UI**: show or hide fields based on another field's value.
- **Resetting or setting values programmatically**: `setName("")`.
- **Syncing the value** with other state, the URL, or a store.

```tsx
<input
  value={code}
  onChange={(e) => setCode(e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, ""))}
  maxLength={8}
/>
```

### The costs

- **A re-render per keystroke** of the component that owns the state (and its children). Usually fine; with huge forms it adds up ([`../14-performance/01-rendering-performance.md`](../14-performance/01-rendering-performance.md)).
- **Boilerplate**: one state variable and handler per field, unless you use an object or a library.

---

## Uncontrolled inputs

React renders the input once and doesn't track its value; the DOM does. You read it on demand.

### With `FormData` (usually the best way)

```tsx
function SignupForm() {
  function handleSubmit(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const data = new FormData(e.currentTarget);
    const email = data.get("email");
    const plan = data.get("plan");
    // values are string | File | null; narrow before use
    if (typeof email !== "string" || typeof plan !== "string") return;
    save({ email, plan });
  }

  return (
    <form onSubmit={handleSubmit}>
      <input name="email" type="email" defaultValue="" required />
      <select name="plan" defaultValue="free">
        <option value="free">Free</option>
        <option value="pro">Pro</option>
      </select>
      <button>Sign up</button>
    </form>
  );
}
```

Requirements: every input needs a `name`, and you set the initial value with `defaultValue` (or `defaultChecked` for checkboxes and radios), **not** `value`.

### With a ref

```tsx
function SearchBox() {
  const inputRef = useRef<HTMLInputElement>(null);

  function handleSubmit(e: FormEvent) {
    e.preventDefault();
    search(inputRef.current?.value ?? "");
  }

  return (
    <form onSubmit={handleSubmit}>
      <input ref={inputRef} defaultValue="" />
      <button>Search</button>
    </form>
  );
}
```

Useful for a single field, or for imperative actions (`inputRef.current?.focus()`).

### What uncontrolled inputs make easy

- **No re-renders while typing** — the component doesn't re-run per keystroke.
- **Less code** for simple forms; plain `<form>` behavior (Enter to submit, browser validation) just works.
- **Integrating non-React code** that owns the DOM.
- **File inputs**, which are always uncontrolled (see below).

### The costs

- You **can't react to the value as it changes** without adding listeners.
- Setting or resetting the value programmatically is awkward (`form.reset()` or a ref).
- Live validation and dependent fields need extra wiring.

---

## Comparison

| | Controlled | Uncontrolled |
|---|-----------|--------------|
| Source of truth | React state | The DOM |
| Initial value prop | `value` | `defaultValue` / `defaultChecked` |
| Read the value | From state, anytime | On demand (`FormData`, ref) |
| Re-renders per keystroke | **Yes** | No |
| Live validation / formatting | Easy | Needs listeners |
| Programmatic set/reset | `setValue(...)` | `form.reset()`, refs |
| Boilerplate | More | Less |
| Good for | Interactive, dependent, transformed inputs | Simple forms, large forms, file inputs, libraries |

---

## Choosing

Ask what the form needs to do **while the user types**:

- **Nothing — just read values on submit** → uncontrolled with `FormData`.
- **Show live feedback, format, or depend on other fields** → controlled (or a library that watches only those fields).
- **A form with many fields and validation** → use **React Hook Form** ([`02-react-hook-form.md`](./02-react-hook-form.md)): it's uncontrolled by default (fast, few re-renders) and gives you controlled-style features where you ask for them.
- **A reusable input component** → support both modes ([`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md)).

---

## Rules that apply to both

### Don't switch modes

An input that starts with `value={undefined}` (uncontrolled) and later receives a defined value becomes controlled, which triggers a warning. Initialize controlled inputs with `""`, never `undefined`:

```tsx
const [name, setName] = useState("");        // ✅
const [name, setName] = useState<string>();  // ❌ undefined → "ada" flips modes
```

### `value` without `onChange` is read-only

React warns, and the field won't accept typing. Use `readOnly` if that's intended, or `defaultValue` for an editable uncontrolled input.

### Checkboxes and radios use `checked`

```tsx
<input type="checkbox" checked={agreed} onChange={(e) => setAgreed(e.target.checked)} />
<input type="checkbox" defaultChecked />    {/* uncontrolled */}
```

### Number and date inputs return strings

```tsx
const [age, setAge] = useState("");          // keep the string while typing
const ageNumber = age === "" ? null : Number(age);
```

Storing a number directly makes it hard to represent an empty field or "1." mid-typing.

### Resetting an uncontrolled form

```tsx
formRef.current?.reset();       // resets inputs to their defaultValue
```

Or give the form a `key` and change it to remount everything with fresh defaults ([`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md)):

```tsx
<ProfileForm key={user.id} user={user} />   // fresh uncontrolled inputs per user
```

### Many fields: one object, one handler

```tsx
const [form, setForm] = useState({ name: "", email: "" });

function handleChange(e: ChangeEvent<HTMLInputElement>) {
  const { name, value } = e.target;
  setForm((prev) => ({ ...prev, [name]: value }));
}
```

(See [`../01-fundamentals/07-forms-basics.md`](../01-fundamentals/07-forms-basics.md) and [`../04-typescript-with-react/01-typing-events.md`](../04-typescript-with-react/01-typing-events.md).)

---

## File inputs are always uncontrolled

For security reasons, a file input's value can't be set from JavaScript, so there's no `value` prop for `type="file"`. Read `e.target.files` or `FormData`. Full coverage in [`06-file-upload.md`](./06-file-upload.md).

---

## Controlled/uncontrolled hybrids

A form library can give you uncontrolled performance with controlled conveniences. React Hook Form `register` attaches to the DOM input (uncontrolled), and `watch` or `Controller` selectively subscribe where you need live values ([`02-react-hook-form.md`](./02-react-hook-form.md)). You can also mix: most fields uncontrolled, one controlled field that needs live formatting.

---

## Common mistakes

- **Initializing controlled state as `undefined`** — causes the controlled/uncontrolled warning.
- **Using `value` on an uncontrolled input** — it becomes controlled; use `defaultValue`.
- **Using `value` without `onChange`** — a read-only field with a console warning.
- **Forgetting `name` attributes** — `FormData` skips unnamed inputs.
- **Using `checked` incorrectly**: `value` on a checkbox, or reading `e.target.value` instead of `e.target.checked`.
- **Storing numbers in state for number inputs** — can't represent an empty or partial value.
- **Controlling every field in a huge form** — a re-render per keystroke across the form; use a library.
- **Trying to set a file input's value** — not allowed; reset it with a `key` or `form.reset()`.
- **Assuming `FormData.get` returns a string** — it can be `File` or `null`; narrow first.

## Quick summary

- Controlled: React state owns the value (`value` + `onChange`); best for live feedback and dependent UI
- Uncontrolled: the DOM owns it (`defaultValue` + `FormData` or ref); best for simple, large, or file forms
- Never start a controlled input as `undefined`; never mix `value` with `defaultValue`
- Number and date inputs yield strings; file inputs are always uncontrolled
- Large forms: use React Hook Form for uncontrolled performance with controlled features where needed

## Next

**[`01-form-validation.md`](./01-form-validation.md)** covers checking input: native rules, hand-written validation, and schema validation with Zod.
