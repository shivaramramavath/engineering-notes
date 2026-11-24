# 13. API Key & HMAC Request Signing Authentication in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, Cryptographic Hashing Basics |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1 |

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
In machine-to-machine (M2M) integrations, webhook ingestion, and public developer APIs (such as Stripe, AWS, or Twilio), interactive user logins with redirect flows (OAuth2 Authorization Code) are impossible. Services need a reliable, high-performance way to verify the identity and authenticity of incoming requests.

A raw **API Key** (`X-API-KEY: sk_live_...`) provides identification, but it has severe limitations:
- If transmitted over an unencrypted channel or intercepted by a malicious proxy, an API key can be replayed indefinitely.
- An API key alone does **not** protect message integrity; a Man-In-The-Middle (MITM) can tamper with the request payload (e.g., altering a payment amount from \$10 to \$10,000) while presenting a valid API key.

**HMAC (Hash-based Message Authentication Code) Request Signing** solves this by pairing an API Key with a shared secret to cryptographically sign the HTTP method, URI, timestamp, and request body. The server verifies both caller authenticity and payload integrity.

### Historical Evolution
- **Early Web APIs (2000s)**: API keys were passed as cleartext query parameters (`?api_key=xyz`), frequently leaking into server access logs and browser histories.
- **AWS Signature Version 4 (SigV4) & Webhook Standards (2010s)**: AWS, GitHub, and Stripe standardized HMAC-SHA256 request signing with timestamp nonces to eliminate payload tampering and replay attacks.
- **Modern Spring Boot 3.x/4.x**: Implemented via custom `OncePerRequestFilter` chains, `ContentCachingRequestWrapper` for safe repeated body reads, and constant-time byte comparisons (`MessageDigest.isEqual`).

### Real-World Relevance
Implementing hardened API Key management and HMAC signature verification is critical for any team building B2B integrations, financial webhooks, or high-throughput external developer APIs.

---

## 2. Core Concepts

### Architectural Contracts
- **API Key**: A public identifier (e.g., `pk_live_...`) identifying the calling client.
- **Shared Secret**: A high-entropy private key (e.g., `sk_live_...`) shared out-of-band between client and server, used solely for cryptographic signing. Never transmitted in HTTP requests!
- **HMAC Signature**: A cryptographic hash computed using HMAC-SHA256 over a canonical representation of the request (`HTTP Method + Path + Timestamp + Nonce + Body Hash`).
- **Timestamp / Nonce**: Prevents replay attacks by establishing a strict validity window (e.g., $\pm 5$ minutes) and tracking one-time nonces in Redis.
- **`ContentCachingRequestWrapper`**: A Spring Web wrapper that caches the servlet input stream so the HMAC filter can read the body for hashing without exhausting the stream before MVC controllers execute.

### The Mental Model
```
Client Application                                        Spring Boot API
       │                                                         │
       ├── 1. Constructs Canonical String:                       │
       │      "POST\n/api/v1/payments\n1700000000\n{"amt":100}"  │
       ├── 2. Computes HMAC-SHA256 using SecretKey               │
       │      Signature = Base64(HMAC(CanonicalString))          │
       │                                                         │
       ├── 3. Sends Request:                                     │
       │      POST /api/v1/payments                              │
       │      Headers:                                           │
       │        X-API-Key: pk_live_abc123                        │
       │        X-Signature-Timestamp: 1700000000                │
       │        X-Signature: <ComputedSignature>                 │
       │      Body: {"amt":100}                                  ▼
       │                                              [ HmacAuthenticationFilter ]
       │                                              1. Validates timestamp (< 5m)
       │                                              2. Fetches Secret for pk_live_abc123
       │                                              3. Recomputes HMAC over body
       │                                              4. Compares signatures (Constant-Time)
       │                                                         │
       │◄── 4. Response 200 OK ──────────────────────────────────┤ Authenticated!
```

### Invariants
1. The shared secret must **never** be transmitted across the wire in the HTTP request.
2. Signatures must be compared using **constant-time comparison** (`MessageDigest.isEqual`) to defeat timing attacks.
3. Every signed request must include a timestamp; requests with timestamps drifting beyond a strict window (e.g., 300 seconds) must be rejected immediately.

---

## 3. How It Works Internally

### The Caching Request Challenge
In standard Java Servlet containers (`HttpServletRequest`), the input stream (`getInputStream()`) can only be read **once**. If an authentication filter reads the stream to compute the body hash, downstream Spring MVC controllers receive an empty stream and fail with `HttpMessageNotReadableException`.

To solve this, Spring provides `ContentCachingRequestWrapper`:
1. `HmacAuthenticationFilter` wraps `request` in `ContentCachingRequestWrapper`.
2. Reads the body bytes to calculate `SHA-256(body)`.
3. Passes the wrapped request down the filter chain.
4. When Jackson deserializes the body in the controller, it reads from the internal memory buffer of the wrapper.

---

## 4. Syntax & Usage

### 1. Custom Authentication Token
```java
package com.example.security.apikey;

import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.core.GrantedAuthority;

import java.util.Collection;

public class ApiKeyAuthenticationToken extends AbstractAuthenticationToken {

    private final String apiKey;
    private final Object principal;

    public ApiKeyAuthenticationToken(String apiKey) {
        super(null);
        this.apiKey = apiKey;
        this.principal = apiKey;
        setAuthenticated(false);
    }

    public ApiKeyAuthenticationToken(String apiKey, Object principal, Collection<? extends GrantedAuthority> authorities) {
        super(authorities);
        this.apiKey = apiKey;
        this.principal = principal;
        setAuthenticated(true);
    }

    @Override
    public Object getCredentials() { return null; }

    @Override
    public Object getPrincipal() { return this.principal; }

    public String getApiKey() { return apiKey; }
}
```

---

## 5. Practical Examples

### Example 1: Production HMAC-SHA256 Signature Filter
A complete filter validating timestamps, computing HMAC signatures, preventing timing attacks, and setting `SecurityContextHolder`.

```java
package com.example.security.hmac;

import com.example.security.apikey.ApiKeyAuthenticationToken;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;
import org.springframework.web.util.ContentCachingRequestWrapper;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Instant;
import java.util.Base64;
import java.util.List;
import java.util.Map;

@Component
public class HmacAuthenticationFilter extends OncePerRequestFilter {

    private static final String HEADER_API_KEY = "X-API-Key";
    private static final String HEADER_TIMESTAMP = "X-Signature-Timestamp";
    private static final String HEADER_SIGNATURE = "X-Signature";
    private static final long MAX_ALLOWED_DRIFT_SECONDS = 300L; // 5 minutes

    // In production, query from a database or secure secrets vault
    private final Map<String, String> keySecretStore = Map.of(
        "client_corp_123", "secret_key_super_confidential_456"
    );

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        String apiKey = request.getHeader(HEADER_API_KEY);
        String timestampStr = request.getHeader(HEADER_TIMESTAMP);
        String clientSignature = request.getHeader(HEADER_SIGNATURE);

        // If headers are missing, proceed down chain (may be a public endpoint or authenticated via JWT)
        if (apiKey == null || timestampStr == null || clientSignature == null) {
            filterChain.doFilter(request, response);
            return;
        }

        // 1. Replay Prevention: Timestamp validation
        long timestamp;
        try {
            timestamp = Long.parseLong(timestampStr);
        } catch (NumberFormatException e) {
            sendError(response, "Invalid timestamp format");
            return;
        }

        long currentEpoch = Instant.now().getEpochSecond();
        if (Math.abs(currentEpoch - timestamp) > MAX_ALLOWED_DRIFT_SECONDS) {
            sendError(response, "Request timestamp expired or skewed beyond tolerance");
            return;
        }

        // 2. Secret Resolution
        String secret = keySecretStore.get(apiKey);
        if (secret == null) {
            sendError(response, "Unknown API Key");
            return;
        }

        // 3. Wrap Request for Body Caching
        ContentCachingRequestWrapper wrappedRequest = new ContentCachingRequestWrapper(request);

        // Cache request body by reading it
        byte[] body = wrappedRequest.getInputStream().readAllBytes();

        // 4. Canonical String Construction
        String canonicalPayload = String.join("\n",
            request.getMethod(),
            request.getRequestURI(),
            timestampStr,
            new String(body, StandardCharsets.UTF_8)
        );

        // 5. Compute HMAC
        String expectedSignature = calculateHmacSha256(canonicalPayload, secret);

        // 6. Constant-Time Verification to Defeat Timing Attacks
        if (!MessageDigest.isEqual(
                expectedSignature.getBytes(StandardCharsets.UTF_8),
                clientSignature.getBytes(StandardCharsets.UTF_8))) {
            sendError(response, "HMAC signature mismatch");
            return;
        }

        // 7. Establish Authentication Context
        ApiKeyAuthenticationToken auth = new ApiKeyAuthenticationToken(
            apiKey,
            apiKey,
            List.of(new SimpleGrantedAuthority("ROLE_API_CLIENT"))
        );
        SecurityContextHolder.getContext().setAuthentication(auth);

        filterChain.doFilter(wrappedRequest, response);
    }

    private String calculateHmacSha256(String data, String secret) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            SecretKeySpec secretKey = new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256");
            mac.init(secretKey);
            byte[] hmacBytes = mac.doFinal(data.getBytes(StandardCharsets.UTF_8));
            return Base64.getEncoder().encodeToString(hmacBytes);
        } catch (Exception e) {
            throw new IllegalStateException("Failed to calculate HMAC signature", e);
        }
    }

    private void sendError(HttpServletResponse response, String message) throws IOException {
        response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
        response.setContentType("application/json");
        response.getWriter().write("{\"error\": \"" + message + "\"}");
    }
}
```

### Example 2: Security Filter Chain Wiring
```java
package com.example.security.config;

import com.example.security.hmac.HmacAuthenticationFilter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;

@Configuration
public class ApiKeySecurityConfig {

    private final HmacAuthenticationFilter hmacFilter;

    public ApiKeySecurityConfig(HmacAuthenticationFilter hmacFilter) {
        this.hmacFilter = hmacFilter;
    }

    @Bean
    public SecurityFilterChain apiSecurityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable())
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/v1/public/**").permitAll()
                .requestMatchers("/api/v1/**").hasRole("API_CLIENT")
                .anyRequest().authenticated()
            )
            .addFilterBefore(hmacFilter, UsernamePasswordAuthenticationFilter.class)
            .build();
    }
}
```

### Example 3: Client Request Signing Utility (Java `RestClient`)
How external client applications build the canonical string, compute the signature, and attach headers using Spring's `RestClient`.

```java
package com.example.client;

import org.springframework.web.client.RestClient;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Base64;

public class HmacSigningRestClient {

    private final String apiKey;
    private final String secretKey;
    private final RestClient restClient;

    public HmacSigningRestClient(String apiKey, String secretKey, String baseUrl) {
        this.apiKey = apiKey;
        this.secretKey = secretKey;
        this.restClient = RestClient.builder().baseUrl(baseUrl).build();
    }

    public String sendPayment(String payload) {
        String path = "/api/v1/payments";
        String timestamp = String.valueOf(Instant.now().getEpochSecond());

        String canonicalString = String.join("\n", "POST", path, timestamp, payload);
        String signature = sign(canonicalString, secretKey);

        return restClient.post()
            .uri(path)
            .header("X-API-Key", apiKey)
            .header("X-Signature-Timestamp", timestamp)
            .header("X-Signature", signature)
            .header("Content-Type", "application/json")
            .body(payload)
            .retrieve()
            .body(String.class);
    }

    private String sign(String data, String secret) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            return Base64.getEncoder().encodeToString(mac.doFinal(data.getBytes(StandardCharsets.UTF_8)));
        } catch (Exception e) {
            throw new RuntimeException("Error signing request", e);
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using standard Java string equality (`expected.equals(actual)`) to compare signatures, leaving the system vulnerable to **timing attacks**.

```java
// CRITICAL VULNERABILITY: Short-circuiting string comparison leaks timing information byte-by-byte!
if (expectedSignature.equals(clientSignature)) {
    // Attacker can measure response latency down to microseconds to guess the signature!
}
```

**Production Fix**:
Always use `MessageDigest.isEqual(...)`, which executes in constant time regardless of where bytes differ.

```java
// SECURE: Constant-time comparison defeats side-channel timing analysis
if (MessageDigest.isEqual(
        expectedSignature.getBytes(StandardCharsets.UTF_8), 
        clientSignature.getBytes(StandardCharsets.UTF_8))) {
    // Signature verified safely
}
```

### Example 5: Distributed Nonce Deduplication with Redis
Preventing replay attacks within the 5-minute validity window by enforcing one-time nonces via Redis `SETNX`.

```java
package com.example.security.hmac;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class ReplayDefenseService {

    private final StringRedisTemplate redisTemplate;

    public ReplayDefenseService(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public boolean validateAndRecordNonce(String nonce, long timestamp) {
        String key = "nonce:" + nonce;
        // Atomically set key if absent with 10-minute TTL
        Boolean isNew = redisTemplate.opsForValue().setIfAbsent(key, String.valueOf(timestamp), Duration.ofMinutes(10));
        return Boolean.TRUE.equals(isNew);
    }
}
```

---

## 6. Best Practices

1. **Enforce Constant-Time Comparison**: Always use `MessageDigest.isEqual(...)` for signature comparisons.
2. **Never Transmit the Secret Key**: The secret key is only used to compute hashes locally on the client and server; it must never appear in HTTP headers or URLs.
3. **Include a Strict Timestamp Window**: Reject requests older than $\pm 5$ minutes to limit the vulnerability window for intercepted requests.
4. **Sign the Entire Canonical Request**: Include HTTP method, URI, timestamp, nonce, and request body hash in the signature payload to prevent parameter tampering.
5. **Hash Stored Secrets in the Database**: Store hashed representations of API keys (using SHA-256 or BCrypt) to mitigate internal database leak risks.
6. **Rate-Limit by API Key**: Place rate limiters (e.g., Bucket4j) directly behind the API key filter to guard against denial-of-service brute forcing.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| String `.equals()` comparison | Vulnerable to timing attacks that leak signature characters. | Use `MessageDigest.isEqual(...)`. |
| Reading request body without wrapping | Controller fails with `IOException: Stream closed` because stream was consumed by filter. | Wrap `HttpServletRequest` in `ContentCachingRequestWrapper`. |
| Omitting timestamp validation | Attacker intercepts a valid signed request and replays it indefinitely. | Enforce `Math.abs(now - timestamp) < 300` seconds. |
| Inconsistent canonical formatting | Differences in line endings (`\r\n` vs `\n`) or whitespace break signature verification. | Strictly standardize on `\n` delimiters and strip trailing whitespace. |
| Query parameter omission | Attacker alters `?amount=100` to `?amount=1000` because query strings were omitted from the canonical hash. | Include `request.getQueryString()` in the canonical signature string. |
| Storing cleartext API secrets | Database backup leaks compromise all partner integrations. | Encrypt secrets using AES-GCM or store one-way SHA-256 hashes. |

---

## 8. Performance & Security Notes

- **HMAC Computational Efficiency**: Unlike RSA asymmetric decryption (which takes ~1ms per verification), HMAC-SHA256 is symmetric and requires less than 5 microseconds of CPU time. This makes HMAC ideal for high-throughput APIs processing 50,000+ RPS.
- **Memory Consumption with Large Payloads**: `ContentCachingRequestWrapper` buffers the entire request payload into JVM heap memory. For file upload endpoints (e.g., 50MB files), buffer bloat can trigger OutOfMemory errors. Exclude streaming/binary endpoints from HMAC body hashing; sign chunk headers instead.
- **Virtual Threads Compatibility**: Cryptographic hashing operations are CPU-bound and run without blocking carrier threads in Java 21+.

---

## 9. Comparison / When to Use What

| Authentication Type | Secret Shared? | Protects Body Integrity? | Replay Protected? | Primary Use Case |
|---|---|---|---|---|
| **Simple API Key** | Yes (Passed in Header) | No | No | Low-risk read-only developer APIs |
| **HMAC Request Signing** | Yes (Never Transmitted) | **Yes** | **Yes (with Timestamp/Nonce)** | High-security financial webhooks, B2B APIs |
| **OAuth2 Client Credentials**| Yes (For Token Exchange) | No (Without mTLS) | Partial (Token TTL) | Microservice-to-microservice mesh |
| **Mutual TLS (mTLS)** | No (Public Key Certs) | Yes (Transport Layer) | Yes | Banking, zero-trust infrastructure |

---

## 10. Interview Questions

<details>
<summary>1. Why is HMAC request signing superior to passing a simple API key in an HTTP header?</summary>

**Answer:**
A simple API key identifies the caller, but it does not protect payload integrity (an attacker can tamper with data in transit) and can be replayed indefinitely if intercepted. HMAC request signing hashes the payload, method, and timestamp together using a private shared secret. The secret is never sent over the network, ensuring message integrity and tamper-proofing.
</details>

<details>
<summary>2. What is a timing attack, and how does `MessageDigest.isEqual` mitigate it?</summary>

**Answer:**
Standard string comparison (`String.equals()`) terminates early upon finding the first mismatching character. An attacker can measure microscopic variations in server response time to determine how many leading characters of a signature were correct, systematically guessing the signature. `MessageDigest.isEqual` executes in constant time by comparing all bytes regardless of mismatches, eliminating the timing side-channel.
</details>

<details>
<summary>3. Why does reading `request.getInputStream()` inside a filter break subsequent controller execution?</summary>

**Answer:**
Servlet specification streams are single-use forward-only streams. Once read by an authentication filter, the stream reaches EOF. When Spring MVC attempts to deserialize the body into a DTO via Jackson, it encounters an empty stream and throws `HttpMessageNotReadableException`. Wrapping the request in `ContentCachingRequestWrapper` buffers the bytes in memory, allowing repeated reads.
</details>

<details>
<summary>4. How does an HMAC architecture defend against replay attacks?</summary>

**Answer:**
By requiring a timestamp header (e.g., `X-Signature-Timestamp`) and optionally a random unique nonce (`X-Nonce`) included within the signed canonical payload. The server rejects any request whose timestamp differs from current server time by more than a set threshold (e.g., 5 minutes) and uses a fast cache (Redis) to reject any reused nonces within that window.
</details>

<details>
<summary>5. How does Stripe or GitHub webhook verification work?</summary>

**Answer:**
Stripe sends webhooks with a header like `Stripe-Signature: t=1492774577,v1=5257a863e...`. The client server takes the timestamp `t`, appends the raw JSON body, and computes an HMAC-SHA256 signature using their webhook signing secret. If the computed hash matches `v1` and the timestamp is within 5 minutes, the webhook is considered authentic.
</details>

<details>
<summary>6. Can HMAC signatures be verified without buffering the entire request body?</summary>

**Answer:**
Yes, by computing an incremental digest as bytes arrive via a custom `FilterInputStream`, or by having the client supply a pre-computed `Content-SHA256` header, which the server verifies while streaming without buffering the full payload in memory.
</details>

<details>
<summary>7. What should be included in a canonical request string?</summary>

**Answer:**
A canonical request string should include:
1. HTTP Method (in uppercase, e.g., `POST`).
2. Request URI path (e.g., `/api/v1/orders`).
3. Query parameters (sorted alphabetically).
4. Request timestamp (epoch seconds).
5. Nonce (if used).
6. Hash of the request body (SHA-256).
</details>

<details>
<summary>8. How do you rotate HMAC shared secrets without breaking client integrations?</summary>

**Answer:**
Implement dual-secret verification. The server stores both a current active secret and a retired secondary secret. When a request arrives, the server first attempts verification with the primary secret. If that fails, it attempts verification with the secondary secret during the transition window before fully decommissioning the retired key.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Timestamp Validator (Easy)</summary>

**Task**: Write a utility method that verifies whether an epoch timestamp string is within 300 seconds of current system time.

**Solution**:
```java
public boolean isTimestampValid(String timestampStr) {
    try {
        long timestamp = Long.parseLong(timestampStr);
        long now = Instant.now().getEpochSecond();
        return Math.abs(now - timestamp) <= 300L;
    } catch (NumberFormatException e) {
        return false;
    }
}
```
</details>

<details>
<summary>Exercise 2: HMAC-SHA256 Calculator (Easy)</summary>

**Task**: Implement a standalone method that accepts a data string and a secret key and returns a Base64-encoded HMAC-SHA256 string.

**Solution**:
```java
public String computeHmac(String data, String secret) throws Exception {
    Mac mac = Mac.getInstance("HmacSHA256");
    mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
    byte[] hash = mac.doFinal(data.getBytes(StandardCharsets.UTF_8));
    return Base64.getEncoder().encodeToString(hash);
}
```
</details>

<details>
<summary>Exercise 3: Custom ApiKeyAuthenticationProvider (Medium)</summary>

**Task**: Create an `AuthenticationProvider` that supports `ApiKeyAuthenticationToken`, looks up the key in an `ApiKeyRepository`, and returns an authenticated token with assigned authorities.

**Solution**:
```java
@Component
public class ApiKeyAuthenticationProvider implements AuthenticationProvider {

    private final ApiKeyRepository repository;

    public ApiKeyAuthenticationProvider(ApiKeyRepository repository) {
        this.repository = repository;
    }

    @Override
    public Authentication authenticate(Authentication authentication) throws AuthenticationException {
        ApiKeyAuthenticationToken token = (ApiKeyAuthenticationToken) authentication;
        ApiKeyEntity entity = repository.findByKey(token.getApiKey())
            .orElseThrow(() -> new BadCredentialsException("Invalid API Key"));

        return new ApiKeyAuthenticationToken(
            entity.getKey(),
            entity.getOwner(),
            List.of(new SimpleGrantedAuthority("ROLE_" + entity.getRole()))
        );
    }

    @Override
    public boolean supports(Class<?> authentication) {
        return ApiKeyAuthenticationToken.class.isAssignableFrom(authentication);
    }
}
```
</details>

<details>
<summary>Exercise 4: Stripe-Style Webhook Signature Verifier (Medium)</summary>

**Task**: Implement a method that validates incoming webhook signatures where the header format is `t=1700000000,v1=abcdef...`.

**Solution**:
```java
public boolean verifyWebhook(String header, String rawBody, String secret) {
    String[] parts = header.split(",");
    String timestamp = null;
    String signature = null;

    for (String part : parts) {
        if (part.startsWith("t=")) timestamp = part.substring(2);
        if (part.startsWith("v1=")) signature = part.substring(3);
    }

    if (timestamp == null || signature == null || !isTimestampValid(timestamp)) {
        return false;
    }

    String payload = timestamp + "." + rawBody;
    String expected = computeHmac(payload, secret);
    return MessageDigest.isEqual(expected.getBytes(StandardCharsets.UTF_8), signature.getBytes(StandardCharsets.UTF_8));
}
```
</details>

<details>
<summary>Exercise 5: Body Caching OncePerRequestFilter (Hard)</summary>

**Task**: Write a complete servlet filter that wraps incoming requests in a `ContentCachingRequestWrapper` before filter chain execution and computes a SHA-256 hash of the buffered body after completion for audit logging.

**Solution**:
```java
public class AuditBodyCachingFilter extends OncePerRequestFilter {

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        ContentCachingRequestWrapper wrappedRequest = new ContentCachingRequestWrapper(request);
        try {
            filterChain.doFilter(wrappedRequest, response);
        } finally {
            byte[] body = wrappedRequest.getContentAsByteArray();
            if (body.length > 0) {
                String sha256 = DigestUtils.sha256Hex(body);
                logger.info("Request " + request.getRequestURI() + " Body SHA-256: " + sha256);
            }
        }
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Test for HMAC Validation (Hard)</summary>

**Task**: Write a `@WebMvcTest` demonstrating that sending a request with an invalid signature produces HTTP 401 Unauthorized, while a correctly signed request produces HTTP 200 OK.

**Solution**:
```java
@WebMvcTest(controllers = PaymentController.class)
@Import(ApiKeySecurityConfig.class)
class HmacControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldRejectInvalidSignature() throws Exception {
        mockMvc.perform(post("/api/v1/payments")
                .header("X-API-Key", "client_corp_123")
                .header("X-Signature-Timestamp", String.valueOf(Instant.now().getEpochSecond()))
                .header("X-Signature", "invalid-signature")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"amount\":100}"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldAcceptValidSignature() throws Exception {
        String timestamp = String.valueOf(Instant.now().getEpochSecond());
        String body = "{\"amount\":100}";
        String canonical = "POST\n/api/v1/payments\n" + timestamp + "\n" + body;
        String signature = calculateHmacSha256(canonical, "secret_key_super_confidential_456");

        mockMvc.perform(post("/api/v1/payments")
                .header("X-API-Key", "client_corp_123")
                .header("X-Signature-Timestamp", timestamp)
                .header("X-Signature", signature)
                .contentType(MediaType.APPLICATION_JSON)
                .content(body))
            .andExpect(status().isOk());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **HMAC Protects Integrity & Identity**: Unlike simple API keys, HMAC ensures the request body, method, and headers cannot be altered in transit.
- **Never Transmit the Secret**: The secret remains private to the client and server.
- **Always Validate Timestamps**: Reject requests outside a 5-minute window to neutralize replay attacks.
- **Use `ContentCachingRequestWrapper`**: Enables the filter to hash request body bytes without breaking Spring MVC controller deserialization.
- **Enforce Constant-Time Comparison**: Use `MessageDigest.isEqual(...)` to neutralize timing side-channel attacks.

---

## 13. Further Reading
- [AWS Signature Version 4 Documentation](https://docs.aws.amazon.com/general/latest/gr/signing_aws_api_requests.html)
- [Stripe Webhook Signature Verification](https://stripe.com/docs/webhooks/signatures)
- [RFC 2104: HMAC: Keyed-Hashing for Message Authentication](https://datatracker.ietf.org/doc/html/rfc2104)
- [Spring Security Reference: Custom Filters](https://docs.spring.io/spring-security/reference/servlet/architecture.html#servlet-filters-review)
- [OWASP REST Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/REST_Security_Cheat_Sheet.html)
