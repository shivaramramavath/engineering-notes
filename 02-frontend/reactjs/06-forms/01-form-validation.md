# Form Validation

Validation answers two questions: **is this input acceptable?** and **how do we tell the user?** This file covers the layers of validation (native browser, hand-written, schema-based), when to show errors, validating across fields and asynchronously, and the rule that client validation is a convenience while the server is the authority.

## Prerequisites

[`00-controlled-and-uncontrolled-inputs.md`](./00-controlled-and-uncontrolled-inputs.md) and [`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md) (schemas and `z.infer`)

---

## Client validation is for users; server validation is for safety

Anyone can bypass your front-end (DevTools, `curl`). **Always validate on the server.** Client-side validation exists to give fast, friendly feedback and to avoid pointless requests.

| | Client | Server |
|---|--------|--------|
| Purpose | UX: instant feedback | Correctness and security |
| Can it be skipped by a user? | Yes | No |
| Required? | Optional (but expected) | **Always** |

Ideally both sides share the same **schema** (possible when the back end is also TypeScript), so rules never drift apart. See [`../19-production/05-security.md`](../19-production/05-security.md).

---

## Layer 1: native browser validation

HTML attributes give you free validation, messages, and accessibility semantics:

```tsx
<form onSubmit={handleSubmit}>
  <input type="email" name="email" required />
  <input type="password" name="password" minLength={8} required />
  <input type="number" name="age" min={18} max={120} />
  <input name="zip" pattern="[0-9]{5}" title="Five digits" />
  <button>Submit</button>
</form>
```

Attributes: `required`, `type` (`email`, `url`, `number`…), `min`/`max`, `minLength`/`maxLength`, `pattern`.

The browser blocks submission and shows a message **before** `onSubmit` fires. Useful extras:

```tsx
form.checkValidity();               // true/false
form.reportValidity();              // true/false and shows messages
input.setCustomValidity("Bad!");    // custom message; empty string clears it
input.validity.valueMissing;        // inspect why it's invalid
```

**Limits:** browser messages differ across browsers and are hard to style; rules are limited; cross-field and async rules aren't supported. Many teams add `noValidate` to the `<form>` and take over with their own UI while still using the same attributes for semantics:

```tsx
<form noValidate onSubmit={handleSubmit}>…</form>
```

---

## Layer 2: hand-written validation

A validation function takes the values and returns an errors object:

```tsx
type Values = { email: string; password: string };
type Errors = Partial<Record<keyof Values, string>>;

function validate(values: Values): Errors {
  const errors: Errors = {};

  if (!values.email) errors.email = "Email is required";
  else if (!/^\S+@\S+\.\S+$/.test(values.email)) errors.email = "Enter a valid email";

  if (values.password.length < 8) errors.password = "Use at least 8 characters";

  return errors;
}
```

Wire it into a form. Errors are **derived** from the values, not stored in state ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)); what you store is *which fields the user has touched* and whether they've tried to submit:

```tsx
function SignupForm() {
  const [values, setValues] = useState<Values>({ email: "", password: "" });
  const [touched, setTouched] = useState<Partial<Record<keyof Values, boolean>>>({});
  const [submitted, setSubmitted] = useState(false);

  const errors = validate(values);                         // derived each render
  const show = (field: keyof Values) =>
    (touched[field] || submitted) && errors[field];        // when to display

  function handleSubmit(e: FormEvent) {
    e.preventDefault();
    setSubmitted(true);
    if (Object.keys(errors).length > 0) return;
    save(values);
  }

  return (
    <form noValidate onSubmit={handleSubmit}>
      <label htmlFor="email">Email</label>
      <input
        id="email"
        value={values.email}
        onChange={(e) => setValues((v) => ({ ...v, email: e.target.value }))}
        onBlur={() => setTouched((t) => ({ ...t, email: true }))}
        aria-invalid={Boolean(show("email"))}
        aria-describedby={show("email") ? "email-error" : undefined}
      />
      {show("email") && <p id="email-error" role="alert">{errors.email}</p>}
      {/* password field omitted */}
      <button>Sign up</button>
    </form>
  );
}
```

This works, but it scales poorly (touched tracking, async, arrays). It's still worth building once to understand what libraries do for you.

---

## Layer 3: schema validation with Zod

A **schema** describes valid data declaratively and doubles as the source of the TypeScript type:

```tsx
import { z } from "zod";

const signupSchema = z
  .object({
    email: z.string().min(1, "Email is required").email("Enter a valid email"),
    password: z.string().min(8, "Use at least 8 characters"),
    confirmPassword: z.string(),
    age: z.coerce.number().int().min(18, "You must be 18 or older"),
    terms: z.literal(true, { errorMap: () => ({ message: "You must accept the terms" }) }),
  })
  .refine((data) => data.password === data.confirmPassword, {
    message: "Passwords don't match",
    path: ["confirmPassword"],      // attach the error to this field
  });

type SignupValues = z.infer<typeof signupSchema>;
```

Notes:

- **`z.infer`** gives you the type — no duplicate interface to maintain.
- **`z.coerce.number()`** converts the string from a number input into a number.
- **`.refine`** handles **cross-field rules**; `path` decides where the error shows.
- Zod's API evolves between major versions (for example, newer versions provide top-level helpers like `z.email()` and a different `errorMap` form); check the docs for the version you install. The ideas here carry across versions.

Using it by hand:

```tsx
const result = signupSchema.safeParse(values);

if (!result.success) {
  // result.error.issues: [{ path: ["email"], message: "…" }, …]
  const errors: Record<string, string> = {};
  for (const issue of result.error.issues) {
    const field = String(issue.path[0]);
    if (!(field in errors)) errors[field] = issue.message;   // first error per field
  }
  setErrors(errors);
  return;
}

save(result.data);   // typed as SignupValues
```

`safeParse` returns a result object instead of throwing. In practice you won't write this mapping yourself — React Hook Form's resolver does it ([`02-react-hook-form.md`](./02-react-hook-form.md)).

### One schema, many uses

- **Form validation** (here)
- **Types** via `z.infer`
- **Validating API responses** ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md))
- **Server-side validation** if the server also runs TypeScript

---

## When to show errors

Showing errors too early is hostile (a red message after typing one character); too late is frustrating.

| Strategy | Behavior | Notes |
|----------|----------|-------|
| **On submit** | Validate and show errors when the user submits | Simple, least annoying; errors appear all at once |
| **On blur** | Validate when a field loses focus | Good default: feedback after the user finishes a field |
| **On change** | Validate on every keystroke | Good for password strength and availability hints; avoid for "required" and format errors |
| **Hybrid ("reward early, punish late")** | First validate on blur/submit; **after** a field has shown an error, re-validate on change so the error clears as soon as it's fixed | Often the best UX |

React Hook Form's `mode` and `reValidateMode` implement these ([`02-react-hook-form.md`](./02-react-hook-form.md)).

Other UX guidance:

- Say **what's wrong and how to fix it** ("Use at least 8 characters"), not "Invalid".
- Don't validate untouched, empty fields until submit.
- Keep the message next to the field and keep it from shifting the layout (reserve space).
- Mark required fields; don't rely on color alone.

---

## Accessible errors

Validation messages are useless if screen readers can't find them:

```tsx
<label htmlFor="email">Email</label>
<input
  id="email"
  aria-invalid={Boolean(error)}
  aria-describedby={error ? "email-error" : undefined}
/>
{error && <p id="email-error" role="alert">{error}</p>}
```

- `aria-invalid` flags the field; `aria-describedby` connects it to the message.
- `role="alert"` (or an `aria-live` region) announces new errors.
- On failed submit, **move focus to the first invalid field** or to an error summary.

More in [`05-form-submission-and-errors.md`](./05-form-submission-and-errors.md) and [`../08-accessibility/`](../08-accessibility/README.md).

---

## Cross-field validation

Rules that involve several fields: matching passwords, date ranges (`end` after `start`), "provide either phone or email", totals that must add up. Put them in the **schema** (`.refine` / `.superRefine`) rather than in per-field checks, and use `path` to place the message:

```tsx
const rangeSchema = z
  .object({ start: z.coerce.date(), end: z.coerce.date() })
  .refine((d) => d.end >= d.start, { message: "End must be after start", path: ["end"] });
```

---

## Async validation

Some checks need the server ("is this username taken?"):

```tsx
async function isUsernameAvailable(name: string): Promise<boolean> {
  const res = await fetch(`/api/usernames/${encodeURIComponent(name)}`);
  return res.ok;
}
```

Guidelines:

- **Debounce** calls ([`../03-hooks/10-hook-recipes.md`](../03-hooks/10-hook-recipes.md)) and **cancel** stale requests (`AbortController`), or an old response can overwrite a new one ([`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md)).
- Treat it as a **hint**: the server must still enforce uniqueness at submit (race conditions: two people can pick the same name).
- Show a pending state ("Checking…") so users aren't confused by silence.
- Don't run it for values that fail cheap sync checks first.

---

## What to do with `null`, empty strings, and whitespace

- Inputs give `""`, not `undefined` — schemas should treat empty as missing (`min(1, "Required")`).
- `.trim()` before validating text fields users can fill with spaces.
- Optional fields: decide on one representation (`undefined` or `""`) and transform consistently (`z.string().optional().or(z.literal(""))`).

---

## Common mistakes

- **Trusting client validation** — the server must validate again.
- **Storing derived errors in state** — compute them from values; store only touched and submitted flags.
- **Validating on every keystroke from the first character** — noisy; use blur/submit, then re-validate on change.
- **Vague messages** ("Invalid input") — say what's expected.
- **Cross-field rules attached to the wrong field** — use `.refine` with a `path`.
- **Forgetting `aria-invalid`/`aria-describedby`/`role="alert"`** — errors invisible to assistive tech.
- **Async validation without debounce or cancellation** — floods the server and shows stale results.
- **Defining a TypeScript type and a schema separately** — they drift; use `z.infer`.
- **Treating `type="number"` values as numbers** — they're strings; coerce.

## Quick summary

- Validate on the client for UX, on the server for security — always both
- Layers: native attributes, hand-written functions, schema validation (Zod)
- Derive errors from values; store only touched/submitted state
- Show errors on blur or submit, and re-validate on change once an error appeared
- Put cross-field rules in the schema with a `path`; debounce and cancel async checks
- Make errors accessible: `aria-invalid`, `aria-describedby`, `role="alert"`, focus management

## Next

**[`02-react-hook-form.md`](./02-react-hook-form.md)** wires schemas like these into forms with far less code and fewer re-renders.
