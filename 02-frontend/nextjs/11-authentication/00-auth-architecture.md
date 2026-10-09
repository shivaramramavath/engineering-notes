# Auth Architecture

Authentication in Next.js is not one feature; it is a set of checks placed at several layers. This note gives the map: what the three concepts are, where each check runs in the App Router, and when to write it yourself versus use a library.

> Verified against the Next.js 16.4 Authentication and Data Security guides.

## What it is

| Concept | Meaning | Typical tools |
|---|---|---|
| **Authentication** | Verify the user is who they claim (password, OAuth, passkey, magic link) | Server Action + database, or a provider |
| **Session management** | Remember that verification across requests | Cookie holding a signed token or a session ID |
| **Authorization** | Decide what that user can access or change | Checks in the Data Access Layer, actions, handlers |

## Why it needs a map

An App Router app has **many entry points** to the same data:

```text
Browser ──► Page render (Server Component)
        ──► Server Action        (POST to the page URL)
        ──► Route Handler        (/api/...)
        ──► Prefetch / RSC fetch (a layout or segment on its own)
        ──► Proxy                (runs before all of the above)
```

A check on one entry point does not protect the others. A hidden button does not stop a direct POST to the Server Action. A redirect in a layout does not stop a nested segment from being requested. That is why the framework guidance is to check **where the data is read or written**.

## Where checks run

| Layer | Purpose | Strength |
|---|---|---|
| **Proxy** (`proxy.ts`) | Optimistic filter: read the cookie, redirect early, protect static shared routes | Fast, but cookie-only; not enough alone |
| **Data Access Layer** (`verifySession()`, `getUser()`) | The real check, next to the data | Primary defense |
| **Server Component / leaf** | Show or hide UI by role | Convenience, not security |
| **Server Action** | Re-verify identity and permission before every mutation | Required |
| **Route Handler** | Re-verify; return 401 or 403 | Required |
| **Client Component** | Never a security boundary | UI only |

Two kinds of check, in the docs' terms:

- **Optimistic**: trust the session data in the cookie (after verifying its signature). Cheap; good for redirects and UI.
- **Secure**: consult the database (is the session still valid, does the user still have this role?). Use before sensitive data or actions.

## The request lifecycle

```text
1. POST /login (Server Action)
   validate input → find user → verify password → create session → set cookie → redirect
2. GET /dashboard
   Proxy: cookie present and valid? no → redirect /login
   Page: verifySession() → userId → load user's data → render DTO
3. Server Action: deletePost(id)
   verifySession() → load post → owner === userId? → delete
4. POST /logout (Server Action)
   delete cookie (and the database row, if any) → redirect /login
```

## Session strategies at a glance

| | Stateless (signed token in cookie) | Database session (ID in cookie) |
|---|---|---|
| Server state | None | Table of sessions |
| Revoke one session | Hard (wait for expiry or keep a denylist) | Delete the row |
| "Log out everywhere" | Hard | Delete all rows for the user |
| Role change takes effect | After the token expires or is reissued | Immediately if you read the DB |
| Cost per request | Verify a signature | A database lookup (cache it per render) |
| Good for | Simple apps, edge-friendly checks | Anything needing revocation or device lists |

Details in [Sessions and Cookies](./01-sessions-and-cookies.md) and [JWT and Tokens](./02-jwt-and-tokens.md). The docs say you can use either, or both: a database session whose **ID** is stored in a signed cookie, so Proxy can still do a cheap optimistic check.

## Build or buy

The Next.js docs walk through username and password auth "for educational purposes" and recommend an authentication library for increased security and simplicity.

| Choose | When |
|---|---|
| **Auth library** (Better Auth, Auth.js, Clerk, Auth0, WorkOS, Supabase Auth and others listed in the docs) | Production apps; social login, MFA, passkeys, organizations, password reset, email verification |
| **Session library only** (`jose`, `iron-session`) | You already have identity (SSO, internal accounts) and only need cookies |
| **Hand-rolled** | Learning, prototypes, tightly scoped internal tools with a security review |

Things a hand-rolled system must also solve, beyond what the examples show: password reset, email verification, rate limiting and lockout, MFA, session listing, account recovery, audit logs, and secret rotation. Each is a source of bugs.

> The Auth.js project (formerly NextAuth) announced it is now part of Better Auth. If you evaluate libraries for a new project, read both projects' current guidance. See the Auth.js announcement at the end of this note.

## Threats this design addresses

| Threat | Mitigation |
|---|---|
| **XSS steals the session** | `HttpOnly` cookie; never put the token in `localStorage`; a Content Security Policy |
| **CSRF** | Server Actions only accept `POST` and compare `Origin` to `Host`; `SameSite=Lax` cookies; set `serverActions.allowedOrigins` behind reverse proxies |
| **Password database leak** | Slow salted hashes (bcrypt, scrypt, argon2); never store plaintext |
| **Forged cookie** | Sign (or encrypt) the cookie; verify with a pinned algorithm and a secret kept in an environment variable |
| **IDOR** (guessing another user's record ID) | Ownership check on every read and write ([Authorization](./05-authorization.md)) |
| **Direct POST to an action** | Authenticate and authorize inside every action |
| **Leaking secrets to the client** | `import "server-only"`, DTOs, tainting |
| **Open redirect after login** | Validate the `next` parameter ([Protecting Routes](./04-protecting-routes.md)) |
| **User enumeration** | Same error message and similar timing for "no such user" and "wrong password" |
| **Brute force** | Rate limit login and reset endpoints |

## Cache Components and auth

With `cacheComponents` enabled, reading the session is a request-time read, so it cannot be part of the static shell:

- Components that read the session sit behind `<Suspense>`.
- `cookies()` cannot be called inside a plain `use cache` function; read it outside and pass the value in, or use `use cache: private`.
- Cache keys and `cacheTag` values are stored in plain text. Use stable IDs, never secrets or emails.

See [Protecting Routes](./04-protecting-routes.md#cache-components) and [Cache Components](../06-caching/05-cache-components.md).

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Logged out after every deploy | `SESSION_SECRET` changed or differs between instances | Keep the same secret; plan rotation |
| Cookie not sent back | `Secure` cookie on plain HTTP, wrong `Path` or `Domain`, `SameSite` blocking a cross-site flow | Check attributes in DevTools → Application → Cookies |
| Works locally, fails in production | Missing env var, proxy rewriting `Host`/`Origin` | Set env vars; configure `allowedOrigins` |
| Page protected but data leaks via API | Only the page checks the session | Check in the DAL and in the Route Handler |
| User sees stale role | Role cached in the token | Reissue the token or read the role from the database |
| `Failed to verify session` in logs | Expired, tampered or wrong-secret token | Treat as signed out; clear the cookie |

## Common mistakes

| Mistake | Fix |
|---|---|
| Proxy as the only check | Add DAL checks next to the data |
| Checking auth in a layout only | Check in pages and the DAL |
| Trusting a client-sent `userId` or `role` | Derive both from the verified session |
| Storing tokens in `localStorage` | `HttpOnly` cookies |
| Putting PII in the token payload | Store the minimum (user ID, role) |
| Returning whole user rows | Return a DTO |
| Different error messages for "unknown email" and "bad password" | One generic message |
| Writing your own crypto | Use `jose`, `iron-session` or a library |

## Quick Summary

- Auth is three things: authentication, session management, authorization.
- Checks must happen at every entry point; **Proxy filters, the DAL decides**.
- Stateless sessions are simple; database sessions can be revoked.
- Prefer a maintained library for production; the hand-built version teaches what it does.
- Reading the session is request-time, so it streams behind `<Suspense>` under Cache Components.

## Next

- [Sessions and Cookies](./01-sessions-and-cookies.md)
- [Server Actions](../07-server-actions/00-server-actions.md)

Sources: [Auth.js is now part of Better Auth](https://github.com/nextauthjs/next-auth/discussions/13252)
