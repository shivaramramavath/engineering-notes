# Threat Model

Security design starts by asking "who attacks what, and how?" This note lists the attacks that shape the authentication architecture, and which control defends against each. Later chapters implement the controls; this one explains why they exist.

**Prerequisite:** [01-credential-types.md](./01-credential-types.md)

---

## The attacker questions

For every credential, ask:

```text
Can it be GUESSED?     → brute force, credential stuffing
Can it be STOLEN?      → XSS, database leak, logs, network
Can it be REPLAYED?    → reuse a captured token
Can it be FORCED?      → CSRF: the browser sends it for the attacker
```

---

## Threats and defenses

| Threat | What happens | Main defenses |
|---|---|---|
| **Brute force** | Many guesses against one account | Rate limiting, lockout/backoff, strong hashing |
| **Credential stuffing** | Leaked passwords from other sites tried here | Rate limiting per IP and account, MFA, breached-password checks |
| **Database leak** | Attacker reads MongoDB/Redis | Argon2id hashes, store only token hashes |
| **XSS** | Injected script runs in your page | Keep access token in memory, refresh token HttpOnly, CSP, output escaping |
| **CSRF** | Another site makes the browser send your cookie | `SameSite`, restricted cookie `Path`, CSRF token where needed |
| **Token theft / replay** | Captured token used by attacker | Short access TTL, refresh rotation, reuse detection, HTTPS |
| **Account enumeration** | Responses reveal which emails exist | Generic responses, consistent timing |
| **Session fixation / hijack** | Attacker obtains or plants a session id | New session on login, secure cookies, revocation |
| **Phishing / password reuse** | User gives away the password | MFA, passkeys (WebAuthn) |
| **Timing attacks** | Comparison time leaks correct bytes | Constant-time comparison |
| **Open redirect / OAuth abuse** | Auth code sent to attacker's URL | Exact redirect URI match, `state`, PKCE |
| **Log / URL leakage** | Tokens in logs, referrers, history | Never log secrets, `Referrer-Policy`, strip tokens from URL |

---

## Walkthroughs

### XSS: why the token placement matters

```text
Attacker script runs in your page
  ├── localStorage token        → read and exfiltrated  ❌
  ├── token in JS memory        → can still be USED while the page is open ⚠️
  └── HttpOnly refresh cookie   → JS cannot read it ✅
```

Honest limit: HttpOnly does not stop XSS. A script can still make authenticated requests from inside the page. The design reduces damage (no long-lived stealable secret), but fixing XSS itself (escaping, CSP) remains mandatory.

### CSRF: why cookies need care

```text
evil.com  ──form POST──►  yourapp.com/api/auth/refresh
              browser attaches cookies automatically
```

The access token is sent via the `Authorization` header by your own JS, so it is not vulnerable to CSRF. Only cookie-carried credentials are. Defenses here: `SameSite=Strict` and `Path=/api/auth/refresh`. See [csrf](../04-sessions/05-csrf.md).

### Replay and rotation

```text
Attacker steals R2 ──┐
User refreshes: R2 → R3 (R2 now invalid)
Attacker sends R2    └─► hash mismatch → reject and revoke session
```

---

## What a leak of each store costs

| Leaked | Impact | Why it is limited |
|---|---|---|
| MongoDB | Emails, password hashes | Argon2id is expensive to crack; no tokens stored |
| Redis | Session and token hashes | Hashes cannot be turned back into valid tokens |
| JWT signing secret | Attacker can forge any access token | Rotate immediately ([key rotation](../00-setup/01-env-and-secrets.md)) |
| Logs | Anything accidentally logged | Hence the no-secrets-in-logs rule |

The JWT signing secret is the single highest-value secret: compromise means forged identities, including admin.

---

## Out of scope for this repo

State clearly what is not defended, so it isn't assumed: a compromised user device or malware, a malicious server operator, social engineering of support staff, and DDoS (handled at the infrastructure layer).

---

## Quick summary

- For each credential ask: guessed, stolen, replayed, or forced?
- Layered defenses: slow hashing, token hashing, short TTLs, rotation, rate limits, MFA.
- HttpOnly + memory storage reduces XSS impact but does not replace preventing XSS.
- CSRF only affects credentials the browser attaches automatically (cookies).
- The JWT signing key is the crown jewel.

**Next:** [03-crypto-primitives.md](./03-crypto-primitives.md)
