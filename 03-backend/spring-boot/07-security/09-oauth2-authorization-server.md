# 09. Spring Authorization Server Architecture & Implementation

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `07-JWT-resource-server.md`, `08-oauth2-client-and-social-login.md`, RFC 6749, RFC 7519, RFC 7517 |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Spring Authorization Server 1.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In enterprise architectures, organizations often need full ownership of their identity infrastructure rather than relying entirely on third-party multi-tenant SaaS providers (Auth0, Okta) or complex standalone external platforms (Keycloak). Requirements such as custom cryptographic signing algorithms, regulatory on-premise data localization, deep integration with proprietary legacy user stores, and tailor-made consent/audit workflows demand a dedicated in-house **OAuth2 / OpenID Connect Authorization Server**.

Building an OAuth2 server from scratch without a framework is catastrophic: subtle protocol bugs in token exchange, authorization code handling, JWKS rotation, or PKCE validation introduce critical identity hijacking vulnerabilities.

### Historical Evolution
- **Spring Security OAuth 2.x (Legacy)**: Included `@EnableAuthorizationServer`. Deprecated in 2018 due to architectural rigidity, protocol deviations, and technical debt.
- **The Spring Authorization Server Project (2020-Present)**: Launched as a community-driven, ground-up rewrite adhering strictly to OpenID Connect Core 1.0, OAuth 2.1, RFC 7636 (PKCE), and RFC 8414 (OAuth 2.0 Authorization Server Metadata).
- **SAS 1.x / Modern Spring Boot 3.x/4.x**: Officially promoted as the premier framework component for hosting custom OAuth2 servers within standard Spring Boot ecosystems. Fully native, reactive-ready, and optimized for Virtual Threads.

### Real-World Relevance
Spring Authorization Server empowers engineering teams to build sovereign identity brokers, internal developer platform token issuers, and microservice ecosystem authentication gateways.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.security.oauth2.server.authorization.client.RegisteredClient`**: Immutable representation of an authorized client application (Client ID, hashed Client Secret, allowed Grant Types, Redirect URIs, Scopes, and Token Settings).
- **`org.springframework.security.oauth2.server.authorization.client.RegisteredClientRepository`**: Repository contract managing `RegisteredClient` entities (in-memory or JDBC).
- **`org.springframework.security.oauth2.server.authorization.OAuth2Authorization`**: Represents a live authorization state (issued authorization codes, access tokens, refresh tokens, and OIDC ID tokens).
- **`org.springframework.security.oauth2.server.authorization.OAuth2AuthorizationService`**: Persistence service for tracking active authorizations and tokens.
- **`org.springframework.security.oauth2.server.authorization.settings.AuthorizationServerSettings`**: Configuration object defining standard OAuth2 endpoint paths (`/oauth2/authorize`, `/oauth2/token`, `/oauth2/jwks`, etc.).
- **`org.springframework.security.oauth2.server.authorization.token.OAuth2TokenCustomizer`**: Functional callback for enriching issued tokens (JWT claims, custom headers, or opaque token attributes).

### The Mental Model
```
[ Client Application ] 
       │
       │ 1. Authorization Request (/oauth2/authorize?response_type=code...)
       ▼
[ Spring Authorization Server ]
       │
       ├── 2. User Authentication (via standard Spring Security Web Filter Chain)
       ├── 3. User Consent Screen (if configured)
       ├── 4. Issues Authorization Code -> Redirects to Client
       │
       │ 5. Token Request (/oauth2/token + code + code_verifier)
       ▼
[ Token Endpoint Filter ]
       │
       ├── 6. Validates Client Credentials & PKCE
       ├── 7. Generates Access Token & ID Token (signs via JWKSource RSA key)
       ├── 8. Invokes OAuth2TokenCustomizer (adds tenant_id, permissions)
       └── 9. Stores in OAuth2AuthorizationService (JDBC/Redis)
```

### Invariants
1. Client secrets must **never** be stored in plaintext; they must be hashed using a standard `PasswordEncoder` (e.g., BCrypt or Argon2).
2. All Authorization Code exchanges must enforce PKCE (RFC 7636) in modern implementations.
3. Cryptographic signing keys exposed at the JWKS endpoint must only contain public key components (`n`, `e`, `kid`); private keys must remain securely stored on the server.

---

## 3. How It Works Internally

### Dual Security Filter Chain Architecture
A Spring Authorization Server application operates **two distinct `SecurityFilterChain` beans**:
1. **Authorization Server Filter Chain (High Precedence - `@Order(1)`)**:
   - Manages protocol endpoints: `/oauth2/authorize`, `/oauth2/token`, `/oauth2/jwks`, `/oauth2/revoke`, `/userinfo`, `/.well-known/openid-configuration`.
   - Protects token endpoints with client authentication filters.
   - Redirects unauthenticated end-users to the login page on `/oauth2/authorize`.
2. **Standard Web Application Filter Chain (Default Precedence - `@Order(2)`)**:
   - Manages human user authentication (Form Login, MFA, Passkeys, Session cookies).
   - Provides user self-service interfaces, consent screens, and administrative dashboards.

```
Incoming Request
  │
  ├──► Matches SAS Endpoints (/oauth2/**, /userinfo)?
  │       ├─ YES ──► Authorization Server Filter Chain (@Order(1))
  │       │           ├─ Token Endpoint / Client Authentication
  │       │           ├─ JWKS Endpoint (public)
  │       │           └─ Unauthenticated user? ──► Redirect to /login
  │       │
  │       └─ NO  ──► Default Web Security Filter Chain (@Order(2))
  │                   ├─ /login (Form Login)
  │                   ├─ /consent
  │                   └─ Application Pages
```

---

## 4. Syntax & Usage

### 1. Maven Starter (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-oauth2-authorization-server</artifactId>
</dependency>
```

### 2. Standard Dual Filter Chain Configuration
```java
package com.example.authserver.config;

import com.nimbusds.jose.jwk.JWKSet;
import com.nimbusds.jose.jwk.RSAKey;
import com.nimbusds.jose.jwk.source.ImmutableJWKSet;
import com.nimbusds.jose.jwk.source.JWKSource;
import com.nimbusds.jose.proc.SecurityContext;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.annotation.Order;
import org.springframework.http.MediaType;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.oauth2.server.authorization.config.annotation.web.configuration.OAuth2AuthorizationServerConfiguration;
import org.springframework.security.oauth2.server.authorization.config.annotation.web.configurers.OAuth2AuthorizationServerConfigurer;
import org.springframework.security.oauth2.server.authorization.settings.AuthorizationServerSettings;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.LoginUrlAuthenticationEntryPoint;
import org.springframework.security.web.util.matcher.MediaTypeRequestMatcher;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.util.UUID;

@Configuration
@EnableWebSecurity
public class AuthorizationServerConfig {

    @Bean
    @Order(1)
    public SecurityFilterChain authorizationServerSecurityFilterChain(HttpSecurity http) throws Exception {
        OAuth2AuthorizationServerConfigurer authorizationServerConfigurer =
                OAuth2AuthorizationServerConfigurer.authorizationServer();

        http
            .securityMatcher(authorizationServerConfigurer.getEndpointsMatcher())
            .with(authorizationServerConfigurer, authorizationServer ->
                authorizationServer.oidc(Customizer.withDefaults()) // Enable OpenID Connect 1.0
            )
            .authorizeHttpRequests(authorize -> authorize.anyRequest().authenticated())
            .exceptionHandling(exceptions -> exceptions
                .defaultAuthenticationEntryPointFor(
                    new LoginUrlAuthenticationEntryPoint("/login"),
                    new MediaTypeRequestMatcher(MediaType.TEXT_HTML)
                )
            );

        return http.build();
    }

    @Bean
    @Order(2)
    public SecurityFilterChain defaultSecurityFilterChain(HttpSecurity http) throws Exception {
        http
            .authorizeHttpRequests(authorize -> authorize.anyRequest().authenticated())
            .formLogin(Customizer.withDefaults()); // Standard form login for end-users

        return http.build();
    }

    @Bean
    public AuthorizationServerSettings authorizationServerSettings() {
        return AuthorizationServerSettings.builder().build();
    }
}
```

---

## 5. Practical Examples

### Example 1: In-Memory Client Registration with PKCE Enforcement
Registering a client application that requires PKCE, allows Authorization Code and Refresh Token grants, and sets custom token lifespans.

```java
package com.example.authserver.client;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.oauth2.core.AuthorizationGrantType;
import org.springframework.security.oauth2.core.ClientAuthenticationMethod;
import org.springframework.security.oauth2.core.oidc.OidcScopes;
import org.springframework.security.oauth2.server.authorization.client.InMemoryRegisteredClientRepository;
import org.springframework.security.oauth2.server.authorization.client.RegisteredClient;
import org.springframework.security.oauth2.server.authorization.client.RegisteredClientRepository;
import org.springframework.security.oauth2.server.authorization.settings.ClientSettings;
import org.springframework.security.oauth2.server.authorization.settings.TokenSettings;

import java.time.Duration;
import java.util.UUID;

@Configuration
public class ClientConfig {

    @Bean
    public RegisteredClientRepository registeredClientRepository(PasswordEncoder passwordEncoder) {
        RegisteredClient spaClient = RegisteredClient.withId(UUID.randomUUID().toString())
            .clientId("single-page-app")
            .clientSecret(passwordEncoder.encode("super-secret-pass"))
            .clientAuthenticationMethod(ClientAuthenticationMethod.CLIENT_SECRET_BASIC)
            .clientAuthenticationMethod(ClientAuthenticationMethod.NONE) // For public SPA clients
            .authorizationGrantType(AuthorizationGrantType.AUTHORIZATION_CODE)
            .authorizationGrantType(AuthorizationGrantType.REFRESH_TOKEN)
            .redirectUri("http://127.0.0.1:3000/callback")
            .postLogoutRedirectUri("http://127.0.0.1:3000/logged-out")
            .scope(OidcScopes.OPENID)
            .scope(OidcScopes.PROFILE)
            .scope("orders:read")
            .scope("orders:write")
            .clientSettings(ClientSettings.builder()
                .requireAuthorizationConsent(true)
                .requireProofKeyForCodeExchange(true) // Enforce PKCE
                .build()
            )
            .tokenSettings(TokenSettings.builder()
                .accessTokenTimeToLive(Duration.ofMinutes(15))
                .refreshTokenTimeToLive(Duration.ofDays(7))
                .reuseRefreshTokens(false) // Token rotation
                .build()
            )
            .build();

        return new InMemoryRegisteredClientRepository(spaClient);
    }
}
```

### Example 2: Enterprise Custom JWT Claims Customizer
Injecting custom business attributes (user ID, tenant identifier, department, granted permissions) into the generated access token and OIDC ID token.

```java
package com.example.authserver.token;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.oauth2.server.authorization.OAuth2TokenType;
import org.springframework.security.oauth2.server.authorization.token.JwtEncodingContext;
import org.springframework.security.oauth2.server.authorization.token.OAuth2TokenCustomizer;

import java.util.Set;
import java.util.stream.Collectors;

@Configuration
public class TokenCustomizerConfig {

    @Bean
    public OAuth2TokenCustomizer<JwtEncodingContext> jwtTokenCustomizer() {
        return context -> {
            Authentication principal = context.getPrincipal();

            // Enrich Access Token
            if (OAuth2TokenType.ACCESS_TOKEN.equals(context.getTokenType())) {
                Set<String> authorities = principal.getAuthorities().stream()
                    .map(GrantedAuthority::getAuthority)
                    .collect(Collectors.toSet());

                context.getClaims().claims(claims -> {
                    claims.put("authorities", authorities);
                    claims.put("tenant_id", "TENANT-CORP-42");
                    claims.put("audit_user_id", principal.getName());
                });
            }

            // Enrich OIDC ID Token
            if (OAuth2TokenType.ID_TOKEN.equals(context.getTokenType())) {
                context.getClaims().claims(claims -> {
                    claims.put("organization", "Engineering Core Platform");
                    claims.put("preferred_username", principal.getName());
                });
            }
        };
    }
}
```

### Example 3: Edge Case – Key Rotation via Dynamic JWKSource
Generating RSA key pairs on the fly or loading rotating keys from an external keystore to ensure smooth rotation.

```java
package com.example.authserver.keys;

import com.nimbusds.jose.jwk.JWKSet;
import com.nimbusds.jose.jwk.RSAKey;
import com.nimbusds.jose.jwk.source.ImmutableJWKSet;
import com.nimbusds.jose.jwk.source.JWKSource;
import com.nimbusds.jose.proc.SecurityContext;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.util.UUID;

@Configuration
public class JwkKeyConfig {

    @Bean
    public JWKSource<SecurityContext> jwkSource() {
        KeyPair keyPair = generateRsaKey();
        RSAPublicKey publicKey = (RSAPublicKey) keyPair.getPublic();
        RSAPrivateKey privateKey = (RSAPrivateKey) keyPair.getPrivate();

        RSAKey rsaKey = new RSAKey.Builder(publicKey)
            .privateKey(privateKey)
            .keyID(UUID.randomUUID().toString())
            .build();

        JWKSet jwkSet = new JWKSet(rsaKey);
        return new ImmutableJWKSet<>(jwkSet);
    }

    private static KeyPair generateRsaKey() {
        try {
            KeyPairGenerator keyPairGenerator = KeyPairGenerator.getInstance("RSA");
            keyPairGenerator.initialize(2048);
            return keyPairGenerator.generateKeyPair();
        } catch (Exception ex) {
            throw new IllegalStateException("Failed to generate RSA KeyPair", ex);
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using ephemeral in-memory storage (`InMemoryRegisteredClientRepository`, `InMemoryOAuth2AuthorizationService`) in clustered production environments, causing logged-in users to lose their authorization sessions on container redeployments.

```java
// BUGGY: Ephemeral state loses all issued tokens across multi-replica deployments!
@Bean
public OAuth2AuthorizationService brokenService() {
    return new InMemoryOAuth2AuthorizationService(); // Fails in multi-node clusters!
}
```

**Production Fix**:
Use standard `JdbcOAuth2AuthorizationService` and `JdbcRegisteredClientRepository` backed by PostgreSQL/MySQL with schema migration scripts.

```java
// SECURE & PRODUCTION READY: Relational database persistence
@Bean
public RegisteredClientRepository registeredClientRepository(JdbcTemplate jdbcTemplate) {
    return new JdbcRegisteredClientRepository(jdbcTemplate);
}

@Bean
public OAuth2AuthorizationService authorizationService(JdbcTemplate jdbcTemplate, RegisteredClientRepository clientRepo) {
    return new JdbcOAuth2AuthorizationService(jdbcTemplate, clientRepo);
}

@Bean
public OAuth2AuthorizationConsentService authorizationConsentService(JdbcTemplate jdbcTemplate, RegisteredClientRepository clientRepo) {
    return new JdbcOAuth2AuthorizationConsentService(jdbcTemplate, clientRepo);
}
```

### Example 5: Custom Client Secret Post Authentication
Supporting clients that transmit client credentials inside the HTTP POST body (`client_id` and `client_secret` form parameters) rather than the HTTP Basic Authorization header.

```java
package com.example.authserver.auth;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.annotation.Order;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.oauth2.server.authorization.config.annotation.web.configurers.OAuth2AuthorizationServerConfigurer;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
public class ClientPostAuthConfig {

    @Bean
    @Order(1)
    public SecurityFilterChain customClientAuthFilterChain(HttpSecurity http) throws Exception {
        OAuth2AuthorizationServerConfigurer configurer = OAuth2AuthorizationServerConfigurer.authorizationServer();

        configurer.clientAuthentication(authentication -> 
            authentication.clientAuthenticationConverter(
                new org.springframework.security.oauth2.server.authorization.web.authentication.DelegatingAuthenticationConverter(
                    java.util.List.of(
                        new org.springframework.security.oauth2.server.authorization.web.authentication.ClientSecretBasicAuthenticationConverter(),
                        new org.springframework.security.oauth2.server.authorization.web.authentication.ClientSecretPostAuthenticationConverter(),
                        new org.springframework.security.oauth2.server.authorization.web.authentication.PublicClientAuthenticationConverter()
                    )
                )
            )
        );

        http
            .securityMatcher(configurer.getEndpointsMatcher())
            .with(configurer, customizer -> {})
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated());

        return http.build();
    }
}
```

---

## 6. Best Practices

1. **Strictly Enforce PKCE for All Clients**: Configure `.requireProofKeyForCodeExchange(true)` on `ClientSettings` for confidential and public clients alike.
2. **Never Persist Ephemeral In-Memory Autorizations in Production**: Utilize `JdbcOAuth2AuthorizationService` or Redis-backed authorization stores.
3. **Persist Private Keys Securely**: Never hardcode private keys in application source code. Store cryptographic keys in HashiCorp Vault, AWS KMS, or encrypted environment keystores.
4. **Disable Refresh Token Reuse**: Configure `reuseRefreshTokens(false)` to mandate refresh token rotation upon every redemption, immediately alerting against token compromise.
5. **Set Short Access Token Lifespans**: Limit access token TTL to 10-15 minutes, relying on refresh tokens (or silent SSO refresh) for extended user sessions.
6. **Enforce Exact Redirect URI Matching**: Do not permit wildcard subdomains or regex matches in redirect URIs to thwart token interception attacks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Wildcards in `redirectUri` | Attackers can redirect authorization codes to malicious subdomains. | Register exact, fully-qualified URI strings including protocol, host, and port. |
| Neglecting `@Order(1)` on SAS filter chain | Default form login intercepts `/oauth2/token` and `/oauth2/authorize` before the protocol engine runs. | Ensure the SAS `SecurityFilterChain` has `@Order(Ordered.HIGHEST_PRECEDENCE)` or `@Order(1)`. |
| Exposing private keys at `/oauth2/jwks` | Catastrophic cryptographic compromise; anyone can forge valid access tokens. | Only add public RSA/EC key components (`n`, `e`, `kid`) to the published `JWKSet`. |
| Plaintext client secrets in repository | Secret leakage gives attackers full impersonation ability. | Hash client secrets with `PasswordEncoder` (BCrypt) before persisting in `RegisteredClient`. |
| Missing `authorizationServerSettings` bean | Endpoint URIs fail to resolve properly or default to incorrect root paths behind proxies. | Provide explicit `AuthorizationServerSettings.builder().issuer("https://auth.company.com").build()`. |
| Generating fresh RSA keys on every server restart | In-flight tokens become cryptographically invalid across server restarts/deployments. | Store and load persistent RSA/EC key pairs from a key vault or file-backed keystore. |

---

## 8. Performance & Security Notes

- **Token Signing Throughput**: RSA-2048 signing uses significant CPU. In high-volume token issuing environments (thousands of tokens/sec), benchmark ECDSA (`ES256`) which provides smaller signatures and faster computation.
- **Database Bloat from Authorization Records**: Every token generation, refresh, and consent event creates database rows in `oauth2_authorization`. Implement automated cron purge jobs for expired authorizations where `access_token_expires_at < NOW() - INTERVAL '30 days'`.
- **Virtual Threads Compatibility**: Spring Authorization Server 1.4+ executes cleanly on Java 21 Virtual Threads, offloading database JDBC persistence and cryptographic operations without pinning carrier threads.

---

## 9. Comparison / When to Use What

| Solution | Customizability | Maintenance Burden | Protocol Compliance | Best Use Case |
|---|---|---|---|---|
| **Spring Authorization Server** | Maximum (100% Java control) | Medium (Developer-owned) | Full OAuth 2.1 / OIDC 1.0 | Custom enterprise identity logic, tightly coupled domain models |
| **Keycloak** | Medium (SPI plugins) | High (Separate platform ops) | Full OAuth 2.1 / OIDC 1.0 / SAML | Ready-to-use GUI, enterprise LDAP/AD federation out-of-the-box |
| **Auth0 / Okta** | Low (Webhooks & Rules) | Zero (Managed SaaS) | Full Standards | Fast time-to-market, zero operational overhead, commercial budget |
| **Roll-Your-Own Filter Server** | N/A (Dangerous) | Extreme (Vulnerability prone) | Low / Broken | Never; always use certified implementations |

---

## 10. Interview Questions

<details>
<summary>1. Why was `@EnableAuthorizationServer` deprecated in older Spring Security versions?</summary>

**Answer:**
The legacy Spring Security OAuth project was architected before modern OAuth 2.1 and OIDC standards matured. It had tight coupling between resource servers, clients, and authorization servers, suffered from complex XML/JavaConfig fragmentation, lacked native PKCE support, and proved difficult to maintain alongside modern reactive and Java evolution. Spring Authorization Server was built from the ground up as a dedicated, decoupled replacement.
</details>

<details>
<summary>2. How does Spring Authorization Server distinguish between human user login and client authentication?</summary>

**Answer:**
It uses two separate filter chains. The Authorization Server chain (`@Order(1)`) intercepts protocol endpoints and performs client authentication (e.g., verifying `client_id` and `client_secret` via `ClientSecretBasicAuthenticationConverter`). If an unauthenticated end-user hits `/oauth2/authorize`, the chain throws an `AuthenticationException`, triggering an entry point that redirects the user to the default web filter chain (`@Order(2)`), where standard form login or MFA takes place.
</details>

<details>
<summary>3. What is the role of `OAuth2TokenCustomizer<JwtEncodingContext>`?</summary>

**Answer:**
`OAuth2TokenCustomizer` is a hook invoked before a JWT is digitally signed. It receives the `JwtEncodingContext`, allowing developers to inspect the authenticated principal, client registration, and token type (Access Token or ID Token) and add, modify, or remove claims (e.g., adding user roles, tenant IDs, email, or department).
</details>

<details>
<summary>4. What is the difference between `RegisteredClientRepository` and `OAuth2AuthorizationService`?</summary>

**Answer:**
`RegisteredClientRepository` stores client metadata (e.g., registered client applications, client secrets, permitted scopes, redirect URIs).
`OAuth2AuthorizationService` stores the dynamic, runtime authorization transactions (e.g., active authorization codes, granted access tokens, refresh tokens, and consent state).
</details>

<details>
<summary>5. How does Refresh Token Rotation work in Spring Authorization Server?</summary>

**Answer:**
When configured with `tokenSettings.reuseRefreshTokens(false)`, each time a client presents a refresh token to obtain a new access token, the authorization server invalidates the used refresh token and issues a brand-new refresh token alongside the access token. If an attacker attempts to replay an older refresh token, the server detects the reuse and invalidates the entire authorization family.
</details>

<details>
<summary>6. How does the Authorization Server expose its public keys to Resource Servers?</summary>

**Answer:**
The server exposes an HTTP endpoint (by default at `/oauth2/jwks`) driven by a `JWKSource<SecurityContext>` bean. Resource servers fetch this JSON Web Key Set to verify signatures on issued JWT tokens without needing pre-shared secret keys or database access.
</details>

<details>
<summary>7. What is `AuthorizationServerSettings` and why is configuring `issuer` critical?</summary>

**Answer:**
`AuthorizationServerSettings` centralizes OAuth2/OIDC endpoint URI definitions. Setting `.issuer("https://auth.company.com")` is critical because this URL is stamped into the `iss` claim of all issued JWT tokens and published in `/.well-known/openid-configuration`. If incorrect, resource servers will reject tokens due to issuer mismatch.
</details>

<details>
<summary>8. How do you implement custom consent screens in Spring Authorization Server?</summary>

**Answer:**
Set `clientSettings.requireAuthorizationConsent(true)` on the `RegisteredClient`. Configure a custom consent page URL via `authorizationServerConfigurer.authorizationEndpoint(endpoint -> endpoint.consentPage("/oauth2/consent"))` and implement a Spring MVC controller that presents the requested scopes to the user and processes their consent submission.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Dual Filter Chain Setup (Easy)</summary>

**Task**: Configure two `SecurityFilterChain` beans where chain 1 handles OAuth2 Authorization Server endpoints with `@Order(1)` and chain 2 handles standard form login with `@Order(2)`.

**Solution**:
```java
@Configuration
@EnableWebSecurity
public class BasicSasConfig {

    @Bean
    @Order(1)
    public SecurityFilterChain sasFilterChain(HttpSecurity http) throws Exception {
        OAuth2AuthorizationServerConfiguration.applyDefaultSecurity(http);
        http.getConfigurer(OAuth2AuthorizationServerConfigurer.class).oidc(Customizer.withDefaults());
        http.exceptionHandling(exceptions -> 
            exceptions.authenticationEntryPoint(new LoginUrlAuthenticationEntryPoint("/login"))
        );
        return http.build();
    }

    @Bean
    @Order(2)
    public SecurityFilterChain webFilterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .formLogin(Customizer.withDefaults())
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 2: Public Client Configuration for Mobile Apps (Easy)</summary>

**Task**: Define a `RegisteredClient` for a mobile application that uses `ClientAuthenticationMethod.NONE`, requires PKCE, and allows `authorization_code` grant with redirect URI `myapp://callback`.

**Solution**:
```java
@Bean
public RegisteredClient mobileClient() {
    return RegisteredClient.withId(UUID.randomUUID().toString())
        .clientId("mobile-app")
        .clientAuthenticationMethod(ClientAuthenticationMethod.NONE)
        .authorizationGrantType(AuthorizationGrantType.AUTHORIZATION_CODE)
        .redirectUri("myapp://callback")
        .scope(OidcScopes.OPENID)
        .scope("profile")
        .clientSettings(ClientSettings.builder()
            .requireProofKeyForCodeExchange(true)
            .requireAuthorizationConsent(false)
            .build())
        .build();
}
```
</details>

<details>
<summary>Exercise 3: Custom Claim Injection for Tenant ID (Medium)</summary>

**Task**: Implement an `OAuth2TokenCustomizer<JwtEncodingContext>` that extracts a `tenant_id` attribute from the authenticated user's principal and injects it into every issued access token.

**Solution**:
```java
@Bean
public OAuth2TokenCustomizer<JwtEncodingContext> tokenCustomizer() {
    return context -> {
        if (OAuth2TokenType.ACCESS_TOKEN.equals(context.getTokenType())) {
            Authentication principal = context.getPrincipal();
            if (principal.getPrincipal() instanceof CustomUserDetails user) {
                context.getClaims().claim("tenant_id", user.getTenantId());
            }
        }
    };
}
```
</details>

<details>
<summary>Exercise 4: Short-Lived Access Tokens with Refresh Token Rotation (Medium)</summary>

**Task**: Configure `TokenSettings` for a registered client that grants 5-minute access tokens, 24-hour refresh tokens, and enforces refresh token rotation.

**Solution**:
```java
TokenSettings settings = TokenSettings.builder()
    .accessTokenTimeToLive(Duration.ofMinutes(5))
    .refreshTokenTimeToLive(Duration.ofHours(24))
    .reuseRefreshTokens(false) // Rotation enforced
    .accessTokenFormat(OAuth2TokenFormat.SELF_CONTAINED)
    .build();
```
</details>

<details>
<summary>Exercise 5: JdbcRegisteredClientRepository Schema Bean (Hard)</summary>

**Task**: Configure production database-backed `RegisteredClientRepository` and `OAuth2AuthorizationService` beans using `JdbcTemplate` and configure custom table column mappers if needed.

**Solution**:
```java
@Configuration
public class JdbcAuthorizationServerStorageConfig {

    @Bean
    public RegisteredClientRepository registeredClientRepository(JdbcTemplate jdbcTemplate) {
        return new JdbcRegisteredClientRepository(jdbcTemplate);
    }

    @Bean
    public OAuth2AuthorizationService authorizationService(
            JdbcTemplate jdbcTemplate, 
            RegisteredClientRepository registeredClientRepository) {
        return new JdbcOAuth2AuthorizationService(jdbcTemplate, registeredClientRepository);
    }

    @Bean
    public OAuth2AuthorizationConsentService authorizationConsentService(
            JdbcTemplate jdbcTemplate, 
            RegisteredClientRepository registeredClientRepository) {
        return new JdbcOAuth2AuthorizationConsentService(jdbcTemplate, registeredClientRepository);
    }
}
```
</details>

<details>
<summary>Exercise 6: Integration Testing Token Generation (Hard)</summary>

**Task**: Write an integration test using Spring Boot and `TestRestTemplate` or `MockMvc` that executes the client credentials grant (`grant_type=client_credentials`) against `/oauth2/token` and verifies that a signed JWT access token is returned.

**Solution**:
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@AutoConfigureMockMvc
class TokenEndpointIntegrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void clientCredentialsGrantShouldReturnJwtAccessToken() throws Exception {
        mockMvc.perform(post("/oauth2/token")
                .with(httpBasic("service-client", "client-secret"))
                .param("grant_type", "client_credentials")
                .param("scope", "internal:read"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.access_token").isString())
            .andExpect(jsonPath("$.token_type").value("Bearer"))
            .andExpect(jsonPath("$.expires_in").isNumber());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Dual Filter Chains**: Always maintain two chains: `@Order(1)` for SAS protocol endpoints, `@Order(2)` for human login and web resources.
- **Enforce PKCE**: Always require Proof Key for Code Exchange via `ClientSettings.builder().requireProofKeyForCodeExchange(true)`.
- **Customize Claims cleanly**: Inject tenant data, roles, and user metadata via `OAuth2TokenCustomizer<JwtEncodingContext>`.
- **Database Persistence**: Use `JdbcRegisteredClientRepository` and `JdbcOAuth2AuthorizationService` in multi-instance production deployments.
- **Rotate Refresh Tokens**: Set `reuseRefreshTokens(false)` to prevent replay attacks.

---

## 13. Further Reading
- [Spring Authorization Server Official Reference](https://docs.spring.io/spring-authorization-server/reference/index.html)
- [OAuth 2.1 Draft Specification](https://datatracker.ietf.org/doc/html/draft-ietf-oauth-v2-1-10)
- [RFC 8414: OAuth 2.0 Authorization Server Metadata](https://datatracker.ietf.org/doc/html/rfc8414)
- [OpenID Connect Core 1.0 Specification](https://openid.net/specs/openid-connect-core-1_0.html)
- [RFC 7636: Proof Key for Code Exchange (PKCE)](https://datatracker.ietf.org/doc/html/rfc7636)
