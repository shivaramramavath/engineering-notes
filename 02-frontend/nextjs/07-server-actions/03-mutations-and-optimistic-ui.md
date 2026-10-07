# Mutations and Optimistic UI

A mutation is more than "write to the database". After the write, the cache must be refreshed and the user must see the result. This note covers the full mutation flow, which invalidation call to use, and how to make the UI respond before the server does.

> Verified against the Next.js 16.4 docs (Server Actions guide, Mutating Data, Interactive Apps). Cache details are in [Revalidation](../06-caching/04-revalidation.md).

## The mutation flow

```text
auth ─► validate ─► write ─► invalidate cache ─► (redirect | return result)
```

```ts
"use server";

import { updateTag } from "next/cache";
import { redirect } from "next/navigation";
import { auth } from "@/lib/auth";
import { db } from "@/lib/db";

export async function createPost(_prev: unknown, formData: FormData) {
  const session = await auth();
  if (!session?.user) throw new Error("Unauthorized");

  const title = String(formData.get("title") ?? "").trim();
  if (!title) return { error: "Title is required" };

  const post = await db.post.create({ data: { title, authorId: session.user.id } });

  updateTag("posts");           // 1. invalidate (before redirect)
  redirect(`/posts/${post.id}`); // 2. navigate; throws, so nothing after this runs
}
```

Order matters: `redirect` throws, so call revalidation **before** it.

## Which invalidation call?

| Call | Effect | Re-renders in the same response? | Use when |
|---|---|---|---|
| `updateTag(tag)` | Expires the tag immediately; next read waits for fresh data | Yes | The user must see their own write ("read-your-own-writes") |
| `revalidateTag(tag, profile)` | Stale-while-revalidate refresh in the background | **No** | Stale data is acceptable briefly (counters, feeds) |
| `revalidatePath(path)` | Invalidates by URL | Yes | One route is affected; tagging is overkill |
| `refresh()` | Re-fetches the current route's RSC payload; does **not** touch cached data | Yes | The view depends on uncached state the action changed |

All four are called inside the Server Action. `updateTag` and `refresh` work **only** in Server Actions. `revalidateTag` and `revalidatePath` also work in Route Handlers. None of them throws, so an action can invalidate and still return a value.

Cookies behave the same way: setting or deleting a cookie in an action re-renders the current page so the UI reflects it.

```ts
"use server";

import { cookies } from "next/headers";

export async function setTheme(theme: "light" | "dark") {
  (await cookies()).set("theme", theme);
  // current page re-renders automatically
}
```

### If you are not using Cache Components

The invalidation calls are the same in both models, but what they invalidate differs: with `fetch` tags on the previous model, or `cacheTag` on `use cache` functions with Cache Components. Check which model you are on in the [Caching Overview](../06-caching/00-caching-overview.md), and see [Revalidation](../06-caching/04-revalidation.md) for the exact behavior of each call.

## One roundtrip

When an action calls `updateTag`, `revalidatePath`, `refresh`, sets a cookie, or calls `redirect`, Next.js runs the action and re-renders the route in a single HTTP request. The response holds the return value **and** the new RSC payload, which the client applies. You do not write a follow-up `fetch`.

An action that does none of these returns only its value; the current page is **not** re-rendered. A common bug is "the write worked but the list is stale": the action forgot to invalidate.

## Event-handler mutations

Outside forms, call the action inside a transition so you get pending state and error forwarding:

```tsx
"use client";

import { useTransition } from "react";
import { deletePost } from "./actions";

export function DeleteButton({ id }: { id: string }) {
  const [isPending, startTransition] = useTransition();

  return (
    <button
      disabled={isPending}
      onClick={() => startTransition(async () => { await deletePost(id); })}
    >
      {isPending ? "Deleting…" : "Delete"}
    </button>
  );
}
```

If the action throws inside a transition, the error is forwarded to the nearest error boundary without a manual `try/catch`. See [Action Errors](./04-action-errors.md).

## Optimistic UI

After a click, the server takes time to respond. **Optimistic UI** shows the expected result immediately and reconciles when the real data arrives. React's `useOptimistic` does this:

```tsx
const [optimisticValue, setOptimistic] = useOptimistic(realValue, reducer?);
```

- `optimisticValue` equals `realValue` normally.
- Calling `setOptimistic(...)` **inside a transition or form action** overrides it while that transition is pending.
- When the transition ends (success or failure), React drops the override and shows the real value again. That is also your automatic rollback.

### Toggle (like, priority, done)

```tsx
"use client";

import { useOptimistic, useTransition } from "react";
import { toggleDone } from "./actions";

export function TodoItem({ id, done, title }: { id: string; done: boolean; title: string }) {
  const [optimisticDone, setOptimisticDone] = useOptimistic(done);
  const [, startTransition] = useTransition();

  return (
    <label>
      <input
        type="checkbox"
        checked={optimisticDone}
        onChange={() =>
          startTransition(async () => {
            setOptimisticDone(!optimisticDone); // instant
            await toggleDone(id);               // real change; server re-render brings the true value
          })
        }
      />
      {title}
    </label>
  );
}
```

Read from `optimisticDone`, not the prop, so rapid clicks flip correctly instead of reading a stale value.

### Adding to a list

Use a reducer that adds a temporary item:

```tsx
"use client";

import { useOptimistic } from "react";
import { send } from "./actions";

type Message = { id: string; text: string; sending?: boolean };

export function Thread({ messages }: { messages: Message[] }) {
  const [optimistic, addOptimistic] = useOptimistic<Message[], string>(
    messages,
    (state, text) => [...state, { id: crypto.randomUUID(), text, sending: true }],
  );

  async function formAction(formData: FormData) {
    const text = String(formData.get("message") ?? "");
    addOptimistic(text);   // appears now
    await send(text);      // when the server render arrives, the real list replaces it
  }

  return (
    <>
      <ul>
        {optimistic.map((m) => (
          <li key={m.id} style={{ opacity: m.sending ? 0.5 : 1 }}>{m.text}</li>
        ))}
      </ul>
      <form action={formAction}>
        <input name="message" />
        <button>Send</button>
      </form>
    </>
  );
}
```

The temporary `id` is only for React keys; the server assigns the real one.

### Pending-only list (common with Server Components)

When the persisted list is rendered by a Server Component, keep only the *pending* items in the client component and render them next to the server list. Start from `useOptimistic([])`; when fresh data arrives the pending list resets to empty and the real item appears in the server-rendered list:

```tsx
const [pending, addPending] = useOptimistic<{ id: string; content: string }[]>([]);
```

### Failure and rollback

- If the action **throws**, the transition ends, the optimistic value is discarded, and the error goes to the nearest error boundary.
- For an **expected** failure, return `{ success: false, error }`, show it (a toast, an inline message), and the optimistic value reverts on its own.

```tsx
startTransition(async () => {
  moveTask({ taskId, status });
  const result = await updateStatus(taskId, status);
  if (!result.success) toast.error(result.error); // card snaps back
});
```

### Rules for `useOptimistic`

| Rule | Why |
|---|---|
| Call the setter inside a transition or form action | Outside one, React warns and the override has no clear end |
| The setter applies instantly; `useState` setters after an `await` inside a transition do not | Use `useOptimistic` or direct DOM for same-frame feedback |
| Never optimistically show something the server may reject **silently** | The UI will "revert" confusingly; surface failures |
| The optimistic value is temporary | Keep the source of truth on the server |

## Pending feedback without optimism

Not everything needs optimism. For a slow, rarely repeated action ("Generate report"), a pending state is honest and enough: `pending` from `useActionState`, `isPending` from `useTransition`, or `useFormStatus` in a child. A parent can dim itself while a child action runs by having the child set a `data-pending` attribute and styling with CSS `:has()`.

## Double submission and idempotency

Users double-click, networks retry, and tabs go stale. Actions are dispatched one at a time per client, but that does not protect you from two tabs or a replayed request.

- Disable the button while pending.
- For operations that must run once (payment, sending email), use an **idempotency key** or a database unique constraint, and make "already done" a no-op.
- Do not rely on UI state for correctness.

## Choosing

| Situation | Approach |
|---|---|
| Form creates a record, then leaves the page | `updateTag` / `revalidatePath`, then `redirect` |
| Toggle or counter, user expects instant feedback | `useOptimistic` + transition |
| Appending to a list the server renders | Pending-only optimistic list |
| Mutation changes something not in the cache | `refresh()` |
| Eventual consistency is fine (view count) | `revalidateTag` with a profile |
| Must run exactly once | Idempotency key or unique constraint |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| No invalidation after the write | Data saved, UI stale | `updateTag`, `revalidatePath` or `refresh()` |
| `redirect` before revalidating | Destination shows old data | Revalidate first |
| `updateTag` from a Route Handler | Error (Server Actions only) | Use `revalidateTag` there |
| Expecting `revalidateTag` to refresh the current page immediately | Page unchanged | Use `updateTag` for read-your-own-writes |
| `useOptimistic` setter called outside a transition | Warning, flicker | Wrap in `startTransition` or use a form action |
| Reading the prop instead of the optimistic value | Rapid clicks misbehave | Derive the next value from the optimistic one |
| Awaiting several actions with `Promise.all` | No speedup | Dispatch is sequential; merge into one action |
| Optimistic update with no failure handling | UI silently reverts | Return an error result and show it |
| Trusting UI state to stop duplicates | Double charges | Idempotency key or unique constraint |

## Quick Summary

- Mutation flow: authenticate, validate, write, invalidate, then redirect or return.
- `updateTag`/`revalidatePath`/`refresh`/cookies/`redirect` re-render the route in the same response; `revalidateTag` with a profile does not.
- `updateTag` and `refresh` are Server Actions only.
- Wrap event-handler calls in `startTransition`.
- `useOptimistic` shows a temporary value during a transition and reverts automatically.
- Return expected failures, throw unexpected ones, and guard against duplicates at the data layer.

## Next

- [Action Errors](./04-action-errors.md)
- [Revalidation](../06-caching/04-revalidation.md)
- [Router Cache](../06-caching/03-router-cache.md)
