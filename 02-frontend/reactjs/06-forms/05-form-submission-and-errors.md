# Form Submission and Errors

Validation passing is only the start. Submitting a form means a network request that can be slow, fail, be rejected by the server, or be triggered twice. This file covers the whole submission lifecycle: pending state, double-submit protection, server-side errors mapped onto fields, failure handling, success, accessibility, unsaved-changes warnings, and how React 19 form Actions fit in.

## Prerequisites

[`02-react-hook-form.md`](./02-react-hook-form.md) and [`01-form-validation.md`](./01-form-validation.md)

---

## The submission lifecycle

```
idle → validating → submitting (pending) → success | server error | network error
```

Design each state deliberately:

| State | What the user needs to see |
|-------|----------------------------|
| **Pending** | Feedback that something is happening; the submit button can't be pressed again |
| **Field errors** | The message next to the field, and focus on the first one |
| **Form-level error** | A message that isn't tied to a field (wrong password, server down) |
| **Success** | Confirmation, a redirect, or a reset form — never silence |

---

## 1. Pending state and double-submit protection

With React Hook Form, `isSubmitting` is `true` while your async submit function runs:

```tsx
const { handleSubmit, formState: { isSubmitting } } = useForm<FormValues>();

async function onSubmit(values: FormValues) {
  await api.createUser(values);   // must be awaited, or isSubmitting ends immediately
}

<button type="submit" disabled={isSubmitting} aria-busy={isSubmitting}>
  {isSubmitting ? "Saving…" : "Save"}
</button>
```

Guidelines:

- **Disable the submit button** while pending, but also guard in code; disabled buttons don't stop Enter-key submits in every case, and slow networks invite double-clicks. RHF's `handleSubmit` already ignores re-submissions while the previous one is in flight in most setups, but treat the server as the final authority.
- **Keep the button's size stable** — changing the label length shouldn't shift the layout.
- **Don't clear the form on submit.** Keep the user's data until you know it succeeded.
- **Make non-idempotent requests safe to retry** with an **idempotency key**, which the server uses to ignore duplicates (important for payments and orders).
- Disabling the form's inputs during submit (`<fieldset disabled>`) prevents edits mid-request but also removes focus; use it deliberately.

---

## 2. Server-side validation errors

The server must validate ([`01-form-validation.md`](./01-form-validation.md)), and it can find errors the client can't (duplicate email, expired coupon). A good API returns **field-level** errors in a predictable shape:

```json
// HTTP 422 (or 400)
{
  "errors": {
    "email": "That email is already registered",
    "password": "Password is too common"
  }
}
```

Map them onto the form with `setError`:

```tsx
async function onSubmit(values: FormValues) {
  try {
    await api.signup(values);
    navigate("/welcome");
  } catch (err) {
    if (err instanceof ApiError && err.status === 422) {
      for (const [field, message] of Object.entries(err.fieldErrors)) {
        setError(field as keyof FormValues, { type: "server", message });
      }
      return;   // handled; don't rethrow
    }
    setError("root.serverError", {
      type: "server",
      message: "Something went wrong. Please try again.",
    });
  }
}
```

- `setError(field, { type: "server", message })` makes the error appear exactly where client-side errors do — same UI, same accessibility wiring.
- **`root.*` errors** (for example, `root.serverError`) hold **form-level** messages not tied to any one field; render with `errors.root?.serverError?.message`. Check the RHF docs for the supported form of this API in your version.
- Server errors **clear when the field is next validated** (RHF removes manually-set errors on the next change/validate for that field in most setups), so users aren't stuck with a stale message after editing.
- **Don't leak internals.** Show friendly messages, not stack traces. For authentication, avoid revealing whether an email exists if that's a privacy concern ("Invalid email or password").

### Mapping unknown error shapes safely

Treat the response as untrusted input and validate it (a small Zod schema for the error body works well) rather than assuming its shape ([`../04-typescript-with-react/05-advanced-typing-patterns.md`](../04-typescript-with-react/05-advanced-typing-patterns.md)). The API client layer is the right place to normalize errors into one shape ([`../11-api-integration/05-api-error-handling.md`](../11-api-integration/05-api-error-handling.md)).

---

## 3. Network and unexpected failures

When the request itself fails (offline, timeout, 5xx):

- **Keep the user's input.** Never reset on failure.
- Show a form-level message with a clear next step ("Check your connection and try again") and keep the submit button enabled so they can retry.
- Distinguish **client mistakes** (4xx — fix your input) from **server problems** (5xx — try later).
- For transient failures, retrying is fine **only if the request is idempotent** (or has an idempotency key).
- Consider showing a toast for non-blocking failures and an inline message for blocking ones ([`../09-ui-components/08-toasts-and-notifications.md`](../09-ui-components/08-toasts-and-notifications.md)).

---

## 4. Success handling

Pick one deliberately:

| Outcome | When |
|---------|------|
| **Redirect** to the next place | Sign-ups, checkouts, creating a record with its own page |
| **Show a confirmation message** and keep the form or hide it | Contact forms, feedback |
| **Reset the form** (`reset()`) and keep going | Repeated data entry (adding items in a list) |
| **Update the UI optimistically** | Instant-feel edits; roll back on failure ([`../12-server-state/06-optimistic-updates.md`](../12-server-state/06-optimistic-updates.md)) |

```tsx
async function onSubmit(values: FormValues) {
  await api.createTodo(values);
  reset();                       // back to defaultValues
  setFocus("title");             // keep keyboard users in flow
  toast.success("Todo added");
}
```

After a successful save of an **edit form**, call `reset(savedValues)` so the new data becomes the baseline, and `isDirty` turns false.

If the mutation updates data shown elsewhere (lists, caches), invalidate or update it with your data layer ([`../12-server-state/05-mutations.md`](../12-server-state/05-mutations.md)).

---

## 5. Accessible error handling

Errors only help if everyone perceives them:

- **Focus the first invalid field** on failed submit. RHF does this by default (`shouldFocusError: true`).
- **Per-field messages** are connected with `aria-describedby`, and the field has `aria-invalid="true"` ([`01-form-validation.md`](./01-form-validation.md)).
- **Announce form-level errors**: put them in an element with `role="alert"` (or `aria-live="assertive"`), rendered at the top of the form or near the submit button:

```tsx
{errors.root?.serverError && (
  <div role="alert" className="form-error">
    {errors.root.serverError.message}
  </div>
)}
```

- For long forms, add an **error summary** at the top listing each problem with links to the fields. Move focus to the summary on failed submit.
- Don't rely on **color alone**; pair it with text and/or an icon.
- Announce success too (a polite live region like `role="status"`).

More in [`../08-accessibility/03-screen-readers.md`](../08-accessibility/03-screen-readers.md) and [`../08-accessibility/04-accessibility-checklist.md`](../08-accessibility/04-accessibility-checklist.md).

---

## 6. Unsaved changes

Warn users before they lose work, **only when the form is dirty**:

```tsx
const { formState: { isDirty } } = useForm<FormValues>();

// 1) Browser tab close / refresh / external navigation
useEffect(() => {
  if (!isDirty) return;
  function handler(e: BeforeUnloadEvent) {
    e.preventDefault();       // browsers show their own generic prompt
  }
  window.addEventListener("beforeunload", handler);
  return () => window.removeEventListener("beforeunload", handler);
}, [isDirty]);

// 2) In-app navigation: use your router's blocker (React Router: useBlocker)
```

Browsers ignore custom messages for `beforeunload`. For client-side routing, use your router's navigation-blocking API ([`../10-routing/03-navigation.md`](../10-routing/03-navigation.md)). Don't nag when nothing changed, and clear the dirty state after a successful save.

---

## 7. React 19 form Actions

React 19 lets a `<form>` take a function as its `action`, with built-in pending state hooks:

```tsx
import { useActionState } from "react";
import { useFormStatus } from "react-dom";

function SubmitButton() {
  const { pending } = useFormStatus();          // reads the parent <form>'s status
  return <button disabled={pending}>{pending ? "Saving…" : "Save"}</button>;
}

function NewsletterForm() {
  const [state, formAction] = useActionState(
    async (_prev: { error?: string; ok?: boolean }, formData: FormData) => {
      const email = formData.get("email");
      if (typeof email !== "string" || !email.includes("@")) {
        return { error: "Enter a valid email" };
      }
      await api.subscribe(email);
      return { ok: true };
    },
    {}
  );

  return (
    <form action={formAction}>
      <input name="email" type="email" required />
      <SubmitButton />
      {state.error && <p role="alert">{state.error}</p>}
      {state.ok && <p role="status">Subscribed!</p>}
    </form>
  );
}
```

How it differs from the RHF approach:

- Built into React: no library, uncontrolled inputs read via `FormData`.
- The action runs inside a transition, so pending state comes free ([`../15-concurrent-and-modern-react/01-transitions.md`](../15-concurrent-and-modern-react/01-transitions.md)).
- **Uncontrolled forms are reset automatically** after a successful action in current React versions; check the behavior and options in the docs, since you may want to preserve values on error.
- With server frameworks, the action can be a **server function**, which enables progressive enhancement (the form works before JavaScript loads).
- `useOptimistic` pairs with actions for instant UI ([`../15-concurrent-and-modern-react/05-react-19-features.md`](../15-concurrent-and-modern-react/05-react-19-features.md)).

Use Actions for simple forms and server-centric apps; use RHF + Zod for forms with rich client validation, dynamic fields, and complex state. They can be combined: RHF `handleSubmit` can call an action.

---

## 8. Security notes

- **CSRF:** state-changing requests need protection (SameSite cookies, CSRF tokens, or token-based auth) ([`../19-production/05-security.md`](../19-production/05-security.md)).
- **Rate limit and bot-protect** public forms (sign-up, contact) on the server.
- **Never trust the client**: re-validate everything and authorize the action server-side.
- **Don't put secrets in hidden fields** — they're visible and editable.

---

## Common mistakes

- **Not awaiting the async submit** — `isSubmitting` ends early and the button re-enables mid-request.
- **Clearing the form before success is confirmed** — the user loses everything on failure.
- **Only handling the happy path** — no UI for network errors or 5xx responses.
- **Showing server errors in a toast only** — field-specific errors belong at the field.
- **Assuming the server's error shape** — validate it, and normalize in the API layer.
- **No double-submit protection or idempotency** — duplicate orders and payments.
- **Not announcing errors** to assistive technology — missing `role="alert"` and focus management.
- **Warning about unsaved changes when nothing changed**, or never clearing the dirty state after save.
- **Leaking sensitive details** in error messages ("no account with that email").
- **Forgetting to update or invalidate related cached data** after a successful mutation.

## Quick summary

- Handle each state: pending, field errors, form-level errors, success
- `await` the submit function so `isSubmitting` is accurate; disable the button and use idempotency keys for risky requests
- Map server validation errors onto fields with `setError`; use `root` errors for form-level messages
- Keep input on failure; distinguish 4xx from 5xx; retry only idempotent requests
- Focus the first error, wire `aria-describedby`, and announce messages with `role="alert"`
- Warn on unsaved changes only when dirty; clear after save
- React 19 Actions (`useActionState`, `useFormStatus`) offer a built-in path for simple, server-centric forms

## Next

**[`06-file-upload.md`](./06-file-upload.md)** covers the one input type that's always uncontrolled: files.
