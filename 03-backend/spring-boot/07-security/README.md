# Module 07: Security & Identity

Welcome to **Module 07: Security & Identity**. This module covers enterprise security in Spring Security 6/7: the `SecurityFilterChain` Lambda DSL, modern stateless JWT Resource Server architectures, OAuth2 / OIDC Keycloak integration, Passkeys/WebAuthn, RBAC/ABAC method security, and OWASP Top 10 hardening.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-spring-security-architecture.md](file:///03-backend/spring-boot/07-security/01-spring-security-architecture.md) | Filter chain mechanics, `SecurityContextHolder`, `AuthenticationManager` flow | Advanced | Complete |
| 02 | [02-authentication-fundamentals.md](file:///03-backend/spring-boot/07-security/02-authentication-fundamentals.md) | Authentication tokens, `AuthenticationProvider`, custom authentication filters | Intermediate | Complete |
| 03 | [03-authorization-and-method-security.md](file:///03-backend/spring-boot/07-security/03-authorization-and-method-security.md) | `@PreAuthorize`, `@PostAuthorize`, SpEL expression evaluation, method interceptors | Intermediate | Complete |
| 04 | [04-password-encoding-and-user-details.md](file:///03-backend/spring-boot/07-security/04-password-encoding-and-user-details.md) | `PasswordEncoder`, Argon2 vs BCrypt, `UserDetailsService`, custom principal records | Intermediate | Complete |
| 05 | [05-session-and-cookie-security.md](file:///03-backend/spring-boot/07-security/05-session-and-cookie-security.md) | Stateful session management, concurrent session controls, Session Fixation protection | Intermediate | Complete |
| 06 | [06-CSRF-and-CORS.md](file:///03-backend/spring-boot/07-security/06-CSRF-and-CORS.md) | Single Page Application (SPA) CSRF token handling, `CookieCsrfTokenRepository` | Advanced | Complete |
| 07 | [07-JWT-resource-server.md](file:///03-backend/spring-boot/07-security/07-JWT-resource-server.md) | Stateless JWT verification, JWKS endpoint integration, custom claims converters | Advanced | Complete |
| 08 | [08-oauth2-client-and-social-login.md](file:///03-backend/spring-boot/07-security/08-oauth2-client-and-social-login.md) | OAuth2 Login flow, Authorization Code grant with PKCE, Google/GitHub providers | Intermediate | Complete |
| 09 | [09-oauth2-authorization-server.md](file:///03-backend/spring-boot/07-security/09-oauth2-authorization-server.md) | Building a standalone OAuth2 Authorization Server with Spring Authorization Server | Advanced | Complete |
| 10 | [10-OIDC-and-keycloak-integration.md](file:///03-backend/spring-boot/07-security/10-OIDC-and-keycloak-integration.md) | OpenID Connect token verification with Keycloak, role mapping, refresh flows | Advanced | Complete |
| 11 | [11-refresh-tokens-and-logout.md](file:///03-backend/spring-boot/07-security/11-refresh-tokens-and-logout.md) | Refresh token rotation, token reuse detection, blacklisting in Redis, logout | Advanced | Complete |
| 12 | [12-RBAC-ABAC-and-ACLs.md](file:///03-backend/spring-boot/07-security/12-RBAC-ABAC-and-ACLs.md) | Role-Based vs Attribute-Based Access Control, domain permission matrices | Advanced | Complete |
| 13 | [13-api-keys-and-HMAC.md](file:///03-backend/spring-boot/07-security/13-api-keys-and-HMAC.md) | Machine-to-machine API key authentication, request HMAC signature verification | Advanced | Complete |
| 14 | [14-passkeys-and-webauthn.md](file:///03-backend/spring-boot/07-security/14-passkeys-and-webauthn.md) | FIDO2 / WebAuthn passkey authentication support in modern Spring Security | Advanced | Complete |
| 15 | [15-method-security-and-domain-object-security.md](file:///03-backend/spring-boot/07-security/15-method-security-and-domain-object-security.md) | Spring Security ACLs, domain-level object permissions, `@PostFilter` | Advanced | Complete |
| 16 | [16-security-testing.md](file:///03-backend/spring-boot/07-security/16-security-testing.md) | Testing security with `@WithMockUser`, `@WithUserDetails`, MockMvc security | Intermediate | Complete |
| 17 | [17-security-headers-and-hardening.md](file:///03-backend/spring-boot/07-security/17-security-headers-and-hardening.md) | Content-Security-Policy (CSP), HSTS, X-Frame-Options, secure cookie attributes | Intermediate | Complete |
| 18 | [18-common-vulnerabilities-and-owasp-mapping.md](file:///03-backend/spring-boot/07-security/18-common-vulnerabilities-and-owasp-mapping.md) | OWASP Top 10 mapping, Spring4Shell analysis, SpEL injection mitigation | Advanced | Complete |
| 19 | [19-secrets-management.md](file:///03-backend/spring-boot/07-security/19-secrets-management.md) | HashiCorp Vault, AWS Secrets Manager, Kubernetes Secrets integration | Intermediate | Complete |
| 20 | [20-rate-limiting-and-brute-force.md](file:///03-backend/spring-boot/07-security/20-rate-limiting-and-brute-force.md) | Bucket4j and Redis distributed token bucket rate limiting to prevent brute force | Advanced | Complete |
| 21 | [21-multi-tenant-security.md](file:///03-backend/spring-boot/07-security/21-multi-tenant-security.md) | Multi-tenant authentication context, tenant-scoped JWT tokens, data isolation | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 08: Testing](file:///03-backend/spring-boot/08-testing/)** to master testing slices, Testcontainers, WireMock, and architectural fitness functions.
