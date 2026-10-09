# 11 · Authentication

How to know who the user is, keep them signed in, and decide what they may do, in an App Router app. The chapter builds the pieces by hand so you understand them, then says when a library is the better choice.

> Verified against the Next.js 16.4 documentation (Authentication, Authentication with Cache Components, Data Security, `cookies`, `unauthorized`, `forbidden`). OAuth and JWT sections follow the standards (RFC 6749, RFC 7636, RFC 7519) and the `jose` library; check your provider's docs for exact endpoints.

## The three concepts

| Concept | Question it answers | Where it lives in this chapter |
|---|---|---|
| **Authentication** | Who are you? | [Architecture](./00-auth-architecture.md), [OAuth](./03-oauth.md) |
| **Session management** | Are you still the same person on the next request? | [Sessions and Cookies](./01-sessions-and-cookies.md), [JWT and Tokens](./02-jwt-and-tokens.md) |
| **Authorization** | What may you see and do? | [Protecting Routes](./04-protecting-routes.md), [Authorization](./05-authorization.md) |

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Auth Architecture](./00-auth-architecture.md) | The three concepts, where checks run, build vs library, threats |
| 01 | [Sessions and Cookies](./01-sessions-and-cookies.md) | Sign-up and login with Server Actions, stateless and database sessions, cookie options |
| 02 | [JWT and Tokens](./02-jwt-and-tokens.md) | JWT anatomy, signing vs encrypting, `jose`, refresh tokens, verifying third-party tokens |
| 03 | [OAuth](./03-oauth.md) | Authorization Code flow with PKCE, a hand-built Google sign-in, account linking, libraries |
| 04 | [Protecting Routes](./04-protecting-routes.md) | Proxy, the Data Access Layer, pages, actions, handlers, Cache Components |
| 05 | [Authorization](./05-authorization.md) | Roles, ownership checks, policies, `forbidden()`, an audit checklist |

## The rules to remember

1. **Prefer a maintained auth library** for production. Hand-rolled auth is for learning and for small, well-reviewed cases.
2. **Check as close to the data as possible.** Proxy is a fast filter, not the lock.
3. **Every Server Action and Route Handler re-verifies the session.** They are public HTTP endpoints.
4. **Do not rely on a layout** to protect pages; layouts do not re-render on every navigation.
5. **Session cookies are `HttpOnly`, `Secure` in production, `SameSite=Lax`,** set on the server.
6. **Authentication is not authorization.** Being signed in does not mean the user owns that record.
7. **Return DTOs, not database rows.** Never ship password hashes or internal fields to the client.

## Next

Chapter 12 in the repo root [README](../README.md).
