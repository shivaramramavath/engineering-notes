# Spring Security Configuration Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Spring Security 6+ & Lambda DSL Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Advanced |
| **Prerequisites** | Java 21, Spring Security Architecture, JWT, OAuth 2.0 |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | Spring Security 6.4+, JJWT, Nimbus JOSE |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Authentication Strategies](#9-comparison-table-authentication-strategies)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Spring Security 6+ deprecates legacy adapter classes (such as `WebSecurityConfigurerAdapter`) in favor of component-based bean registration and functional Lambda DSL configuration. Modern production microservices rely on stateless JWT authentication, OAuth 2.0 Resource Server capabilities, fine-grained CORS/CSRF configurations, and declarative method-level authorization.

This cheatsheet provides immediate, copy-pasteable Lambda DSL configuration recipes that satisfy current industry standards.

---

## 2. Core Concepts
- **`SecurityFilterChain`**: Bean-based filter chain orchestrating request inspection, authentication, and authorization.
- **Lambda DSL**: Modern fluent builder pattern (`http.authorizeHttpRequests(auth -> auth...)`) eliminating chaining syntax ambiguity.
- **Stateless Session Management**: `SessionCreationPolicy.STATELESS` ensures zero HTTP session cookies in REST environments.
- **Method Security**: `@PreAuthorize("hasRole('ADMIN')")` powered by `@EnableMethodSecurity`.
- **OAuth 2.0 Resource Server**: Native JWT token validation using asymmetric public keys or JWK sets.

---

## 3. Internals & Architecture
Spring Security Filter Chain Execution Sequence:

```
[Inbound HTTP Request]
          |
          v
+-------------------------------------------------------------+
|                     FilterChainProxy                        |
|  1. CorsFilter (Validates Origin, Headers, Methods)         |
|  2. CsrfFilter (Validates CSRF token or bypassed)           |
|  3. BearerTokenAuthenticationFilter (Extracts 'Bearer ...') |
|     -> Calls AuthenticationManager / JwtAuthenticationProvider|
|  4. SecurityContextHolderFilter (Binds Authentication)      |
|  5. AuthorizationFilter (Evaluates RequestMatcher rules)    |
+-------------------------------------------------------------+
          |
          v
[DispatcherServlet -> Controller Endpoint]
          |
          v
[Method Security Interceptor: Evaluates @PreAuthorize / SpEL]
```

---

## 4. Syntax & Categorized Reference

### 4.1 SecurityFilterChain Lambda DSL Template
```java
@Configuration
@EnableWebSecurity
@EnableMethodSecurity
public class SecurityConfiguration {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(AbstractHttpConfigurer::disable)
            .cors(cors -> cors.configurationSource(corsConfigurationSource()))
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/v1/public/**", "/actuator/health/**").permitAll()
                .requestMatchers("/api/v1/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(Customizer.withDefaults()))
            .build();
    }
}
```

### 4.2 Common Request Matchers & Authorizations
| Matcher Syntax | Description |
|---|---|
| `.requestMatchers(HttpMethod.GET, "/api/v1/items/**").permitAll()` | Public GET requests |
| `.requestMatchers("/api/v1/users/**").hasAuthority("SCOPE_user:read")` | Requires specific JWT scope |
| `.requestMatchers("/admin/**").hasRole("ADMIN")` | Requires `ROLE_ADMIN` |
| `.anyRequest().authenticated()` | Enforces authentication on all unspecified routes |

---

## 5. 5 Practical Production Examples

### Example 1: Full Stateless JWT Resource Server SecurityFilterChain
```java
@Configuration
@EnableWebSecurity
@EnableMethodSecurity
public class ProductionSecurityConfig {

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http, CorsConfigurationSource corsSource) throws Exception {
        http
            .csrf(AbstractHttpConfigurer::disable)
            .cors(cors -> cors.configurationSource(corsSource))
            .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/v1/auth/**", "/swagger-ui/**", "/v3/api-docs/**").permitAll()
                .requestMatchers("/actuator/health", "/actuator/prometheus").permitAll()
                .requestMatchers("/api/v1/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(jwt -> jwt.jwtAuthenticationConverter(jwtAuthenticationConverter())));

        return http.build();
    }

    @Bean
    public JwtAuthenticationConverter jwtAuthenticationConverter() {
        JwtGrantedAuthoritiesConverter grantedAuthoritiesConverter = new JwtGrantedAuthoritiesConverter();
        grantedAuthoritiesConverter.setAuthoritiesClaimName("roles");
        grantedAuthoritiesConverter.setAuthorityPrefix("ROLE_");

        JwtAuthenticationConverter jwtAuthenticationConverter = new JwtAuthenticationConverter();
        jwtAuthenticationConverter.setJwtGrantedAuthoritiesConverter(grantedAuthoritiesConverter);
        return jwtAuthenticationConverter;
    }
}
```

### Example 2: Production CORS Configuration Source
```java
@Bean
public CorsConfigurationSource corsConfigurationSource() {
    CorsConfiguration configuration = new CorsConfiguration();
    configuration.setAllowedOrigins(List.of("https://app.example.com", "https://admin.example.com"));
    configuration.setAllowedMethods(List.of("GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"));
    configuration.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-Correlation-Id"));
    configuration.setExposedHeaders(List.of("Location", "X-Total-Count"));
    configuration.setAllowCredentials(true);
    configuration.setMaxAge(3600L);

    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", configuration);
    return source;
}
```

### Example 3: Declarative Method Security with SpEL
```java
@Service
public class DocumentService {

    @PreAuthorize("hasRole('ADMIN') or #document.ownerId == authentication.name")
    public void deleteDocument(Document document) {
        // Business logic
    }

    @PostAuthorize("returnObject.tenantId == authentication.principal.claims['tenant_id']")
    public Document getDocument(UUID documentId) {
        return documentRepository.findById(documentId).orElseThrow();
    }
}
```

### Example 4: BCrypt Password Encoder Configuration
```java
@Configuration(proxyBeanMethods = false)
public class PasswordEncoderConfig {

    @Bean
    public PasswordEncoder passwordEncoder() {
        return new BCryptPasswordEncoder(12);
    }
}
```

### Example 5: Custom AuthenticationEntryPoint for RFC 9457 Errors
```java
@Component
public class CustomAuthenticationEntryPoint implements AuthenticationEntryPoint {

    private final ObjectMapper objectMapper = new ObjectMapper();

    @Override
    public void commence(HttpServletRequest request, HttpServletResponse response,
                         AuthenticationException authException) throws IOException {
        response.setStatus(HttpStatus.UNAUTHORIZED.value());
        response.setContentType(MediaType.APPLICATION_PROBLEM_JSON_VALUE);

        ProblemDetail problem = ProblemDetail.forStatusAndDetail(
                HttpStatus.UNAUTHORIZED, "Authentication required: " + authException.getMessage());
        problem.setTitle("Unauthorized Access");
        problem.setProperty("path", request.getRequestURI());

        response.getWriter().write(objectMapper.writeValueAsString(problem));
    }
}
```

---

## 6. Best Practices
1. **Disable CSRF on Stateless APIs**: If your API uses JWT bearer tokens and has no session cookies, disable CSRF to prevent unnecessary token handling.
2. **Never Permit Unauthenticated Wildcards Blindly**: Avoid `permitAll()` on broad prefixes like `/api/**`.
3. **Use Method Security for Fine-Grained Authorization**: Let the filter chain handle broad perimeter auth and `@PreAuthorize` enforce business domain authorization.
4. **Always Specify Hash Strength on BCrypt**: Use a work factor of 12 for strong brute-force resistance without overwhelming CPU.
5. **Use `hasRole` vs `hasAuthority` Consistently**: `hasRole("ADMIN")` automatically looks for `ROLE_ADMIN`; `hasAuthority("ADMIN")` looks for exact match `ADMIN`.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Leaving session management default in REST API | Creates HTTP session cookies in memory, breaking horizontal scaling | Set `sessionCreationPolicy(SessionCreationPolicy.STATELESS)` |
| Confusing `hasRole` and `hasAuthority` | Denies access because `ROLE_` prefix is missing or duplicated | Understand `hasRole("ADMIN")` matches `ROLE_ADMIN` |
| Permitting `/actuator/**` unconditionally | Leaks environment variables, heap dumps, and secrets | Expose only `/actuator/health` and `/actuator/prometheus` |
| Defining custom CORS headers in controller methods | Conflicts with Security filter chain CORS filter | Configure a centralized `CorsConfigurationSource` bean |
| Forgetting `@EnableMethodSecurity` | `@PreAuthorize` annotations are ignored; methods are completely unprotected | Add `@EnableMethodSecurity` to configuration class |
| Committing signing secrets in properties files | Critical security compromise | Load private keys or secrets via environment variables |

---

## 8. Performance & Security Considerations
- **BCrypt Work Factor**: Setting strength higher than 14 can consume excessive CPU during spikes, causing denial of service. Keep at 10–12.
- **JWT Verification Caching**: Use Nimbus JWK Set caching to avoid outbound HTTP calls on every incoming JWT request.
- **SecurityContext Propagation**: When using `@Async` or virtual threads, ensure `SecurityContextHolder.setStrategyName(SecurityContextHolder.MODE_INHERITABLETHREADLOCAL)` or use `DelegatingSecurityContextAsyncTaskExecutor`.

---

## 9. Comparison Table: Authentication Strategies

| Feature | Stateful Cookie/Session | Stateless JWT | OAuth 2.0 / OIDC Resource Server |
|---|---|---|---|
| **Storage** | Server Memory / Redis | Client-side Token | Centralized Identity Provider (IdP) |
| **Scalability** | Requires sticky sessions / Redis | Completely horizontally scalable | Enterprise SSO scalable |
| **Revocation** | Instant (delete session) | Difficult (requires blocklist) | Centralized token revocation |
| **Complexity** | Low | Moderate | High (JWKS / Keycloak / Auth0) |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. How does the modern Lambda DSL replace WebSecurityConfigurerAdapter?</summary>
In Spring Security 6+, developers define a `@Bean` returning `SecurityFilterChain` where `HttpSecurity` is configured using fluent lambda callbacks, promoting immutability and modular bean assembly.
</details>

<details>
<summary>2. What is the difference between `hasRole('ADMIN')` and `hasAuthority('ADMIN')`?</summary>
`hasRole('ADMIN')` automatically prepends the prefix `ROLE_`, checking for authority `ROLE_ADMIN`. `hasAuthority('ADMIN')` checks for the exact raw string `ADMIN`.
</details>

<details>
<summary>3. Why is CSRF protection disabled in stateless REST APIs?</summary>
CSRF attacks rely on browsers automatically attaching stored session cookies to cross-origin requests. Since stateless REST APIs authenticate via the `Authorization: Bearer <token>` header, browsers cannot attach tokens automatically.
</details>

<details>
<summary>4. What is the role of `SecurityContextHolder`?</summary>
`SecurityContextHolder` provides access to the `SecurityContext`, which contains the `Authentication` object of the currently authenticated principal, typically backed by `ThreadLocal`.
</details>

<details>
<summary>5. How does Spring Security handle CORS requests?</summary>
Preflight `OPTIONS` requests are intercepted by `CorsFilter` early in the security chain. If allowed, appropriate `Access-Control-Allow-*` headers are attached; otherwise HTTP 403/401 is returned.
</details>

<details>
<summary>6. What does `@EnableMethodSecurity` replace in Spring Security 6?</summary>
It replaces the legacy `@EnableGlobalMethodSecurity`, enabling JSR-250 (`@RolesAllowed`), `@Secured`, and Spring Expression Language (`@PreAuthorize`, `@PostAuthorize`) by default.
</details>

<details>
<summary>7. How do you propagate SecurityContext across Virtual Threads?</summary>
Wrap execution using `DelegatingSecurityContextExecutorService` or configure Spring's task execution auto-configuration to propagate the context automatically.
</details>

<details>
<summary>8. How do you customize JWT claims extraction in Spring Security?</summary>
Register a custom `JwtAuthenticationConverter` that parses proprietary claims (like realm roles) and converts them into `GrantedAuthority` collections.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Configure a stateless SecurityFilterChain</summary>
Write a configuration class disabling CSRF, enabling stateless sessions, and protecting `/api/v1/private/**`.
</details>

<details>
<summary>Exercise 2: Implement a custom JWT claims converter</summary>
Extract a nested claim `realm_access.roles` from a Keycloak JWT and map it to Spring Security `ROLE_` authorities.
</details>

<details>
<summary>Exercise 3: Write a controller method with SpEL authorization</summary>
Create an endpoint `@PreAuthorize("#userId == authentication.name or hasRole('ADMIN')")`.
</details>

<details>
<summary>Exercise 4: Test security using @WithMockUser</summary>
Write a MockMvc test verifying that an unauthenticated user gets 401 Unauthorized and an admin gets 200 OK.
</details>

<details>
<summary>Exercise 5: Configure centralized CORS rules</summary>
Create a `CorsConfigurationSource` allowing `https://dashboard.example.com` with specific headers.
</details>

<details>
<summary>Exercise 6: Implement a custom AuthenticationEntryPoint</summary>
Produce an RFC 9457 `ProblemDetail` response with status 401 and custom error code whenever authentication fails.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    SPRING SECURITY CHEAT SHEET                                  |
+---------------------------------------------------------------------------------+
| CHAIN          | @Bean SecurityFilterChain filterChain(HttpSecurity http)       |
| STATELESS      | sessionManagement(sm -> sm.sessionCreationPolicy(STATELESS))    |
| CSRF           | csrf(AbstractHttpConfigurer::disable)                          |
| CORS           | cors(c -> c.configurationSource(corsSource))                   |
| MATCHERS       | requestMatchers("/public/**").permitAll()                      |
| ROLES          | requestMatchers("/admin/**").hasRole("ADMIN")                  |
| OAUTH2 JWT     | oauth2ResourceServer(oauth2 -> oauth2.jwt(Customizer...))       |
| METHOD SEC     | @EnableMethodSecurity, @PreAuthorize("hasRole('ADMIN')")       |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Security Official Reference Documentation](https://docs.spring.io/spring-security/reference/)
- [OAuth 2.0 Resource Server JWT Architecture](https://docs.spring.io/spring-security/reference/servlet/oauth2/resource-server/jwt.html)
- [OWASP REST Security Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/REST_Security_Cheat_Sheet.html)
