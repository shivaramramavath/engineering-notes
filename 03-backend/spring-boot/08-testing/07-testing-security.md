# 07. Security Testing & Mock Security Contexts in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, `03-spring-boot-test-and-slices.md`, `05-MockMvc-and-RestTestClient.md` |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Spring Security Test 6.4 |

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
Security configurations are among the most fragile components of enterprise applications. A minor typo in an ant matcher, a misplaced `@PreAuthorize` annotation, or an unhandled OAuth2 claim can silently expose sensitive financial or healthcare records to unauthenticated users without failing compilation or throwing startup exceptions.

Manually logging into an application via a browser or issuing cURL commands with real tokens is slow, non-deterministic, and incompatible with CI/CD automation.

The **Spring Security Test** framework provides declarative tools to test authentication rules, authorization boundaries, CSRF token validation, and OAuth2/JWT token parsing in automated tests without requiring live authentication servers or real user databases.

### Historical Evolution
- **Spring Security 2.x - 3.x**: Developers manually set `SecurityContextHolder.getContext().setAuthentication(...)` in test setup methods, which frequently leaked state across test executions.
- **Spring Security 4.x - 5.x**: Introduced `@WithMockUser`, `@WithUserDetails`, and `SecurityMockMvcRequestPostProcessors` (`csrf()`, `user()`, `oauth2Login()`).
- **Spring Security 6.x / 7.x**: Added first-class support for OAuth 2.0 Resource Server testing via `.with(jwt())`, fine-grained reactive security testing via `WebTestClient.mutateWith()`, and modernized test execution listeners.

### Real-World Relevance
Automated security tests act as executable compliance contracts, ensuring that role-based permissions, multi-tenant boundaries, and token scopes remain intact through routine refactorings and dependency upgrades.

---

## 2. Core Concepts

### Architectural Contracts
- **`@WithMockUser`**: Injects an authenticated `UsernamePasswordAuthenticationToken` directly into `SecurityContextHolder` before the test executes, allowing quick configuration of `username`, `roles`, and `authorities`.
- **`@WithAnonymousUser`**: Explicitly establishes an anonymous authentication context (`AnonymousAuthenticationToken`), verifying that public routes work and protected routes are rejected.
- **`@WithUserDetails`**: Simulates authentication by loading a real user from your application's `UserDetailsService` bean.
- **`@WithSecurityContext`**: Meta-annotation allowing developers to create custom test annotations (e.g., `@WithCompanyAdmin`) backed by a custom `WithSecurityContextFactory`.
- **`SecurityMockMvcRequestPostProcessors`**: Factory providing request transformers:
  - `csrf()`: Injects valid CSRF tokens into MockMvc requests.
  - `jwt()`: Injects a mock `JwtAuthenticationToken` with custom claims and authorities.
  - `oauth2Login()`: Injects an `OAuth2AuthenticationToken` simulating social login.
  - `httpBasic()`: Attaches HTTP Basic authorization headers.

### The Mental Model
```
[ MockMvc Test ]
       │
       ├── @WithMockUser(username = "clerk", roles = "STAFF")
       │         │
       │         ▼
       │   [ WithSecurityContextTestExecutionListener ]
       │         └─ Sets SecurityContextHolder BEFORE test method runs!
       │
       ├── mockMvc.perform(post("/api/refunds").with(csrf()))
       │         │
       │         ▼
       │   [ SecurityFilterChainProxy ]
       │         ├─ CsrfFilter: Validates injected CSRF token (PASS)
       │         ├─ AuthorizationFilter: Checks hasRole('MANAGER')
       │         └─ Denied! Returns HTTP 403 Forbidden
       │
       └── andExpect(status().isForbidden())
```

### Invariants
1. `WithSecurityContextTestExecutionListener` wipes and clears `SecurityContextHolder` after every test method completes, guaranteeing zero cross-test context pollution.
2. In `@WebMvcTest` slice tests, your custom security configuration is **not** imported by default unless explicitly registered via `@Import(SecurityConfig.class)`.
3. `@WithMockUser(roles = "ADMIN")` automatically prepends `ROLE_` (resulting in `ROLE_ADMIN`). If you use `authorities = "ADMIN"`, the string is kept as-is without prefixing.

---

## 3. How It Works Internally

### Security Context Injection Pipeline
1. Before a test method executes, JUnit invokes registered `TestExecutionListener` instances.
2. `WithSecurityContextTestExecutionListener` inspects the test method and class for `@WithSecurityContext` annotations.
3. Finds the configured `WithSecurityContextFactory` (e.g., `WithMockUserSecurityContextFactory`).
4. The factory instantiates a fresh `SecurityContext`, populates it with the desired `Authentication` token, and returns it.
5. The listener invokes `SecurityContextHolder.setContext(securityContext)`.
6. Test method executes; MockMvc dispatches the request through the `SecurityFilterChain`.
7. After the test method completes, `afterTestMethod()` invokes `SecurityContextHolder.clearContext()`.

---

## 4. Syntax & Usage

### 1. Minimal Testing Starter (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.security</groupId>
    <artifactId>spring-security-test</artifactId>
    <scope>test</scope>
</dependency>
```

### 2. Standard Role & Authorization Testing
```java
package com.example.web;

import com.example.security.SecurityConfig;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;
import org.springframework.security.test.context.support.WithAnonymousUser;
import org.springframework.security.test.context.support.WithMockUser;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.security.test.web.servlet.request.SecurityMockMvcRequestPostProcessors.csrf;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.delete;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(AdminController.class)
@Import(SecurityConfig.class)
class AdminControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    @WithAnonymousUser
    void anonymousUserShouldReceive401() throws Exception {
        mockMvc.perform(get("/api/admin/metrics"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    @WithMockUser(roles = "USER")
    void standardUserShouldReceive403() throws Exception {
        mockMvc.perform(delete("/api/admin/users/42").with(csrf()))
            .andExpect(status().isForbidden());
    }

    @Test
    @WithMockUser(roles = "ADMIN")
    void adminUserShouldSucceed() throws Exception {
        mockMvc.perform(delete("/api/admin/users/42").with(csrf()))
            .andExpect(status().isOk());
    }
}
```

---

## 5. Practical Examples

### Example 1: Testing OAuth2 JWT Resource Server Endpoints with `.with(jwt())`
Testing endpoints protected by JWT scopes, claims, and tenant identifiers.

```java
package com.example.web;

import com.example.security.ResourceServerConfig;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.security.test.web.servlet.request.SecurityMockMvcRequestPostProcessors.jwt;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(BillingController.class)
@Import(ResourceServerConfig.class)
class BillingControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldDenyAccessIfBillingScopeIsMissing() throws Exception {
        mockMvc.perform(get("/api/v1/invoices")
                .with(jwt().authorities(new SimpleGrantedAuthority("SCOPE_profile"))))
            .andExpect(status().isForbidden());
    }

    @Test
    void shouldAllowAccessWhenBillingScopeAndTenantPresent() throws Exception {
        mockMvc.perform(get("/api/v1/invoices")
                .with(jwt()
                    .jwt(jwt -> jwt
                        .subject("user-999")
                        .claim("tenant_id", "ACME-LOGISTICS")
                    )
                    .authorities(new SimpleGrantedAuthority("SCOPE_billing:read"))
                ))
            .andExpect(status().isOk());
    }
}
```

### Example 2: Reusable Domain Security Meta-Annotations
Creating custom annotations like `@WithTenantManager` to eliminate repetitive security test boilerplate.

```java
package com.example.security.test;

import org.springframework.security.test.context.support.WithSecurityContext;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
@WithSecurityContext(factory = WithTenantManagerSecurityContextFactory.class)
public @interface WithTenantManager {
    String username() default "manager_bob";
    String tenant() default "ALPHA_CORP";
}
```
```java
package com.example.security.test;

import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContext;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.test.context.support.WithSecurityContextFactory;

import java.util.List;

public class WithTenantManagerSecurityContextFactory 
        implements WithSecurityContextFactory<WithTenantManager> {

    @Override
    public SecurityContext createSecurityContext(WithTenantManager annotation) {
        SecurityContext context = SecurityContextHolder.createEmptyContext();

        TenantPrincipal principal = new TenantPrincipal(
            annotation.username(),
            annotation.tenant()
        );

        var token = new UsernamePasswordAuthenticationToken(
            principal,
            "mock-password",
            List.of(new SimpleGrantedAuthority("ROLE_MANAGER"))
        );

        context.setAuthentication(token);
        return context;
    }

    public record TenantPrincipal(String username, String tenant) {}
}
```

### Example 3: Edge Case – Testing Method Security Directly on Service Classes
Testing `@PreAuthorize` business logic on service beans without launching HTTP or MockMvc.

```java
package com.example.service;

import com.example.security.MethodSecurityConfig;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.context.annotation.Import;
import org.springframework.security.access.AccessDeniedException;
import org.springframework.security.test.context.support.WithMockUser;

import static org.assertj.core.api.Assertions.assertThatNoException;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@SpringBootTest
@Import(MethodSecurityConfig.class)
class DocumentServiceMethodSecurityTest {

    @Autowired
    private DocumentService documentService;

    @Test
    @WithMockUser(username = "guest", roles = "GUEST")
    void guestCannotDeleteDocuments() {
        assertThatThrownBy(() -> documentService.deleteDocument(101L))
            .isInstanceOf(AccessDeniedException.class);
    }

    @Test
    @WithMockUser(username = "admin", roles = "ADMIN")
    void adminCanDeleteDocuments() {
        assertThatNoException().isThrownBy(() -> documentService.deleteDocument(101L));
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Forgetting `.with(csrf())` on mutating MockMvc calls (`POST`, `PUT`, `DELETE`), causing tests to fail with HTTP 403 Forbidden due to CSRF rejection rather than authorization failures.

```java
// BUGGY: Fails with 403 Forbidden due to missing CSRF token, NOT role permissions!
@Test
@WithMockUser(roles = "ADMIN")
void testCreateProduct() throws Exception {
    mockMvc.perform(post("/api/products")
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"name\":\"Gadget\"}"))
        .andExpect(status().isCreated()); // FAILS with 403!
}
```

**Production Fix**:
Always append `.with(csrf())` to mutating requests when CSRF is enabled.

```java
// SECURE & ACCURATE: Injects CSRF token so role authorization is tested accurately
@Test
@WithMockUser(roles = "ADMIN")
void testCreateProduct() throws Exception {
    mockMvc.perform(post("/api/products")
            .with(csrf())
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"name\":\"Gadget\"}"))
        .andExpect(status().isCreated());
}
```

### Example 5: High-Performance Reactive WebFlux Security Testing
Using `WebTestClient` with `SecurityMockServerConfigurers`.

```java
package com.example.reactive;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.reactive.WebFluxTest;
import org.springframework.test.web.reactive.server.WebTestClient;

import static org.springframework.security.test.web.reactive.server.SecurityMockServerConfigurers.csrf;
import static org.springframework.security.test.web.reactive.server.SecurityMockServerConfigurers.mockUser;

@WebFluxTest(controllers = ReactiveAccountController.class)
class ReactiveAccountSecurityTest {

    @Autowired
    private WebTestClient client;

    @Test
    void operatorCanUpdateAccount() {
        client.mutateWith(mockUser().roles("OPERATOR"))
            .mutateWith(csrf())
            .post().uri("/api/accounts/freeze")
            .exchange()
            .expectStatus().isOk();
    }
}
```

---

## 6. Best Practices

1. **Test Both Positive and Negative Cases**: For every route, test that unauthenticated users receive 401, unauthorized roles receive 403, and authorized roles receive 200/201.
2. **Always Use `.with(csrf())` on Mutating Requests**: Ensure `POST`, `PUT`, and `DELETE` requests include CSRF tokens unless CSRF is explicitly disabled for that endpoint.
3. **Use `.with(jwt())` for Resource Servers**: Accurately test OAuth2 claims, scopes, and expiration using `jwt()` post-processors rather than generic `@WithMockUser`.
4. **Build Domain-Specific Meta-Annotations**: Replace messy repetitive setup with clean annotations like `@WithTenantAdmin` or `@WithAuditor`.
5. **Import Security Config into Slices**: Remember that `@WebMvcTest` ignores custom security configurations by default; always add `@Import(WebSecurityConfig.class)`.
6. **Use `@WithAnonymousUser` for Public Routes**: Verify that endpoints meant to be public (`/health`, `/public/**`) remain accessible to unauthenticated callers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Omitting `.with(csrf())` on `POST` | Request fails with 403 Forbidden due to CSRF rejection. | Append `.with(csrf())` to all mutating MockMvc calls. |
| Double `ROLE_` prefix | `@WithMockUser(roles = "ROLE_ADMIN")` results in `ROLE_ROLE_ADMIN`. | Use `roles = "ADMIN"` or `authorities = "ROLE_ADMIN"`. |
| Missing `@Import(SecurityConfig.class)` | `@WebMvcTest` runs with default security auto-config, ignoring custom rules. | Add `@Import(WebSecurityConfig.class)` to the test class. |
| Testing JWT endpoints with `@WithMockUser` | Controller expects `JwtAuthenticationToken`; fails with `ClassCastException`. | Use `.with(jwt())` from `SecurityMockMvcRequestPostProcessors`. |
| Testing method security without proxies | Plain unit tests don't activate Spring AOP proxies; `@PreAuthorize` is ignored. | Test method security with `@SpringBootTest`. |
| State leakage across tests | Manually setting `SecurityContextHolder` without clearing it leaks state. | Use `@WithMockUser` which cleans up automatically. |

---

## 8. Performance & Security Notes

- **Zero Database Overhead**: `@WithMockUser` and `.with(jwt())` construct authentication tokens purely in memory without issuing database queries or network HTTP calls.
- **Context Caching Compatibility**: Tests using `@WithMockUser` do **not** dirty the `ApplicationContext`, allowing contexts to remain cached across all test runs.
- **Virtual Threads Integration**: Security filter execution in tests runs seamlessly on Java 21 Virtual Threads without thread pool exhaustion.

---

## 9. Comparison / When to Use What

| Security Testing Utility | Scope | Backing Mechanism | Best Use Case |
|---|---|---|---|
| **`@WithMockUser`** | Method / Class | In-memory `UsernamePasswordAuthenticationToken` | Simple role checks on MVC controllers or services |
| **`@WithUserDetails`** | Method / Class | Real `UserDetailsService` database query | Custom `UserDetails` properties testing |
| **`.with(jwt())`** | Request-level | `JwtAuthenticationToken` with claims | OAuth2 Resource Server APIs |
| **`.with(oauth2Login())`**| Request-level | `OAuth2AuthenticationToken` | Social login callback testing |
| **`.with(csrf())`** | Request-level | `CsrfToken` injected into request | Mutating HTTP requests (`POST`, `PUT`, `DELETE`) |

---

## 10. Interview Questions

<details>
<summary>1. Why does `@WithMockUser(roles = "ADMIN")` assign the authority `ROLE_ADMIN`?</summary>

**Answer:**
Spring Security enforces a convention where roles are stored as granted authorities prefixed with `ROLE_`. The `roles` attribute in `@WithMockUser` automatically prepends `ROLE_` to each string for developer convenience. If you need to set raw authorities without the prefix (e.g., OAuth2 scopes like `SCOPE_read`), you must use the `authorities` attribute: `@WithMockUser(authorities = "SCOPE_read")`.
</details>

<details>
<summary>2. How does `SecurityMockMvcRequestPostProcessors.jwt()` work under the hood?</summary>

**Answer:**
The `jwt()` request post-processor constructs an in-memory `Jwt` instance populated with standard headers, configurable claims, and timestamps. It wraps this inside a `JwtAuthenticationToken` and establishes it in the request's security context. When the request is dispatched through the filter chain, Spring Security treats the request as fully authenticated via a valid OAuth2 Bearer token without contacting an Identity Provider or fetching JWKS keys.
</details>

<details>
<summary>3. Why does an unauthenticated request to a protected endpoint return HTTP 401 in REST APIs but HTTP 302 in MVC web apps?</summary>

**Answer:**
In browser-based applications configured with form login (`http.formLogin()`), the default `AuthenticationEntryPoint` is `LoginUrlAuthenticationEntryPoint`, which issues an HTTP 302 redirect to the `/login` page. In stateless REST APIs configured with HTTP Basic or OAuth2 Resource Server, the entry point returns HTTP 401 Unauthorized with a `WWW-Authenticate` header.
</details>

<details>
<summary>4. What is the difference between `@WithMockUser` and `@WithSecurityContext`?</summary>

**Answer:**
`@WithMockUser` is a pre-built convenience annotation that populates a standard `UsernamePasswordAuthenticationToken`.
`@WithSecurityContext` is the underlying meta-annotation used to create custom test annotations. It specifies a custom `WithSecurityContextFactory` class that can construct arbitrary, domain-specific `Authentication` objects (e.g., custom principal records with tenant IDs, department codes, or multi-factor authentication flags).
</details>

<details>
<summary>5. How can you test method security (`@PreAuthorize`) in a service layer without HTTP?</summary>

**Answer:**
By running a `@SpringBootTest` that loads the service bean and decorating the test methods with `@WithMockUser(roles = "...")`. When the test invokes the service method directly, Spring AOP proxies intercept the call, evaluate the SpEL expression against the ambient `SecurityContextHolder`, and either permit the execution or throw `AccessDeniedException`.
</details>

<details>
<summary>6. Why is `@WithAnonymousUser` useful if tests run unauthenticated by default?</summary>

**Answer:**
In some test environments, surrounding test configuration or class-level annotations may set a default mock user. Using `@WithAnonymousUser` on a specific test method explicitly overrides any ambient or class-level credentials, ensuring the method runs with an `AnonymousAuthenticationToken` to verify that unauthenticated access is rejected.
</details>

<details>
<summary>7. How do you verify that an endpoint returns a specific security header in MockMvc?</summary>

**Answer:**
Using `MockMvcResultMatchers.header()`:
```java
mockMvc.perform(get("/api/public"))
    .andExpect(header().string("X-Content-Type-Options", "nosniff"))
    .andExpect(header().string("X-Frame-Options", "DENY"));
```
</details>

<details>
<summary>8. How do you test reactive security using `WebTestClient`?</summary>

**Answer:**
By using `SecurityMockServerConfigurers`:
```java
webTestClient
    .mutateWith(mockUser().roles("ADMIN"))
    .mutateWith(csrf())
    .get().uri("/api/admin/data")
    .exchange()
    .expectStatus().isOk();
```
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Role Authorization Test (Easy)</summary>

**Task**: Write a MockMvc test verifying that a user with role `OPERATOR` can access `GET /api/system/status`.

**Solution**:
```java
@Test
@WithMockUser(roles = "OPERATOR")
void operatorCanAccessStatus() throws Exception {
    mockMvc.perform(get("/api/system/status"))
        .andExpect(status().isOk());
}
```
</details>

<details>
<summary>Exercise 2: Testing Anonymous Rejection (Easy)</summary>

**Task**: Write a test verifying that an unauthenticated user attempting to `DELETE /api/items/1` receives HTTP 401 Unauthorized.

**Solution**:
```java
@Test
@WithAnonymousUser
void anonymousUserCannotDeleteItem() throws Exception {
    mockMvc.perform(delete("/api/items/1"))
        .andExpect(status().isUnauthorized());
}
```
</details>

<details>
<summary>Exercise 3: POST Request with CSRF and Role (Medium)</summary>

**Task**: Write a MockMvc test verifying that a user with role `MANAGER` can execute `POST /api/approvals` when supplying a CSRF token.

**Solution**:
```java
@Test
@WithMockUser(roles = "MANAGER")
void managerCanApproveWithCsrf() throws Exception {
    mockMvc.perform(post("/api/approvals")
            .with(csrf())
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"requestId\":101}"))
        .andExpect(status().isOk());
}
```
</details>

<details>
<summary>Exercise 4: Mocking OAuth2 JWT Claims (Medium)</summary>

**Task**: Write a test verifying that an endpoint `/api/profile` returns the user's email extracted from the JWT `email` claim.

**Solution**:
```java
@Test
void shouldReturnProfileFromJwtEmailClaim() throws Exception {
    mockMvc.perform(get("/api/profile")
            .with(jwt()
                .jwt(jwt -> jwt.claim("email", "alice@example.com"))
            ))
        .andExpect(status().isOk())
        .andExpect(jsonPath("$.email").value("alice@example.com"));
}
```
</details>

<details>
<summary>Exercise 5: Custom Security Meta-Annotation (Hard)</summary>

**Task**: Implement an annotation `@WithFinanceUser(department="AUDIT")` and its corresponding `WithSecurityContextFactory` that sets a custom `FinancePrincipal` in `SecurityContextHolder`.

**Solution**:
```java
public class WithFinanceUserFactory implements WithSecurityContextFactory<WithFinanceUser> {
    @Override
    public SecurityContext createSecurityContext(WithFinanceUser annotation) {
        SecurityContext context = SecurityContextHolder.createEmptyContext();
        FinancePrincipal principal = new FinancePrincipal("finance-user", annotation.department());
        var auth = new UsernamePasswordAuthenticationToken(
            principal, "creds", List.of(new SimpleGrantedAuthority("ROLE_FINANCE"))
        );
        context.setAuthentication(auth);
        return context;
    }
}
```
</details>

<details>
<summary>Exercise 6: Testing Method Security in Service Layer (Hard)</summary>

**Task**: Write an integration test using `@SpringBootTest` verifying that invoking `ReportService.generateConfidentialReport()` throws `AccessDeniedException` for `ROLE_USER` and succeeds for `ROLE_EXECUTIVE`.

**Solution**:
```java
@SpringBootTest
class ReportServiceSecurityTest {

    @Autowired
    private ReportService reportService;

    @Test
    @WithMockUser(roles = "USER")
    void standardUserDenied() {
        assertThrows(AccessDeniedException.class, () -> reportService.generateConfidentialReport());
    }

    @Test
    @WithMockUser(roles = "EXECUTIVE")
    void executiveAllowed() {
        assertDoesNotThrow(() -> reportService.generateConfidentialReport());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`@WithMockUser`**: Injects an in-memory authenticated mock user.
- **Append `.with(csrf())`**: Mandatory for mutating requests (`POST`, `PUT`, `DELETE`) when CSRF is enabled.
- **Use `.with(jwt())` for Resource Servers**: Accurately simulates OAuth2 Bearer tokens with custom claims.
- **Meta-Annotations for Clean Tests**: Build custom annotations (`@WithAdminUser`) via `@WithSecurityContext`.
- **Automatic Cleanup**: Spring Security Test guarantees `SecurityContextHolder` is cleared after every test.

---

## 13. Further Reading
- [Spring Security Reference: Testing Support](https://docs.spring.io/spring-security/reference/servlet/test/index.html)
- [Spring Security Reference: MockMvc Integration](https://docs.spring.io/spring-security/reference/servlet/test/mockmvc/index.html)
- [Spring Security Reference: Testing OAuth2](https://docs.spring.io/spring-security/reference/servlet/test/mockmvc/oauth2.html)
- [Spring Security Reference: Method Security Testing](https://docs.spring.io/spring-security/reference/servlet/test/method.html)
- [JUnit 5 User Guide](https://junit.org/junit5/docs/current/user-guide/)
