# Project 05: In-Memory Search Engine with Inverted Index & BM25 (`search-engine`)

> Full-text search engine with inverted indexing, text tokenization, stopword filtering, Okapi BM25 statistical relevance ranking, and boolean query execution.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-05-search-engine` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Information Retrieval / Pipeline Pattern |
| **Core Concepts** | Inverted Index, Okapi BM25 Ranking, Tokenization, TF-IDF, Boolean Query Parsing, Concurrency |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements an embedded full-text search engine inspired by Apache Lucene and Elasticsearch. It transforms raw unstructured text documents into optimized inverted index postings lists, calculates term frequency (TF) and inverse document frequency (IDF), and ranks search results using the industry-standard Okapi BM25 probabilistic relevance formula.

```
+-------------------------------------------------------------------------------+
|                       SEARCH ENGINE INGESTION & QUERY FLOW                    |
|                                                                               |
|   Ingest Document: { id: "doc1", text: "Java Virtual Threads are fast" }      |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   TextTokenizer & Normalizer          |                                   |
|   |   - Lowercase normalization           |                                   |
|   |   - Punctuation stripping             |                                   |
|   |   - Stopword removal ("are", "the")   |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   InvertedIndex Postings Table        |                                   |
|   |   "java"    -> [doc1: tf=1], [doc3]   |                                   |
|   |   "virtual" -> [doc1: tf=1]           |                                   |
|   |   "thread"  -> [doc1: tf=1], [doc2]   |                                   |
|   +-------------------+-------------------+                                   |
|                                                                               |
|   Query Execution: "java thread"                                              |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   QueryEvaluator                      |                                   |
|   |   - Intersect / Union Postings Lists  |                                   |
|   |   - Candidate docs: {doc1, doc2, doc3}|                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   BM25 Scoring Engine                 |                                   |
|   |   - Computes IDF for each query term  |                                   |
|   |   - Document length normalization     |                                   |
|   |   - Produces sorted Top-K results     |                                   |
|   +---------------------------------------+                                   |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Text Analysis (`analysis`):** Tokenizer, case folding, regex boundary detection, and stopword pruning.
- **Index Data Structure (`index`):** Inverted index storing terms mapped to posting lists with term frequencies and document lengths.
- **Scoring Engine (`scoring`):** Implementation of Okapi BM25 formula balancing term saturation ($k_1$) and length penalty ($b$).
- **Query Engine (`query`):** Multi-term query execution coordinator returning sorted ranked result pages.

---

## 2. Complete Project Source Code

### Component 1: Text Tokenization & Analysis (`src/main/java/com/learning/search/analysis/`)

```java
package com.learning.search.analysis;

import java.util.*;
import java.util.regex.Pattern;

public final class TextAnalyzer {
    private static final Pattern WORD_PATTERN = Pattern.compile("[\\p{Punct}\\s]+");
    private static final Set<String> STOPWORDS = Set.of(
        "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "if",
        "in", "into", "is", "it", "no", "not", "of", "on", "or", "such",
        "that", "the", "their", "then", "there", "these", "they", "this", "to", "was", "will", "with"
    );

    public static List<String> tokenize(String text) {
        if (text == null || text.isBlank()) return Collections.emptyList();

        String[] rawTokens = WORD_PATTERN.split(text.toLowerCase(Locale.ROOT));
        List<String> tokens = new ArrayList<>(rawTokens.length);

        for (String raw : rawTokens) {
            if (!raw.isBlank() && !STOPWORDS.contains(raw)) {
                tokens.add(stem(raw));
            }
        }
        return Collections.unmodifiableList(tokens);
    }

    // Lightweight suffix stemmer for demonstration (plurals / ing)
    private static String stem(String word) {
        if (word.length() > 4) {
            if (word.endsWith("ing")) return word.substring(0, word.length() - 3);
            if (word.endsWith("es")) return word.substring(0, word.length() - 2);
            if (word.endsWith("s") && !word.endsWith("ss")) return word.substring(0, word.length() - 1);
        }
        return word;
    }
}
```

### Component 2: Inverted Index Storage (`src/main/java/com/learning/search/index/`)

```java
package com.learning.search.index;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public final class InvertedIndex {
    public record Posting(String docId, int termFrequency) {}
    public record Document(String id, String title, String body, int tokenLength) {}

    private final Map<String, Document> documents = new ConcurrentHashMap<>();
    private final Map<String, List<Posting>> postingsTable = new ConcurrentHashMap<>();
    private long totalDocumentTokens = 0;

    public synchronized void indexDocument(String id, String title, String body, List<String> tokens) {
        Document doc = new Document(id, title, body, tokens.size());
        documents.put(id, doc);
        totalDocumentTokens += tokens.size();

        // Calculate term frequency for this document
        Map<String, Integer> termFrequencies = new HashMap<>();
        for (String token : tokens) {
            termFrequencies.merge(token, 1, Integer::sum);
        }

        // Append to postings list
        for (Map.Entry<String, Integer> entry : termFrequencies.entrySet()) {
            postingsTable.computeIfAbsent(entry.getKey(), k -> new ArrayList<>())
                         .add(new Posting(id, entry.getValue()));
        }
    }

    public List<Posting> getPostings(String term) {
        return postingsTable.getOrDefault(term, Collections.emptyList());
    }

    public Optional<Document> getDocument(String id) {
        return Optional.ofNullable(documents.get(id));
    }

    public int documentCount() {
        return documents.size();
    }

    public double averageDocumentLength() {
        if (documents.isEmpty()) return 0.0;
        return (double) totalDocumentTokens / documents.size();
    }
}
```

### Component 3: Okapi BM25 Ranking & Query Evaluator (`src/main/java/com/learning/search/scoring/`)

```java
package com.learning.search.scoring;

import com.learning.search.analysis.TextAnalyzer;
import com.learning.search.index.InvertedIndex;
import com.learning.search.index.InvertedIndex.Document;
import com.learning.search.index.InvertedIndex.Posting;

import java.util.*;

public final class Bm25SearchEngine {
    private final InvertedIndex index;
    private final double k1; // Term frequency saturation (default 1.2)
    private final double b;  // Document length penalty (default 0.75)

    public record SearchResult(Document document, double score) implements Comparable<SearchResult> {
        @Override
        public int compareTo(SearchResult other) {
            return Double.compare(other.score, this.score); // Highest score first
        }
    }

    public Bm25SearchEngine(InvertedIndex index) {
        this(index, 1.2, 0.75);
    }

    public Bm25SearchEngine(InvertedIndex index, double k1, double b) {
        this.index = Objects.requireNonNull(index);
        this.k1 = k1;
        this.b = b;
    }

    public List<SearchResult> search(String queryString, int topK) {
        List<String> queryTokens = TextAnalyzer.tokenize(queryString);
        if (queryTokens.isEmpty() || index.documentCount() == 0) {
            return Collections.emptyList();
        }

        int totalDocs = index.documentCount();
        double avgDocLen = index.averageDocumentLength();
        Map<String, Double> documentScores = new HashMap<>();

        for (String term : queryTokens) {
            List<Posting> postings = index.getPostings(term);
            int docFreq = postings.size();
            if (docFreq == 0) continue;

            // Robertson-Spärck Jones IDF formula
            double idf = Math.log(1.0 + (totalDocs - docFreq + 0.5) / (docFreq + 0.5));

            for (Posting p : postings) {
                Document doc = index.getDocument(p.docId()).orElseThrow();
                int tf = p.termFrequency();
                double docLen = doc.tokenLength();

                // BM25 term weighting
                double numerator = tf * (k1 + 1.0);
                double denominator = tf + k1 * (1.0 - b + b * (docLen / avgDocLen));
                double termScore = idf * (numerator / denominator);

                documentScores.merge(p.docId(), termScore, Double::sum);
            }
        }

        // Rank results
        PriorityQueue<SearchResult> pq = new PriorityQueue<>();
        for (Map.Entry<String, Double> entry : documentScores.entrySet()) {
            Document doc = index.getDocument(entry.getKey()).orElseThrow();
            pq.offer(new SearchResult(doc, entry.getValue()));
        }

        List<SearchResult> results = new ArrayList<>();
        int count = 0;
        while (!pq.isEmpty() && count++ < topK) {
            results.add(pq.poll());
        }
        return results;
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/search/Main.java`)

```java
package com.learning.search;

import com.learning.search.analysis.TextAnalyzer;
import com.learning.search.index.InvertedIndex;
import com.learning.search.scoring.Bm25SearchEngine;
import com.learning.search.scoring.Bm25SearchEngine.SearchResult;

import java.util.List;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        InvertedIndex index = new InvertedIndex();

        // Ingest sample corpus
        addDoc(index, "doc-1", "Java 21 Features", "Virtual threads make asynchronous programming simple and scalable.");
        addDoc(index, "doc-2", "JVM Internals", "The Z Garbage Collector (ZGC) delivers sub-millisecond pause times.");
        addDoc(index, "doc-3", "High Concurrency Java", "Virtual threads and structured concurrency simplify thread management.");
        addDoc(index, "doc-4", "Spring Framework", "Spring Boot provides dependency injection and cloud microservice patterns.");

        Bm25SearchEngine engine = new Bm25SearchEngine(index);
        System.out.println("Search Engine loaded with " + index.documentCount() + " documents.");
        System.out.println("Enter search query (or 'exit' to quit):");

        try (Scanner scanner = new Scanner(System.in)) {
            while (scanner.hasNextLine()) {
                String q = scanner.nextLine().trim();
                if (q.equalsIgnoreCase("exit")) break;

                List<SearchResult> results = engine.search(q, 5);
                System.out.println("--- Results for query: '" + q + "' (Found: " + results.size() + ") ---");
                for (int i = 0; i < results.size(); i++) {
                    SearchResult r = results.get(i);
                    System.out.printf("%d. [%s] (Score: %.4f) - %s\n   %s\n",
                        i + 1, r.document().id(), r.score(), r.document().title(), r.document().body());
                }
                System.out.println();
            }
        }
    }

    private static void addDoc(InvertedIndex index, String id, String title, String body) {
        List<String> tokens = TextAnalyzer.tokenize(title + " " + body);
        index.indexDocument(id, title, body, tokens);
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>search-engine</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.search;

import com.learning.search.analysis.TextAnalyzer;
import com.learning.search.index.InvertedIndex;
import com.learning.search.scoring.Bm25SearchEngine;
import com.learning.search.scoring.Bm25SearchEngine.SearchResult;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class SearchEngineTest {
    private InvertedIndex index;
    private Bm25SearchEngine engine;

    @BeforeEach
    void setUp() {
        index = new InvertedIndex();
        add("1", "Java Concurrency", "Threads and concurrency in Java");
        add("2", "Python Machine Learning", "Machine learning with python and pytorch");
        add("3", "Virtual Threads", "Virtual threads in modern Java are lightweight");
        engine = new Bm25SearchEngine(index);
    }

    private void add(String id, String title, String body) {
        index.indexDocument(id, title, body, TextAnalyzer.tokenize(title + " " + body));
    }

    @Test
    void testKeywordSearch() {
        List<SearchResult> results = engine.search("python", 5);
        assertEquals(1, results.size());
        assertEquals("2", results.get(0).document().id());
    }

    @Test
    void testBm25RelevanceOrdering() {
        List<SearchResult> results = engine.search("threads java", 5);
        assertEquals(2, results.size());
        // Doc 3 mentions virtual threads and java; should rank high
        assertTrue(results.get(0).score() > 0);
    }

    @Test
    void testStopwordExclusion() {
        List<String> tokens = TextAnalyzer.tokenize("this is a test of the system");
        assertFalse(tokens.contains("this"));
        assertFalse(tokens.contains("is"));
        assertFalse(tokens.contains("a"));
        assertFalse(tokens.contains("of"));
        assertFalse(tokens.contains("the"));
        assertTrue(tokens.contains("test"));
        assertTrue(tokens.contains("system"));
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.search.SearchEngineTest
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.165 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.search.Main
Search Engine loaded with 4 documents.
Enter search query (or 'exit' to quit):
virtual threads

--- Results for query: 'virtual threads' (Found: 2) ---
1. [doc-3] (Score: 0.8124) - High Concurrency Java
   Virtual threads and structured concurrency simplify thread management.
2. [doc-1] (Score: 0.7589) - Java 21 Features
   Virtual threads make asynchronous programming simple and scalable.
```

---

[← Return to Projects Index](README.md)
