# 07. JWT Resource Server in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, RFC 7519 (JWT), RFC 7517 (JWK) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Keycloak 26, Auth0 |

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
In distributed cloud-native microservice systems, server-side HTTP sessions create significant operational friction: session state requires centralized caching clusters (e.g., Redis), introduces sticky-routing dependencies, and degrades horizontal elasticity. JSON Web Tokens (JWT, RFC 7519) solve this by packaging cryptographic assertions (claims) directly within self-contained, digitally signed tokens.

However, implementing JWT authentication naively introduces severe vulnerabilities:
- Trusting tokens signed by malicious algorithms (`alg: none` or symmetric HMAC confusion).
- Neglecting token signature verification or cryptographic key rotation.
- Poor mapping between identity provider (IdP) claims and Spring Security `GrantedAuthority` representations.
- Hardcoding static public keys instead of dynamically discovering keys via JSON Web Key Sets (JWKS, RFC 7517).

Spring Security's **OAuth2 Resource Server** provides a battle-tested, standard-compliant framework for validating bearer JWTs without writing custom filter cryptography.

### Historical Evolution
- **Spring Security OAuth 2.x (Legacy)**: Relied on `@EnableResourceServer` and `RemoteTokenServices`. Heavily coupled, difficult to customize, and deprecated in 2018.
- **Spring Security 5.x**: Introduced first-class native OAuth 2.0 Resource Server support directly in core Spring Security via `NimbusJwtDecoder` and standard Lambda DSL (`http.oauth2ResourceServer(oauth2 -> oauth2.jwt(...))`).
- **Spring Security 6.x / 7.x**: Fully refined for Jakarta EE 10/11 and Java 21. Standardized claims converters, automated JWKS caching and key rotation, enhanced Virtual Thread compatibility, and decoupled Opaque Token introspection from JWT processing.

### Real-World Relevance
Almost every modern enterprise backend API acts as an OAuth2 Resource Server. Whether receiving tokens from Keycloak, Okta, Auth0, Microsoft Entra ID (Azure AD), or an internal Spring Authorization Server, mastering `JwtDecoder`, `JwtAuthenticationConverter`, and multi-issuer routing is a fundamental core competency.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.security.oauth2.jwt.Jwt`**: An immutable representation of a decoded JSON Web Token containing headers, claims, issued/expiration timestamps, and raw string token.
- **`org.springframework.security.oauth2.jwt.JwtDecoder`**: Interface responsible for decoding and cryptographically verifying a string token into a `Jwt` instance. The standard implementation is `NimbusJwtDecoder`.
- **`org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken`**: The concrete `Authentication` token stored in `SecurityContextHolder`. Wraps the `Jwt` principal and resolved `Collection<GrantedAuthority>`.
- **`org.springframework.core.convert.converter.Converter<Jwt, ? extends AbstractAuthenticationToken>`**: Strategy for transforming a decoded `Jwt` into a customized `Authentication` object (e.g., extracting Keycloak `realm_access.roles` into `ROLE_` authorities).
- **`org.springframework.security.oauth2.jwt.JwtValidator`**: Pluggable validation contract used to verify claims beyond cryptographic signatures (e.g., issuer `iss`, audience `aud`, timestamp clocks skew).

### The Mental Model
```
[ HTTP Client ] 
       │
       │ Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...
       ▼
[ BearerTokenAuthenticationFilter ]
       │
       ├── Extracts Bearer String from Authorization header
       ▼
[ JwtAuthenticationProvider ]
       │
       ├── 1. JwtDecoder.decode(token)
       │         ├── Parses header (alg=RS256, kid=key-01)
       │         ├── Fetches JWK from https://idp.example.com/oauth2/jwks (cached)
       │         ├── Verifies cryptographic signature (RSA-SHA256)
       │         └── Executes JwtValidators (iss, aud, nbf, exp)
       │
       ├── 2. JwtAuthenticationConverter.convert(jwt)
       │         └── Maps claims (e.g. "roles": ["ADMIN"]) -> SimpleGrantedAuthority("ROLE_ADMIN")
       │
       ▼
[ SecurityContextHolder ]
       └── Sets JwtAuthenticationToken(principal=Jwt, authorities=[ROLE_ADMIN])
```

### Invariants
1. A Resource Server does **not** issue tokens; it only verifies them.
2. Cryptographic verification must always occur using the IdP's public key (retrieved via JWKS or pre-shared public cert); the private key remains strictly on the Authorization Server.
3. Signature verification alone is insufficient: the `exp` (expiration), `nbf` (not before), and `iss` (issuer) claims must be validated against expected values.

---

## 3. How It Works Internally

### Filter & Provider Lifecycle
1. **Extraction**: `BearerTokenAuthenticationFilter` runs in the filter chain before `AuthorizationFilter`. It invokes `BearerTokenResolver` (default checks `Authorization: Bearer <token>`).
2. **Delegation**: If a token is found, an unauthenticated `BearerTokenAuthenticationToken` is dispatched to `ProviderManager`.
3. **Authentication**: `JwtAuthenticationProvider` receives the token and invokes `JwtDecoder.decode(tokenValue)`.
4. **Nimbus Decoding Pipeline**:
   - `NimbusJwtDecoder` parses the JWS object.
   - Extracts the Key ID (`kid`) from the header.
   - Queries `JWKSource` (backed by a remote URL with an in-memory TTL cache).
   - Validates the signature against the matching public RSA/EC key.
   - Runs `DelegatingOAuth2TokenValidator` to check timestamp validity (with configurable clock skew, default 60s) and issuer identity.
5. **Authority Conversion**: The resulting `Jwt` object is passed to `JwtAuthenticationConverter`, which parses role claims and prefixes them (e.g., `SCOPE_` or `ROLE_`).
6. **Context Population**: A fully authenticated `JwtAuthenticationToken` is returned and placed into `SecurityContextHolder`.

---

## 4. Syntax & Usage

### 1. Minimal Application Configuration (`application.yml`)
```yaml
spring:
  security:
    oauth2:
      resourceserver:
        jwt:
          issuer-uri: https://auth.company.com/realms/production
          audiences:
            - enterprise-order-service
            - enterprise-billing-service
```

### 2. Standard Java Security Configuration
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationConverter;
import org.springframework.security.oauth2.server.resource.authentication.JwtGrantedAuthoritiesConverter;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableWebSecurity
public class ResourceServerSecurityConfig {

    @Bean
    public SecurityFilterChain resourceServerFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable()) // Stateless Bearer tokens are immune to CSRF
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**").permitAll()
                .requestMatchers("/api/admin/**").hasRole("ADMIN")
                .requestMatchers("/api/orders/**").hasAuthority("SCOPE_orders:read")
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2
                .jwt(jwt -> jwt.jwtAuthenticationConverter(jwtAuthenticationConverter()))
            )
            .build();
    }

    @Bean
    public JwtAuthenticationConverter jwtAuthenticationConverter() {
        JwtGrantedAuthoritiesConverter authoritiesConverter = new JwtGrantedAuthoritiesConverter();
        authoritiesConverter.setAuthoritiesClaimName("roles");
        authoritiesConverter.setAuthorityPrefix("ROLE_");

        JwtAuthenticationConverter converter = new JwtAuthenticationConverter();
        converter.setJwtGrantedAuthoritiesConverter(authoritiesConverter);
        return converter;
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Resource Server with Custom Audience Validator
OAuth2 specifications recommend validating that incoming tokens were specifically intended for this service (`aud` claim).

```java
package com.example.security.validator;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.oauth2.core.DelegatingOAuth2TokenValidator;
import org.springframework.security.oauth2.core.OAuth2Error;
import org.springframework.security.oauth2.core.OAuth2TokenValidator;
import org.springframework.security.oauth2.core.OAuth2TokenValidatorResult;
import org.springframework.security.oauth2.jwt.*;

import java.util.List;

@Configuration
public class JwtValidatorConfig {

    @Value("${spring.security.oauth2.resourceserver.jwt.issuer-uri}")
    private String issuerUri;

    @Value("${application.security.audience:order-service}")
    private String expectedAudience;

    @Bean
    public JwtDecoder jwtDecoder() {
        NimbusJwtDecoder jwtDecoder = JwtDecoders.fromIssuerLocation(issuerUri);

        OAuth2TokenValidator<Jwt> audienceValidator = new AudienceValidator(expectedAudience);
        OAuth2TokenValidator<Jwt> withIssuer = JwtValidators.createDefaultWithIssuer(issuerUri);
        OAuth2TokenValidator<Jwt> combinedValidator = new DelegatingOAuth2TokenValidator<>(withIssuer, audienceValidator);

        jwtDecoder.setJwtValidator(combinedValidator);
        return jwtDecoder;
    }

    static class AudienceValidator implements OAuth2TokenValidator<Jwt> {
        private final String audience;

        AudienceValidator(String audience) {
            this.audience = audience;
        }

        @Override
        public OAuth2TokenValidatorResult validate(Jwt jwt) {
            List<String> audiences = jwt.getAudience();
            if (audiences != null && audiences.contains(this.audience)) {
                return OAuth2TokenValidatorResult.success();
            }
            OAuth2Error error = new OAuth2Error("invalid_token", "The required audience '" + this.audience + "' is missing", null);
            return OAuth2TokenValidatorResult.failure(error);
        }
    }
}
```

### Example 2: Enterprise Keycloak Custom Claims Extractor
Keycloak nests roles inside `realm_access.roles` and `resource_access.{client-id}.roles`. Default Spring Security converters fail to locate these nested structures.

```java
package com.example.security.keycloak;

import org.springframework.core.convert.converter.Converter;
import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.stereotype.Component;

import java.util.*;
import java.util.stream.Collectors;

@Component
public class KeycloakJwtAuthenticationConverter implements Converter<Jwt, AbstractAuthenticationToken> {

    private static final String REALM_ACCESS = "realm_access";
    private static final String ROLES = "roles";

    @Override
    public AbstractAuthenticationToken convert(Jwt jwt) {
        Collection<GrantedAuthority> authorities = extractRealmRoles(jwt);
        return new JwtAuthenticationToken(jwt, authorities, extractPrincipalUsername(jwt));
    }

    private Collection<GrantedAuthority> extractRealmRoles(Jwt jwt) {
        Map<String, Object> realmAccess = jwt.getClaimAsMap(REALM_ACCESS);
        if (realmAccess == null || !realmAccess.containsKey(ROLES)) {
            return Collections.emptyList();
        }

        @SuppressWarnings("unchecked")
        List<String> roles = (List<String>) realmAccess.get(ROLES);

        return roles.stream()
            .map(role -> new SimpleGrantedAuthority("ROLE_" + role.toUpperCase()))
            .collect(Collectors.toSet());
    }

    private String extractPrincipalUsername(Jwt jwt) {
        if (jwt.hasClaim("preferred_username")) {
            return jwt.getClaimAsString("preferred_username");
        }
        return jwt.getSubject();
    }
}
```

### Example 3: Edge Case – Multi-Tenant Multi-Issuer Routing
In multi-tenant SaaS environments, different enterprise customers authenticate against their own IdPs (e.g., Tenant A uses Okta, Tenant B uses Azure AD). The resource server must dynamically resolve the JWKS endpoint based on the token's `iss` header.

```java
package com.example.security.multitenant;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.AuthenticationManagerResolver;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.oauth2.jwt.JwtDecoders;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationProvider;
import org.springframework.security.oauth2.server.resource.authentication.JwtIssuerAuthenticationManagerResolver;
import org.springframework.security.web.SecurityFilterChain;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@Configuration
public class MultiTenantResourceServerConfig {

    private final Map<String, AuthenticationManager> authenticationManagers = new ConcurrentHashMap<>();

    @Bean
    public SecurityFilterChain multiTenantFilterChain(HttpSecurity http) throws Exception {
        AuthenticationManagerResolver<HttpServletRequest> resolver = 
            JwtIssuerAuthenticationManagerResolver.fromTrustedIssuers(
                "https://accounts.google.com",
                "https://auth.tenant-alpha.com/oauth2",
                "https://auth.tenant-beta.com/oauth2"
            );

        return http
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .oauth2ResourceServer(oauth2 -> oauth2.authenticationManagerResolver(resolver))
            .build();
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Validating tokens by manually parsing base64 claims without signature verification, or pinning static public RSA keys in source code.

```java
// BUGGY & CRITICAL VULNERABILITY: Inspecting payload without cryptographic verification!
public boolean insecureValidate(String token) {
    String[] parts = token.split("\\.");
    String payload = new String(Base64.getUrlDecoder().decode(parts[1]));
    // Attacker can forge any JSON payload with "sub":"admin" and empty signature!
    return payload.contains("\"sub\":\"admin\"");
}
```

**Production Fix**:
Leverage Nimbus `JWSKeySelector` with dynamic JWKS discovery and algorithm constraint enforcement.

```java
// SECURE: Enforce asymmetric RS256 algorithm and verify cryptographic signature against JWKS
@Bean
public JwtDecoder secureDecoder(@Value("${spring.security.oauth2.resourceserver.jwt.jwk-set-uri}") String jwkSetUri) {
    return NimbusJwtDecoder.withJwkSetUri(jwkSetUri)
        .jwsAlgorithms(algorithms -> {
            algorithms.clear();
            algorithms.add(org.springframework.security.oauth2.jose.jws.SignatureAlgorithm.RS256);
            algorithms.add(org.springframework.security.oauth2.jose.jws.SignatureAlgorithm.ES256);
        })
        .cache(new com.nimbusds.jose.util.DefaultResourceRetriever(2000, 2000))
        .build();
}
```

### Example 5: High-Performance Non-Blocking WebFlux Reactive Resource Server
In high-throughput reactive microservices built on Project Reactor and Netty, JWT verification must be fully non-blocking.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.oauth2.jwt.NimbusReactiveJwtDecoder;
import org.springframework.security.oauth2.jwt.ReactiveJwtDecoder;
import org.springframework.security.web.server.SecurityWebFilterChain;

@Configuration
@EnableWebFluxSecurity
public class ReactiveResourceServerConfig {

    @Bean
    public SecurityWebFilterChain reactiveFilterChain(ServerHttpSecurity http, ReactiveJwtDecoder reactiveJwtDecoder) {
        return http
            .csrf(ServerHttpSecurity.CsrfSpec::disable)
            .authorizeExchange(exchanges -> exchanges
                .pathMatchers("/actuator/health").permitAll()
                .pathMatchers("/orders/**").hasAuthority("SCOPE_orders")
                .anyExchange().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2
                .jwt(jwt -> jwt.jwtDecoder(reactiveJwtDecoder))
            )
            .build();
    }

    @Bean
    public ReactiveJwtDecoder reactiveJwtDecoder() {
        return NimbusReactiveJwtDecoder
            .withJwkSetUri("https://auth.company.com/oauth2/v1/keys")
            .build();
    }
}
```

---

## 6. Best Practices

1. **Always Verify Audience (`aud`) and Issuer (`iss`)**: Never accept a token simply because its signature is valid. Verify that the token was explicitly intended for this service's audience.
2. **Set Restrictive Allowed Algorithms**: Explicitly restrict acceptable JWS signature algorithms to `RS256` or `ES256`. Prevent algorithm switching attacks (`none` or `HS256` confusion).
3. **Configure Connection Timeouts for JWKS Retrieval**: Set reasonable connection and read timeouts on the internal HTTP client fetching keys from the IdP (e.g., 2000ms) to prevent network hiccups from blocking worker threads.
4. **Use Stateless Session Creation Policy**: Ensure `SessionCreationPolicy.STATELESS` is active to stop Spring Security from creating unnecessary `HttpSession` instances.
5. **Keep Clock Skew Minimal**: Spring defaults to a 60-second clock skew for `exp` and `nbf` checks. Keep it at or below 60 seconds to avoid extending the attack window of expired tokens.
6. **Gracefully Handle Key Rotation**: JWKS endpoints publish key IDs (`kid`). `NimbusJwtDecoder` automatically fetches new keys when an unknown `kid` is encountered, but configure rate-limiting on key retrieval to prevent denial-of-service cache busting.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `aud` validation | Tokens issued for external downstream services can be replayed against your service. | Add a custom `AudienceValidator` via `DelegatingOAuth2TokenValidator`. |
| Hardcoding RSA public keys in application code | Key rotation on the identity provider immediately breaks API authentication. | Point to `jwk-set-uri` or `issuer-uri` for automated key discovery and rotation. |
| Relying on default `SCOPE_` mapping for roles | Keycloak/Auth0 roles are not recognized by `@Secured("ROLE_ADMIN")` or `hasRole("ADMIN")`. | Supply a custom `Converter<Jwt, AbstractAuthenticationToken>` mapping custom role claims. |
| Neglecting clock skew synchronization | Server time drift between IdP and Resource Server causes legitimate fresh tokens to be rejected as expired. | Ensure NTP synchronization across servers and utilize reasonable `OAuth2TokenValidator` skew. |
| Unbounded JWKS caching | When keys rotate during security incidents, the resource server rejects new tokens until cache TTL expires. | Configure cache refresh intervals or rely on on-demand refresh for unknown `kid`. |
| Leaving CSRF enabled on pure REST APIs | Causes valid non-browser clients or SPAs with Bearer tokens to be rejected with 403 on `POST`/`PUT`. | Call `csrf.disable()` when authentication is exclusively mediated via `Authorization: Bearer`. |

---

## 8. Performance & Security Notes

- **Cryptographic Verification Cost**: Asymmetric signature verification (RSA-2048/4096) requires non-trivial CPU cycles (modular exponentiation). In high-throughput systems (10k+ RPS), RSA verification can consume significant CPU. `ES256` (ECDSA with P-256) offers faster verification and smaller keys.
- **Virtual Thread Friendliness**: In Java 21+, `NimbusJwtDecoder` performs cryptographic operations on CPU without pinning carrier threads. However, HTTP JWKS cache misses perform network I/O. Virtual threads handle these non-blocking I/O pauses seamlessly without carrier exhaustion.
- **Memory Footprint**: `Jwt` objects are ephemeral and garbage-collected quickly after request completion. Avoid storing full `Jwt` payloads in persistent caches or long-lived static collections.

---

## 9. Comparison / When to Use What

| Security Mechanism | State Location | Verification Strategy | Revocation Speed | Best Use Case |
|---|---|---|---|---|
| **Stateless Bearer JWT** | Client / Token | Cryptographic signature via JWKS | Difficult (Requires short TTL or blacklist) | Distributed microservices, mobile apps, cross-domain APIs |
| **Opaque Token (Introspection)**| Authorization Server | Network RPC to `/oauth2/introspect` | Instantaneous | High-security banking APIs, immediate privilege revocation |
| **Server-Side Session** | Server Memory / Redis | Database / Redis lookup by `JSESSIONID` | Instantaneous | Traditional server-rendered web applications (Thymeleaf/MVC) |
| **Mutual TLS (mTLS)** | TLS Handshake | Client X.509 certificate validation | Real-time (CRL / OCSP) | High-trust inter-service zero-trust mesh (Kubernetes/Istio) |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring Security locate the public key when decoding a JWT?</summary>

**Answer:**
When configured with `spring.security.oauth2.resourceserver.jwt.jwk-set-uri` (or derived from `issuer-uri` via `.well-known/openid-configuration`), Spring Security's `NimbusJwtDecoder` fetches the JSON Web Key Set from the identity provider. When an incoming token arrives, it inspects the token header's `kid` (Key ID), finds the corresponding public key in its local JWKS cache, and verifies the signature. If the `kid` is missing from the cache, it refreshes the JWKS from the IdP.
</details>

<details>
<summary>2. Why does `hasRole("ADMIN")` fail when the JWT contains `"roles": ["ADMIN"]` by default?</summary>

**Answer:**
By default, Spring Security's `JwtGrantedAuthoritiesConverter` extracts authorities from the `scope` or `scp` claim and prefixes each entry with `SCOPE_`. If an IdP provides roles in a `roles` or `authorities` claim, the default converter ignores them. Furthermore, Spring's `hasRole("ADMIN")` expression looks for an authority named `ROLE_ADMIN`. You must configure a custom `JwtGrantedAuthoritiesConverter` or custom converter bean to parse the proper claim and apply the `ROLE_` prefix.
</details>

<details>
<summary>3. What is the difference between an Opaque Token and a JWT in Resource Server design?</summary>

**Answer:**
A JWT is self-contained and cryptographically signed; the Resource Server can verify its signature, claims, and expiration locally using public keys without contacting the IdP.
An Opaque Token is a random string (by-reference token) with no internal cryptographic structure. To validate an opaque token, the Resource Server must make a network call (introspection endpoint, RFC 7662) to the Authorization Server on every request. Opaque tokens offer immediate revocation but impose network latency and high load on the IdP.
</details>

<details>
<summary>4. How does `DelegatingOAuth2TokenValidator` work in Spring Security?</summary>

**Answer:**
It is a composite implementation of `OAuth2TokenValidator<Jwt>` that chains multiple individual validators together. For example, it can combine `JwtTimestampValidator` (validates `exp` and `nbf`), `JwtIssuerValidator` (validates `iss`), and a custom `AudienceValidator` (validates `aud`). If any validator in the chain produces an `OAuth2TokenValidatorResult.failure()`, the overall validation fails and the token is rejected.
</details>

<details>
<summary>5. What is the "JWT None Algorithm Attack" and how does Spring Security guard against it?</summary>

**Answer:**
In the none algorithm attack, an attacker alters the JWT header to `{"alg": "none"}` and strips the signature, hoping the parser accepts the token without verification. `NimbusJwtDecoder` explicitly rejects tokens with `alg: none` by requiring a valid signature algorithm (e.g., `RS256`). Spring Security does not allow unsigned tokens unless explicitly configured with a mock decoder.
</details>

<details>
<summary>6. Can a revoked JWT still be used to access a Resource Server?</summary>

**Answer:**
Yes, until the token's `exp` timestamp passes, because the Resource Server validates tokens statelessly using cryptographic signatures without checking server state. To mitigate this:
1. Keep JWT lifetimes very short (e.g., 5-15 minutes).
2. Maintain a distributed token blacklist/revocation list in Redis for revoked token IDs (`jti`).
3. Transition to Opaque Tokens or Token Introspection for sensitive actions.
</details>

<details>
<summary>7. How does Spring Security handle key rotation without service downtime?</summary>

**Answer:**
Identity providers publish multiple public keys in their JWKS endpoint. During a key rollover, the IdP publishes both the retiring key and the new key. `NimbusJwtDecoder` caches these keys. When a request arrives with a `kid` not found in the local cache, the decoder proactively refreshes the JWKS from the IdP. Once verified, it updates its cache, allowing transparent key rotation with zero API downtime.
</details>

<details>
<summary>8. How do you extract custom claims (like `tenant_id` or `email`) inside a Spring MVC controller?</summary>

**Answer:**
You can inject the principal directly into the controller method using `@AuthenticationPrincipal Jwt jwt`:
```java
@GetMapping("/me")
public UserProfile getProfile(@AuthenticationPrincipal Jwt jwt) {
    String email = jwt.getClaimAsString("email");
    String tenantId = jwt.getClaimAsString("tenant_id");
    return new UserProfile(jwt.getSubject(), email, tenantId);
}
```
Alternatively, inject `JwtAuthenticationToken` or access `SecurityContextHolder.getContext().getAuthentication()`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Minimal Resource Server Setup (Easy)</summary>

**Task**: Configure a minimal Spring Boot `SecurityFilterChain` that enforces JWT authentication on `/api/**` using an external issuer URI `https://idp.example.com/oauth2` and permits `/api/health` anonymously.

**Solution**:
```java
@Configuration
public class SimpleResourceServerConfig {

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable())
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/health").permitAll()
                .requestMatchers("/api/**").authenticated()
                .anyRequest().denyAll()
            )
            .oauth2ResourceServer(oauth2 -> oauth2
                .jwt(Customizer.withDefaults())
            )
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 2: Custom Role Claim Mapping (Easy)</summary>

**Task**: Create a `JwtAuthenticationConverter` bean that reads user roles from a top-level claim named `authorities` and ensures they are mapped to Spring Security authorities with prefix `ROLE_`.

**Solution**:
```java
@Bean
public JwtAuthenticationConverter customJwtAuthenticationConverter() {
    JwtGrantedAuthoritiesConverter grantedAuthoritiesConverter = new JwtGrantedAuthoritiesConverter();
    grantedAuthoritiesConverter.setAuthoritiesClaimName("authorities");
    grantedAuthoritiesConverter.setAuthorityPrefix("ROLE_");

    JwtAuthenticationConverter jwtAuthenticationConverter = new JwtAuthenticationConverter();
    jwtAuthenticationConverter.setJwtGrantedAuthoritiesConverter(grantedAuthoritiesConverter);
    return jwtAuthenticationConverter;
}
```
</details>

<details>
<summary>Exercise 3: Strict Audience and Issuer Token Validator (Medium)</summary>

**Task**: Implement a custom `OAuth2TokenValidator<Jwt>` that verifies the `iss` claim equals `https://auth.internal.net` and that the `aud` claim contains the string `inventory-api`. Register it with `NimbusJwtDecoder`.

**Solution**:
```java
@Bean
public JwtDecoder customJwtDecoder() {
    String issuer = "https://auth.internal.net";
    NimbusJwtDecoder decoder = JwtDecoders.fromIssuerLocation(issuer);

    OAuth2TokenValidator<Jwt> audienceValidator = jwt -> {
        List<String> aud = jwt.getAudience();
        if (aud != null && aud.contains("inventory-api")) {
            return OAuth2TokenValidatorResult.success();
        }
        return OAuth2TokenValidatorResult.failure(new OAuth2Error("invalid_token", "Audience inventory-api missing", null));
    };

    OAuth2TokenValidator<Jwt> defaultWithIssuer = JwtValidators.createDefaultWithIssuer(issuer);
    decoder.setJwtValidator(new DelegatingOAuth2TokenValidator<>(defaultWithIssuer, audienceValidator));
    return decoder;
}
```
</details>

<details>
<summary>Exercise 4: Token Blacklist Filter via Redis (Medium)</summary>

**Task**: Implement a custom `OncePerRequestFilter` placed after `BearerTokenAuthenticationFilter` that inspects the authenticated `Jwt`'s `jti` (JWT ID) claim against a Redis blacklist. If present, clear `SecurityContextHolder` and respond with HTTP 401 Unauthorized.

**Solution**:
```java
public class JwtBlacklistFilter extends OncePerRequestFilter {

    private final StringRedisTemplate redisTemplate;

    public JwtBlacklistFilter(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth instanceof JwtAuthenticationToken jwtAuth) {
            String jti = jwtAuth.getToken().getId();
            if (jti != null && Boolean.TRUE.equals(redisTemplate.hasKey("blacklist:jwt:" + jti))) {
                SecurityContextHolder.clearContext();
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
                response.getWriter().write("{\"error\": \"Token has been revoked\"}");
                return;
            }
        }
        filterChain.doFilter(request, response);
    }
}
```
</details>

<details>
<summary>Exercise 5: Dynamic Multi-Issuer Key Resolver (Hard)</summary>

**Task**: Implement a dynamic `AuthenticationManagerResolver<HttpServletRequest>` that validates JWTs against two completely independent Okta authorization servers based on the URL path (`/tenants/us/**` vs `/tenants/eu/**`).

**Solution**:
```java
@Configuration
public class RegionTenantResourceServerConfig {

    @Bean
    public SecurityFilterChain regionalFilterChain(HttpSecurity http) throws Exception {
        AuthenticationManager usManager = new JwtAuthenticationProvider(
            JwtDecoders.fromIssuerLocation("https://us-auth.company.com")
        )::authenticate;

        AuthenticationManager euManager = new JwtAuthenticationProvider(
            JwtDecoders.fromIssuerLocation("https://eu-auth.company.com")
        )::authenticate;

        AuthenticationManagerResolver<HttpServletRequest> resolver = request -> {
            if (request.getRequestURI().startsWith("/tenants/us")) {
                return usManager;
            }
            return euManager;
        };

        return http
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .oauth2ResourceServer(oauth2 -> oauth2.authenticationManagerResolver(resolver))
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Unit Testing Resource Server Endpoints (Hard)</summary>

**Task**: Write a unit test using `@WebMvcTest` and `SecurityMockMvcRequestPostProcessors.jwt()` verifying that an endpoint `/api/admin/metrics` returns 200 OK when the JWT has `ROLE_ADMIN` and 403 Forbidden when it only has `ROLE_USER`.

**Solution**:
```java
@WebMvcTest(controllers = MetricsController.class)
@Import(ResourceServerSecurityConfig.class)
class MetricsControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void userWithRoleAdminShouldAccessMetrics() throws Exception {
        mockMvc.perform(get("/api/admin/metrics")
                .with(jwt().authorities(new SimpleGrantedAuthority("ROLE_ADMIN"))))
            .andExpect(status().isOk());
    }

    @Test
    void userWithRoleUserShouldBeForbidden() throws Exception {
        mockMvc.perform(get("/api/admin/metrics")
                .with(jwt().authorities(new SimpleGrantedAuthority("ROLE_USER"))))
            .andExpect(status().isForbidden());
    }

    @Test
    void unauthenticatedRequestShouldReturn401() throws Exception {
        mockMvc.perform(get("/api/admin/metrics"))
            .andExpect(status().isUnauthorized());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Resource Server verifies, does not issue**: Validates signatures against remote JWKS without holding the signing private key.
- **Audience (`aud`) validation is essential**: Protects against token redirection attacks across microservice boundaries.
- **Custom claims converter required for Keycloak/Auth0**: Default Spring converters only map `scope` / `scp` claims to `SCOPE_` authorities.
- **Stateless by definition**: Set `SessionCreationPolicy.STATELESS` and disable CSRF when relying exclusively on `Authorization: Bearer`.
- **Fast fail on expired tokens**: Nimbus handles timestamp checks with a 60-second default leeway. Ensure server clocks are synchronized via NTP.

---

## 13. Further Reading
- [Spring Security Reference: OAuth 2.0 Resource Server](https://docs.spring.io/spring-security/reference/servlet/oauth2/resource-server/index.html)
- [RFC 7519: JSON Web Token (JWT)](https://datatracker.ietf.org/doc/html/rfc7519)
- [RFC 7517: JSON Web Key (JWK)](https://datatracker.ietf.org/doc/html/rfc7517)
- [Nimbus JOSE + JWT Library Documentation](https://connect2id.com/products/nimbus-jose-jwt)
- [Keycloak OpenID Connect Documentation](https://www.keycloak.org/docs/latest/securing_apps/)
