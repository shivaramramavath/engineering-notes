# Refresh Tokens and Logout

Short-lived access tokens are safe but annoying: users shouldn't log in every 15 minutes. **Refresh tokens** solve this: a long-lived, **revocable, server-tracked** credential whose only job is to get a new access token. Because the server remembers them, you get logout, "sign out of all devices", and theft detection that pure stateless JWTs can't offer.

Prerequisites: [JWT](./04-jwt.md), [password hashing](./03-password-hashing.md) (token hashing), [transactions](../02-database-foundations/04-transactions.md).

## The model

```text
login ──► access token (15m, stateless)  +  refresh token (7–30d, stored hashed on the server)

API calls:      Authorization: Bearer <access>
access expires: POST /auth/refresh { refreshToken }
                    │  server: look up hash → valid? → ROTATE
                    ▼
               new access token + NEW refresh token (old one revoked)

logout:         revoke the refresh token (and its family)
```

| | Access token | Refresh token |
|-|--------------|---------------|
| Lifetime | Minutes | Days/weeks |
| Validation | Signature check, no DB | **Database lookup** |
| Revocable | Not individually | **Yes** |
| Sent to | Every API request | **Only** the refresh endpoint |
| Form | JWT | Opaque random string (preferred) or a JWT |

## Use opaque refresh tokens, stored hashed

A refresh token doesn't need to be a JWT. A random string you look up server-side is simpler and just as secure:

```ts
import { randomBytes, createHash } from 'node:crypto';

const newRefreshToken = () => randomBytes(48).toString('base64url');     // CSPRNG, plenty of entropy
const hashToken = (t: string) => createHash('sha256').update(t).digest('hex');
```

**Store only the hash**, so a stolen database doesn't yield usable tokens (SHA-256 is fine because the token is high-entropy; passwords need slow hashes, tokens don't).

```text
refresh_tokens
├── id            uuid (pk)
├── user_id       fk → users
├── family_id     uuid        // all tokens descended from one login
├── token_hash    char(64)    // unique, indexed
├── expires_at    timestamptz
├── revoked_at    timestamptz null
├── replaced_by   uuid null   // the token that superseded this one (optional audit trail)
├── created_at    timestamptz
├── user_agent / ip (optional, for session lists and anomaly detection)
```

## Issuing tokens at login

```ts
async login(user: SafeUser, meta: { ip?: string; userAgent?: string }) {
  const familyId = randomUUID();
  return this.issuePair(user, familyId, meta);
}

private async issuePair(user: SafeUser, familyId: string, meta: ClientMeta) {
  const refreshToken = newRefreshToken();
  await this.tokens.create({
    userId: user.id,
    familyId,
    tokenHash: hashToken(refreshToken),
    expiresAt: new Date(Date.now() + 30 * 24 * 3600_000),
    ...meta,
  });
  const accessToken = await this.jwt.signAsync({ sub: user.id, roles: user.roles });
  return { accessToken, refreshToken };
}
```

## Rotation: every refresh issues a new refresh token

Never reuse a refresh token. On each use, **revoke the old one and issue a new one**:

```ts
async refresh(presented: string, meta: ClientMeta) {
  const record = await this.tokens.findByHash(hashToken(presented));
  if (!record) throw new UnauthorizedException();

  // Reuse detection: this token was already used/revoked → likely stolen
  if (record.revokedAt) {
    await this.tokens.revokeFamily(record.familyId);              // kill the whole chain
    throw new UnauthorizedException();
  }
  if (record.expiresAt < new Date()) throw new UnauthorizedException();

  // Atomically claim the token: only one concurrent request can win
  const claimed = await this.tokens.revokeIfActive(record.id);    // UPDATE ... SET revoked_at = now() WHERE id = ? AND revoked_at IS NULL
  if (!claimed) {
    await this.tokens.revokeFamily(record.familyId);
    throw new UnauthorizedException();
  }

  const user = await this.users.findActiveById(record.userId);
  if (!user) throw new UnauthorizedException();

  return this.issuePair(user, record.familyId, meta);             // same family, new token
}
```

Why this design:

- **Reuse detection:** if an old (already rotated) token shows up again, either the legitimate client or an attacker is replaying it. You can't tell which, so revoke the **entire family** and force a fresh login. This turns token theft into a detectable event.
- **Atomic claim:** the conditional `UPDATE ... WHERE revoked_at IS NULL` (check affected rows) prevents two concurrent refresh calls from both succeeding. Don't do "read, check, write" without it ([read-modify-write races](../02-database-foundations/04-transactions.md)).
- The user is re-fetched, so disabled/deleted users can't keep refreshing.

### The parallel-request wrinkle

SPAs and mobile apps often fire several requests when the access token expires, each triggering a refresh with the **same** refresh token. With strict rotation, the second request looks like reuse and kills the family, logging the user out spuriously. Mitigations:

- **Client-side:** serialize refreshes (a single in-flight refresh promise shared by all callers).
- **Server-side grace window:** allow a just-rotated token to be presented again for a few seconds (returning the same new pair, or accepting without family revocation), at the cost of a slightly weaker reuse signal.

Pick one deliberately and test it.

## Logout

```ts
async logout(presented: string) {
  const record = await this.tokens.findByHash(hashToken(presented));
  if (record) await this.tokens.revokeFamily(record.familyId);     // or just this token
}

async logoutAll(userId: string) {
  await this.tokens.revokeAllForUser(userId);
}
```

- Make logout **idempotent** and always return success (don't reveal whether the token existed).
- **Access tokens remain valid** until they expire. With a 15-minute lifetime that's the accepted window. If it's unacceptable, add a [token version or denylist](./04-jwt.md), or use [sessions](./07-session-authentication.md).
- Also clear any client-side storage/cookies ([cookie authentication](./06-cookie-authentication.md)).
- Revoke all of a user's refresh tokens on **password change/reset**, 2FA changes, account disable, and "sign out everywhere".

## Refresh token endpoints

```ts
@Public()
@Post('refresh')
@HttpCode(200)
refresh(@Body() dto: RefreshDto, @Req() req: Request) {
  return this.auth.refresh(dto.refreshToken, { ip: req.ip, userAgent: req.get('user-agent') });
}

@Post('logout')
@HttpCode(204)
async logout(@Body() dto: RefreshDto) { await this.auth.logout(dto.refreshToken); }
```

The refresh endpoint is `@Public()` because the access token is expired by definition. Rate-limit it ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)). `req.ip` is only meaningful behind a proxy if `trust proxy` is configured.

## Where the client stores the refresh token

| Client | Recommended |
|--------|-------------|
| Browser SPA | `httpOnly; Secure; SameSite` cookie scoped to the refresh path (`Path=/auth`), access token kept **in memory** ([cookie auth](./06-cookie-authentication.md)). A cookie needs CSRF consideration on the refresh endpoint |
| Mobile app | Platform secure storage (Keychain/Keystore) |
| Server-to-server | Prefer other flows (client credentials) over refresh tokens ([OAuth 2.0](./08-oauth2.md)) |

Don't put refresh tokens in `localStorage`: they are the **most valuable** credential you issue, and XSS would hand over a 30-day key.

## Housekeeping and visibility

- **Delete expired/revoked tokens** with a scheduled job ([cron](../../05-advanced/02-background-processing/08-scheduling-with-cron.md)); the table grows with every login/refresh.
- Expose **"active sessions"** (device, last used, IP) so users can revoke individual families.
- **Monitor** reuse-detection events and refresh failures; spikes indicate attacks or client bugs.
- Index `token_hash` (unique), `user_id`, `family_id`, and `expires_at` ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).

## Alternative: refresh tokens as JWTs

You can issue the refresh token as a signed JWT (with `jti` and a longer `exp`) and still store the `jti`/hash server-side for revocation. It adds nothing over an opaque token except a parseable `exp`, and invites the "stateless" illusion. Use opaque tokens unless an external system requires JWT refresh tokens. If you do use JWTs, sign them with a **different secret** than access tokens.

## Testing

- Login returns both tokens; the refresh token is **not** stored in plaintext.
- Refresh returns a new pair and revokes the old token.
- Reusing an old refresh token revokes the family and returns 401.
- Expired and unknown tokens return 401; logout then refresh returns 401.
- Concurrent refresh with the same token: exactly one wins (or both within your grace window).
- Password change revokes all refresh tokens.

Run these against a real database ([integration testing](../01-testing/05-integration-testing.md)); mocks can't prove the atomic claim.

## Common mistakes

- **Storing refresh tokens in plaintext.**
- **No rotation** (a stolen token works for its whole lifetime) or **no reuse detection**.
- **Non-atomic "check then revoke"**, allowing concurrent double-use.
- **Long-lived access tokens instead of refresh tokens.**
- **Sending refresh tokens on every request** (they should only go to the refresh endpoint).
- **Refresh tokens in `localStorage`.**
- **Not revoking tokens on password change.**
- **Assuming logout revokes access tokens.**
- **No cleanup job**, so the table grows forever.
- **Not handling parallel refresh requests**, causing random logouts.
- **Using the same secret/format for access and refresh JWTs.**

## Debugging

- Users randomly logged out: parallel refreshes tripping reuse detection; serialize on the client or add a grace window.
- Refresh always 401: hash mismatch (encoding differences, trimming), token already rotated, expiry in the wrong unit, or clock skew.
- Logout "doesn't work": the access token is still valid until `exp`; verify the refresh token is revoked.
- Cookie not sent to the refresh endpoint: `Path`, `SameSite`, `Secure`, or CORS credentials misconfigured ([cookie authentication](./06-cookie-authentication.md)).
- Table bloat/slow lookups: missing index on `token_hash` or no cleanup job.

## Quick Summary

- Access token (minutes, stateless) + refresh token (days, **opaque, stored hashed, revocable**).
- **Rotate** on every refresh; **detect reuse** and revoke the whole family; claim tokens with an **atomic conditional update**.
- Handle parallel refreshes (client single-flight or short server grace window).
- Logout = revoke refresh token(s); access tokens live until `exp`; revoke everything on password change.
- Keep refresh tokens out of `localStorage` (httpOnly cookie in browsers, secure storage on mobile); clean up and monitor.

## Next

[Cookie authentication →](./06-cookie-authentication.md)
