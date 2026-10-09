# Social Login

"Sign in with Google/GitHub" delegates authentication to a provider. It reduces friction and means you may never store a password, but it introduces a subtle, high-impact problem: **mapping an external identity to a user in your system**. Most social-login vulnerabilities (account takeover through unverified emails) live in that mapping, not in the OAuth plumbing.

Prerequisites: [OAuth 2.0](./08-oauth2.md), [Passport](./02-passport-and-local-strategy.md), [refresh tokens](./05-refresh-tokens-and-logout.md) or [sessions](./07-session-authentication.md).

> Provider APIs, scopes, and Passport strategy options change over time. The structure below is stable; check the current docs for the strategy package and provider you use.

## The shape of the flow

```text
GET /auth/google            ──► redirect to Google (client_id, scope, state, redirect_uri)
        user logs in and consents at Google
GET /auth/google/callback?code=...&state=...
        └─► strategy exchanges code, fetches profile ──► validate(profile)
                 └─► find or create YOUR user ──► issue YOUR session / tokens
```

After the callback, **your app issues its own credentials** (a session cookie, or access + refresh tokens). The provider's tokens aren't your API's credentials.

## Setup with Passport

```bash
npm i passport-google-oauth20
npm i -D @types/passport-google-oauth20
```

Register your app in the provider's console to get a **client ID and secret**, and register the **exact callback URL** (`https://api.example.com/auth/google/callback`; also a separate one for local development).

```ts
// auth/strategies/google.strategy.ts
import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { PassportStrategy } from '@nestjs/passport';
import { Profile, Strategy, VerifyCallback } from 'passport-google-oauth20';

@Injectable()
export class GoogleStrategy extends PassportStrategy(Strategy, 'google') {
  constructor(config: ConfigService, private readonly auth: AuthService) {
    super({
      clientID: config.getOrThrow('GOOGLE_CLIENT_ID'),
      clientSecret: config.getOrThrow('GOOGLE_CLIENT_SECRET'),
      callbackURL: config.getOrThrow('GOOGLE_CALLBACK_URL'),
      scope: ['openid', 'email', 'profile'],
    });
  }

  async validate(_accessToken: string, _refreshToken: string, profile: Profile, done: VerifyCallback) {
    try {
      const user = await this.auth.loginWithProvider({
        provider: 'google',
        providerUserId: profile.id,                              // stable subject id
        email: profile.emails?.[0]?.value,
        emailVerified: (profile.emails?.[0] as { verified?: boolean })?.verified === true,
        name: profile.displayName,
      });
      done(null, user);                                          // → req.user
    } catch (err) {
      done(err as Error, false);
    }
  }
}
```

```ts
// auth/guards/google-auth.guard.ts
@Injectable()
export class GoogleAuthGuard extends AuthGuard('google') {}
```

```ts
@Public()
@UseGuards(GoogleAuthGuard)
@Get('google')
google() { /* the guard redirects to Google; this body never runs */ }

@Public()
@UseGuards(GoogleAuthGuard)
@Get('google/callback')
async googleCallback(@Req() req, @Res() res: Response) {
  const { accessToken, refreshToken } = await this.auth.login(req.user, clientMeta(req));
  setAuthCookies(res, accessToken, refreshToken);           // or create a session
  return res.redirect(this.config.getOrThrow('FRONTEND_URL'));   // fixed, allow-listed URL
}
```

Register `GoogleStrategy` as a provider in `AuthModule` (otherwise: "Unknown authentication strategy 'google'"). Which profile fields exist (`emails[0].verified`, `profile.id`) depends on the strategy package and provider; **inspect a real profile** in development and rely only on fields you've verified. Other providers follow the same pattern (`passport-github2`, Apple, Microsoft, ...), each with quirks (GitHub's primary email may require an extra scope and API call; Apple only sends the name on first login, and so on).

### `state` and CSRF on the callback

The flow must verify `state` on the callback (login CSRF protection, see [OAuth 2.0](./08-oauth2.md)). Passport strategies commonly implement this through sessions (an option such as `state: true` that stores the value in the session), which requires session middleware. In a **stateless** API there's no session to hold it, so you either enable a short-lived session/cookie solely for the login handshake, or implement `state` yourself (random value in a signed, short-lived `httpOnly` cookie, compared on callback). Don't disable `state` to make a stateless setup work. Verify how your strategy handles it. For PKCE and generic OIDC providers, consider `openid-client` instead of per-provider strategies.

## The identity model: map `(provider, subject)`, not email

Store external identities in their own table:

```text
users
├── id, email (nullable?), email_verified, name, ...

identities
├── id
├── user_id         fk → users
├── provider        'google' | 'github' | ...
├── provider_user_id   the provider's stable subject id (OIDC `sub`)
├── email_at_provider  (informational)
├── created_at
└── UNIQUE (provider, provider_user_id)
```

Rules:

- Identify a returning user by **`(provider, provider_user_id)`**, which is **stable and unique**. Emails can change, be recycled, or be unverified.
- A user can have **several identities** (Google + GitHub + password), which is why it's a separate table.
- Users who only use social login may have **no password hash** (`passwordHash` nullable) and no way to log in locally until they set one.

## Account linking: the dangerous part

When a provider login arrives, there are three cases:

```ts
async loginWithProvider(p: ProviderProfile) {
  // 1. Known identity → that user
  const existing = await this.identities.find(p.provider, p.providerUserId);
  if (existing) return this.users.findActiveById(existing.userId);

  // 2. New identity, email matches an existing account → LINK? (see below)
  // 3. Brand new → create user + identity
}
```

### Auto-linking by email is an account-takeover vector

Scenario: Alice registered locally as `alice@example.com`. An attacker creates a provider account (or a provider that doesn't verify emails) claiming `alice@example.com`, then "signs in with" it. If you **automatically link by email**, the attacker is now logged into Alice's account.

Rules to stay safe:

1. **Only trust an email the provider asserts as verified** (`email_verified: true` in OIDC, the `verified` flag in provider profiles). Use only providers whose verification you trust.
2. **Prefer not to auto-link at all** for an *existing* account. Instead, require the user to **prove ownership of the existing account** (log in with the existing method, then connect the provider from their settings), or confirm via an emailed link/code sent to the address.
3. **Never create or link when the email is missing or unverified.** Either reject or create a separate account without trusting the email for anything else.
4. The reverse also matters: if someone signs up locally with an email that's already held by a social account, you need email verification before treating the local account as owning that address ("pre-hijacking").

If you do auto-link verified emails, make it a documented policy limited to providers you trust, and notify the existing user.

### New user creation

```ts
const user = await this.users.create({
  email: p.emailVerified ? p.email : null,
  emailVerified: p.emailVerified,
  name: p.name,
  passwordHash: null,
});
await this.identities.create({ userId: user.id, provider: p.provider, providerUserId: p.providerUserId });
```

Do creation + identity insert in one [transaction](../02-database-foundations/04-transactions.md), and rely on the **unique `(provider, provider_user_id)` constraint** to handle two concurrent first logins ([database errors](../02-database-foundations/07-database-errors.md)).

## Issuing your own credentials after the callback

- **Session apps:** regenerate the session id, store your user id ([session authentication](./07-session-authentication.md)).
- **Token apps:** issue your access + refresh tokens ([refresh tokens](./05-refresh-tokens-and-logout.md)).
- **Browser redirect:** the callback is a browser navigation, so deliver credentials via `httpOnly` cookies and redirect to a **fixed, allow-listed** frontend URL.
- **Never put tokens in the redirect URL** (query string or fragment) where they leak into history, logs, and referrers. If you need to hand a token to a SPA or mobile app, use a short-lived, single-use code that the client exchanges via a back-channel request (the same pattern as the authorization code).
- **Open redirects:** if you support a `?redirect=` parameter, validate it against an allow-list of paths/origins.

## Provider tokens: do you need them?

If you only need login, **discard the provider's access/refresh tokens**: less to protect. If you call the provider's APIs later (read calendar, list repos), store the provider's refresh token **encrypted at rest**, request minimal scopes, handle revocation and expiry, and let users disconnect ([OAuth 2.0](./08-oauth2.md)).

## Linking, unlinking, and recovery

- **Connect** an additional provider from an authenticated session (`/auth/google` while logged in, with the flow carrying the current user context safely, for example in server-side state).
- **Unlink** only if the user retains another way to log in (a password or another provider); otherwise they lock themselves out.
- Provide **account recovery** that doesn't depend solely on a provider the user may lose access to.
- Respect **deletion**: if a user deletes their provider account, your identity row is just stale; don't treat the provider's email as a recovery channel without verification.

## Testing

- You can't call the real provider in CI. Unit-test `loginWithProvider` with fake profiles: known identity, new user, **unverified email with existing account (must not link)**, verified email policy, missing email, concurrent creation.
- E2E: override the Passport guard/strategy (`overrideGuard(GoogleAuthGuard)`) to inject a fake `req.user`, then assert the callback issues credentials and redirects to the fixed URL ([E2E testing](../01-testing/06-e2e-testing.md)).
- Manual/integration: a provider sandbox or test project with a dedicated test redirect URI.

## Common mistakes

- **Auto-linking accounts by unverified email** (account takeover).
- **Identifying users by email** rather than `(provider, sub)`.
- **Trusting profile fields blindly** without checking what the provider guarantees.
- **Skipping `state` verification** to make the flow work in a stateless API.
- **Tokens in redirect URLs**, or unvalidated `redirect` parameters (open redirects).
- **Registering many/wildcard callback URLs** at the provider.
- **Storing provider tokens in plaintext** (or storing them when unneeded).
- **Letting users unlink their only login method.**
- **Not handling concurrent first logins** (duplicate users/identities).
- **Hard-coding client secrets** in code instead of secret management.

## Debugging

- `redirect_uri_mismatch`: registered callback differs from `callbackURL` exactly (scheme, host, port, path).
- `Unknown authentication strategy "google"`: the strategy isn't a provider in the module that uses the guard.
- `state` errors / "Failed to verify request state": the session/cookie holding the state wasn't available on the callback (cookie settings, different host, or session middleware missing).
- `profile.emails` undefined: missing `email` scope, or the user's provider settings hide it (GitHub especially); handle gracefully.
- Login works locally, fails in production: the production callback URL isn't registered, or a proxy changes the scheme/host seen by Passport (`trust proxy`).
- Duplicate-key errors on first login bursts: handle the unique constraint violation by re-reading the identity.

## Quick Summary

- Social login = provider authenticates, **your app** maps the identity to a user and issues its own session/tokens.
- Store identities as `(provider, provider_user_id)` in a separate table; a user can have several, and password-less users are normal.
- **Never auto-link by unverified email**; require proof of ownership for existing accounts and trust only verified emails from trusted providers.
- Verify `state` (and use PKCE/OIDC checks); deliver credentials via cookies and a fixed redirect, never tokens in URLs; guard against open redirects.
- Store provider tokens only if you need them (encrypted); support unlinking safely and recovery; test linking logic thoroughly with fake profiles.

## Next

[Two-factor authentication →](./10-two-factor-authentication.md)
