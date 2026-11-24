# 12: Designing a Distributed Search Service with Elasticsearch & CDC

| Metadata Field | Value |
|---|---|
| **Topic** | Full-Text Search, Elasticsearch / OpenSearch, CDC with Debezium & Kafka |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | OpenSearch / Elasticsearch, Apache Kafka, Debezium (PostgreSQL CDC), PostgreSQL |
| **Scale Target** | 100M+ Searchable Documents, Sub-20ms Full-Text Query Latency |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Relational databases excel at transactional consistency and structured joins, but perform poorly on full-text, fuzzy, and faceted queries. Running SQL queries like `WHERE description ILIKE '%wireless noise cancelling%'` requires full-table scans, bypasses B-Tree indexes, and collapses database CPU at scale.

A dedicated **Distributed Search Service** offloads search workloads to an inverted-index search engine (**Elasticsearch** or **OpenSearch**). To keep search indices synchronized with the primary PostgreSQL database without polluting application code or risking dual-write inconsistencies, we employ **Change Data Capture (CDC)** using **Debezium** and **Apache Kafka**.

---

## 2. Core Concepts

### 2.1 Inverted Index Architecture
Instead of storing documents and scanning text, an inverted index maps individual tokenized words (stems, n-grams) to the list of document IDs that contain them. Lookups operate in $O(1)$ time regardless of document size.

### 2.2 Change Data Capture (CDC) via Debezium
Dual-writing (saving to PostgreSQL and then updating Elasticsearch in Spring Boot) suffers from partial failure vulnerabilities: if Elasticsearch fails, data drifts out of sync.
- With CDC, PostgreSQL remains the single source of truth.
- Debezium reads PostgreSQL's low-level **Write-Ahead Log (WAL)** via logical decoding.
- Every commit emits an event to Kafka (`cdc.inventory.products`), which a search indexing worker consumes to update OpenSearch idempotently.

### 2.3 Zero-Downtime Index Aliasing
Clients never query a physical index directly (e.g. `products_v1`). They query an **Index Alias** (`products_search`). When changing analyzers or mappings, a new index (`products_v2`) is built in the background. Once reindexing finishes, the alias is switched atomically with zero application downtime.

---

## 3. Internals & Architecture

```
        [ Client Search: "ergonomic wireless mouse" ]
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │                 Spring Boot Search Service              │
  │   - Executes BM25 Multi-Match + Facets + Highlighting   │
  └──────────────────────────┬──────────────────────────────┘
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │         OpenSearch / Elasticsearch Cluster              │
  │   Alias: "products_search" -> Index: "products_v2"      │
  │   - Inverted Index (Fast sub-15ms text matching)        │
  └─────────────────────────────────────────────────────────┘

                     [ INGESTION PIPELINE ]
  ┌─────────────────────────────────────────────────────────┐
  │            Primary Database: PostgreSQL                 │
  │   - User creates/updates product in 'products' table    │
  │   - Transaction commits to Write-Ahead Log (WAL)        │
  └──────────────────────────┬──────────────────────────────┘
                             │ (Logical Decoding)
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │             Debezium Kafka Connect Engine               │
  │   - Reads WAL changes without application intervention  │
  └──────────────────────────┬──────────────────────────────┘
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │              Apache Kafka Topic                         │
  │              "cdc.public.products"                      │
  └──────────────────────────┬──────────────────────────────┘
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────┐
  │           Spring Boot Search Indexer Worker             │
  │   - Transforms DB schema into rich search document      │
  │   - Executes Bulk Indexing into OpenSearch              │
  └─────────────────────────────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Zero-Downtime Index Alias Swap (REST API)
```json
POST /_aliases
{
  "actions": [
    { "remove": { "index": "products_v1", "alias": "products_search" } },
    { "add":    { "index": "products_v2", "alias": "products_search" } }
  ]
}
```

---

## 5. 5 Practical Examples

### Example 1: OpenSearch Product Document Model

```java
package com.example.search.domain;

import org.springframework.data.annotation.Id;
import org.springframework.data.elasticsearch.annotations.Document;
import org.springframework.data.elasticsearch.annotations.Field;
import org.springframework.data.elasticsearch.annotations.FieldType;

import java.math.BigDecimal;
import java.util.List;

@Document(indexName = "products_search")
public class ProductSearchDocument {

    @Id
    private String id;

    @Field(type = FieldType.Text, analyzer = "english")
    private String name;

    @Field(type = FieldType.Text, analyzer = "english")
    private String description;

    @Field(type = FieldType.Keyword)
    private String category;

    @Field(type = FieldType.Double)
    private BigDecimal price;

    @Field(type = FieldType.Keyword)
    private List<String> tags;

    @Field(type = FieldType.Boolean)
    private boolean inStock;

    public ProductSearchDocument() {}

    public ProductSearchDocument(String id, String name, String description, String category, BigDecimal price, List<String> tags, boolean inStock) {
        this.id = id;
        this.name = name;
        this.description = description;
        this.category = category;
        this.price = price;
        this.tags = tags;
        this.inStock = inStock;
    }

    public String getId() { return id; }
    public String getName() { return name; }
    public String getDescription() { return description; }
    public String getCategory() { return category; }
    public BigDecimal getPrice() { return price; }
}
```

### Example 2: Complex Full-Text & Faceted Search Query Service

```java
package com.example.search.service;

import co.elastic.clients.elasticsearch.ElasticsearchClient;
import co.elastic.clients.elasticsearch._types.query_dsl.BoolQuery;
import co.elastic.clients.elasticsearch.core.SearchResponse;
import com.example.search.domain.ProductSearchDocument;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.util.List;

@Service
public class ProductSearchQueryService {

    private final ElasticsearchClient esClient;

    public ProductSearchQueryService(ElasticsearchClient esClient) {
        this.esClient = esClient;
    }

    public record SearchResult(List<ProductSearchDocument> products, long totalHits) {}

    public SearchResult searchProducts(String queryText, String categoryFilter, int page, int size) throws IOException {
        SearchResponse<ProductSearchDocument> response = esClient.search(s -> s
                .index("products_search")
                .from(page * size)
                .size(size)
                .query(q -> q.bool(b -> buildBoolQuery(b, queryText, categoryFilter))),
                ProductSearchDocument.class
        );

        List<ProductSearchDocument> products = response.hits().hits().stream()
                .map(h -> h.source())
                .toList();

        long totalHits = response.hits().total() != null ? response.hits().total().value() : 0;
        return new SearchResult(products, totalHits);
    }

    private void buildBoolQuery(BoolQuery.Builder b, String queryText, String category) {
        // Multi-match across title and description with title boosting (^3)
        b.must(m -> m.multiMatch(mm -> mm
                .query(queryText)
                .fields("name^3", "description")
                .fuzziness("AUTO")
        ));

        // Exact filter for category
        if (category != null && !category.isBlank()) {
            b.filter(f -> f.term(t -> t.field("category").value(category)));
        }
        b.filter(f -> f.term(t -> t.field("inStock").value(true)));
    }
}
```

### Example 3: Debezium CDC Kafka Consumer & Index Synchronizer

```java
package com.example.search.cdc;

import com.example.search.domain.ProductSearchDocument;
import com.example.search.repository.ProductSearchRepository;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

import java.math.BigDecimal;
import java.util.Collections;

@Component
public class DebeziumCdcProductConsumer {

    private static final Logger log = LoggerFactory.getLogger(DebeziumCdcProductConsumer.class);
    private final ProductSearchRepository searchRepository;
    private final ObjectMapper objectMapper;

    public DebeziumCdcProductConsumer(ProductSearchRepository searchRepository, ObjectMapper objectMapper) {
        this.searchRepository = searchRepository;
        this.objectMapper = objectMapper;
    }

    @KafkaListener(topics = "cdc.public.products", groupId = "search-indexing-workers")
    public void onDebeziumEvent(String cdcJson) {
        try {
            JsonNode payload = objectMapper.readTree(cdcJson).get("payload");
            if (payload == null) return;

            String operation = payload.get("op").asText(); // 'c' (create), 'u' (update), 'd' (delete)

            if ("d".equals(operation)) {
                String deletedId = payload.get("before").get("id").asText();
                log.info("Deleting product {} from OpenSearch index...", deletedId);
                searchRepository.deleteById(deletedId);
            } else {
                JsonNode after = payload.get("after");
                ProductSearchDocument doc = new ProductSearchDocument(
                        after.get("id").asText(),
                        after.get("name").asText(),
                        after.get("description").asText(),
                        after.get("category").asText(),
                        new BigDecimal(after.get("price").asText()),
                        Collections.emptyList(),
                        after.get("in_stock").asBoolean()
                );
                log.info("Indexing updated product {} into OpenSearch...", doc.getId());
                searchRepository.save(doc);
            }

        } catch (Exception ex) {
            log.error("Failed to process CDC record for search index: {}", ex.getMessage());
        }
    }
}
```

### Example 4: Search REST Controller with Paging

```java
package com.example.search.web;

import com.example.search.service.ProductSearchQueryService;
import org.springframework.web.bind.annotation.*;

import java.io.IOException;

@RestController
@RequestMapping("/api/v1/search")
public class ProductSearchController {

    private final ProductSearchQueryService queryService;

    public ProductSearchController(ProductSearchQueryService queryService) {
        this.queryService = queryService;
    }

    @GetMapping
    public ProductSearchQueryService.SearchResult search(
            @RequestParam String q,
            @RequestParam(required = false) String category,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) throws IOException {

        return queryService.searchProducts(q, category, page, size);
    }
}
```

### Example 5: Zero-Downtime Reindexing Service

```java
package com.example.search.admin;

import co.elastic.clients.elasticsearch.ElasticsearchClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.io.IOException;

@Service
public class IndexMigrationService {

    private static final Logger log = LoggerFactory.getLogger(IndexMigrationService.class);
    private final ElasticsearchClient esClient;

    public IndexMigrationService(ElasticsearchClient esClient) {
        this.esClient = esClient;
    }

    public void switchAlias(String aliasName, String oldIndex, String newIndex) throws IOException {
        log.info("Switching alias {} from {} to {}", aliasName, oldIndex, newIndex);

        esClient.indices().updateAliases(a -> a.actions(
                act -> act.remove(r -> r.index(oldIndex).alias(aliasName)),
                act -> act.add(ad -> ad.index(newIndex).alias(aliasName))
        ));
    }
}
```

---

## 6. Best Practices

- **Never Dual-Write Directly from Spring Service**: If Elasticsearch is down during an order save, your data permanently drifts. Use Debezium CDC via Kafka.
- **Always Query via Aliases**: Never point application configurations to hardcoded index names (`products_v1`). Point them to an alias (`products_search`) to allow zero-downtime reindexing.
- **Use Multi-Match with Field Boosting**: Weight titles higher than descriptions: `"name^3", "description"`.
- **Apply Fuzziness Sparingly**: Use `fuzziness("AUTO")` for typo tolerance, but avoid high prefix fuzziness as it inflates memory and latency.
- **Batch CDC Indexing via Bulk API**: For high-velocity CDC events, buffer records for 500ms or 1,000 items and call Elasticsearch `bulk()` API to minimize network overhead.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Dual-Writing in Application Code** | If ES is slow or network drops, the DB commits but ES fails, leading to ghost products and sync drift. | Read PostgreSQL WAL via Debezium CDC and stream to Kafka. |
| **Directly Querying Concrete Index** | Changing mapping/analyzer requires modifying and redeploying all application configurations. | Always query via an Index Alias (`_aliases`). |
| **Using Elasticsearch as Primary DB** | ES does not provide ACID transactional guarantees or relational constraints; data loss can occur during cluster splits. | Treat PostgreSQL as source of truth; ES is an auxiliary search index. |
| **Unbounded Aggregations (Facets)** | Aggregating on high-cardinality text fields causes Java heap `OutOfMemoryError` on ES nodes. | Only aggregate on `keyword` fields and set execution sizes explicitly. |
| **Indexing Every Document Field** | Storing unsearched JSON columns inflates disk space and index size by 500%. | Only index searchable and filterable fields in the ES mapping. |
| **Over-Sharding** | Creating 20 shards for a 50,000-document index incurs massive Lucene overhead and slow queries. | Follow the rule: keep individual shard sizes between 20 GB and 50 GB. |

---

## 8. Performance & Security Considerations

### 8.1 Shard Sizing
Aim for **20 GB to 40 GB per primary shard**. For a 50 GB product catalog: 2 Primary Shards + 2 Replica Shards are optimal.

### 8.2 Securing Search Endpoints
Do not expose the OpenSearch cluster directly to public clients. Always proxy queries through Spring Boot to enforce authorization, input sanitization, and rate limiting.

---

## 9. Comparison / Decision Matrix

| Dimension | PostgreSQL Full-Text (`tsvector`) | Elasticsearch / OpenSearch | Apache Solr |
|---|---|---|---|
| **Max Scale** | $\sim 5\text{M}$ rows | $100\text{M}+$ documents | $100\text{M}+$ documents |
| **Fuzzy Matching** | Basic (Trigrams) | Advanced (Levenshtein, BM25) | Advanced |
| **Operational Overhead** | Zero (Already running PG) | High (Requires dedicated cluster) | High |
| **Faceted Filtering** | Slow at scale | Sub-15ms aggregations | Sub-15ms aggregations |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. Why is Change Data Capture (CDC) preferred over dual-writing to PostgreSQL and Elasticsearch?</strong></summary>

Dual-writing cannot guarantee atomicity across disparate data stores without complex 2PC protocols. If the database commit succeeds but Elasticsearch fails (or vice versa), data permanently diverges. CDC reads the DB transaction log directly, ensuring exactly-once stream capture and eventual consistency.
</details>

<details>
<summary><strong>2. How does an Inverted Index work in Elasticsearch?</strong></summary>

An inverted index tokenizes text into distinct stems and records a sorted dictionary mapping each word to the list of document IDs containing it. This allows instant lookups without scanning every document.
</details>

<details>
<summary><strong>3. How do you reindex 100 million documents without incurring downtime?</strong></summary>

1. Create a new index `products_v2` with updated mappings.
2. Run the Elasticsearch `_reindex` API in the background.
3. Switch the index alias `products_search` from `products_v1` to `products_v2` atomically using `_aliases`.
4. Delete `products_v1`.
</details>

<details>
<summary><strong>4. What is the BM25 scoring algorithm?</strong></summary>

BM25 (Best Matching 25) is the default ranking algorithm in Lucene. It calculates relevance based on Term Frequency (TF - how often the term appears in document), Inverse Document Frequency (IDF - how rare the term is across the entire index), and field-length saturation.
</details>

<details>
<summary><strong>5. How do you handle typo-tolerance and misspelling in search?</strong></summary>

Use fuzzy matching based on Damerau-Levenshtein distance (e.g. `fuzziness("AUTO")`). This allows up to 1 or 2 character edits (insertions, deletions, substitutions, transpositions).
</details>

<details>
<summary><strong>6. How many shards should you allocate for an index?</strong></summary>

Calculate total expected data size and divide by 30 GB (optimal shard size). Never allocate more primary shards than worker nodes for small indices.
</details>

<details>
<summary><strong>7. What is the difference between `text` and `keyword` field types in Elasticsearch?</strong></summary>

`text` fields are analyzed, tokenized, and lowercased for full-text search. `keyword` fields are stored exactly as provided without tokenization, ideal for exact matching, sorting, and faceted aggregations.
</details>

<details>
<summary><strong>8. How do you prevent search query timeouts from cascading?</strong></summary>

Configure explicit request timeouts (e.g., `timeout("2s")`) on search requests, and wrap search queries in a Resilience4j circuit breaker with fallback to popular cached search results.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Elasticsearch Document Entity</strong></summary>

**Prompt**: Define a Spring Data Elasticsearch entity for `Article` with a boosted title field and keyword category.

**Solution**:
Inspect Example 1: use `@Document`, `@Id`, `@Field(type = FieldType.Text)`, and `@Field(type = FieldType.Keyword)`.
</details>

<details>
<summary><strong>Exercise 2: Implement Multi-Match Query with Boost</strong></summary>

**Prompt**: Write a query searching across `title` (3x boost) and `content` with auto fuzziness.

**Solution**:
Inspect Example 2: call `mm.query(queryText).fields("title^3", "content").fuzziness("AUTO")`.
</details>

<details>
<summary><strong>Exercise 3: Write Debezium CDC Parser</strong></summary>

**Prompt**: Parse a Debezium JSON message and extract `op`, `before`, and `after` fields using Jackson.

**Solution**:
Inspect Example 3: inspect `payload.get("op")` and map `after` into a domain model.
</details>

<details>
<summary><strong>Exercise 4: Implement Zero-Downtime Alias Swapper</strong></summary>

**Prompt**: Write an administrative service method executing an atomic alias swap between two indices.

**Solution**:
Inspect Example 5: invoke `esClient.indices().updateAliases()` with remove and add actions.
</details>

<details>
<summary><strong>Exercise 5: Add Category Facet Aggregation</strong></summary>

**Prompt**: Add a terms aggregation to count matching products grouped by category.

**Solution**:
```java
SearchResponse<ProductSearchDocument> resp = esClient.search(s -> s
        .index("products_search")
        .aggregations("categories", a -> a.terms(t -> t.field("category"))),
        ProductSearchDocument.class
);
```
</details>

<details>
<summary><strong>Exercise 6: Test Search Service with Testcontainers OpenSearch</strong></summary>

**Prompt**: Write an integration test using Testcontainers OpenSearch asserting that searching for "nois" returns a product titled "Noise Cancelling Headphones".

**Solution**:
Seed OpenSearch test container, call `searchProducts("nois", null, 0, 10)`, and verify result list contains the document.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SEARCH SERVICE ARCHITECTURE CHEATSHEET               │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Architecture: PostgreSQL (Source of Truth) -> Debezium CDC -> Kafka │
│ 2. Index Sync: Search worker consumes Kafka & updates OpenSearch bulk  │
│ 3. Querying: Always query via Alias (products_search), never raw index │
│ 4. Reindexing: Create v2, reindex background, swap alias with 0-downtime│
│ 5. Mapping: 'text' for full-text search; 'keyword' for filtering/facets│
│ 6. Sizing: Target 20 GB to 40 GB per primary shard                     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Elasticsearch Java API Client Documentation](https://www.elastic.co/guide/en/elasticsearch/client/java-api-client/current/index.html)
- [Debezium PostgreSQL Connector Documentation](https://debezium.io/documentation/reference/stable/connectors/postgresql.html)
- [OpenSearch Project Official Guide](https://opensearch.org/docs/latest/)
- [Lucene Scoring: The BM25 Algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
