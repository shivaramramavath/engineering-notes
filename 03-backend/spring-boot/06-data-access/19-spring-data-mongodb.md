# Spring Data MongoDB: Document Mapping, Aggregations & Transactions

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `05-repositories-and-derived-queries.md`, NoSQL document store concepts, JSON/BSON |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data MongoDB 4.4.x, MongoDB 7.0+ / 8.0, Testcontainers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Document Mapping with Compound and Text Indexes](#51-document-mapping-with-compound-and-text-indexes)
   - [5.2 Fluent Criteria Queries with MongoTemplate](#52-fluent-criteria-queries-with-mongotemplate)
   - [5.3 Multi-Stage Aggregation Pipelines for Analytics](#53-multi-stage-aggregation-pipelines-for-analytics)
   - [5.4 Anti-Pattern: Treating MongoDB as a Relational Database with @DBRef](#54-anti-pattern-treating-mongodb-as-a-relational-database-with-dbref)
   - [5.5 Multi-Document ACID Transactions with MongoTransactionManager](#55-multi-document-acid-transactions-with-mongotransactionmanager)
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
Relational databases enforce rigid, normalized tabular schemas. When dealing with semi-structured data, hierarchical nested structures, polymorphic catalogs, rapid prototyping where schemas evolve continuously, or massive write ingestion rates, relational models impose costly migration overhead and complex multi-table joins. Document-oriented stores like MongoDB store data as self-contained BSON (Binary JSON) documents.

### Historical Evolution
Spring Data MongoDB was one of the earliest non-relational modules in the Spring Data ecosystem. It provides the same familiar repository abstractions (`MongoRepository`) found in JPA, combined with `MongoTemplate` for low-level criteria building and complex aggregation pipelines. Since MongoDB 4.0+, multi-document ACID transactions are fully supported, allowing developers to execute transactional operations across documents and collections using Spring's standard `@Transactional`.

### Real-World Relevance
MongoDB powers product catalogs, content management systems, event logging systems, user profile services, and real-time operational analytics. Combining Spring Data's declarative repositories with MongoDB's rich document model enables rapid development without sacrificing type safety or transaction guarantees.

---

## 2. Core Concepts

### Core Annotations
- `@Document(collection = "users")`: Marks a class as a MongoDB document stored in a specific collection.
- `@Id`: Marks the primary key (maps to MongoDB `_id`, typically `String`, `ObjectId`, or `Long`).
- `@Field("col_name")`: Configures BSON document property naming.
- `@Indexed`: Declares a single-field index (with options for `unique = true`, `expireAfterSeconds` for TTL).
- `@CompoundIndex`: Declares a multi-field composite index on the class level.
- `@Transient`: Excludes fields from BSON serialization.

### MongoRepository vs MongoTemplate
- **`MongoRepository`**: High-level declarative interface supporting derived query methods (`findByEmail`), pagination, and basic CRUD.
- **`MongoTemplate`**: Low-level synchronous fluent API providing total control over `Query`, `Update`, `Criteria`, and complex `Aggregation` pipeline stages.

### Aggregation Framework
MongoDB's pipeline-based analytics engine: documents enter a multi-stage pipeline (`$match`, `$project`, `$group`, `$sort`, `$unwind`) that transforms them into aggregated results.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring Data MongoDB Architecture                           |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller: userRepository.save(user)                                                 |
|       |                                                                            |
|       v                                                                            |
|  SimpleMongoRepository                                                             |
|       |                                                                            |
|       v                                                                            |
|  MappingMongoConverter                                                             |
|  - Converts Java POJO / Record into org.bson.Document                              |
|  - Maps types, converts UUIDs / Dates / Decimals                                   |
|  - Generates ObjectId if @Id is null                                               |
|       |                                                                            |
|       v                                                                            |
|  MongoTemplate / MongoClient                                                       |
|       |                                                                            |
|       v (Wire Protocol)                                                            |
|  MongoDB Cluster (Primary Replica Node)                                            |
|  - Writes BSON to WiredTiger storage engine                                        |
|  - Replicates to secondary replica nodes                                           |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-mongodb</artifactId>
</dependency>
```

### Basic Document Definition
```java
@Document(collection = "customers")
@CompoundIndex(name = "idx_country_status", def = "{'country': 1, 'status': 1}")
public class Customer {

    @Id
    private String id; // Automatically populates with MongoDB ObjectId hex string

    @Indexed(unique = true)
    private String email;

    private String name;
    private String country;
    private String status;
}
```

---

## 5. Practical Examples

### 5.1 Document Mapping with Compound and Text Indexes
```java
package com.example.mongo.domain;

import org.springframework.data.annotation.Id;
import org.springframework.data.annotation.Version;
import org.springframework.data.mongodb.core.index.CompoundIndex;
import org.springframework.data.mongodb.core.index.CompoundIndexes;
import org.springframework.data.mongodb.core.index.Indexed;
import org.springframework.data.mongodb.core.index.TextIndexed;
import org.springframework.data.mongodb.core.mapping.Document;
import org.springframework.data.mongodb.core.mapping.Field;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;

@Document(collection = "products")
@CompoundIndexes({
    @CompoundIndex(name = "idx_category_price", def = "{'category': 1, 'price': 1}")
})
public class ProductDocument {

    @Id
    private String id;

    @Indexed(unique = true)
    private String sku;

    @TextIndexed(weight = 3)
    private String title;

    @TextIndexed(weight = 1)
    private String description;

    @Field("unit_price")
    private BigDecimal price;

    private String category;

    // Embedded document list stored directly inside BSON array
    private List<ProductAttribute> attributes;

    @Indexed(expireAfterSeconds = 2592000) // TTL Index: Auto-evicts documents after 30 days
    private Instant createdAt = Instant.now();

    @Version
    private Long version;

    public record ProductAttribute(String key, String value) {}

    public ProductDocument(String sku, String title, BigDecimal price, String category) {
        this.sku = sku;
        this.title = title;
        this.price = price;
        this.category = category;
    }

    public String getId() { return id; }
    public String getSku() { return sku; }
    public String getTitle() { return title; }
    public BigDecimal getPrice() { return price; }
}
```

### 5.2 Fluent Criteria Queries with MongoTemplate
```java
package com.example.mongo.repository;

import com.example.mongo.domain.ProductDocument;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageImpl;
import org.springframework.data.domain.Pageable;
import org.springframework.data.mongodb.core.MongoTemplate;
import org.springframework.data.mongodb.core.query.Criteria;
import org.springframework.data.mongodb.core.query.Query;
import org.springframework.stereotype.Repository;

import java.math.BigDecimal;
import java.util.List;

@Repository
public class ProductCustomSearchRepository {

    private final MongoTemplate mongoTemplate;

    public ProductCustomSearchRepository(MongoTemplate mongoTemplate) {
        this.mongoTemplate = mongoTemplate;
    }

    public Page<ProductDocument> searchProducts(
        String category, 
        BigDecimal minPrice, 
        BigDecimal maxPrice, 
        Pageable pageable
    ) {
        Query query = new Query();

        if (category != null && !category.isBlank()) {
            query.addCriteria(Criteria.where("category").is(category));
        }

        if (minPrice != null && maxPrice != null) {
            query.addCriteria(Criteria.where("price").gte(minPrice).lte(maxPrice));
        } else if (minPrice != null) {
            query.addCriteria(Criteria.where("price").gte(minPrice));
        } else if (maxPrice != null) {
            query.addCriteria(Criteria.where("price").lte(maxPrice));
        }

        long totalCount = mongoTemplate.count(query, ProductDocument.class);

        // Apply pagination directly to Mongo query
        query.with(pageable);
        List<ProductDocument> results = mongoTemplate.find(query, ProductDocument.class);

        return new PageImpl<>(results, pageable, totalCount);
    }
}
```

### 5.3 Multi-Stage Aggregation Pipelines for Analytics
```java
package com.example.mongo.service;

import org.springframework.data.domain.Sort;
import org.springframework.data.mongodb.core.MongoTemplate;
import org.springframework.data.mongodb.core.aggregation.Aggregation;
import org.springframework.data.mongodb.core.aggregation.AggregationResults;
import org.springframework.data.mongodb.core.query.Criteria;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;

import static org.springframework.data.mongodb.core.aggregation.Aggregation.*;

@Service
public class OrderAnalyticsService {

    private final MongoTemplate mongoTemplate;

    public OrderAnalyticsService(MongoTemplate mongoTemplate) {
        this.mongoTemplate = mongoTemplate;
    }

    public record CategorySalesSummary(
        String category,
        long totalOrders,
        BigDecimal totalRevenue,
        double averagePrice
    ) {}

    /**
     * Executes multi-stage MongoDB aggregation:
     * 1. $match (Filter by status = 'COMPLETED')
     * 2. $group (Group by category, calculate sum and avg)
     * 3. $sort (Order by revenue descending)
     * 4. $limit (Top 10 categories)
     */
    public List<CategorySalesSummary> getTopSellingCategories() {
        Aggregation aggregation = newAggregation(
            match(Criteria.where("status").is("COMPLETED")),
            group("category")
                .count().as("totalOrders")
                .sum("price").as("totalRevenue")
                .avg("price").as("averagePrice"),
            project("totalOrders", "totalRevenue", "averagePrice")
                .and("_id").as("category"),
            sort(Sort.Direction.DESC, "totalRevenue"),
            limit(10)
        );

        AggregationResults<CategorySalesSummary> results = mongoTemplate.aggregate(
            aggregation, 
            "orders", 
            CategorySalesSummary.class
        );

        return results.getMappedResults();
    }
}
```

### 5.4 Anti-Pattern: Treating MongoDB as a Relational Database with @DBRef
```java
// =========================================================================
// ANTI-PATTERN: Heavy use of @DBRef across documents (Relational Trap)
// =========================================================================
@Document
public class BadOrder {
    @Id private String id;

    // SEVERE ANTI-PATTERN IN MONGODB:
    // @DBRef does NOT perform database joins!
    // Spring Data executes separate client-side queries for EVERY referenced item!
    // Triggers severe N+1 query storms in NoSQL!
    @org.springframework.data.mongodb.core.mapping.DBRef
    private List<OrderItem> items; 
}

// =========================================================================
// PRODUCTION FIX: Embed Related Data Directly in Document
// =========================================================================
@Document(collection = "orders")
public class GoodOrder {
    @Id private String id;

    // OPTIMAL: Embedded documents stored in a single BSON array!
    // Read and written in ONE single atomic document operation!
    private List<OrderItemDto> items; 
}
```

### 5.5 Multi-Document ACID Transactions with MongoTransactionManager
```java
package com.example.mongo.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.data.mongodb.MongoDatabaseFactory;
import org.springframework.data.mongodb.MongoTransactionManager;

@Configuration
public class MongoTransactionConfig {

    /**
     * Enables standard Spring @Transactional support over MongoDB!
     * Requires MongoDB running as a Replica Set.
     */
    @Bean
    public MongoTransactionManager transactionManager(MongoDatabaseFactory dbFactory) {
        return new MongoTransactionManager(dbFactory);
    }
}
```

```java
package com.example.mongo.service;

import com.example.mongo.domain.Account;
import org.springframework.data.mongodb.core.MongoTemplate;
import org.springframework.data.mongodb.core.query.Criteria;
import org.springframework.data.mongodb.core.query.Query;
import org.springframework.data.mongodb.core.query.Update;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

@Service
public class AccountTransferService {

    private final MongoTemplate mongoTemplate;

    public AccountTransferService(MongoTemplate mongoTemplate) {
        this.mongoTemplate = mongoTemplate;
    }

    /**
     * ACID Multi-Document Transaction across MongoDB collections!
     */
    @Transactional
    public void transferFunds(String fromId, String toId, BigDecimal amount) {
        // Deduct from source
        mongoTemplate.updateFirst(
            Query.query(Criteria.where("id").is(fromId)),
            new Update().inc("balance", amount.negate()),
            Account.class
        );

        if (amount.compareTo(BigDecimal.valueOf(10000)) > 0) {
            // Simulated business exception rolls back both document updates!
            throw new IllegalArgumentException("Transfer limit exceeded");
        }

        // Credit to target
        mongoTemplate.updateFirst(
            Query.query(Criteria.where("id").is(toId)),
            new Update().inc("balance", amount),
            Account.class
        );
    }
}
```

---

## 6. Best Practices

1. **Design Documents for Application Access Patterns**: Embed data that is accessed and mutated together; reference data only when datasets are unbounded ($>16\text{ MB}$ limit) or updated independently.
2. **Always Create Indexes for Filtering and Sorting**: Unindexed queries in MongoDB trigger collection scans (`COLLSCAN`), exhausting disk I/O and CPU. Configure `auto-index-creation: true` in development and use Flyway/mongosh scripts in production.
3. **Avoid `@DBRef`**: Prefer embedding child data or storing manual string ID references (`String customerId`). `@DBRef` incurs client-side round-trips and promotes relational anti-patterns.
4. **Use Atomic Document Operators (`$inc`, `$set`, `$push`)**: Perform field updates directly in MongoDB using `mongoTemplate.updateFirst()` rather than loading the entire document into memory, modifying it, and saving it back.
5. **Enable `@Version` for Optimistic Locking**: Protect against concurrent lost updates on documents by declaring `@Version private Long version`.
6. **Deploy MongoDB as a Replica Set**: Multi-document transactions, change streams, and high availability require a replica set configuration (even in local Docker test environments).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Normalizing Data Like an RDBMS** | Breaking documents into dozens of collections forces costly client-side multi-query loops. | Embed cohesive child data directly into parent BSON documents. |
| **Exceeding the 16MB BSON Document Limit** | Unbounded arrays (e.g. storing 100,000 log lines inside an entity) exceed MongoDB's 16MB document cap. | Cap embedded arrays or store unbounded logs in separate documents. |
| **Using Transactions on Standalone MongoDB Instances** | MongoDB transactions require a Replica Set; standalone instances throw `CommandTransactionException`. | Run MongoDB as a single-node replica set (`mongod --replSet rs0`). |
| **Assuming `@Indexed` Automatically Builds Indexes in Production** | Spring Boot disables automatic index creation by default (`spring.data.mongodb.auto-index-creation = false`). | Enable auto-index in dev or run explicit index creation scripts in production. |
| **Using Case-Sensitive Regex in Queries** | `Criteria.regex("pattern")` without a compound prefix bypasses indexes, causing full collection scans. | Use Text Indexes (`$text`) or exact prefix searches. |
| **Storing Dates Without Timezone Awareness** | MongoDB stores all dates in UTC; forgetting this causes timezone display mismatches in web layers. | Store dates as `Instant` in UTC. |

---

## 8. Performance & Security Notes

### Performance
- **Atomic Single-Document Updates**: Single-document modifications (`$set`, `$inc`) are natively atomic in MongoDB with zero multi-document transaction coordination overhead.
- **WiredTiger Compression**: MongoDB automatically applies Snappy or zlib compression to disk blocks, saving $50\text{--}70\%$ storage compared to uncompressed relational tables.

### Security
- **NoSQL Injection Defense**: Never construct queries using raw JSON string concatenation (`"{'$where': 'this.name == " + input + "'}"`). Always use typed `Criteria` objects or parameterized repository methods.

---

## 9. Comparison / When to Use What

| Feature | MongoDB | PostgreSQL (with JSONB) | Redis |
| :--- | :--- | :--- | :--- |
| **Primary Data Model** | BSON Documents | Relational Tables & JSONB | In-Memory Key-Value / Structures |
| **Schema Flexibility** | Schema-less / Dynamic | Typed Tables with flexible JSON | Schema-less |
| **Complex Aggregations** | Aggregation Pipeline | SQL / Window Functions / CTEs | Lua Scripts / Redis Streams |
| **ACID Transactions** | Multi-Document (Replica Set) | Full ACID (Standard) | Multi / Exec (Single Node) |
| **Best Used For** | Rapid catalogs, semi-structured docs | Hybrid relational + JSON systems | Caching, sessions, real-time counters |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does Spring Data MongoDB handle ID generation for documents?</strong></summary>

If an entity's `@Id` field is of type `String`, `ObjectId`, or `BigInteger`, and the field is `null` when saved, Spring Data MongoDB automatically generates a unique 12-byte BSON `ObjectId` (represented as a 24-character hexadecimal string) and assigns it to the entity prior to insertion.
</details>

<details>
<summary><strong>2. Why is the <code>@DBRef</code> annotation discouraged in high-performance MongoDB applications?</strong></summary>

`@DBRef` creates a reference to another collection document using an identifier and collection name. However, MongoDB does not perform relational joins on `@DBRef` fields. Spring Data resolves `@DBRef` by executing a separate, secondary query for each referenced document, creating a severe N+1 query problem. Developers should embed the data directly or store raw IDs.
</details>

<details>
<summary><strong>3. How do multi-document transactions work in MongoDB?</strong></summary>

Introduced in MongoDB 4.0 (for replica sets) and 4.2 (for sharded clusters), multi-document transactions use snapshot isolation. Spring Boot enables transaction management via `MongoTransactionManager`, allowing `@Transactional` methods to coordinate multi-collection writes. If an exception occurs, all writes across all collections are rolled back atomically.
</details>

<details>
<summary><strong>4. What is a TTL (Time-To-Live) index in MongoDB and how do you configure it in Spring Data?</strong></summary>

A TTL index automatically deletes documents from a collection after a specified duration. It is configured on a date/timestamp field using `@Indexed(expireAfterSeconds = N)`. A background thread in MongoDB runs every 60 seconds to purge expired documents, making it ideal for sessions, OTP tokens, and temporary audit logs.
</details>

<details>
<summary><strong>5. What is the MongoDB BSON document size limit and how does it influence data modeling?</strong></summary>

The maximum BSON document size is strictly **16 Megabytes**. This limit prevents documents from consuming excessive RAM and network bandwidth. It influences data modeling by dictating that unbounded arrays (e.g., an order having potentially 100,000 status logs) should not be embedded inside a single document; they must be modeled as separate documents.
</details>

<details>
<summary><strong>6. How does the MongoDB Aggregation Pipeline differ from standard queries?</strong></summary>

Standard queries (`find`) filter and sort documents, returning matching subsets. The Aggregation Pipeline passes documents through a sequence of transformation stages (e.g., `$match`, `$unwind`, `$group`, `$project`, `$sort`), allowing multi-stage data processing, calculations, groupings, and analytics directly on the database server.
</details>

<details>
<summary><strong>7. Why is <code>spring.data.mongodb.auto-index-creation</code> disabled by default in Spring Boot?</strong></summary>

Building indexes on large production collections can consume significant CPU and memory, potentially blocking operations or slowing down startup. Disabling it prevents application restarts from unintentionally triggering heavy index builds in production. Indexes should be managed via automated migration tools or administrative scripts.
</details>

<details>
<summary><strong>8. What is the difference between <code>save()</code> and <code>insert()</code> in <code>MongoTemplate</code>?</strong></summary>

- `insert()` throws a `DuplicateKeyException` if a document with the same `_id` already exists.
- `save()` performs an upsert: if a document with the given `_id` exists, it replaces the existing document; if it does not exist, it inserts a new document.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Document Entity with TTL Index</strong></summary>

**Task**: Create a `VerificationToken` document entity containing `id`, `email`, `token`, and an `Instant createdAt` configured with a TTL index to expire after 15 minutes (900 seconds).

```java
@Document(collection = "verification_tokens")
public record VerificationToken(
    @Id String id,
    String email,
    String token,
    @Indexed(expireAfterSeconds = 900) Instant createdAt
) {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Derived Query Method in MongoRepository</strong></summary>

**Task**: Declare a repository interface for `VerificationToken` to find a token by `email` and `token` string.

```java
public interface VerificationTokenRepository extends MongoRepository<VerificationToken, String> {
    Optional<VerificationToken> findByEmailAndToken(String email, String token);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Atomic Field Increment with MongoTemplate</strong></summary>

**Task**: Implement a service method using `MongoTemplate` that atomically increments an article's `viewCount` by 1 and returns the updated count.

```java
public long incrementArticleViews(String articleId) {
    Query query = Query.query(Criteria.where("id").is(articleId));
    Update update = new Update().inc("viewCount", 1);
    FindAndModifyOptions options = FindAndModifyOptions.options().returnNew(true);

    Article updated = mongoTemplate.findAndModify(query, update, options, Article.class);
    return updated != null ? updated.getViewCount() : 0L;
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Compound Index Declaration</strong></summary>

**Task**: Annotate an `Order` document with a compound index on `customerId` ascending and `orderDate` descending.

```java
@Document(collection = "orders")
@CompoundIndex(name = "idx_customer_date", def = "{'customerId': 1, 'orderDate': -1}")
public class Order {
    @Id private String id;
    private Long customerId;
    private Instant orderDate;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Multi-Stage Aggregation Pipeline Query</strong></summary>

**Task**: Write an aggregation query calculating the total spending and order count per `customerId` for all orders with status `"SHIPPED"`.

```java
public List<CustomerSpendResult> calculateSpending() {
    Aggregation agg = Aggregation.newAggregation(
        Aggregation.match(Criteria.where("status").is("SHIPPED")),
        Aggregation.group("customerId")
            .sum("totalAmount").as("totalSpent")
            .count().as("orderCount"),
        Aggregation.project("totalSpent", "orderCount")
            .and("_id").as("customerId")
    );

    return mongoTemplate.aggregate(agg, "orders", CustomerSpendResult.class).getMappedResults();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Transactional Multi-Document Transfer Test</strong></summary>

**Task**: Write an integration test using Testcontainers MongoDB replica set verifying that an exception thrown during a `@Transactional` method rolls back document mutations across two accounts.

```java
@Test
void testTransactionRollback() {
    Account acc1 = mongoTemplate.save(new Account("acc-1", BigDecimal.valueOf(100)));
    Account acc2 = mongoTemplate.save(new Account("acc-2", BigDecimal.ZERO));

    assertThatThrownBy(() -> transferService.transferFunds("acc-1", "acc-2", BigDecimal.valueOf(50000)))
        .isInstanceOf(IllegalArgumentException.class);

    Account reloaded1 = mongoTemplate.findById("acc-1", Account.class);
    assertThat(reloaded1.getBalance()).isEqualByComparingTo("100");
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Annotations:  @Document(collection = "name"), @Id (ObjectId), @Indexed, @Field        |
| Operations:   MongoRepository for derived queries; MongoTemplate for complex criteria |
| Update Rule:  Use atomic update operators (new Update().inc().set()) over full saves   |
| Avoid:        @DBRef (incurs client-side N+1 round trips; embed data instead!)        |
| Analytics:    Use Aggregation.newAggregation(match, group, project, sort)             |
| Transactions: MongoTransactionManager enables standard Spring @Transactional          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data MongoDB Reference Documentation](https://docs.spring.io/spring-data/mongodb/reference/)
- [MongoDB Aggregation Pipeline Builder Guide](https://www.mongodb.com/docs/manual/core/aggregation-pipeline/)
- [MongoDB Multi-Document Transactions Overview](https://www.mongodb.com/docs/manual/core/transactions/)
- [Baeldung: Spring Data MongoDB Tutorial](https://www.baeldung.com/spring-data-mongodb-tutorial)
