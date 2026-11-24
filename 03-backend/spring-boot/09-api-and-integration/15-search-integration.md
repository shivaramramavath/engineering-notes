# Full-Text Search Integration with OpenSearch and Elasticsearch

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `org.opensearch.client:opensearch-java:2.16.x`, `co.elastic.clients:elasticsearch-java:8.15.x` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Relational databases (PostgreSQL, MySQL, Oracle) are optimized for transactional integrity (ACID) and structured B-Tree index lookups. When applications require rich full-text search (fuzzy search, stemming, autocomplete, multi-word relevance scoring, faceted filtering, synonym matching, and geo-distance queries across millions of text documents), traditional SQL `LIKE '%keyword%'` queries result in full table scans, high disk I/O, database CPU exhaustion, and multi-second response latencies.

### The Evolution
Distributed search engines built on Apache Lucene (such as OpenSearch and Elasticsearch) solve this by utilizing **Inverted Indexes**. Instead of scanning rows sequentially, an inverted index maintains a dictionary of terms mapped to lists of document IDs. In the Spring ecosystem, earlier high-level abstractions like Spring Data Elasticsearch have evolved alongside the official modern Java clients (`opensearch-java` and `elasticsearch-java`), providing strongly-typed, non-blocking, builder-based query APIs that map JSON search requests and aggregations directly to Java records.

### Relevance in Modern Engineering
E-commerce product discovery, distributed log aggregation, legal document archiving, and real-time analytical dashboards require resilient search integration. Spring Boot applications synchronize domain writes from PostgreSQL/MySQL into OpenSearch/Elasticsearch using change-data-capture (Debezium/Kafka) or transactional event listeners, powering sub-50ms search experiences.

---

## 2. Core Concepts

- **Inverted Index**: The foundational data structure mapping individual words/tokens to the documents and positional offsets where they appear.
- **Index & Document**: An *Index* is a collection of documents sharing similar characteristics (analogous to an SQL table); a *Document* is a JSON object stored with a unique ID.
- **Analyzers (Tokenizer + Filters)**: Text processing pipeline that breaks sentences into tokens (e.g., lowercase filter, stop-word removal, Porter stemming).
- **Match vs Term Query**:
  - `match`: Full-text search that runs input through the field analyzer (analyzed, fuzzy, case-insensitive).
  - `term`: Exact match query without analysis (exact IDs, statuses, keywords).
- **Bool Query (Must, Should, Filter, MustNot)**: Compound query combining relevance scoring (`must`, `should`) with un-scored caching filters (`filter`, `must_not`).
- **Faceted Aggregations**: Grouping query results into categorized bucket counts (e.g., categories, price ranges, brand tallies) executed concurrently with the search.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring Boot Search Engine Flow                             |
|                                                                                   |
| 1. Client Search Query: "cordless drill"                                          |
|    GET /api/v1/search/products?q=cordless+drill&brand=DeWalt&maxPrice=150         |
|                   |                                                               |
|                   v                                                               |
| 2. Spring Service Constructs Query via Java Client Builder                        |
|    +---------------------------------------------------------------------+        |
|    | Query = BoolQuery.of(b -> b                                         |        |
|    |   .must(m -> m.match(t -> t.field("name").query("cordless drill"))) |        |
|    |   .filter(f -> f.term(t -> t.field("brand.keyword").value("DeWalt"))|        |
|    |   .filter(f -> f.range(r -> r.field("price").lte(150.0)))           |        |
|    | )                                                                   |        |
|    +---------------------------------------------------------------------+        |
|                   | Serializes to JSON over HTTP/REST                             |
|                   v                                                               |
| 3. OpenSearch / Elasticsearch Cluster                                             |
|    +---------------------------------------------------------------------+        |
|    | Primary & Replica Shards query Inverted Indexes in parallel         |        |
|    | BM25 Relevance Scoring ranks results + Aggregations computed        |        |
|    +---------------------------------------------------------------------+        |
|                   |                                                               |
|                   v                                                               |
| 4. Client Deserializes Response Hits to Java Records                              |
|    SearchResponse<ProductDocument> hits.forEach(h -> h.source())                 |
+-----------------------------------------------------------------------------------+
```

When an inquiry is initiated, the Spring Boot application utilizes the `OpenSearchClient` or `ElasticsearchClient` to construct an immutable query tree. The client serializes the query into an HTTP JSON request using Jackson and dispatches it over a connection pool to the search cluster. The search engine coordinates shard execution, calculates BM25 text relevance scores, applies filters without scoring (which benefit from Lucene bitset caching), computes facet aggregations, and returns the top hits. The Java client unmarshals hits directly into domain records.

---

## 4. Syntax & Usage

### Maven Dependency (OpenSearch Java Client)
```xml
<dependency>
    <groupId>org.opensearch.client</groupId>
    <artifactId>opensearch-java</artifactId>
    <version>2.16.0</version>
</dependency>
<dependency>
    <groupId>org.apache.httpcomponents.client5</groupId>
    <artifactId>httpclient5</artifactId>
</dependency>
```

### Application Properties
```properties
# OpenSearch / Elasticsearch Cluster Connection
search.host=localhost
search.port=9200
search.scheme=http
search.username=admin
search.password=admin
```

---

## 5. Practical Examples

### Example 1: OpenSearch Client Configuration Bean
```java
package com.example.platform.search.config;

import org.apache.hc.client5.http.auth.AuthScope;
import org.apache.hc.client5.http.auth.UsernamePasswordCredentials;
import org.apache.hc.client5.http.impl.auth.BasicCredentialsProvider;
import org.apache.hc.core5.http.HttpHost;
import org.opensearch.client.OpenSearchClient;
import org.opensearch.client.json.jackson.JacksonJsonpMapper;
import org.opensearch.client.transport.OpenSearchTransport;
import org.opensearch.client.transport.httpclient5.ApacheHttpClient5TransportBuilder;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class OpenSearchConfig {

    @Value("${search.host:localhost}")
    private String host;

    @Value("${search.port:9200}")
    private int port;

    @Value("${search.scheme:http}")
    private String scheme;

    @Value("${search.username:admin}")
    private String username;

    @Value("${search.password:admin}")
    private String password;

    @Bean
    public OpenSearchClient openSearchClient() {
        HttpHost httpHost = new HttpHost(scheme, host, port);

        BasicCredentialsProvider credentialsProvider = new BasicCredentialsProvider();
        credentialsProvider.setCredentials(
                new AuthScope(httpHost),
                new UsernamePasswordCredentials(username, password.toCharArray()));

        OpenSearchTransport transport = ApacheHttpClient5TransportBuilder
                .builder(new HttpHost[]{httpHost})
                .setMapper(new JacksonJsonpMapper())
                .setHttpClientConfigCallback(httpClientBuilder ->
                        httpClientBuilder.setDefaultCredentialsProvider(credentialsProvider))
                .build();

        return new OpenSearchClient(transport);
    }
}
```

### Example 2: Document Model Record and Indexing Service
```java
package com.example.platform.search.service;

import org.opensearch.client.OpenSearchClient;
import org.opensearch.client.opensearch.core.IndexResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.util.List;

public record ProductDocument(
        String id,
        String name,
        String description,
        String brand,
        String category,
        double price,
        List<String> tags
) {}

@Service
public class ProductIndexingService {

    private static final Logger log = LoggerFactory.getLogger(ProductIndexingService.class);
    private static final String INDEX_NAME = "products";
    private final OpenSearchClient client;

    public ProductIndexingService(OpenSearchClient client) {
        this.client = client;
    }

    public void indexProduct(ProductDocument product) {
        try {
            IndexResponse response = client.index(i -> i
                    .index(INDEX_NAME)
                    .id(product.id())
                    .document(product)
            );
            log.info("Document indexed. Result: {}, Version: {}", response.result(), response.version());
        } catch (IOException e) {
            log.error("Failed to index product document ID: {}", product.id(), e);
            throw new RuntimeException("Indexing failure", e);
        }
    }
}
```

### Example 3: Compound Search with Relevance Scoring and Filtering
```java
package com.example.platform.search.service;

import org.opensearch.client.OpenSearchClient;
import org.opensearch.client.opensearch._types.query_dsl.Query;
import org.opensearch.client.opensearch.core.SearchResponse;
import org.opensearch.client.opensearch.core.search.Hit;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.util.List;

@Service
public class ProductSearchService {

    private static final String INDEX_NAME = "products";
    private final OpenSearchClient client;

    public ProductSearchService(OpenSearchClient client) {
        this.client = client;
    }

    public record SearchResult(List<ProductDocument> products, long totalHits) {}

    public SearchResult searchProducts(String searchTerm, String brandFilter, Double maxPrice, int page, int size) {
        try {
            SearchResponse<ProductDocument> response = client.search(s -> s
                    .index(INDEX_NAME)
                    .from(page * size)
                    .size(size)
                    .query(q -> q.bool(b -> {
                        // 1. Full-text relevance search
                        if (searchTerm != null && !searchTerm.isBlank()) {
                            b.must(m -> m.multiMatch(mm -> mm
                                    .fields("name^3", "description") // name has 3x boost
                                    .query(searchTerm)
                                    .fuzziness("AUTO")
                            ));
                        } else {
                            b.must(m -> m.matchAll(ma -> ma));
                        }

                        // 2. Unscored, cached filters
                        if (brandFilter != null && !brandFilter.isBlank()) {
                            b.filter(f -> f.term(t -> t.field("brand.keyword").value(brandFilter)));
                        }
                        if (maxPrice != null) {
                            b.filter(f -> f.range(r -> r.field("price").lte(org.opensearch.client.json.JsonData.of(maxPrice))));
                        }

                        return b;
                    })),
                    ProductDocument.class
            );

            List<ProductDocument> items = response.hits().hits().stream()
                    .map(Hit::source)
                    .toList();

            long total = response.hits().total() != null ? response.hits().total().value() : 0;
            return new SearchResult(items, total);

        } catch (IOException e) {
            throw new RuntimeException("Search execution failed", e);
        }
    }
}
```

### Example 4: Match vs Term Query on Analyzed Text (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Executing a term query against a text field that was processed by a standard analyzer.
// Text analyzer lowercases and stems "DeWalt", but TermQuery searches exact case-sensitive token "DeWalt", returning 0 hits!
// Query query = TermQuery.of(t -> t.field("brand").value("DeWalt")).toQuery(); // RETURNS NOTHING!

// FIX: Always query the non-analyzed keyword sub-field ("brand.keyword") for exact matches
package com.example.platform.search.util;

import org.opensearch.client.opensearch._types.query_dsl.Query;
import org.opensearch.client.opensearch._types.query_dsl.TermQuery;

public class ExactMatchQueryBuilder {
    public static Query buildKeywordTerm(String fieldName, String value) {
        // Query the '.keyword' subfield to bypass the Lucene standard analyzer
        return TermQuery.of(t -> t
                .field(fieldName + ".keyword")
                .value(value)
        ).toQuery();
    }
}
```

### Example 5: High-Performance Faceted Aggregation Service
```java
package com.example.platform.search.service;

import org.opensearch.client.OpenSearchClient;
import org.opensearch.client.opensearch._types.aggregations.StringTermsBucket;
import org.opensearch.client.opensearch.core.SearchResponse;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.util.Map;
import java.util.stream.Collectors;

@Service
public class CatalogAggregationService {

    private final OpenSearchClient client;

    public CatalogAggregationService(OpenSearchClient client) {
        this.client = client;
    }

    public Map<String, Long> getBrandFacets() {
        try {
            SearchResponse<Void> response = client.search(s -> s
                    .index("products")
                    .size(0) // size=0 skips returning individual documents for pure aggregation
                    .aggregations("brands_facet", a -> a
                            .terms(t -> t.field("brand.keyword").size(20))
                    ),
                    Void.class
            );

            var termsAggregate = response.aggregations().get("brands_facet").sterms();
            return termsAggregate.buckets().array().stream()
                    .collect(Collectors.toMap(
                            StringTermsBucket::key,
                            StringTermsBucket::docCount
                    ));
        } catch (IOException e) {
            throw new RuntimeException("Aggregation query failed", e);
        }
    }
}
```

---

## 6. Best Practices

1. **Use `filter` Clauses for Structured Predicates**: Place non-scoring conditions (dates, categories, status, tenant IDs) inside `bool.filter` instead of `bool.must`; filters bypass scoring algorithms and are cached in Lucene bitset memory.
2. **Use Multi-Fields (`text` + `.keyword`)**: Map text properties with a `.keyword` sub-field so you can perform both fuzzy full-text search (`name`) and exact matching/aggregations (`name.keyword`).
3. **Set `size = 0` on Pure Aggregation Queries**: When querying metrics or category facets, set `size = 0` so the cluster skips collecting, sorting, and serializing document payloads.
4. **Use Bulk Ingestion API for Write Throughput**: Index documents in batches (e.g., 500-1000 documents per BulkRequest) rather than issuing single `client.index()` calls in loops.
5. **Decouple Database Writes from Search Updates**: Never index synchronously inside database transactions; use Outbox events or Kafka to index asynchronously to prevent distributed transaction failures.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Executing `TermQuery` on analyzed `text` fields | The analyzer lowercase/stemmed tokens during indexing; case-sensitive exact term query finds 0 matches | Query the `.keyword` subfield (e.g., `status.keyword`) for exact matching. |
| Deep pagination using `from + size` ($> 10,000$) | Lucene must collect and sort millions of records across shards, causing high heap consumption | Use `search_after` or point-in-time (PIT) pagination for deep datasets. |
| Synchronous indexing inside relational DB transaction | Slow search cluster response holds database locks open and fails DB write if search is down | Update search index asynchronously via Kafka or `@TransactionalEventListener(AFTER_COMMIT)`. |
| Excessive Shard Count | Thousands of small shards create massive JVM heap overhead per node | Aim for shard sizes between 20GB and 50GB; consolidate small indices. |
| Wildcard queries starting with asterisks (`*search`) | Forces exhaustive scan of every token in the inverted index, halting node CPU | Use N-gram tokenizers or edge-ngram analyzers for prefix/infix search. |
| Leaving mapping dynamic in production | Search engine guesses data types incorrectly (e.g., mapping IP addresses as string) | Define explicit index mappings and templates before ingesting data. |

---

## 8. Performance & Security Notes

- **Lucene Bitset Caching**: Filters inside `bool.filter` generate bitsets representing document matches. Re-running the same filter reuses bitsets directly in memory without CPU evaluation.
- **Deep Pagination Limits**: OpenSearch enforces `index.max_result_window = 10000`. Querying `from: 50000, size: 20` throws an exception. Migrate to `search_after` for large result sets.
- **TLS & Transport Encryption**: Always connect to search clusters over HTTPS with valid CA certificates and TLS enabled.

---

## 9. Comparison / When to Use What

| Feature | OpenSearch / Elasticsearch | PostgreSQL Full-Text (tsvector) | Relational SQL `LIKE '%x%'` |
|---|---|---|---|
| **Architecture** | Distributed Inverted Index | In-database inverted index | Full Table Scan / B-Tree |
| **Search Features** | BM25, Fuzzy, Facets, Synonyms | Basic stemming, ranking | Substring matching only |
| **Throughput** | Millions of queries/sec | Moderate | Catastrophic at scale |
| **Data Synchronization** | Eventual consistency | Transactional (ACID) | Transactional (ACID) |
| **Best For** | Discovery, catalogs, log analytics | Simple search in small apps | Internal administrative lookups |

---

## 10. Interview Questions

<details>
<summary>1. What is an Inverted Index and why does it make full-text search faster than SQL LIKE queries?</summary>
An inverted index is a data structure that tokenizes text into distinct terms and maps each term to a posting list containing document IDs and positions where the term appears. Unlike SQL `LIKE '%term%'` which scans every row sequentially ($O(N)$), an inverted index looks up the exact term in a dictionary hash or B-tree in $O(1)$ to $O(\log K)$ time, immediately retrieving matching document IDs.
</details>

<details>
<summary>2. What is the difference between a `match` query and a `term` query?</summary>
A `match` query passes the input string through the analyzer assigned to that field (lowercasing, stemming, tokenizing) before searching. A `term` query does not analyze the search string; it looks for the exact binary token in the inverted index. `term` queries must be run against `keyword` fields.
</details>

<details>
<summary>3. Why should filter queries be preferred over must queries when relevance scoring is not needed?</summary>
Clauses inside `must` calculate BM25 relevance scores, consuming CPU cycles. Clauses inside `filter` participate in binary yes/no matching without calculating scores. Lucene caches filter results as bitsets in memory, drastically accelerating subsequent queries.
</details>

<details>
<summary>4. How does the BM25 scoring algorithm rank search results?</summary>
BM25 (Best Matching 25) calculates relevance based on:
1. Term Frequency (TF): How often the term appears in the document (with saturation to prevent keyword stuffing).
2. Inverse Document Frequency (IDF): Rare words across the entire index get higher weights than common words.
3. Field-length normalization: Matches in shorter fields (titles) are ranked higher than matches in lengthy paragraphs.
</details>

<details>
<summary>5. How should an application synchronize data between PostgreSQL and OpenSearch?</summary>
The enterprise standard is Change Data Capture (CDC) using Debezium and Kafka. When a row changes in PostgreSQL, the database write-ahead log (WAL) streams the change to a Kafka topic. A search consumer worker reads the topic and writes updates to OpenSearch in bulk. Alternatively, Spring's `@TransactionalEventListener(phase = AFTER_COMMIT)` can publish search update events.
</details>

<details>
<summary>6. Why is deep pagination using `from` and `size` dangerous, and what is the alternative?</summary>
To serve `from: 100000, size: 10`, each shard must collect and sort 100,010 documents and send them to the coordinating node, which merges all shard results and sorts them. This causes high memory consumption and GC spikes. The alternative is `search_after`, which uses the sort key of the last document as a cursor.
</details>

<details>
<summary>7. What is the role of the `.keyword` subfield in text mappings?</summary>
When a string field is defined with multi-fields (`type: text` with `fields: { keyword: { type: keyword } }`), the `text` field is analyzed for fuzzy full-text search, while the `.keyword` field is indexed as an untouched literal string for exact filtering, sorting, and aggregations.
</details>

<details>
<summary>8. How do you implement autocomplete in OpenSearch?</summary>
Autocomplete can be implemented using:
1. Prefix query on keyword fields.
2. Edge N-gram token filters that index word prefixes (e.g., `s`, `sp`, `spr`, `spring`) at index time.
3. Completion suggester using Finite State Transducers (FST) loaded in memory for fast prefix lookups.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Connect OpenSearch client to local cluster</summary>
Write a Spring `@Configuration` configuring an `OpenSearchClient` connecting to `http://localhost:9200`.
</details>

<details>
<summary>Exercise 2 (Easy): Index a single JSON document</summary>
Write a method `void indexArticle(ArticleDoc doc)` using `client.index()`.
</details>

<details>
<summary>Exercise 3 (Medium): Build a multi-match query with boost</summary>
Write a search method that searches a query string across `title` (boosted by 2.0) and `content` fields using `multiMatch`.
</details>

<details>
<summary>Exercise 4 (Medium): Build a BoolQuery with term filters and price range</summary>
Construct a query that searches for "laptop", filtered by `status.keyword = "IN_STOCK"` and `price <= 1000.0`.
</details>

<details>
<summary>Exercise 5 (Hard): Implement faceted terms aggregation</summary>
Write an aggregation service that executes a search query and returns the top 10 categories and their document counts.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test with OpenSearch Testcontainers</summary>
Write a JUnit 5 test using `org.opensearch.testcontainers:opensearch-testcontainers` that starts an OpenSearch cluster, indexes 3 documents, and asserts search hits.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       OpenSearch / Elasticsearch Summary                        |
+---------------------------------------------------------------------------------+
| Client:             org.opensearch.client:opensearch-java                       |
| Index Structure:    Inverted Index (terms -> posting lists of Doc IDs)          |
| Text vs Keyword:    text = analyzed (match query, fuzzy, BM25 scored)           |
|                     keyword = unanalyzed (term query, exact, aggregation)       |
| Bool Query:         must (scored), filter (unscored + cached), should, mustNot  |
| Aggregations:       client.search(s -> s.size(0).aggregations(...))             |
| Deep Paging:        Use search_after rather than from + size                    |
| Synchronization:    Use CDC (Debezium/Kafka) or transactional outbox            |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [OpenSearch Java Client Documentation](https://opensearch.org/docs/latest/clients/java/)
- [Elasticsearch Java Client Documentation](https://www.elastic.co/guide/en/elasticsearch/client/java-api-client/current/index.html)
- [Lucene in Action (Otis Gospodnetic et al.)](https://www.manning.com/books/lucene-in-action-second-edition)
