# JPA Entity Mapping & Relationships: Associations, Cascades & Embeddables

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, Relational foreign key concepts (`09-sql`) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Hibernate 6.6.x, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Bidirectional One-to-Many with Helper Synchronization Methods](#51-bidirectional-one-to-many-with-helper-synchronization-methods)
   - [5.2 Many-to-Many Mapping with Intermediate Join Entity and Extra Columns](#52-many-to-many-mapping-with-intermediate-join-entity-and-extra-columns)
   - [5.3 Value Objects via @Embeddable and @Embedded](#53-value-objects-via-embeddable-and-embedded)
   - [5.4 Anti-Pattern: Unidirectional @OneToMany with Join Table Overhead](#54-anti-pattern-unidirectional-onetomany-with-join-table-overhead)
   - [5.5 Composite Primary Keys Using @EmbeddedId](#55-composite-primary-keys-using-embeddedid)
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
In relational schemas, relationships are represented via foreign key constraints (`FOREIGN KEY (author_id) REFERENCES authors(id)`). In Java object models, relationships are represented by object references or collections (`List<Book> books`). If mapped incorrectly in JPA, relationship updates can result in redundant SQL statements, unintended `NULL` updates on foreign key columns, orphaned child records, or accidental table-wide cascading deletions.

### Historical Evolution
Early versions of EJB and Hibernate made bidirectional mapping notoriously error-prone, frequently resulting in infinite circular references during JSON serialization and dirty-checking desynchronization. JPA standardized the concepts of **owning side** versus **inverse side**, explicit cascade controls (`CascadeType`), and orphan removal semantics (`orphanRemoval = true`). In modern Hibernate 6.6+, relationship mapping utilizes the SQM compiler to produce optimal SQL joins without unnecessary temporary join tables.

### Real-World Relevance
Designing enterprise data models—such as E-Commerce Orders with OrderItems, Users with Roles, or Multi-Tenant Organizations with Subscriptions—requires mastering entity associations. Clean mappings ensure referential integrity, eliminate duplicate queries, and prevent catastrophic cascading deletes in production databases.

---

## 2. Core Concepts

### Association Types
1. **`@ManyToOne`**: Many child entities point to one parent entity. Represents the physical foreign key column in the child table. Always the **owning side**.
2. **`@OneToMany`**: One parent entity has a collection of child entities. Should almost always be paired with `@ManyToOne` using the `mappedBy` attribute to represent the **inverse side**.
3. **`@OneToOne`**: One-to-one mapping. Can be bidirectional or unidirectional.
4. **`@ManyToMany`**: Many entities associate with many entities via a junction/join table. In enterprise applications, best modeled as two `@ManyToOne` associations to an intermediate join entity.

### Owning Side vs Inverse Side
- **Owning Side**: The entity that owns the physical database foreign key column (decorated with `@JoinColumn`). Hibernate only inspects the owning side when writing foreign key updates to the database!
- **Inverse Side**: Marked with `mappedBy = "fieldName"`. It serves as a read-only mirror of the relationship. Changing the inverse collection without updating the child's owning reference will NOT update the database foreign key!

### CascadeType & Orphan Removal
- `CascadeType.PERSIST`: Persisting parent automatically persists new children.
- `CascadeType.MERGE`: Merging parent automatically merges child state.
- `CascadeType.REMOVE`: Deleting parent triggers `DELETE` for all children.
- `orphanRemoval = true`: If a child is removed from the parent's Java `Collection`, Hibernate automatically schedules an SQL `DELETE` for that orphaned child.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Bidirectional One-to-Many Mechanics                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Parent Entity: Post (Inverse Side)          Child Entity: PostComment (Owning Side)|
|  +------------------------------+            +----------------------------------+  |
|  | id: 1L                       |            | id: 10L                          |  |
|  | title: "Spring Boot Guide"   |            | text: "Great article!"           |  |
|  | @OneToMany(mappedBy="post")  |<-----------| @ManyToOne                       |  |
|  | comments: List<PostComment>  |            | @JoinColumn(name="post_id")      |  |
|  +------------------------------+            | post: Post                       |  |
|                 |                            +----------------------------------+  |
|                 |                                      |                           |
|                 v                                      v                           |
|  Hibernate Dirty Checking                     SQL Generation:                      |
|  - Reads comments collection                  INSERT INTO post_comments            |
|  - IGNORES collection for FK updates!         (id, text, post_id)                  |
|  - Inspects comment.getPost() <-------------- VALUES (10, 'Great article!', 1);   |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

1. **Foreign Key Ownership**: In `@OneToMany(mappedBy = "post")`, Hibernate delegates SQL `FOREIGN KEY` management entirely to `PostComment.post`.
2. **Synchronization Invariant**: If you invoke `post.getComments().add(comment)` but fail to invoke `comment.setPost(post)`, Hibernate will execute an `INSERT` statement with `post_id = NULL`!
3. **Helper Methods**: Bidirectional entities must include synchronization methods (`addComment` / `removeComment`) that set both sides of the relationship simultaneously.

---

## 4. Syntax & Usage

### Idiomatic Bidirectional @OneToMany Mapping
```java
@Entity
public class Order {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @OneToMany(mappedBy = "order", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<OrderItem> items = new ArrayList<>();

    // Synchronization helper method
    public void addItem(OrderItem item) {
        items.add(item);
        item.setOrder(this);
    }

    public void removeItem(OrderItem item) {
        items.remove(item);
        item.setOrder(null);
    }
}

@Entity
public class OrderItem {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "order_id", nullable = false)
    private Order order;
}
```

---

## 5. Practical Examples

### 5.1 Bidirectional One-to-Many with Helper Synchronization Methods
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import java.util.ArrayList;
import java.util.List;

@Entity
@Table(name = "posts")
public class Post {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String title;

    // Inverse side: mappedBy points to the field in PostComment
    @OneToMany(mappedBy = "post", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<PostComment> comments = new ArrayList<>();

    protected Post() {}

    public Post(String title) {
        this.title = title;
    }

    /**
     * CRITICAL: Synchronization helper method.
     * Guarantees both in-memory Java references and database foreign keys remain consistent!
     */
    public void addComment(PostComment comment) {
        this.comments.add(comment);
        comment.setPost(this);
    }

    public void removeComment(PostComment comment) {
        this.comments.remove(comment);
        comment.setPost(null);
    }

    public Long getId() { return id; }
    public String getTitle() { return title; }
    public List<PostComment> getComments() { return List.copyOf(comments); }
}
```

```java
package com.example.jpa.domain;

import jakarta.persistence.*;

@Entity
@Table(name = "post_comments")
public class PostComment {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String review;

    // Owning side: Contains the actual foreign key column (post_id)
    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "post_id", nullable = false)
    private Post post;

    protected PostComment() {}

    public PostComment(String review) {
        this.review = review;
    }

    public void setPost(Post post) {
        this.post = post;
    }

    public Long getId() { return id; }
    public String getReview() { return review; }
    public Post getPost() { return post; }
}
```

### 5.2 Many-to-Many Mapping with Intermediate Join Entity and Extra Columns
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import java.time.Instant;

/**
 * Enterprise Many-to-Many: Rather than using a bare @ManyToMany join table,
 * model the junction table as an explicit entity to support metadata (roles, timestamps).
 */
@Entity
@Table(name = "project_memberships")
public class ProjectMembership {

    @EmbeddedId
    private MembershipId id;

    @ManyToOne(fetch = FetchType.LAZY)
    @MapsId("userId")
    @JoinColumn(name = "user_id")
    private User user;

    @ManyToOne(fetch = FetchType.LAZY)
    @MapsId("projectId")
    @JoinColumn(name = "project_id")
    private Project project;

    @Column(nullable = false)
    private String role; // e.g. "LEAD", "CONTRIBUTOR"

    @Column(nullable = false)
    private Instant joinedAt = Instant.now();

    protected ProjectMembership() {}

    public ProjectMembership(User user, Project project, String role) {
        this.user = user;
        this.project = project;
        this.role = role;
        this.id = new MembershipId(user.getId(), project.getId());
    }

    public String getRole() { return role; }
    public User getUser() { return user; }
    public Project getProject() { return project; }
}
```

### 5.3 Value Objects via @Embeddable and @Embedded
```java
package com.example.jpa.domain;

import jakarta.persistence.Column;
import jakarta.persistence.Embeddable;
import java.util.Objects;

/**
 * Domain-Driven Design Value Object: Embeddable component with no independent table or ID.
 */
@Embeddable
public class Address {

    @Column(name = "street_address", nullable = false)
    private String street;

    @Column(name = "city", nullable = false)
    private String city;

    @Column(name = "zip_code", nullable = false, length = 10)
    private String zipCode;

    protected Address() {}

    public Address(String street, String city, String zipCode) {
        this.street = Objects.requireNonNull(street);
        this.city = Objects.requireNonNull(city);
        this.zipCode = Objects.requireNonNull(zipCode);
    }

    public String getStreet() { return street; }
    public String getCity() { return city; }
    public String getZipCode() { return zipCode; }
}

@Entity
@Table(name = "companies")
public class Company {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String legalName;

    // Embedded directly into the 'companies' table columns
    @Embedded
    private Address headquartersAddress;

    protected Company() {}

    public Company(String legalName, Address headquartersAddress) {
        this.legalName = legalName;
        this.headquartersAddress = headquartersAddress;
    }
}
```

### 5.4 Anti-Pattern: Unidirectional @OneToMany with Join Table Overhead
```java
// =========================================================================
// ANTI-PATTERN: Unidirectional @OneToMany without @JoinColumn
// =========================================================================
@Entity
public class BadDepartment {
    @Id @GeneratedValue
    private Long id;

    // SEVERE DESIGN FLAW:
    // Without @JoinColumn or mappedBy, JPA assumes a separate join table:
    // CREATE TABLE bad_department_employees (bad_department_id, employees_id)!
    // Every insert results in an INSERT into employee, followed by an INSERT into the join table!
    @OneToMany(cascade = CascadeType.ALL)
    private List<Employee> employees = new ArrayList<>();
}

// =========================================================================
// PRODUCTION FIX: Bidirectional @OneToMany or Explicit @JoinColumn
// =========================================================================
@Entity
public class GoodDepartment {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    // Direct Foreign Key on 'employees' table: No join table needed!
    @OneToMany(cascade = CascadeType.ALL, orphanRemoval = true)
    @JoinColumn(name = "department_id") // Enforces FK column directly in employee table
    private List<Employee> employees = new ArrayList<>();
}
```

### 5.5 Composite Primary Keys Using @EmbeddedId
```java
package com.example.jpa.domain;

import jakarta.persistence.Column;
import jakarta.persistence.Embeddable;
import java.io.Serializable;
import java.util.Objects;

@Embeddable
public class MembershipId implements Serializable {

    @Column(name = "user_id")
    private Long userId;

    @Column(name = "project_id")
    private Long projectId;

    public MembershipId() {}

    public MembershipId(Long userId, Long projectId) {
        this.userId = userId;
        this.projectId = projectId;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof MembershipId that)) return false;
        return Objects.equals(userId, that.userId) && Objects.equals(projectId, that.projectId);
    }

    @Override
    public int hashCode() {
        return Objects.hash(userId, projectId);
    }
}
```

---

## 6. Best Practices

1. **Always Use `FetchType.LAZY` on `@ManyToOne` and `@OneToOne`**: JPA defaults them to `EAGER`, which immediately triggers unoptimized queries across associations.
2. **Always Provide Bidirectional Helper Methods**: Encapsulate relationship mutation inside `parent.addChild(child)` and `parent.removeChild(child)` to maintain memory and DB synchronization.
3. **Prefer Bidirectional `@ManyToOne` Over Unidirectional `@OneToMany`**: The child table naturally contains the foreign key column; managing it from the child side (`@ManyToOne`) produces the cleanest SQL `INSERT` statements.
4. **Never Use Unidirectional `@ManyToMany`**: Bare `@ManyToMany` cannot record association metadata (such as timestamps, statuses, or roles) and prevents query optimization. Use an intermediate entity.
5. **Use Value Objects (`@Embeddable`) for Reusable Domain Concepts**: Encapsulate multi-column attributes like addresses, monetary amounts (`currency` + `amount`), or date ranges inside `@Embeddable` classes.
6. **Implement `Serializable`, `equals`, and `hashCode` on Composite Keys**: Any class annotated with `@Embeddable` used as an `@EmbeddedId` must correctly implement value-based `equals()` and `hashCode()`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Updating Only the Inverse Side (`mappedBy`)** | Hibernate only reads the owning side (`@ManyToOne`); modifying the parent's collection without setting the child's reference results in `NULL` foreign keys. | Always update both sides using helper methods (`addComment`). |
| **Default EAGER Fetching on `@ManyToOne`** | JPA defaults `@ManyToOne` to `EAGER`, executing unwanted immediate joins or secondary queries for every row loaded. | Explicitly configure `@ManyToOne(fetch = FetchType.LAZY)`. |
| **Unidirectional `@OneToMany` Without `@JoinColumn`** | Creates an unnecessary junction table, doubling SQL write operations. | Add `@JoinColumn(name = "fk_id")` or convert to bidirectional. |
| **Using `CascadeType.REMOVE` on `@ManyToMany`** | Deleting one user will cascade and delete all associated roles from the system! | Never use `CascadeType.REMOVE` on many-to-many associations. |
| **Mutable Public Collections in Entities** | Exposing `public List<Item> getItems()` allows external code to mutate the list without invoking synchronization helpers. | Return unmodifiable views: `return Collections.unmodifiableList(items)`. |
| **Omitting `orphanRemoval = true` When Replacing Collections** | Removing an item from a collection leaves an orphaned row with a dangling foreign key in the database. | Enable `orphanRemoval = true` when child lifecycle is strictly tied to parent. |

---

## 8. Performance & Security Notes

### Performance
- **Avoid Unnecessary Join Tables**: Unidirectional `@OneToMany` generates separate join table records, increasing foreign key index overhead and doubling DML operations.
- **Collection Type Optimization**: Use `Set` instead of `List` when mapping multiple collections within an entity to prevent Hibernate from throwing `MultipleBagFetchException`.

### Security
- **Cascade Deletion Boundaries**: Accidental placement of `CascadeType.ALL` or `CascadeType.REMOVE` on multi-tenant parent entities can lead to catastrophic cascading data loss across tenants.
- **Circular Reference Serialization**: Bidirectional mappings serialized via Jackson directly cause `StackOverflowError`. Always annotate the inverse relationship with `@JsonIgnore` or use dedicated DTOs.

---

## 9. Comparison / When to Use What

| Relationship Pattern | Owning Side | Join Table Created? | Recommended In Enterprise? |
| :--- | :--- | :--- | :--- |
| **Bidirectional `@OneToMany` / `@ManyToOne`** | `@ManyToOne` (Child) | No (Direct FK) | **YES (Industry Standard)** |
| **Unidirectional `@ManyToOne`** | `@ManyToOne` (Child) | No (Direct FK) | **YES (Clean & Simple)** |
| **Unidirectional `@OneToMany`** | `@OneToMany` (Parent) | **Yes (Unless `@JoinColumn`)** | No (Generates redundant SQL) |
| **Bare `@ManyToMany`** | Any side | Yes | No (Lacks association metadata) |
| **Many-to-Many via Join Entity** | Join Entity | Yes (Mapped directly) | **YES (Full auditability)** |

---

## 10. Interview Questions

<details>
<summary><strong>1. What does the <code>mappedBy</code> attribute signify in JPA?</strong></summary>

`mappedBy` designates the **inverse (non-owning) side** of a bidirectional relationship. It informs Hibernate that the foreign key mapping and physical database column are owned and managed by the field named in `mappedBy` on the target entity class. Hibernate ignores changes to the collection containing `mappedBy` when issuing SQL foreign key updates.
</details>

<details>
<summary><strong>2. Why is default fetching dangerous on <code>@ManyToOne</code> associations?</strong></summary>

The JPA specification mandates that `@ManyToOne` and `@OneToOne` default to `FetchType.EAGER`. If an entity has three `@ManyToOne` relations, loading 100 rows can trigger massive multi-table outer joins or 300 additional `SELECT` statements (N+1 query problem). They should always be explicitly declared as `fetch = FetchType.LAZY`.
</details>

<details>
<summary><strong>3. What is the difference between <code>CascadeType.REMOVE</code> and <code>orphanRemoval = true</code>?</strong></summary>

- `CascadeType.REMOVE`: If the *parent* entity is deleted (`em.remove(parent)`), all its associated children are also deleted.
- `orphanRemoval = true`: In addition to parent deletion cascading, if a child entity is simply removed from the parent's collection (`parent.getChildren().remove(child)`), Hibernate automatically detects that the child has been orphaned and issues an SQL `DELETE` for that child.
</details>

<details>
<summary><strong>4. Why is a bidirectional <code>@OneToMany</code> preferred over a unidirectional <code>@OneToMany</code>?</strong></summary>

In a unidirectional `@OneToMany`, the parent has no physical foreign key column on its own table. JPA therefore creates a junction table by default, requiring separate `INSERT` and `UPDATE` statements for every child added. In a bidirectional relationship, the child manages the foreign key directly via `@ManyToOne`, allowing the child to be inserted in a single clean `INSERT` statement containing the parent ID.
</details>

<details>
<summary><strong>5. Why is modeling <code>@ManyToMany</code> with an explicit intermediate entity considered a best practice?</strong></summary>

A bare `@ManyToMany` junction table only stores two foreign keys. Real-world business requirements almost always evolve to require metadata on the relationship itself (e.g., date assigned, permissions, active flag, creator). An intermediate entity (e.g., `Membership`, `Enrollment`) allows adding attributes naturally and provides explicit control over queries.
</details>

<details>
<summary><strong>6. What is the purpose of synchronization helper methods on bidirectional relationships?</strong></summary>

Java does not automatically synchronize both sides of an object relationship in memory. If `child.setParent(parent)` is called, `parent.getChildren()` does not automatically contain `child`. Helper methods (e.g., `parent.addChild(child)`) ensure both in-memory object references remain consistent, preventing subtle bugs prior to database synchronization.
</details>

<details>
<summary><strong>7. What are the requirements for an <code>@Embeddable</code> class used as an <code>@EmbeddedId</code>?</strong></summary>

1. It must be annotated with `@Embeddable`.
2. It must implement `java.io.Serializable`.
3. It must implement a valid, value-based `equals()` and `hashCode()` method.
4. It must have a no-argument constructor.
</details>

<details>
<summary><strong>8. How does <code>@MapsId</code> work when using composite keys with foreign entities?</strong></summary>

`@MapsId` binds a foreign key entity reference directly to a component field of an `@EmbeddedId`. It tells Hibernate that the primary key value for that composite component should be copied directly from the identifier of the associated `@ManyToOne` entity.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Lazy @ManyToOne Mapping</strong></summary>

**Task**: Map a `Review` entity containing an `id`, `content`, and a lazy `@ManyToOne` reference to a `Product` entity with a foreign key column `product_id`.

```java
@Entity
public class Review {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String content;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "product_id", nullable = false)
    private Product product;
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Embedding an Audit Stamp Value Object</strong></summary>

**Task**: Create an `@Embeddable` class `AuditMetadata` containing `createdAt` and `createdBy`, and embed it inside an `Article` entity.

```java
@Embeddable
public record AuditMetadata(Instant createdAt, String createdBy) {
    public AuditMetadata() { this(Instant.now(), "SYSTEM"); }
}

@Entity
public class Article {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Embedded
    private AuditMetadata audit = new AuditMetadata();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Bidirectional Order and OrderItem Implementation</strong></summary>

**Task**: Implement `Order` and `OrderItem` entities with bidirectional relationship and complete synchronization helper methods (`addItem`, `removeItem`).

```java
@Entity
public class Order {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @OneToMany(mappedBy = "order", cascade = CascadeType.ALL, orphanRemoval = true)
    private List<OrderItem> items = new ArrayList<>();

    public void addItem(OrderItem item) {
        items.add(item);
        item.setOrder(this);
    }

    public void removeItem(OrderItem item) {
        items.remove(item);
        item.setOrder(null);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Orphan Removal Unit Verification</strong></summary>

**Task**: Write an integration test asserting that removing an item from a parent's collection with `orphanRemoval = true` issues an SQL `DELETE` for the child upon flush.

```java
@Test
@Transactional
void testOrphanRemoval() {
    Order order = new Order();
    OrderItem item = new OrderItem("SKU-100");
    order.addItem(item);
    orderRepository.saveAndFlush(order);

    order.removeItem(item);
    orderRepository.saveAndFlush(order);

    assertThat(orderItemRepository.findAll()).isEmpty();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Composite Primary Key with @MapsId</strong></summary>

**Task**: Implement a student course enrollment model where `Enrollment` has an `@EmbeddedId` of `(studentId, courseId)` and uses `@MapsId` to populate keys from `@ManyToOne` associations.

```java
@Entity
public class Enrollment {
    @EmbeddedId
    private EnrollmentId id;

    @ManyToOne(fetch = FetchType.LAZY)
    @MapsId("studentId")
    private Student student;

    @ManyToOne(fetch = FetchType.LAZY)
    @MapsId("courseId")
    private Course course;

    private int grade;
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Self-Referencing Hierarchical Category Mapping</strong></summary>

**Task**: Create a `Category` entity that supports a hierarchical parent-child category tree (self-referencing bidirectional `@ManyToOne` and `@OneToMany`).

```java
@Entity
public class Category {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String name;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "parent_id")
    private Category parent;

    @OneToMany(mappedBy = "parent", cascade = CascadeType.ALL)
    private List<Category> subCategories = new ArrayList<>();

    public void addSubCategory(Category sub) {
        subCategories.add(sub);
        sub.parent = this;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Rule 1: Always specify fetch = FetchType.LAZY on @ManyToOne and @OneToOne            |
| Rule 2: Owning side has @JoinColumn; Inverse side has mappedBy="fieldName"           |
| Rule 3: Always provide helper methods (parent.addChild(child)) on bidirectional links |
| Rule 4: Use orphanRemoval = true when child life cycle is strictly bound to parent    |
| Rule 5: Value Objects: @Embeddable on class, @Embedded on entity field                |
| Rule 6: Avoid bare @ManyToMany; use an intermediate entity with @EmbeddedId           |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Hibernate 6.6 User Guide: Associations and Relations](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#associations)
- [Vlad Mihalcea: The Best Way to Map a @OneToMany Association with JPA and Hibernate](https://vladmihalcea.com/the-best-way-to-map-a-onetomany-association-with-jpa-and-hibernate/)
- [Domain-Driven Design: Value Objects vs Entities](https://martinfowler.com/bliki/ValueObject.html)
- [Spring Data JPA Entity Mapping Best Practices](https://docs.spring.io/spring-data/jpa/reference/jpa/getting-started.html)
