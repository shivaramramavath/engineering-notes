# Technical Interview Mastery: Authentication, Authorization & OWASP Security

## Overview & Evaluation Criteria

Security questions for backend engineers probe JWT token lifecycle design, OAuth2 password flows, refresh token rotation, cross-site scripting (XSS), cross-site request forgery (CSRF), SQL injection prevention, and timing attack mitigation.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: Why is storing JWT access tokens in browser <code>localStorage</code> considered an OWASP security anti-pattern? What is the recommended production alternative?</strong></summary>

### Answer:
- **The Threat**: JavaScript running in the browser has full read access to `window.localStorage`. If the site suffers from any Cross-Site Scripting (XSS) vulnerability (via a compromised npm package or malicious injected script), the attacker can instantly steal the JWT token and impersonate the user until expiration.
- **Production Standard**:
  1. Store the **Access Token** (short-lived, e.g. 15 minutes) in-memory inside the client JavaScript state.
  2. Store the **Refresh Token** (long-lived, e.g. 7-30 days) inside an **`HttpOnly; Secure; SameSite=Strict` (or `Lax`) cookie**.
  3. `HttpOnly` completely blocks client-side JavaScript from reading the cookie, neutralizing XSS credential theft. `SameSite=Strict` blocks Cross-Site Request Forgery (CSRF).

</details>

<details>
<summary><strong>Q2: How does Refresh Token Rotation with Automatic Reuse Detection prevent token replay attacks?</strong></summary>

### Answer:
- **Mechanics**:
  1. When a client requests a new access token using a refresh token `RT_1`, the auth server validates `RT_1`, invalidates `RT_1`, and issues a brand-new pair: `(AT_2, RT_2)`.
  2. The server stores a cryptographic hash or family identifier for the token lineage in Redis or the database.
- **Replay / Breach Detection**:
  - If an attacker stole `RT_1` and attempts to use it *after* the legitimate client already consumed it, the auth server detects that an **already-invalidated refresh token was presented**.
  - This indicates a token theft event.
  - The auth server **immediately revokes all active tokens in that entire family / session**, forcing the attacker and user to re-authenticate.

</details>

<details>
<summary><strong>Q3: Why must password hashes and HMAC webhook signatures be compared using <code>hmac.compare_digest()</code> instead of standard <code>==</code>?</strong></summary>

### Answer:
- Standard string equality (`==`) evaluates characters sequentially from left to right and exits immediately on the first non-matching byte (**short-circuit evaluation**).
- **Timing Attack**: An attacker can measure request execution latency with sub-microsecond precision. By varying each character and measuring which byte takes slightly longer before returning a 401 error, the attacker can iteratively deduce the secret signature or hash byte by byte.
- **Constant-Time Comparison**: `hmac.compare_digest(a, b)` compares every single byte regardless of where the mismatch occurs, taking identical execution time and eliminating timing side-channel leaks.

</details>

<details>
<summary><strong>Q4: Explain how SQL Injection can still happen in modern Python backends even when using an ORM like SQLAlchemy.</strong></summary>

### Answer:
- While SQLAlchemy's query builder (`select(User).where(User.id == user_id)`) automatically uses parameterized queries (`$1`), developers often inadvertently introduce SQL injection when using raw SQL helpers:
```python
# VULNERABLE TO SQL INJECTION:
stmt = text(f"SELECT * FROM users WHERE username = '{untrusted_input}'")

# SECURE PARAMETERIZED QUERY:
stmt = text("SELECT * FROM users WHERE username = :name").bindparams(name=untrusted_input)
```
- Another common vulnerability is dynamic `ORDER BY` or table name injection (`ORDER BY ${col}`), which cannot be parameterized and must be strictly validated against an allowed whitelist.
</details>
