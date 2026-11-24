# Chat Models and Embeddings in Spring AI

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring AI Version** | 1.0.x / 1.1.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | OpenAI text-embedding-3-small, Ollama nomic-embed-text, pgvector |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Traditional enterprise search relies on exact keyword matching (SQL `LIKE %term%` or full-text Lucene queries). Keyword search fails completely when users search using synonyms, misspellings, conceptual queries, or different phrasing (e.g., searching for "automobile coverage" yields zero matches on documents titled "car insurance"). Furthermore, standard chat models cannot perform semantic similarity comparisons between raw texts on their own.

### The Evolution
**Vector Embeddings** revolutionized natural language processing by transforming raw text into high-dimensional numerical vectors (e.g., arrays of 1,536 floating-point numbers). In this mathematical embedding space, texts with similar conceptual meaning are positioned close to one another, enabling **Cosine Similarity** and semantic vector search.
Spring AI provides the **`EmbeddingModel`** and **`EmbeddingClient`** abstractions, standardizing vector generation across OpenAI, Amazon Titan, Google Vertex AI, and local Ollama models with seamless integration into Spring Boot services.

### Production Relevance
Combining `ChatModel` for conversational intelligence with `EmbeddingModel` for semantic search is the prerequisite foundation for Retrieval-Augmented Generation (RAG), recommendation systems, anomaly detection, semantic deduplication, and enterprise knowledge search.

---

## 2. Core Concepts

### Vector Embedding Space Mental Model

```
+-----------------------------------------------------------------------------------------+
|                              VECTOR EMBEDDING SPACE                                     |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|       "Puppy" [0.12, 0.84, -0.41, ...]                                                  |
|          *                                                                              |
|           \  (High Cosine Similarity ~ 0.94)                                            |
|            * "Dog" [0.14, 0.81, -0.39, ...]                                             |
|                                                                                         |
|                                                                                         |
|                                         * "Vehicle" [-0.72, 0.05, 0.63, ...]             |
|                                        /                                                |
|                                       / (High Cosine Similarity ~ 0.91)                 |
|                                      * "Automobile" [-0.69, 0.08, 0.61, ...]            |
|                                                                                         |
|   Distance between "Dog" and "Automobile": Orthogonal / Low Similarity (~0.12)          |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Definitions
1. **Embedding**: A fixed-length array of floating-point numbers (`float[]` or `List<Double>`) representing the latent semantic meaning of a text string.
2. **Dimension**: The length of the vector array. For example, OpenAI `text-embedding-3-small` produces 1,536 dimensions; `text-embedding-3-large` produces 3,072 dimensions.
3. **Cosine Similarity**: A metric measuring the cosine of the angle between two vectors in multi-dimensional space, ranging from -1.0 (opposite) to 1.0 (identical semantic meaning).
4. **`EmbeddingModel`**: The core Spring AI SPI interface that converts text into numeric embeddings.

---

## 3. How It Works Internally

### Embedding Generation & Vector Calculation Flow

```
[ Input Text: "Spring Boot Microservices" ]
                    |
                    v
[ EmbeddingModel.embed("Spring Boot Microservices") ]
                    |
                    v
[ HTTP Request to Embedding Provider API ]
   POST https://api.openai.com/v1/embeddings
   Body: {"input": "Spring Boot Microservices", "model": "text-embedding-3-small"}
                    |
                    v
[ API Response: 1536 Floats ]
   {"data": [{"embedding": [0.0123, -0.0456, 0.0789, ... 1533 more]}]}
                    |
                    v
[ Spring AI EmbeddingResponse ]
   - Extracts float[] vector
   - Records token usage metadata (Prompt tokens consumed)
                    |
                    v
[ VectorStore / Database Persistence (pgvector) ]
   INSERT INTO vector_store (content, embedding) VALUES ('Spring Boot Microservices', '[0.0123,...]');
```

1. **Text Normalization**: Spring AI passes text strings to the underlying embedding provider via batch or single requests.
2. **Dense Vector Output**: The provider's deep learning neural network processes tokens through transformer layers and returns the final hidden-state vector.
3. **Similarity Search**: Queries are embedded using the identical `EmbeddingModel` and matched against stored vectors using Euclidean distance ($L_2$), inner product, or cosine similarity ($1 - \text{cosine\_distance}$).

---

## 4. Syntax & Usage

### 1. Configuration in `application.yaml`

```yaml
spring:
  ai:
    openai:
      api-key: ${OPENAI_API_KEY}
      embedding:
        options:
          model: text-embedding-3-small
          dimensions: 1536
```

### 2. Injecting and Using `EmbeddingModel`

```java
package com.example.ai.service;

import org.springframework.ai.embedding.EmbeddingModel;
import org.springframework.ai.embedding.EmbeddingResponse;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class EmbeddingService {

    private final EmbeddingModel embeddingModel;

    public EmbeddingService(EmbeddingModel embeddingModel) {
        this.embeddingModel = embeddingModel;
    }

    public float[] generateVector(String text) {
        // Generates dense 1536-dimensional vector
        return embeddingModel.embed(text);
    }

    public List<float[]> batchGenerate(List<String> texts) {
        // Batch generation for high-throughput document ingestion
        EmbeddingResponse response = embeddingModel.embedForResponse(texts);
        return response.getResults().stream()
                .map(result -> result.getOutput())
                .map(this::toFloatArray)
                .toList();
    }

    private float[] toFloatArray(List<Double> doubleList) {
        float[] floats = new float[doubleList.size()];
        for (int i = 0; i < doubleList.size(); i++) {
            floats[i] = doubleList.get(i).floatValue();
        }
        return floats;
    }
}
```

---

## 5. Practical Examples

### Example 1: In-Memory Cosine Similarity Calculator

Calculating semantic proximity between customer inquiries without external vector databases:

```java
package com.example.ai.similarity;

import org.springframework.ai.embedding.EmbeddingModel;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class SemanticSimilarityComparator {

    private final EmbeddingModel embeddingModel;

    public SemanticSimilarityComparator(EmbeddingModel embeddingModel) {
        this.embeddingModel = embeddingModel;
    }

    public double calculateSimilarity(String text1, String text2) {
        List<Double> vectorA = embeddingModel.embed(text1);
        List<Double> vectorB = embeddingModel.embed(text2);

        return cosineSimilarity(vectorA, vectorB);
    }

    public static double cosineSimilarity(List<Double> vecA, List<Double> vecB) {
        if (vecA.size() != vecB.size()) {
            throw new IllegalArgumentException("Vector dimensions must match: " + vecA.size() + " vs " + vecB.size());
        }

        double dotProduct = 0.0;
        double normA = 0.0;
        double normB = 0.0;

        for (int i = 0; i < vecA.size(); i++) {
            double a = vecA.get(i);
            double b = vecB.get(i);
            dotProduct += a * b;
            normA += a * a;
            normB += b * b;
        }

        if (normA == 0.0 || normB == 0.0) return 0.0;
        return dotProduct / (Math.sqrt(normA) * Math.sqrt(normB));
    }
}
```

### Example 2: Semantic Intent Classifier using Embeddings

Classifying customer support tickets by finding the nearest centroid among pre-calculated intent vectors:

```java
package com.example.ai.classifier;

import com.example.ai.similarity.SemanticSimilarityComparator;
import org.springframework.ai.embedding.EmbeddingModel;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;

public enum SupportCategory {
    BILLING,
    ACCOUNT_ACCESS,
    HARDWARE_DEFECT,
    SHIPPING_DELAY
}

@Service
public class SemanticTicketClassifier {

    private final EmbeddingModel embeddingModel;
    private final Map<SupportCategory, List<Double>> categoryCentroids;

    public SemanticTicketClassifier(EmbeddingModel embeddingModel) {
        this.embeddingModel = embeddingModel;
        // Pre-compute intent vectors for standard categories
        this.categoryCentroids = Map.of(
            SupportCategory.BILLING, embeddingModel.embed("Invoices, credit card charges, refunds, payment failures"),
            SupportCategory.ACCOUNT_ACCESS, embeddingModel.embed("Password reset, login locked out, 2FA auth issues"),
            SupportCategory.HARDWARE_DEFECT, embeddingModel.embed("Broken screen, damaged physical device, won't turn on"),
            SupportCategory.SHIPPING_DELAY, embeddingModel.embed("Where is my package, tracking number, courier lost shipment")
        );
    }

    public SupportCategory classify(String incomingTicketText) {
        List<Double> ticketVector = embeddingModel.embed(incomingTicketText);

        SupportCategory bestCategory = null;
        double highestSimilarity = -1.0;

        for (var entry : categoryCentroids.entrySet()) {
            double similarity = SemanticSimilarityComparator.cosineSimilarity(ticketVector, entry.getValue());
            if (similarity > highestSimilarity) {
                highestSimilarity = similarity;
                bestCategory = entry.getKey();
            }
        }

        return bestCategory;
    }
}
```

### Example 3: Prompt Engineering with Prompt Templates and Parameter Substitution

```java
package com.example.ai.prompt;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.chat.prompt.PromptTemplate;
import org.springframework.stereotype.Service;

import java.util.Map;

@Service
public class CodeReviewAssistantService {

    private final ChatClient chatClient;

    public CodeReviewAssistantService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public String generateReview(String language, String pullRequestDiff) {
        String templateString = """
            You are a Principal Software Engineer.
            Review the following {language} code changes.
            Identify performance bottlenecks, security vulnerabilities, and adherence to clean code.
            
            DIFF TO REVIEW:
            ```
            {diff}
            ```
            """;

        PromptTemplate promptTemplate = new PromptTemplate(templateString);
        String renderedPrompt = promptTemplate.render(Map.of(
                "language", language,
                "diff", pullRequestDiff
        ));

        return chatClient.prompt()
                .user(renderedPrompt)
                .call()
                .content();
    }
}
```

### Example 4: Anti-Pattern (Mismatching Embedding Models) vs Consistent Dimension Store

#### The Anti-Pattern: Ingesting with Model A and Querying with Model B
```java
// CATASTROPHIC BUG:
// Ingestion was run with OpenAI text-embedding-3-small (1536 dimensions)
// Search query service accidentally configured with Ollama nomic-embed-text (768 dimensions)!

@Service
public class BrokenSearchService {
    @Autowired private EmbeddingModel ollamaEmbeddingModel; // 768 dimensions!
    @Autowired private VectorStore vectorStore; // Storing 1536-dimensional vectors!

    public List<Document> search(String query) {
        // PostgreSQL pgvector throws:
        // ERROR: different vector dimensions 768 and 1536
        // Query crashes with DataIntegrityViolationException!
        return vectorStore.similaritySearch(query);
    }
}
```

#### Production Fix: Explicit Dimension Validation & Model Locking
```java
// GOOD: Ensure identical model and dimension configurations across the entire pipeline
@Configuration
public class SafeVectorStoreConfig {

    @Value("${spring.ai.openai.embedding.options.dimensions:1536}")
    private int expectedDimensions;

    @Bean
    public VectorStore vectorStore(JdbcTemplate jdbcTemplate, EmbeddingModel embeddingModel) {
        // Explicitly enforce that the database schema and embedding model dimensions align
        return PgVectorStore.builder(jdbcTemplate, embeddingModel)
                .dimensions(expectedDimensions)
                .distanceType(PgVectorStore.PgDistanceType.COSINE_DISTANCE)
                .build();
    }
}
```

### Example 5: High-Speed Batch Ingestion with Reactive Pipeline

```java
package com.example.ai.batch;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.ai.embedding.EmbeddingModel;
import org.springframework.stereotype.Service;
import reactor.core.publisher.Flux;

import java.time.Duration;
import java.util.List;

@Service
public class HighThroughputEmbeddingIngestionService {

    private static final Logger log = LoggerFactory.getLogger(HighThroughputEmbeddingIngestionService.class);
    private final EmbeddingModel embeddingModel;

    public HighThroughputEmbeddingIngestionService(EmbeddingModel embeddingModel) {
        this.embeddingModel = embeddingModel;
    }

    public void ingestAllDocuments(List<String> rawDocuments) {
        // Chunk into batches of 100 texts to maximize API throughput and avoid rate limits
        Flux.fromIterable(rawDocuments)
                .buffer(100)
                .delayElements(Duration.ofMillis(200)) // Rate-limit compliance
                .doOnNext(batch -> {
                    log.info("Generating embeddings for batch of {} documents...", batch.size());
                    embeddingModel.embedForResponse(batch);
                })
                .blockLast();
    }
}
```

---

## 6. Best Practices

1. **Lock Embedding Models Permanently Once Vectors Are Stored**: If you change your embedding model from `text-embedding-ada-002` to `text-embedding-3-small`, you **MUST** re-embed and re-index all historical documents in your database. Vector spaces from different models are mathematically incompatible.
2. **Use Batch Embedding Ingestion**: Never call `embed(text)` in a loop of 1,000 items. Send batches of 50–100 items using `embedForResponse(List<String>)` to minimize HTTP connection overhead.
3. **Normalize Texts Before Embedding**: Strip redundant whitespace, markdown artifacts, and binary tokens before generating embeddings to prevent wasting token budgets on noise.
4. **Choose Dimensions Wisely**: Newer models like `text-embedding-3-small` allow customizing output dimensions (e.g., reducing from 1536 to 512). This cuts database storage and index memory by 66% with negligible loss in retrieval accuracy.
5. **Use Cosine Distance for Text Search**: For normalized text vectors, Cosine Distance is the industry standard for semantic search.
6. **Cache Frequent Query Embeddings in Redis**: Cache vectors of frequent search queries to save OpenAI API costs and reduce search latency from 200ms to 2ms.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Changing Embedding Models Without Re-indexing** | Searching with Model B against vectors created by Model A produces random garbage or dimension mismatch crashes. | Re-embed all database records when upgrading embedding models. |
| **Embedding Texts Exceeding Context Window** | Passing a 50-page PDF to `embed()` triggers an API token limit exception (e.g., >8192 tokens). | Chunk documents into smaller passages (500 tokens) using `TokenTextSplitter`. |
| **Comparing Vectors with Different Dimensions** | Calculating dot product on arrays of length 768 and 1536 throws `IllegalArgumentException` or SQL error. | Enforce dimension invariants at startup. |
| **Embedding Ephemeral User Identifiers** | Embedding strings like `"User 98124 bought shoe"` biases the vector toward user IDs rather than semantic meaning. | Strip internal database primary keys and ephemeral IDs from text before embedding. |
| **Single-Item HTTP Loops** | Ingesting 10,000 documents via 10,000 sequential HTTP calls takes 45 minutes and hits rate limits. | Use batch embedding endpoints (`embedForResponse(List)`). |
| **Ignoring Embedding API Costs** | Continuous re-embedding of static documents wastes thousands of dollars. | Cache embeddings by SHA-256 hash of the content. |

---

## 8. Performance & Security Notes

### Performance
- **Index Types in PostgreSQL (pgvector)**:
  - **HNSW (Hierarchical Navigable Small World)**: Provides the fastest query retrieval speed ($<5\text{ms}$) with high recall, at the expense of longer index build times and higher RAM usage.
  - **IVFFlat**: Inverted file with flat compression. Faster to build, smaller memory footprint, but requires periodic re-indexing as data grows.
- **SIMD Hardware Acceleration**: Modern JVMs (Java 21+) leverage Vector API / AVX-512 SIMD instructions for accelerating in-memory dot product and cosine similarity calculations.

### Security
- **Vector Inversion Attacks**: While embeddings look like random numbers, specialized neural networks can reconstruct significant portions of the original text from dense vector embeddings. Protect vector database tables with the same encryption and access controls as plaintext data.

---

## 9. Comparison / When to Use What

| Technique | Query Type | Precision / Exact Match | Synonyms & Conceptual Match | Computational Cost |
|---|---|---|---|---|
| **Keyword Search (`LIKE` / B-Tree)** | Exact substring match | High for exact strings | Zero | Very Low ($O(\log N)$) |
| **Full-Text Search (Lucene / Gin)**| Stemmed words, BM25 | High for keywords | Moderate (synonym tables) | Low-to-Medium |
| **Dense Vector Embeddings** | Natural language concepts | Moderate for exact codes | Maximum | High (GPU / API call) |
| **Hybrid Search (BM25 + Vector)** | Mixed keywords & concepts | Maximum across all types | Maximum | Medium-High |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the mathematical definition of Cosine Similarity and why is it preferred for text embeddings?</strong></summary>

Cosine similarity measures the cosine of the angle between two non-zero vectors in multi-dimensional space:
$$\text{Cosine Similarity}(A, B) = \frac{A \cdot B}{\|A\| \|B\|} = \frac{\sum_{i=1}^n A_i B_i}{\sqrt{\sum_{i=1}^n A_i^2} \sqrt{\sum_{i=1}^n B_i^2}}$$
**Why preferred**:
- Cosine similarity evaluates the **direction** of vectors rather than their magnitude (length).
- In text processing, a longer document discussing machine learning and a short tweet discussing machine learning point in the same direction in semantic space. Cosine similarity recognizes them as semantically identical, whereas Euclidean distance ($L_2$) would penalize the longer text due to its larger magnitude.
</details>

<details>
<summary><strong>2. Why can't you mix vectors generated by two different embedding models?</strong></summary>

Every embedding model is trained on distinct neural network architectures, tokenizers, vocabulary sizes, and latent dimensional spaces.
- `text-embedding-3-small` maps concepts to a 1,536-dimensional manifold according to OpenAI's internal representations.
- `nomic-embed-text` maps concepts to a 768-dimensional manifold according to Nomic's representations.
The vector coordinates are relative only to the specific model's training space. Calculating cosine distance between vectors from different models is mathematically meaningless and produces random noise.
</details>

<details>
<summary><strong>3. What is the difference between Euclidean Distance ($L_2$) and Inner Product (Dot Product) in pgvector?</strong></summary>

- **Euclidean Distance ($<->$)**: Measures the straight-line physical distance between two points in vector space. Sensitive to vector magnitude.
- **Negative Inner Product ($<\#>$)**: Calculates the dot product between vectors. If vectors are unit-normalized ($\|A\| = 1$), the inner product is mathematically identical to Cosine Similarity, but executes significantly faster because the square root and division steps are skipped.
- **Cosine Distance ($<=>$)**: $1 - \text{Cosine Similarity}$.
</details>

<details>
<summary><strong>4. How does Spring AI support Prompt Templating and variable substitution?</strong></summary>

Spring AI provides `PromptTemplate`:
- Allows writing prompts with named placeholders: `"Tell me a joke about {topic} in the style of {style}"`.
- Renders the prompt safely by passing a `Map<String, Object>` of variables.
- Can load prompt templates from Spring `Resource` abstractions (`classpath:prompts/review-prompt.st`), keeping large system prompts externalized from Java source code.
</details>

<details>
<summary><strong>5. How can you implement Semantic Deduplication using Spring AI embeddings?</strong></summary>

To prevent storing duplicate support tickets or product reviews:
1. When a new item arrives, generate its vector via `embeddingModel.embed(text)`.
2. Query the vector store for the nearest neighbor with a cosine similarity threshold (e.g., $\ge 0.95$).
3. If a record matches with similarity $\ge 0.95$, treat the incoming item as a duplicate and link it to the existing record instead of creating a new row.
</details>

<details>
<summary><strong>6. What is Matryoshka Representation Learning (MRL) and how does it affect modern embeddings?</strong></summary>

Introduced in modern embedding models (such as OpenAI `text-embedding-3`), MRL trains embeddings so that the most important semantic information is compressed into the first $K$ dimensions of the vector.
This allows developers to truncate a 1,536-dimensional vector down to 512 or 256 dimensions simply by slicing the array. This reduces vector database RAM usage and disk storage by up to 80% while retaining over 97% of retrieval accuracy.
</details>

<details>
<summary><strong>7. How do you handle document chunking before generating embeddings?</strong></summary>

Because embedding models have strict context window limits (e.g., 8,192 tokens) and smaller passages provide higher retrieval precision:
- Documents are split into chunks of 300–500 tokens using Spring AI's `TokenTextSplitter`.
- An **overlap** (e.g., 50 tokens) is maintained between adjacent chunks to prevent cutting a sentence or concept in half at chunk boundaries.
</details>

<details>
<summary><strong>8. Why should you hash document contents before calling the EmbeddingModel?</strong></summary>

Generating embeddings via cloud APIs incurs monetary costs and network latency.
By hashing document content (`SHA-256(text)`):
- Check if the hash already exists in a local cache or database table.
- If it exists, retrieve the pre-computed vector.
- Only call the external embedding API when document content has genuinely changed.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Generate an Embedding Vector for a User Input</strong></summary>

**Objective**: Write a service method that converts an incoming search string into a float array.

**Solution**:
```java
@Service
public class SimpleEmbeddingService {
    private final EmbeddingModel embeddingModel;

    public SimpleEmbeddingService(EmbeddingModel embeddingModel) {
        this.embeddingModel = embeddingModel;
    }

    public float[] toVector(String query) {
        List<Double> doubleList = embeddingModel.embed(query);
        float[] vector = new float[doubleList.size()];
        for (int i = 0; i < doubleList.size(); i++) {
            vector[i] = doubleList.get(i).floatValue();
        }
        return vector;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Load Prompt Template from External Classpath File</strong></summary>

**Objective**: Use Spring AI's `PromptTemplate` to load a prompt template from `classpath:prompts/system-assistant.st`.

**Solution**:
```java
@Service
public class ExternalizedPromptService {

    @Value("classpath:prompts/system-assistant.st")
    private Resource promptResource;

    public String createPrompt(String userName, String task) {
        PromptTemplate template = new PromptTemplate(promptResource);
        return template.render(Map.of("user", userName, "task", task));
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement SHA-256 Content-Addressable Embedding Cache</strong></summary>

**Objective**: Cache embeddings in Redis using the SHA-256 hash of the input text as the cache key.

**Solution**:
```java
@Service
public class CachedEmbeddingService {

    private final EmbeddingModel embeddingModel;
    private final StringRedisTemplate redis;
    private final ObjectMapper mapper;

    public CachedEmbeddingService(EmbeddingModel embeddingModel, StringRedisTemplate redis, ObjectMapper mapper) {
        this.embeddingModel = embeddingModel;
        this.redis = redis;
        this.mapper = mapper;
    }

    public List<Double> getCachedEmbedding(String text) {
        String hash = DigestUtils.sha256Hex(text);
        String cachedJson = redis.opsForValue().get("emb:" + hash);

        if (cachedJson != null) {
            try {
                return mapper.readValue(cachedJson, new TypeReference<List<Double>>() {});
            } catch (Exception ignored) {}
        }

        List<Double> embedding = embeddingModel.embed(text);
        try {
            redis.opsForValue().set("emb:" + hash, mapper.writeValueAsString(embedding), Duration.ofDays(30));
        } catch (Exception ignored) {}

        return embedding;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Document Chunking with `TokenTextSplitter`</strong></summary>

**Objective**: Split a large text into chunks of 400 tokens with 50 tokens overlap using Spring AI.

**Solution**:
```java
@Service
public class DocumentPreparationService {

    public List<Document> chunkText(String rawText, Map<String, Object> metadata) {
        TokenTextSplitter splitter = new TokenTextSplitter(400, 50, 5, 10000, true);
        Document document = new Document(rawText, metadata);
        return splitter.apply(List.of(document));
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Batch Vector Ingestion into PostgreSQL pgvector using `JdbcClient`</strong></summary>

**Objective**: Write a high-performance batch insertion routine using `JdbcClient` and PostgreSQL's vector syntax.

**Solution**:
```java
@Repository
public class PgVectorCustomRepository {

    private final JdbcClient jdbcClient;

    public PgVectorCustomRepository(JdbcClient jdbcClient) {
        this.jdbcClient = jdbcClient;
    }

    public void insertVector(String content, List<Double> vector) {
        // Convert List<Double> to PostgreSQL vector literal: '[0.1, 0.2, 0.3]'
        String vectorLiteral = vector.toString();

        jdbcClient.sql("INSERT INTO document_vectors (content, embedding) VALUES (:content, :vector::vector)")
                .param("content", content)
                .param("vector", vectorLiteral)
                .update();
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test Comparing Cosine Similarity of Related vs Unrelated Phrases</strong></summary>

**Objective**: Write an integration test asserting that "cat" and "kitten" have a higher cosine similarity than "cat" and "submarine".

**Solution**:
```java
@SpringBootTest
class SemanticSimilarityIntegrationTest {

    @Autowired private SemanticSimilarityComparator comparator;

    @Test
    void assertSemanticProximity() {
        double catKittenSim = comparator.calculateSimilarity("cat", "kitten");
        double catSubmarineSim = comparator.calculateSimilarity("cat", "nuclear submarine");

        assertTrue(catKittenSim > 0.75, "Expected high similarity between cat and kitten");
        assertTrue(catSubmarineSim < 0.35, "Expected low similarity between cat and submarine");
        assertTrue(catKittenSim > catSubmarineSim, "Cat-kitten must be more similar than cat-submarine");
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| CHAT MODELS AND EMBEDDINGS QUICK REFERENCE                                                         |
+----------------------------------------------------------------------------------------------------+
| Concept             | Spring AI Class / Tool                    | Key Rule                         |
+---------------------+-------------------------------------------+----------------------------------+
| Embedding Model     | org.springframework.ai.embedding.         | Converts text to float[] vector; |
|                     | EmbeddingModel                            | dimension-specific               |
+---------------------+-------------------------------------------+----------------------------------+
| Prompt Template     | org.springframework.ai.chat.prompt.       | Variable substitution for prompts|
|                     | PromptTemplate                            | from string or Resource files    |
+---------------------+-------------------------------------------+----------------------------------+
| Document Splitter   | TokenTextSplitter                         | Chunks large text into passages  |
|                     |                                           | with token overlap               |
+---------------------+-------------------------------------------+----------------------------------+
| Model Locking       | Never mix vectors from different models   | Re-index DB if model changes!    |
+---------------------+-------------------------------------------+----------------------------------+
| Batching Rule       | Use embedForResponse(List<String>)        | Drastically reduces HTTP latency |
+---------------------+-------------------------------------------+----------------------------------+
| Distance Metric     | Cosine Distance (1 - Cosine Similarity)   | Standard metric for text vectors |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring AI Embeddings Documentation](https://docs.spring.io/spring-ai/reference/api/embeddings.html)
- [OpenAI Embeddings Guide and Best Practices](https://platform.openai.com/docs/guides/embeddings)
- [PostgreSQL pgvector Extension GitHub](https://github.com/pgvector/pgvector)
- [Vector Search Explained (Pinecone Learning Center)](https://www.pinecone.io/learn/vector-search-basics/)
- [Matryoshka Representation Learning Research Paper](https://arxiv.org/abs/2205.13147)
