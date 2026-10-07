# Forms

Forms are the main way to call a Server Action. React extends the HTML `<form>` so its `action` prop accepts a function, and that function receives the form's `FormData`.

> Verified against the Next.js 16.4 docs and the React 19 form APIs.

## The basic form

```tsx
// app/invoices/new/page.tsx
import { createInvoice } from "../actions";

export default function NewInvoicePage() {
  return (
    <form action={createInvoice}>
      <label htmlFor="customer">Customer</label>
      <input id="customer" name="customerId" required />

      <label htmlFor="amount">Amount</label>
      <input id="amount" name="amount" type="number" step="0.01" required />

      <button type="submit">Create</button>
    </form>
  );
}
```

```ts
// app/invoices/actions.ts
"use server";

export async function createInvoice(formData: FormData) {
  const customerId = formData.get("customerId");
  const amount = formData.get("amount");
  // validate → write → revalidate (see 02-validation.md)
}
```

Rules that trip people up:

- Fields are matched by **`name`**, not `id`. An input without `name` is not in `FormData`.
- `formData.get()` returns `string | File | null`. Numbers arrive as **strings**. Never assume the type.
- This form works in a **Server Component** and, because it is a real HTML form, it submits even before JavaScript loads or with JavaScript disabled (progressive enhancement). In a Client Component, submissions made before hydration are queued, and the page hydrates with priority.

For many fields, `Object.fromEntries(formData)` is convenient, but the object also contains extra properties prefixed with `$ACTION_`. Pick fields explicitly or validate with a schema that strips unknown keys.

## Passing extra arguments with `bind`

When the action needs data that is not an input (an ID, say), bind it:

```tsx
import { updateUser } from "./actions";

export function UserProfile({ userId }: { userId: string }) {
  const updateUserWithId = updateUser.bind(null, userId);

  return (
    <form action={updateUserWithId}>
      <input name="name" />
      <button type="submit">Save</button>
    </form>
  );
}
```

```ts
"use server";

export async function updateUser(userId: string, formData: FormData) {
  // bound args come first, FormData last
}
```

`bind` works in Server and Client Components and keeps progressive enhancement. The alternative is `<input type="hidden" name="userId" value={userId} />`, but the value then sits in the rendered HTML and the user can edit it.

Either way, a bound ID is **client-controlled**. Re-check authorization on the server; do not assume the ID is the current user's.

## Pending state

Disable the button and show progress while the action runs. Two options.

### `useActionState`: pending plus returned state

```tsx
"use client";

import { useActionState } from "react";
import { createUser } from "@/app/actions";

const initialState = { message: "" };

export function Signup() {
  const [state, formAction, pending] = useActionState(createUser, initialState);

  return (
    <form action={formAction}>
      <input name="email" type="email" required />
      <p aria-live="polite">{state.message}</p>
      <button disabled={pending}>Sign up</button>
    </form>
  );
}
```

When you use `useActionState`, the action receives the **previous state as its first argument**:

```ts
"use server";

export async function createUser(prevState: { message: string }, formData: FormData) {
  // ...
  return { message: "Check your inbox" };
}
```

### `useFormStatus`: pending in a nested component

`useFormStatus` reads the status of the **parent `<form>`**, so it must be used in a component rendered *inside* the form:

```tsx
"use client";

import { useFormStatus } from "react-dom";

export function SubmitButton({ children }: { children: React.ReactNode }) {
  const { pending } = useFormStatus();
  return (
    <button type="submit" disabled={pending}>
      {pending ? "Saving…" : children}
    </button>
  );
}
```

```tsx
<form action={createUser}>
  {/* fields */}
  <SubmitButton>Sign up</SubmitButton>
</form>
```

Calling `useFormStatus` in the same component that renders the `<form>` does not work, because that component is not *inside* the form. That is why the button is a separate component.

| Need | Use |
|---|---|
| Pending flag and returned errors in one component | `useActionState` |
| A reusable submit button anywhere inside a form | `useFormStatus` |

## Multiple actions in one form

Buttons can carry their own action through `formAction`:

```tsx
<form action={publishPost}>
  <textarea name="body" />
  <button type="submit" formAction={saveDraft}>Save draft</button>
  <button type="submit">Publish</button>
</form>
```

The button with `formAction` calls `saveDraft`; the default submit calls `publishPost`. Both receive the full form's `FormData`.

## Submitting programmatically

Use `requestSubmit()` (not `submit()`) so the submission goes through React's form handling:

```tsx
"use client";

export function Entry() {
  function onKeyDown(e: React.KeyboardEvent<HTMLTextAreaElement>) {
    if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
      e.preventDefault();
      e.currentTarget.form?.requestSubmit();
    }
  }
  return <textarea name="entry" required onKeyDown={onKeyDown} />;
}
```

## Redirecting after submit

Call `redirect` from the action. It throws, so anything after it does not run. Revalidate first.

```ts
"use server";

import { revalidatePath } from "next/cache";
import { redirect } from "next/navigation";

export async function createPost(formData: FormData) {
  // ...write...
  revalidatePath("/posts");
  redirect("/posts");
}
```

In a Server Action, `redirect` pushes a new history entry and performs a client-side navigation when JavaScript is available. For a form submitted without JavaScript it serves a `303`. Do not call `redirect` inside a `try` block; see [Action Errors](./04-action-errors.md).

## File uploads

A file input arrives as a `File` in `FormData`. The request body is capped at **1 MB** by default; raise `serverActions.bodySizeLimit` for bigger files, or have the browser upload straight to object storage and send only the resulting key to the action.

```tsx
<form action={uploadAvatar}>
  <input type="file" name="avatar" accept="image/*" />
  <button>Upload</button>
</form>
```

```ts
"use server";

export async function uploadAvatar(formData: FormData) {
  const file = formData.get("avatar");
  if (!(file instanceof File) || file.size === 0) return;
  // validate type and size on the server before storing
}
```

## Preserving input after an error

React resets a form's uncontrolled fields after its action completes. If validation fails, the user loses what they typed. Return the submitted values from the action and feed them back as `defaultValue`:

```tsx
<input name="email" defaultValue={state.values?.email} />
```

The full pattern is in [Validation](./02-validation.md).

## Client-side validation still helps

HTML attributes (`required`, `type="email"`, `minLength`, `pattern`) give instant feedback with no JavaScript. Treat them as a convenience. The server check is the one that counts.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Input has no `name` | Field missing from `FormData` | Add `name` |
| Treating `formData.get("amount")` as a number | `"10" + 5 === "105"` | Convert and validate |
| `useFormStatus` in the component that renders `<form>` | `pending` is always `false` | Move it into a child component |
| Forgetting the `prevState` parameter with `useActionState` | `formData` is the previous state, `.get` is undefined | Signature is `(prevState, formData)` |
| Fields clear after a validation error | Lost user input | Return values and use `defaultValue` |
| Hidden `userId` field trusted by the action | Users edit it in DevTools | Use the session |
| `form.submit()` to trigger the action | Bypasses React's handling | Use `requestSubmit()` |
| `redirect()` before `revalidatePath()` | Destination shows stale data | Revalidate first |
| Large file upload rejected | Request over 1 MB | Raise `bodySizeLimit` or upload to storage directly |

## Quick Summary

- `<form action={fn}>` passes `FormData` to the function; fields are matched by `name` and values are strings or files.
- Use `bind` for extra arguments, but still verify them server side.
- `useActionState` gives `[state, formAction, pending]` and changes the action signature to `(prevState, formData)`.
- `useFormStatus` works only inside a child of the form.
- `formAction` on buttons lets one form call several actions.
- Revalidate, then `redirect`; keep `redirect` out of `try` blocks.

## Next

- [Validation](./02-validation.md)
- [Mutations and Optimistic UI](./03-mutations-and-optimistic-ui.md)
- [Client Components](../03-components/01-client-components.md)
