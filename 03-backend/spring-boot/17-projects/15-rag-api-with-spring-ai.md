# Project 15: Enterprise RAG Knowledge Search API with Spring AI & PgVector

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring AI Version** | 1.0.0-M5 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring AI OpenAI Starter, Spring AI PgVector Store Starter, Spring Boot Starter Web, Testcontainers |
| **Architecture Style** | Retrieval-Augmented Generation (RAG) Architecture with Vector Embeddings & Contextual LLM Question Answering |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements an enterprise Retrieval-Augmented Generation (RAG) service. During ingestion, enterprise documents (PDF/Markdown/Text) are read via `TextReader`, chunked into overlapping token windows using `TokenTextSplitter`, embedded using OpenAI's `text-embedding-3-small`, and stored in PostgreSQL with the `pgvector` extension. During query evaluation, the user's prompt is vectorized, top-$k$ nearest document chunks are retrieved using cosine similarity, and injected into a contextual prompt template executed by `ChatClient`.

```
                        [ Ingestion Pipeline ]
  ┌─────────────────┐      ┌────────────────────┐      ┌──────────────────┐
  │ Technical Docs  │ ───> │ TokenTextSplitter  │ ───> │ OpenAI Embedding │
  │ (Markdown/PDF)  │      │ (Chunk: 500 tokens)│      │ Model            │
  └─────────────────┘      └────────────────────┘      └────────┬─────────┘
                                                                │ Vector
                                                                ▼
                                                       ┌──────────────────┐
                                                       │ PostgreSQL       │
                                                       │ + pgvector Store │
                                                       └────────┬─────────┘
                        [ Retrieval & QA Flow ]                 │
  ┌─────────────────┐                                           │ Cosine
  │ User Question   │ ────────────────────────────────────────┐ │ Similarity
  └────────┬────────┘                                         │ │ Search
           │                                                  ▼ ▼
           │                                        ┌────────────────────┐
           │                                        │ Top-K Chunks       │
           │                                        └─────────┬──────────┘
           ▼                                                  ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │                       Spring AI ChatClient                           │
  │ System: "Answer strictly based on the following verified context:"   │
  │ Context: {retrieved_chunks}                                          │
  │ Question: {user_prompt}                                              │
  └──────────────────────────────────┬───────────────────────────────────┘
                                     │
                                     ▼
                       [ Synthesized RAG Response ]
```

### 1.2 Directory Layout

```
15-rag-api-with-spring-ai/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/rag/
│   │   │   ├── RagApiApplication.java
│   │   │   ├── config/
│   │   │   │   └── RagConfiguration.java
│   │   │   ├── service/
│   │   │   │   ├── DocumentIngestionService.java
│   │   │   │   └── RagQueryService.java
│   │   │   └── web/
│   │   │       ├── RagController.java
│   │   │       └── dto/
│   │   │           ├── IngestDocumentRequest.java
│   │   │           ├── AskQuestionRequest.java
│   │   │           └── RagAnswerResponse.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       └── db/migration/V1__init_vector_extension.sql
│   └── test/
│       └── java/com/example/rag/
│           └── RagIntegrationTest.java
├── Dockerfile
└── compose.yaml
```

---

## 2. Complete Production Implementation

### 2.1 `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.2</version>
        <relativePath/>
    </parent>
    <groupId>com.example</groupId>
    <artifactId>rag-spring-ai</artifactId>
    <version>1.0.0</version>
    <name>rag-spring-ai</name>
    <description>Enterprise RAG Knowledge API with Spring AI and PgVector</description>

    <properties>
        <java.version>21</java.version>
        <spring-ai.version>1.0.0-M5</spring-ai.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencyManagement>
        <dependencies>
            <dependency>
                <groupId>org.springframework.ai</groupId>
                <artifactId>spring-ai-bom</artifactId>
                <version>${spring-ai.version}</version>
                <type>pom</type>
                <scope>import</scope>
            </dependency>
        </dependencies>
    </dependencyManagement>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.ai</groupId>
            <artifactId>spring-ai-openai-spring-boot-starter</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.ai</groupId>
            <artifactId>spring-ai-pgvector-store-spring-boot-starter</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>

        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-testcontainers</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <repositories>
        <repository>
            <id>spring-milestones</id>
            <name>Spring Milestones</name>
            <url>https://repo.spring.io/milestone</url>
            <snapshots><enabled>false</enabled></snapshots>
        </repository>
    </repositories>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2.2 `application.yaml`

```yaml
server:
  port: 8080

spring:
  application:
    name: rag-spring-ai
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:rag_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  ai:
    openai:
      api-key: ${OPENAI_API_KEY:mock-key-for-local-testing}
      chat:
        options:
          model: gpt-4o-mini
          temperature: 0.2
      embedding:
        options:
          model: text-embedding-3-small
    vectorstore:
      pgvector:
        index-type: HNSW
        distance-type: COSINE_DISTANCE
        dimensions: 1536
```

### 2.3 Spring AI Configuration & Services

```java
package com.example.rag.config;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class RagConfiguration {

    @Bean
    public ChatClient chatClient(ChatClient.Builder builder) {
        return builder.build();
    }
}
```

```java
package com.example.rag.web.dto;

import jakarta.validation.constraints.NotBlank;

public record IngestDocumentRequest(
        @NotBlank String documentId,
        @NotBlank String title,
        @NotBlank String textContent
) {}
```

```java
package com.example.rag.web.dto;

import jakarta.validation.constraints.NotBlank;

public record AskQuestionRequest(
        @NotBlank String question
) {}
```

```java
package com.example.rag.web.dto;

import java.util.List;

public record RagAnswerResponse(
        String question,
        String answer,
        List<String> sourceCitations
) {}
```

```java
package com.example.rag.service;

import com.example.rag.web.dto.IngestDocumentRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.ai.document.Document;
import org.springframework.ai.transformer.splitter.TokenTextSplitter;
import org.springframework.ai.vectorstore.VectorStore;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;

@Service
public class DocumentIngestionService {

    private static final Logger log = LoggerFactory.getLogger(DocumentIngestionService.class);
    private final VectorStore vectorStore;
    private final TokenTextSplitter textSplitter = new TokenTextSplitter(400, 50, 10, 5000, true);

    public DocumentIngestionService(VectorStore vectorStore) {
        this.vectorStore = vectorStore;
    }

    public void ingestDocument(IngestDocumentRequest request) {
        log.info("Ingesting document '{}' (id: {})...", request.title(), request.documentId());

        Document rawDocument = new Document(
                request.textContent(),
                Map.of("docId", request.documentId(), "title", request.title())
        );

        List<Document> chunks = textSplitter.apply(List.of(rawDocument));
        log.info("Split document into {} vector chunks. Writing to PgVector...", chunks.size());

        vectorStore.accept(chunks);
    }
}
```

```java
package com.example.rag.service;

import com.example.rag.web.dto.RagAnswerResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.chat.prompt.PromptTemplate;
import org.springframework.ai.document.Document;
import org.springframework.ai.vectorstore.SearchRequest;
import org.springframework.ai.vectorstore.VectorStore;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@Service
public class RagQueryService {

    private static final Logger log = LoggerFactory.getLogger(RagQueryService.class);
    private final VectorStore vectorStore;
    private final ChatClient chatClient;

    private static final String RAG_PROMPT_TEMPLATE = """
            You are a technical support assistant. Answer the user query strictly using the supplied reference documents.
            If the answer cannot be determined from the documents, state that you do not have sufficient information.

            --- REFERENCE DOCUMENTS ---
            {context}

            --- USER QUESTION ---
            {question}
            """;

    public RagQueryService(VectorStore vectorStore, ChatClient chatClient) {
        this.vectorStore = vectorStore;
        this.chatClient = chatClient;
    }

    public RagAnswerResponse askQuestion(String question) {
        log.info("Executing vector similarity search for query: '{}'", question);
        List<Document> matchingDocs = vectorStore.similaritySearch(
                SearchRequest.query(question).withTopK(3).withSimilarityThreshold(0.70)
        );

        String contextText = matchingDocs.stream()
                .map(Document::getText)
                .collect(Collectors.joining("\n\n"));

        List<String> citations = matchingDocs.stream()
                .map(doc -> (String) doc.getMetadata().getOrDefault("title", "Unknown Source"))
                .distinct()
                .toList();

        PromptTemplate template = new PromptTemplate(RAG_PROMPT_TEMPLATE);
        String renderedPrompt = template.render(Map.of("context", contextText, "question", question));

        String answer = chatClient.prompt()
                .user(renderedPrompt)
                .call()
                .content();

        return new RagAnswerResponse(question, answer, citations);
    }
}
```

```java
package com.example.rag.web;

import com.example.rag.service.DocumentIngestionService;
import com.example.rag.service.RagQueryService;
import com.example.rag.web.dto.AskQuestionRequest;
import com.example.rag.web.dto.IngestDocumentRequest;
import com.example.rag.web.dto.RagAnswerResponse;
import jakarta.validation.Valid;
import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/rag")
public class RagController {

    private final DocumentIngestionService ingestionService;
    private final RagQueryService queryService;

    public RagController(DocumentIngestionService ingestionService, RagQueryService queryService) {
        this.ingestionService = ingestionService;
        this.queryService = queryService;
    }

    @PostMapping("/ingest")
    @ResponseStatus(HttpStatus.ACCEPTED)
    public void ingest(@Valid @RequestBody IngestDocumentRequest request) {
        ingestionService.ingestDocument(request);
    }

    @PostMapping("/query")
    public RagAnswerResponse askQuestion(@Valid @RequestBody AskQuestionRequest request) {
        return queryService.askQuestion(request.question());
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.rag;

import com.example.rag.service.DocumentIngestionService;
import com.example.rag.web.dto.IngestDocumentRequest;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.vectorstore.VectorStore;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import static org.assertj.core.api.Assertions.assertThatNoException;

@SpringBootTest
@Testcontainers
public class RagIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> pgVector = new PostgreSQLContainer<>("pgvector/pgvector:pg16");

    @MockBean
    private ChatClient chatClient;

    @MockBean
    private VectorStore vectorStore;

    @Autowired
    private DocumentIngestionService ingestionService;

    @Test
    void shouldIngestAndSplitDocumentChunks() {
        IngestDocumentRequest req = new IngestDocumentRequest(
                "doc-01",
                "Kubernetes Architecture",
                "Kubernetes is an open-source container orchestration system for automating application deployment..."
        );

        assertThatNoException().isThrownBy(() -> ingestionService.ingestDocument(req));
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src ./src
RUN mvn clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
WORKDIR /app
COPY --from=builder /app/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:+ZGenerational", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  pgvector-db:
    image: pgvector/pgvector:pg16
    container_name: pgvector-db
    environment:
      POSTGRES_DB: rag_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: ragpassword
    ports:
      - "5432:5432"
    volumes:
      - pgvector_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  rag-service:
    build: .
    container_name: rag-service
    ports:
      - "8080:8080"
    environment:
      DB_HOST: pgvector-db
      DB_PORT: 5432
      DB_NAME: rag_db
      DB_USER: postgres
      DB_PASS: ragpassword
      OPENAI_API_KEY: ${OPENAI_API_KEY}
    depends_on:
      pgvector-db:
        condition: service_healthy

volumes:
  pgvector_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start application stack with your OpenAI API key
OPENAI_API_KEY=sk-proj-xxxx docker compose up -d

# 2. Ingest internal knowledge document
curl -i -X POST http://localhost:8080/api/v1/rag/ingest \
  -H "Content-Type: application/json" \
  -d '{
    "documentId": "hr-policy-01",
    "title": "Remote Work Policy 2026",
    "textContent": "Employees are eligible for a $1,500 annual home office equipment stipend starting January 2026."
  }'

# 3. Query the knowledge base via RAG
curl -i -X POST http://localhost:8080/api/v1/rag/query \
  -H "Content-Type: application/json" \
  -d '{"question": "How much is the home office stipend for 2026?"}'

# Expected response:
# {
#   "question": "How much is the home office stipend for 2026?",
#   "answer": "According to the Remote Work Policy 2026, employees are eligible for an annual home office equipment stipend of $1,500.",
#   "sourceCitations": ["Remote Work Policy 2026"]
# }
```
