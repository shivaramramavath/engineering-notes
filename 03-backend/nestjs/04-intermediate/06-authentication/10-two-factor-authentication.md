# Two-Factor Authentication

Passwords get phished, reused, and leaked. **Two-factor authentication (2FA/MFA)** requires a second proof, something the user **has** (an authenticator app, a security key) in addition to something they **know** (the password). It's the single most effective upgrade to password login. This note builds the most common form, **TOTP** (the 6-digit codes from Google Authenticator, Authy, 1Password, and similar apps), including the parts that are easy to get wrong: the enrollment flow, the "half-logged-in" state, replay, and recovery.

Prerequisites: [Passport local strategy](./02-passport-and-local-strategy.md), [JWT](./04-jwt.md) or [sessions](./07-session-authentication.md), [password hashing](./03-password-hashing.md) (recovery codes).

> Library APIs differ between versions. The examples use the `otplib` package's classic `authenticator` API; newer major versions of `otplib` reorganized their API, so check the current README. The flow and security rules apply regardless of library.

## Factor options

| Factor | Strength | Notes |
|--------|----------|-------|
| **TOTP** (authenticator app) | Good | Offline, free, standard (RFC 6238). Phishable in real time by a proxy page |
| **WebAuthn / passkeys / security keys** | **Strongest** | Phishing-resistant (bound to the origin). Libraries such as `@simplewebauthn/server` exist; more complex to build |
| Push approval | Good | Needs an app/infrastructure; beware "MFA fatigue" |
| **SMS / voice codes** | Weak | SIM swapping and interception; acceptable only as a fallback to nothing |
| Email codes | Weak-moderate | Only as strong as the mailbox |
| **Recovery codes** | Fallback | Single-use backup when the device is lost |

Offer TOTP at minimum; support passkeys if you can; avoid SMS as the primary second factor.

## How TOTP works

Server and authenticator app share a **secret**. Both compute a code from `HMAC(secret, floor(time / 30s))`, truncated to 6 digits. The server verifies the user's code against the current time window (usually allowing ±1 step for clock drift). The secret is shown **once**, at enrollment, usually as a QR code of an `otpauth://` URL.

```text
otpauth://totp/MyApp:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=MyApp
```

## Data model

```text
users
├── ...
├── totp_secret_encrypted   text null        // never plaintext
├── totp_enabled            boolean default false
├── totp_last_used_step     bigint null      // replay protection

recovery_codes
├── id, user_id
├── code_hash               // hashed like a password/token
├── used_at null
```

**Encrypt the TOTP secret at rest** (application-level encryption with a key from a secret manager/KMS; the database alone shouldn't reveal it). A leaked plaintext TOTP secret lets an attacker generate valid codes forever. Unlike passwords you can't hash it (the server needs the secret to compute codes), so encryption is the control ([secrets management](../../07-production/01-security/06-secrets-management.md)).

## Enrollment: a two-step flow

Never enable 2FA on the first request. Confirm the user's app actually works first, or you can lock them out.

```ts
import { authenticator } from 'otplib';
import * as QRCode from 'qrcode';

@Injectable()
export class TwoFactorService {
  constructor(private readonly users: UsersService, private readonly crypto: SecretCipher) {}

  // Step 1: generate a secret, store it as PENDING (not enabled), return the QR
  async beginSetup(user: { id: string; email: string }) {
    const secret = authenticator.generateSecret();
    await this.users.saveTotpSecret(user.id, this.crypto.encrypt(secret), { enabled: false });

    const otpauthUrl = authenticator.keyuri(user.email, 'MyApp', secret);
    const qrDataUrl = await QRCode.toDataURL(otpauthUrl);
    return { qrDataUrl, manualEntryKey: secret };          // show once; offer the key for manual entry
  }

  // Step 2: user submits a code from their app → enable, issue recovery codes
  async confirmSetup(userId: string, code: string) {
    const secret = this.crypto.decrypt(await this.users.getTotpSecret(userId));
    if (!authenticator.verify({ token: code, secret })) throw new BadRequestException('Invalid code');

    await this.users.enableTotp(userId);
    return { recoveryCodes: await this.createRecoveryCodes(userId) };   // returned ONCE
  }
}
```

Both endpoints require an **already authenticated** user, and enabling/disabling 2FA should require **re-authentication** (password or current second factor) to prevent a stolen session from adding the attacker's authenticator.

## Login with 2FA: a two-step flow

The tricky part: after the password step the user is **not yet authenticated**. Don't issue normal credentials until the second factor passes.

```text
POST /auth/login        { email, password }
   └─ password OK, 2FA enabled ─► 200 { mfaRequired: true, mfaToken }     // NO access/refresh tokens
POST /auth/2fa/verify   { mfaToken, code }
   └─ code OK ─► issue the real access + refresh tokens / session
```

The `mfaToken` is a **short-lived, single-purpose** credential proving the password step succeeded:

```ts
// after password verification
if (user.totpEnabled) {
  const mfaToken = await this.jwt.signAsync(
    { sub: user.id, purpose: 'mfa' },
    { expiresIn: '5m', secret: this.config.getOrThrow('JWT_MFA_SECRET') },   // different secret/audience from access tokens
  );
  return { mfaRequired: true, mfaToken };
}
return this.issueTokens(user);
```

```ts
async verifyLogin(mfaToken: string, code: string) {
  const payload = await this.jwt.verifyAsync(mfaToken, { secret: this.config.getOrThrow('JWT_MFA_SECRET') })
    .catch(() => { throw new UnauthorizedException(); });
  if (payload.purpose !== 'mfa') throw new UnauthorizedException();

  const user = await this.users.findActiveById(payload.sub);
  const ok = await this.twoFactor.verifyTotpOrRecovery(user, code);   // see below
  if (!ok) throw new UnauthorizedException('Invalid code');

  return this.auth.issueTokens(user);
}
```

Rules:

- The MFA token must **not** be accepted by your normal authentication guard (different secret and `purpose`/audience), or the password step alone would grant access.
- With **sessions**, keep a `pendingUserId` in the session after the password step (not `userId`), and promote it to `userId` (after regenerating the session id) once the code is verified ([sessions](./07-session-authentication.md)).
- Keep the password step's responses **indistinguishable** between "wrong password" and "no such user"; only reveal `mfaRequired` after a *successful* password check.

## Verifying codes safely

```ts
async verifyTotp(user: UserWithTotp, code: string) {
  const secret = this.crypto.decrypt(user.totpSecretEncrypted);

  // otplib: verify within the allowed window, then enforce one-time use per time step
  const delta = authenticator.checkDelta(code, secret);          // null if invalid, else the step offset
  if (delta === null) return false;

  const currentStep = Math.floor(Date.now() / 30_000) + delta;
  if (user.totpLastUsedStep != null && currentStep <= user.totpLastUsedStep) return false;   // replay

  await this.users.setTotpLastUsedStep(user.id, currentStep);    // do atomically (conditional UPDATE)
  return true;
}
```

- **Replay protection:** a valid code stays valid for its time window (~30-90 s). Record the last accepted time step per user and reject codes at or before it, so an intercepted code can't be reused. Update atomically so two concurrent submissions can't both succeed.
- **Window:** allow ±1 step for clock drift (library option). A wider window weakens security.
- **Rate limit** the verification endpoint hard (per user **and** per IP), and lock or back off after repeated failures: a 6-digit code has only a million possibilities, so unthrottled guessing succeeds ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)). Throttle on the `mfaToken`/user, not only the IP.
- Use constant-time comparison where you compare codes yourself (libraries do this internally).
- Treat the verification failure message generically.

## Recovery codes

Users lose phones. Without a fallback they're locked out forever, or your support team becomes the weakest link.

```ts
import { randomBytes } from 'node:crypto';

async createRecoveryCodes(userId: string) {
  const codes = Array.from({ length: 10 }, () => randomBytes(5).toString('hex'));   // e.g. "a1b2c3d4e5"
  await this.recoveryCodes.replaceAll(
    userId,
    await Promise.all(codes.map(async (c) => ({ codeHash: await this.hasher.hash(c) }))),
  );
  return codes;       // shown to the user ONCE; only hashes are stored
}
```

- Generate **8-10 single-use codes** with a CSPRNG (enough entropy, at least ~40 bits each).
- **Store hashes** (a slow hash is appropriate if codes are short; if long and random, SHA-256 suffices).
- Mark a code **used** atomically when consumed; invalidate and regenerate on request.
- Let the user download/print them and tell them to store them offline.
- Accept a recovery code in the same endpoint as the TOTP code, and **alert the user** (email) when one is used.

## Disabling, resetting, and edge cases

- **Disable 2FA** only after re-authenticating (password + current code or a recovery code), and notify the user by email.
- **Admin/support resets** are a social-engineering target: require strong identity verification, log and alert.
- **Password reset must not bypass 2FA.** After a reset, the user still completes the second factor on login.
- **Sensitive actions (step-up):** require a fresh 2FA check for high-risk operations (changing email/password, payouts) even in an existing session, tracked via a "last MFA at" timestamp in the session/token claims.
- **Remember this device:** optional "trust this browser for 30 days" uses a long-lived, random, hashed, revocable device token; it lowers security slightly, so make it opt-in and revocable.
- **Revoke sessions/refresh tokens** when 2FA is enabled, disabled, or reset ([refresh tokens](./05-refresh-tokens-and-logout.md)).
- **Clock drift:** device clocks differ; enable a ±1 window and show a helpful message ("check your phone's time") on failure.
- **Social login users** ([social login](./09-social-login.md)): the provider may already enforce MFA; decide whether your app adds its own for sensitive actions.

## Passkeys / WebAuthn (brief)

WebAuthn replaces shared secrets with public-key cryptography bound to your origin, so a phishing site can't capture a usable credential. Server libraries (for example `@simplewebauthn/server`) handle registration and assertion ceremonies; you store credential public keys and signature counters. It's the best option when you can support it, either as a second factor or as passwordless login, but the browser/UX side is more involved. Evaluate it once TOTP is solid.

## Testing

- Unit-test the verification logic with a fixed secret and controlled time (`authenticator.generate` in tests, or fake timers): valid code, wrong code, **replayed code rejected**, drifted code within the window, expired window.
- E2E: login without 2FA; login with 2FA returns `mfaRequired` and **no tokens**; the MFA token alone can't access protected routes; verify issues tokens; recovery codes are single-use; failed attempts get rate-limited ([E2E testing](../01-testing/06-e2e-testing.md)).
- Never include real secrets or QR codes in test fixtures committed to git.

## Common mistakes

- **Storing TOTP secrets in plaintext.**
- **Enabling 2FA without a confirmation step**, locking users out.
- **Issuing real tokens after the password step**, or accepting the MFA token as a normal access token.
- **No rate limiting** on code verification (6 digits are brute-forceable).
- **No replay protection** (same code accepted repeatedly).
- **No recovery codes**, or storing them in plaintext.
- **Letting password reset or account recovery bypass 2FA.**
- **Disabling 2FA without re-authentication**, or without notifying the user.
- **Relying on SMS** as the main second factor.
- **Wide verification windows** that weaken the code's lifetime.
- **Different responses revealing whether an account exists or has 2FA** before the password step succeeds.

## Debugging

- Codes always rejected: server or phone clock drift (check time sync on both), wrong secret (encoding, decrypted incorrectly), or digits/period/algorithm mismatch between the `otpauth` URL and the verifier (defaults are 6 digits, 30 s, SHA-1).
- Works on one device only: the secret was re-generated by a repeated setup call; keep one pending secret per setup and don't overwrite an enabled secret.
- Users locked out: no recovery path; add recovery codes and a verified support process.
- Intermittent failures when codes are used twice quickly: your replay protection is rejecting a legitimate retry within the same step; ask users to wait for the next code.
- QR scans but wrong account name: check `keyuri` arguments (account label, issuer).

## Quick Summary

- 2FA adds "something you have"; TOTP (RFC 6238) is the practical baseline, passkeys/WebAuthn are strongest, SMS is weak.
- Enroll in **two steps** (generate pending secret → confirm with a code → enable → show recovery codes once); require re-authentication to change 2FA.
- Login is **two steps**: password → short-lived single-purpose `mfaToken` (never normal credentials) → verify code → issue real credentials.
- Encrypt TOTP secrets at rest; enforce **replay protection**, a small window, and **aggressive rate limiting**.
- Provide hashed, single-use **recovery codes**; make reset/recovery flows respect 2FA; revoke sessions on changes; notify users.

## Next

Section complete. Continue with [Authorization](../07-authorization/README.md), where `req.user` from these flows is used to decide what each user may do.

← Back to [Authentication overview](./README.md)
