# 11. Refresh Tokens, Token Rotation & Logout in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `07-JWT-resource-server.md`, `08-oauth2-client-and-social-login.md`, RFC 6749, RFC 7009 |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Redis 7.2 |

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
Stateless JSON Web Tokens (JWTs) solve scalability challenges by eliminating server-side session lookups on API calls. However, pure statelessness introduces a critical architectural vulnerability: **revocation impossibility**. Once a JWT access token is signed and issued, it remains valid until its expiration timestamp (`exp`) elapses. If an access token has a long lifespan (e.g., 24 hours) and is exfiltrated by an attacker, the user account remains compromised for the duration of that lifetime, even if the user changes their password or clicks "Log Out".

Conversely, setting an extremely short access token lifespan (e.g., 5 minutes) disrupts user experience by forcing frequent interactive logins.

The standard architectural solution is the **Access Token + Refresh Token lifecycle**, augmented by **Refresh Token Rotation (RTR)**, **Token Reuse Detection**, and **Distributed Blacklisting**.

### Historical Evolution
- **Early OAuth 2.0 (RFC 6749)**: Defined refresh tokens as long-lived credentials used to obtain new access tokens. Refresh tokens were static and reusable until revoked, making refresh token theft devastating.
- **OAuth 2.0 Security Best Current Practice (BCP)**: Mandated **Refresh Token Rotation (RTR)**. Every redemption invalidates the submitted refresh token and issues a new pair. If an already-used refresh token is presented, the server recognizes a token compromise and immediately revokes all descendant tokens in the family.
- **Spring Security 6.x / 7.x**: Provides native support in Spring Authorization Server for RTR, explicit token revocation endpoints (RFC 7009), and flexible logout handler chains.

### Real-World Relevance
Implementing secure refresh token lifecycles with Redis-backed revocation and `HttpOnly` cookie storage is the gold standard for banking, fintech, healthcare, and SaaS applications.

---

## 2. Core Concepts

### Architectural Contracts
- **Access Token**: Short-lived (5–15 min), stateless JWT bearer credential used to authorize API requests.
- **Refresh Token**: Longer-lived (7–30 days), stateful credential stored securely by the client (ideally in an `HttpOnly`, `SameSite=Strict`, `Secure` cookie).
- **Token Family**: A cryptographic lineage of refresh tokens traced back to an initial login event. All rotated tokens belong to the same family ID.
- **Reuse Detection**: An automated security mechanism where presenting an invalidated/consumed refresh token indicates that an attacker (or concurrent stolen session) is attempting replay, triggering immediate invalidation of the entire token family.
- **Token Blacklisting**: An in-memory distributed store (Redis) caching revoked Access Token IDs (`jti`) until their natural expiration time.

### The Mental Model
```
Client (SPA)                            Auth Server / API Gateway                 Redis Store
    │                                              │                                   │
    ├── 1. POST /api/auth/refresh ────────────────►│                                   │
    │      Cookie: refresh_token=RT_1              │                                   │
    │                                              ├── Check if RT_1 was already used? │
    │                                              │   ├─ YES: REUSE DETECTED! ───────►│ Invalidate Family
    │                                              │   │       Return 401 Forbidden    │ Revoke all tokens
    │                                              │   │                               │
    │                                              │   └─ NO: Valid rotation           │
    │                                              │       Invalidate RT_1 ───────────►│ Mark RT_1 USED
    │                                              │       Generate AT_2 & RT_2        │ Save RT_2
    │                                              │                                   │
    │◄── 2. Response 200 OK ───────────────────────┤                                   │
    │      Body: { "access_token": "AT_2" }        │                                   │
    │      Set-Cookie: refresh_token=RT_2; HttpOnly│                                   │
```

### Invariants
1. A refresh token must only be consumed **once**. The instant it is redeemed, it must transition to the revoked/consumed state.
2. If an already-consumed refresh token is presented again, the authentication system must treat the session as compromised and invalidate the entire token family.
3. Access tokens should remain ephemeral (5 to 15 minutes). Refresh tokens should never be stored in browser `localStorage` or `sessionStorage` due to XSS vulnerability.

---

## 3. How It Works Internally

### Token Family State Machine
```
Login ──► Issues [RT_1 (Family: F-1, Status: Active)]
             │
             ├── Client exchanges RT_1 for AT_2 + RT_2
             │     ├─ RT_1 marked USED
             │     └─ RT_2 issued (Family: F-1, Status: Active)
             │
             ├── Scenario A (Normal Flow):
             │     Client exchanges RT_2 ──► RT_2 marked USED, RT_3 issued.
             │
             └── Scenario B (Breach / Token Theft):
                   Attacker attempts to exchange leaked RT_1
                     ├─ System detects RT_1 is already USED
                     ├─ Triggers ALARM: Token Family F-1 compromised!
                     └─ System deletes RT_2, RT_3, and blacklists ATs.
```

---

## 4. Syntax & Usage

### 1. Redis Token Entity Definition
```java
package com.example.security.token;

import org.springframework.data.annotation.Id;
import org.springframework.data.redis.core.RedisHash;
import org.springframework.data.redis.core.TimeToLive;
import org.springframework.data.redis.core.index.Indexed;

import java.time.Instant;

@RedisHash("refresh_tokens")
public class RefreshTokenRecord {

    @Id
    private String tokenHash;

    @Indexed
    private String familyId;

    @Indexed
    private String userId;

    private boolean used;

    private Instant createdAt;

    @TimeToLive
    private Long ttlSeconds;

    // Constructors, Getters, and Setters
    public RefreshTokenRecord(String tokenHash, String familyId, String userId, boolean used, Long ttlSeconds) {
        this.tokenHash = tokenHash;
        this.familyId = familyId;
        this.userId = userId;
        this.used = used;
        this.createdAt = Instant.now();
        this.ttlSeconds = ttlSeconds;
    }

    public String getTokenHash() { return tokenHash; }
    public String getFamilyId() { return familyId; }
    public String getUserId() { return userId; }
    public boolean isUsed() { return used; }
    public void setUsed(boolean used) { this.used = used; }
}
```

---

## 5. Practical Examples

### Example 1: High-Security Refresh Token Service with Rotation & Reuse Detection
A complete production service handling rotation, token family tracking, cryptographic hashing, and reuse breach detection.

```java
package com.example.security.service;

import com.example.security.token.RefreshTokenRecord;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.time.Duration;
import java.util.*;

@Service
public class RefreshTokenService {

    private static final Duration REFRESH_TTL = Duration.ofDays(7);
    private final RedisTemplate<String, Object> redisTemplate;
    private final SecureRandom secureRandom = new SecureRandom();

    public RefreshTokenService(RedisTemplate<String, Object> redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public record TokenPair(String accessToken, String rawRefreshToken) {}

    public String issueInitialRefreshToken(String userId) {
        String familyId = UUID.randomUUID().toString();
        return createAndStoreToken(userId, familyId);
    }

    public String rotateRefreshToken(String rawOldToken) {
        String oldHash = hashToken(rawOldToken);
        String key = "rt:" + oldHash;

        RefreshTokenRecord record = (RefreshTokenRecord) redisTemplate.opsForValue().get(key);

        if (record == null) {
            throw new BadCredentialsException("Invalid or expired refresh token");
        }

        if (record.isUsed()) {
            // COMPROMISE DETECTED: Attacker or Victim is reusing an old token!
            // Invalidate ALL tokens in this family immediately!
            invalidateFamily(record.getFamilyId());
            throw new BadCredentialsException("Security breach detected: Refresh token reuse. Session terminated.");
        }

        // Mark old token as used (retain in Redis for reuse detection window)
        record.setUsed(true);
        redisTemplate.opsForValue().set(key, record, Duration.ofMinutes(10)); // Grace window

        // Issue new token in same family
        return createAndStoreToken(record.getUserId(), record.getFamilyId());
    }

    private String createAndStoreToken(String userId, String familyId) {
        byte[] randomBytes = new byte[32];
        secureRandom.nextBytes(randomBytes);
        String rawToken = Base64.getUrlEncoder().withoutPadding().encodeToString(randomBytes);
        String tokenHash = hashToken(rawToken);

        RefreshTokenRecord newRecord = new RefreshTokenRecord(
            tokenHash, familyId, userId, false, REFRESH_TTL.toSeconds()
        );

        redisTemplate.opsForValue().set("rt:" + tokenHash, newRecord, REFRESH_TTL);
        redisTemplate.opsForSet().add("family:" + familyId, tokenHash);
        redisTemplate.expire("family:" + familyId, REFRESH_TTL);

        return rawToken;
    }

    public void invalidateFamily(String familyId) {
        Set<Object> tokenHashes = redisTemplate.opsForSet().members("family:" + familyId);
        if (tokenHashes != null) {
            for (Object hash : tokenHashes) {
                redisTemplate.delete("rt:" + hash);
            }
        }
        redisTemplate.delete("family:" + familyId);
    }

    private String hashToken(String rawToken) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(rawToken.getBytes(StandardCharsets.UTF_8));
            return Base64.getUrlEncoder().encodeToString(hash);
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA-256 algorithm missing", e);
        }
    }
}
```

### Example 2: Secure Cookie Injection and Refresh Controller
Managing the refresh handshake via secure, HTTP-only cookies to eliminate XSS vulnerability.

```java
package com.example.security.controller;

import com.example.security.service.JwtTokenService;
import com.example.security.service.RefreshTokenService;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpHeaders;
import org.springframework.http.ResponseCookie;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Duration;
import java.util.Map;

@RestController
@RequestMapping("/api/auth")
public class AuthTokenController {

    private final RefreshTokenService refreshTokenService;
    private final JwtTokenService jwtTokenService;

    public AuthTokenController(RefreshTokenService refreshTokenService, JwtTokenService jwtTokenService) {
        this.refreshTokenService = refreshTokenService;
        this.jwtTokenService = jwtTokenService;
    }

    @PostMapping("/refresh")
    public ResponseEntity<?> refreshToken(
            @CookieValue(name = "refresh_token", required = false) String oldRefreshToken,
            HttpServletResponse response) {

        if (oldRefreshToken == null || oldRefreshToken.isBlank()) {
            return ResponseEntity.status(401).body(Map.of("error", "Refresh token cookie missing"));
        }

        String newRefreshToken = refreshTokenService.rotateRefreshToken(oldRefreshToken);
        String newAccessToken = jwtTokenService.generateAccessToken("user-id", "ROLE_USER");

        // Set secure HTTP-only cookie
        ResponseCookie cookie = ResponseCookie.from("refresh_token", newRefreshToken)
            .httpOnly(true)
            .secure(true) // HTTPS only
            .sameSite("Strict")
            .path("/api/auth")
            .maxAge(Duration.ofDays(7))
            .build();

        response.addHeader(HttpHeaders.SET_COOKIE, cookie.toString());

        return ResponseEntity.ok(Map.of(
            "access_token", newAccessToken,
            "token_type", "Bearer",
            "expires_in", 900
        ));
    }
}
```

### Example 3: Distributed Redis JWT Blacklist Filter for Immediate Logout
When a user clicks "Log Out", invalidate the refresh token and blacklist the access token's `jti` in Redis until its expiration timestamp.

```java
package com.example.security.logout;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class JwtBlacklistValidationFilter extends OncePerRequestFilter {

    private final StringRedisTemplate redisTemplate;

    public JwtBlacklistValidationFilter(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        Authentication authentication = SecurityContextHolder.getContext().getAuthentication();

        if (authentication instanceof JwtAuthenticationToken jwtAuth) {
            Jwt jwt = jwtAuth.getToken();
            String jti = jwt.getId();

            if (jti != null && Boolean.TRUE.equals(redisTemplate.hasKey("blacklist:jwt:" + jti))) {
                SecurityContextHolder.clearContext();
                response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
                response.setContentType("application/json");
                response.getWriter().write("{\"error\": \"Access token has been revoked via logout\"}");
                return;
            }
        }

        filterChain.doFilter(request, response);
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Returning refresh tokens in the JSON response body and allowing single-page apps to store them in `window.localStorage`.

```javascript
// CRITICAL VULNERABILITY (Frontend):
// Storing refresh tokens in localStorage exposes them to any Cross-Site Scripting (XSS) attack!
localStorage.setItem('refresh_token', response.data.refresh_token);
```

**Production Fix**:
Enforce `Set-Cookie` with `HttpOnly`, `Secure`, and `SameSite=Strict`. Restrict the cookie `Path` exclusively to `/api/auth/refresh` so normal API requests never transmit the refresh token.

```java
// SECURE: Browser JavaScript cannot read HttpOnly cookies, neutralizing XSS exfiltration
ResponseCookie refreshCookie = ResponseCookie.from("refresh_token", newRefreshToken)
    .httpOnly(true)
    .secure(true)
    .sameSite("Strict")
    .path("/api/auth/refresh") // Scoped strictly to the refresh endpoint
    .maxAge(Duration.ofDays(7))
    .build();
```

### Example 5: Clean Logout Handler with Cookie Clearing
Integrating a custom Spring Security `LogoutHandler` that cleans Redis state and clears browser cookies.

```java
package com.example.security.logout;

import com.example.security.service.RefreshTokenService;
import jakarta.servlet.http.Cookie;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpHeaders;
import org.springframework.http.ResponseCookie;
import org.springframework.security.core.Authentication;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.security.web.authentication.logout.LogoutHandler;
import org.springframework.stereotype.Component;

import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;

@Component
public class ProductionJwtLogoutHandler implements LogoutHandler {

    private final RefreshTokenService refreshTokenService;
    private final StringRedisTemplate redisTemplate;

    public ProductionJwtLogoutHandler(RefreshTokenService refreshTokenService, StringRedisTemplate redisTemplate) {
        this.refreshTokenService = refreshTokenService;
        this.redisTemplate = redisTemplate;
    }

    @Override
    public void logout(HttpServletRequest request, HttpServletResponse response, Authentication authentication) {
        // 1. Blacklist Access Token (if present)
        if (authentication instanceof JwtAuthenticationToken jwtAuth) {
            Jwt jwt = jwtAuth.getToken();
            String jti = jwt.getId();
            Instant exp = jwt.getExpiresAt();
            if (jti != null && exp != null) {
                long ttlSeconds = Duration.between(Instant.now(), exp).getSeconds();
                if (ttlSeconds > 0) {
                    redisTemplate.opsForValue().set("blacklist:jwt:" + jti, "revoked", Duration.ofSeconds(ttlSeconds));
                }
            }
        }

        // 2. Invalidate Refresh Token from Cookie
        if (request.getCookies() != null) {
            Arrays.stream(request.getCookies())
                .filter(cookie -> "refresh_token".equals(cookie.getName()))
                .findFirst()
                .ifPresent(cookie -> {
                    // Revoke token record
                    // ...
                });
        }

        // 3. Delete Cookie on Browser
        ResponseCookie clearCookie = ResponseCookie.from("refresh_token", "")
            .httpOnly(true)
            .secure(true)
            .sameSite("Strict")
            .path("/api/auth/refresh")
            .maxAge(0) // Expire immediately
            .build();

        response.addHeader(HttpHeaders.SET_COOKIE, clearCookie.toString());
    }
}
```

---

## 6. Best Practices

1. **Always Rotate Refresh Tokens (RTR)**: Never allow multiple uses of a single refresh token. Invalidate upon use and issue a new token.
2. **Store Refresh Tokens in `HttpOnly` Cookies**: Protect refresh tokens from XSS by enforcing `HttpOnly`, `Secure`, and `SameSite=Strict`.
3. **Scope Cookie Paths**: Restrict the refresh token cookie path to `/api/auth/refresh` so it is not sent with every standard API request.
4. **Hash Refresh Tokens in the Database**: Store SHA-256 hashes of refresh tokens in Redis/PostgreSQL; never store plaintext tokens in case of database leaks.
5. **Set Short Access Token Lifetimes**: Configure access tokens to expire in 5–15 minutes, limiting the window of risk if an access token is intercepted.
6. **Time-To-Live on Blacklist Entries**: When blacklisting JWT IDs (`jti`), set the Redis TTL equal to the remaining lifespan of the token so keys expire automatically without manual cleanup.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Refresh tokens in `localStorage` | Malicious scripts injected via XSS can exfiltrate the token and impersonate the user indefinitely. | Use `HttpOnly` cookies with `SameSite=Strict`. |
| Omitting token reuse detection | If an attacker steals a refresh token, both victim and attacker can refresh concurrently without detection. | Implement token families; any reuse of a consumed token invalidates the entire family. |
| Perpetual Redis blacklist growth | Storing blacklisted tokens without a TTL consumes infinite memory over time. | Set key TTL equal to `token.getExpiresAt() - Instant.now()`. |
| Lack of concurrency grace period | Network retries or rapid parallel frontend calls cause false-positive reuse detections, logging out valid users. | Allow a 5–10 second grace window where identical refresh requests return the new token. |
| Stateless logout illusion | Deleting the token only on the frontend leaves the token valid on the server until expiration. | Blacklist the `jti` in Redis or shorten access token lifetimes to under 5 minutes. |
| Forgetting to hash tokens | If Redis or database logs are dumped, all active refresh tokens are compromised. | Hash tokens using SHA-256 before saving to the datastore. |

---

## 8. Performance & Security Notes

- **Redis Memory Sizing for Blacklists**: If an API processes 100,000 active users and each user logs out once per day with a 15-minute access token lifespan, the blacklist holds at most `(100,000 / 24) * 0.25 = ~1,040` active keys at any moment. At ~100 bytes per key, memory usage is well under 1 megabyte.
- **Race Conditions in Single Page Apps**: When multiple components trigger API calls simultaneously with an expired access token, multiple parallel `/api/auth/refresh` requests may fire. Implement a frontend promise-based mutex queue so only one refresh call executes while others await the result.
- **Virtual Threads Integration**: Redis lookups during filter chain execution (`JwtBlacklistValidationFilter`) perform network socket I/O. On Java 21+ with Virtual Threads, this unmounts the carrier thread cleanly with near-zero latency impact.

---

## 9. Comparison / When to Use What

| Strategy | Token Storage | Revocation Speed | XSS Resilient? | Implementation Complexity |
|---|---|---|---|---|
| **Pure Stateless JWT** | Memory / LocalStorage | None (Wait for `exp`) | No | Very Low |
| **JWT + Redis Blacklist** | In-Memory + Redis TTL | Instantaneous | Yes | Moderate |
| **RTR + HttpOnly Cookies** | Secure Cookie + Redis Store | Instantaneous (Family Invalidation)| Yes (Immune) | High (Recommended) |
| **Opaque Session Tokens** | Redis / Database | Instantaneous | Yes | Moderate (Higher I/O per request) |

---

## 10. Interview Questions

<details>
<summary>1. What is Refresh Token Rotation (RTR) and why is it essential?</summary>

**Answer:**
RTR is a security mechanism where an authorization server issues a new refresh token every time the client exchanges an existing refresh token for a new access token, immediately invalidating the old refresh token. It limits the window of vulnerability if a token is intercepted and allows the server to detect token theft through reuse detection.
</details>

<details>
<summary>2. How does Token Reuse Detection operate when a token is stolen?</summary>

**Answer:**
When an attacker and a legitimate user possess the same refresh token, whichever party arrives second will present a token that has already been marked as `USED`. The server identifies this event as unauthorized reuse, assumes a breach has occurred, and terminates all active sessions associated with that token family (`familyId`), forcing both parties to re-authenticate.
</details>

<details>
<summary>3. Why is storing JWTs in browser `localStorage` considered an anti-pattern?</summary>

**Answer:**
`localStorage` is accessible to any JavaScript code running in the same origin. Any Cross-Site Scripting (XSS) vulnerability (such as a compromised npm dependency or unescaped user input) enables attackers to extract tokens instantly. Storing refresh tokens in `HttpOnly` cookies ensures JavaScript cannot access the token bytes.
</details>

<details>
<summary>4. How does an access token blacklist work with Redis?</summary>

**Answer:**
When a user logs out, the server extracts the JWT ID (`jti`) and expiration timestamp (`exp`). It saves the `jti` into Redis with a Time-To-Live (TTL) equal to the remaining seconds until `exp`. A servlet filter checks incoming JWTs against this Redis key. If the `jti` exists in Redis, the request is rejected as revoked. Once the token naturally expires, Redis automatically purges the key.
</details>

<details>
<summary>5. How do you handle frontend race conditions during token refresh?</summary>

**Answer:**
If an access token expires while three parallel API requests are in flight, a naive frontend might fire three concurrent refresh requests, triggering false-positive reuse detection. Frontends solve this by using an Axios/Fetch interceptor that pauses outgoing requests, checks if a refresh is already in progress via a shared Promise mutex, awaits that single refresh, and replays all queued requests with the new access token.
</details>

<details>
<summary>6. What is the purpose of the `jti` (JWT ID) claim in RFC 7519?</summary>

**Answer:**
The `jti` claim provides a unique identifier for a JSON Web Token. It is critical for preventing replay attacks and enabling token blacklisting/revocation without tracking the full token string.
</details>

<details>
<summary>7. What is the difference between `SameSite=Lax` and `SameSite=Strict` for refresh cookies?</summary>

**Answer:**
`SameSite=Strict` guarantees the cookie is never sent in cross-site requests (e.g., following a link from an external site or email).
`SameSite=Lax` permits the cookie on top-level cross-site navigations using safe methods (`GET`). For sensitive refresh token endpoints (`POST /api/auth/refresh`), `SameSite=Strict` provides the strongest CSRF defense.
</details>

<details>
<summary>8. Can you implement sliding sessions with JWTs?</summary>

**Answer:**
Yes. While individual access tokens should not have sliding expiration (they should have short, fixed lifetimes), the refresh token lifespan can be extended on each successful rotation (sliding window), provided the overall idle timeout has not been exceeded.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Cookie Generation Helper (Easy)</summary>

**Task**: Write a utility method that generates a secure Spring `ResponseCookie` for a refresh token with `HttpOnly`, `Secure`, `SameSite=Strict`, path `/api/auth/refresh`, and a 7-day duration.

**Solution**:
```java
public ResponseCookie createRefreshTokenCookie(String tokenValue) {
    return ResponseCookie.from("refresh_token", tokenValue)
        .httpOnly(true)
        .secure(true)
        .sameSite("Strict")
        .path("/api/auth/refresh")
        .maxAge(Duration.ofDays(7))
        .build();
}
```
</details>

<details>
<summary>Exercise 2: Blacklist Entry Writer (Easy)</summary>

**Task**: Write a method that takes a decoded `Jwt` object and writes its `jti` to Redis with a TTL matching its expiration time.

**Solution**:
```java
public void blacklistToken(Jwt jwt, StringRedisTemplate redis) {
    String jti = jwt.getId();
    Instant exp = jwt.getExpiresAt();
    if (jti != null && exp != null) {
        long secondsLeft = Duration.between(Instant.now(), exp).getSeconds();
        if (secondsLeft > 0) {
            redis.opsForValue().set("blacklist:" + jti, "revoked", Duration.ofSeconds(secondsLeft));
        }
    }
}
```
</details>

<details>
<summary>Exercise 3: Token Family Revocation (Medium)</summary>

**Task**: Implement a method that receives a `familyId`, queries all member token hashes from a Redis Set (`family:{id}`), deletes all individual token records (`rt:{hash}`), and removes the family set.

**Solution**:
```java
public void revokeFamily(String familyId, RedisTemplate<String, Object> redis) {
    String familyKey = "family:" + familyId;
    Set<Object> tokens = redis.opsForSet().members(familyKey);
    if (tokens != null && !tokens.isEmpty()) {
        List<String> keysToDelete = tokens.stream()
            .map(hash -> "rt:" + hash)
            .toList();
        redis.delete(keysToDelete);
    }
    redis.delete(familyKey);
}
```
</details>

<details>
<summary>Exercise 4: Grace Period for Rapid Concurrent Refreshes (Medium)</summary>

**Task**: Modify the refresh token logic so that if a token was rotated within the last 5 seconds, it returns the newly issued token instead of triggering a reuse breach error.

**Solution**:
```java
public String handleConcurrentRefresh(RefreshTokenRecord record, String oldHash, RedisTemplate<String, Object> redis) {
    if (record.isUsed()) {
        String cachedNewToken = (String) redis.opsForValue().get("grace:" + oldHash);
        if (cachedNewToken != null) {
            return cachedNewToken; // Return recently issued token safely
        }
        // Grace period expired: real reuse attack!
        throw new BadCredentialsException("Token reuse detected!");
    }
    // Proceed with normal rotation and cache new token in grace key for 5 seconds
    String newToken = generateNewToken();
    redis.opsForValue().set("grace:" + oldHash, newToken, Duration.ofSeconds(5));
    return newToken;
}
```
</details>

<details>
<summary>Exercise 5: Spring Security Logout Configuration (Hard)</summary>

**Task**: Configure a `SecurityFilterChain` that attaches a custom `LogoutHandler` and `LogoutSuccessHandler` to `/api/auth/logout`, clearing `SecurityContextHolder`, cookies, and Redis keys, and responding with HTTP 204 No Content.

**Solution**:
```java
@Bean
public SecurityFilterChain securityFilterChain(HttpSecurity http, 
                                               ProductionJwtLogoutHandler logoutHandler) throws Exception {
    return http
        .authorizeHttpRequests(auth -> auth
            .requestMatchers("/api/auth/login", "/api/auth/refresh").permitAll()
            .anyRequest().authenticated()
        )
        .logout(logout -> logout
            .logoutUrl("/api/auth/logout")
            .addLogoutHandler(logoutHandler)
            .logoutSuccessHandler((req, res, auth) -> res.setStatus(HttpServletResponse.SC_NO_CONTENT))
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Test for Refresh Token Rotation (Hard)</summary>

**Task**: Write an integration test using MockMvc verifying that calling `/api/auth/refresh` with an expired or already used refresh token returns HTTP 401 Unauthorized.

**Solution**:
```java
@WebMvcTest(controllers = AuthTokenController.class)
@Import(SecurityConfig.class)
class AuthTokenControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private RefreshTokenService refreshTokenService;

    @MockBean
    private JwtTokenService jwtTokenService;

    @Test
    void shouldReturn401WhenRefreshTokenIsReused() throws Exception {
        when(refreshTokenService.rotateRefreshToken("compromised-token"))
            .thenThrow(new BadCredentialsException("Security breach detected"));

        mockMvc.perform(post("/api/auth/refresh")
                .cookie(new Cookie("refresh_token", "compromised-token")))
            .andExpect(status().isUnauthorized());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Never Keep Access Tokens Long**: Set access tokens to expire in 5–15 minutes.
- **Enforce Refresh Token Rotation (RTR)**: Invalidate refresh tokens immediately upon redemption and issue a new pair.
- **Track Token Families**: Any attempt to reuse a consumed refresh token indicates theft and must immediately invalidate the entire family.
- **Store in `HttpOnly` Cookies**: Keep refresh tokens out of `localStorage` to defeat XSS credential extraction.
- **Blacklist via Redis TTL**: Use the JWT `jti` claim to blacklist revoked access tokens until their natural expiration timestamp.

---

## 13. Further Reading
- [OAuth 2.0 Security Best Current Practice (BCP)](https://datatracker.ietf.org/doc/html/draft-ietf-oauth-security-topics)
- [RFC 7009: OAuth 2.0 Token Revocation](https://datatracker.ietf.org/doc/html/rfc7009)
- [OWASP Session Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)
- [RFC 6265bis: Cookies - HTTP State Management Mechanism](https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis)
- [Spring Security Reference: Logout Handling](https://docs.spring.io/spring-security/reference/servlet/authentication/logout.html)
