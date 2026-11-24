# Spring Data Elasticsearch: Inverted Indexes, Document Mapping & Full-Text Search

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | NoSQL & Lucene search concepts, JSON REST APIs, Distributed indexing |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data Elasticsearch 5.4.x, Elasticsearch 8.15+ / OpenSearch 2.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Document Entity Mapping with Text and Keyword Analyzers](#51-document-entity-mapping-with-text-and-keyword-analyzers)
   - [5.2 Full-Text Search and Fuzzy Matching with NativeQuery](#52-full-text-search-and-fuzzy-matching-with-nativequery)
   - [5.3 Search Result Highlighting and Snippet Extraction](#53-search-result-highlighting-and-snippet-extraction)
   - [5.4 Anti-Pattern: Treating Elasticsearch as a Primary Source of Truth](#54-anti-pattern-treating-elasticsearch-as-a-primary-source-of-truth)
   - [5.5 Multi-Field Search with Dynamic Aggregations (Faceting)](#55-multi-field-search-with-dynamic-aggregations-faceting)
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
Relational databases and document stores execute exact-match queries and basic range filters efficiently, but they degrade severely when handling full-text search requirements: fuzzy matching ("iPhone" matching "ifone"), typo tolerance, relevance scoring (BM25 algorithms), stemming, multi-lingual synonym matching, and search highlighting. Running SQL queries like `WHERE description LIKE '%phone%'` forces full table scans that lock database CPU resources and cannot rank results by relevance.

### Historical Evolution
Apache Lucene pioneered inverted-index full-text search in Java. Elasticsearch distributed Lucene across clusters of shards and nodes, exposing JSON HTTP APIs. Spring Data Elasticsearch integrates Elasticsearch into Spring Boot applications, providing familiar declarative repositories (`ElasticsearchRepository`), template operations (`ElasticsearchOperations`), and modern client integration powered by the official **Elasticsearch Java API Client 8.x**.

### Real-World Relevance
Elasticsearch powers e-commerce product catalogs with faceted filters (filtering by brand, size, price range), enterprise documentation search, application log aggregators (ELK stack), and security analytics. Integrating Spring Data Elasticsearch allows developers to build search features with type safety and low latency.

---

## 2. Core Concepts

### Inverted Index Architecture
Unlike relational databases that map rows to columns, an inverted index maps individual **terms (words)** to the list of documents containing that term:
$$\text{Term} \longrightarrow [\text{Doc 1}, \text{Doc 4}, \text{Doc 12}]$$
This allows finding all documents matching a query in $O(1)$ time regardless of index size.

### FieldType.Text vs FieldType.Keyword
- **`FieldType.Text`**: Analyzed and tokenized. The text is passed through an analyzer (lowercased, punctuation removed, stemmed into root words). Ideal for full-text search in descriptions, articles, and titles. **Cannot be used for exact sorting or aggregations**.
- **`FieldType.Keyword`**: Stored as a single exact string token without analysis. Ideal for exact filtering, sorting, status codes, IDs, and aggregations (facets).
- **Multi-Fields**: Storing a field as both `Text` (for search) and a `keyword` sub-field (for sorting).

### Core Spring Data Annotations
- `@Document(indexName = "products")`: Maps a class to an Elasticsearch index.
- `@Id`: Maps the document identifier (`_id`).
- `@Field(type = FieldType.*)`: Configures Lucene field type, analyzer, and search analyzer.
- `@Highlight`: Declaratively extracts highlighted search snippets matching query terms.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                      Spring Data Elasticsearch Architecture                        |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller: productRepository.search("wireless headphones")                          |
|       |                                                                            |
|       v                                                                            |
|  ElasticsearchTemplate / ElasticsearchOperations                                   |
|       |                                                                            |
|       v                                                                            |
|  Elasticsearch Java API Client 8.x                                                 |
|  - Serializes criteria into Elasticsearch JSON DSL query:                          |
|    {"query": {"multi_match": {"query": "wireless headphones", "fuzziness": "AUTO"}}}|
|       |                                                                            |
|       v (HTTP / REST Wire Protocol)                                                |
|  Elasticsearch Node (HTTP Port 9200)                                               |
|  - Routes query to Shards containing Lucene Inverted Indexes                       |
|  - Calculates BM25 relevance scores                                                |
|  - Collects top-N hits and highlights matching tokens                              |
|       |                                                                            |
|       v                                                                            |
|  SearchHits<Product> mapped into Java 21 Records / POJOs                           |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-data-elasticsearch</artifactId>
</dependency>
```

### Basic Application Configuration (`application.yaml`)
```yaml
spring:
  elasticsearch:
    uris: http://localhost:9200
    connection-timeout: 3s
    socket-timeout: 10s
```

---

## 5. Practical Examples

### 5.1 Document Entity Mapping with Text and Keyword Analyzers
```java
package com.example.es.domain;

import org.springframework.data.annotation.Id;
import org.springframework.data.elasticsearch.annotations.Document;
import org.springframework.data.elasticsearch.annotations.Field;
import org.springframework.data.elasticsearch.annotations.FieldType;
import org.springframework.data.elasticsearch.annotations.MultiField;
import org.springframework.data.elasticsearch.annotations.InnerField;

import java.math.BigDecimal;
import java.time.Instant;

@Document(indexName = "catalog_products")
public class CatalogProduct {

    @Id
    private String id;

    // Multi-field: Analyzed Text for full-text search, with a raw 'keyword' subfield for sorting!
    @MultiField(
        mainField = @Field(type = FieldType.Text, analyzer = "standard"),
        otherFields = {
            @InnerField(suffix = "keyword", type = FieldType.Keyword)
        }
    )
    private String name;

    @Field(type = FieldType.Text, analyzer = "english")
    private String description;

    @Field(type = FieldType.Keyword)
    private String category;

    @Field(type = FieldType.Keyword)
    private String brand;

    @Field(type = FieldType.Double)
    private BigDecimal price;

    @Field(type = FieldType.Date)
    private Instant createdAt = Instant.now();

    public CatalogProduct(String id, String name, String description, String category, BigDecimal price) {
        this.id = id;
        this.name = name;
        this.description = description;
        this.category = category;
        this.price = price;
    }

    public String getId() { return id; }
    public String getName() { return name; }
    public String getDescription() { return description; }
    public BigDecimal getPrice() { return price; }
}
```

### 5.2 Full-Text Search and Fuzzy Matching with NativeQuery
```java
package com.example.es.service;

import com.example.es.domain.CatalogProduct;
import co.elastic.clients.elasticsearch._types.query_dsl.QueryBuilders;
import org.springframework.data.domain.PageRequest;
import org.springframework.data.elasticsearch.client.elc.NativeQuery;
import org.springframework.data.elasticsearch.client.elc.NativeQueryBuilder;
import org.springframework.data.elasticsearch.core.ElasticsearchOperations;
import org.springframework.data.elasticsearch.core.SearchHit;
import org.springframework.data.elasticsearch.core.SearchHits;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class ProductSearchService {

    private final ElasticsearchOperations operations;

    public ProductSearchService(ElasticsearchOperations operations) {
        this.operations = operations;
    }

    /**
     * Executes fuzzy multi-match search across 'name' and 'description' fields,
     * applying typo tolerance and boosting the name field weight by 3x.
     */
    public List<CatalogProduct> searchWithFuzzy(String searchTerm, int page, int size) {
        NativeQuery query = new NativeQueryBuilder()
            .withQuery(QueryBuilders.multiMatch(mm -> mm
                .query(searchTerm)
                .fields("name^3", "description") // Boosting name 3x
                .fuzziness("AUTO")              // Auto typo-tolerance (e.g. 'samung' -> 'samsung')
            ))
            .withPageable(PageRequest.of(page, size))
            .build();

        SearchHits<CatalogProduct> searchHits = operations.search(query, CatalogProduct.class);

        return searchHits.getSearchHits().stream()
            .map(SearchHit::getContent)
            .toList();
    }
}
```

### 5.3 Search Result Highlighting and Snippet Extraction
```java
package com.example.es.service;

import com.example.es.domain.CatalogProduct;
import org.springframework.data.elasticsearch.client.elc.NativeQuery;
import org.springframework.data.elasticsearch.client.elc.NativeQueryBuilder;
import org.springframework.data.elasticsearch.core.ElasticsearchOperations;
import org.springframework.data.elasticsearch.core.SearchHit;
import org.springframework.data.elasticsearch.core.SearchHits;
import org.springframework.data.elasticsearch.core.query.highlight.Highlight;
import org.springframework.data.elasticsearch.core.query.highlight.HighlightField;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class HighlightSearchService {

    private final ElasticsearchOperations operations;

    public HighlightSearchService(ElasticsearchOperations operations) {
        this.operations = operations;
    }

    public record HighlightedProductResult(
        CatalogProduct product,
        List<String> matchedSnippets
    ) {}

    public List<HighlightedProductResult> searchWithHighlighting(String keyword) {
        NativeQuery query = new NativeQueryBuilder()
            .withQuery(q -> q.match(m -> m.field("description").query(keyword)))
            .withHighlightQuery(new Highlight(List.of(
                new HighlightField("description")
            )))
            .build();

        SearchHits<CatalogProduct> hits = operations.search(query, CatalogProduct.class);

        return hits.getSearchHits().stream()
            .map(hit -> new HighlightedProductResult(
                hit.getContent(),
                hit.getHighlightField("description") // Returns e.g. ["Wireless <em>headphones</em> with noise cancellation"]
            ))
            .toList();
    }
}
```

### 5.4 Anti-Pattern: Treating Elasticsearch as a Primary Source of Truth
```java
// =========================================================================
// ANTI-PATTERN: Using Elasticsearch as the Single Primary Transactional Database
// =========================================================================
@Service
public class BadOrderPlacementService {
    @Autowired private OrderElasticsearchRepository esRepo;

    public void processPaymentAndSaveOrder(Order order) {
        // CATASTROPHIC ARCHITECTURAL MISTAKE:
        // 1. Elasticsearch is NOT an ACID database! It lacks multi-document transactions.
        // 2. Near-Real-Time (NRT): Newly indexed documents are not readable until refreshed (up to 1s delay).
        // 3. Lucene flushes to disk asynchronously; network partitions can drop uncommitted segments!
        esRepo.save(order);
    }
}

// =========================================================================
// PRODUCTION FIX: Dual-Store Architecture with CDC or Outbox Pattern
// =========================================================================
@Service
public class GoodOrderPlacementService {
    @Autowired private OrderJpaRepository jpaRepo; // Primary ACID Source of Truth
    @Autowired private ApplicationEventPublisher eventPublisher;

    @Transactional
    public void processPaymentAndSaveOrder(Order order) {
        // 1. Write to PostgreSQL in a strict ACID transaction
        jpaRepo.save(order);

        // 2. Publish event to asynchronous worker (Kafka/Outbox) to synchronize Elasticsearch
        eventPublisher.publishEvent(new OrderIndexedEvent(order.getId()));
    }
}
```

### 5.5 Multi-Field Search with Dynamic Aggregations (Faceting)
```java
package com.example.es.service;

import com.example.es.domain.CatalogProduct;
import co.elastic.clients.elasticsearch._types.aggregations.StringTermsBucket;
import org.springframework.data.elasticsearch.client.elc.NativeQuery;
import org.springframework.data.elasticsearch.client.elc.NativeQueryBuilder;
import org.springframework.data.elasticsearch.core.ElasticsearchOperations;
import org.springframework.data.elasticsearch.core.SearchHits;
import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.stream.Collectors;

@Service
public class FacetedSearchService {

    private final ElasticsearchOperations operations;

    public FacetedSearchService(ElasticsearchOperations operations) {
        this.operations = operations;
    }

    public record FacetSearchResult(
        long totalMatches,
        Map<String, Long> categoryCounts
    ) {}

    /**
     * Executes query and computes aggregated counts per category in a single round-trip.
     */
    public FacetSearchResult searchWithCategoryFacets(String keyword) {
        NativeQuery query = new NativeQueryBuilder()
            .withQuery(q -> q.match(m -> m.field("name").query(keyword)))
            .withAggregation("categories_facet", a -> a.terms(t -> t.field("category")))
            .build();

        SearchHits<CatalogProduct> hits = operations.search(query, CatalogProduct.class);

        // Extract aggregation buckets
        var aggResponse = hits.getAggregations();
        var termsAgg = aggResponse.get("categories_facet").aggregation().getAggregate().sterms();

        Map<String, Long> facetCounts = termsAgg.buckets().array().stream()
            .collect(Collectors.toMap(
                StringTermsBucket::key,
                StringTermsBucket::docCount
            ));

        return new FacetSearchResult(hits.getTotalHits(), facetCounts);
    }
}
```

---

## 6. Best Practices

1. **Use Multi-Fields for Searchable Text That Requires Sorting**: Map text fields using `@MultiField` with a `keyword` subfield so you can full-text search the text and sort/aggregate on `.keyword`.
2. **Never Treat Elasticsearch as the Primary Source of Truth**: Always store financial, transactional, and customer records in an ACID database (PostgreSQL/MySQL); synchronize to Elasticsearch asynchronously for searching.
3. **Use Explicit Index Aliases**: Never point applications directly to physical index names (e.g. `products_v1`). Point to a logical alias (`products`), allowing zero-downtime reindexing via alias swaps.
4. **Tune Fuzziness Appropriately**: Use `fuzziness("AUTO")` (Levenshtein distance 1 for 3–5 chars, 2 for $>5$ chars) to provide typo tolerance without degrading search precision.
5. **Always Set Sane Page Sizes**: Requesting large pages (`size = 10000`) forces Elasticsearch to allocate huge Lucene priority queues in memory. Use `SearchAfter` for deep scrolling.
6. **Configure Read and Socket Timeouts**: Always set client socket timeouts (e.g. 5–10 seconds) to prevent slow search cluster responses from freezing application worker threads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Sorting on an Analyzed `FieldType.Text` Field** | Throws `Fielddata is disabled on text fields` runtime exception. | Sort on a `FieldType.Keyword` or multi-field subfield (`name.keyword`). |
| **Expecting Immediate Read Consistency (NRT)** | Elasticsearch is Near-Real-Time; newly indexed documents are not visible until refreshed (default: 1s). | Accept eventual consistency or trigger explicit refresh in tests. |
| **Over-Sharding Small Datasets** | Creating 10 shards for a 5,000-document index wastes Lucene overhead and memory. | Use 1 primary shard for small/medium indexes. |
| **Storing Large Unindexed Payloads** | Bloats the inverted index and wastes Lucene heap memory. | Exclude non-searchable metadata or store references to S3/PostgreSQL. |
| **Using Wildcard Searches with Leading Asterisks (`*phone`)** | Bypasses the inverted index, forcing a full scan across all index terms. | Use edge-ngram analyzers or reverse token filters. |
| **Creating Deep Unbounded Pagination (`from + size > 10000`)** | Elasticsearch caps `index.max_result_window` at 10,000 to prevent OOM. | Use `SearchAfter` with point-in-time (PIT) for deep pagination. |

---

## 8. Performance & Security Notes

### Performance
- **Sub-Second Search Over Millions of Documents**: Inverted indexes resolve complex Boolean full-text queries in single-digit milliseconds ($<10\text{ ms}$).
- **BM25 Relevance Scoring**: Lucene evaluates term frequency ($TF$) and inverse document frequency ($IDF$) to rank results by true contextual relevance rather than simple keyword counts.

### Security
- **Index Access Control**: Enforce TLS encryption and role-based access control (RBAC) via Elasticsearch Security / Search Guard. Ensure Spring applications authenticate using dedicated API keys with restricted index privileges.
- **Query Injection Prevention**: Construct queries using Spring Data's fluent `NativeQuery` or `CriteriaQuery` builders to prevent raw JSON injection.

---

## 9. Comparison / When to Use What

| Feature | Elasticsearch | PostgreSQL (Full-Text Search) | MongoDB |
| :--- | :--- | :--- | :--- |
| **Primary Focus** | **Specialized Search & Analytics** | Relational ACID Database | Document Store |
| **Typo Tolerance / Fuzzy** | **Built-in (`fuzziness: AUTO`)** | Limited (requires `pg_trgm`) | Basic text index |
| **Relevance Scoring** | **Advanced (BM25, boosting)** | Basic rank calculation | Basic weights |
| **ACID Multi-Document** | None (Eventual consistency) | **Full ACID** | Multi-Document (Replica) |
| **Best Used For** | Product search, logs, faceted browsing | Hybrid transactional + simple search | Rapid document prototyping |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is an Inverted Index and how does it enable sub-millisecond search?</strong></summary>

An inverted index is a database data structure that inverts the traditional document-to-word relationship. Instead of listing words contained inside a document, it builds an index mapping every unique word (term) to the set of document IDs that contain it. When a search query arrives, the search engine looks up the term in the dictionary ($O(1)$) and immediately retrieves the list of matching document IDs without scanning any documents.
</details>

<details>
<summary><strong>2. What is the fundamental difference between <code>FieldType.Text</code> and <code>FieldType.Keyword</code>?</strong></summary>

- `FieldType.Text` is analyzed: it is processed by an analyzer that tokenizes sentences, lowercases words, strips punctuation, and applies language stemming. It is used exclusively for full-text search and cannot be used for sorting or aggregations.
- `FieldType.Keyword` is stored as an exact, unanalyzed string. It is used for exact-match filtering, sorting, aggregations, and facets.
</details>

<details>
<summary><strong>3. Why is Elasticsearch described as "Near-Real-Time" (NRT)?</strong></summary>

When a document is indexed in Elasticsearch, it is written to an in-memory buffer and a transaction log (translog). It is not immediately searchable until an index **refresh** occurs (default: every 1 second), which opens a new in-memory Lucene segment. Hence, documents become visible for search within approximately 1 second of insertion.
</details>

<details>
<summary><strong>4. What is a Multi-Field in Spring Data Elasticsearch?</strong></summary>

A Multi-Field (configured via `@MultiField`) indexes the exact same entity field in multiple ways. For example, a `name` field can be mapped primarily as `Text` for full-text search, while also generating a secondary sub-field `name.keyword` as a `Keyword` for sorting and aggregation.
</details>

<details>
<summary><strong>5. How does Elasticsearch compute document relevance scores?</strong></summary>

Elasticsearch uses the **Okapi BM25** algorithm by default. BM25 evaluates:
1. **Term Frequency (TF)**: How frequently the term appears in the specific document.
2. **Inverse Document Frequency (IDF)**: How rare the term is across the entire index (rare words get higher weights).
3. **Field Length Normalization**: Matches in shorter fields (e.g. titles) are weighted more heavily than matches in long text blocks.
</details>

<details>
<summary><strong>6. Why should you avoid <code>from + size > 10,000</code> in Elasticsearch pagination?</strong></summary>

Elasticsearch enforces `index.max_result_window = 10,000`. When you request `from = 100,000, size = 10`, each shard must collect 100,010 documents and pass them to the coordinating node, which merges and sorts them in heap memory before discarding 100,000 rows. This can cause high memory usage and garbage collection pauses. For deep pagination, developers should use `search_after`.
</details>

<details>
<summary><strong>7. What is an Index Alias and why is it important in production?</strong></summary>

An Index Alias is a logical pointer that can point to one or more physical indexes. In production, applications always query and write to an alias (`products`) rather than a versioned index (`products_v1`). When changing mapping or analyzers, developers build `products_v2` in the background and atomically update the alias pointer, achieving zero-downtime reindexing.
</details>

<details>
<summary><strong>8. Why is treating Elasticsearch as your primary transactional database an anti-pattern?</strong></summary>

Elasticsearch lacks multi-document ACID transactions, does not provide foreign key constraints, suffers from near-real-time latency, and can lose uncommitted data during split-brain network failures. It should be used as a read-optimized secondary search index alongside a primary ACID datastore.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Document Entity Declaration</strong></summary>

**Task**: Create an entity `ArticleDocument` mapped to index `"articles"` with `id`, `title` (Text), and `status` (Keyword).

```java
@Document(indexName = "articles")
public record ArticleDocument(
    @Id String id,
    @Field(type = FieldType.Text) String title,
    @Field(type = FieldType.Keyword) String status
) {}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): ElasticsearchRepository Search Method</strong></summary>

**Task**: Declare a repository for `ArticleDocument` with a derived query method finding articles by status.

```java
public interface ArticleRepository extends ElasticsearchRepository<ArticleDocument, String> {
    List<ArticleDocument> findByStatus(String status);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Multi-Field Name Mapping</strong></summary>

**Task**: Map a `Book` entity where `title` is analyzed as Text and has a `raw` keyword sub-field.

```java
@Document(indexName = "books")
public class Book {
    @Id private String id;

    @MultiField(
        mainField = @Field(type = FieldType.Text),
        otherFields = { @InnerField(suffix = "raw", type = FieldType.Keyword) }
    )
    private String title;
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Native Match Query with Fuzziness</strong></summary>

**Task**: Write an `ElasticsearchOperations` search method that searches books by title with `AUTO` fuzziness.

```java
public List<Book> searchBooksFuzzy(String keyword) {
    NativeQuery query = new NativeQueryBuilder()
        .withQuery(q -> q.match(m -> m.field("title").query(keyword).fuzziness("AUTO")))
        .build();
    return operations.search(query, Book.class).map(SearchHit::getContent).toList();
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Highlighting Search Query Implementation</strong></summary>

**Task**: Implement a search method that queries the `content` field and returns the first highlighted snippet wrapped in `<em>` tags.

```java
public List<String> searchHighlights(String phrase) {
    NativeQuery query = new NativeQueryBuilder()
        .withQuery(q -> q.match(m -> m.field("content").query(phrase)))
        .withHighlightQuery(new Highlight(List.of(new HighlightField("content"))))
        .build();

    return operations.search(query, ArticleDocument.class).getSearchHits().stream()
        .flatMap(hit -> hit.getHighlightField("content").stream())
        .toList();
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test with Real Elasticsearch Container</strong></summary>

**Task**: Write a test using Testcontainers Elasticsearch asserting that inserting a document and querying with typo tolerance successfully returns the document.

```java
@Test
void testTypoTolerance() {
    operations.save(new ArticleDocument("1", "Spring Boot Microservices", "PUBLISHED"));
    operations.indexOps(ArticleDocument.class).refresh(); // Force refresh for immediate searchability

    // Search with typo: "Microsrvices"
    List<ArticleDocument> results = searchService.searchWithFuzzy("Microsrvices", 0, 10);
    assertThat(results).isNotEmpty();
    assertThat(results.get(0).title()).isEqualTo("Spring Boot Microservices");
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Text vs Keyword: Text = Analyzed/tokenized (Full-Text); Keyword = Exact string/sort   |
| Multi-Field:     @MultiField(mainField = Text, otherFields = InnerField(raw, Keyword))|
| Relevance:       BM25 algorithm evaluates term frequency and inverse document frequency|
| Highlighting:    new Highlight(List.of(new HighlightField("field")))                  |
| Golden Rule 1:   NEVER use Elasticsearch as your primary transactional ACID database   |
| Golden Rule 2:   NEVER sort on analyzed Text fields (throws runtime Fielddata error)  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data Elasticsearch Official Reference Documentation](https://docs.spring.io/spring-data/elasticsearch/reference/)
- [Elasticsearch Java API Client 8.x Guide](https://www.elastic.co/guide/en/elasticsearch/client/java-api-client/current/index.html)
- [Lucene Inverted Index Architecture Deep Dive](https://lucene.apache.org/core/)
- [Baeldung: Introduction to Spring Data Elasticsearch](https://www.baeldung.com/spring-data-elasticsearch-tutorial)
