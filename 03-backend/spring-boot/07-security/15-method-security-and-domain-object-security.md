# 15. Method Security & Domain Object ACLs in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `03-authorization-and-method-security.md`, `12-RBAC-ABAC-and-ACLs.md`, Relational Database Schema Design |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, PostgreSQL 16 |

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
In collaborative platforms (such as Google Drive, Jira, GitHub, or Figma), permissions cannot be modeled simply as "Can an Editor edit documents?" Instead, permissions belong to specific **individual domain object instances**:
- User Alice owns Document #101 and grants User Bob `READ` permission.
- User Bob cannot edit Document #101, but has full `ADMINISTRATION` rights on Document #202.
- The `FINANCE_TEAM` role is granted `WRITE` on all documents within Folder #5 via inheritance.

Hardcoding domain ownership checks in business service methods quickly results in spaghetti code, race conditions, and unmaintainable database queries. The **Spring Security ACL (Access Control List)** module provides a standardized, battle-tested schema and service layer for managing instance-level permissions.

### Historical Evolution
- **Spring Security 2.x - 3.x**: Introduced the 4-table ACL schema (`acl_sid`, `acl_class`, `acl_object_identity`, `acl_entry`) and `AclPermissionEvaluator`.
- **Spring Security 4.x - 5.x**: Optimized `BasicLookupStrategy` for ANSI SQL compliance, batch queries, and Ehcache/Caffeine L2 caching.
- **Spring Security 6.x / 7.x**: Updated for Jakarta Persistence and high-concurrency environments with Virtual Threads. Enhanced SpEL integration with `@PreAuthorize("hasPermission(#document, 'ADMINISTRATION')")`.

### Real-World Relevance
Spring Security ACL is the industry standard for multi-tenant document management systems, healthcare record sharing, and enterprise workflow engines requiring granular, auditable permission matrices per row.

---

## 2. Core Concepts

### Architectural Contracts
- **Security Identity (SID)**: Represents the recipient of a permission. Can be a human user (`PrincipalSid`) or a granted authority/role (`GrantedAuthoritySid`).
- **Object Identity**: Represents the secured domain entity instance (e.g., entity class name `com.example.Document` + primary key `101`).
- **Access Control Entry (ACE)**: An individual permission record assigning a bitmask permission to a specific SID for a specific Object Identity.
- **Access Control List (ACL)**: The aggregate root containing all ACEs for an Object Identity, along with parent ACL references for hierarchical inheritance.
- **`org.springframework.security.acls.model.AclService`**: Core query interface for retrieving ACLs.
- **`org.springframework.security.acls.model.MutableAclService`**: Management interface for creating, modifying, and deleting ACLs and ACEs.
- **`org.springframework.security.acls.domain.BasePermission`**: Standard bitmask permission definitions (`READ = 1`, `WRITE = 2`, `CREATE = 4`, `DELETE = 8`, `ADMINISTRATION = 16`).

### The Database Schema (The 4 Canonical Tables)
```
┌──────────────┐         ┌─────────────────────────┐         ┌─────────────────────┐
│   acl_sid    │         │   acl_object_identity   │         │      acl_entry      │
├──────────────┤         ├─────────────────────────┤         ├─────────────────────┤
│ id (PK)      │◄──┐     │ id (PK)                 │◄──┐     │ id (PK)             │
│ principal    │   │     │ object_id_class (FK)────┼───┼────►│ acl_object_identity │
│ sid (string) │   │     │ object_id_identity (str)│   │     │ ace_order           │
└──────────────┘   │     │ parent_object (FK)      │   │     │ sid (FK)────────────┘
                   │     │ owner_sid (FK)──────────┘   │     │ mask (integer)
┌──────────────┐   │     │ entries_inheriting      │   │     │ granting (boolean)
│  acl_class   │   │     └─────────────────────────┘   │     └─────────────────────┘
├──────────────┤   │                                   │
│ id (PK)      │◄──┼───────────────────────────────────┘
│ class (str)  │   │
└──────────────┘   │
```

---

## 3. How It Works Internally

### SpEL Permission Evaluation Pipeline
```
Method Invocation: documentService.deleteDocument(docId)
    │
    ▼
[@PreAuthorize("hasPermission(#docId, 'com.example.Document', 'DELETE')")]
    │
    ├── 1. Spring AOP MethodSecurityInterceptor invokes AclPermissionEvaluator
    ├── 2. Resolves ObjectIdentity (Class = "com.example.Document", ID = 101)
    ├── 3. Resolves SIDs for current Authentication (e.g., PrincipalSid("alice"), GrantedAuthoritySid("ROLE_USER"))
    ├── 4. Queries AclService.readAclById(objectIdentity, sids)
    │         ├─ Checks local ACL cache (Caffeine / Redis)
    │         └─ On cache miss: Executes optimized SQL join across acl_entry, acl_object_identity, acl_sid
    │
    ├── 5. Inspects ACEs on the ACL:
    │         ├─ Finds matching ACE for SID "alice"
    │         ├─ Bitwise check: (mask & BasePermission.DELETE.getMask()) != 0
    │         └─ If not found, traverses parent ACL hierarchy (if entries_inheriting = true)
    │
    ├── 6. If matching granting ACE found: Returns TRUE
    └── 7. If no granting ACE or explicitly blocked: Throws AccessDeniedException
```

---

## 4. Syntax & Usage

### 1. Spring Security ACL Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.security</groupId>
    <artifactId>spring-security-acl</artifactId>
</dependency>
<dependency>
    <groupId>com.github.ben-manes.caffeine</groupId>
    <artifactId>caffeine</artifactId>
</dependency>
```

### 2. Spring ACL Configuration Bean Setup
```java
package com.example.security.acl;

import org.springframework.cache.caffeine.CaffeineCacheManager;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.access.expression.method.DefaultMethodSecurityExpressionHandler;
import org.springframework.security.access.expression.method.MethodSecurityExpressionHandler;
import org.springframework.security.acls.AclPermissionEvaluator;
import org.springframework.security.acls.domain.*;
import org.springframework.security.acls.jdbc.BasicLookupStrategy;
import org.springframework.security.acls.jdbc.JdbcMutableAclService;
import org.springframework.security.acls.jdbc.LookupStrategy;
import org.springframework.security.acls.model.MutableAclService;
import org.springframework.security.core.authority.SimpleGrantedAuthority;

import javax.sql.DataSource;

@Configuration
public class AclConfig {

    @Bean
    public MutableAclService aclService(DataSource dataSource, LookupStrategy lookupStrategy, AclCache aclCache) {
        JdbcMutableAclService service = new JdbcMutableAclService(dataSource, lookupStrategy, aclCache);
        service.setClassIdentityQuery("SELECT currval(pg_get_serial_sequence('acl_class', 'id'))");
        service.setSidIdentityQuery("SELECT currval(pg_get_serial_sequence('acl_sid', 'id'))");
        return service;
    }

    @Bean
    public LookupStrategy lookupStrategy(DataSource dataSource, AclCache aclCache) {
        return new BasicLookupStrategy(
            dataSource,
            aclCache,
            new AclAuthorizationStrategyImpl(new SimpleGrantedAuthority("ROLE_ADMIN")),
            new ConsoleAuditLogger()
        );
    }

    @Bean
    public AclCache aclCache() {
        return new SpringCacheBasedAclCache(
            new CaffeineCacheManager().getCache("aclCache"),
            new DefaultPermissionGrantingStrategy(new ConsoleAuditLogger()),
            new AclAuthorizationStrategyImpl(new SimpleGrantedAuthority("ROLE_ADMIN"))
        );
    }

    @Bean
    public MethodSecurityExpressionHandler methodSecurityExpressionHandler(AclPermissionEvaluator permissionEvaluator) {
        DefaultMethodSecurityExpressionHandler handler = new DefaultMethodSecurityExpressionHandler();
        handler.setPermissionEvaluator(permissionEvaluator);
        return handler;
    }

    @Bean
    public AclPermissionEvaluator aclPermissionEvaluator(MutableAclService aclService) {
        return new AclPermissionEvaluator(aclService);
    }
}
```

---

## 5. Practical Examples

### Example 1: Programmatic ACL Management Service
A clean enterprise service for granting, modifying, and revoking object permissions.

```java
package com.example.security.acl;

import org.springframework.security.acls.domain.BasePermission;
import org.springframework.security.acls.domain.ObjectIdentityImpl;
import org.springframework.security.acls.domain.PrincipalSid;
import org.springframework.security.acls.model.*;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class PermissionManagementService {

    private final MutableAclService aclService;

    public PermissionManagementService(MutableAclService aclService) {
        this.aclService = aclService;
    }

    @Transactional
    public void grantPermission(Class<?> entityClass, Long entityId, String recipientUsername, Permission permission) {
        ObjectIdentity oi = new ObjectIdentityImpl(entityClass, entityId);
        Sid sid = new PrincipalSid(recipientUsername);

        MutableAcl acl;
        try {
            acl = (MutableAcl) aclService.readAclById(oi);
        } catch (NotFoundException nfe) {
            acl = aclService.createAcl(oi);
        }

        // Insert new Access Control Entry (ACE) at the end of the entry list
        acl.insertAce(acl.getEntries().size(), permission, sid, true);
        aclService.updateAcl(acl);
    }

    @Transactional
    public void revokePermission(Class<?> entityClass, Long entityId, String recipientUsername, Permission permission) {
        ObjectIdentity oi = new ObjectIdentityImpl(entityClass, entityId);
        Sid sid = new PrincipalSid(recipientUsername);

        MutableAcl acl = (MutableAcl) aclService.readAclById(oi);
        for (int i = 0; i < acl.getEntries().size(); i++) {
            AccessControlEntry ace = acl.getEntries().get(i);
            if (ace.getSid().equals(sid) && ace.getPermission().equals(permission)) {
                acl.deleteAce(i);
                break;
            }
        }
        aclService.updateAcl(acl);
    }

    @Transactional
    public void deleteObjectAcl(Class<?> entityClass, Long entityId) {
        ObjectIdentity oi = new ObjectIdentityImpl(entityClass, entityId);
        aclService.deleteAcl(oi, true); // true = cascade delete children
    }
}
```

### Example 2: Domain Entity Protected by SpEL ACL Checks
Using `@PreAuthorize` with `hasPermission` on entity IDs and domain objects.

```java
package com.example.service;

import com.example.domain.Document;
import com.example.repository.DocumentRepository;
import com.example.security.acl.PermissionManagementService;
import org.springframework.security.access.prepost.PostAuthorize;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.acls.domain.BasePermission;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class DocumentManagementService {

    private final DocumentRepository repository;
    private final PermissionManagementService permissionService;

    public DocumentManagementService(DocumentRepository repository, PermissionManagementService permissionService) {
        this.repository = repository;
        this.permissionService = permissionService;
    }

    @Transactional
    public Document createDocument(String title, String content) {
        Document doc = repository.save(new Document(title, content));
        String currentUser = SecurityContextHolder.getContext().getAuthentication().getName();

        // Grant owner full ADMINISTRATION and WRITE permissions
        permissionService.grantPermission(Document.class, doc.getId(), currentUser, BasePermission.ADMINISTRATION);
        permissionService.grantPermission(Document.class, doc.getId(), currentUser, BasePermission.WRITE);
        permissionService.grantPermission(Document.class, doc.getId(), currentUser, BasePermission.READ);

        return doc;
    }

    @PreAuthorize("hasPermission(#id, 'com.example.domain.Document', 'READ')")
    public Document getDocument(Long id) {
        return repository.findById(id).orElseThrow();
    }

    @PreAuthorize("hasPermission(#id, 'com.example.domain.Document', 'WRITE')")
    @Transactional
    public void updateDocument(Long id, String newContent) {
        Document doc = repository.findById(id).orElseThrow();
        doc.setContent(newContent);
    }

    @PreAuthorize("hasPermission(#id, 'com.example.domain.Document', 'ADMINISTRATION')")
    @Transactional
    public void deleteDocument(Long id) {
        repository.deleteById(id);
        permissionService.deleteObjectAcl(Document.class, id);
    }
}
```

### Example 3: Edge Case – Hierarchical ACL Inheritance
Folder-level permissions inheriting downward to child documents via `setParent()` and `setEntriesInheriting(true)`.

```java
package com.example.service;

import org.springframework.security.acls.domain.ObjectIdentityImpl;
import org.springframework.security.acls.model.MutableAcl;
import org.springframework.security.acls.model.MutableAclService;
import org.springframework.security.acls.model.ObjectIdentity;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class FolderHierarchyAclService {

    private final MutableAclService aclService;

    public FolderHierarchyAclService(MutableAclService aclService) {
        this.aclService = aclService;
    }

    @Transactional
    public void linkDocumentToFolder(Long documentId, Long folderId) {
        ObjectIdentity docOi = new ObjectIdentityImpl("com.example.domain.Document", documentId);
        ObjectIdentity folderOi = new ObjectIdentityImpl("com.example.domain.Folder", folderId);

        MutableAcl docAcl = (MutableAcl) aclService.readAclById(docOi);
        MutableAcl folderAcl = (MutableAcl) aclService.readAclById(folderOi);

        docAcl.setParent(folderAcl);
        docAcl.setEntriesInheriting(true); // Inherit all folder permissions!
        aclService.updateAcl(docAcl);
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Forgetting to delete ACL records when deleting an entity from the database, leaving orphan rows in `acl_object_identity` and `acl_entry` that slowly degrade query performance.

```java
// BUGGY: Deleting entity without cleaning up ACL schema leads to orphan records!
@Transactional
public void removeProject(Long projectId) {
    projectRepository.deleteById(projectId); // Leaves acl_object_identity orphaned!
}
```

**Production Fix**:
Explicitly delete the ACL object identity with cascade removal enabled.

```java
// FIXED: Ensures full cascading cleanup across acl_entry and acl_object_identity
@Transactional
public void removeProject(Long projectId) {
    projectRepository.deleteById(projectId);
    ObjectIdentity oi = new ObjectIdentityImpl(Project.class, projectId);
    aclService.deleteAcl(oi, true); // true = delete all children and entries
}
```

### Example 5: High-Performance Distributed Caching with Redis
Replacing local in-memory Caffeine caches with Redis to synchronize ACL invalidations across clustered microservices.

```java
package com.example.security.acl;

import org.springframework.data.redis.cache.RedisCacheManager;
import org.springframework.security.acls.domain.*;
import org.springframework.security.acls.model.AclCache;
import org.springframework.security.core.authority.SimpleGrantedAuthority;

public class DistributedRedisAclCacheConfig {

    public static AclCache createRedisAclCache(RedisCacheManager redisCacheManager) {
        return new SpringCacheBasedAclCache(
            redisCacheManager.getCache("spring_security_acl"),
            new DefaultPermissionGrantingStrategy(new ConsoleAuditLogger()),
            new AclAuthorizationStrategyImpl(new SimpleGrantedAuthority("ROLE_ADMIN"))
        );
    }
}
```

---

## 6. Best Practices

1. **Always Use a High-Performance L2 Cache**: The ACL database schema relies on multi-table joins. Never query the ACL database directly on every request without Caffeine or Redis caching.
2. **Clean Up ACLs on Entity Deletion**: Always invoke `aclService.deleteAcl(oi, true)` inside the same transactional boundary when deleting domain entities.
3. **Use Bitmask Combinations**: Combine permissions using bitwise OR (`BasePermission.READ.getMask() | BasePermission.WRITE.getMask()`) to grant multiple rights in a single ACE.
4. **Leverage Hierarchical Inheritance**: Use `setParent()` to propagate folder or workspace permissions to child objects rather than creating redundant ACE rows for thousands of documents.
5. **Use `GrantedAuthoritySid` for Groups**: Assign permissions to roles (`GrantedAuthoritySid("ROLE_FINANCE")`) whenever possible to minimize per-user ACE rows.
6. **Limit `@PostFilter` Usage**: For search queries returning large record sets, avoid filtering via ACL SpEL in memory; construct SQL queries joining directly on `acl_object_identity`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Uncached ACL lookups | Every `@PreAuthorize` executes 3 database joins, crippling database performance. | Configure `SpringCacheBasedAclCache` with Caffeine or Redis. |
| Forgetting to cascade ACL deletes | Orphan rows accumulate in `acl_entry` and `acl_object_identity`, corrupting future entities reusing IDs. | Call `aclService.deleteAcl(oi, true)` on entity deletion. |
| Non-deterministic class resolution | Renaming domain classes breaks historical string records in `acl_class`. | Register fixed class aliases or avoid renaming secured domain entity classes. |
| Neglecting Parent ACL loops | Setting circular parents (`A -> B -> A`) triggers `StackOverflowError` during inheritance traversal. | Prevent circular parent assignment in application logic. |
| Hardcoding wrong SQL sequence queries | PostgreSQL uses `currval(...)` while MySQL uses `LAST_INSERT_ID()`; wrong query breaks ACL creation. | Configure `classIdentityQuery` and `sidIdentityQuery` matching your dialect. |
| Mutating ACL without calling `updateAcl` | In-memory changes to `MutableAcl` are lost and never persisted to the database. | Always execute `aclService.updateAcl(acl)` after adding/removing ACEs. |

---

## 8. Performance & Security Notes

- **Query Cost of `BasicLookupStrategy`**: Loading an ACL requires querying `acl_object_identity`, `acl_class`, `acl_sid`, and `acl_entry`. Spring's `BasicLookupStrategy` optimizes this by issuing a single batch query for multiple object identities using ANSI SQL `IN (...)`.
- **Database Indexing**: Ensure the following composite indexes exist in your database:
  - `CREATE INDEX idx_acl_entry_obj_sid ON acl_entry(acl_object_identity, sid);`
  - `CREATE INDEX idx_acl_obj_identity ON acl_object_identity(object_id_class, object_id_identity);`
- **Virtual Threads Integration**: JDBC operations performed by `JdbcMutableAclService` execute cleanly without pinning carrier threads in Java 21+.

---

## 9. Comparison / When to Use What

| Security Mechanism | State Location | Granularity | Inheritance Support? | Best Use Case |
|---|---|---|---|---|
| **Simple SpEL ABAC** | Domain Entity columns | Per-row | No | Entities with a simple `owner_id` column |
| **Spring Security ACL** | Dedicated 4-table schema | Per-instance, per-user, per-permission | **Yes (Parent hierarchies)** | Google Drive-style file sharing, multi-user document portals |
| **RBAC** | In-Memory / User Roles | Coarse (Global) | Role hierarchies only | Public vs Admin URL routing |
| **Open Policy Agent (OPA)**| External Policy Files | Dynamic | Via Rego rules | Polyglot microservices, unified cloud compliance |

---

## 10. Interview Questions

<details>
<summary>1. What are the four core tables required by Spring Security ACL?</summary>

**Answer:**
1. **`acl_sid`**: Stores security identities (principals/users and granted authorities/roles).
2. **`acl_class`**: Maps fully-qualified Java domain class names to unique IDs.
3. **`acl_object_identity`**: Identifies specific domain object instances via class ID and primary key string, tracking owner SID and parent ACL hierarchy.
4. **`acl_entry`**: Stores individual Access Control Entries (ACEs), binding a SID to an object identity with a bitmask permission (`mask`) and granting/blocking flag.
</details>

<details>
<summary>2. What is the difference between `PrincipalSid` and `GrantedAuthoritySid`?</summary>

**Answer:**
- **`PrincipalSid`**: Represents a specific authenticated individual user (e.g., username `john.doe`).
- **`GrantedAuthoritySid`**: Represents a role or authority (e.g., `ROLE_MANAGER` or `SCOPE_orders`). Permissions assigned to a `GrantedAuthoritySid` apply to any user possessing that role.
</details>

<details>
<summary>3. How does bitmask permission evaluation work in `BasePermission`?</summary>

**Answer:**
Permissions are represented as powers of two: `READ = 1 (1 << 0)`, `WRITE = 2 (1 << 1)`, `CREATE = 4 (1 << 2)`, `DELETE = 8 (1 << 3)`, `ADMINISTRATION = 16 (1 << 4)`. When evaluating whether a user has `READ` on an entry with mask `3` (`READ | WRITE`), Spring performs bitwise AND: `(mask & permission.getMask()) == permission.getMask()`. If true, the permission is granted.
</details>

<details>
<summary>4. What role does `LookupStrategy` play in Spring Security ACL?</summary>

**Answer:**
`LookupStrategy` defines how ACL data is retrieved from the database and converted into `Acl` object graphs. The standard implementation is `BasicLookupStrategy`, which executes optimized SQL batch queries to fetch object identities, entries, and parent hierarchies in an ANSI SQL-compliant manner, populating the `AclCache`.
</details>

<details>
<summary>5. How does ACL inheritance work when an entity has a parent ACL?</summary>

**Answer:**
When an ACL has `entries_inheriting = true` and a non-null `parent_object`, if an authorization check finds no matching ACE on the immediate object, `AclImpl` traverses up to the parent ACL and evaluates its ACEs. This traversal continues up the hierarchy until a matching granting ACE is found or the root is reached.
</details>

<details>
<summary>6. Why is an `AclCache` virtually mandatory in production?</summary>

**Answer:**
Without caching, evaluating `@PreAuthorize("hasPermission(...)")` on every secured method call triggers multiple database joins across the 4 ACL tables. In an API processing hundreds of requests per second, this introduces severe database bottlenecks. `AclCache` caches populated `Acl` instances in Caffeine or Redis.
</details>

<details>
<summary>7. What happens if an entity is renamed or moved to another package in the codebase?</summary>

**Answer:**
The `acl_class` table stores fully-qualified Java class names as strings (e.g., `com.example.Document`). If you refactor or move the class to a new package, existing records in `acl_class` will no longer match the new class identity, breaking all ACL lookups. A database migration script must be executed to update the class strings in `acl_class`.
</details>

<details>
<summary>8. Can Spring Security ACL handle negative permissions (explicit denials)?</summary>

**Answer:**
Yes. The `acl_entry` table includes a boolean column `granting`. When `granting = false`, the ACE acts as an explicit denial. If an explicit denial ACE matches the user's SID before any granting ACE, the permission check immediately fails.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Permission Bitmask Combination (Easy)</summary>

**Task**: Write a Java snippet combining `BasePermission.READ` and `BasePermission.WRITE` into a cumulative bitmask integer.

**Solution**:
```java
int readWriteMask = BasePermission.READ.getMask() | BasePermission.WRITE.getMask();
// READ (1) | WRITE (2) = 3
```
</details>

<details>
<summary>Exercise 2: Create Principal and Authority SIDs (Easy)</summary>

**Task**: Create a `PrincipalSid` for user `"alice"` and a `GrantedAuthoritySid` for role `"ROLE_EDITOR"`.

**Solution**:
```java
Sid userSid = new PrincipalSid("alice");
Sid roleSid = new GrantedAuthoritySid("ROLE_EDITOR");
```
</details>

<details>
<summary>Exercise 3: Granting Object Permission via MutableAclService (Medium)</summary>

**Task**: Write a method that takes an entity ID, checks if an ACL exists, creates it if missing, and adds a `READ` permission for the user `"bob"`.

**Solution**:
```java
public void grantRead(Long entityId, String username, MutableAclService aclService) {
    ObjectIdentity oi = new ObjectIdentityImpl("com.example.Report", entityId);
    Sid sid = new PrincipalSid(username);

    MutableAcl acl;
    try {
        acl = (MutableAcl) aclService.readAclById(oi);
    } catch (NotFoundException e) {
        acl = aclService.createAcl(oi);
    }

    acl.insertAce(acl.getEntries().size(), BasePermission.READ, sid, true);
    aclService.updateAcl(acl);
}
```
</details>

<details>
<summary>Exercise 4: Securing a Method with hasPermission SpEL (Medium)</summary>

**Task**: Secure a service method `archiveDocument(Long docId)` using `@PreAuthorize` ensuring the caller has `BasePermission.ADMINISTRATION` on the document entity.

**Solution**:
```java
@PreAuthorize("hasPermission(#docId, 'com.example.domain.Document', 'ADMINISTRATION')")
public void archiveDocument(Long docId) {
    documentRepository.archive(docId);
}
```
</details>

<details>
<summary>Exercise 5: Cascading Object Deletion with ACL Cleanup (Hard)</summary>

**Task**: Implement a transactional service method that deletes a `Document` entity and safely deletes its corresponding ACL hierarchy from the database without leaving orphan records.

**Solution**:
```java
@Transactional
public void deleteDocumentCompletely(Long documentId, 
                                     DocumentRepository repo, 
                                     MutableAclService aclService) {
    repo.deleteById(documentId);
    ObjectIdentity oi = new ObjectIdentityImpl(Document.class, documentId);
    try {
        aclService.deleteAcl(oi, true); // Cascade delete children and entries
    } catch (NotFoundException e) {
        // ACL didn't exist; safe to ignore
    }
}
```
</details>

<details>
<summary>Exercise 6: Unit Testing ACL Method Security (Hard)</summary>

**Task**: Write a JUnit 5 test with `@WithMockUser(username = "charlie")` verifying that a user without `READ` permission receives `AccessDeniedException` when calling an ACL-secured method.

**Solution**:
```java
@SpringBootTest
class DocumentAclSecurityTest {

    @Autowired
    private DocumentManagementService documentService;

    @Test
    @WithMockUser(username = "charlie")
    void shouldDenyAccessWhenNoAclEntryExists() {
        assertThrows(AccessDeniedException.class, () -> {
            documentService.getDocument(999L);
        });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **4 Core Tables**: `acl_sid`, `acl_class`, `acl_object_identity`, `acl_entry`.
- **SIDs represent recipients**: `PrincipalSid` for users, `GrantedAuthoritySid` for roles.
- **Bitmask Permissions**: Combine permissions via `BasePermission` masks (`READ=1`, `WRITE=2`, `ADMINISTRATION=16`).
- **Always Cache ACLs**: Use `SpringCacheBasedAclCache` with Caffeine or Redis to prevent multi-join database saturation.
- **Cascade Cleanup**: Always invoke `aclService.deleteAcl(oi, true)` when deleting domain entities.

---

## 13. Further Reading
- [Spring Security Reference: Domain Object Security (ACLs)](https://docs.spring.io/spring-security/reference/servlet/appendix/database-schema.html#_acl_schema)
- [Spring Security Reference: Method Security](https://docs.spring.io/spring-security/reference/servlet/authorization/method-security.html)
- [RFC 2828: Internet Security Glossary](https://datatracker.ietf.org/doc/html/rfc2828)
- [OWASP Access Control Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Access_Control_Cheat_Sheet.html)
- [PostgreSQL Database Documentation: Hierarchical Queries & Indexing](https://www.postgresql.org/docs/current/indexes.html)
