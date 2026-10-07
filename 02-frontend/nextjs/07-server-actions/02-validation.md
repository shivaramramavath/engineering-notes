# Validation

Every Server Action is a public endpoint, so its input is untrusted: `FormData` fields, bound arguments, and any object passed from a Client Component. Validate on the server, always. Client-side validation is a convenience for the user, not a guarantee for you.

> Verified against the Next.js 16.4 docs. Examples use Zod 4; notes mark the Zod 3 differences.

## Where validation happens

| Layer | Tool | Purpose | Trustworthy? |
|---|---|---|---|
| Browser | `required`, `type="email"`, `minLength`, `pattern` | Instant feedback, no JS | No, easily bypassed |
| Client Component | Optional JS checks | Better UX | No |
| **Server Action** | Schema library (Zod, Valibot) | The real check | **Yes** |

Validation answers "is this input well-formed?". It does not answer "is this user allowed to do this?". Authorization is a separate step. A perfectly valid ID can still belong to someone else.

## The standard pattern

1. Define a schema once.
2. `safeParse` the input.
3. On failure, **return** field errors (do not throw). On success, continue.

```ts
// app/signup/actions.ts
"use server";

import { z } from "zod";

const SignupSchema = z.object({
  email: z.email("Enter a valid email"),
  password: z.string().min(8, "At least 8 characters"),
});

export type SignupState = {
  errors?: { email?: string[]; password?: string[] };
  message?: string;
  values?: { email?: string };
};

export async function signup(
  _prev: SignupState,
  formData: FormData,
): Promise<SignupState> {
  const parsed = SignupSchema.safeParse({
    email: formData.get("email"),
    password: formData.get("password"),
  });

  if (!parsed.success) {
    return {
      errors: z.flattenError(parsed.error).fieldErrors,
      values: { email: String(formData.get("email") ?? "") }, // never echo the password
    };
  }

  const { email, password } = parsed.data; // typed and validated
  // ...create the user...
  return { message: "Account created" };
}
```

> **Zod 3:** use `z.string().email(...)` and `parsed.error.flatten().fieldErrors`. Zod 4 deprecates the method form in favor of `z.email()` and `z.flattenError(error)`.

Why return instead of throw: a validation failure is an **expected** outcome the user can fix. Returned data reaches `useActionState` and renders inline. Thrown errors go to an error boundary. See [Action Errors](./04-action-errors.md).

## Showing the errors

```tsx
"use client";

import { useActionState } from "react";
import { signup, type SignupState } from "./actions";

const initial: SignupState = {};

export function SignupForm() {
  const [state, formAction, pending] = useActionState(signup, initial);

  return (
    <form action={formAction} noValidate>
      <label htmlFor="email">Email</label>
      <input
        id="email"
        name="email"
        type="email"
        defaultValue={state.values?.email}
        aria-invalid={!!state.errors?.email}
        aria-describedby="email-error"
      />
      <p id="email-error" aria-live="polite">{state.errors?.email?.[0]}</p>

      <label htmlFor="password">Password</label>
      <input id="password" name="password" type="password" aria-invalid={!!state.errors?.password} />
      <p aria-live="polite">{state.errors?.password?.[0]}</p>

      <button disabled={pending}>{pending ? "Creating…" : "Sign up"}</button>
      <p aria-live="polite">{state.message}</p>
    </form>
  );
}
```

- `defaultValue={state.values?.email}` restores what the user typed, because React clears uncontrolled fields after an action finishes.
- `aria-live="polite"` makes screen readers announce new errors.
- `noValidate` turns off the browser's built-in popups so your messages are the ones shown. Omit it if you want both.

## `FormData` gotchas

`FormData` is stringly typed. A schema needs to handle that.

| Input | What arrives | Handle with |
|---|---|---|
| Text field left empty | `""` (not `null`) | `z.string().min(1)` |
| Missing field | `null` | Schema rejects non-strings |
| Number input | `"42"` | `z.coerce.number()` |
| Checkbox checked | `"on"` (or its `value`) | `z.literal("on")` or `.transform(v => v === "on")` |
| Checkbox unchecked | **Field absent** | `.optional()` |
| Multiple values (`<select multiple>`, repeated names) | Use `formData.getAll("tags")` | `z.array(z.string())` |
| File input | `File` (empty `File` when none chosen) | Check `instanceof File` and `size` |
| Date input | `"2026-10-07"` | `z.coerce.date()` or validate the string format |

```ts
const PostSchema = z.object({
  title: z.string().trim().min(1, "Required").max(120),
  price: z.coerce.number().positive(),
  published: z.literal("on").optional().transform((v) => v === "on"),
  tags: z.array(z.string()).max(5),
});

const parsed = PostSchema.safeParse({
  title: formData.get("title"),
  price: formData.get("price"),
  published: formData.get("published") ?? undefined,
  tags: formData.getAll("tags"),
});
```

Caution with `z.coerce.number()`: `Number("")` is `0`, so an empty field passes as zero. Add `.positive()`/`.min()` or check for an empty string first.

Prefer picking fields explicitly over `Object.fromEntries(formData)`; the latter includes internal `$ACTION_*` keys.

## Validate non-form calls too

An action called with arguments (not `FormData`) still receives untrusted data, and TypeScript types vanish at runtime:

```ts
"use server";

const IdSchema = z.uuid();

export async function deletePost(rawId: unknown) {
  const session = await auth();
  if (!session?.user) throw new Error("Unauthorized");

  const id = IdSchema.parse(rawId); // throws if the caller sent something else
  await db.post.deleteMany({ where: { id, authorId: session.user.id } });
}
```

A `(id: string)` annotation does nothing against a hand-crafted `POST`.

## Order inside an action

```text
1. authenticate        → no session? stop
2. validate input      → invalid? return field errors
3. authorize           → may this user touch this row? (look up by id AND owner)
4. mutate
5. invalidate cache / redirect
```

Authentication goes first so unauthenticated callers cannot probe your validation messages. Authorization needs data (the row), so it follows validation.

## Reuse the schema on the client

Export the schema from a shared, non-`"use server"` module and run it in the browser for fast feedback, then run it again on the server:

```ts
// lib/schemas/signup.ts  (no "use server")
export const SignupSchema = z.object({ /* ... */ });
```

A `"use server"` file can only export async functions, so keep schemas and types in a separate module. Importing the server-only parts of the file into a Client Component would also pull code you do not want in the bundle.

## Server-side errors that are not schema errors

Some rules need the database: "email already taken". Return them in the same shape:

```ts
const existing = await db.user.findUnique({ where: { email } });
if (existing) {
  return { errors: { email: ["That email is already registered"] }, values: { email } };
}
```

For uniqueness, also rely on a database unique constraint and handle its error. A pre-check alone is racy: two requests can both pass it.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Relying on `required` / `type="email"` | Bad data reaches the database via a hand-made `POST` | Validate on the server |
| `as string` casts on `formData.get` | `null` or a `File` slips through | Parse with a schema |
| Throwing on invalid input | User sees an error page instead of field errors | Return errors, throw only for unexpected failures |
| Echoing the password back in `values` | Secret in the response and HTML | Return only safe fields |
| Typing-only protection (`id: string`) | Arbitrary payloads accepted | Runtime-validate arguments |
| Validating but skipping authorization | Users edit others' rows | Look up by id **and** owner |
| `z.coerce.number()` on an empty field | Empty becomes `0` | Add `.positive()` or check emptiness |
| Exporting a schema from a `"use server"` file | Build error (only async functions allowed) | Put it in a separate module |
| Existence pre-check only | Duplicate rows under concurrency | Add a unique constraint and handle the error |

## Quick Summary

- Server validation is mandatory; browser validation is only UX.
- `safeParse`, then return `{ errors, values }` on failure and let `useActionState` render it.
- `FormData` is stringly typed: coerce numbers, handle absent checkboxes, use `getAll` for lists.
- Validate non-form arguments at runtime too.
- Validation checks shape; authorization checks ownership; do both.
- Keep schemas in a shared module, not a `"use server"` file.

## Next

- [Mutations and Optimistic UI](./03-mutations-and-optimistic-ui.md)
- [Action Errors](./04-action-errors.md)
- [Auth Architecture](../11-authentication/00-auth-architecture.md)
