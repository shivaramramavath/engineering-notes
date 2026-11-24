# Reactive Security with Spring Security in WebFlux

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Spring WebFlux Architecture, Spring Security Fundamentals, Project Reactor Context |
| **Estimated Reading Time** | 24 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Spring Security 6.4, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Servlet Security vs Reactive Security](#9-comparison-servlet-security-vs-reactive-security)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In traditional Spring MVC applications, Spring Security relies entirely on `SecurityFilterChain`, `HttpServletRequest`, and **`ThreadLocal`** storage via `SecurityContextHolder`.

### The Problem in Reactive Systems
- In Spring WebFlux, execution hops across multiple event-loop threads (`reactor-http-nio-*`) during a single request lifecycle.
- Storing authentication data in a `ThreadLocal` results in immediate security context loss or, worse, **security context leakage** where one tenant's credentials bleed into another user's request sharing the same event loop.
- Standard blocking authentication providers (`UserDetailsService`) stall the Netty event loop during database password verification.

### The Solution: Spring Security Reactive Architecture
Spring Security provides a dedicated reactive module (`spring-security-webflux`). It replaces Servlet filters with non-blocking `WebFilter` chains, substitutes `SecurityContextHolder` with the immutable **Reactor `Context`** (`ReactiveSecurityContextHolder`), and introduces non-blocking security abstractions like `ReactiveUserDetailsService` and `ReactiveAuthenticationManager`.

---

## 2. Core Concepts

### Mental Model & The Reactive Security Filter Chain
```
              Incoming Request (ServerWebExchange)
                                │
                                ▼
                   SecurityWebFilterChain
                                │
         ┌──────────────────────┼──────────────────────┐
         ▼                      ▼                      ▼
  CorsWebFilter          CsrfWebFilter        AuthenticationWebFilter
                                                       │
                                                       ▼
                                          ReactiveAuthenticationManager
                                          (Verifies JWT / Password non-blockingly)
                                                       │
                                                       ▼
                                         Extracts Authentication Principal
                                                       │
                                                       ▼
                                         Writes SecurityContext into
                                            Reactor Context!
                                                       │
                                                       ▼
                                         AuthorizationWebFilter
                                         (Evaluates Reactive Authorization Rules)
                                                       │
                                                       ▼
                                           Dispatches to WebFlux Handler
```

### Invariants
1. **Context-Bound Identity**: In WebFlux, the authenticated user (`Authentication`) is stored in the **Reactor `Context`**, never in a `ThreadLocal`.
2. **Non-Blocking Authentication**: Every security operation (password hashing, database lookup, JWT signature verification) must return `Mono<Authentication>` or `Mono<AuthorizationDecision>`.
3. **`SecurityWebFilterChain` DSL**: Configuration uses `ServerHttpSecurity` to construct a `SecurityWebFilterChain` bean (replacing `HttpSecurity` and `SecurityFilterChain`).

---

## 3. How It Works Internally

### `ReactiveSecurityContextHolder` Mechanics
1. When a request is authenticated via `AuthenticationWebFilter`, the resulting `SecurityContext` is written into the subscriber's Reactor `Context` under key `SecurityContext.class`.
2. In your controller or service, invoking `ReactiveSecurityContextHolder.getContext()` inspects the subscriber's `ContextView`:
```java
public static Mono<SecurityContext> getContext() {
    return Mono.deferContextual(contextView -> 
        Mono.justOrEmpty(contextView.getOrDefault(SECURITY_CONTEXT_KEY, null))
    );
}
```
3. Because the Reactor `Context` travels downstream along the subscription path, the authentication context remains intact regardless of how many threads the stream hops across.

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-security</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-webflux</artifactId>
</dependency>
```

### Enabling Reactive Security Configuration
```java
package com.example.webflux.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.method.configuration.EnableReactiveMethodSecurity;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.web.server.SecurityWebFilterChain;

@Configuration(proxyBeanMethods = false)
@EnableWebFluxSecurity
@EnableReactiveMethodSecurity // Enables @PreAuthorize on reactive methods
public class ReactiveSecurityConfig {

    @Bean
    public SecurityWebFilterChain springSecurityFilterChain(ServerHttpSecurity http) {
        return http
                .csrf(ServerHttpSecurity.CsrfSpec::disable) // Typically disabled for stateless JWT APIs
                .authorizeExchange(exchanges -> exchanges
                        .pathMatchers("/api/public/**", "/actuator/health/**").permitAll()
                        .pathMatchers("/api/admin/**").hasRole("ADMIN")
                        .anyExchange().authenticated()
                )
                .httpBasic(ServerHttpSecurity.HttpBasicSpec::disable)
                .formLogin(ServerHttpSecurity.FormLoginSpec::disable)
                .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Stateless Reactive JWT Authentication with `ReactiveAuthenticationManager`

```java
package com.example.webflux.security.jwt;

import org.springframework.http.HttpHeaders;
import org.springframework.security.authentication.ReactiveAuthenticationManager;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.stereotype.Component;
import org.springframework.web.server.ServerWebExchange;
import org.springframework.web.server.WebFilter;
import org.springframework.web.server.WebFilterChain;
import reactor.core.publisher.Mono;

import java.util.List;

@Component
public class JwtReactiveAuthFilter implements WebFilter {

    private final JwtTokenValidator tokenValidator;

    public JwtReactiveAuthFilter(JwtTokenValidator tokenValidator) {
        this.tokenValidator = tokenValidator;
    }

    @Override
    public Mono<Void> filter(ServerWebExchange exchange, WebFilterChain chain) {
        String authHeader = exchange.getRequest().getHeaders().getFirst(HttpHeaders.AUTHORIZATION);

        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            String token = authHeader.substring(7);
            return tokenValidator.validateAndExtractClaims(token)
                    .flatMap(claims -> {
                        var auth = new UsernamePasswordAuthenticationToken(
                                claims.username(),
                                null,
                                List.of(new SimpleGrantedAuthority("ROLE_" + claims.role()))
                        );
                        // Inject Authentication into the Reactor Context!
                        return chain.filter(exchange)
                                .contextWrite(org.springframework.security.core.context.ReactiveSecurityContextHolder
                                        .withAuthentication(auth));
                    });
        }

        return chain.filter(exchange);
    }
}

record ClaimsDto(String username, String role) {}
interface JwtTokenValidator { Mono<ClaimsDto> validateAndExtractClaims(String token); }
```

### Example 2: Accessing Current User Principal in a WebFlux Controller

```java
package com.example.webflux.security.controller;

import org.springframework.security.core.Authentication;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.security.core.context.ReactiveSecurityContextHolder;
import org.springframework.security.core.context.SecurityContext;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Mono;

import java.security.Principal;

@RestController
@RequestMapping("/api/v1/user")
public class ReactiveUserProfileController {

    /**
     * Option A: Direct injection via @AuthenticationPrincipal
     */
    @GetMapping("/me")
    public Mono<String> getCurrentUser(@AuthenticationPrincipal Principal principal) {
        return Mono.just("Hello, " + principal.getName());
    }

    /**
     * Option B: Programmatic access from the Reactor Context
     */
    @GetMapping("/context-me")
    public Mono<String> getCurrentUserFromContext() {
        return ReactiveSecurityContextHolder.getContext()
                .map(SecurityContext::getAuthentication)
                .map(Authentication::getName)
                .map(name -> "Logged in as (from Reactor Context): " + name)
                .defaultIfEmpty("Anonymous User");
    }
}
```

### Example 3: Edge Case — Reactive Method Security with `@PreAuthorize`

```java
package com.example.webflux.security.service;

import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Mono;

import java.util.UUID;

@Service
public class SecuredDocumentService {

    /**
     * In WebFlux, @PreAuthorize intercepts the returned Mono/Flux.
     * The security check evaluates against the subscriber's Reactor Context!
     */
    @PreAuthorize("hasRole('ADMIN') or #username == authentication.name")
    public Mono<String> readConfidentialDocument(UUID docId, String username) {
        return Mono.just("Top Secret Payload for Document: " + docId);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Accessing `SecurityContextHolder` (ThreadLocal) in WebFlux
```java
// ANTI-PATTERN: Calling synchronous SecurityContextHolder in WebFlux
@GetMapping("/bad-identity")
public Mono<String> getBadIdentity() {
    // FATAL BUG: Returns null or reads another user's authentication from a reused Netty thread!
    Authentication auth = SecurityContextHolder.getContext().getAuthentication(); 
    return Mono.just("User: " + (auth != null ? auth.getName() : "null"));
}
```

#### Production Fix: Use `ReactiveSecurityContextHolder`
```java
// PRODUCTION FIX: Pure reactive context lookup
@GetMapping("/good-identity")
public Mono<String> getGoodIdentity() {
    return ReactiveSecurityContextHolder.getContext()
            .map(SecurityContext::getAuthentication)
            .map(Authentication::getName);
}
```

### Example 5: High-Performance Non-Blocking Database Authentication with `ReactiveUserDetailsService`

```java
package com.example.webflux.security.service;

import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.userdetails.ReactiveUserDetailsService;
import org.springframework.security.core.userdetails.User;
import org.springframework.security.core.userdetails.UserDetails;
import org.springframework.security.core.userdetails.UsernameNotFoundException;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Mono;

import java.util.List;

@Service
public class NonBlockingUserDetailsService implements ReactiveUserDetailsService {

    private final ReactiveUserRepository userRepository;

    public NonBlockingUserDetailsService(ReactiveUserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @Override
    public Mono<UserDetails> findByUsername(String username) {
        return userRepository.findByUsername(username)
                .switchIfEmpty(Mono.error(new UsernameNotFoundException("User not found: " + username)))
                .map(user -> User.builder()
                        .username(user.username())
                        .password(user.passwordHash())
                        .authorities(List.of(new SimpleGrantedAuthority("ROLE_" + user.role())))
                        .build());
    }
}

interface ReactiveUserRepository {
    Mono<UserEntity> findByUsername(String username);
}
record UserEntity(String username, String passwordHash, String role) {}
```

---

## 6. Best Practices

1. **Never Use `SecurityContextHolder`**: In WebFlux applications, strictly use `ReactiveSecurityContextHolder` to avoid thread leakage vulnerabilities.
2. **Implement `ReactiveUserDetailsService`**: Avoid blocking JDBC in user details lookups; use reactive repositories (R2DBC, Reactive Mongo).
3. **Enable Method Security via `@EnableReactiveMethodSecurity`**: Enables `@PreAuthorize` and `@PostAuthorize` expressions on methods returning `Mono` and `Flux`.
4. **Propagate Context Across Thread Hops**: When using operators that switch threads (`publishOn`, `subscribeOn`), ensure security context is not lost by using Reactor's automatic context propagation.
5. **Configure Reactive CSRF for Browser Form Apps**: For browser-facing WebFlux apps, configure `CookieServerCsrfTokenRepository` rather than disabling CSRF blindly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `SecurityContextHolder.getContext()`** | Relies on `ThreadLocal`; returns `null` or leaks other users' credentials across requests on Netty. | Use `ReactiveSecurityContextHolder.getContext()`. |
| **Using `HttpSecurity` Instead of `ServerHttpSecurity`** | `HttpSecurity` is for Servlet applications; causes compile or bean definition errors in WebFlux. | Define a `SecurityWebFilterChain` bean using `ServerHttpSecurity`. |
| **Using Blocking `UserDetailsService`** | Blocks Netty event-loop threads during password lookups. | Implement `ReactiveUserDetailsService` returning `Mono<UserDetails>`. |
| **Forgetting `@EnableWebFluxSecurity`** | Spring Security auto-configuration may fail to configure reactive filters properly. | Add `@EnableWebFluxSecurity` to your configuration class. |
| **Lost Security Context in `subscribeOn`** | Calling `subscribeOn(Schedulers.newSingle())` without Reactor context propagation loses the context. | Ensure context is captured or use `contextWrite()`. |
| **Applying `@PreAuthorize` on Methods Returning Void** | Method security requires reactive return types (`Mono`/`Flux`) to defer evaluation. | Ensure secured reactive service methods return `Mono<T>` or `Flux<T>`. |

---

## 8. Performance & Security Considerations

### Password Hashing (BCrypt / Argon2) on Event Loops
Password hashing algorithms (BCrypt, Argon2) are intentionally CPU-intensive (taking ~100-300ms of pure CPU time). Executing `passwordEncoder.matches()` directly on a Netty event loop stalls all other requests on that core! Always offload password verification to worker threads:
```java
Mono.fromCallable(() -> passwordEncoder.matches(raw, hash))
    .subscribeOn(Schedulers.boundedElastic());
```

### Context Bleeding in Shared Memory
Reactor `Context` is immutable and attached to each unique subscription pipeline. Unlike `ThreadLocal`, which persists across requests on pooled threads unless explicitly cleared, Reactor `Context` is naturally garbage collected when the reactive stream completes, eliminating thread-pool credential leakage.

---

## 9. Comparison: Servlet Security vs Reactive Security

| Aspect | Spring Security (Servlet / MVC) | Spring Security (Reactive / WebFlux) |
| :--- | :--- | :--- |
| **Configuration Class** | `HttpSecurity` $\rightarrow$ `SecurityFilterChain` | `ServerHttpSecurity` $\rightarrow$ `SecurityWebFilterChain` |
| **Security Context Store** | `SecurityContextHolder` (`ThreadLocal`) | `ReactiveSecurityContextHolder` (Reactor `Context`) |
| **Filter Chain** | `jakarta.servlet.Filter` | `org.springframework.web.server.WebFilter` |
| **User Details Service** | `UserDetailsService` (blocking) | `ReactiveUserDetailsService` (returns `Mono<UserDetails>`) |
| **Authentication Manager**| `AuthenticationManager` | `ReactiveAuthenticationManager` |
| **Method Security** | `@EnableMethodSecurity` | `@EnableReactiveMethodSecurity` |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why is `ThreadLocal`-based security (traditional `SecurityContextHolder`) fundamentally broken in Spring WebFlux?</strong></summary>

**Answer:**
Spring MVC uses a thread-per-request model where one thread serves an entire request from start to finish. In contrast, Spring WebFlux uses an event-loop architecture where an HTTP request is broken into discrete non-blocking tasks that hop across multiple threads (or share the same thread with thousands of other requests concurrently). A `ThreadLocal` set at the beginning of a request will be lost after an asynchronous boundary. Worse, because Netty reuses event-loop threads continuously, subsequent requests on the same thread could inherit the previous user's `ThreadLocal`, causing critical privilege escalation bugs.
</details>

<details>
<summary><strong>Q2: How does `ReactiveSecurityContextHolder` store and retrieve the `SecurityContext`?</strong></summary>

**Answer:**
`ReactiveSecurityContextHolder` leverages the **Reactor `Context`** feature of Project Reactor. The `Context` is an immutable key-value store tied to the downstream `Subscriber`. When an authentication filter validates credentials, it calls `.contextWrite(ReactiveSecurityContextHolder.withAuthentication(auth))`. Any downstream operator or controller can query this context using `ReactiveSecurityContextHolder.getContext()`, which extracts the authentication object from the stream's contextual snapshot.
</details>

<details>
<summary><strong>Q3: How do you configure a `SecurityWebFilterChain` in Spring Boot WebFlux?</strong></summary>

**Answer:**
Declare a `@Bean` returning `SecurityWebFilterChain` accepting `ServerHttpSecurity`:
```java
@Bean
public SecurityWebFilterChain springSecurityFilterChain(ServerHttpSecurity http) {
    return http
        .authorizeExchange(ex -> ex.pathMatchers("/public/**").permitAll().anyExchange().authenticated())
        .oauth2ResourceServer(oauth2 -> oauth2.jwt(Customizer.withDefaults()))
        .build();
}
```
</details>

<details>
<summary><strong>Q4: Why should password hashing (BCrypt) never be executed directly on a Netty event loop?</strong></summary>

**Answer:**
BCrypt is intentionally designed to consume significant CPU time (e.g. 100ms) to prevent brute-force attacks. Netty event-loop threads are limited in number (typically 1 per CPU core) and are designed for rapid, non-blocking I/O. Freezing an event loop for 100ms to calculate a hash halts all other concurrent network connections handled by that event loop, causing severe latency spikes. Password hashing must be offloaded using `Mono.fromCallable(...).subscribeOn(Schedulers.boundedElastic())`.
</details>

<details>
<summary><strong>Q5: What is the purpose of `ReactiveAuthenticationManager`?</strong></summary>

**Answer:**
`ReactiveAuthenticationManager` is the reactive equivalent of `AuthenticationManager`. It defines a single method: `Mono<Authentication> authenticate(Authentication authentication)`. It validates credentials asynchronously without blocking, returning a `Mono` emitting the fully authenticated principal with granted authorities.
</details>

<details>
<summary><strong>Q6: How does `@PreAuthorize` work in Spring WebFlux?</strong></summary>

**Answer:**
When `@EnableReactiveMethodSecurity` is enabled, Spring creates a reactive AOP proxy around the service method. Instead of evaluating the SpEL expression before method invocation, it wraps the method's returned `Mono` or `Flux` with a security operator. When the publisher is subscribed to, the operator extracts the `Authentication` from the subscriber's Reactor `Context` and evaluates the expression. If unauthorized, it emits an `AccessDeniedException` into the reactive error channel.
</details>

<details>
<summary><strong>Q7: Can you use OAuth2 Resource Server (JWT verification) in Spring WebFlux?</strong></summary>

**Answer:**
Yes. Spring Security provides reactive OAuth2 resource server support via `http.oauth2ResourceServer(oauth2 -> oauth2.jwt(Customizer.withDefaults()))`, backed by the non-blocking `ReactiveJwtDecoder` (Nimbus reactive adapter).
</details>

<details>
<summary><strong>Q8: How does CSRF protection work in Spring WebFlux?</strong></summary>

**Answer:**
WebFlux provides `CsrfWebFilter` and `ServerCsrfTokenRepository` (e.g. `WebSessionServerCsrfTokenRepository` or `CookieServerCsrfTokenRepository`). The CSRF token is exposed asynchronously as a `Mono<CsrfToken>` in `ServerWebExchange.getAttribute()`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Permit Public Endpoint in WebFlux Security</strong></summary>

**Task**: Configure `ServerHttpSecurity` to permit all GET requests to `/api/public/**` and require authentication for everything else.

```java
// Solution:
@Bean
public SecurityWebFilterChain securityWebFilterChain(ServerHttpSecurity http) {
    return http
            .authorizeExchange(ex -> ex
                    .pathMatchers(HttpMethod.GET, "/api/public/**").permitAll()
                    .anyExchange().authenticated())
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Inject Authentication in Controller</strong></summary>

**Task**: Write an endpoint `/api/whoami` injecting `Mono<Principal>` and returning the principal name.

```java
// Solution:
@GetMapping("/api/whoami")
public Mono<String> whoAmI(Mono<Principal> principalMono) {
    return principalMono.map(Principal::getName).defaultIfEmpty("Anonymous");
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Offload BCrypt Verification to `boundedElastic`</strong></summary>

**Task**: Implement a helper method that verifies a raw password against a hash using `PasswordEncoder` without blocking the calling thread.

```java
// Solution:
public Mono<Boolean> verifyPasswordAsync(PasswordEncoder encoder, String rawPassword, String hash) {
    return Mono.fromCallable(() -> encoder.matches(rawPassword, hash))
            .subscribeOn(Schedulers.boundedElastic());
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Reactive Method Security with `@PreAuthorize`</strong></summary>

**Task**: Secure a service method `deleteRecord(UUID id)` so that only users with authority `ROLE_ADMIN` can invoke it.

```java
// Solution:
@Service
public class RecordService {
    @PreAuthorize("hasRole('ADMIN')")
    public Mono<Void> deleteRecord(UUID id) {
        return Mono.empty();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom Reactive API Key Authentication Manager</strong></summary>

**Task**: Build a `ReactiveAuthenticationManager` that inspects an `ApiKeyAuthenticationToken`, queries a Redis reactive store for the key's permissions, and returns a verified `Authentication` object.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Tenant Reactive Security Filter</strong></summary>

**Task**: Implement a `WebFilter` that extracts a `X-Tenant-ID` header, verifies that the authenticated user belongs to that tenant using an R2DBC repository, and writes an enriched `TenantSecurityContext` into the Reactor `Context`.
</details>

---

## 12. Summary & Quick Reference

- **Key Rule**: NEVER use `SecurityContextHolder`. Use `ReactiveSecurityContextHolder`.
- **Context Storage**: Security credentials live in the immutable **Reactor `Context`**.
- **Configuration**: Use `ServerHttpSecurity` to build `SecurityWebFilterChain`.
- **Method Security**: Enable via `@EnableReactiveMethodSecurity`.
- **CPU Offloading**: Offload heavy password hashing (BCrypt) to `Schedulers.boundedElastic()`.

---

## 13. Further Reading

- [Spring Security Official Reference: Reactive Applications](https://docs.spring.io/spring-security/reference/reactive/index.html)
- [Project Reactor Context Documentation](https://projectreactor.io/docs/core/release/reference/#context)
- [OAuth2 Resource Server with Spring WebFlux](https://docs.spring.io/spring-security/reference/reactive/oauth2/resource-server/jwt.html)
- [Securing Reactive Microservices Best Practices](https://spring.io/guides)
