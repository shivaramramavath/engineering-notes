# Authorization

Authentication says who the user is. Authorization says what that user may do with **this specific thing**. Most real-world breaches in signed-in apps are authorization bugs: a user changes an ID in a request and reads or edits someone else's record.

> Verified against the Next.js 16.4 Authentication and Data Security guides and the `forbidden` reference (experimental). Role models and policy helpers are general patterns.

## What it is

| Check | Question | Example |
|---|---|---|
| **Authentication** | Are you signed in? | A valid session cookie |
| **Role check (RBAC)** | Does your role permit this kind of action? | Only admins may delete users |
| **Ownership check** | Is this record yours? | You may edit only posts where `authorId === you` |
| **Attribute / policy check (ABAC)** | Do the facts about you, the resource and the context allow it? | Editors may publish drafts in their own team, before the deadline |

You usually need two of these on one action: a role **and** an ownership or scope check.

## The vulnerability to remember: IDOR

Insecure Direct Object Reference: the server trusts an ID from the client.

```ts
// VULNERABLE: authenticated, but any user can delete any post
"use server";
export async function deletePost(postId: string) {
  await verifySession();
  await db.posts.delete(postId);
}
```

```ts
// FIXED: authorization is part of the check
"use server";
export async function deletePost(postId: string) {
  const session = await verifySession();

  const post = await db.posts.findById(postId);
  if (!post) return { message: "Not found" };
  if (post.authorId !== session.userId && session.role !== "admin") {
    return { message: "Forbidden" };            // or forbidden() (experimental)
  }

  await db.posts.delete(postId);
  revalidatePath("/posts");
}
```

Better still, **put the ownership in the query** so a wrong ID simply finds nothing:

```ts
const deleted = await db.posts.deleteWhere({ id: postId, authorId: session.userId });
if (!deleted) return { message: "Not found" };
```

Returning "not found" rather than "forbidden" also avoids confirming that the record exists.

## Roles (RBAC)

Keep roles few and put them in one place.

```ts
// app/lib/permissions.ts
export type Role = "user" | "editor" | "admin";

const rank: Record<Role, number> = { user: 0, editor: 1, admin: 2 };

export const atLeast = (role: Role, min: Role) => rank[role] >= rank[min];
```

```tsx
// app/admin/page.tsx
import { verifySession } from "@/app/lib/dal";
import { atLeast } from "@/app/lib/permissions";
import { forbidden } from "next/navigation";          // experimental; or redirect/notFound

export default async function AdminPage() {
  const session = await verifySession();
  if (!atLeast(session.role, "admin")) forbidden();
  return <h1>Admin</h1>;
}
```

Where does the role live?

| Source | Trade-off |
|---|---|
| **In the session token** | No database read; the role stays stale until the token expires or is reissued |
| **In the database, read per request** | Always current; one cached lookup per render (`React.cache`) |

For high-impact actions (changing roles, billing, deleting data), read the role from the database. Use the token role only for UI and cheap optimistic checks.

## Central policy functions

Scattering `if (role === "admin")` through the codebase invites drift. Define the rules once:

```ts
// app/lib/policy.ts
import "server-only";
import type { Role } from "./permissions";

type Actor = { id: string; role: Role };
type Post = { id: string; authorId: string; status: "draft" | "published" };

export const can = {
  readPost: (actor: Actor | null, post: Post) =>
    post.status === "published" || actor?.id === post.authorId || actor?.role === "admin",

  editPost: (actor: Actor, post: Post) =>
    actor.id === post.authorId || actor.role === "admin",

  deletePost: (actor: Actor, post: Post) =>
    actor.role === "admin" || (actor.id === post.authorId && post.status === "draft"),

  manageUsers: (actor: Actor) => actor.role === "admin",
};
```

Use it in the DAL, so every caller gets the same answer:

```ts
// app/lib/dal.ts
export const getPost = cache(async (id: string) => {
  const session = await getSession();                     // may be null for public reads
  const post = await db.posts.findById(id);
  if (!post || !can.readPost(session && { id: session.userId, role: session.role }, post)) {
    return null;                                          // caller renders notFound()
  }
  return toPostDTO(post);                                 // only the fields the viewer may see
});
```

## DTOs shaped by permission

Authorization also decides **which fields** leave the server.

```ts
// app/lib/dto.ts
import "server-only";

export async function getProfileDTO(slug: string) {
  const viewer = await getUser();                     // from the DAL
  const user = await db.users.findBySlug(slug);

  return {
    username: user.username,
    phone: viewer && (viewer.isAdmin || viewer.team === user.team) ? user.phone : null,
  };
}
```

Return only what this viewer may see. Do not send a whole row and rely on the UI to hide fields: everything passed to a Client Component is in the page payload.

## Server Actions and handlers: the checklist

For every mutation:

1. **Authenticate**: `verifySession()` inside the action.
2. **Validate input**: schema parse ([Validation](../07-server-actions/02-validation.md)).
3. **Load the resource** and **authorize**: role plus ownership or scope.
4. **Mutate**, preferably with the scope in the query.
5. **Revalidate** the affected cache.
6. **Return a minimal result** (`{ success: true }`), not the database record.

```ts
"use server";
import { verifySession } from "@/app/lib/dal";
import { can } from "@/app/lib/policy";
import { revalidatePath } from "next/cache";

export async function publishPost(postId: string) {
  const session = await verifySession();
  const post = await db.posts.findById(postId);
  if (!post || !can.editPost({ id: session.userId, role: session.role }, post)) {
    return { message: "Not found" };
  }
  await db.posts.update(postId, { status: "published" });
  revalidatePath("/posts");
  return { success: true };
}
```

The docs also show keeping `"use server"` actions thin and delegating to a `server-only` data function that performs authentication and authorization itself:

```ts
// data/posts.ts
import "server-only";
export async function deletePost(postId: string) { /* auth + ownership + delete */ }

// app/actions.ts
"use server";
import { deletePost } from "@/data/posts";
export async function deletePostAction(postId: string) {
  await deletePost(postId);
  revalidatePath("/posts");
}
```

Closures: an action defined inside a component captures variables, and Next.js encrypts them for the round trip. Do not rely on that to protect sensitive values, and never put an authorization decision in a captured variable (`isAdmin`); recompute it inside the action.

## Multi-tenant scoping

For apps with organizations or workspaces, every query carries the tenant:

```ts
// the active organization comes from the session or a verified membership, not from the URL alone
const membership = await db.memberships.find({ userId: session.userId, orgId });
if (!membership) notFound();

const projects = await db.projects.findMany({ orgId });          // always filtered by orgId
```

Better: make the tenant a required parameter of every DAL function so a query without it does not compile. Never trust an `orgId` that came only from a route param or a hidden field; confirm membership.

## Authorization and caching

| Caching case | Rule |
|---|---|
| User-specific data in `use cache` | Resolve the user first, pass the ID as an argument, keep the cached function unexported |
| Cache tags / keys | IDs only; they are stored in plain text |
| Shared static data behind a paywall | Protect with Proxy; DAL checks do not run at build time |
| After a permission change | Revoke or reissue the session; revalidate affected tags |

See [Protecting Routes](./04-protecting-routes.md#cache-components) and [Caching](../06-caching/README.md).

## UI-level checks

Hide what the user cannot do (buttons, nav links) because it is better UX. It is **never** security: the server action behind the button must check again.

```tsx
const session = await getSession();
{session && can.deletePost({ id: session.userId, role: session.role }, post) && <DeleteButton id={post.id} />}
```

## Audit checklist

Adapted from the Data Security guide's auditing section:

- Is there one **Data Access Layer**, and are database packages and `process.env` used only there?
- Do `"use client"` files take props with **private data**? Are their types too broad?
- In each `"use server"` file: arguments validated? user **re-authorized inside the action**? **ownership** checked (not just login)? return value filtered? database access in a `server-only` layer?
- Folders named `[param]`: is every param **validated**? Params and `searchParams` are user input.
- `proxy.ts` and `route.ts` have great reach: review them like any public API, and test them (penetration or vulnerability scans on your normal schedule).
- Do tokens or secrets appear in cache keys, tags, URLs or logs?
- Is a rate limit on login, reset and expensive actions?

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| User edits another user's record | Missing ownership check (IDOR) | Check `authorId`/scope in the query or policy |
| Admin UI visible to everyone | Role read from client or props | Read from the session in a Server Component |
| Demoted user still has access | Role cached in the token | Short token life, re-read from DB for sensitive actions |
| `forbidden()` shows nothing / not found | `authInterrupts` disabled | Enable the experimental flag |
| Policy passes in one route, fails in another | Rules duplicated inline | Centralize in `can.*` |
| Data leaks via an API route | Handler only checks login | Add role and ownership checks |
| Different users see each other's cached data | Per-user data in a shared cache key | Include the user ID as a cache argument; unexported helper |
| Everything in the payload visible in DevTools | Whole row passed to a Client Component | DTO with only permitted fields |

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating "logged in" as "allowed" | Add role and ownership checks |
| Trusting IDs, roles or tenant IDs from the client | Derive from the session; verify membership |
| Hiding UI instead of checking on the server | Check in the action and DAL |
| Checks scattered in components | Central `can` policy used in the DAL |
| Returning full records | Permission-shaped DTOs |
| Long-lived role in a token with no refresh | Short expiry or DB read |
| Revealing existence with "Forbidden" | "Not found" when existence is sensitive |
| No audit trail for admin actions | Log who did what to which resource |

## Quick Summary

- Authorization = role checks plus **ownership/scope checks** on the specific resource.
- IDOR is the common bug: never act on a client-supplied ID without checking it belongs to the session's user.
- Put rules in one `can` policy and call it from the DAL and every action.
- Read roles from the database for high-impact actions; tokens may be stale.
- Hide UI for UX, enforce on the server; return only the fields the viewer may see.

## Next

- Chapter 12 in the repo root [README](../README.md)
- [Protecting Routes](./04-protecting-routes.md)
- [Server Actions](../07-server-actions/00-server-actions.md)
