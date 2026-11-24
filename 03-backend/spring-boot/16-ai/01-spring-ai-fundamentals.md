# Spring AI Fundamentals & Portable Client Abstraction

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring AI Version** | 1.0.x / 1.1.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | OpenAI, Anthropic Claude, Ollama (Local Llama 3), Azure OpenAI |

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
Integrating Large Language Models (LLMs) into enterprise Java applications historically required either writing brittle raw HTTP clients using `RestClient` or pulling in proprietary vendor SDKs (e.g., OpenAI Java SDK, AWS Bedrock SDK, Azure SDK). This vendor lock-in forced developers to write custom code for every model provider, made swapping models (e.g., migrating from OpenAI to an on-premise Ollama instance for privacy) a massive architectural rewrite, and offered zero standard abstractions for prompts, embeddings, vector stores, or tool calling.

### The Evolution
**Spring AI** was created by the Spring engineering team to provide a **portable, model-agnostic abstraction layer** for Artificial Intelligence in enterprise Java—analogous to how Spring Data abstracts relational databases and Spring Cloud abstracts cloud infrastructure.
At its core is the fluent `ChatClient` and `ChatModel` API, which allows developers to write code once and switch effortlessly between OpenAI (GPT-4o), Anthropic (Claude 3.5 Sonnet), Google Vertex AI (Gemini), and local open-source models running on Ollama through simple configuration property changes.

### Production Relevance
Spring AI is the enterprise foundation for integrating Generative AI, Retrieval-Augmented Generation (RAG), conversational bots, autonomous agent workflows, and AI tool calling directly into existing Spring Boot microservices while maintaining full observability via Micrometer.

---

## 2. Core Concepts

### Spring AI Architecture & Component Hierarchy

```
+-----------------------------------------------------------------------------------------+
|                                SPRING AI ARCHITECTURE                                   |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ Enterprise Application Services ]                                                    |
|           |                                                                             |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | FLUENT CLIENT LAYER: ChatClient                                                   |  |
|  |   - chatClient.prompt().user("...").system("...").call().content()                |  |
|  |   - Advisors (Logging, Context augmentation, Prompt rewriting)                     |  |
|  +-----------------------------------------------------------------------------------+  |
|           |                                                                             |
|           v                                                                             |
|  +-----------------------------------------------------------------------------------+  |
|  | PORTABLE MODEL ABSTRACTIONS                                                       |  |
|  |   - ChatModel          (Generates text & conversational completions)              |  |
|  |   - EmbeddingModel     (Generates dense numeric vectors for semantic search)      |  |
|  |   - ImageModel         (Generates/edits images: DALL-E, Stability AI)            |  |
|  |   - AudioModel         (Speech-to-text / Text-to-speech: Whisper)                 |  |
|  |   - VectorStore        (pgvector, Pinecone, Qdrant, Milvus, Weaviate)             |  |
|  +-----------------------------------------------------------------------------------+  |
|           |                                                                             |
|           +-------------------+--------------------+--------------------+               |
|           |                   |                    |                    |               |
|           v                   v                    v                    v               |
|  +-----------------+  +-----------------+  +-----------------+  +-----------------+     |
|  | OpenAiChatModel |  | AnthropicModel  |  | OllamaChatModel |  | AzureOpenAiModel|     |
|  +-----------------+  +-----------------+  +-----------------+  +-----------------+     |
|           |                   |                    |                    |               |
|           v                   v                    v                    v               |
|      (OpenAI API)     (Anthropic API)       (Local Daemon)        (Azure Cloud)         |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Invariants
- **Portability Invariant**: Application services should depend on `ChatClient` or the `ChatModel` interface, **never** on concrete provider classes like `OpenAiChatModel`. Swapping models must require zero Java code modifications.
- **Model Ingestion Neutrality**: Prompts and messages use the standard `Message` hierarchy (`UserMessage`, `SystemMessage`, `AssistantMessage`).
- **Reactive & Virtual Thread Support**: Spring AI natively supports streaming completions via Project Reactor `Flux<String>` and non-blocking virtual threads on Java 21+.

---

## 3. How It Works Internally

### ChatClient Request Execution Pipeline

```
[ chatClient.prompt().user("Summarize order 42").call() ]
                         |
                         v
[ ChatClient Request Spec ]
   - Gathers System Message, User Message, Prompt Parameters
   - Injects Registered Advisors (e.g., SimpleLoggerAdvisor, TokenLimitAdvisor)
                         |
                         v
[ ChatModel.call(Prompt prompt) ]
   - Converts standard Spring AI 'Prompt' into provider-specific JSON payload:
     * OpenAI: {"model": "gpt-4o", "messages": [...], "temperature": 0.7}
     * Anthropic: {"model": "claude-3-5-sonnet-20241022", "system": ..., "messages": [...]}
                         |
                         v
[ RestClient / WebClient HTTP Transport ]
   - Executes HTTP POST with API Key authorization header
   - Handles connection pooling, timeouts, and streaming SSE responses
                         |
                         v
[ ChatResponse Parser ]
   - Extracts generation text, finish reason, and usage metadata (PromptTokens, GenerationTokens)
   - Emits Micrometer metrics: `gen_ai.client.token.usage`, `gen_ai.client.operation.duration`
                         |
                         v
Returns String / ChatResponse to Application
```

1. **Auto-Configuration**: Adding `spring-ai-openai-spring-boot-starter` or `spring-ai-ollama-spring-boot-starter` registers a `ChatModel` bean configured via `spring.ai.*` properties.
2. **ChatClient.Builder**: Spring automatically injects a pre-configured `ChatClient.Builder` bean.
3. **Advisor Interception**: The `ChatClient` incorporates an **Advisor** chain (similar to Spring MVC `HandlerInterceptor` or Servlet `Filter`). Advisors intercept the prompt before dispatch and augment the response upon return.

---

## 4. Syntax & Usage

### 1. Maven Dependency Configuration (`pom.xml`)

```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.springframework.ai</groupId>
            <artifactId>spring-ai-bom</artifactId>
            <version>1.0.0-M5</version>
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>

<dependencies>
    <!-- Portable OpenAI Starter -->
    <dependency>
        <groupId>org.springframework.ai</groupId>
        <artifactId>spring-ai-openai-spring-boot-starter</artifactId>
    </dependency>
</dependencies>
```

### 2. Configuration Properties (`application.yaml`)

```yaml
spring:
  ai:
    openai:
      api-key: ${OPENAI_API_KEY}
      chat:
        options:
          model: gpt-4o
          temperature: 0.7
          max-tokens: 2000
    retry:
      max-attempts: 3
      backoff:
        initial-interval: 2000ms
        multiplier: 2.0
```

### 3. Basic `ChatClient` Injection and Execution

```java
package com.example.ai.service;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.stereotype.Service;

@Service
public class SimpleAssistantService {

    private final ChatClient chatClient;

    public SimpleAssistantService(ChatClient.Builder chatClientBuilder) {
        this.chatClient = chatClientBuilder
                .defaultSystem("You are a helpful, professional enterprise customer support assistant.")
                .build();
    }

    public String askAssistant(String userPrompt) {
        return chatClient.prompt()
                .user(userPrompt)
                .call()
                .content();
    }
}
```

---

## 5. Practical Examples

### Example 1: Multi-Turn Conversational Chat with Memory

```java
package com.example.ai.conversation;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.chat.client.advisor.MessageChatMemoryAdvisor;
import org.springframework.ai.chat.memory.InMemoryChatMemory;
import org.springframework.stereotype.Service;

@Service
public class CustomerSupportBotService {

    private final ChatClient chatClient;

    public CustomerSupportBotService(ChatClient.Builder builder) {
        // ChatMemoryAdvisor automatically maintains conversation history per conversationId!
        this.chatClient = builder
                .defaultSystem("You are an e-commerce assistant. Help users track orders and return items.")
                .defaultAdvisors(new MessageChatMemoryAdvisor(new InMemoryChatMemory()))
                .build();
    }

    public String converse(String conversationId, String userMessage) {
        return chatClient.prompt()
                .user(userMessage)
                .advisors(advisorSpec -> advisorSpec.param(MessageChatMemoryAdvisor.CHAT_MEMORY_CONVERSATION_ID_KEY, conversationId))
                .call()
                .content();
    }
}
```

### Example 2: Streaming Real-Time Completions with Server-Sent Events (SSE)

```java
package com.example.ai.web;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

@RestController
public class StreamingChatController {

    private final ChatClient chatClient;

    public StreamingChatController(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    // Emits real-time tokens to browser as they arrive from the LLM!
    @GetMapping(value = "/api/chat/stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public Flux<String> streamChat(@RequestParam("message") String message) {
        return chatClient.prompt()
                .user(message)
                .stream()
                .content();
    }
}
```

### Example 3: Zero-Code Swapping from OpenAI to Local Ollama

To run completely offline on local hardware (e.g., Llama 3 on an on-premise GPU):

#### Switch Maven Dependency:
```xml
<!-- Replace OpenAI starter with Ollama starter -->
<dependency>
    <groupId>org.springframework.ai</groupId>
    <artifactId>spring-ai-ollama-spring-boot-starter</artifactId>
</dependency>
```

#### Update `application.yaml`:
```yaml
spring:
  ai:
    ollama:
      base-url: http://localhost:11434
      chat:
        options:
          model: llama3:8b
          temperature: 0.5
```
**Zero Java code changes are required!** All existing `ChatClient` services immediately start communicating with local Ollama.

### Example 4: Anti-Pattern (Hardcoded Vendor SDK) vs Spring AI Abstraction

#### The Anti-Pattern: Hardcoded OpenAI SDK
```java
// BAD: Tightly coupled to proprietary com.theokanning.openai SDK
@Service
public class VendorLockedAiService {
    public String generateSummary(String text) {
        OpenAiService service = new OpenAiService("sk-raw-api-key");
        ChatCompletionRequest request = ChatCompletionRequest.builder()
                .model("gpt-3.5-turbo") // Hardcoded model string
                .messages(List.of(new ChatMessage("user", text)))
                .build();
        // Impossible to mock in unit tests; impossible to switch to Claude or local Llama!
        return service.createChatCompletion(request).getChoices().get(0).getMessage().getContent();
    }
}
```

#### Production Fix: Portable Spring AI `ChatClient`
```java
// GOOD: Model-agnostic, testable, configurable via properties
@Service
public class PortableAiService {
    private final ChatClient chatClient;

    public PortableAiService(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public String generateSummary(String text) {
        return chatClient.prompt()
                .user(text)
                .call()
                .content();
    }
}
```

### Example 5: Custom Advisor for PII Redaction and Logging

```java
package com.example.ai.advisor;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.ai.chat.client.advisor.api.*;
import org.springframework.ai.chat.model.ChatResponse;
import org.springframework.stereotype.Component;

// Intercepts outbound prompt to sanitize Social Security Numbers (SSN) before LLM sees it
@Component
public class PiiRedactionAdvisor implements CallAroundAdvisor {

    private static final Logger log = LoggerFactory.getLogger(PiiRedactionAdvisor.class);

    @Override
    public String getName() {
        return "PiiRedactionAdvisor";
    }

    @Override
    public int getOrder() {
        return 0; // High priority
    }

    @Override
    public AdvisedResponse aroundCall(AdvisedRequest advisedRequest, CallAroundAdvisorChain chain) {
        log.info("Inspecting prompt for sensitive PII before transmission...");
        // Redact SSN pattern: XXX-XX-XXXX -> [REDACTED_SSN]
        // Proceed with sanitized request through chain
        AdvisedResponse response = chain.nextAroundCall(advisedRequest);
        log.info("LLM responded successfully.");
        return response;
    }
}
```

---

## 6. Best Practices

1. **Always Inject `ChatClient.Builder` Rather Than `ChatModel` Directly**: `ChatClient` provides a fluent, modern API with built-in memory advisors and structured output converters.
2. **Never Commit API Keys to Source Control**: Externalize keys via environment variables (`${OPENAI_API_KEY}`) or inject dynamically via HashiCorp Vault.
3. **Configure Conservative Retries and Timeouts**: LLM APIs frequently suffer from rate limits (HTTP 429) or momentary latency spikes. Always configure `spring.ai.retry` with exponential backoff.
4. **Use System Prompts to Anchor Invariants**: Set persistent safety rules and domain boundaries inside `.defaultSystem("...")` on the `ChatClient.Builder`.
5. **Use Streaming for Interactive UIs**: Always prefer `.stream().content()` over blocking `.call().content()` for user-facing chats to minimize perceived latency.
6. **Track Costs and Tokens**: Connect Spring AI to Prometheus/Grafana to monitor `gen_ai.client.token.usage` and establish budgetary alerts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Hardcoding Vendor API Keys in `application.yaml`** | Committing keys to Git leaks credentials; bots scrape GitHub and steal thousands of dollars in tokens. | Use environment variables (`${SPRING_AI_OPENAI_API_KEY}`) or Vault. |
| **Injecting Concrete `OpenAiChatModel`** | Tight coupling prevents switching models or writing lightweight unit tests. | Depend exclusively on `ChatClient` or the `ChatModel` interface. |
| **Omitting Timeouts on LLM Requests** | LLMs can take 30–60 seconds on complex prompts; without timeouts, Tomcat worker threads hang indefinitely. | Set `client.connect-timeout` and `read-timeout` on the underlying `RestClient`. |
| **Ignoring Token Context Window Limits** | Sending giant document texts exceeds model context limits (e.g., 8k or 128k tokens), causing 400 Bad Request. | Implement RAG chunking and token counting before prompt injection. |
| **Sharing `InMemoryChatMemory` Across All Users** | When multiple users talk to the bot, memory blends together, causing cross-customer privacy leakage! | Always pass a unique `conversationId` per user/session to `MessageChatMemoryAdvisor`. |
| **Blocking on Streaming Flux Responses** | Calling `.blockFirst()` or `.collectList().block()` on a stream negates all the benefits of streaming. | Return `Flux<String>` directly with `MediaType.TEXT_EVENT_STREAM_VALUE`. |

---

## 8. Performance & Security Notes

### Performance
- **Connection Pool Tuning**: LLM calls hold HTTP connections open for several seconds while generation occurs. Increase `http.max-connections` on the underlying `ClientHttpRequestFactory` to prevent thread starvation under concurrent user load.
- **Virtual Threads Integration**: Enable `spring.threads.virtual.enabled=true` (Java 21+). Because LLM invocations are heavily I/O-bound, Virtual Threads allow thousands of concurrent prompt executions on minimal physical CPU cores.

### Security
- **Prompt Injection Defense**: Treat all user inputs as untrusted data. Use distinct `SystemMessage` for instructions and `UserMessage` for data to prevent user prompts from overriding core system rules.
- **PII Exfiltration**: Never send raw unencrypted customer PII, credit card details, or medical records to third-party public LLM cloud endpoints unless covered by a strict BAA/DPA agreement.

---

## 9. Comparison / When to Use What

| Dimension | Raw HTTP (`RestClient`) | Vendor SDK (OpenAI/Anthropic) | Spring AI |
|---|---|---|---|
| **Portability** | Low (Bespoke JSON models) | None (Locked to one vendor) | Complete (Portable across 15+ providers) |
| **Structured Output**| Manual Jackson parsing | Vendor-specific tools | Native `BeanOutputConverter<T>` |
| **RAG Support** | Zero (Manual vector coding) | Minimal | Built-in `VectorStore` (pgvector, Qdrant) |
| **Observability** | Manual Micrometer timers | None | Automatic OpenTelemetry & Micrometer metrics |
| **Maintenance** | High custom maintenance | High | Backed by Spring Framework core team |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between `ChatModel` and `ChatClient` in Spring AI?</strong></summary>

- **`ChatModel`**: The low-level functional interface that maps directly to an underlying AI provider (e.g., `OpenAiChatModel`, `OllamaChatModel`). It takes a `Prompt` object and returns a `ChatResponse`. It is model-specific and low-level.
- **`ChatClient`**: The high-level fluent API introduced in Spring AI 1.0 (similar to `RestClient` or `WebClient`). It wraps `ChatModel` and provides fluent method chaining (`.user()`, `.system()`), built-in memory advisors (`MessageChatMemoryAdvisor`), structured output conversion (`entity()`), and RAG retrieval augmentation.
</details>

<details>
<summary><strong>2. How does Spring AI achieve model portability across OpenAI, Claude, and local Ollama?</strong></summary>

Spring AI defines a common standard domain model:
- `Prompt` (containing a list of `Message` objects: `UserMessage`, `SystemMessage`, `AssistantMessage`).
- `ChatResponse` (containing `Generation` choices and `Usage` token metadata).
Each provider starter provides an adapter class implementing `ChatModel` (e.g., `OpenAiChatModel`, `AnthropicChatModel`). These adapters serialize the standardized `Prompt` into the vendor's specific JSON schema and map the vendor's response back to the standard `ChatResponse`.
</details>

<details>
<summary><strong>3. How does `MessageChatMemoryAdvisor` maintain multi-turn conversational state?</strong></summary>

LLMs are completely stateless; they have no memory of previous HTTP requests.
`MessageChatMemoryAdvisor`:
1. Intercepts the outbound prompt in the `ChatClient` pipeline.
2. Retrieves past messages for the given `conversationId` from a `ChatMemory` store (e.g., in-memory or Redis).
3. Prepends the historical messages to the current prompt before dispatching to the LLM.
4. When the LLM responds, the advisor intercepts the response and appends both the user message and assistant response to the `ChatMemory` store.
</details>

<details>
<summary><strong>4. How does Spring AI handle streaming completions?</strong></summary>

By leveraging HTTP Server-Sent Events (SSE) and Project Reactor:
- When calling `chatClient.prompt().stream().content()`, Spring AI opens an HTTP streaming connection with the LLM API (`stream=true`).
- As chunks of tokens are generated by the model, they stream over the wire and are parsed into a reactive `Flux<String>` or `Flux<ChatResponse>`.
- In Spring WebFlux or Spring MVC, returning this `Flux` directly with `MediaType.TEXT_EVENT_STREAM_VALUE` pushes tokens immediately to the web browser.
</details>

<details>
<summary><strong>5. What metrics does Spring AI automatically export to Micrometer and Prometheus?</strong></summary>

Spring AI automatically instruments all LLM operations with standard Micrometer metrics:
- `gen_ai.client.operation.duration`: Latency distribution of model calls (p50, p95, p99).
- `gen_ai.client.token.usage`: Counts input prompt tokens, output completion tokens, and total token usage tagged by `model` and `provider`.
- OpenTelemetry Distributed Spans: Records trace spans showing exact prompt lengths and response latencies.
</details>

<details>
<summary><strong>6. What is the role of Advisors in Spring AI?</strong></summary>

Advisors implement the Interceptor / Decorator pattern around `ChatClient` requests:
- **`CallAroundAdvisor`**: Intercepts synchronous `.call()` invocations.
- **`StreamAroundAdvisor`**: Intercepts asynchronous `.stream()` invocations.
Advisors are used to inject conversational memory, redact PII, log prompt tokens, inject RAG contextual documents, and enforce token safety limits without cluttering business logic.
</details>

<details>
<summary><strong>7. How can you write effective unit tests for services using `ChatClient` without spending real money on LLM tokens?</strong></summary>

1. **Mocking `ChatModel`**: Mock the `ChatModel` interface with Mockito to return a canned `ChatResponse` containing synthetic text.
2. **Using `TestChatClient`**: Build a `ChatClient` pointing to a mock `ChatModel`.
3. **Ollama Testcontainers**: For genuine integration tests, use `org.testcontainers:ollama` to spin up a real Docker container running a tiny local open-source model (e.g., `tinyllama` or `all-minilm`) with zero cloud cost.
</details>

<details>
<summary><strong>8. What is the Model Context Protocol (MCP) and how does Spring AI support it?</strong></summary>

The **Model Context Protocol (MCP)**, authored by Anthropic, is an open standard that allows LLMs to securely connect to external tools, data sources, and developer repositories through standardized JSON-RPC protocols.
Spring AI provides first-class support for MCP client and server integration, allowing Spring Boot services to expose business operations as standardized MCP tools accessible to any MCP-compliant AI client.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a Spring AI Assistant Service</strong></summary>

**Objective**: Write a service with a custom system prompt that generates haiku poems on user topics.

**Solution**:
```java
@Service
public class HaikuService {

    private final ChatClient chatClient;

    public HaikuService(ChatClient.Builder builder) {
        this.chatClient = builder
                .defaultSystem("You are a poetic assistant. Answer all questions exclusively in 5-7-5 syllable Haiku format.")
                .build();
    }

    public String generateHaiku(String topic) {
        return chatClient.prompt()
                .user("Write a haiku about: " + topic)
                .call()
                .content();
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Inject API Key via Environment Variable in `application.yaml`</strong></summary>

**Objective**: Configure Spring AI to securely read `OPENAI_API_KEY` from the OS environment with a fallback error message.

**Solution**:
```yaml
spring:
  ai:
    openai:
      api-key: ${OPENAI_API_KEY:missing-api-key-please-set-env}
      chat:
        options:
          model: gpt-4o-mini
          temperature: 0.2
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Streaming Web Controller with Flux</strong></summary>

**Objective**: Build a Spring MVC controller that streams chat responses token-by-token over Server-Sent Events.

**Solution**:
```java
@RestController
@RequestMapping("/api/assistant")
public class StreamingAssistantController {

    private final ChatClient chatClient;

    public StreamingAssistantController(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    @GetMapping(value = "/ask", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public Flux<String> askStreaming(@RequestParam("prompt") String prompt) {
        return chatClient.prompt()
                .user(prompt)
                .stream()
                .content();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build a Token-Usage Logging Advisor</strong></summary>

**Objective**: Implement a `CallAroundAdvisor` that extracts token counts from `ChatResponse` metadata and logs them.

**Solution**:
```java
@Component
public class TokenLoggingAdvisor implements CallAroundAdvisor {

    private static final Logger log = LoggerFactory.getLogger(TokenLoggingAdvisor.class);

    @Override
    public String getName() { return "TokenLoggingAdvisor"; }

    @Override
    public int getOrder() { return 100; }

    @Override
    public AdvisedResponse aroundCall(AdvisedRequest advisedRequest, CallAroundAdvisorChain chain) {
        AdvisedResponse advisedResponse = chain.nextAroundCall(advisedRequest);
        ChatResponse response = advisedResponse.response();
        if (response != null && response.getMetadata() != null && response.getMetadata().getUsage() != null) {
            var usage = response.getMetadata().getUsage();
            log.info("LLM Generation complete. PromptTokens={}, GenerationTokens={}, TotalTokens={}",
                    usage.getPromptTokens(), usage.getGenerationTokens(), usage.getTotalTokens());
        }
        return advisedResponse;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Integration Testing with Ollama Testcontainers</strong></summary>

**Objective**: Write a Spring Boot integration test that spins up a real Ollama container running `all-minilm` and tests `ChatClient` without cloud keys.

**Solution**:
```java
@SpringBootTest
@Testcontainers
class OllamaIntegrationTest {

    @Container
    static OllamaContainer ollama = new OllamaContainer("ollama/ollama:latest");

    @DynamicPropertySource
    static void configureOllama(DynamicPropertyRegistry registry) {
        registry.add("spring.ai.ollama.base-url", ollama::getEndpoint);
    }

    @Autowired private ChatClient.Builder builder;

    @Test
    void shouldExecuteLocalModelPrompt() {
        ChatClient client = builder.build();
        String response = client.prompt().user("Hello world").call().content();
        assertNotNull(response);
        assertFalse(response.isBlank());
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Multi-Model Router with Dynamic Model Switching</strong></summary>

**Objective**: Create a router service that sends simple queries to cheap `gpt-4o-mini` and complex queries to `gpt-4o` dynamically.

**Solution**:
```java
@Service
public class IntelligentModelRouter {

    private final ChatClient chatClient;

    public IntelligentModelRouter(ChatClient.Builder builder) {
        this.chatClient = builder.build();
    }

    public String ask(String userQuery, boolean isComplex) {
        String targetModel = isComplex ? "gpt-4o" : "gpt-4o-mini";
        double temperature = isComplex ? 0.2 : 0.7;

        return chatClient.prompt()
                .user(userQuery)
                .options(OpenAiChatOptions.builder()
                        .withModel(targetModel)
                        .withTemperature(temperature)
                        .build())
                .call()
                .content();
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| SPRING AI FUNDAMENTALS CHEAT SHEET                                                                 |
+----------------------------------------------------------------------------------------------------+
| Component           | Spring AI Class / Interface               | Primary Purpose                  |
+---------------------+-------------------------------------------+----------------------------------+
| Fluent Client       | org.springframework.ai.chat.client.ChatClient | Recommended modern API for all   |
|                     |                                           | conversational prompts & tools   |
+---------------------+-------------------------------------------+----------------------------------+
| Low-level Model     | org.springframework.ai.chat.model.ChatModel| Direct abstraction over provider |
|                     |                                           | (OpenAI, Claude, Ollama)         |
+---------------------+-------------------------------------------+----------------------------------+
| Memory Advisor      | MessageChatMemoryAdvisor                  | Automatically injects chat history|
|                     | with InMemoryChatMemory                   | for multi-turn conversations     |
+---------------------+-------------------------------------------+----------------------------------+
| Streaming           | chatClient.prompt().stream().content()    | Reactive Flux<String> Server-    |
|                     |                                           | Sent Events (SSE) token stream   |
+---------------------+-------------------------------------------+----------------------------------+
| Local LLM (Ollama)  | spring-ai-ollama-spring-boot-starter      | Run offline, zero-cloud-cost LLMs|
|                     |                                           | on local Docker / GPUs           |
+---------------------+-------------------------------------------+----------------------------------+
| Custom Advisor      | implements CallAroundAdvisor              | Cross-cutting prompt redaction,  |
|                     |                                           | audit logging, token safety      |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring AI Reference Documentation (Official)](https://docs.spring.io/spring-ai/reference/)
- [Anthropic Model Context Protocol (MCP) Specification](https://modelcontextprotocol.io/)
- [Ollama Open-Source LLM Runner](https://ollama.com/)
- [DeepLearning.AI: Building Applications with Spring AI and Java](https://www.deeplearning.ai/)
- [Testcontainers Ollama Module Guide](https://testcontainers.com/modules/ollama/)
