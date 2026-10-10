# Actions and Forms

Server Actions are functions the client can call over the network, which makes their types a boundary: the arguments come from an untrusted browser, and the return value is serialized back. This note covers typing action signatures, form state with `useActionState`, input validated by a schema, and optimistic updates.

> Verified against the Next.js 16.4 Authentication and Data Security guides (which show typed `FormState`, `useActionState` and Server Actions) and React 19 types. Details of `@types/react` signatures can shift between versions; check yours.

## What it is

| Piece | Type shape |
|---|---|
| Action for `<form action={...}>` | `(formData: FormData) => void \| Promise<void>` |
| Action for `useActionState` | `(prevState: State, formData: FormData) => State \| Promise<State>` |
| Action called from an event handler | Any arguments that are serializable; returns a `Promise` of a serializable value |
| `FormData` entries | `FormDataEntryValue` = `string \| File` (`get` returns that or `null`) |

Two facts drive everything else:

1. **Anything an action receives is `unknown` in practice.** A direct POST can send any shape, whatever your TypeScript says. Validate at runtime ([Validation](../07-server-actions/02-validation.md)).
2. **Anything an action returns must be serializable** (plain data, `Date`, `bigint`, `Map`, `Set`). Do not return ORM rows, class instances or functions.

## A typed form action

```ts
// app/actions/posts.ts
"use server";

import * as z from "zod";
import { redirect } from "next/navigation";
import { verifySession } from "@/app/lib/dal";

const CreatePostSchema = z.object({
  title: z.string().trim().min(1, "Title is required").max(200),
  body: z.string().trim().max(10_000),
});

export type CreatePostState =
  | { errors?: { title?: string[]; body?: string[] }; message?: string }
  | undefined;

export async function createPost(
  prevState: CreatePostState,
  formData: FormData,
): Promise<CreatePostState> {
  await verifySession();                                           // authenticate first

  const parsed = CreatePostSchema.safeParse({
    title: formData.get("title"),                                  // FormDataEntryValue | null -> schema decides
    body: formData.get("body"),
  });
  if (!parsed.success) {
    return { errors: parsed.error.flatten().fieldErrors };         // typed per field
  }

  const post = await insertPost(parsed.data);                      // parsed.data is { title: string; body: string }
  redirect(`/posts/${post.id}`);                                   // returns `never`; keep outside try/catch
}
```

```tsx
// app/ui/post-form.tsx
"use client";

import { useActionState } from "react";
import { createPost } from "@/app/actions/posts";

export function PostForm() {
  const [state, action, pending] = useActionState(createPost, undefined);
  //     ^ CreatePostState     ^ (formData: FormData) => void     ^ boolean

  return (
    <form action={action}>
      <input name="title" />
      {state?.errors?.title && <p>{state.errors.title[0]}</p>}
      <textarea name="body" />
      {state?.errors?.body && <p>{state.errors.body[0]}</p>}
      {state?.message && <p role="alert">{state.message}</p>}
      <button disabled={pending}>Save</button>
    </form>
  );
}
```

Points:

- The docs' pattern: `type FormState = { errors?: {...}; message?: string } | undefined` and `useActionState(signup, undefined)`. The initial state's type is the state type, so `undefined` must be part of it.
- `useActionState` infers `state` from the **action's return type** and `action` from its parameters; you rarely write generics by hand.
- `redirect()` returns `never`, so a path that ends in `redirect(...)` satisfies a `Promise<State>` return type without returning anything.
- With `.flatten().fieldErrors`, error keys are the schema's keys. In Zod 4 `z.flattenError(error)` is the newer form of the same helper; check your Zod version.

## Result types: `ok` / `errors`

When actions are called from event handlers (not forms), a **discriminated union result** keeps call sites honest:

```ts
// app/lib/result.ts
export type ActionResult<T = void> =
  | { ok: true; data: T }
  | { ok: false; message: string; fieldErrors?: Record<string, string[]> };
```

```ts
"use server";

export async function renamePost(id: string, title: string): Promise<ActionResult<{ id: string }>> {
  const session = await verifySession();
  const parsed = z.string().trim().min(1).max(200).safeParse(title);
  if (!parsed.success) return { ok: false, message: "Invalid title" };

  const updated = await updateOwnedPost(id, session.userId, parsed.data);
  if (!updated) return { ok: false, message: "Not found" };

  revalidatePath("/posts");
  return { ok: true, data: { id } };
}
```

```tsx
"use client";

async function onSave() {
  const result = await renamePost(id, title);
  if (!result.ok) {
    setError(result.message);       // narrowed: message exists
    return;
  }
  console.log(result.data.id);      // narrowed: data exists
}
```

Expected failures are **returned**; unexpected ones are thrown and handled by an error boundary ([Action Errors](../07-server-actions/04-action-errors.md)).

## Schema-first types

Write the schema once and derive the input type, so validation and types cannot drift:

```ts
const SignupSchema = z.object({
  name: z.string().min(2).trim(),
  email: z.email().trim().toLowerCase(),
  password: z.string().min(8).max(72),
});

type SignupInput = z.infer<typeof SignupSchema>;       // { name: string; email: string; password: string }
```

`FormData` carries only strings and files, so numbers, booleans and dates need conversion:

| Field | Raw `FormData` value | Schema |
|---|---|---|
| Number | `"42"` | `z.coerce.number().int()` |
| Checkbox | `"on"` or missing (`null`) | `z.string().optional().transform((v) => v === "on")` or `z.stringbool()` where available |
| Date | `"2026-10-10"` | `z.coerce.date()` |
| Multiple values | several entries, same name | `formData.getAll("tags")` → `z.array(z.string())` |
| File | `File` | `z.instanceof(File)` and check `size`/`type` |

Converting every field from `FormData` manually:

```ts
const raw = Object.fromEntries(formData);        // { [k: string]: FormDataEntryValue }
const parsed = SignupSchema.safeParse(raw);      // schema validates the unknowns
```

`Object.fromEntries` keeps only the **last** value of repeated names, so use `getAll` for multi-value fields. Hidden fields such as `userId` are user input; derive identity from the session, not from the form. The Next.js Route Handler docs point to `zod-form-data` as a helper for coercing `FormData`.

## Arguments other than `FormData`

```ts
"use server";

export async function deletePost(postId: string) {
  const session = await verifySession();
  const id = z.uuid().parse(postId);               // never trust the argument's TypeScript type
  /* authorize, delete, revalidate */
}
```

Bind extra arguments for forms:

```tsx
const deleteWithId = deletePost.bind(null, post.id);
<form action={deleteWithId}><button>Delete</button></form>
```

After `bind`, the function type is `(formData: FormData) => ...` if the original was `(id: string, formData: FormData) => ...`. Bound values are serialized and sent to the server, so treat them as untrusted input and re-validate. Never bind secrets.

Arguments and results must be serializable: no functions, class instances or `Symbol`s. TypeScript does not check this, so keep action signatures to plain data.

## Typing exports from `"use server"` files

A `"use server"` file exports **async functions** only. Type-only exports (`export type CreatePostState`) are erased at compile time and are fine to colocate; exporting a constant or a schema object is not. Keep schemas and types in a separate module (`app/lib/definitions.ts`, as in the Authentication guide) and import them into both the action and the form.

## Passing actions to components

```tsx
// reusable client form accepting any "FormData action"
"use client";

import type { ReactNode } from "react";

type ActionFormProps = {
  action: (formData: FormData) => void | Promise<void>;
  children: ReactNode;
};

export function ActionForm({ action, children }: ActionFormProps) {
  return <form action={action}>{children}</form>;
}
```

For `useActionState` forms, type the whole `[state, action, pending]` flow in one place and pass `state` and `action` to child fields. A small typed field helper avoids repeating the error markup:

```tsx
type FieldErrors = Record<string, string[] | undefined> | undefined;

function FieldError({ errors, name }: { errors: FieldErrors; name: string }) {
  const messages = errors?.[name];
  return messages?.length ? <p role="alert">{messages[0]}</p> : null;
}
```

## `useFormStatus`

```tsx
"use client";

import { useFormStatus } from "react-dom";

export function SubmitButton({ children }: { children: React.ReactNode }) {
  const { pending } = useFormStatus();          // must render inside a <form>
  return <button disabled={pending}>{children}</button>;
}
```

In React 19, `useFormStatus` also returns `data`, `method` and `action`; the Next.js Authentication guide notes this. `pending` is always available.

## `useOptimistic`

```tsx
"use client";

import { useOptimistic, useTransition } from "react";

type Todo = { id: string; text: string; done: boolean };

export function TodoList({ todos, toggle }: { todos: Todo[]; toggle: (id: string) => Promise<void> }) {
  const [optimisticTodos, applyOptimistic] = useOptimistic(
    todos,
    (current: Todo[], id: string) => current.map((t) => (t.id === id ? { ...t, done: !t.done } : t)),
  );
  const [, startTransition] = useTransition();

  return (
    <ul>
      {optimisticTodos.map((t) => (
        <li key={t.id}>
          <button
            onClick={() =>
              startTransition(async () => {
                applyOptimistic(t.id);        // update immediately
                await toggle(t.id);           // then the Server Action; props refresh on revalidation
              })
            }
          >
            {t.done ? "✓" : "○"} {t.text}
          </button>
        </li>
      ))}
    </ul>
  );
}
```

`useOptimistic<State, Action>(state, reducer)` infers both from the reducer's annotated parameters. The optimistic value reverts to the real `todos` prop once the transition finishes, so the Server Action must revalidate for the true state to arrive ([Mutations and Optimistic UI](../07-server-actions/03-mutations-and-optimistic-ui.md)).

## Security reminder in types

A TypeScript signature like `deletePost(postId: string)` documents intent; it does not stop a direct POST with different values. Every action should:

1. Authenticate (`verifySession()`).
2. Parse its inputs with a schema.
3. Authorize against the specific resource ([Authorization](../11-authentication/05-authorization.md)).
4. Return only what the UI needs, not the database record.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `Argument of type '(prev, formData) => ...' is not assignable to 'string \| ((formData: FormData) => void \| Promise<void>)'` | Passing a `useActionState`-style action straight to `<form action>` | Use the `action` returned by `useActionState`, or write a one-argument action |
| `state` is possibly `undefined` | Initial state `undefined` is part of the type | `state?.errors` |
| `useActionState` infers `Awaited<...> \| undefined` unexpectedly | Return type of the action not annotated | Annotate `Promise<State>` |
| Runtime error: only async functions may be exported from `"use server"` | Exported a schema or constant | Move to a separate module |
| Numbers arrive as strings | `FormData` is strings | `z.coerce.number()` |
| Checkbox always missing | Unchecked boxes are not submitted | Default to false in the schema |
| Return value `Cannot be serialized` | Returned an ORM row, class instance or function | Map to plain data |
| `redirect` seems to do nothing / caught | Inside `try/catch` | Call outside; or `unstable_rethrow` |
| `useFormStatus` always `pending: false` | Rendered outside the `<form>` | Put the component inside the form |
| Types say `string` but a malicious POST sends an object | Types are not validation | Parse with a schema |
| Optimistic UI flickers back | Action did not revalidate | `revalidatePath` / `updateTag` in the action |

## Common mistakes

| Mistake | Fix |
|---|---|
| `formData.get("x") as string` | Parse with a schema |
| Trusting hidden `userId` fields | Derive from the session |
| Returning the database record | `{ success: true }` or a DTO |
| `any` for state | A union type |
| Exporting schemas from a `"use server"` file | Separate definitions module |
| Throwing for expected validation errors | Return them in state |
| Duplicated type and schema | `z.infer` |
| Passing callbacks (not actions) from a Server Component | Server Action or a client wrapper |
| Optimistic update without revalidation | Revalidate after the mutation |

## Quick Summary

- Form actions take `FormData`; `useActionState` actions take `(prevState, formData)` and return the next state.
- Declare a state or result union (`{ errors?; message? } | undefined`, or `{ ok: true; data } | { ok: false; ... }`) and annotate the return type.
- Types are not validation: parse `FormData` and arguments with a schema, and derive types with `z.infer`.
- `FormData` values are strings or files; coerce numbers, dates and checkboxes in the schema.
- Return only serializable plain data; keep schemas out of `"use server"` files; revalidate after mutations.

## Next

- Chapter 14 in the repo root [README](../README.md)
- [Forms](../07-server-actions/01-forms.md)
- [Validation](../07-server-actions/02-validation.md)
- [Mutations and Optimistic UI](../07-server-actions/03-mutations-and-optimistic-ui.md)

Sources: [Next.js Authentication guide](https://nextjs.org/docs/app/guides/authentication), [Data Security guide](https://nextjs.org/docs/app/guides/data-security), [route.js reference](https://nextjs.org/docs/app/api-reference/file-conventions/route)