# 03: Designing a Distributed Authentication & Identity System

| Metadata Field | Value |
|---|---|
| **Topic** | Enterprise Distributed Auth, OAuth2/OIDC, JWT & Key Rotation |
| **Spring Boot Version** | 3.4.x |
| **Spring Security** | 6.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | Redis (Token Revocation & Sessions), PostgreSQL, Spring Authorization Server |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

In distributed microservice ecosystems, authentication (verifying identity) and authorization (verifying permissions) must be decoupled from individual business services. A centralized identity architecture provides Single Sign-On (SSO), standardized token issuance (OAuth 2.1 & OpenID Connect), and cryptographic trust federation without requiring every microservice to query the user database on every HTTP request.

A production-grade distributed auth system balances stateless performance (verifying signed JSON Web Tokens locally via public keys) with stateful control (instant token revocation via distributed Redis blocklists, refresh token rotation, and multi-factor authentication).

---

## 2. Core Concepts

### 2.1 Access Tokens vs. Refresh Tokens
- **Access Token (Short-lived: 5–15 mins)**: Digitally signed JWT (RSA RS256 or Elliptic Curve ES256). Downstream microservices validate the signature locally without calling the Auth Server.
- **Refresh Token (Long-lived: 7–30 days)**: Opaque UUID or encrypted token stored securely in database/Redis. Used strictly against the Auth Server to obtain new access tokens.
- **Refresh Token Rotation (RTR)**: Every time a refresh token is exchanged, a new refresh token is issued and the old one is invalidated. If an old refresh token is reused, the entire token family is revoked (compromise detection).

### 2.2 JWKS & Asymmetric Key Rotation
The Auth Server signs JWTs with a private key and exposes public keys at the `/.well-known/jwks.json` endpoint (JSON Web Key Set). Downstream Resource Servers cache public keys. During key rotation, the Auth Server introduces a new `kid` (Key ID), allowing seamless verification without downtime.

---

## 3. Internals & Architecture

```
        [ Client (SPA / Mobile) ]
                    │
         1. POST /oauth2/token (Credentials)
                    ▼
     ┌────────────────────────────────────────────────────────┐
     │         Spring Authorization Server (Auth Service)     │
     │                                                        │
     │  ┌──────────────────────────────────────────────────┐  │
     │  │ AuthenticationProvider (BCrypt / Argon2 Hash)    │  │
     │  └────────────────────────┬─────────────────────────┘  │
     │                           ▼                            │
     │  ┌──────────────────────────────────────────────────┐  │
     │  │ RSA Private Key: Signs JWT (kid: "key-2026-a")   │  │
     │  └────────────────────────┬─────────────────────────┘  │
     │                           ▼                            │
     │  ┌──────────────────────────────────────────────────┐  │
     │  │ Save Refresh Token in PostgreSQL & Redis Family  │  │
     │  └──────────────────────────────────────────────────┘  │
     └───────────────────────────┬────────────────────────────┘
                                 │ Returns: Access Token (JWT) + Refresh Token
                                 ▼
        [ Client makes API Request with Bearer Token ]
                                 │
                                 ▼
     ┌────────────────────────────────────────────────────────┐
     │              API Gateway / Resource Server             │
     │                                                        │
     │  1. Check Redis Revocation List: "revoked:jti:{id}"    │
     │  2. Local JWT Validation via cached JWKS Public Key    │
     │  3. Extract Claims: sub, roles, tenant_id              │
     └───────────────────────────┬────────────────────────────┘
                                 │ (Forward with SecurityContext)
                                 ▼
                     ┌───────────────────────┐
                     │ Order / User Service  │
                     └───────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 JWKS Endpoint Structure (`/.well-known/jwks.json`)
```json
{
  "keys": [
    {
      "kty": "RSA",
      "e": "AQAB",
      "use": "sig",
      "kid": "auth-key-2026-v1",
      "alg": "RS256",
      "n": "u1P8vQ..."
    }
  ]
}
```

---

## 5. 5 Practical Examples

### Example 1: Resource Server Configuration with JWKS Caching

```java
package com.example.auth.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.oauth2.jwt.JwtDecoder;
import org.springframework.security.oauth2.jwt.NimbusJwtDecoder;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
public class ResourceServerConfig {

    @Value("${spring.security.oauth2.resourceserver.jwt.jwk-set-uri}")
    private String jwkSetUri;

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
                .csrf(csrf -> csrf.disable())
                .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
                .authorizeHttpRequests(auth -> auth
                        .requestMatchers("/api/v1/public/**", "/actuator/health").permitAll()
                        .requestMatchers("/api/v1/admin/**").hasRole("ADMIN")
                        .anyRequest().authenticated()
                )
                .oauth2ResourceServer(oauth2 -> oauth2.jwt(jwt -> jwt.decoder(jwtDecoder())))
                .build();
    }

    @Bean
    public JwtDecoder jwtDecoder() {
        return NimbusJwtDecoder.withJwkSetUri(jwkSetUri).build();
    }
}
```

### Example 2: Distributed JWT Revocation Filter with Redis

```java
package com.example.auth.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class JwtRevocationCheckFilter extends OncePerRequestFilter {

    private final StringRedisTemplate redisTemplate;

    public JwtRevocationCheckFilter(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth != null && auth.getPrincipal() instanceof Jwt jwt) {
            String jti = jwt.getId(); // JWT ID claim
            if (jti != null) {
                Boolean isRevoked = redisTemplate.hasKey("revoked:jwt:" + jti);
                if (Boolean.TRUE.equals(isRevoked)) {
                    response.setStatus(HttpStatus.UNAUTHORIZED.value());
                    response.getWriter().write("{\"error\":\"Token has been revoked\"}");
                    return;
                }
            }
        }
        filterChain.doFilter(request, response);
    }
}
```

### Example 3: Refresh Token Entity with Rotation Tracking

```java
package com.example.auth.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "refresh_tokens")
public class RefreshToken {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false, unique = true)
    private String token;

    @Column(name = "user_id", nullable = false)
    private String userId;

    @Column(name = "token_family", nullable = false)
    private UUID tokenFamily;

    @Column(nullable = false)
    private boolean revoked = false;

    @Column(name = "expires_at", nullable = false)
    private Instant expiresAt;

    public RefreshToken() {}

    public RefreshToken(String token, String userId, UUID tokenFamily, Instant expiresAt) {
        this.token = token;
        this.userId = userId;
        this.tokenFamily = tokenFamily;
        this.expiresAt = expiresAt;
    }

    public boolean isExpired() {
        return Instant.now().isAfter(expiresAt);
    }

    // Getters and setters
    public UUID getId() { return id; }
    public String getToken() { return token; }
    public String getUserId() { return userId; }
    public UUID getTokenFamily() { return tokenFamily; }
    public boolean isRevoked() { return revoked; }
    public void setRevoked(boolean revoked) { this.revoked = revoked; }
}
```

### Example 4: Refresh Token Rotation (RTR) Service with Compromise Detection

```java
package com.example.auth.service;

import com.example.auth.domain.RefreshToken;
import com.example.auth.repository.RefreshTokenRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;

@Service
public class TokenRotationService {

    private static final Logger log = LoggerFactory.getLogger(TokenRotationService.class);
    private final RefreshTokenRepository repository;

    public TokenRotationService(RefreshTokenRepository repository) {
        this.repository = repository;
    }

    @Transactional
    public RefreshToken rotateRefreshToken(String rawToken) {
        RefreshToken token = repository.findByToken(rawToken)
                .orElseThrow(() -> new IllegalArgumentException("Invalid refresh token"));

        if (token.isRevoked() || token.isExpired()) {
            log.error("Compromised token reuse detected for family {}! Revoking all tokens.", token.getTokenFamily());
            repository.revokeAllInFamily(token.getTokenFamily());
            throw new SecurityException("Suspicious activity detected. Please re-authenticate.");
        }

        // Invalidate current token
        token.setRevoked(true);
        repository.save(token);

        // Issue new token in the same family
        RefreshToken newToken = new RefreshToken(
                UUID.randomUUID().toString(),
                token.getUserId(),
                token.getTokenFamily(),
                Instant.now().plus(7, ChronoUnit.DAYS)
        );
        return repository.save(newToken);
    }
}
```

### Example 5: High-Performance Key Rotation Manager

```java
package com.example.auth.security;

import com.nimbusds.jose.jwk.JWKSet;
import com.nimbusds.jose.jwk.RSAKey;
import org.springframework.stereotype.Component;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class KeyRotationManager {

    private final ConcurrentHashMap<String, RSAKey> keyStore = new ConcurrentHashMap<>();
    private volatile String currentActiveKeyId;

    public KeyRotationManager() throws Exception {
        rotateKey();
    }

    public synchronized void rotateKey() throws Exception {
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
        kpg.initialize(2048);
        KeyPair kp = kpg.generateKeyPair();

        String keyId = "key-" + UUID.randomUUID().toString().substring(0, 8);
        RSAKey rsaKey = new RSAKey.Builder((RSAPublicKey) kp.getPublic())
                .privateKey((RSAPrivateKey) kp.getPrivate())
                .keyID(keyId)
                .build();

        keyStore.put(keyId, rsaKey);
        this.currentActiveKeyId = keyId;
    }

    public RSAKey getActiveSigningKey() {
        return keyStore.get(currentActiveKeyId);
    }

    public JWKSet getPublicJwkSet() {
        return new JWKSet(keyStore.values().stream().map(RSAKey::toPublicJWK).toList());
    }
}
```

---

## 6. Best Practices

- **Keep Access Tokens Short-Lived**: Set JWT expiration to 5–15 minutes. This minimizes the vulnerability window if a token is intercepted without needing aggressive database checks.
- **Store Refresh Tokens in HttpOnly, Secure, SameSite Cookies**: Prevent Cross-Site Scripting (XSS) from reading refresh tokens.
- **Implement Refresh Token Rotation (RTR)**: Detect token theft by tracking family UUIDs; immediately invalidate the session if an older refresh token is submitted.
- **Use RS256 or ES256 (Asymmetric)**: Never use symmetric HS256 for microservices; sharing a shared secret with 50 microservices means one compromised service compromises all signatures.
- **Cache JWKS with HTTP Cache-Control**: Ensure Resource Servers cache the JWKS keys locally and only refresh when an unknown `kid` is encountered.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Symmetric HS256 in Microservices** | Every microservice needs the secret key to verify tokens. If one service is breached, the attacker can forge admin tokens. | Use Asymmetric RS256/ES256 (Private key on Auth Server only, Public JWKS everywhere). |
| **Storing JWTs in LocalStorage** | Any XSS vulnerability exposes tokens to malicious JavaScript scripts. | Store refresh tokens in `HttpOnly; Secure; SameSite=Strict` cookies. |
| **Indefinite JWT Expiry** | Lost or stolen tokens remain valid forever; revocation requires expensive centralized blocklists. | Cap access token life at 10 minutes and use refresh token rotation. |
| **Querying DB on Every JWT Request** | Defeats the entire purpose of stateless JWTs, creating a database bottleneck. | Validate signatures locally with public keys; check Redis blocklist only for revoked IDs. |
| **Missing `kid` (Key ID) in JWT Header** | During key rotation, downstream services fail to match the new public key, causing 100% auth failure. | Always include the `kid` claim in the JWT JOSE header. |
| **Not Checking `aud` (Audience) Claim** | A token issued for Mobile App A can be submitted to Internal API B. | Validate `aud` and `iss` claims strictly in the Resource Server configuration. |

---

## 8. Performance & Security Considerations

### 8.1 Argon2id vs BCrypt
Use Argon2id for password hashing in new systems: it provides memory-hardness resistant to GPU/ASIC cracking attacks.
```java
@Bean
public PasswordEncoder passwordEncoder() {
    return new Argon2PasswordEncoder(16, 32, 1, 65536, 3);
}
```

### 8.2 Redis Revocation Eviction Sizing
When adding a revoked `jti` to Redis, set the Redis TTL equal to the remaining lifespan of the JWT:
$$\text{Redis TTL} = \text{jwt.getExpiresAt()} - \text{Instant.now()}$$
This guarantees that revoked records self-cleanse automatically when the token expires naturally.

---

## 9. Comparison / Decision Matrix

| Dimension | Stateful Sessions (Cookie/Redis) | Stateless JWT (RS256) | Hybrid (Short JWT + RTR) |
|---|---|---|---|
| **Revocation Speed** | Instant | Impossible until expiry | Instant via Refresh / Fast via Redis |
| **DB / Cache Overhead** | High (Read per request) | Zero (Local signature check) | Low (Check only on refresh & blacklist) |
| **Microservice Fit** | Poor (Requires shared session store) | Excellent | Excellent (Industry standard) |
| **Mobile / SPA Fit** | Difficult (Cookie cross-origin) | Good | Excellent |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do downstream microservices validate JWTs without hitting the Auth Server?</strong></summary>

Downstream Resource Servers fetch the Auth Server's public keys once from the `/.well-known/jwks.json` endpoint and cache them in memory. When a request arrives with a Bearer JWT, the Resource Server inspects the `kid` in the header and validates the cryptographic signature locally using the corresponding public key.
</details>

<details>
<summary><strong>2. How do you revoke a stateless JWT immediately if a user logs out?</strong></summary>

Extract the unique JWT identifier (`jti`) from the token and write it to a distributed Redis set (`revoked:jwt:{jti}`) with a TTL equal to the token's remaining validity duration. An API Gateway filter checks Redis before routing requests.
</details>

<details>
<summary><strong>3. What is Refresh Token Rotation, and how does it detect token theft?</strong></summary>

When a client uses a refresh token, the server revokes it and issues a new pair. If an attacker and a legitimate client both attempt to use the same refresh token, the server detects that an already-revoked token was submitted. It immediately revokes the entire token family, forcing both parties to log in again.
</details>

<details>
<summary><strong>4. Why is RS256 preferred over HS256 in microservices?</strong></summary>

RS256 uses asymmetric cryptography (public/private key pair). The Auth Server holds the private key to sign tokens; microservices only need the public key to verify them. HS256 uses a symmetric shared secret, meaning any microservice with the key could forge valid tokens.
</details>

<details>
<summary><strong>5. How do you handle JWT key rotation without causing service outages?</strong></summary>

1. Generate new Key Pair B with a new `kid`.
2. Add Key B to the JWKS endpoint alongside existing Key A.
3. Wait for Resource Servers to refresh their JWKS cache.
4. Begin signing new JWTs with Key B.
5. After all Key A tokens expire, decommission Key A.
</details>

<details>
<summary><strong>6. What are the essential claims in a security JWT?</strong></summary>

`sub` (Subject/User ID), `iss` (Issuer), `aud` (Audience), `exp` (Expiration time), `iat` (Issued at), and `jti` (Unique JWT ID for revocation).
</details>

<details>
<summary><strong>7. How do you mitigate CSRF attacks when using JWTs?</strong></summary>

If JWTs are passed via the `Authorization: Bearer <token>` header, CSRF is inherently mitigated because browsers do not attach custom headers to cross-site requests. If stored in cookies, the cookie must have `SameSite=Strict` or `Lax` flags set.
</details>

<details>
<summary><strong>8. What is the difference between OAuth 2.0 and OpenID Connect (OIDC)?</strong></summary>

OAuth 2.0 is an authorization framework designed for granting third-party applications scoped access to APIs (issuing Access Tokens). OpenID Connect (OIDC) is an identity layer built on top of OAuth 2.0 that provides user authentication (issuing an ID Token containing user profile identity).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Spring Security Resource Server with Custom Claims</strong></summary>

**Prompt**: Extract a custom `tenant_id` claim from a JWT and populate it into the Spring `SecurityContext`.

**Solution**:
```java
@Bean
public JwtAuthenticationConverter jwtAuthenticationConverter() {
    JwtAuthenticationConverter converter = new JwtAuthenticationConverter();
    converter.setJwtGrantedAuthoritiesConverter(jwt -> {
        String tenant = jwt.getClaimAsString("tenant_id");
        TenantContext.setCurrentTenant(tenant);
        return Collections.emptyList();
    });
    return converter;
}
```
</details>

<details>
<summary><strong>Exercise 2: Implement Redis Token Blacklist with Auto-Expiry</strong></summary>

**Prompt**: Write a method that extracts the `exp` timestamp from a JWT, calculates remaining seconds, and stores the `jti` in Redis.

**Solution**:
```java
public void blacklistToken(Jwt jwt) {
    Instant expiresAt = jwt.getExpiresAt();
    long secondsRemaining = Duration.between(Instant.now(), expiresAt).getSeconds();
    if (secondsRemaining > 0) {
        redis.opsForValue().set("revoked:jwt:" + jwt.getId(), "true", Duration.ofSeconds(secondsRemaining));
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Argon2 Password Hashing</strong></summary>

**Prompt**: Define an `Argon2PasswordEncoder` bean with 64MB memory cost and 3 iterations.

**Solution**:
```java
@Bean
public PasswordEncoder passwordEncoder() {
    return new Argon2PasswordEncoder(16, 32, 1, 65536, 3);
}
```
</details>

<details>
<summary><strong>Exercise 4: Implement Refresh Token Family Revocation Query</strong></summary>

**Prompt**: Write a Spring Data JPA `@Modifying` query to revoke all tokens in a given family.

**Solution**:
```java
@Modifying
@Query("UPDATE RefreshToken r SET r.revoked = true WHERE r.tokenFamily = :family")
void revokeAllInFamily(@Param("family") UUID family);
```
</details>

<details>
<summary><strong>Exercise 5: Build a Public JWKS REST Controller</strong></summary>

**Prompt**: Expose the public keys from `KeyRotationManager` at `/.well-known/jwks.json`.

**Solution**:
```java
@RestController
public class JwksController {
    private final KeyRotationManager keyManager;
    public JwksController(KeyRotationManager km) { this.keyManager = km; }

    @GetMapping("/.well-known/jwks.json")
    public Map<String, Object> getKeys() {
        return keyManager.getPublicJwkSet().toJSONObject();
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Unit Test Token Rotation Compromise Detection</strong></summary>

**Prompt**: Write a JUnit test verifying that submitting an already-revoked refresh token revokes all sibling tokens.

**Solution**:
Create a token family with 2 tokens, mark the first as revoked, call `TokenRotationService.rotateRefreshToken()`, and assert that `SecurityException` is thrown and the second token is also marked revoked.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│               DISTRIBUTED AUTHENTICATION CHEATSHEET                    │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Asymmetric Cryptography: Sign with RS256 Private; verify with JWKS  │
│ 2. Short Access Tokens: 5-15 mins TTL; validates locally on microservice│
│ 3. Refresh Token Rotation (RTR): Invalidate old token on exchange      │
│ 4. Compromise Detection: Reused refresh token = revoke entire family   │
│ 5. Instant Revocation: Store jti in Redis with TTL = remaining life    │
│ 6. Storage: Refresh tokens in HttpOnly, Secure, SameSite cookies       │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Security 6.4 Reference Documentation](https://docs.spring.io/spring-security/reference/)
- [Spring Authorization Server Official Guide](https://docs.spring.io/spring-authorization-server/reference/)
- [RFC 7519: JSON Web Token (JWT)](https://datatracker.ietf.org/doc/html/rfc7519)
- [RFC 6749: The OAuth 2.0 Authorization Framework](https://datatracker.ietf.org/doc/html/rfc6749)
- [OWASP JSON Web Token Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/JSON_Web_Token_for_Java_Cheat_Sheet.html)
