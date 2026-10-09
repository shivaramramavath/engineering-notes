# OAuth

OAuth 2.0 lets a user grant your app limited access to an account at another service. OpenID Connect (OIDC) sits on top of it to prove **who the user is**. Together they power "Sign in with Google" or GitHub. This note explains the flow, builds one by hand in Route Handlers so the moving parts are visible, and says when to use a library instead.

> Follows RFC 6749 (OAuth 2.0), RFC 7636 (PKCE) and OpenID Connect Core. Provider URLs below are Google's; confirm endpoints and scopes in your provider's current docs. The Next.js docs do not hand-build OAuth; they point to auth libraries.

## What it is

| Term | Meaning |
|---|---|
| **Resource owner** | The user |
| **Client** | Your app |
| **Authorization server** | The provider's login and consent screen (Google) |
| **Resource server** | The provider's API (userinfo, calendar) |
| **Access token** | Lets the client call the provider's API |
| **ID token** (OIDC) | A signed JWT stating who authenticated |
| **Scope** | What you are asking for (`openid email profile`) |
| **Redirect URI** | Where the provider sends the user back; must match exactly what you registered |

OAuth alone is **authorization** ("this app may read your calendar"). For login you want **OIDC** (the `openid` scope), which adds the ID token and a standard userinfo endpoint.

## Why and when

| Use OAuth/OIDC sign-in | Avoid |
|---|---|
| You do not want to store passwords | You need offline accounts with no third-party dependency |
| Users already have Google/GitHub/Microsoft accounts | Provider outages would lock out your users with no fallback |
| You need the provider's API on the user's behalf | You only need identity inside your own company (use SSO, SAML or OIDC through a workforce provider) |

## The flow: Authorization Code with PKCE

```text
Browser            Your app (Route Handlers)        Provider
   │  click "Sign in with Google"  │                    │
   │ ───────────────────────────►  │ GET /login/google  │
   │                               │ make state, code_verifier
   │                               │ store them in short-lived HttpOnly cookies
   │ ◄───── 302 to provider ──────  (client_id, redirect_uri, scope, state, code_challenge)
   │ ──────────────────────────────────────────────────►│ user signs in, consents
   │ ◄──────────────── 302 to /login/google/callback?code=…&state=… ──
   │ ───────────────────────────►  │ GET /login/google/callback
   │                               │ check state == cookie
   │                               │ POST code + code_verifier + client_secret ─►│
   │                               │ ◄──────── access_token, id_token ───────────│
   │                               │ GET userinfo ───────────────────────────────►│
   │                               │ find or create user, create YOUR session
   │ ◄──── 302 /dashboard + Set-Cookie (your session) ──
```

| Piece | Purpose |
|---|---|
| `state` | Random value you check on return; stops login CSRF (someone forcing their account onto your browser) |
| `code_verifier` / `code_challenge` (PKCE) | Proves the party redeeming the code is the one that started the flow; stops code interception. Recommended for all clients, including confidential server apps |
| `nonce` (OIDC) | Binds the ID token to this login; verify when you validate the ID token yourself |
| `client_secret` | Proves your server's identity to the token endpoint; server-only |

## Setup

1. In the provider's console (Google Cloud → Credentials) create an OAuth client of type Web application.
2. Register redirect URIs: `http://localhost:3000/login/google/callback` and your production URL.
3. Add the environment variables (server only; no `NEXT_PUBLIC_`):

```bash
# .env.local
GOOGLE_CLIENT_ID=...
GOOGLE_CLIENT_SECRET=...
APP_URL=http://localhost:3000
```

## Step 1: start the flow

```ts
// app/login/google/route.ts
import { cookies } from "next/headers";
import { NextResponse } from "next/server";
import { randomBytes, createHash } from "node:crypto";

const b64url = (b: Buffer) => b.toString("base64url");

export async function GET() {
  const state = b64url(randomBytes(32));
  const verifier = b64url(randomBytes(32));
  const challenge = b64url(createHash("sha256").update(verifier).digest());

  const jar = await cookies();
  const opts = {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax" as const,   // must be sent on the top-level redirect back
    maxAge: 10 * 60,
    path: "/",
  };
  jar.set("oauth_state", state, opts);
  jar.set("oauth_verifier", verifier, opts);

  const url = new URL("https://accounts.google.com/o/oauth2/v2/auth");
  url.search = new URLSearchParams({
    client_id: process.env.GOOGLE_CLIENT_ID!,
    redirect_uri: `${process.env.APP_URL}/login/google/callback`,
    response_type: "code",
    scope: "openid email profile",
    state,
    code_challenge: challenge,
    code_challenge_method: "S256",
  }).toString();

  return NextResponse.redirect(url);
}
```

`sameSite: "lax"` matters: the provider redirects back with a top-level GET, which Lax cookies accompany; `Strict` would drop them.

## Step 2: handle the callback

```ts
// app/login/google/callback/route.ts
import { cookies } from "next/headers";
import { NextRequest, NextResponse } from "next/server";
import { createSession } from "@/app/lib/session";
import { db } from "@/app/lib/db";

export async function GET(req: NextRequest) {
  const { searchParams } = req.nextUrl;
  const code = searchParams.get("code");
  const state = searchParams.get("state");

  const jar = await cookies();
  const savedState = jar.get("oauth_state")?.value;
  const verifier = jar.get("oauth_verifier")?.value;
  jar.delete("oauth_state");
  jar.delete("oauth_verifier");

  if (!code || !state || !savedState || !verifier || state !== savedState) {
    return new Response("Invalid OAuth state", { status: 400 });
  }

  // Exchange the code for tokens (server to server)
  const tokenRes = await fetch("https://oauth2.googleapis.com/token", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams({
      grant_type: "authorization_code",
      code,
      redirect_uri: `${process.env.APP_URL}/login/google/callback`,
      client_id: process.env.GOOGLE_CLIENT_ID!,
      client_secret: process.env.GOOGLE_CLIENT_SECRET!,
      code_verifier: verifier,
    }),
  });
  if (!tokenRes.ok) return new Response("Token exchange failed", { status: 502 });
  const { access_token } = await tokenRes.json();

  // Ask the provider who this is
  const infoRes = await fetch("https://openidconnect.googleapis.com/v1/userinfo", {
    headers: { Authorization: `Bearer ${access_token}` },
  });
  if (!infoRes.ok) return new Response("Userinfo failed", { status: 502 });
  const profile: { sub: string; email?: string; email_verified?: boolean; name?: string } =
    await infoRes.json();

  // Find or create the local user, keyed by the provider's stable `sub`
  let user = await db.users.findByProvider("google", profile.sub);
  if (!user) {
    if (!profile.email || !profile.email_verified) {
      return new Response("Email not verified by provider", { status: 403 });
    }
    user = await db.users.createFromProvider({
      provider: "google",
      providerAccountId: profile.sub,
      email: profile.email.toLowerCase(),
      name: profile.name ?? profile.email,
    });
  }

  await createSession(user.id, user.role);
  return NextResponse.redirect(new URL("/dashboard", req.nextUrl));
}
```

Why these choices:

- **Identify the user by `sub`**, the provider's stable ID, not by email; emails change.
- **Fetching userinfo over TLS with the access token** is allowed by OIDC as a way to get identity claims. If you instead read the `id_token`, you must verify its signature (JWKS), `iss`, `aud`, `exp` and `nonce` (see [JWT and Tokens](./02-jwt-and-tokens.md#verifying-tokens-from-other-issuers)).
- A Route Handler reading `cookies()` and the request is **dynamic**; no caching concerns here. See [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md).
- Then it is just your normal session: the rest of the app does not know or care that OAuth was used.

### The button

```tsx
<a href="/login/google">Sign in with Google</a>
```

Use a plain anchor (a full navigation to a Route Handler), not `<Link>`: you do not want prefetching to start an OAuth flow and set state cookies.

## Account linking: the dangerous part

| Situation | Safe handling |
|---|---|
| New user, provider email **verified** | Create the account |
| Existing local account with the same email, provider email verified | Link only after the user proves control of the local account (sign in first, or confirm by email) |
| Provider does not verify emails | Never auto-link by email |

Auto-linking on an unverified email lets an attacker create a provider account with the victim's email and take over the victim's account.

## Calling the provider's API later

If you requested extra scopes (and `access_type=offline` for Google), keep the **access token** and **refresh token** server-side, encrypted at rest, in your database. Refresh when the access token expires, and request the narrowest scopes you need. Never send provider tokens to the browser.

## Use a library for production

Hand-built flows miss: multiple providers, token refresh, account-linking UI, error pages, provider quirks, mix-up attacks, session UI, and security patches. The Next.js docs list these Next-compatible options: Auth0, Better Auth, Clerk, Descope, Kinde, Logto, NextAuth.js (Auth.js), Ory, Stack Auth, Supabase, Stytch and WorkOS.

Better Auth, for example, mounts one catch-all Route Handler and a server `auth` object:

```ts
// app/api/auth/[...all]/route.ts
import { auth } from "@/lib/auth";
import { toNextJsHandler } from "better-auth/next-js";

export const { GET, POST } = toNextJsHandler(auth);
```

```ts
// lib/auth.ts
import { betterAuth } from "better-auth";
import { nextCookies } from "better-auth/next-js";

export const auth = betterAuth({
  // database, socialProviders, ...
  plugins: [nextCookies()], // keep last: lets Server Actions set cookies
});
```

```tsx
// a Server Component
import { auth } from "@/lib/auth";
import { headers } from "next/headers";
import { redirect } from "next/navigation";

const session = await auth.api.getSession({ headers: await headers() });
if (!session) redirect("/sign-in");
```

Treat the library's `getSession` as the `verifySession()` of your DAL ([Protecting Routes](./04-protecting-routes.md)). Whatever you choose, the principles in this chapter still apply: check at the data, check in every action and handler, return DTOs.

Auth.js (the project behind NextAuth) announced it is now part of Better Auth; read the current guidance of both before starting a new project. Setup details change between versions, so follow each library's own Next.js guide.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `redirect_uri_mismatch` | URI differs by scheme, host, port, path or trailing slash | Register the exact URI used in code |
| `invalid_grant` | Code reused, expired, wrong `redirect_uri` or `code_verifier` | Codes are single use and short-lived; send the same values as in step 1 |
| State check fails | State cookie not sent back (Strict SameSite, different host such as `127.0.0.1` vs `localhost`, expired) | Lax cookie; use the same host throughout |
| Works locally, fails in production | `APP_URL` or provider console still on localhost | Update env and registered URIs |
| `invalid_client` | Wrong secret or client type | Re-check credentials; secret is server-only |
| Logged in, but no email | Scope missing, or provider hides email (GitHub private email) | Request the scope; fetch the emails endpoint where the provider offers one |
| Prefetch triggers the flow | `<Link>` to the login route | Use `<a>` |
| Infinite redirect after callback | Session cookie not set or Proxy rejects it | Inspect Set-Cookie on the callback response |

## Common mistakes

| Mistake | Fix |
|---|---|
| Skipping `state` | Generate, store, compare |
| Skipping PKCE | Add the challenge and verifier |
| Matching users by email | Match by provider and `sub` |
| Auto-linking unverified emails | Require `email_verified` and a link confirmation |
| Exposing the client secret (`NEXT_PUBLIC_`) | Server-only env vars |
| Reading `id_token` claims without verifying | Verify via JWKS, or use userinfo |
| Requesting broad scopes | Least privilege |
| Storing provider tokens in a cookie or `localStorage` | Server-side, encrypted |
| Open redirect via a `next` parameter | Allow only relative paths ([Protecting Routes](./04-protecting-routes.md)) |

## Quick Summary

- OAuth gives your app delegated access; OIDC adds identity. Sign-in uses the **Authorization Code flow with PKCE**.
- Protect the dance with `state`, PKCE and an exact redirect URI; exchange the code on the server.
- Identify users by the provider's `sub`, and link accounts only on verified emails.
- After the callback, issue your own session cookie; the rest of the app is unchanged.
- For production, use a maintained library and keep this chapter's checks at the data.

## Next

- [Protecting Routes](./04-protecting-routes.md)
- [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md)
- [Sessions and Cookies](./01-sessions-and-cookies.md)

Sources: [Next.js Authentication guide](https://nextjs.org/docs/app/guides/authentication), [Better Auth Next.js integration](https://www.better-auth.com/docs/integrations/next), [Auth.js is now part of Better Auth](https://github.com/nextauthjs/next-auth/discussions/13252)
