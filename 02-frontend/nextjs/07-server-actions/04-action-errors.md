# Action Errors

Server Actions fail in two different ways, and each needs a different response. Mixing them up is the most common source of "my form shows a white error page" and "my error message disappeared in production".

> Verified against the Next.js 16.4 docs. Error-message sanitizing in production is long-standing Next.js behavior; check the Error Handling page for your version if you depend on its details.

## Two kinds of error

| | Expected | Unexpected |
|---|---|---|
| Examples | Validation failure, "email already taken", "insufficient balance", not found | Database down, bug, unhandled exception, network failure |
| The user can fix it? | Yes | No |
| Mechanism | **Return** a value | **Throw** |
| Where it shows | Inline in the form, a toast | Nearest `error.tsx` boundary |
| Logged as an error? | Usually not | Yes |

Rule: if the user did something you can describe ("that name is taken"), return it. If the server broke, throw.

## Expected errors: return them

Model the result as data. A small discriminated type keeps callers honest:

```ts
// lib/action-result.ts (a plain module, not "use server")
export type ActionResult<T = void> =
  | { ok: true; data: T }
  | { ok: false; error: string; fieldErrors?: Record<string, string[]> };
```

```ts
"use server";

export async function renameProject(
  _prev: ActionResult | null,
  formData: FormData,
): Promise<ActionResult> {
  const session = await auth();
  if (!session?.user) throw new Error("Unauthorized"); // unexpected: not a normal user path

  const name = String(formData.get("name") ?? "").trim();
  if (!name) return { ok: false, error: "Name is required" };

  const taken = await db.project.findFirst({ where: { name, ownerId: session.user.id } });
  if (taken) return { ok: false, error: "You already have a project with that name" };

  await db.project.update({ /* ... */ });
  updateTag("projects");
  return { ok: true, data: undefined };
}
```

```tsx
"use client";

import { useActionState } from "react";
import { renameProject } from "./actions";

export function RenameForm() {
  const [state, action, pending] = useActionState(renameProject, null);

  return (
    <form action={action}>
      <input name="name" required />
      <button disabled={pending}>Save</button>
      {state && !state.ok && <p role="alert">{state.error}</p>}
    </form>
  );
}
```

`role="alert"` (or `aria-live`) makes assistive tech announce the message.

## Unexpected errors: throw

Let them throw. The nearest [`error.tsx`](../02-routing/05-error-and-not-found.md) boundary renders a fallback and offers a retry:

```tsx
// app/projects/error.tsx
"use client";

export default function Error({ error, reset }: { error: Error & { digest?: string }; reset: () => void }) {
  return (
    <div role="alert">
      <p>Something went wrong.</p>
      <button onClick={reset}>Try again</button>
    </div>
  );
}
```

Details:

- Inside a **transition** or a **form action**, a thrown error is forwarded to the error boundary. You do not need a `try/catch` just to surface it.
- In **production**, Next.js replaces the message of an error thrown on the server with a generic one plus a `digest`. The real message stays in your server logs; match them with the digest. Do **not** depend on `error.message` reaching the browser. This is also why expected errors must be *returned*, not thrown.
- An error boundary catches errors during rendering, including transitions. Errors thrown in a plain `onClick` that is *not* inside a transition are not caught by it. Another reason to use `startTransition`.

## The `try/catch` and `redirect` trap

`redirect()`, `notFound()` and similar helpers work by **throwing** a special control-flow error. A `catch` block that swallows every error also swallows the redirect:

```ts
// Broken: the redirect never happens
try {
  await db.post.create({ /* ... */ });
  redirect("/posts");        // throws inside try → caught below
} catch (e) {
  return { ok: false, error: "Could not save" };
}
```

Keep `redirect` **outside** the `try`:

```ts
let post;
try {
  post = await db.post.create({ /* ... */ });
} catch (e) {
  console.error(e);
  return { ok: false, error: "Could not save" };
}

updateTag("posts");
redirect(`/posts/${post.id}`);
```

If you must call framework helpers inside a `try`, rethrow framework errors first with `unstable_rethrow` from `next/navigation` (the name signals the API may change):

```ts
import { unstable_rethrow } from "next/navigation";

try {
  // ...code that may call redirect() or notFound()
} catch (e) {
  unstable_rethrow(e); // rethrows Next.js control-flow errors, ignores the rest
  // handle real errors here
}
```

## Catching known failures from libraries

Convert predictable database or API failures into expected errors, and let the rest throw:

```ts
try {
  await db.user.create({ data: { email, name } });
} catch (e) {
  if (isUniqueViolation(e)) {            // your helper: Prisma P2002, Postgres 23505, etc.
    return { ok: false, error: "That email is already registered" };
  }
  throw e;                                // unknown → error boundary + logs
}
```

This is how you handle races that a "check then insert" pre-query cannot prevent.

## Missing data and permissions

| Situation | Use |
|---|---|
| Resource does not exist | `notFound()` from `next/navigation`; renders the nearest `not-found.tsx` |
| User is not signed in | `redirect("/login")`, or `throw new Error("Unauthorized")` for a hard stop |
| Signed in but not allowed | `throw new Error("Forbidden")` and log it |

With the **experimental** `authInterrupts` flag enabled, you can throw `unauthorized()` and `forbidden()` from `next/navigation`, and Next.js renders `unauthorized.tsx` / `forbidden.tsx`. Because the flag is experimental, verify it before relying on it.

For destructive operations (deletes, role changes), fail loudly on a failed permission check rather than returning silently, so a mismatch shows up in logs.

## "Failed to find Server Action"

Seen after a deployment when a browser still runs the previous build. Action IDs change between builds, so the old ID no longer exists. Handle it as a retry path ("Please refresh the page") rather than a crash, prefer rolling deployments, and keep `NEXT_SERVER_ACTIONS_ENCRYPTION_KEY` the same across self-hosted instances. See [Server Actions](./00-server-actions.md).

## Network failures

If the client cannot reach the server, the action's promise rejects and the transition surfaces it like any thrown error. For user-facing flows, a boundary-level "Try again" button via `reset()` is usually enough. Experimental offline support exists for pending actions; verify it in the docs before relying on it.

## Logging

- Log unexpected errors **on the server** with context (user ID, action name, input shape, never secrets or full passwords).
- Return a user-friendly message, not the raw error.
- Send exceptions to your monitoring tool from the server code path, since the browser only sees a sanitized message and digest.

## Debugging checklist

| Symptom | Likely cause | Check |
|---|---|---|
| Redirect silently does not happen | `redirect` inside `try/catch` | Move it out, or `unstable_rethrow` |
| Error message is generic in production | Server error sanitized | Look up the `digest` in server logs; return expected errors instead of throwing |
| Whole page replaced by `error.tsx` on a validation error | Validation errors were thrown | Return them |
| Nothing happens and no error | Action returned early without a result the UI renders | Return a message, render `state` |
| Form state shows the previous state's shape | Missing `prevState` parameter | `(prevState, formData)` signature |
| Error not caught by the boundary | Thrown in a plain event handler outside a transition | Wrap in `startTransition` |
| "Failed to find Server Action" | Stale client after deploy | Refresh prompt; rolling deploys |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Throwing for validation errors | Error page instead of inline message | Return `{ ok: false, error }` |
| Returning for server faults | Bugs hidden, nothing logged | Throw, or log then return a generic message |
| Relying on `error.message` in the browser | Generic text in production | Return user-facing messages |
| `redirect` inside `try` | No navigation | Call it after the `try/catch` |
| Catching everything and ignoring it | Silent failures | Handle known cases, rethrow the rest |
| Leaking internal details in returned errors | Table names or stack traces to users | Return curated messages |
| No `error.tsx` near the form | Errors bubble to a distant boundary or the root | Add one at the route segment |

## Quick Summary

- Expected, user-fixable failures: **return** data and render it with `useActionState`.
- Unexpected failures: **throw** and let `error.tsx` handle it.
- Production hides thrown messages behind a digest; use server logs.
- Never swallow `redirect()` or `notFound()` in a `catch`; keep them outside `try` or use `unstable_rethrow`.
- Convert known database errors (unique violations) into expected errors.
- Handle stale-deploy errors with a refresh prompt.

## Next

- [08 · Route Handlers and Proxy](../08-route-handlers-and-proxy/README.md)
- [Error and Not Found](../02-routing/05-error-and-not-found.md)
- [Validation](./02-validation.md)
