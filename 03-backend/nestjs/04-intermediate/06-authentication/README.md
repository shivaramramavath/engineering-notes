# Authentication

Authentication answers one question: **who is making this request?** (Authorization, covered in [the next section](../07-authorization/README.md), answers "and what may they do?"). Getting authentication wrong is one of the costliest mistakes a backend can make, so this section favors boring, well-understood designs over clever ones.

```text
 credentials ──► verify (password / OAuth / 2FA) ──► issue proof (session id or token)
                                                              │
 later requests ──► present proof ──► guard verifies ──► req.user ──► handler
```

> Applies to NestJS 10/11 with `@nestjs/passport`, `@nestjs/jwt`, and common libraries (`argon2`/`bcrypt`, `express-session`, `otplib`). Library APIs and major versions change; confirm specifics against their current docs. This section gives security guidance, not a guarantee: for high-stakes systems, prefer a vetted identity provider and get a security review.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Authentication architecture](./01-authentication-architecture.md) | Sessions vs tokens, where proof lives, threat model, how the pieces fit in Nest |
| 02 | [Passport and local strategy](./02-passport-and-local-strategy.md) | Login with email/password, `AuthGuard`, strategies |
| 03 | [Password hashing](./03-password-hashing.md) | Argon2/bcrypt, parameters, rehashing, reset tokens |
| 04 | [JWT](./04-jwt.md) | Signing, verifying, claims, common vulnerabilities |
| 05 | [Refresh tokens and logout](./05-refresh-tokens-and-logout.md) | Rotation, reuse detection, revocation |
| 06 | [Cookie authentication](./06-cookie-authentication.md) | Cookie attributes, CSRF, CORS with credentials |
| 07 | [Session authentication](./07-session-authentication.md) | `express-session`, stores, fixation, timeouts |
| 08 | [OAuth 2.0](./08-oauth2.md) | Authorization code + PKCE, scopes, OIDC |
| 09 | [Social login](./09-social-login.md) | Google/GitHub login, account linking pitfalls |
| 10 | [Two-factor authentication](./10-two-factor-authentication.md) | TOTP, recovery codes, step-up flow |

## Prerequisites

- [Guards](../../03-core-concepts/01-request-pipeline/04-guards.md) and [custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md)
- [Configuration](../../03-core-concepts/03-configuration/README.md) (secrets come from config)
- [Security fundamentals](../../07-production/01-security/01-security-fundamentals.md)

## Related

- [Authorization](../07-authorization/README.md)
- [Rate limiting and brute-force protection](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)
- [CSRF](../../07-production/01-security/03-csrf.md)
- [Secrets management](../../07-production/01-security/06-secrets-management.md)
- [Project: authentication system](../../10-projects/03-authentication-system/) and [system design: authentication](../../11-system-design/03-authentication-system.md)
- [Authentication quick reference](../../13-quick-reference/06-authentication.md)
