# Module 07: Authentication & Enterprise Security

Welcome to **Module 07: Authentication & Enterprise Security**. This module covers modern authentication standards, cryptographic token rotation, identity federation, and OWASP API security defenses.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-security-fundamentals-in-fastapi.md](./01-security-fundamentals-in-fastapi.md) | **Security Fundamentals** | OAuth2 password flows, `HTTPBearer`, and security utilities. |
| [02-password-hashing-with-argon2.md](./02-password-hashing-with-argon2.md) | **Argon2id Password Hashing** | Memory-hard password hashing with `argon2-cffi`. |
| [03-jwt-access-tokens-with-pyjwt.md](./03-jwt-access-tokens-with-pyjwt.md) | **JWT Access Tokens** | RS256/ES256 asymmetric signing and claims validation. |
| [04-refresh-token-rotation-and-logout.md](./04-refresh-token-rotation-and-logout.md) | **Refresh Token Rotation** | Replay detection, database tracking, and Redis blacklisting. |
| [05-cookie-and-session-auth.md](./05-cookie-and-session-auth.md) | **Cookie & Session Auth** | `HttpOnly`, `SameSite=Lax`, and signed cookie sessions. |
| [06-oauth2-and-social-login-with-authlib.md](./06-oauth2-and-social-login-with-authlib.md) | **OAuth2 & Social Login** | Google, GitHub, and Apple login flows via Authlib. |
| [07-oidc-and-keycloak-integration.md](./07-oidc-and-keycloak-integration.md) | **OIDC & Keycloak** | Centralized Enterprise SSO with OpenID Connect and Keycloak. |
| [08-api-keys-and-hmac-signed-requests.md](./08-api-keys-and-hmac-signed-requests.md) | **API Keys & HMAC Signatures** | Header API keys and replay-proof HMAC payload signatures. |
| [09-rbac-abac-and-permissions-dependencies.md](./09-rbac-abac-and-permissions-dependencies.md)| **RBAC & ABAC Permissions** | Role and attribute-based access control via dependencies. |
| [10-ownership-checks-and-idor.md](./10-ownership-checks-and-idor.md) | **Ownership & IDOR Prevention** | Defending against Insecure Direct Object References. |
| [11-multi-tenant-auth.md](./11-multi-tenant-auth.md) | **Multi-Tenant Authentication** | Tenant isolation in JWT claims and context propagation. |
| [12-two-factor-and-passkeys-server-side.md](./12-two-factor-and-passkeys-server-side.md) | **2FA & Passkeys (WebAuthn)**| TOTP (Google Authenticator) and FIDO2 Passkeys. |
| [13-csrf-and-cors-properly.md](./13-csrf-and-cors-properly.md) | **CSRF & CORS Hardening** | Double-Submit cookie pattern and production CORS policies. |
| [14-security-headers-and-hardening.md](./14-security-headers-and-hardening.md) | **Security Headers** | HSTS, CSP directives, X-Frame-Options, and nosniff. |
| [15-rate-limiting-and-brute-force.md](./15-rate-limiting-and-brute-force.md) | **Brute-Force Protection** | IP and user-targeted throttling with Redis. |
| [16-input-validation-and-injection.md](./16-input-validation-and-injection.md) | **Injection Defenses** | Preventing SQL, NoSQL, and command injection attacks. |
| [17-secrets-management.md](./17-secrets-management.md) | **Secrets Management** | AWS Secrets Manager, HashiCorp Vault, and AES-256-GCM. |
| [18-owasp-api-top-10-mapping.md](./18-owasp-api-top-10-mapping.md) | **OWASP API Top 10 Mapping** | Mapping FastAPI controls directly to the OWASP Top 10. |
| [19-dependency-and-supply-chain.md](./19-dependency-and-supply-chain.md) | **Supply Chain Security** | SBOM, lockfile pinning, and `pip-audit` in CI. |
| [20-fastapi-users-overview.md](./20-fastapi-users-overview.md) | **FastAPI-Users Evaluation** | Pre-built authentication library: trade-offs and architecture. |
