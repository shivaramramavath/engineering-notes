# Logging and Audit

Logs answer "what happened?" during an incident, and they are also one of the most common places secrets leak. This note covers both sides: **what never to log**, and **what security events to record** so that attacks, bugs and abuse are visible and investigable.

**Prerequisites:** [02-threat-model.md](../01-fundamentals/02-threat-model.md), [01-token-hashing.md](../03-tokens/01-token-hashing.md)

---

## Two different things

| | Application log | Audit log |
|---|---|---|
| Purpose | Debugging and operations | Accountability: who did what, when, from where |
| Audience | Developers/on-call | Security, compliance, support, the user (activity page) |
| Content | Errors, requests, performance | Security-relevant **actions** and outcomes |
| Retention | Days to weeks | Months to years (per policy/regulation) |
| Mutability | Rotated freely | **Append-only**, tamper-resistant |
| Storage | Log pipeline (stdout → collector) | Dedicated collection/table/stream with restricted access |

Don't mix them: debug logs get noisy, rotated and widely accessible; audit records must be complete, durable and protected.

---

## What never to log

```text
❌ Passwords (current, new, attempted), including failed login attempts
❌ Access tokens, refresh tokens, session ids/cookies
❌ Password-reset, email-verification, magic-link and OAuth tokens/codes
❌ Authorization headers, Cookie / Set-Cookie headers
❌ API keys, client secrets, signing/encryption keys, pepper
❌ MFA secrets, TOTP/OTP codes, recovery codes
❌ Full request/response bodies on auth routes
❌ Full URLs containing tokens (?token=..., ?code=...&state=...)
❌ Payment card data, government ids, other regulated data
```

Leaks happen indirectly:

| Source | How secrets sneak in |
|---|---|
| `console.log(req)` / `JSON.stringify(req)` | Headers, cookies, body |
| `console.log(error)` for HTTP client errors | **Axios/fetch error objects include the request config, including the `Authorization` header** |
| Reverse proxy / load balancer **access logs** | The query string: reset tokens, OAuth `code` |
| APM / error trackers (Sentry, Datadog) | Request bodies, headers, breadcrumbs unless scrubbed |
| ORM/DB query logging | Query parameters: emails, tokens, hashes |
| **Failed-login logs** | Users type their **password into the email field**; logging the "attempted username" logs a password |
| Debug flags left on in production | `DEBUG=*`, verbose HTTP logging |
| Browser console / frontend error reporting | Tokens in state dumps |
| Crash dumps / core files | Memory contents |

Treat any log line as readable by every engineer, every log vendor and, eventually, an attacker who gets the logging system.

---

## Structured logging with redaction

Use a structured logger (JSON) with automatic **redaction** of sensitive paths, as a safety net behind careful code. Pino example:

```bash
npm i pino pino-http
```

```js
// utils/logger.js
import pino from "pino";

export const logger = pino({
  level: process.env.LOG_LEVEL || "info",
  redact: {
    paths: [
      "req.headers.authorization",
      "req.headers.cookie",
      "res.headers['set-cookie']",
      "req.body.password",
      "req.body.currentPassword",
      "req.body.newPassword",
      "req.body.token",
      "req.body.code",
      "req.body.refreshToken",
      "*.password",
      "*.passwordHash",
      "*.token",
      "*.accessToken",
      "*.refreshToken",
      "*.secret",
    ],
    censor: "[REDACTED]",
  },
});
```

```js
import pinoHttp from "pino-http";

app.use(pinoHttp({
  logger,
  genReqId: (req) => req.headers["x-request-id"] ?? crypto.randomUUID(),
  serializers: {
    req: (req) => ({ id: req.id, method: req.method, url: req.url.split("?")[0], remoteAddress: req.remoteAddress }),   // strip query strings
  },
}));
```

Important caveats:

- Redaction paths are an **allow-nothing-by-default denylist**: a field you forgot (or one nested differently) is *not* redacted. Prefer **logging only fields you choose** (an allowlist of safe fields) over dumping objects and hoping redaction catches everything.
- Stripping the **query string** from logged URLs avoids token leakage; do the same in your proxy's log format.
- Don't log request bodies for auth routes at all.
- Add a test that fails if known secret values appear in log output (below).

---

## Security events to record

Name events consistently and log them from the **service layer** where the action happens, so no code path skips them.

| Category | Events |
|---|---|
| Authentication | `LOGIN_SUCCESS`, `LOGIN_FAILED`, `LOGIN_BLOCKED` (rate limit/lockout), `LOGOUT`, `LOGOUT_ALL` |
| Sessions/tokens | `SESSION_CREATED`, `SESSION_REVOKED`, `TOKEN_REFRESHED`, **`REFRESH_REUSE_DETECTED`** |
| Passwords | `PASSWORD_CHANGED`, `PASSWORD_RESET_REQUESTED`, `PASSWORD_RESET_COMPLETED` |
| Email | `EMAIL_VERIFIED`, `EMAIL_CHANGE_REQUESTED`, `EMAIL_CHANGED` |
| MFA | `MFA_ENABLED`, `MFA_DISABLED`, `MFA_CHALLENGE_FAILED`, `MFA_RECOVERY_USED`, `RECOVERY_CODES_REGENERATED`, `PASSKEY_ADDED`, `PASSKEY_REMOVED` |
| OAuth | `OAUTH_LOGIN`, `OAUTH_LINKED`, `OAUTH_UNLINKED`, `OAUTH_STATE_MISMATCH` |
| Authorization | `ACCESS_DENIED`, `ROLE_CHANGED`, `PRIVILEGE_ESCALATION_ATTEMPT`, `STEP_UP_SUCCEEDED/FAILED` |
| Account | `ACCOUNT_CREATED`, `ACCOUNT_DISABLED`, `ACCOUNT_DELETED`, `API_KEY_CREATED/REVOKED` |
| Abuse | `RATE_LIMITED`, `CAPTCHA_FAILED`, `SUSPICIOUS_ACTIVITY` |

### Fields on every event

```json
{
  "ts": "2026-10-31T12:34:56.789Z",
  "event": "LOGIN_FAILED",
  "outcome": "failure",
  "reason": "bad_password",
  "userId": "665f...",
  "sessionId": "S1…(prefix only)",
  "ip": "203.0.113.7",
  "userAgent": "Mozilla/5.0 …(truncated)",
  "requestId": "0b9c…",
  "actorId": "665f...",
  "target": { "type": "user", "id": "665f..." }
}
```

- **who** (`userId`/`actorId`, plus `target` when an admin acts on someone else), **what** (`event`, `outcome`, `reason`), **when** (UTC timestamp), **where** (`ip`, `userAgent`, `requestId`).
- **Reasons are internal.** The *response* to the client stays generic; the *log* says whether it was `unknown_user`, `bad_password`, `locked`, or `mfa_failed`. That's how you debug without leaking to attackers ([01-enumeration-protection.md](./01-enumeration-protection.md)).
- Log **identifiers, not secrets**: user ids, a hash/mask of the email for unknown accounts, a short prefix of the session id.

### The helper

```js
// utils/security-log.js
import { logger } from "./logger.js";
import { AuditEvent } from "../models/audit-event.model.js";

const AUDITED = new Set(["PASSWORD_CHANGED", "MFA_DISABLED", "ROLE_CHANGED", "OAUTH_LINKED", "SESSION_REVOKED", "ACCOUNT_DELETED", "REFRESH_REUSE_DETECTED"]);

export async function securityEvent(event, ctx = {}) {
  const record = {
    event,
    outcome: ctx.outcome ?? "success",
    reason: ctx.reason,
    userId: ctx.userId,
    actorId: ctx.actorId ?? ctx.userId,
    sessionId: ctx.sessionId?.slice(0, 8),
    ip: ctx.ip,
    userAgent: ctx.userAgent?.slice(0, 200),
    requestId: ctx.requestId,
    target: ctx.target,
  };

  logger.info({ security: record }, event);                              // searchable operational log

  if (AUDITED.has(event)) {
    await AuditEvent.create({ ...record, at: new Date() }).catch((err) =>
      logger.error({ err: err.message }, "audit write failed"));         // never let audit failures break the request silently: alert on this
  }
}
```

Call it where decisions are made:

```js
await securityEvent("LOGIN_FAILED", { outcome: "failure", reason: "bad_password", userId: user._id, ip: req.ip, userAgent: req.get("user-agent"), requestId: req.id });
```

For failed logins against **unknown** accounts, record a hash or masked form of the identifier (never the raw string, which may be a mistyped password):

```js
const idForLog = hashToken(email).slice(0, 12);
```

---

## Audit log properties

| Property | How |
|---|---|
| **Append-only** | App role has `insert` but not `update`/`delete` on the audit collection; or write to an append-only stream (S3 object lock, WORM storage, a log service) |
| **Tamper-evident** | Optional hash chain (each record includes the hash of the previous) or periodic signed checkpoints |
| **Separate access** | Different credentials/permissions than the main DB; read access limited to security/support |
| **Complete** | Written in the same code path as the action; for critical actions, make the audit write part of the operation's success criteria |
| **Retention** | Defined by policy and law; a TTL index only for events you may delete |
| **Queryable per user** | Index `userId` + `at` so you can show "Recent security activity" in account settings |

```js
const auditSchema = new mongoose.Schema({
  at: { type: Date, default: Date.now, index: true },
  event: { type: String, index: true },
  outcome: String, reason: String,
  userId: { type: ObjectId, index: true },
  actorId: ObjectId, target: Object,
  ip: String, userAgent: String, requestId: String, sessionId: String,
}, { versionKey: false });
```

---

## Privacy and compliance

IP addresses, user agents, emails and device info are **personal data** under GDPR-style laws.

- Log the minimum necessary; keep audit retention justified and documented.
- Mask or truncate where full values aren't needed (`203.0.113.0/24`, `a***@example.com`).
- Support deletion/anonymization requests without destroying the integrity of security records (anonymize identifiers rather than delete the event, where the law allows).
- Restrict who can read logs, and log access to the audit log itself.

---

## Logging safely: other rules

- **Log injection:** user-controlled text containing newlines can forge fake log lines in plain-text logs. Structured JSON logging escapes them; also truncate untrusted fields (`User-Agent`, usernames).
- **Correlation:** carry a **request id** through logs, audit events and error reports; generate one at the edge and return it as `X-Request-Id` so support can find a specific failed request.
- **Levels:** `error` for failures needing attention, `warn` for suspicious-but-handled events (reuse detection, blocked logins), `info` for normal security events, `debug` off in production.
- **Errors:** log the stack **server-side**; return a generic message and request id to the client ([02-middleware-patterns.md](../07-authorization/02-middleware-patterns.md)).
- **Time:** UTC, ISO 8601, with a synchronized clock (NTP), which also keeps TOTP working ([01-totp.md](../05-mfa/01-totp.md)).
- **Volume:** rate-limit or sample very noisy events (`RATE_LIMITED`, `LOGIN_FAILED` during an attack) so logging can't become the DoS vector or hide the important lines.

---

## Detection and alerting

Logs matter only if someone looks. Turn the highest-signal events into alerts ([02-monitoring-and-alerts.md](../10-production/02-monitoring-and-alerts.md)):

| Signal | Likely meaning |
|---|---|
| Spike in `LOGIN_FAILED` / `LOGIN_BLOCKED` | Credential stuffing or brute force |
| Many `LOGIN_FAILED` across many accounts from one IP / ASN | Stuffing run |
| **`REFRESH_REUSE_DETECTED`** (any) | Probable token theft: investigate |
| Successful login after many failures | Possible compromised account |
| `MFA_DISABLED`, `PASSWORD_CHANGED`, `EMAIL_CHANGED` shortly after a new-device login | Account takeover pattern |
| `ROLE_CHANGED` to admin, `PRIVILEGE_ESCALATION_ATTEMPT` | Authorization abuse |
| Surge in `PASSWORD_RESET_REQUESTED` | Email bombing or enumeration |
| `OAUTH_STATE_MISMATCH` bursts | Login CSRF attempts or a broken deploy |
| Audit write failures, Redis/limiter errors | Control plane degraded |

Pair detection with response: auto-revoke sessions, force step-up, notify the user ([04-logout-and-revocation.md](../04-sessions/04-logout-and-revocation.md), [03-incident-response.md](../10-production/03-incident-response.md)).

Also notify **users**: "New sign-in from ...", "Password changed", "MFA disabled". It's the cheapest detection system, since the real owner spots what you can't.

---

## Testing your logging

```js
it("never writes secrets to logs", async () => {
  const lines = [];
  const logger = makeTestLogger((line) => lines.push(line));        // capture output

  await request(appWith(logger))
    .post("/api/auth/login")
    .set("Cookie", "refreshToken=SECRET_REFRESH_VALUE")
    .send({ email: "alice@example.com", password: "SuperSecretPassw0rd!" });

  const output = lines.join("\n");
  expect(output).not.toContain("SuperSecretPassw0rd!");
  expect(output).not.toContain("SECRET_REFRESH_VALUE");
});
```

Also: run a **secret scanner** over the repo and CI logs (gitleaks, trufflehog), grep production logs for token-shaped strings (`eyJ...` JWT prefix, long base64url strings) as a periodic audit, and review APM/Sentry scrubbing settings.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| `console.log(req.body)` on auth routes | Passwords in logs forever |
| Logging full URLs with query strings | Reset/OAuth tokens in logs and proxies |
| Logging failed-login usernames raw | Passwords typed into the email field |
| Dumping HTTP client errors | Authorization headers leaked |
| Relying on redaction alone | Missed fields leak |
| Audit log writable/deletable by the app | An attacker (or insider) erases traces |
| No request id | Impossible to correlate a user report with logs |
| Logging security events only on failure | No baseline, no forensic timeline |
| Different reasons in responses vs. logs mixed up | Either enumeration leaks or undebuggable errors |
| No alerting on `REFRESH_REUSE_DETECTED` | Token theft goes unnoticed |
| Unlimited log retention of personal data | Compliance risk |

## Debugging

| Symptom | Cause |
|---|---|
| Can't tell why a login failed | Reason not logged server-side (log it, don't return it) |
| Secrets found in logs | Search for the source (proxy logs, error handlers, APM), then **rotate** the exposed secrets |
| Missing events | Logging added in controllers, not services; async audit writes failing silently |
| Log volume explosion | Attack traffic logged at `info` per request; sample/rate-limit |
| Timestamps out of order | Unsynced clocks or local time zones instead of UTC |

If a secret **was** logged, treat it as leaked: rotate, purge where possible, and review who could access the log store ([01-env-and-secrets.md](../00-setup/01-env-and-secrets.md)).

---

## Quick summary

- Never log passwords, tokens, cookies, auth headers, codes, keys, or token-bearing URLs; beware indirect leaks (HTTP client errors, proxy logs, APM, failed-login usernames).
- Use structured logs with redaction as a safety net, but log only fields you choose.
- Record consistent security events (who/what/when/where/outcome) from the service layer; keep failure reasons in logs, not responses.
- Keep an append-only, access-controlled audit log separate from debug logs; mind privacy and retention.
- Alert on high-signal events (especially refresh-token reuse) and notify users of sensitive changes.

**Next:** [Testing and debugging: testing auth](../09-testing-debugging/00-testing-auth.md)
