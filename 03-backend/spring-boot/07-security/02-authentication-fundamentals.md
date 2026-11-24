# Authentication Fundamentals: Tokens, Providers & Custom Authentication Filters

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, HTTP Request lifecycle, Cryptographic hashing |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Security 6.4.x / 7.x, Spring Boot 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Custom JSON Login Filter with AuthenticationManager](#51-custom-json-login-filter-with-authenticationmanager)
   - [5.2 Multi-Provider Authentication with Fallback Support](#52-multi-provider-authentication-with-fallback-support)
   - [5.3 Custom AuthenticationSuccessHandler and AuthenticationFailureHandler](#53-custom-authenticationsuccesshandler-and-authenticationfailurehandler)
   - [5.4 Anti-Pattern: Retaining Plaintext Passwords in SecurityContext](#54-anti-pattern-retaining-plaintext-passwords-in-securitycontext)
   - [5.5 One-Time Password (OTP) Dual-Phase Authentication Token](#55-one-time-password-otp-dual-phase-authentication-token)
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
In modern web applications, user authentication rarely follows the traditional HTML `<form>` POST pattern with URL-encoded form data. Modern single-page applications (SPAs) and mobile clients authenticate via JSON payloads (`{"username": "...", "password": "..."}`), one-time passwords (OTP / 2FA SMS tokens), dynamic API keys, or pre-authenticated headers from reverse proxies. Attempting to fit these diverse mechanisms into standard form login or writing ad-hoc authentication logic inside controllers bypasses Spring Security's auditing, password erasing, security context management, and exception translation mechanisms.

### Historical Evolution
Spring Security decoupled the mechanism of *extracting* credentials from the mechanism of *validating* them. An HTTP filter extracts credentials from the request and wraps them into an unauthenticated `Authentication` token. The `AuthenticationManager` coordinates one or more `AuthenticationProvider` instances to validate the token and return an authenticated `Authentication` object. In Spring Security 6+, this architecture remains the core engine for all authentication schemes, from Basic Auth to OAuth2, Passkeys, and custom enterprise SSO.

### Real-World Relevance
Designing clean authentication filters and custom providers ensures that enterprise identity systems (LDAP, Active Directory, internal microservice tokens, TOTP two-factor codes) integrate seamlessly with Spring Security without breaking method-level security or request authorization rules.

---

## 2. Core Concepts

### The Authentication Contract
The `org.springframework.security.core.Authentication` interface defines four fundamental contracts:
1. `getPrincipal()`: The identity of the subject (e.g. `UserDetails` object, email, or user ID).
2. `getCredentials()`: The secret proving identity (e.g. raw password, token string). **Must be erased after authentication**.
3. `getAuthorities()`: An immutable collection of `GrantedAuthority` instances representing roles/permissions (e.g. `ROLE_ADMIN`, `SCOPE_READ`).
4. `isAuthenticated()`: Boolean flag indicating whether the token has been verified by an `AuthenticationProvider`.

### Standard Authentication Tokens
- `UsernamePasswordAuthenticationToken`: Standard token for password-based logins (has unauthenticated and authenticated constructor variants).
- `PreAuthenticatedAuthenticationToken`: Used when identity was already validated externally (e.g., via mTLS x509 client certificates or reverse proxy headers).
- `RememberMeAuthenticationToken`: Generated via remember-me persistent cookies.
- `AnonymousAuthenticationToken`: Assigned to unauthenticated public requests to avoid `null` checks in downstream filters.

### The ProviderManager Delegation Chain
`ProviderManager` is the default `AuthenticationManager`. It holds an ordered list of `AuthenticationProvider` instances. When asked to authenticate a token:
1. It queries each provider: `provider.supports(token.getClass())`.
2. If supported, it invokes `provider.authenticate(token)`.
3. If the provider returns an authenticated token, execution halts and the token is returned.
4. If a provider returns `null`, the next provider is tried.
5. If all providers fail or return `null`, an `AuthenticationException` is thrown.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring Authentication Pipeline                             |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Incoming Request: POST /api/login {"username": "alice", "password": "secret"}  |
|         |                                                                          |
|         v                                                                          |
|  CustomJsonAuthenticationFilter                                                    |
|  - Deserializes JSON body into LoginRequest record                                 |
|  - Creates Unauthenticated Token:                                                  |
|    new UsernamePasswordAuthenticationToken("alice", "secret") [isAuthenticated=false]
|         |                                                                          |
|         v                                                                          |
|  AuthenticationManager (ProviderManager)                                           |
|         |                                                                          |
|         +---> Provider 1: LdapAuthenticationProvider (supports = false)            |
|         |                                                                          |
|         +---> Provider 2: DaoAuthenticationProvider (supports = true)              |
|               |                                                                    |
|               +-- userDetailsService.loadUserByUsername("alice")                   |
|               +-- passwordEncoder.matches("secret", user.getPasswordHash())        |
|               +-- Returns Authenticated Token:                                     |
|                   new UsernamePasswordAuthenticationToken(user, null, authorities) |
|                   [isAuthenticated=true]                                           |
|                                                                                    |
|  2. Success Handling:                                                              |
|     - SecurityContextHolder.getContext().setAuthentication(authenticatedToken);    |
|     - AuthenticationSuccessHandler.onAuthenticationSuccess()                       |
|       (Generates JWT or sets session cookie and returns HTTP 200 JSON)             |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Creating an Authenticated Token
```java
// Unauthenticated (Credentials present, authorities empty, isAuthenticated = false)
Authentication unauth = UsernamePasswordAuthenticationToken.unauthenticated("user", "pass");

// Authenticated (Principal present, credentials null, authorities loaded, isAuthenticated = true)
Authentication auth = UsernamePasswordAuthenticationToken.authenticated(
    userDetails, 
    null, // Erase credentials!
    userDetails.getAuthorities()
);
```

---

## 5. Practical Examples

### 5.1 Custom JSON Login Filter with AuthenticationManager
```java
package com.example.security.filter;

import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.AuthenticationServiceException;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.web.authentication.AbstractAuthenticationProcessingFilter;
import org.springframework.security.web.util.matcher.AntPathRequestMatcher;

import java.io.IOException;

public class JsonLoginFilter extends AbstractAuthenticationProcessingFilter {

    private final ObjectMapper objectMapper = new ObjectMapper();

    public record LoginPayload(String username, String password) {}

    public JsonLoginFilter(AuthenticationManager authenticationManager) {
        // Intercepts specifically POST /api/v1/auth/login
        super(new AntPathRequestMatcher("/api/v1/auth/login", "POST"), authenticationManager);
    }

    @Override
    public Authentication attemptAuthentication(
        HttpServletRequest request, 
        HttpServletResponse response
    ) throws AuthenticationException, IOException {

        if (!request.getContentType().startsWith("application/json")) {
            throw new AuthenticationServiceException("Authentication method not supported: " + request.getContentType());
        }

        LoginPayload payload = objectMapper.readValue(request.getInputStream(), LoginPayload.class);

        // Instantiate unauthenticated token
        var authRequest = UsernamePasswordAuthenticationToken.unauthenticated(
            payload.username().trim(),
            payload.password()
        );

        setDetails(request, authRequest);

        // Delegate credential verification to AuthenticationManager
        return this.getAuthenticationManager().authenticate(authRequest);
    }
}
```

### 5.2 Multi-Provider Authentication with Fallback Support
```java
package com.example.security.auth;

import org.springframework.security.authentication.AuthenticationProvider;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.stereotype.Component;

import java.util.List;

/**
 * Secondary authentication provider for emergency master administrative access.
 */
@Component
public class BreakGlassAuthenticationProvider implements AuthenticationProvider {

    private static final String MASTER_ADMIN = "breakglass_admin";
    private static final String MASTER_KEY = "super-secret-emergency-override-key";

    @Override
    public Authentication authenticate(Authentication authentication) throws AuthenticationException {
        String username = authentication.getName();
        String credentials = authentication.getCredentials().toString();

        if (MASTER_ADMIN.equals(username)) {
            if (MASTER_KEY.equals(credentials)) {
                return UsernamePasswordAuthenticationToken.authenticated(
                    username,
                    null,
                    List.of(new SimpleGrantedAuthority("ROLE_ADMIN"), new SimpleGrantedAuthority("ROLE_BREAKGLASS"))
                );
            }
            throw new BadCredentialsException("Invalid emergency key");
        }

        // Return null to signal ProviderManager to try next AuthenticationProvider!
        return null;
    }

    @Override
    public boolean supports(Class<?> authentication) {
        return UsernamePasswordAuthenticationToken.class.isAssignableFrom(authentication);
    }
}
```

### 5.3 Custom AuthenticationSuccessHandler and AuthenticationFailureHandler
```java
package com.example.security.handler;

import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.web.authentication.AuthenticationFailureHandler;
import org.springframework.security.web.authentication.AuthenticationSuccessHandler;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.time.Instant;
import java.util.Map;

@Component
public class JsonAuthenticationHandlers implements AuthenticationSuccessHandler, AuthenticationFailureHandler {

    private final ObjectMapper objectMapper = new ObjectMapper();

    @Override
    public void onAuthenticationSuccess(
        HttpServletRequest request, 
        HttpServletResponse response, 
        Authentication authentication
    ) throws IOException {

        response.setStatus(HttpStatus.OK.value());
        response.setContentType(MediaType.APPLICATION_JSON_VALUE);

        Map<String, Object> body = Map.of(
            "status", "SUCCESS",
            "principal", authentication.getName(),
            "timestamp", Instant.now().toString()
        );

        objectMapper.writeValue(response.getOutputStream(), body);
    }

    @Override
    public void onAuthenticationFailure(
        HttpServletRequest request, 
        HttpServletResponse response, 
        AuthenticationException exception
    ) throws IOException {

        response.setStatus(HttpStatus.UNAUTHORIZED.value());
        response.setContentType(MediaType.APPLICATION_PROBLEM_JSON_VALUE);

        Map<String, Object> error = Map.of(
            "type", "about:blank",
            "title", "Authentication Failed",
            "status", 401,
            "detail", exception.getMessage()
        );

        objectMapper.writeValue(response.getOutputStream(), error);
    }
}
```

### 5.4 Anti-Pattern: Retaining Plaintext Passwords in SecurityContext
```java
// =========================================================================
// ANTI-PATTERN: Retaining raw user passwords in memory inside Authentication
// =========================================================================
public class VulnerableAuthProvider implements AuthenticationProvider {
    @Override
    public Authentication authenticate(Authentication auth) {
        String rawPassword = auth.getCredentials().toString();
        // CATASTROPHIC SECURITY FLAW:
        // Passing rawPassword back into the authenticated token!
        // The plaintext password remains pinned in the heap memory inside SecurityContextHolder!
        // Memory heap dumps, logging interceptors, or serialization can leak user passwords!
        return new UsernamePasswordAuthenticationToken(user, rawPassword, authorities); 
    }
}

// =========================================================================
// PRODUCTION FIX: Always erase credentials when creating authenticated tokens
// =========================================================================
public class SecureAuthProvider implements AuthenticationProvider {
    @Override
    public Authentication authenticate(Authentication auth) {
        // SECURE: Set credentials parameter strictly to NULL!
        return UsernamePasswordAuthenticationToken.authenticated(user, null, authorities);
    }
}
```

### 5.5 One-Time Password (OTP) Dual-Phase Authentication Token
```java
package com.example.security.otp;

import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.core.GrantedAuthority;

import java.util.Collection;

public class OtpAuthenticationToken extends AbstractAuthenticationToken {

    private final String username;
    private final String otpCode;

    // Phase 1: Unauthenticated token with user and submitted OTP code
    public OtpAuthenticationToken(String username, String otpCode) {
        super(null);
        this.username = username;
        this.otpCode = otpCode;
        setAuthenticated(false);
    }

    // Phase 2: Authenticated token
    public OtpAuthenticationToken(String username, Collection<? extends GrantedAuthority> authorities) {
        super(authorities);
        this.username = username;
        this.otpCode = null; // Cleared!
        setAuthenticated(true);
    }

    @Override
    public Object getCredentials() { return otpCode; }
    @Override
    public Object getPrincipal() { return username; }
}
```

---

## 6. Best Practices

1. **Always Set Credentials to `null` Upon Authentication**: When returning a verified `Authentication` object from an `AuthenticationProvider`, always set credentials to `null` to erase plaintext passwords from the JVM heap.
2. **Use Two Constructors on Custom Tokens**: Maintain distinct constructors: a public unauthenticated constructor (`isAuthenticated = false`) and a protected authenticated constructor accepting `Collection<GrantedAuthority>` (`isAuthenticated = true`).
3. **Return `null` from `AuthenticationProvider` for Unsupported Tokens**: If a provider does not handle a specific user category or credential type, returning `null` permits the `ProviderManager` to try the next provider in the chain.
4. **Use `EraseCredentialsMethodInterceptor`**: Ensure Spring Security's automatic credential erasing is enabled on `ProviderManager` (`providerManager.setEraseCredentialsAfterAuthentication(true)`).
5. **Never Use Controller Code for Authentication**: Do not write custom `login()` methods inside Spring MVC controllers that manually manipulate session attributes. Use standard filter and `AuthenticationManager` pipelines.
6. **Return RFC 7807 `ProblemDetail` on Failures**: Use custom `AuthenticationFailureHandler` implementations to return structured JSON errors rather than HTML error pages.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Manually Calling `setAuthenticated(true)` on Raw Input** | Bypasses credential validation, allowing attackers to forge authenticated sessions. | Only `AuthenticationProvider` should instantiate authenticated tokens. |
| **Retaining Raw Passwords in `getCredentials()`** | Plaintext credentials persist in memory and can leak via heap dumps or logging. | Pass `null` as credentials to the authenticated token constructor. |
| **Throwing Unchecked Generic Exceptions in Providers** | Non-`AuthenticationException` errors bypass the `AuthenticationFailureHandler` and cause unhandled HTTP 500s. | Always throw subclasses of `AuthenticationException` (e.g. `BadCredentialsException`). |
| **Registering Custom Filters as Standard Spring Beans** | Marking a filter with `@Component` causes Spring Boot to register it twice: once in the Servlet container and once in the SecurityFilterChain. | Register custom filters exclusively via `http.addFilterBefore(...)` without `@Component`. |
| **Forgetting to Call `setDetails(request, token)`** | Omits client IP address and session ID details from security audit logs. | Call `authRequest.setDetails(new WebAuthenticationDetailsSource().buildDetails(request))`. |
| **Overwriting `SecurityContext` Without Saving to Repository** | In stateful applications, modifying `SecurityContextHolder` without saving to `SecurityContextRepository` causes session loss. | Explicitly invoke `securityContextRepository.saveContext(context, request, response)`. |

---

## 8. Performance & Security Notes

### Performance
- **Provider Short-Circuiting**: Order fast in-memory or cache-based `AuthenticationProvider`s before slow network-based providers (like LDAP or OAuth2) to accelerate authentication throughput.
- **Garbage Collection Optimization**: Reusable token objects and immediate credential clearing minimize heap memory fragmentation.

### Security
- **Enumeration Attack Defense**: When credentials fail, always throw a generic `BadCredentialsException("Invalid username or password")`. Never reveal whether the username exists or the password was incorrect.
- **Heap Memory Inspection**: Clear string buffers and char arrays containing sensitive passwords immediately after cryptographic comparison.

---

## 9. Comparison / When to Use What

| Authentication Mechanism | Credential Format | State Stored | Best Use Case |
| :--- | :--- | :--- | :--- |
| **UsernamePasswordToken** | Password / Secret | Session or Stateless | Standard Web / API Login |
| **Bearer Token (JWT)** | Signed Cryptographic JWT | Stateless (In token) | Distributed Microservices |
| **PreAuthenticatedToken** | Client Cert / Proxy Header | External Proxy | mTLS, Cloudflare Zero Trust, Envoy |
| **API Key / Secret** | Static Secret Header | Stateless (Database) | Machine-to-Machine Integration |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the architectural role of <code>AuthenticationManager</code>, <code>ProviderManager</code>, and <code>AuthenticationProvider</code>.</strong></summary>

- `AuthenticationManager` is the top-level API interface responsible for authenticating an `Authentication` request.
- `ProviderManager` is the standard implementation of `AuthenticationManager`. It coordinates a collection of `AuthenticationProvider` instances, iterating through them until one successfully validates the token.
- `AuthenticationProvider` is a strategy interface that handles a specific token type (e.g., `DaoAuthenticationProvider` for username/password, `JwtAuthenticationProvider` for JWTs).
</details>

<details>
<summary><strong>2. Why are there two separate constructors on <code>UsernamePasswordAuthenticationToken</code>?</strong></summary>

- The first constructor accepts `(principal, credentials)`. It sets `isAuthenticated(false)` and is used by filters before authentication to represent unverified credentials.
- The second constructor accepts `(principal, credentials, authorities)`. It sets `isAuthenticated(true)` and can only be used by `AuthenticationProvider` after credentials have been verified.
</details>

<details>
<summary><strong>3. What happens if an <code>AuthenticationProvider</code> returns <code>null</code> during authentication?</strong></summary>

Returning `null` indicates that the provider does not support or cannot authenticate the given token, but has not encountered an explicit error. `ProviderManager` interprets `null` as a signal to continue down the chain and attempt authentication with the next registered provider.
</details>

<details>
<summary><strong>4. Why should custom security filters NOT be annotated with <code>@Component</code> when registered in <code>SecurityFilterChain</code>?</strong></summary>

If a filter is annotated with `@Component`, Spring Boot's servlet auto-configuration automatically registers it with the servlet container (Tomcat) as a global servlet filter. When you also register it inside the `SecurityFilterChain` via `http.addFilterBefore(...)`, the filter executes **twice** for every request, which can cause duplicate authentication processing or unexpected state mutations.
</details>

<details>
<summary><strong>5. How does Spring Security prevent credential leakage in heap memory?</strong></summary>

`ProviderManager` has `eraseCredentialsAfterAuthentication` enabled by default (set to `true`). If an authentication token implements `CredentialsContainer`, Spring Security automatically invokes `eraseCredentials()`, overwriting passwords and sensitive tokens in memory immediately after successful authentication.
</details>

<details>
<summary><strong>6. What is the difference between <code>AbstractAuthenticationProcessingFilter</code> and <code>OncePerRequestFilter</code>?</strong></summary>

- `AbstractAuthenticationProcessingFilter` is specifically designed for login entry points (like `/login`). It checks if the request matches a specific URL, attempts authentication, and delegates to success/failure handlers.
- `OncePerRequestFilter` is a general-purpose filter that executes on every request. It is typically used for per-request stateless authentication (e.g., inspecting an `Authorization: Bearer <token>` header on every incoming API call).
</details>

<details>
<summary><strong>7. How do you implement a fallback authentication mechanism in Spring Security?</strong></summary>

Register multiple `AuthenticationProvider` beans in your security configuration (e.g., primary `LdapAuthenticationProvider` and secondary `DaoAuthenticationProvider`). `ProviderManager` attempts the primary provider first; if it returns `null` or credentials fail in a non-fatal manner, it falls back to the database provider.
</details>

<details>
<summary><strong>8. Why should you always use generic error messages for authentication failures?</strong></summary>

Distinguishing between "User not found" and "Incorrect password" allows malicious actors to perform user enumeration attacks, identifying valid email addresses or usernames in your system. Throwing a uniform `BadCredentialsException("Invalid username or password")` prevents enumeration.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Verifying Unauthenticated vs Authenticated Tokens</strong></summary>

**Task**: Write a unit test verifying that `UsernamePasswordAuthenticationToken.unauthenticated()` has `isAuthenticated() == false`, while `.authenticated()` has `isAuthenticated() == true`.

```java
@Test
void testTokenAuthenticationFlags() {
    var unauth = UsernamePasswordAuthenticationToken.unauthenticated("user", "pass");
    assertThat(unauth.isAuthenticated()).isFalse();

    var auth = UsernamePasswordAuthenticationToken.authenticated(
        "user", null, List.of(new SimpleGrantedAuthority("ROLE_USER"))
    );
    assertThat(auth.isAuthenticated()).isTrue();
    assertThat(auth.getCredentials()).isNull();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Registering JsonLoginFilter in SecurityFilterChain</strong></summary>

**Task**: Configure `SecurityFilterChain` to register a `JsonLoginFilter` before `UsernamePasswordAuthenticationFilter`.

```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http, AuthenticationManager authManager) throws Exception {
    return http
        .addFilterBefore(new JsonLoginFilter(authManager), UsernamePasswordAuthenticationFilter.class)
        .authorizeHttpRequests(a -> a.anyRequest().authenticated())
        .build();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom AuthenticationProvider Implementation</strong></summary>

**Task**: Implement an `AuthenticationProvider` that validates static secret tokens for background batch worker processes.

```java
@Component
public class BatchWorkerAuthProvider implements AuthenticationProvider {
    @Override
    public Authentication authenticate(Authentication auth) throws AuthenticationException {
        if ("worker-batch".equals(auth.getName()) && "secret-worker-key".equals(auth.getCredentials())) {
            return UsernamePasswordAuthenticationToken.authenticated(
                "worker-batch", null, List.of(new SimpleGrantedAuthority("ROLE_BATCH"))
            );
        }
        throw new BadCredentialsException("Invalid worker credentials");
    }

    @Override
    public boolean supports(Class<?> authClass) {
        return UsernamePasswordAuthenticationToken.class.isAssignableFrom(authClass);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): JSON Authentication Failure Handler</strong></summary>

**Task**: Write a failure handler that logs failed login attempts with client IP and outputs an HTTP 401 JSON body.

```java
public class AuditFailureHandler implements AuthenticationFailureHandler {
    @Override
    public void onAuthenticationFailure(HttpServletRequest req, HttpServletResponse res, AuthenticationException ex) 
            throws IOException {
        res.setStatus(401);
        res.setContentType("application/json");
        res.getWriter().write("{\"error\": \"Invalid credentials\", \"ip\": \"" + req.getRemoteAddr() + "\"}");
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): OTP Verification Two-Factor Authentication Provider</strong></summary>

**Task**: Implement an `AuthenticationProvider` that supports `OtpAuthenticationToken`, validates a 6-digit TOTP code, and returns authenticated authorities.

```java
@Component
public class OtpAuthenticationProvider implements AuthenticationProvider {
    @Autowired private TotpService totpService;

    @Override
    public Authentication authenticate(Authentication auth) throws AuthenticationException {
        OtpAuthenticationToken token = (OtpAuthenticationToken) auth;
        if (totpService.isValid(token.getName(), token.getCredentials().toString())) {
            return new OtpAuthenticationToken(token.getName(), List.of(new SimpleGrantedAuthority("ROLE_2FA_VERIFIED")));
        }
        throw new BadCredentialsException("Invalid or expired 2FA OTP code");
    }

    @Override
    public boolean supports(Class<?> authClass) {
        return OtpAuthenticationToken.class.isAssignableFrom(authClass);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Exposing AuthenticationManager Bean in Spring Boot 3</strong></summary>

**Task**: Configure a Spring configuration class that extracts and exposes the `AuthenticationManager` bean from `AuthenticationConfiguration`.

```java
@Configuration
public class AuthManagerConfig {
    @Bean
    public AuthenticationManager authenticationManager(AuthenticationConfiguration config) throws Exception {
        return config.getAuthenticationManager();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Contract:     Authentication: getPrincipal(), getCredentials(), getAuthorities()      |
| Tokens:       Unauthenticated (pass credentials) -> Authenticated (erase credentials!)|
| Manager:      ProviderManager delegates to list of AuthenticationProviders            |
| Providers:    Return authenticated token, throw AuthenticationException, or null (skip)|
| Clean JSON:   Extend AbstractAuthenticationProcessingFilter for JSON POST logins      |
| Handlers:     AuthenticationSuccessHandler and AuthenticationFailureHandler for JSON  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Security Authentication Architecture Documentation](https://docs.spring.io/spring-security/reference/servlet/authentication/architecture.html)
- [Baeldung: Custom AuthenticationProvider in Spring Security](https://www.baeldung.com/spring-security-authentication-provider)
- [Spring Security UsernamePasswordAuthenticationFilter Reference](https://docs.spring.io/spring-security/site/docs/current/api/org/springframework/security/web/authentication/UsernamePasswordAuthenticationFilter.html)
- [OWASP Authentication Verification Standard](https://owasp.org/www-project-application-security-verification-standard/)
