# Next.js

Next.js is the most widely used React framework, and its **App Router** changes how you think about components: by default they run on the **server**, and you opt into the browser with `"use client"`. That split has real TypeScript consequences: which props can cross the boundary, how page and route handler parameters are typed, how environment variables and server actions are typed, and where validation must live. This note focuses on typing and on the mistakes that the server/client boundary makes easy.

> **Version note.** Next.js evolves quickly, and details differ between versions. Notably, in Next.js 15 the `params` and `searchParams` page props are **promises** (they were plain objects in 14), and caching defaults for `fetch` changed. Check the documentation for your installed version before copying signatures. The examples use the App Router.

**Prerequisites:**
- [Component props and children](./00-component-props-and-children.md)
- [Hooks](./02-hooks.md) and [context](./03-context.md)
- [Schema validation](../15-runtime-validation/01-schema-validation.md)
- [Compiler and tsconfig recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md)

---

## Server and client components

| | Server component (default) | Client component (`"use client"`) |
|---|---|---|
| Runs | on the server (at build or request time) | on the server for first render, then hydrates in the browser |
| Can be `async` and `await` data | **yes** | no |
| Can use state, effects, refs, context, event handlers, browser APIs | **no** | yes |
| Can read server-only resources (database, secrets) | **yes** | no, never |
| Adds to the JavaScript bundle | no | yes |

```tsx
// app/users/page.tsx  (a server component)
import { UserList } from "./user-list";

export default async function UsersPage() {
  const users = await db.users.findMany();           // direct data access, runs on the server
  return <UserList users={users.map(toUserDto)} />;
}
```

```tsx
// app/users/user-list.tsx
"use client";                                         // marks the client boundary

import { useState } from "react";

export function UserList({ users }: { users: UserDto[] }) {
  const [filter, setFilter] = useState("");
  return (
    <>
      <input value={filter} onChange={(e) => setFilter(e.target.value)} />
      <ul>{users.filter((u) => u.name.includes(filter)).map((u) => <li key={u.id}>{u.name}</li>)}</ul>
    </>
  );
}
```

`"use client"` marks a **boundary**: that file and everything it imports become client code. Keep client components small and near the leaves, and keep data fetching in server components.

### Props that cross the boundary

A server component passes props to a client component. Those props are **serialized** and sent to the browser, so they must be serializable values: strings, numbers, booleans, `null`, plain objects and arrays, and certain built-ins that React supports. They **cannot** be:

- **Functions** (event handlers, callbacks), except server actions.
- **Class instances** with methods, `Map`/`Set` in some cases, or anything with non-serializable references.
- **Secrets or server-only data** you do not want the client to see. Everything you pass is visible in the page.

TypeScript does not check serializability for you. A function-typed prop on a client component compiles, and the error appears when you render from a server component ("Functions cannot be passed directly to Client Components"). Pass **DTOs** (plain data), and use ISO strings for dates to avoid surprises ([DTO pattern](../16-type-safe-apis/02-dto-pattern.md), [request and response types](../16-type-safe-apis/01-request-response-types.md)).

Server components **can** be passed as `children` to client components, which is how a client layout can wrap server-rendered content.

### Keep server-only code out of the client

```ts
// lib/db.ts
import "server-only";            // build error if a client component imports this module
export const db = createClient(process.env.DATABASE_URL!);
```

The `server-only` package makes the build fail if client code imports the module, a safety net against leaking secrets and database code.

## Typing pages and layouts

```tsx
// app/blog/[slug]/page.tsx
interface PageProps {
  params: Promise<{ slug: string }>;                                  // Next 15: a promise
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}

export default async function BlogPost({ params, searchParams }: PageProps) {
  const { slug } = await params;
  const { page } = await searchParams;
  const post = await getPost(slug);
  if (!post) notFound();                                              // from "next/navigation"
  return <article>{post.title}</article>;
}
```

- **Dynamic segments** (`[slug]`) arrive in `params` as **strings** (or `string[]` for catch-all `[...slug]`). They come from the URL, so they are untrusted.
- **`searchParams`** values are `string | string[] | undefined`. Validate and convert them with a schema, as in any query string ([validation recipes](../15-runtime-validation/04-validation-recipes.md)):

```tsx
const SearchSchema = z.object({
  page: z.coerce.number().int().min(1).default(1),
  q: z.string().optional(),
});

const { page, q } = SearchSchema.parse(await searchParams);
```

- In Next.js 14 and earlier, `params` and `searchParams` are plain objects (no `await`). Some newer versions also offer generated helper types for page props, and typed routes, so check what your version provides.
- Layouts take `children: React.ReactNode` (and `params` for dynamic layouts).

### Metadata

```tsx
import type { Metadata } from "next";

export const metadata: Metadata = { title: "Blog", description: "Posts" };

export async function generateMetadata({ params }: PageProps): Promise<Metadata> {
  const { slug } = await params;
  const post = await getPost(slug);
  return { title: post?.title ?? "Not found" };
}
```

`Metadata` is a typed object, so a misspelled field is a compile error.

## Route handlers

```ts
// app/api/users/[id]/route.ts
import { NextRequest, NextResponse } from "next/server";

export async function GET(
  request: NextRequest,
  { params }: { params: Promise<{ id: string }> },
) {
  const { id } = await params;
  const user = await users.find(id);
  if (!user) {
    return NextResponse.json({ error: { code: "NOT_FOUND", message: "User not found" } }, { status: 404 });
  }
  return NextResponse.json(toUserDto(user));
}

export async function POST(request: NextRequest) {
  const parsed = CreateUserSchema.safeParse(await request.json());
  if (!parsed.success) {
    return NextResponse.json({ error: { code: "VALIDATION", issues: toIssues(parsed.error) } }, { status: 400 });
  }
  const user = await users.create(parsed.data);
  return NextResponse.json(toUserDto(user), { status: 201 });
}
```

Export one function per HTTP method. The request body is **untrusted**, so validate it before use, and return a consistent error shape ([error response types](../16-type-safe-apis/04-error-response-types.md)). `NextResponse.json` is not generic over the body, so the response type is not checked against your DTO: use your mapper function's return type, and share the contract types with the client ([API contracts](../16-type-safe-apis/00-api-contracts.md)).

## Server actions

A **server action** is an async function that runs on the server, callable from client code (often from a form):

```tsx
// app/signup/actions.ts
"use server";

import { z } from "zod";

const SignupSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
});

export type SignupState = { ok: boolean; errors?: Partial<Record<"email" | "password", string>> };

export async function signup(prev: SignupState, formData: FormData): Promise<SignupState> {
  const parsed = SignupSchema.safeParse(Object.fromEntries(formData));
  if (!parsed.success) {
    const errors: SignupState["errors"] = {};
    for (const issue of parsed.error.issues) {
      const key = issue.path[0];
      if (key === "email" || key === "password") errors[key] ??= issue.message;
    }
    return { ok: false, errors };
  }
  await createUser(parsed.data);
  return { ok: true };
}
```

```tsx
// app/signup/form.tsx
"use client";

import { useActionState } from "react";
import { signup, type SignupState } from "./actions";

const initial: SignupState = { ok: false };

export function SignupForm() {
  const [state, formAction, pending] = useActionState(signup, initial);
  return (
    <form action={formAction}>
      <input name="email" />
      {state.errors?.email && <p>{state.errors.email}</p>}
      <input name="password" type="password" />
      <button disabled={pending}>Sign up</button>
    </form>
  );
}
```

Important points:

- A server action is a **public HTTP endpoint** in effect. **Validate its input and check authorization inside it**, exactly as you would for an API route. Types do not protect it, since the arguments arrive from the network ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).
- Arguments and return values must be **serializable**, like props across the boundary.
- Return a typed result (a state object, a discriminated union) instead of throwing for expected failures, so the form can display errors ([the Result pattern](../11-error-handling/02-result-pattern.md)).
- Types shared between the action file and the client form are fine to import as `type` imports.

See [forms](./05-forms.md) for the client side of this pattern.

## Environment variables

`process.env.X` is `string | undefined` in the types. Validate once, and use the validated object:

```ts
// lib/env.ts
import "server-only";
import { z } from "zod";

const EnvSchema = z.object({
  DATABASE_URL: z.string().url(),
  SESSION_SECRET: z.string().min(32),
});

export const env = EnvSchema.parse(process.env);
```

- Variables prefixed with **`NEXT_PUBLIC_`** are inlined into the **browser bundle**. Never put secrets in them.
- Everything without the prefix is available only on the server.
- Do not import a server-only env module from client code (the `server-only` import makes that a build error).

See [config and environment](../20-nodejs-backend/01-config-and-environment.md).

## Configuration and tsconfig

Next.js generates and updates a `tsconfig.json` and a `next-env.d.ts` file. Do not edit `next-env.d.ts`. Recent versions support a typed config file:

```ts
// next.config.ts
import type { NextConfig } from "next";

const config: NextConfig = {
  reactStrictMode: true,
};

export default config;
```

The typical tsconfig (see [tsconfig recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md)) uses `moduleResolution: "bundler"`, `noEmit: true`, `isolatedModules: true`, and a `next` plugin for editor support. Add `strict` and consider `noUncheckedIndexedAccess`. `next build` runs the type check, so type errors fail the build.

## Data fetching and caching

Server components fetch directly with `await`. How results are cached and revalidated is configured through `fetch` options, route segment settings, and cache APIs, and **the defaults changed between Next.js versions**. Read the documentation for yours, and do not assume that a `fetch` call is cached (or not). For client-side interactivity (mutations, polling, optimistic UI), TanStack Query works in client components, with data prefetched on the server and handed over as dehydrated state ([server state](./07-server-state-tanstack-query.md)). Create the `QueryClient` per request on the server so cached data is never shared between users.

## Using client-only libraries

State libraries, context providers, and browser-only code must live in client components. A common pattern is a small client `Providers` component that wraps `children`, rendered from a server layout:

```tsx
// app/providers.tsx
"use client";

export function Providers({ children }: { children: React.ReactNode }) {
  const [queryClient] = useState(() => new QueryClient());      // one per browser session, not a module singleton
  return <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
}

// app/layout.tsx  (server component)
export default function RootLayout({ children }: { children: React.ReactNode }) {
  return <html lang="en"><body><Providers>{children}</Providers></body></html>;
}
```

For stores like Zustand, avoid module-level singletons holding per-user data on the server ([client state](./08-client-state-zustand.md), [context](./03-context.md)).

## Common mistakes

- Using `useState`, `useEffect`, or event handlers in a server component (add `"use client"` to the component that needs them, and keep it small).
- Passing functions, class instances, or non-serializable values from server to client components.
- Marking a large part of the tree `"use client"` and losing the benefits of server rendering.
- Treating `params` and `searchParams` as validated and typed numbers (they are strings from the URL).
- Forgetting to `await` `params` in Next.js 15, or writing 14-style code against 15.
- Skipping validation and authorization in server actions and route handlers.
- Putting secrets in `NEXT_PUBLIC_` variables or importing server modules into client files.
- Reading `localStorage`, `window`, or `document` during server rendering.
- Using a module-level store or `QueryClient` that is shared across requests.
- Assuming `fetch` caching behavior without checking the version.
- Hydration mismatches from values that differ between server and client (dates, random ids, persisted state).

## Debugging

- **"Functions cannot be passed directly to Client Components":** a server component is passing a function prop. Move the handler into the client component, or use a server action.
- **"You're importing a component that needs `useState`..."**: add `"use client"` to the file that uses the client-only API, or to the nearest appropriate parent.
- **Hydration errors:** compare server HTML with the client's first render. Look for browser-only APIs, dates, `Math.random`, or persisted state read during render.
- **Type errors only at build:** run `npx tsc --noEmit` and `next build` locally.
- **Stale data:** check caching and revalidation settings for the segment and the `fetch` call.
- **A variable is `undefined` in the browser:** it lacks the `NEXT_PUBLIC_` prefix, or the build did not see it.
- Inspect server logs for server component errors, and the browser console for client ones.

## Quick summary

- Components are **server by default**. Add `"use client"` only where you need state, effects, events, or browser APIs, and keep client components small.
- Props crossing the boundary must be serializable: pass plain DTOs, never functions or secrets.
- Type pages with `params` (a promise in Next 15) and `searchParams`, treat both as untrusted strings, and validate with a schema.
- Route handlers and server actions are network endpoints: validate input, check authorization, and return typed results.
- Validate environment variables once, keep secrets server-only (`server-only`), and remember `NEXT_PUBLIC_` ships to the browser.
- Provide client-side libraries through small client components, and avoid shared singletons for per-request state. Check the docs for your Next.js version, since APIs and caching defaults change.

**Next:** [20 Node.js Backend](../20-nodejs-backend/README.md)
