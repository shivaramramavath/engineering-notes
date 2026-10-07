# Server Actions

A **Server Function** is an `async` function that runs on the server and that you can call from the client. When you use one for a mutation (a form submit, a button click that changes data), it is called a **Server Action**. They replace the "write an API route, then `fetch` it from the client" step for most mutations.

> Verified against the Next.js 16.4 docs.

## What it is, and why it exists

Without Server Actions, a mutation needs three things: a Route Handler, a client `fetch` with serialization, and manual cache and UI updates afterward. With a Server Action you write one function, point a form at it, and Next.js handles the request, the cache invalidation and the UI refresh together.

```text
<form action={createPost}>  ──POST──►  createPost() runs on the server
                                          │ auth → validate → write → revalidate
        UI updated  ◄──── one response: return value + re-rendered route
```

## Creating one

The `"use server"` directive marks a function (or every export of a file) as a Server Function. The function must be `async`.

**In a file** (usable from Server and Client Components):

```ts
// app/posts/actions.ts
"use server";

import { revalidatePath } from "next/cache";
import { auth } from "@/lib/auth";
import { db } from "@/lib/db";

export async function createPost(formData: FormData) {
  const session = await auth();
  if (!session?.user) throw new Error("Unauthorized");

  await db.post.create({
    data: { title: String(formData.get("title")), authorId: session.user.id },
  });

  revalidatePath("/posts");
}
```

**Inline in a Server Component** (the directive goes at the top of the function body):

```tsx
export default function Page() {
  async function createPost(formData: FormData) {
    "use server";
    // ...
  }
  return <form action={createPost}>{/* ... */}</form>;
}
```

You **cannot define** a Server Function inside a Client Component. A Client Component can *import* one from a `"use server"` file, or receive one as a prop:

```tsx
"use client";
import { createPost } from "@/app/posts/actions";

export function Button() {
  return <button formAction={createPost}>Create</button>;
}
```

Prefer a dedicated `actions.ts` file per feature. It works from both component types and keeps mutations easy to find and review.

## Ways to call one

| From | How | Notes |
|---|---|---|
| A form | `<form action={fn}>` | Receives `FormData`. Works before JavaScript loads (progressive enhancement) |
| A button inside a form | `<button formAction={fn}>` | Several actions in one form; see [Forms](./01-forms.md) |
| An event handler | `startTransition(async () => { await fn(...) })` in a Client Component | Use a transition so pending state and errors work correctly |
| `useEffect` | `startTransition(...)` inside the effect | For automatic mutations (view counts, shortcuts); rarely needed |

A function passed to `action` or `formAction` is run inside a transition for you. When you call an action from `onClick` yourself, wrap it in `startTransition` (or use `useActionState`, which exposes an `action` meant for that).

```tsx
"use client";

import { useState, useTransition } from "react";
import { incrementLike } from "./actions";

export function LikeButton({ initial }: { initial: number }) {
  const [likes, setLikes] = useState(initial);
  const [isPending, startTransition] = useTransition();

  return (
    <button
      disabled={isPending}
      onClick={() =>
        startTransition(async () => {
          setLikes(await incrementLike());
        })
      }
    >
      Likes: {likes}
    </button>
  );
}
```

## What actually happens

- An action is a **`POST`** request to the page that invoked it. Only `POST` can call it.
- At build time the compiler replaces the function in client bundles with a **reference** (an encrypted action ID plus a dispatcher). The implementation never ships to the browser.
- If the action revalidates (`updateTag`, `revalidatePath`, `refresh`), sets or deletes a cookie, or calls `redirect`, Next.js re-renders the current route **inside the same request**. The response carries both the action's return value and the new UI. No follow-up fetch is needed.
- `revalidateTag` with a stale-while-revalidate profile is the exception: it does **not** re-render in the same response. See [Mutations](./03-mutations-and-optimistic-ui.md).

### Sequential dispatch

The client sends actions **one at a time**. Three quick clicks produce three requests that run in order, not in parallel. Do not use `Promise.all` to parallelize actions. If you need parallel work, do it inside one action (server side), or use Server Components or a Route Handler for reads.

## Security: treat every action as a public endpoint

Anyone who can send the same `POST` can call an action, whether or not your UI rendered the form. Hiding a form behind a logged-in page is not a security boundary.

Inside **every** action:

1. **Authenticate and authorize.** Read the session from cookies or headers. Never accept a user ID or token as an argument.
2. **Validate input.** `FormData` and arguments are untrusted. See [Validation](./02-validation.md).
3. **Constrain return values.** The result is serialized to the browser. Return what the UI needs, not whole database rows.
4. **Look things up by ownership.** Take an ID from the client, then re-read the row using the session.

```ts
"use server";

// Unsafe: the client supplies the whole item, including its id
export async function completeUnsafe(item: { id: string }) {
  await db.item.update({ where: { id: item.id }, data: { done: true } });
}

// Safe: take only a reference, check ownership with the session
export async function complete(itemId: string) {
  const session = await auth();
  if (!session?.user) return;

  const item = await db.item.findFirst({
    where: { id: itemId, ownerId: session.user.id },
  });
  if (!item) return;

  await db.item.update({ where: { id: item.id }, data: { done: true } });
}
```

Schema validation only checks shape. A well-formed ID can still point at someone else's row.

Built-in protections (not a substitute for the above):

| Protection | What it does |
|---|---|
| CSRF origin check | Compares `Origin` with `Host` (or `X-Forwarded-Host`); mismatches are rejected |
| Body size limit | 1 MB per action request by default |
| Encrypted action IDs, dead-code elimination | Unused actions are removed from client bundles |
| Closure encryption | Variables captured by an inline action are encrypted before going to the client |

See [Auth Architecture](../11-authentication/00-auth-architecture.md) for session handling.

## Configuration

```ts
// next.config.ts
import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  experimental: {
    serverActions: {
      allowedOrigins: ["my-proxy.com", "*.my-proxy.com"], // proxies / CDNs in front of the app
      bodySizeLimit: "2mb", // raise for file uploads
    },
  },
};

export default nextConfig;
```

For **self-hosted, multi-instance** deployments, set `NEXT_SERVER_ACTIONS_ENCRYPTION_KEY` to the same stable value on every instance. Otherwise instances cannot decrypt each other's closure variables and action references.

## Deployments and stale clients

Action IDs are tied to a build, and Next.js rotates them at most every 14 days. A browser tab left open on the previous build may call an ID that no longer exists and get "**Failed to find Server Action**". To reduce it:

- Prefer rolling deployments.
- Keep the encryption key stable across instances.
- Show a "please refresh" retry path instead of a hard failure.

## Server Actions or Route Handlers?

| | Server Action | Route Handler |
|---|---|---|
| Purpose | Mutations triggered by your own UI | HTTP endpoints for anyone: webhooks, mobile apps, third parties |
| Method | `POST` only | Any HTTP method |
| Call it from | Forms, event handlers | `fetch`, other services |
| Cache and UI refresh | Built in (same response) | Manual |
| Reads | Not designed for it (sequential dispatch) | Fine, but Server Components are usually better |

Use an action for your own app's writes. Use a [Route Handler](../08-route-handlers-and-proxy/00-route-handlers.md) when something other than your UI must call it.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| No auth check because "the page is protected" | Anyone can call the action by hand | Check the session inside the action |
| Trusting an ID or object from the client | Users modify other people's data | Look up by ID **and** owner |
| Defining an action inside a Client Component | Build error | Move to a `"use server"` file and import it |
| Non-`async` function with `"use server"` | Build error | Make it `async` |
| Returning whole DB records | Private fields reach the browser | Return a small DTO |
| Using actions for reads | Sequential, slow, no caching | Read in Server Components |
| `Promise.all([actionA(), actionB()])` | Still runs one after another | Combine into a single action |
| Calling from `onClick` without a transition | No pending state, odd error behavior | `startTransition` or `useActionState` |
| Large upload fails | Request rejected at 1 MB | Raise `bodySizeLimit`, or upload straight to storage |
| "Failed to find Server Action" after a deploy | Stale tab | Rolling deploys, stable key, retry UI |

## Quick Summary

- A Server Function is an `async` function marked `"use server"`; used for mutations it is a Server Action.
- Define in a `"use server"` file (works everywhere) or inline in a Server Component. Client Components import them.
- Actions are `POST` requests, dispatched one at a time per client.
- One response carries the return value and the re-rendered route.
- Authenticate, authorize and validate inside every action. It is a public endpoint.
- Use Route Handlers for external callers; Server Components for reads.

## Next

- [Forms](./01-forms.md)
- [Validation](./02-validation.md)
- [Revalidation](../06-caching/04-revalidation.md)
