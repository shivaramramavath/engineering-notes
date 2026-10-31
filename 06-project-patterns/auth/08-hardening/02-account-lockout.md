# Account Lockout

Lockout slows password guessing by refusing further attempts after repeated failures. The catch: a naive lockout lets an attacker **lock real users out on purpose** (a denial of service). Good designs make guessing expensive without giving attackers a switch that disables other people's accounts.

**Prerequisites:** [00-rate-limiting.md](./00-rate-limiting.md), [01-enumeration-protection.md](./01-enumeration-protection.md)

---

## The trade-off

```text
Strict lockout (5 failures → locked 24 h)
   + stops brute force dead
   − attacker types 5 wrong passwords for victim@x.com every day → victim never gets in (DoS)

No lockout
   + nobody gets locked out by attackers
   − unlimited online guessing against weak passwords
```

Aim for **friction proportional to suspicion**: delay and challenge, don't ban.

---

## Strategies

| Strategy | How | Pros | Cons |
|---|---|---|---|
| Hard lock until manual/email unlock | Locked after N failures until reset | Simple | Easy DoS, support load |
| **Temporary lock** | Locked for a fixed time | Simple, self-heals | Attacker re-triggers on schedule |
| **Progressive delay / exponential backoff** | Each extra failure doubles the wait | Slows guessing sharply, limited DoS impact | Needs state per account |
| **Scope by (account, IP/device)** | Lock the pair, not the account globally | Attacker can't lock the owner out from their own device | More keys; botnets rotate IPs |
| **CAPTCHA / challenge after N failures** | Require proof of humanity | Blocks bots, humans still get in | Accessibility, UX, solver services |
| **Known-device bypass** | A device that previously signed in successfully isn't blocked | Owner unaffected during an attack | Needs device tokens |
| **MFA** | Second factor makes guessed passwords insufficient | Strongest complement | Doesn't stop the guesses themselves |

NIST SP 800-63B only requires limiting **consecutive failed attempts** and sets a fairly generous upper bound (on the order of 100), because heavy lockouts hurt more than they help; combine moderate limits with checks of breached/common passwords ([01-password-policy.md](../02-passwords/01-password-policy.md)). Check the current revision for exact wording.

---

## A practical design

```text
Per (account, IP):   5 failures / 15 min  → block that pair for the window       (stops one location guessing)
Per account:         10 failures / 15 min → require CAPTCHA/step-up, add delay    (stops distributed guessing)
Per account:         escalating delay on each further failure (1 s, 2 s, 4 s ... capped at ~15 min)
Known device cookie: bypasses the per-pair block (still counts toward account metrics)
Success:             clears counters for that pair, decays the account counter
Notification:        email the owner after repeated failures / a successful login after many
```

The attacker's cost grows exponentially, while the owner on a known device is unaffected and elsewhere can recover via password reset (which doesn't require guessing).

---

## Implementation

Keys use a **hash of the normalized email** so they don't hold personal data and work the same for unknown accounts:

```js
const acct = (emailHash) => `lock:acct:${emailHash}`;          // failure counter (window)
const pair = (emailHash, ip) => `lock:pair:${emailHash}:${ip}`;
const until = (emailHash) => `lock:until:${emailHash}`;        // account-level cooldown

const WINDOW = 15 * 60;
```

### Check before verifying the password

```js
export async function assertNotLocked(emailHash, ip, knownDevice) {
  const [ttl, pairFails] = await Promise.all([
    redis.pTTL(until(emailHash)),
    redis.get(`rl:${pair(emailHash, ip)}`),          // counters live under the limiter's "rl:" prefix
  ]);

  if (ttl > 0 && !knownDevice) throw new TooManyAttempts(Math.ceil(ttl / 1000));
  if (Number(pairFails) >= 5 && !knownDevice) throw new TooManyAttempts(WINDOW);
}
```

Checking **first** matters: it avoids spending an Argon2 hash on blocked attempts, and it means a locked account refuses even the **correct** password (otherwise an attacker learns the right guess during the lock).

### Record a failure

```js
export async function recordFailure(emailHash, ip) {
  await hit(`${pair(emailHash, ip)}`, 5, WINDOW);                          // atomic counter from 00-rate-limiting.md
  const { count } = await hit(acct(emailHash), 10, WINDOW);

  if (count > 10) {
    const over = count - 10;
    const seconds = Math.min(2 ** over, 15 * 60);                          // 2, 4, 8 ... capped at 15 min
    await redis.set(until(emailHash), "1", { EX: seconds });               // escalating cooldown
  }
}

export async function recordSuccess(emailHash, ip) {
  await redis.del(`rl:${pair(emailHash, ip)}`);                            // clear this pair; keep account counter to decay naturally
}
```

(The helpers reuse the atomic limiter from [00-rate-limiting.md](./00-rate-limiting.md), including its `rl:` key prefix.)

### In the login handler

```js
export async function login(req, res) {
  const { email, password } = loginSchema.parse(req.body);
  const emailHash = hashToken(email);
  const knownDevice = await isKnownDevice(req, emailHash);

  try {
    await assertNotLocked(emailHash, req.ip, knownDevice);
  } catch (e) {
    if (e instanceof TooManyAttempts) {
      res.set("Retry-After", String(e.retryAfter));
      return res.status(429).json({ message: "Too many attempts. Try again later or reset your password." });
    }
    throw e;
  }

  const user = await User.findOne({ email }).select("+passwordHash");
  const valid = await argon2.verify(user?.passwordHash ?? DUMMY_HASH, password);

  if (!user || !valid) {
    await recordFailure(emailHash, req.ip);                                // counts unknown accounts too
    return res.status(401).json({ message: "Invalid email or password" });
  }

  await recordSuccess(emailHash, req.ip);
  /* ...MFA challenge or session creation... */
}
```

Properties worth noticing:

- **Unknown emails are counted and locked identically**, so the lock doesn't reveal existence.
- The same `429` message is used regardless of which counter tripped.
- Success clears only the pair counter; the account-level count simply expires with its window. Clearing it on success would hand a distributed attacker a fresh guess budget every time the real owner logs in, even though the guessing is still going on.

### Known-device token (optional)

After a successful login (and MFA), set a long-lived random cookie and store its hash:

```js
const deviceId = randomToken(32);
await redis.set(`device:${emailHash}:${hashToken(deviceId)}`, "1", { EX: 90 * 24 * 3600 });
res.cookie("did", deviceId, { httpOnly: true, secure: true, sameSite: "strict", maxAge: 90 * 24 * 3600 * 1000, path: "/api/auth" });
```

The device cookie lets the legitimate owner bypass **temporary** blocks. It's a convenience signal, not a credential: the password is still verified, and an attacker who steals the cookie only skips the block, not authentication.

---

## Notifying the user

- After N failures: "We noticed several failed sign-in attempts." (Rate-limit these emails too.)
- After a successful login following many failures, or from a new device/location: "New sign-in to your account."
- Include a link to change the password and sign out other sessions, never a "click to unlock" link that is itself an attack vector.

Only notify real accounts, and send through a queue so timing doesn't leak ([01-enumeration-protection.md](./01-enumeration-protection.md)).

---

## CAPTCHA step

After the account threshold, require a CAPTCHA token (reCAPTCHA, hCaptcha, Cloudflare Turnstile) on login:

```js
if (accountFailures >= 5) {
  if (!(await verifyCaptcha(req.body.captchaToken, req.ip))) return res.status(401).json({ code: "CAPTCHA_REQUIRED" });
}
```

The server must verify the token with the provider; checking that "a token was sent" proves nothing. Keep a non-CAPTCHA path (support, accessible alternatives) for users who can't solve them.

---

## MFA and other secondary endpoints

The same logic applies wherever a guess is accepted:

| Endpoint | Counter |
|---|---|
| MFA code | Per challenge (5) + per account ([00-mfa-overview.md](../05-mfa/00-mfa-overview.md)) |
| Recovery code | Per account, strict |
| Step-up password | Per session and account ([05-step-up-auth.md](../05-mfa/05-step-up-auth.md)) |
| Password change (current password) | Per user |
| Reset token | Per IP; tokens are 256-bit so guessing isn't realistic, but limit anyway |

---

## Admin and unlocking

- Admins can view lock state and clear counters (`DEL` the keys) after verifying the user out-of-band; audit the action ([05-logging-and-audit.md](./05-logging-and-audit.md)).
- Self-service recovery = **password reset** (email proof). It doesn't depend on lock state, so it also serves as the escape hatch from a DoS lock.
- Locks expire on their own (TTL); there's no permanent lock flag to forget about. A **permanent** `isActive: false` is a separate admin decision, not a lockout.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Permanent lock after N failures | Anyone can DoS any account |
| Locking only existing accounts | Enumeration oracle |
| Verifying the password before checking the lock | Wastes CPU, and a correct guess during lock still reveals success |
| Allowing the correct password through during a lock | Attackers learn the password mid-lock |
| Counters stored in process memory | Reset on restart, bypassed with multiple instances |
| Unhashed email in keys/logs | Personal data in Redis/monitoring |
| "Unlock" links in emails | Phishing template; use "change password" instead |
| Same counter for IP and account | One shared NAT locks everyone, or one botnet bypasses |
| Not clearing the pair counter on success | Owner penalized for past typos |
| Only checking the password endpoint | Attackers use MFA/reset/step-up endpoints to guess |

## Debugging

| Symptom | Cause |
|---|---|
| Owner locked out after a few typos | Pair threshold too low, or counters not cleared on success |
| Attackers aren't slowed | Distributed IPs, and no account-level counter/delay |
| Lock never ends | Missing TTL on a counter or cooldown key |
| Everyone behind an office NAT locked | Per-IP threshold too low; rely on pair/account counters and known-device bypass |
| Users locked immediately after deploy | Redis state shared between environments, or clock/TTL unit mix-up (`pTTL` ms vs `TTL` s) |

---

## Quick summary

- Lockout trades guess-resistance against attacker-induced DoS; avoid permanent locks.
- Use escalating delays, scope by (account, IP), CAPTCHA after thresholds, and known-device bypass.
- Check the lock before verifying; count unknown accounts too; same response always.
- Notify owners, give password reset as the escape hatch, and let TTLs clear locks.
- MFA and other guess endpoints need their own counters.

**Next:** [03-cors-and-headers.md](./03-cors-and-headers.md)
