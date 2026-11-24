# 12. Authorization Models: RBAC, ABAC & ACLs in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `03-authorization-and-method-security.md`, SpEL Basics |
| **Estimated Reading Time** | 40 minutes |
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
Modern enterprise applications start with simple authorization: "Can an Admin perform this action?" Over time, business requirements explode in complexity:
- "A Manager can only approve expense reports for employees within their specific department."
- "A Doctor can view patient records only if they have an active assignment to that patient, and only during hospital shift hours."
- "A User can edit a document if they are the author, or if they have been granted explicit `WRITE` permission by an owner via an Access Control List."

Relying exclusively on **Role-Based Access Control (RBAC)** leads to **Role Explosion** (e.g., `ROLE_FINANCE_DEPT_A_TIER_2_MANAGER`). To build maintainable enterprise security, developers must combine RBAC with **Attribute-Based Access Control (ABAC)**, **Access Control Lists (ACLs)**, and externalized Policy Engines.

### Historical Evolution
- **Spring Security 2.x - 3.x**: Focused almost exclusively on RBAC (`ROLE_USER`, `ROLE_ADMIN`) using `AccessDecisionManager` and `AccessDecisionVoter`.
- **Spring Security 4.x - 5.x**: Introduced fine-grained SpEL method security (`@PreAuthorize`, `@PostAuthorize`, `PermissionEvaluator`). Provided `spring-security-acl` for database-backed instance-level permissions.
- **Spring Security 6.x / 7.x**: Replaced legacy voter architectures with the streamlined `AuthorizationManager<T>` contract. Embraced ABAC via functional SpEL evaluation and externalized Policy Decision Points (PDP) like Open Policy Agent (OPA).

### Real-World Relevance
Understanding when to use RBAC (broad coarse-grained routing), ABAC (contextual fine-grained business logic), and ACLs (per-record object sharing) is crucial for multi-tenant SaaS, healthcare (HIPAA), and financial systems.

---

## 2. Core Concepts

### Architectural Contracts
- **Role-Based Access Control (RBAC)**: Maps permissions to abstract roles (`ROLE_EDITOR`), which are assigned to subjects (users). Decision logic depends solely on user role membership.
- **Attribute-Based Access Control (ABAC)**: Evaluates policies dynamically using four attribute dimensions:
  1. *Subject Attributes* (user department, clearance level, manager status).
  2. *Resource Attributes* (document classification, record owner, tenant ID).
  3. *Action Attributes* (read, update, delete, approve).
  4. *Environment / Context Attributes* (IP address, time of day, device trust level).
- **Access Control List (ACL)**: An explicit mapping attached directly to a domain object instance defining permissions (`READ`, `WRITE`, `ADMIN`) for individual users or groups.
- **`org.springframework.security.access.PermissionEvaluator`**: Spring Security's SPI for bridging SpEL expressions like `hasPermission(#document, 'WRITE')` to custom ABAC or ACL evaluators.
- **`org.springframework.security.authorization.AuthorizationManager<T>`**: Core contract introduced in Spring Security 6 that decides whether an `Authentication` is granted access to an object `T`.

### The Mental Model
```
                       ┌────────────────────────┐
                       │  Incoming HTTP Request │
                       └───────────┬────────────┘
                                   │
              Coarse-Grained Gate (URL Filter Chain)
                                   ▼
             ┌────────────────────────────────────────────┐
             │ RBAC: Is user in ROLE_MANAGER?             │
             └─────────────────────┬──────────────────────┘
                                   │ YES
               Fine-Grained Gate (Service Method Invocation)
                                   ▼
             ┌────────────────────────────────────────────┐
             │ ABAC / ACL: @PreAuthorize                  │
             │ hasPermission(#report, 'APPROVE')          │
             │                                            │
             │ Checks:                                    │
             │ 1. report.department == user.department    │
             │ 2. report.amount <= user.approvalLimit     │
             │ 3. time.now() between 08:00 and 18:00      │
             └─────────────────────┬──────────────────────┘
                                   │ YES
                                   ▼
                       [ Execute Business Logic ]
```

---

## 3. How It Works Internally

### SpEL Method Interception & PermissionEvaluator Flow
```
Client Call: orderService.cancelOrder(orderId)
    │
    ▼
[ Spring AOP MethodSecurityInterceptor ]
    │
    ├── 1. Intercepts method invocation
    ├── 2. Parses @PreAuthorize("hasPermission(#orderId, 'Order', 'CANCEL')")
    ├── 3. Resolves arguments (#orderId) from method signature
    ├── 4. Calls SecurityContextHolder to get Authentication
    ├── 5. Invokes CustomPermissionEvaluator.hasPermission(auth, targetId, targetType, permission)
    │         ├─ Loads Order entity from DB
    │         ├─ Evaluates ABAC business rules (owner, status, cutoff time)
    │         └─ Returns boolean
    │
    ├── 6. If TRUE: Proceeds to execute orderService.cancelOrder()
    └── 7. If FALSE: Throws AccessDeniedException -> 403 Forbidden
```

---

## 4. Syntax & Usage

### 1. Activating Method Security
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.access.expression.method.DefaultMethodSecurityExpressionHandler;
import org.springframework.security.access.expression.method.MethodSecurityExpressionHandler;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;

@Configuration
@EnableMethodSecurity(prePostEnabled = true, securedEnabled = true)
public class MethodSecurityConfig {

    @Bean
    public MethodSecurityExpressionHandler methodSecurityExpressionHandler(CustomPermissionEvaluator permissionEvaluator) {
        DefaultMethodSecurityExpressionHandler expressionHandler = new DefaultMethodSecurityExpressionHandler();
        expressionHandler.setPermissionEvaluator(permissionEvaluator);
        return expressionHandler;
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom ABAC Permission Evaluator
An enterprise `PermissionEvaluator` implementing contextual checks based on subject attributes, resource attributes, and business constraints.

```java
package com.example.security.abac;

import com.example.domain.Document;
import com.example.repository.DocumentRepository;
import org.springframework.security.access.PermissionEvaluator;
import org.springframework.security.core.Authentication;
import org.springframework.stereotype.Component;

import java.io.Serializable;
import java.time.LocalTime;

@Component
public class CustomPermissionEvaluator implements PermissionEvaluator {

    private final DocumentRepository documentRepository;

    public CustomPermissionEvaluator(DocumentRepository documentRepository) {
        this.documentRepository = documentRepository;
    }

    @Override
    public boolean hasPermission(Authentication authentication, Object targetDomainObject, Object permission) {
        if (authentication == null || targetDomainObject == null || !(permission instanceof String perm)) {
            return false;
        }

        if (targetDomainObject instanceof Document document) {
            return evaluateDocumentPolicy(authentication, document, perm);
        }

        return false;
    }

    @Override
    public boolean hasPermission(Authentication authentication, Serializable targetId, String targetType, Object permission) {
        if (authentication == null || targetId == null || targetType == null || !(permission instanceof String perm)) {
            return false;
        }

        if ("Document".equalsIgnoreCase(targetType)) {
            return documentRepository.findById((Long) targetId)
                .map(doc -> evaluateDocumentPolicy(authentication, doc, perm))
                .orElse(false);
        }

        return false;
    }

    private boolean evaluateDocumentPolicy(Authentication auth, Document doc, String permission) {
        String username = auth.getName();

        // 1. Super-Admins bypass object-level restrictions
        if (auth.getAuthorities().stream().anyMatch(a -> a.getAuthority().equals("ROLE_ADMIN"))) {
            return true;
        }

        // 2. Owner has full permissions
        if (doc.getOwnerUsername().equalsIgnoreCase(username)) {
            return true;
        }

        // 3. ABAC Policy: Read permission restricted by department and working hours
        if ("READ".equalsIgnoreCase(permission)) {
            boolean sameDepartment = doc.getDepartment().equalsIgnoreCase(getUserDepartment(auth));
            boolean withinWorkingHours = isWithinBusinessHours();
            return sameDepartment && withinWorkingHours;
        }

        // 4. ABAC Policy: Write permission strictly requires explicit collaborator status
        if ("WRITE".equalsIgnoreCase(permission)) {
            return doc.getCollaborators().contains(username);
        }

        return false;
    }

    private String getUserDepartment(Authentication auth) {
        // In real systems, extract from custom principal or JWT claim
        return "FINANCE";
    }

    private boolean isWithinBusinessHours() {
        LocalTime now = LocalTime.now();
        return now.isAfter(LocalTime.of(8, 0)) && now.isBefore(LocalTime.of(18, 0));
    }
}
```

### Example 2: Domain Service Utilizing `@PreAuthorize` and `@PostAuthorize`
Enforcing ABAC rules on both input parameters and return payloads.

```java
package com.example.service;

import com.example.domain.Document;
import com.example.repository.DocumentRepository;
import org.springframework.security.access.prepost.PostAuthorize;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class DocumentService {

    private final DocumentRepository documentRepository;

    public DocumentService(DocumentRepository documentRepository) {
        this.documentRepository = documentRepository;
    }

    // ABAC Check via PermissionEvaluator on ID
    @PreAuthorize("hasPermission(#id, 'Document', 'WRITE')")
    @Transactional
    public void updateDocument(Long id, String newContent) {
        Document doc = documentRepository.findById(id).orElseThrow();
        doc.setContent(newContent);
    }

    // @PostAuthorize checks the RETURNED object before handing it to the caller
    @PostAuthorize("returnObject.ownerUsername == authentication.name or hasRole('AUDITOR')")
    public Document getConfidentialDocument(Long id) {
        return documentRepository.findById(id).orElseThrow();
    }
}
```

### Example 3: Edge Case – Dynamic Collection Filtering via `@PostFilter`
Filtering out unauthorized list items from a search result query based on dynamic ABAC ownership without writing complex SQL clauses.

```java
package com.example.service;

import com.example.domain.ProjectReport;
import org.springframework.security.access.prepost.PostFilter;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class ReportAnalyticsService {

    @PostFilter("filterObject.department == authentication.principal.department or hasRole('EXECUTIVE')")
    public List<ProjectReport> getAllQuarterlyReports() {
        // Fetches reports from database; Spring Security filters unauthorized elements from the returned List
        return loadRawReportsFromStore();
    }

    private List<ProjectReport> loadRawReportsFromStore() {
        return List.of(
            new ProjectReport("R-1", "FINANCE", "Financial Summary"),
            new ProjectReport("R-2", "MARKETING", "Q3 Campaigns"),
            new ProjectReport("R-3", "HR", "Hiring Targets")
        );
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Fetching the target domain object inside the controller or service method and performing manual `if (!user.equals(owner))` authorization checks scattered throughout business code.

```java
// BUGGY & UNMAINTAINABLE: Authorization logic pollutes business domain!
@Transactional
public void deleteDocument(Long docId, String currentUser) {
    Document doc = repository.findById(docId).orElseThrow();
    if (!doc.getOwner().equals(currentUser)) { // Manual check! Easily forgotten!
        throw new SecurityException("Not authorized!");
    }
    repository.delete(doc);
}
```

**Production Fix**:
Declaratively enforce security via `@PreAuthorize` and `PermissionEvaluator`, ensuring separation of concerns and eliminating forgotten authorization checks.

```java
// SECURE: Declarative authorization cleanly decoupled from business transaction
@PreAuthorize("hasPermission(#docId, 'Document', 'DELETE')")
@Transactional
public void deleteDocument(Long docId) {
    repository.deleteById(docId);
}
```

### Example 5: Open Policy Agent (OPA) Externalized ABAC Decision Engine
Integrating Spring Security with an external Open Policy Agent (OPA) PDP via REST.

```java
package com.example.security.opa;

import org.springframework.security.authorization.AuthorizationDecision;
import org.springframework.security.authorization.AuthorizationManager;
import org.springframework.security.core.Authentication;
import org.springframework.security.web.access.intercept.RequestAuthorizationContext;
import org.springframework.stereotype.Component;
import org.springframework.web.client.RestClient;

import java.util.Map;
import java.util.function.Supplier;

@Component
public class OpaAuthorizationManager implements AuthorizationManager<RequestAuthorizationContext> {

    private final RestClient restClient;

    public OpaAuthorizationManager(RestClient.Builder restClientBuilder) {
        this.restClient = restClientBuilder.baseUrl("http://opa:8181/v1/data").build();
    }

    @Override
    public AuthorizationDecision check(Supplier<Authentication> authentication, RequestAuthorizationContext context) {
        Authentication auth = authentication.get();
        if (auth == null || !auth.isAuthenticated()) {
            return new AuthorizationDecision(false);
        }

        Map<String, Object> input = Map.of(
            "user", auth.getName(),
            "roles", auth.getAuthorities().stream().map(a -> a.getAuthority()).toList(),
            "path", context.getRequest().getRequestURI(),
            "method", context.getRequest().getMethod()
        );

        OpaResponse response = restClient.post()
            .uri("/httpapi/authz/allow")
            .body(Map.of("input", input))
            .retrieve()
            .body(OpaResponse.class);

        boolean allowed = response != null && response.result();
        return new AuthorizationDecision(allowed);
    }

    record OpaResponse(boolean result) {}
}
```

---

## 6. Best Practices

1. **Use RBAC at the Periphery, ABAC in the Core**: Enforce high-level role checks (`hasRole('EMPLOYEE')`) in the HTTP filter chain, and fine-grained ABAC rules (`hasPermission(...)`) at the service method layer.
2. **Implement `PermissionEvaluator` for Reusability**: Centralize domain security policies in a dedicated `PermissionEvaluator` rather than writing messy multi-line SpEL expressions in `@PreAuthorize`.
3. **Prefer Database Filtering over In-Memory `@PostFilter` for Large Datasets**: `@PostFilter` loads all records into JVM memory before filtering. For queries returning thousands of rows, filter in SQL (e.g., Spring Data JPA Specifications).
4. **Leverage `@PostAuthorize` for Return Object Verification**: Use `@PostAuthorize("returnObject.owner == authentication.name")` when the target entity ID is not known before method execution.
5. **Fail Closed**: Default your `PermissionEvaluator` implementation to return `false` if inputs, permissions, or domain entities cannot be resolved.
6. **Keep Policies Auditable**: Log permission denials with user identity, resource ID, and failing attribute conditions to facilitate compliance tracking.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Role explosion in RBAC | Creating hundreds of static roles for every slight permission variation. | Migrate contextual constraints to ABAC attributes. |
| Using `@PostFilter` on huge database sets | Causes OutOfMemory errors and excessive database I/O by loading thousands of unneeded records. | Use Spring Data Specifications or query parameters to filter at the database level. |
| Forgetting `@EnableMethodSecurity` | `@PreAuthorize` and `@PostAuthorize` annotations are silently ignored at runtime. | Add `@EnableMethodSecurity(prePostEnabled = true)` to a configuration class. |
| Calling secure methods from within the same class | Spring AOP proxies only intercept external calls; self-invocation bypasses security checks. | Inject self-bean, invoke via an external service, or move security to caller. |
| Mutating collections during `@PreFilter` | Throws `UnsupportedOperationException` if the input collection is immutable (`List.of()`). | Ensure filtered collections are mutable (e.g., `new ArrayList<>(list)`). |
| Hardcoding business hours/IPs in SpEL | SpEL becomes brittle, unreadable, and impossible to unit test cleanly. | Encapsulate dynamic logic within custom Spring service beans (`@securityService.canAccess(...)`). |

---

## 8. Performance & Security Notes

- **AOP Proxy Overhead**: Every `@PreAuthorize` method invocation creates an AOP proxy dispatch, argument array allocation, and SpEL expression tree evaluation. In latency-critical hot loops (e.g., millions of method calls), avoid method security inside tight inner loops.
- **Database Round-Trips in `PermissionEvaluator`**: Looking up domain entities by ID within `hasPermission(auth, targetId, ...)` adds an extra SQL query before every secured method. Use second-level caching or pass the loaded domain object directly.
- **Open Policy Agent (OPA) Latency**: External REST calls to OPA introduce network latency (1–5ms). Colocate OPA as a local sidecar or use an in-process evaluation library.

---

## 9. Comparison / When to Use What

| Model | Decision Basis | Granularity | Maintenance Effort | Best Use Case |
|---|---|---|---|---|
| **RBAC** | User Roles (`ROLE_USER`) | Coarse | Low | Basic dashboards, public vs internal routing |
| **ABAC** | Subject, Resource, Action, Environment | Very Fine | Moderate | Healthcare, multi-tenant billing, departmental limits |
| **ACL** | Explicit Object-to-User mapping table | Instance-level | High | Document sharing (Google Docs model, file permissions) |
| **External PDP (OPA)** | External Rego policy files | Extreme | High (DevOps) | Cloud-native microservice mesh, cross-language consistency |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental limitation of RBAC that ABAC resolves?</summary>

**Answer:**
RBAC bases decisions solely on a user's static role assignments. It cannot express contextual, dynamic, or relational rules (such as "a user can only edit their own profile" or "managers can only approve reports during business hours"). Attempting to model these in RBAC leads to "Role Explosion". ABAC resolves this by evaluating attributes of the user, the resource, the action, and the environment dynamically.
</details>

<details>
<summary>2. How does `PermissionEvaluator` work in Spring Security?</summary>

**Answer:**
`PermissionEvaluator` is an interface with two overloaded methods:
1. `hasPermission(Authentication auth, Object targetDomainObject, Object permission)`
2. `hasPermission(Authentication auth, Serializable targetId, String targetType, Object permission)`
When Spring Security encounters a SpEL expression like `hasPermission(#doc, 'READ')` or `hasPermission(#id, 'Document', 'WRITE')`, it delegates execution to the registered `PermissionEvaluator` bean.
</details>

<details>
<summary>3. Why does `@PreAuthorize` fail to intercept a private method or internal method call?</summary>

**Answer:**
Spring Security's method security relies on Spring AOP dynamic proxies (CGLIB or JDK dynamic proxies). Proxies can only intercept calls made through public methods from external beans. Private methods cannot be proxied, and internal self-invocations (`this.method()`) bypass the proxy entirely.
</details>

<details>
<summary>4. What is the difference between `@PreAuthorize` and `@PostAuthorize`?</summary>

**Answer:**
`@PreAuthorize` evaluates the authorization decision **before** the method executes. If authorization fails, the method is not invoked.
`@PostAuthorize` allows the method to execute first, and evaluates the authorization decision **after** completion, providing access to the returned object via the SpEL keyword `returnObject`. If authorization fails, an `AccessDeniedException` is thrown and the client never receives the return value.
</details>

<details>
<summary>5. When should you avoid using `@PostFilter`?</summary>

**Answer:**
You should avoid `@PostFilter` when querying large volumes of data from a database. `@PostFilter` fetches all rows into JVM memory and iterates through them to discard unauthorized items. This wastes database I/O, network bandwidth, and JVM heap memory. In large datasets, filtering must be performed directly in the database query via SQL/JPQL.
</details>

<details>
<summary>6. How do you reference a Spring bean directly within a SpEL authorization expression?</summary>

**Answer:**
You reference a bean using the `@` prefix. For example:
`@PreAuthorize("@documentSecurityService.canEdit(#documentId, authentication)")`
This delegates authorization logic directly to a testable, injectable Spring bean.
</details>

<details>
<summary>7. What is an Access Control List (ACL) and how does it differ from ABAC?</summary>

**Answer:**
An ACL is an explicit, stateful data structure (often stored in dedicated database tables) that enumerates specific permissions granted to specific principals for a given object instance (e.g., "User Alice has `READ` on Document #42; User Bob has `ADMIN` on Document #42").
ABAC is policy-based and rule-driven (e.g., "Any user in the same department as Document #42 has `READ`"). ACLs are ideal for user-driven sharing; ABAC is ideal for organizational rules.
</details>

<details>
<summary>8. What is a Policy Decision Point (PDP) versus a Policy Enforcement Point (PEP)?</summary>

**Answer:**
In standard authorization architecture:
- **PEP (Policy Enforcement Point)**: The component that intercepts the request and enforces the authorization decision (e.g., Spring Security's `AuthorizationFilter` or `@PreAuthorize` aspect).
- **PDP (Policy Decision Point)**: The engine that evaluates policy rules against input attributes and issues an `ALLOW` or `DENY` decision (e.g., `PermissionEvaluator` or Open Policy Agent).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Enable Method Security (Easy)</summary>

**Task**: Create a Spring configuration class that enables method security with pre/post annotations and registers a bean exposing the current username via SpEL.

**Solution**:
```java
@Configuration
@EnableMethodSecurity(prePostEnabled = true)
public class SecurityMethodConfig {
    // Standard configuration enables @PreAuthorize, @PostAuthorize, @PreFilter, @PostFilter
}
```
</details>

<details>
<summary>Exercise 2: Basic Ownership Check with SpEL (Easy)</summary>

**Task**: Annotate a service method `updateProfile(UserProfile profile)` with `@PreAuthorize` ensuring that only the profile owner (matching `authentication.name`) or a user with `ROLE_ADMIN` can execute it.

**Solution**:
```java
@PreAuthorize("#profile.username == authentication.name or hasRole('ADMIN')")
public void updateProfile(UserProfile profile) {
    profileRepository.save(profile);
}
```
</details>

<details>
<summary>Exercise 3: Custom Security Evaluation Bean (Medium)</summary>

**Task**: Implement a Spring Bean `OrderSecurityService` and write a `@PreAuthorize` expression that checks if the order belongs to the authenticated user's organization.

**Solution**:
```java
@Component("orderSecurity")
public class OrderSecurityService {
    private final OrderRepository orderRepository;

    public OrderSecurityService(OrderRepository orderRepository) {
        this.orderRepository = orderRepository;
    }

    public boolean isOrgMember(Long orderId, Authentication auth) {
        Order order = orderRepository.findById(orderId).orElse(null);
        if (order == null) return false;
        CustomUserPrincipal principal = (CustomUserPrincipal) auth.getPrincipal();
        return order.getOrgId().equals(principal.getOrgId());
    }
}

// In Service:
@PreAuthorize("@orderSecurity.isOrgMember(#orderId, authentication)")
public void cancelOrder(Long orderId) {
    orderRepository.deleteById(orderId);
}
```
</details>

<details>
<summary>Exercise 4: PostAuthorize Return Object Check (Medium)</summary>

**Task**: Write a method that loads an `Invoice` by ID and uses `@PostAuthorize` to ensure that an invoice marked as `CONFIDENTIAL` can only be viewed by users with authority `ROLE_EXECUTIVE`.

**Solution**:
```java
@PostAuthorize("!returnObject.confidential or hasAuthority('ROLE_EXECUTIVE')")
public Invoice getInvoiceById(Long invoiceId) {
    return invoiceRepository.findById(invoiceId)
        .orElseThrow(() -> new EntityNotFoundException("Invoice not found"));
}
```
</details>

<details>
<summary>Exercise 5: Multi-Tenant ABAC PermissionEvaluator (Hard)</summary>

**Task**: Implement a `PermissionEvaluator` that extracts `tenant_id` from the JWT claims of `Authentication`, compares it to `TenantAware` domain objects, and validates that users can only perform operations within their own tenant.

**Solution**:
```java
@Component
public class TenantAwarePermissionEvaluator implements PermissionEvaluator {

    @Override
    public boolean hasPermission(Authentication auth, Object target, Object permission) {
        if (!(auth instanceof JwtAuthenticationToken jwtAuth) || !(target instanceof TenantAware entity)) {
            return false;
        }

        String userTenant = jwtAuth.getToken().getClaimAsString("tenant_id");
        return userTenant != null && userTenant.equals(entity.getTenantId());
    }

    @Override
    public boolean hasPermission(Authentication auth, Serializable id, String type, Object perm) {
        return false; // Direct ID resolution not supported in this slice
    }
}
```
</details>

<details>
<summary>Exercise 6: Unit Testing Method Security with `@WithMockUser` (Hard)</summary>

**Task**: Write a JUnit 5 test using Spring Security Test verifying that invoking a secured method throws `AccessDeniedException` when the user does not own the resource and succeeds when the user is an owner.

**Solution**:
```java
@SpringBootTest
class DocumentMethodSecurityTest {

    @Autowired
    private DocumentService documentService;

    @Test
    @WithMockUser(username = "bob")
    void shouldDenyAccessWhenUserIsNotOwner() {
        Document doc = new Document(1L, "alice", "Confidential Data");
        assertThrows(AccessDeniedException.class, () -> {
            documentService.editDocument(doc);
        });
    }

    @Test
    @WithMockUser(username = "alice")
    void shouldAllowAccessWhenUserIsOwner() {
        Document doc = new Document(1L, "alice", "Confidential Data");
        assertDoesNotThrow(() -> {
            documentService.editDocument(doc);
        });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **RBAC**: Simple role checks (`hasRole('ADMIN')`). Best for coarse-grained URL-level protection.
- **ABAC**: Dynamic policy checks based on User, Resource, Action, and Environment attributes. Best for business domain rules.
- **ACL**: Instance-level explicit permission mapping for fine-grained sharing models.
- **`PermissionEvaluator`**: The standard SPI for `hasPermission(...)` in SpEL.
- **External PDP**: Use Open Policy Agent (OPA) when enterprise policies must be decoupled from Java source code.

---

## 13. Further Reading
- [Spring Security Reference: Method Security](https://docs.spring.io/spring-security/reference/servlet/authorization/method-security.html)
- [NIST Special Publication 800-162: Guide to Attribute Based Access Control (ABAC)](https://csrc.nist.gov/publications/detail/sp/800-162/final)
- [Open Policy Agent (OPA) Documentation](https://www.openpolicyagent.org/docs/latest/)
- [Spring Security Reference: Architecture & AuthorizationManager](https://docs.spring.io/spring-security/reference/servlet/authorization/architecture.html)
- [OWASP Access Control Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Access_Control_Cheat_Sheet.html)
