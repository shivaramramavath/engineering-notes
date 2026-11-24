# Module 08: Authentication, Authorization & Node.js Security

> Hardening server-side Node.js: passwords, tokens, sessions, OAuth2/OIDC, Passkeys, rate limiting, and OWASP defenses.

---

## Module Overview

Security in Node.js requires a defense-in-depth posture: from cryptographic password hashing with Argon2 to tamper-proof session tokens, timing-safe HMAC webhook verification, and the OWASP Top 10 defenses.

> **Related Architectural Theory**: For broad authentication architecture, OAuth 2.0 flows, and identity federation theory, cross-reference [06-project-patterns/auth/](../../06-project-patterns/auth/README.md).

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-password-hashing.md](./01-password-hashing.md) | Password hashing: Argon2 vs bcrypt, salt rounds, timing-safe verification. |
| **02** | [02-jwt-and-tokens.md](./02-jwt-and-tokens.md) | JSON Web Tokens (`jose`), asymmetric keys (RS256/EdDSA), revocation. |
| **03** | [03-sessions.md](./03-sessions.md) | Stateful session management, Redis stores, cookie hijacking defenses. |
| **04** | [04-oauth-and-oidc.md](./04-oauth-and-oidc.md) | OAuth 2.1 authorization code flow with PKCE, OpenID Connect discovery. |
| **05** | [05-common-vulnerabilities.md](./05-common-vulnerabilities.md) | OWASP Top 10 for Node.js: Prototype pollution, SQLi, ReDoS, SSRF. |
| **06** | [06-rate-limiting.md](./06-rate-limiting.md) | Token bucket, sliding window rate limiting via Redis, DDoS mitigation. |
| **07** | [07-helmet.md](./07-helmet.md) | Helmet security headers, Content Security Policy (CSP), HSTS. |
| **08** | [08-security-checklist.md](./08-security-checklist.md) | Production release security audit checklist. |
| **09** | [09-passkeys-and-webauthn.md](./09-passkeys-and-webauthn.md) | FIDO2 / WebAuthn passwordless authentication with `@simplewebauthn/server`. |
| **10** | [10-rbac-abac-and-policy-enforcement.md](./10-rbac-abac-and-policy-enforcement.md) | RBAC and ABAC dynamic policy evaluation with CASL. |
| **11** | [11-api-keys-and-hmac-signatures.md](./11-api-keys-and-hmac-signatures.md) | Machine-to-machine API keys, constant-time HMAC signature verification. |

---

## Next Steps
Proceed to [09-api-development](../09-api-development/README.md) to explore REST design, OpenAPI, Webhooks, GraphQL, and tRPC.
