# Sessions and Cookies

A session is how the server recognizes the same user across requests. This note builds sign-up, login, logout and a stateless session with `jose`, then shows the database-session variant.

> Verified against the Next.js 16.4 Authentication guide and `cookies` API reference. The hashing and database parts are pseudocode: replace `db.*` with your ORM.

## What it is

HTTP is stateless. After a successful login, the server hands the browser a **cookie**; the browser sends it back on every request; the server checks it.

| Stateless session | Database session |
|---|---|
| Cookie contains a signed token with the user ID (and maybe role) | Cookie contains a session ID; the row lives in your database |
| Verified by checking a signature | Verified by looking up the row |

## Setup

```bash
npm install jose bcryptjs zod server-only
```

```bash
openssl rand -base64 32      # generate a secret
```

```bash
# .env.local  (never commit)
SESSION_SECRET=paste_the_generated_value
```

Use `bcryptjs` or any maintained password hasher (argon2, scrypt). The docs use `bcrypt.hash(password, 10)`. bcrypt only uses the first 72 bytes of input, so very long passwords are truncated; limit length at validation.

## The session module

```ts
// app/lib/session.ts
import "server-only";
import { SignJWT, jwtVerify, type JWTPayload } from "jose";
import { cookies } from "next/headers";

const secret = process.env.SESSION_SECRET;
if (!secret) throw new Error("SESSION_SECRET is not set");
const key = new TextEncoder().encode(secret);

const COOKIE = "session";
const SESSION_MS = 7 * 24 * 60 * 60 * 1000;

export type SessionPayload = { userId: string; role: "user" | "admin" };

// The docs name these encrypt/decrypt. A JWT signed with HS256 is SIGNED, not
// encrypted: anyone can read the payload, nobody can forge it.
export async function signSession(payload: SessionPayload | JWTPayload) {
  return new SignJWT(payload as JWTPayload)
    .setProtectedHeader({ alg: "HS256" })
    .setIssuedAt()
    .setExpirationTime("7d")
    .sign(key);
}

export async function readSession(token: string | undefined) {
  if (!token) return null;
  try {
    const { payload } = await jwtVerify(token, key, { algorithms: ["HS256"] });
    return payload;
  } catch {
    return null; // expired, tampered, or wrong secret
  }
}

export async function createSession(userId: string, role: SessionPayload["role"]) {
  const expires = new Date(Date.now() + SESSION_MS);
  const token = await signSession({ userId, role });
  (await cookies()).set(COOKIE, token, {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",
    expires,
    path: "/",
  });
}

export async function deleteSession() {
  (await cookies()).delete(COOKIE);
}
```

Details:

- `import "server-only"` makes a build error if a Client Component imports this file.
- `algorithms: ["HS256"]` pins the algorithm so a token cannot choose a weaker one.
- Keep the payload to the **minimum**: user ID and role. No email, phone, or anything sensitive.
- `cookies().set` and `.delete` work only in a **Server Function or Route Handler**; HTTP cannot set cookies after streaming starts, so you cannot do it during a Server Component render.
- The docs example uses `secure: true`. Browsers refuse `Secure` cookies on plain HTTP in some setups, so `NODE_ENV === "production"` is a common choice for local development. Always `true` in production.

### Cookie attributes

| Attribute | Value | Why |
|---|---|---|
| `httpOnly` | `true` | JavaScript cannot read it, limiting XSS damage |
| `secure` | `true` in production | Only sent over HTTPS |
| `sameSite` | `"lax"` | Not sent on cross-site subrequests; sent on top-level navigations (links) |
| `expires` / `maxAge` | e.g. 7 days | Bounded lifetime |
| `path` | `"/"` | Sent for the whole site |
| `domain` | omit | Host-only is tighter than sharing with subdomains |

`SameSite=Strict` is tighter but can drop the session on links arriving from other sites (a first click from an email looks signed out).

## Sign-up

```ts
// app/lib/definitions.ts
import * as z from "zod";

export const SignupSchema = z.object({
  name: z.string().min(2, { error: "Name must be at least 2 characters." }).trim(),
  email: z.email({ error: "Enter a valid email." }).trim().toLowerCase(),
  password: z
    .string()
    .min(8, { error: "At least 8 characters." })
    .max(72, { error: "At most 72 characters." })
    .regex(/[a-zA-Z]/, { error: "Include a letter." })
    .regex(/[0-9]/, { error: "Include a number." }),
});

export const LoginSchema = z.object({
  email: z.email().trim().toLowerCase(),
  password: z.string().min(1),
});

export type FormState =
  | { errors?: { name?: string[]; email?: string[]; password?: string[] }; message?: string }
  | undefined;
```

```ts
// app/actions/auth.ts
"use server";

import bcrypt from "bcryptjs";
import { redirect } from "next/navigation";
import { SignupSchema, LoginSchema, type FormState } from "@/app/lib/definitions";
import { createSession, deleteSession } from "@/app/lib/session";
import { db } from "@/app/lib/db"; // your data layer (pseudocode below)

export async function signup(state: FormState, formData: FormData): Promise<FormState> {
  const parsed = SignupSchema.safeParse(Object.fromEntries(formData));
  if (!parsed.success) {
    return { errors: parsed.error.flatten().fieldErrors };
  }
  const { name, email, password } = parsed.data;

  const existing = await db.users.findByEmail(email);
  if (existing) {
    // Avoid confirming which emails are registered where possible; see Debugging.
    return { errors: { email: ["Could not create the account."] } };
  }

  const passwordHash = await bcrypt.hash(password, 10);
  const user = await db.users.create({ name, email, passwordHash, role: "user" });

  await createSession(user.id, user.role);
  redirect("/dashboard");              // throws; keep it outside any try/catch
}
```

```tsx
// app/ui/signup-form.tsx
"use client";

import { useActionState } from "react";
import { signup } from "@/app/actions/auth";

export function SignupForm() {
  const [state, action, pending] = useActionState(signup, undefined);
  return (
    <form action={action}>
      <label htmlFor="name">Name</label>
      <input id="name" name="name" autoComplete="name" />
      {state?.errors?.name && <p>{state.errors.name}</p>}

      <label htmlFor="email">Email</label>
      <input id="email" name="email" type="email" autoComplete="email" />
      {state?.errors?.email && <p>{state.errors.email}</p>}

      <label htmlFor="password">Password</label>
      <input id="password" name="password" type="password" autoComplete="new-password" />
      {state?.errors?.password && (
        <ul>{state.errors.password.map((e) => <li key={e}>{e}</li>)}</ul>
      )}

      {state?.message && <p role="alert">{state.message}</p>}
      <button disabled={pending} type="submit">Sign up</button>
    </form>
  );
}
```

See [Forms](../07-server-actions/01-forms.md) and [Validation](../07-server-actions/02-validation.md) for the form pattern.

## Login

```ts
// app/actions/auth.ts (continued)
const DUMMY_HASH = bcrypt.hashSync("not-a-real-password", 10); // computed once at startup

export async function login(state: FormState, formData: FormData): Promise<FormState> {
  const parsed = LoginSchema.safeParse(Object.fromEntries(formData));
  if (!parsed.success) return { message: "Invalid email or password." };

  const { email, password } = parsed.data;
  const user = await db.users.findByEmail(email);

  // Compare even when the user is missing so timing does not reveal which emails exist.
  const ok = await bcrypt.compare(password, user?.passwordHash ?? DUMMY_HASH);
  if (!user || !ok) return { message: "Invalid email or password." };

  await createSession(user.id, user.role);
  redirect("/dashboard");
}

export async function logout() {
  await deleteSession();
  redirect("/login");
}
```

```tsx
<form action={logout}><button>Log out</button></form>
```

Notes:

- Same message for unknown email and wrong password.
- Add **rate limiting** (per IP and per account) before you ship; see the rate limiting example in the Backend for Frontend guide linked from the Data Security docs.
- Logout is a **Server Action behind a form**, not a GET link: mutations must not be triggered by simple GET requests (prefetching, crawlers).

## Refreshing (sliding expiry)

The docs show re-setting the cookie with a later expiry when the user returns:

```ts
export async function updateSession() {
  const store = await cookies();
  const token = store.get(COOKIE)?.value;
  const payload = await readSession(token);
  if (!token || !payload) return null;

  store.set(COOKIE, token, {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",
    expires: new Date(Date.now() + SESSION_MS),
    path: "/",
  });
}
```

This extends the **cookie**, not the token's own `exp` claim: after 7 days the token fails verification regardless. A real sliding session issues a **new token** with a new `exp`. Cookies cannot be set while a Server Component renders, so run the refresh from a Server Action, a Route Handler or [Proxy](../08-route-handlers-and-proxy/02-proxy.md) (which can set cookies on its response).

## Database sessions

Use when you need revocation, "log out everywhere", or a list of devices.

```ts
// app/lib/session.ts (database variant)
export async function createDbSession(userId: string) {
  const expiresAt = new Date(Date.now() + SESSION_MS);

  // 1. Create the row
  const { id: sessionId } = await db.sessions.create({ userId, expiresAt });

  // 2. Sign the session ID so the cookie cannot be forged
  const token = await new SignJWT({ sessionId })
    .setProtectedHeader({ alg: "HS256" })
    .setExpirationTime("7d")
    .sign(key);

  // 3. Store it in the cookie (also lets Proxy do an optimistic check)
  (await cookies()).set(COOKIE, token, {
    httpOnly: true, secure: process.env.NODE_ENV === "production",
    sameSite: "lax", expires: expiresAt, path: "/",
  });
}

export async function deleteDbSession() {
  const payload = await readSession((await cookies()).get(COOKIE)?.value);
  if (payload?.sessionId) await db.sessions.delete(String(payload.sessionId));
  (await cookies()).delete(COOKIE);
}
```

Verifying on each request (in the DAL, with `React.cache`):

```ts
const row = await db.sessions.findActive(sessionId);   // not expired, joined to the user
if (!row) redirect("/login");
```

Make the session ID **unguessable** (a UUID v4 or 128+ bits of randomness) and store an `expiresAt`. Delete expired rows periodically. The docs suggest server caching for the session's lifetime and combining the session read with your user query to cut round trips.

## Cookies API cheat sheet

| Task | Code | Where |
|---|---|---|
| Read | `(await cookies()).get("name")?.value` | Server Components, actions, handlers |
| Set | `(await cookies()).set(name, value, options)` | Server Functions, Route Handlers only |
| Delete | `(await cookies()).delete(name)` | Server Functions, Route Handlers only |
| Delete alternative | `set(name, "", { maxAge: 0 })` | same |
| In Proxy | `req.cookies.get("name")?.value` | `proxy.ts` |

`cookies()` is async and request-time: using it in a layout or page makes the route dynamic. After setting or deleting a cookie in a Server Action, Next.js can return the updated UI and data in one round trip; call `revalidatePath` or `revalidateTag` to refresh cached data too.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| "Cookies can only be modified in a Server Action or Route Handler" | `set`/`delete` during render | Move to an action or handler |
| Cookie set but missing next request | `Secure` on HTTP, `SameSite` mismatch, wrong `Path` | Inspect Set-Cookie in the Network tab |
| Login works, `/dashboard` redirects back to `/login` | Proxy or DAL cannot read the cookie (different name, `jose` error) | Log the `jwtVerify` error in development |
| `SESSION_SECRET is not set` | Env var missing in that environment | Add it; restart the server |
| Signature verification fails after deploy | Different secret per instance or rotated secret | Same value everywhere; support old and new during rotation |
| Redirect after login not happening | `redirect()` inside `try/catch` | Move it outside |
| User stays logged in after logout | Only the cookie was deleted for a database session | Delete the row too |
| Hydration shows signed-out UI briefly | Session read in a Client Component after load | Read on the server, pass props or a context promise |

## Common mistakes

| Mistake | Fix |
|---|---|
| Calling a signed JWT "encrypted" and putting private data in it | Payload is readable; keep it minimal or use JWE ([JWT and Tokens](./02-jwt-and-tokens.md)) |
| Cookie without `httpOnly` | Always set it |
| Long-lived session with no revocation path | Shorter lifetime, or database sessions |
| Plain SHA or MD5 for passwords | bcrypt, scrypt or argon2 |
| Different errors for unknown user vs bad password | One generic message, compare against a dummy hash |
| Logout via GET link | Use a POST Server Action |
| Trusting a `role` field from form data | Derive from the session or database |
| Hard-coding the secret | Environment variable, rotated when exposed |

## Quick Summary

- A session = a cookie the server can verify; stateless (signed token) or database (signed ID).
- Set cookies only from Server Functions or Route Handlers: `httpOnly`, `secure` in production, `sameSite: "lax"`, an expiry.
- Validate input, hash passwords with a slow algorithm, give one generic login error, rate limit.
- Keep the token payload to a user ID and role; a signed JWT is readable.
- Use database sessions when you need to revoke.

## Next

- [JWT and Tokens](./02-jwt-and-tokens.md)
- [Protecting Routes](./04-protecting-routes.md)
- [Server Actions](../07-server-actions/00-server-actions.md)
