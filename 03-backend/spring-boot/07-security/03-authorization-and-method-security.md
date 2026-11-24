# Authorization & Method Security: @EnableMethodSecurity, SpEL Expressions & Interceptors

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, Spring AOP Proxying (`01-core-spring/05-spring-aop-and-proxies.md`), SpEL syntax |
| **Estimated Reading Time** | 30 minutes |
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
   - [5.1 Fine-Grained Access Control with @PreAuthorize and SpEL](#51-fine-grained-access-control-with-preauthorize-and-spel)
   - [5.2 Domain-Object Ownership Verification with @PostAuthorize](#52-domain-object-ownership-verification-with-postauthorize)
   - [5.3 Collection Filtering via @PostFilter and @PreFilter](#53-collection-filtering-via-postfilter-and-prefilter)
   - [5.4 Anti-Pattern: Relying Solely on URL-Based Security for Business Logic](#54-anti-pattern-relying-solely-on-url-based-security-for-business-logic)
   - [5.5 Custom Method Security Bean Evaluator](#55-custom-method-security-bean-evaluator)
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
URL-based authorization (`http.authorizeHttpRequests()`) operates at the HTTP edge. While it excels at coarse-grained routing (e.g. restricting all `/admin/**` endpoints to administrators), it cannot enforce fine-grained business domain rules. For example, verifying whether the calling user is the *owner* of a specific bank account, whether an editor has permission to modify an unpublished draft, or filtering a list of documents based on a user's multi-tenant department. Relying solely on URL security leads to severe authorization bypass vulnerabilities (BOLA: Broken Object Level Authorization, OWASP API1).

### Historical Evolution
Earlier versions of Spring Security used `@EnableGlobalMethodSecurity` with legacy JSR-250 (`@RolesAllowed`) or `@Secured` annotations. Spring Security 6 completely overhauled method security by introducing **`@EnableMethodSecurity`** (enabled by default in modern Spring Boot starters) and replacing heavyweight interceptors with streamlined **`AuthorizationManagerBeforeMethodInterceptor`** and **`AuthorizationManagerAfterMethodInterceptor`**.

### Real-World Relevance
Method-level security protects your application regardless of how a method is invoked—whether from a REST controller, a scheduled background `@Scheduled` task, an asynchronous `@Async` executor, or an incoming Kafka/RabbitMQ event consumer. It embeds authorization rules directly alongside domain service interfaces.

---

## 2. Core Concepts

### Method Security Annotations
1. **`@PreAuthorize("expression")`**: Evaluates authorization *before* the method executes. If the SpEL expression evaluates to `false`, the method execution is aborted immediately and an `AccessDeniedException` is thrown.
2. **`@PostAuthorize("expression")`**: Executes the method, then evaluates authorization *after* completion. It can inspect the return value using the built-in SpEL variable `returnObject`. If unauthorized, an `AccessDeniedException` is thrown and the return value is withheld.
3. **`@PreFilter("filterTarget = 'items', filterObject.owner == authentication.name")`**: Filters incoming collection or array arguments before method execution.
4. **`@PostFilter("filterObject.department == authentication.principal.department")`**: Filters outgoing collection or stream return values, automatically stripping unauthorized elements before returning to the caller.

### Common SpEL Security Expressions
- `hasRole('ADMIN')`: Verifies the presence of authority `"ROLE_ADMIN"`.
- `hasAuthority('SCOPE_READ')`: Verifies exact string authority match.
- `hasAnyRole('MANAGER', 'ADMIN')`: Matches any of the specified roles.
- `authentication.principal`: References the current authenticated principal object.
- `#paramName`: References method arguments by name (e.g., `#userId == authentication.name`).
- `@customBean.hasAccess(#id)`: Delegates evaluation to a Spring bean method.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Method Security Interception Flow                          |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller: orderService.cancelOrder(orderId)                                         |
|       |                                                                            |
|       v                                                                            |
|  OrderService$$SpringCGLIB (Spring AOP Proxy)                                      |
|       |                                                                            |
|       v                                                                            |
|  AuthorizationManagerBeforeMethodInterceptor (@PreAuthorize)                      |
|       |                                                                            |
|       +---> Evaluates SpEL: @securityService.isOrderOwner(#orderId)                |
|       |     - Checks SecurityContextHolder.getContext().getAuthentication()        |
|       |     - Reads method argument: orderId                                       |
|       |                                                                            |
|       |-- If FALSE: Throws AccessDeniedException! (Halts execution)                |
|       |                                                                            |
|       v-- If TRUE: Invokes target method                                           |
|  Target Method: OrderServiceImpl.cancelOrder(orderId)                              |
|       |                                                                            |
|       v                                                                            |
|  AuthorizationManagerAfterMethodInterceptor (@PostAuthorize)                       |
|       |                                                                            |
|       +---> Evaluates SpEL: returnObject.customerEmail == authentication.name      |
|       |                                                                            |
|       |-- If FALSE: Throws AccessDeniedException! (Result withheld)                |
|       |                                                                            |
|       v-- If TRUE: Returns result to caller                                        |
|  Caller receives OrderDto                                                          |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Enabling Method Security
In modern Spring Boot 3.4+ / Spring Security 6+, add `@EnableMethodSecurity` to any `@Configuration` class:
```java
@Configuration
@EnableMethodSecurity // Pre/PostAuthorize enabled by default!
public class MethodSecurityConfig {
}
```

### Basic Method Annotations
```java
@Service
public class DocumentService {

    @PreAuthorize("hasRole('ADMIN') or #author == authentication.name")
    public void deleteDocument(Long documentId, String author) {
        // Business logic...
    }
}
```

---

## 5. Practical Examples

### 5.1 Fine-Grained Access Control with @PreAuthorize and SpEL
```java
package com.example.security.service;

import com.example.security.domain.Account;
import com.example.security.repository.AccountRepository;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;

@Service
public class AccountManagementService {

    private final AccountRepository accountRepository;

    public AccountManagementService(AccountRepository accountRepository) {
        this.accountRepository = accountRepository;
    }

    /**
     * SpEL Expression:
     * Allowed IF user has ROLE_ADMIN OR the target accountId matches the logged-in principal's ID!
     */
    @PreAuthorize("hasRole('ADMIN') or #accountId == authentication.principal.id")
    public void updateAccountBalance(Long accountId, BigDecimal newBalance) {
        Account account = accountRepository.findById(accountId).orElseThrow();
        account.setBalance(newBalance);
        accountRepository.save(account);
    }
}
```

### 5.2 Domain-Object Ownership Verification with @PostAuthorize
```java
package com.example.security.service;

import com.example.security.domain.TaxDocument;
import com.example.security.repository.TaxDocumentRepository;
import org.springframework.security.access.prepost.PostAuthorize;
import org.springframework.stereotype.Service;

@Service
public class TaxDocumentService {

    private final TaxDocumentRepository repository;

    public TaxDocumentService(TaxDocumentRepository repository) {
        this.repository = repository;
    }

    /**
     * Fetches the document from the database, then verifies ownership:
     * If the returned document's owner does not match the authenticated user,
     * Spring throws AccessDeniedException and prevents the caller from seeing the document!
     */
    @PostAuthorize("returnObject.ownerUsername == authentication.name or hasRole('AUDITOR')")
    public TaxDocument getTaxDocument(Long documentId) {
        return repository.findById(documentId)
            .orElseThrow(() -> new IllegalArgumentException("Document not found: " + documentId));
    }
}
```

### 5.3 Collection Filtering via @PostFilter and @PreFilter
```java
package com.example.security.service;

import com.example.security.domain.Project;
import com.example.security.repository.ProjectRepository;
import org.springframework.security.access.prepost.PostFilter;
import org.springframework.security.access.prepost.PreFilter;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class ProjectService {

    private final ProjectRepository projectRepository;

    public ProjectService(ProjectRepository projectRepository) {
        this.projectRepository = projectRepository;
    }

    /**
     * @PostFilter automatically filters the returned List:
     * Any project where filterObject.leadEmail does NOT match the logged-in user is removed!
     */
    @PostFilter("filterObject.leadEmail == authentication.name or hasRole('ADMIN')")
    public List<Project> getAllProjects() {
        return projectRepository.findAll();
    }

    /**
     * @PreFilter filters incoming list of projects before execution:
     */
    @PreFilter("filterObject.organization == authentication.principal.orgId")
    public void archiveProjects(List<Project> projects) {
        projectRepository.deleteAll(projects);
    }
}
```

### 5.4 Anti-Pattern: Relying Solely on URL-Based Security for Business Logic
```java
// =========================================================================
// ANTI-PATTERN: Broken Object Level Authorization (BOLA / IDOR)
// =========================================================================
@RestController
public class VulnerableOrderController {

    @Autowired private OrderService orderService;

    // SecurityConfig: .requestMatchers("/orders/**").authenticated()
    // CRITICAL SECURITY FLAW:
    // Any logged-in user can pass ANY orderId in the URL path (/orders/1092)!
    // User A can view or cancel User B's orders because there is no ownership check!
    @GetMapping("/orders/{orderId}")
    public OrderDto getOrder(@PathVariable Long orderId) {
        return orderService.findById(orderId); // BOLA EXPLOIT!
    }
}

// =========================================================================
// PRODUCTION FIX: Method Security Ownership Check
// =========================================================================
@Service
public class SecureOrderService {

    // Enforce ownership directly on the service method!
    @PreAuthorize("@orderSecurity.isOwner(#orderId, authentication)")
    public OrderDto findById(Long orderId) {
        return orderRepository.findDtoById(orderId);
    }
}
```

### 5.5 Custom Method Security Bean Evaluator
```java
package com.example.security.evaluator;

import com.example.security.repository.OrderRepository;
import org.springframework.security.core.Authentication;
import org.springframework.stereotype.Component;

/**
 * Custom Spring Bean registered for SpEL method security expressions.
 * Invoked via: @PreAuthorize("@orderSecurity.isOrderOwner(#orderId, authentication)")
 */
@Component("orderSecurity")
public class OrderSecurityEvaluator {

    private final OrderRepository orderRepository;

    public OrderSecurityEvaluator(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    public boolean isOrderOwner(Long orderId, Authentication authentication) {
        if (authentication == null || !authentication.isAuthenticated()) {
            return false;
        }

        // Admins can manage all orders
        if (authentication.getAuthorities().stream().anyMatch(a -> a.getAuthority().equals("ROLE_ADMIN"))) {
            return true;
        }

        String username = authentication.getName();
        return orderRepository.existsByIdAndCustomerEmail(orderId, username);
    }
}
```

---

## 6. Best Practices

1. **Use `@EnableMethodSecurity` Instead of `@EnableGlobalMethodSecurity`**: Migrate all configurations to Spring Security 6's streamlined `@EnableMethodSecurity`.
2. **Encapsulate Complex SpEL Logic in Dedicated Security Beans**: Avoid embedding lengthy SpEL expressions in annotations. Delegate to custom beans: `@PreAuthorize("@permEvaluator.canEdit(#id)")`.
3. **Use `@PostAuthorize` for Cost-Effective Entity Ownership**: Rather than querying the database in `@PreAuthorize` and then re-querying in the service method, use `@PostAuthorize` to evaluate ownership on the already-loaded return object.
4. **Be Cautious with `@PostFilter` on Large Datasets**: `@PostFilter` loads all rows into memory and filters them one by one in Java. For large datasets, push the filtering down to the database level using Spring Data repository queries or Specifications.
5. **Ensure Method Parameter Names Are Preserved**: Compile your project with the `-parameters` flag (enabled by default in modern Spring Boot Maven/Gradle plugins) so SpEL can resolve `#orderId` cleanly without requiring `@Param("orderId")`.
6. **Guard Service Layer Interfaces**: Apply method security to service interfaces or public service classes rather than controllers to ensure security rules apply regardless of invocation entry points.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Missing `-parameters` Compiler Flag** | SpEL cannot resolve method parameter names like `#userId`, throwing `EvaluationException`. | Ensure `maven-compiler-plugin` has `<parameters>true</parameters>`. |
| **Self-Invocation Within Same Bean** | Calling `this.secureMethod()` internally bypasses the Spring AOP proxy; `@PreAuthorize` is never evaluated! | Call method via a separate bean or refactor into distinct services. |
| **Using `@PostFilter` on Unmodifiable Collections** | `@PostFilter` calls `collection.remove()`; invoking it on `List.of()` or `Collections.unmodifiableList()` throws `UnsupportedOperationException`. | Return mutable collections like `new ArrayList<>(...)`. |
| **Applying Method Security to Private Methods** | Spring AOP proxies cannot intercept private methods; annotations are silently ignored. | Annotate only `public` methods. |
| **Using Deprecated `@EnableGlobalMethodSecurity`** | Deprecated in Spring Security 5.6 and removed in modern versions. | Use `@EnableMethodSecurity`. |
| **Throwing Unhandled SpEL NullPointerExceptions** | If `#account.owner` is null, evaluating `#account.owner.id` throws a SpEL evaluation exception instead of denying access. | Use SpEL safe navigation: `#account?.owner?.id`. |

---

## 8. Performance & Security Notes

### Performance
- **SpEL Expression Caching**: Spring Security compiles and caches parsed SpEL expression Abstract Syntax Trees (ASTs), ensuring subsequent invocations execute at near-native Java bytecode speeds.
- **Avoid PostFilter for Large Result Sets**: Using `@PostFilter` on a query that loads 100,000 entities into memory will cause severe garbage collection pauses. Always apply pagination and filtering at the database layer.

### Security
- **Defense-in-Depth**: Combining URL security at the gateway with Method Security at the domain layer ensures that even if an internal routing bug or misconfigured proxy allows a request through, the domain service enforces data ownership.
- **OWASP API1 (BOLA) Mitigation**: Enforcing ownership checks via `@PreAuthorize("@evaluator.isOwner(#id)")` eliminates the number one API vulnerability: Broken Object Level Authorization.

---

## 9. Comparison / When to Use What

| Security Layer | Where Evaluated | Strengths | Weaknesses |
| :--- | :--- | :--- | :--- |
| **URL Security (`authorizeHttpRequests`)** | HTTP Filter Chain | Coarse routing, fast rejection, public paths | Cannot inspect domain object ownership |
| **Method Security (`@PreAuthorize`)** | Service AOP Proxy | Fine-grained, checks arguments, reusable | Requires method invocation |
| **Post-Authorization (`@PostAuthorize`)** | Service AOP Proxy | Inspects return values directly | Method executes even if denied (rolls back) |
| **In-Database Filtering (Specification)** | Database SQL Engine | **Optimal performance for large lists** | Database-specific query logic |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between <code>@PreAuthorize</code> and <code>@PostAuthorize</code>?</strong></summary>

- `@PreAuthorize` evaluates its authorization expression *before* the method body executes. If access is denied, method execution is aborted immediately.
- `@PostAuthorize` allows the method body to execute and evaluates authorization *after* the method returns. It has access to the returned object via the SpEL variable `returnObject`. If authorization fails, an `AccessDeniedException` is thrown and the returned object is withheld from the caller.
</details>

<details>
<summary><strong>2. How does Spring Security intercept method calls annotated with <code>@PreAuthorize</code>?</strong></summary>

When `@EnableMethodSecurity` is active, Spring creates a dynamic AOP proxy (using CGLIB) around the target bean. The proxy registers `AuthorizationManagerBeforeMethodInterceptor`. When a method is called, the interceptor extracts the current `Authentication` from `SecurityContextHolder`, evaluates the SpEL expression against method arguments using `MethodSecurityEvaluationContext`, and either proceeds with method execution or throws `AccessDeniedException`.
</details>

<details>
<summary><strong>3. Why should you avoid using <code>@PostFilter</code> on large collections?</strong></summary>

`@PostFilter` requires the underlying database query to fetch *all* candidate rows into JVM memory first. Spring Security then iterates through the collection in memory, evaluating the SpEL expression on each element and removing unauthorized items. On large datasets, this causes high database I/O, heavy memory allocation, and severe GC pauses. Filtering should be pushed down to the SQL database using `WHERE` clauses.
</details>

<details>
<summary><strong>4. How do you delegate method authorization to a custom Spring bean in SpEL?</strong></summary>

Using the `@beanName.methodName(...)` syntax:
```java
@PreAuthorize("@documentSecurity.canAccess(#docId, authentication)")
public Document getDoc(Long docId) { ... }
```
Spring resolves the bean by name from the `ApplicationContext` and passes the specified arguments.
</details>

<details>
<summary><strong>5. What is the self-invocation issue with method security?</strong></summary>

Like `@Transactional`, method security relies on Spring AOP proxies. If `methodA()` calls `methodB()` on `this` within the same class, the call is internal and bypasses the proxy. Any `@PreAuthorize` annotation on `methodB()` is completely ignored.
</details>

<details>
<summary><strong>6. What happens if an expression in <code>@PreAuthorize</code> throws an exception?</strong></summary>

If the SpEL expression evaluation throws an exception (such as a `NullPointerException`), Spring Security treats it as an authorization failure and aborts execution, translating the error into an `AccessDeniedException`.
</details>

<details>
<summary><strong>7. How does SpEL safe navigation prevent NullPointerExceptions in security annotations?</strong></summary>

Using the `?.` operator (e.g. `authentication.principal?.department?.id`). If any property in the chain is `null`, SpEL evaluates the expression to `null` instead of throwing a `NullPointerException`.
</details>

<details>
<summary><strong>8. Why was <code>@EnableMethodSecurity</code> introduced to replace <code>@EnableGlobalMethodSecurity</code>?</strong></summary>

`@EnableGlobalMethodSecurity` had several legacy flags (`prePostEnabled`, `securedEnabled`, `jsr250Enabled`) and relied on legacy vote-based `AccessDecisionManager` architectures. `@EnableMethodSecurity` enables Pre/Post annotations by default, uses lightweight `AuthorizationManager` components, supports custom authorization managers natively, and simplifies configuration.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Role PreAuthorize Check</strong></summary>

**Task**: Annotate a user deletion method to ensure only users with the `ADMIN` role can execute it.

```java
@PreAuthorize("hasRole('ADMIN')")
public void deleteUser(Long userId) {
    userRepository.deleteById(userId);
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Checking Current User Identity in SpEL</strong></summary>

**Task**: Protect a profile update method ensuring a user can only update their own profile (`#username == authentication.name`).

```java
@PreAuthorize("#username == authentication.name")
public void updateProfile(String username, ProfileDto dto) {
    // Update logic
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): PostAuthorize Return Object Ownership</strong></summary>

**Task**: Write a method that loads an `Invoice` and uses `@PostAuthorize` to verify that `returnObject.customerEmail == authentication.name` or the user has role `ACCOUNTANT`.

```java
@PostAuthorize("returnObject.customerEmail == authentication.name or hasRole('ACCOUNTANT')")
public Invoice getInvoice(Long invoiceId) {
    return invoiceRepository.findById(invoiceId).orElseThrow();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Permission Evaluator Bean</strong></summary>

**Task**: Implement a bean `projectSecurity` with a method `canEdit(Long projectId)` and annotate a service method using SpEL.

```java
@Component("projectSecurity")
public class ProjectSecurity {
    public boolean canEdit(Long projectId) {
        String user = SecurityContextHolder.getContext().getAuthentication().getName();
        return "admin".equals(user); // Example logic
    }
}

// In Service:
@PreAuthorize("@projectSecurity.canEdit(#projectId)")
public void editProject(Long projectId) { ... }
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): PostFilter List Cleaning</strong></summary>

**Task**: Implement a method that returns a mutable list of `Document` objects and filters them using `@PostFilter` so users only receive documents where `confidential == false` or the user is `ADMIN`.

```java
@PostFilter("!filterObject.confidential or hasRole('ADMIN')")
public List<Document> getPublicDocuments() {
    return new ArrayList<>(documentRepository.findAll());
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Method Security Unit Test with WithMockUser</strong></summary>

**Task**: Write a JUnit test asserting that calling a method secured with `@PreAuthorize("hasRole('ADMIN')")` throws `AccessDeniedException` when invoked as a standard `USER`.

```java
@SpringBootTest
class MethodSecurityTest {
    @Autowired private AdminService adminService;

    @Test
    @WithMockUser(roles = "USER")
    void userAccessDenied() {
        assertThatThrownBy(() -> adminService.performAdminAction())
            .isInstanceOf(AccessDeniedException.class);
    }

    @Test
    @WithMockUser(roles = "ADMIN")
    void adminAccessGranted() {
        assertThatCode(() -> adminService.performAdminAction())
            .doesNotThrowAnyException();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Enable:       @EnableMethodSecurity (PrePost enabled by default in Spring 6+)         |
| Pre-Check:    @PreAuthorize("hasRole('ADMIN') or #id == authentication.principal.id") |
| Post-Check:   @PostAuthorize("returnObject.owner == authentication.name")             |
| Filtering:    @PostFilter("filterObject.active") (Requires mutable collection!)       |
| Custom Logic: @PreAuthorize("@mySecurityBean.hasPermission(#id, authentication)")     |
| Golden Rule:  Never rely solely on URL security; protect domain services with SpEL!   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Security Method Security Documentation](https://docs.spring.io/spring-security/reference/servlet/authorization/method-security.html)
- [Spring Expression Language (SpEL) Guide](https://docs.spring.io/spring-framework/reference/core/expressions.html)
- [Baeldung: Spring Security Method Security](https://www.baeldung.com/spring-security-method-security)
- [OWASP API1: Broken Object Level Authorization (BOLA)](https://owasp.org/API-Security/editions/2023/en/0xa1-broken-object-level-authorization/)
