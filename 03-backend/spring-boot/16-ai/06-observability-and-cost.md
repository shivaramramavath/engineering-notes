# Observability, Cost Tracking, and Evaluation in Spring AI

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring AI Version** | 1.0.x / 1.1.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Micrometer, Prometheus, OpenTelemetry, Grafana, Evaluators |

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
Deploying Generative AI applications into production introduces unprecedented operational and financial risks:
1. **Unpredictable API Costs**: Unlike traditional microservices whose resource usage scales linearly with CPU and RAM, LLM APIs charge variable costs per million input and output tokens. A runaway loop, poorly chunked prompt, or spike in user traffic can result in thousands of dollars in surprise cloud bills overnight.
2. **Extreme Latency Variance**: LLM calls take anywhere from 400 milliseconds to 45 seconds depending on generation length and provider load. Without distributed tracing, identifying whether slow requests are caused by internal database queries, RAG vector searches, or the external AI provider is impossible.
3. **Quality & Hallucination Drift**: Software updates to prompts or model version changes (e.g., GPT-4o update) can silently degrade answer quality, break tool calling, or introduce hallucinations without throwing any runtime exceptions.

### The Evolution
Spring AI integrates seamlessly with **Micrometer**, **Micrometer Tracing**, and **OpenTelemetry**, treating AI operations as first-class observable components alongside standard HTTP controllers and SQL queries. Furthermore, it incorporates **AI Evaluation Frameworks** (Relevancy, Factuality, and Groundedness evaluators) to programmatically test model output quality before and during production.

### Production Relevance
Implementing comprehensive observability, token cost tracking, and automated model evaluations is mandatory for managing production budgets, meeting enterprise Service Level Objectives (SLOs), and guaranteeing reliable, hallucination-free AI features.

---

## 2. Core Concepts

### Observability & Evaluation Architecture in Spring AI

```
+-----------------------------------------------------------------------------------------+
|                        SPRING AI OBSERVABILITY & EVALUATION                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  [ ChatClient / RAG / Tool Calling Invocations ]                                        |
|                          |                                                              |
|                          v                                                              |
|  +-----------------------------------------------------------------------------------+  |
|  | OBSERVABILITY LAYER: Micrometer Observation API                                   |  |
|  |                                                                                   |  |
|  |   1. METRICS (Prometheus / Grafana)                                               |  |
|  |      - gen_ai.client.token.usage (prompt_tokens, completion_tokens, total)        |  |
|  |      - gen_ai.client.operation.duration (latency timers: p50, p95, p99)           |  |
|  |                                                                                   |  |
|  |   2. TRACING (OpenTelemetry / Jaeger / Zipkin)                                    |  |
|  |      - Distributed trace spans capturing prompt tokens, finish reason, model tag  |  |
|  |                                                                                   |  |
|  |   3. LOGGING (SLF4J / MDC)                                                        |  |
|  |      - Trace ID and Span ID correlated across HTTP, DB, and AI calls              |  |
|  +-----------------------------------------------------------------------------------+  |
|                          |                                                              |
|                          v                                                              |
|  +-----------------------------------------------------------------------------------+  |
|  | EVALUATION LAYER (Automated Quality Testing)                                      |  |
|  |   - RelevancyEvaluator (Does the response answer the user's specific prompt?)     |  |
|  |   - FactualityEvaluator (Is the answer supported by ground truth facts?)          |  |
|  |   - GroundednessEvaluator (Does the RAG answer strictly derive from context?)     |  |
|  +-----------------------------------------------------------------------------------+  |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Key Architectural Invariants
- **Semantic Conventions Compliance**: Spring AI adheres strictly to OpenTelemetry GenAI semantic conventions (`gen_ai.system`, `gen_ai.request.model`, `gen_ai.usage.input_tokens`).
- **Cost Isolation by Tenant**: In multi-tenant environments, every token consumption metric must be tagged with `tenant_id` to enable accurate customer chargebacks.
- **Continuous Quality Testing**: Quality regression tests evaluate model responses against golden datasets in CI/CD pipelines before code is deployed to production.

---

## 3. How It Works Internally

### Micrometer Observation Hook in Spring AI

```
[ chatClient.prompt().user("Hello").call() ]
                    |
                    v
[ ObservationRegistry.createNotStarted("gen_ai.client.operation", ...) ]
   - Starts timer span: records start time
                    |
                    v
[ ChatModel.call(prompt) ]
   - Dispatches HTTP call to OpenAI/Claude
   - Receives ChatResponse with Usage metadata:
     * prompt_tokens: 42
     * completion_tokens: 18
                    |
                    v
[ Observation.stop() ]
   - Records duration timer: gen_ai.client.operation.duration
   - Increments counter: gen_ai.client.token.usage{type="input"} += 42
   - Increments counter: gen_ai.client.token.usage{type="output"} += 18
   - Closes OpenTelemetry Span: tags span with model="gpt-4o", finish_reason="stop"
                    |
                    v
[ Prometheus Scrape Endpoint (/actuator/prometheus) ]
```

Spring AI utilizes Spring Framework's **Observation API**. Every model invocation automatically participates in the active distributed trace and exports standardized metrics to whichever monitoring backend (Prometheus, Datadog, Dynatrace) is configured in Spring Boot Actuator.

---

## 4. Syntax & Usage

### 1. Actuator and Prometheus Dependencies (`pom.xml`)

```xml
<dependencies>
    <!-- Spring Boot Actuator -->
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-actuator</artifactId>
    </dependency>

    <!-- Prometheus Meter Registry -->
    <dependency>
        <groupId>io.micrometer</groupId>
        <artifactId>micrometer-registry-prometheus</artifactId>
    </dependency>

    <!-- OpenTelemetry Tracing Bridge -->
    <dependency>
        <groupId>io.micrometer</groupId>
        <artifactId>micrometer-tracing-bridge-otel</artifactId>
    </dependency>
</dependencies>
```

### 2. Configuration in `application.yaml`

```yaml
management:
  endpoints:
    web:
      exposure:
        include: health,info,metrics,prometheus
  endpoint:
    prometheus:
      enabled: true
  metrics:
    tags:
      application: ai-customer-service
  observations:
    key-values:
      environment: production

# Spring AI Specific Log & Tracing options
spring:
  ai:
    chat:
      observations:
        include-prompt: false # Set FALSE in production to prevent PII leakage in traces!
```

---

## 5. Practical Examples

### Example 1: Real-Time Multi-Tenant Token Cost Tracker

```java
package com.example.ai.cost;

import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.ai.chat.client.advisor.api.AdvisedRequest;
import org.springframework.ai.chat.client.advisor.api.AdvisedResponse;
import org.springframework.ai.chat.client.advisor.api.CallAroundAdvisor;
import org.springframework.ai.chat.client.advisor.api.CallAroundAdvisorChain;
import org.springframework.ai.chat.metadata.Usage;
import org.springframework.stereotype.Component;

import java.math.BigDecimal;
import java.math.RoundingMode;

@Component
public class TenantCostTrackingAdvisor implements CallAroundAdvisor {

    private final MeterRegistry meterRegistry;

    // Pricing rates per 1,000,000 tokens (e.g. GPT-4o: $2.50 input / $10.00 output)
    private static final BigDecimal INPUT_COST_PER_M = new BigDecimal("2.50");
    private static final BigDecimal OUTPUT_COST_PER_M = new BigDecimal("10.00");
    private static final BigDecimal ONE_MILLION = new BigDecimal("1000000");

    public TenantCostTrackingAdvisor(MeterRegistry meterRegistry) {
        this.meterRegistry = meterRegistry;
    }

    @Override
    public String getName() { return "TenantCostTrackingAdvisor"; }

    @Override
    public int getOrder() { return 50; }

    @Override
    public AdvisedResponse aroundCall(AdvisedRequest advisedRequest, CallAroundAdvisorChain chain) {
        String tenantId = (String) advisedRequest.adviseContext().getOrDefault("tenantId", "unknown");

        AdvisedResponse advisedResponse = chain.nextAroundCall(advisedRequest);

        if (advisedResponse.response() != null && advisedResponse.response().getMetadata() != null) {
            Usage usage = advisedResponse.response().getMetadata().getUsage();
            if (usage != null) {
                long inputTokens = usage.getPromptTokens();
                long outputTokens = usage.getGenerationTokens();

                // 1. Record Prometheus Counters
                Counter.builder("app.ai.tokens")
                        .tag("tenant", tenantId)
                        .tag("type", "input")
                        .register(meterRegistry)
                        .increment(inputTokens);

                Counter.builder("app.ai.tokens")
                        .tag("tenant", tenantId)
                        .tag("type", "output")
                        .register(meterRegistry)
                        .increment(outputTokens);

                // 2. Calculate Exact Cost in USD
                BigDecimal inputCost = new BigDecimal(inputTokens).multiply(INPUT_COST_PER_M).divide(ONE_MILLION, 6, RoundingMode.HALF_UP);
                BigDecimal outputCost = new BigDecimal(outputTokens).multiply(OUTPUT_COST_PER_M).divide(ONE_MILLION, 6, RoundingMode.HALF_UP);
                double totalCostDollars = inputCost.add(outputCost).doubleValue();

                Counter.builder("app.ai.cost.dollars")
                        .tag("tenant", tenantId)
                        .register(meterRegistry)
                        .increment(totalCostDollars);
            }
        }

        return advisedResponse;
    }
}
```

### Example 2: Automated RAG Quality Evaluation with `RelevancyEvaluator`

Using an LLM judge to programmatically score whether a generated RAG response actually answers the user's question:

```java
package com.example.ai.evaluation;

import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.chat.model.ChatModel;
import org.springframework.ai.document.Document;
import org.springframework.ai.evaluation.EvaluationRequest;
import org.springframework.ai.evaluation.EvaluationResponse;
import org.springframework.ai.evaluation.RelevancyEvaluator;
import org.springframework.stereotype.Service;

import java.util.List;

public record EvaluationResult(boolean isPass, float score, String feedback) {}

@Service
public class RagQualityAuditService {

    private final RelevancyEvaluator relevancyEvaluator;

    public RagQualityAuditService(ChatClient.Builder chatClientBuilder) {
        // Initializes LLM judge for relevancy evaluation
        this.relevancyEvaluator = new RelevancyEvaluator(chatClientBuilder);
    }

    public EvaluationResult evaluateRagResponse(String userQuestion, String generatedAnswer, List<Document> contextDocs) {
        EvaluationRequest request = new EvaluationRequest(
                userQuestion,
                contextDocs,
                generatedAnswer
        );

        EvaluationResponse response = relevancyEvaluator.evaluate(request);

        return new EvaluationResult(
                response.isPass(),
                response.getScore(),
                response.getFeedback()
        );
    }
}
```

### Example 3: Budget Enforcer Advisor (Circuit-Breaking Runaway Prompts)

```java
package com.example.ai.budget;

import org.springframework.ai.chat.client.advisor.api.AdvisedRequest;
import org.springframework.ai.chat.client.advisor.api.AdvisedResponse;
import org.springframework.ai.chat.client.advisor.api.CallAroundAdvisor;
import org.springframework.ai.chat.client.advisor.api.CallAroundAdvisorChain;
import org.springframework.stereotype.Component;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

@Component
public class TokenBudgetCircuitBreakerAdvisor implements CallAroundAdvisor {

    private final Map<String, AtomicLong> tenantTokenConsumption = new ConcurrentHashMap<>();
    private static final long DAILY_TOKEN_LIMIT = 500_000; // 500k tokens per day

    @Override public String getName() { return "TokenBudgetCircuitBreakerAdvisor"; }
    @Override public int getOrder() { return 10; } // Runs first before network call

    @Override
    public AdvisedResponse aroundCall(AdvisedRequest advisedRequest, CallAroundAdvisorChain chain) {
        String tenantId = (String) advisedRequest.adviseContext().getOrDefault("tenantId", "default");
        AtomicLong tokensUsed = tenantTokenConsumption.computeIfAbsent(tenantId, k -> new AtomicLong(0));

        if (tokensUsed.get() >= DAILY_TOKEN_LIMIT) {
            throw new BudgetExceededException("Daily AI token budget of " + DAILY_TOKEN_LIMIT + " exceeded for tenant: " + tenantId);
        }

        AdvisedResponse response = chain.nextAroundCall(advisedRequest);

        if (response.response() != null && response.response().getMetadata() != null && response.response().getMetadata().getUsage() != null) {
            tokensUsed.addAndGet(response.response().getMetadata().getUsage().getTotalTokens());
        }

        return response;
    }
}
```

### Example 4: Anti-Pattern (Logging Raw Prompts with Customer PII) vs Sanitized Traces

#### The Anti-Pattern: Exposing Passwords and SSNs in OpenTelemetry Spans
```yaml
# DANGEROUS CONFIGURATION IN PRODUCTION:
spring:
  ai:
    chat:
      observations:
        # VIOLATION: Injects raw prompt and raw completion text into Prometheus / Zipkin!
        # If a customer inputs their credit card or health diagnosis, it is permanently
        # written into unencrypted APM monitoring logs (violating HIPAA and GDPR)!
        include-prompt: true
```

#### Production Fix: Metadata-Only Metrics and PII-Free Tracing
```yaml
# SECURE PRODUCTION CONFIGURATION:
spring:
  ai:
    chat:
      observations:
        # Records only duration, model, finish reason, and token numbers (Zero PII stored!)
        include-prompt: false
```

### Example 5: Continuous CI/CD Golden Dataset Quality Assertion

```java
package com.example.ai.evaluation;

import org.junit.jupiter.api.Test;
import org.springframework.ai.chat.client.ChatClient;
import org.springframework.ai.document.Document;
import org.springframework.ai.evaluation.EvaluationRequest;
import org.springframework.ai.evaluation.EvaluationResponse;
import org.springframework.ai.evaluation.RelevancyEvaluator;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertTrue;

@SpringBootTest
class AiQualityRegressionTest {

    @Autowired private ChatClient.Builder chatClientBuilder;

    @Test
    void assertModelAnswersMatchGoldenKnowledge() {
        ChatClient client = chatClientBuilder.build();
        RelevancyEvaluator evaluator = new RelevancyEvaluator(chatClientBuilder);

        String question = "What is the warranty period for Model X headphones?";
        String context = "All Model X headphones are covered by a 24-month comprehensive manufacturer warranty.";
        
        String response = client.prompt()
                .system("Context: " + context)
                .user(question)
                .call()
                .content();

        EvaluationResponse eval = evaluator.evaluate(new EvaluationRequest(
                question,
                List.of(new Document(context)),
                response
        ));

        assertTrue(eval.isPass(), "Model response failed relevancy evaluation! Feedback: " + eval.getFeedback());
        assertTrue(response.contains("24-month") || response.contains("2 years"));
    }
}
```

---

## 6. Best Practices

1. **Disable `include-prompt` in Production**: Never set `spring.ai.chat.observations.include-prompt=true` in production environments to avoid leaking customer PII into centralized APM logging.
2. **Establish Real-Time Budget Alerts**: Create Prometheus alert rules that fire when token consumption spikes $>300\%$ over normal baselines within a 1-hour window.
3. **Tag Metrics with Business Dimensions**: Ensure custom advisors tag metrics with `tenant_id`, `user_tier`, and `feature_name` to understand which feature drives AI costs.
4. **Use Evaluators in CI/CD Builds**: Test prompt template changes against a fixed golden dataset of 20–50 representative questions before deploying new prompt versions to production.
5. **Monitor Model Latency Percentiles (p95 / p99)**: LLMs suffer from severe tail latency. Configure client read timeouts (e.g. 15 seconds) to prevent cascading thread pool exhaustion.
6. **Track Finish Reasons**: Alert if `finish_reason="length"` spikes, which indicates responses are being abruptly cut off due to insufficient `max-tokens` allocations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Omitting Token Budget Caps** | A rogue while-loop or scraper exhausts the company's entire monthly OpenAI quota in 2 hours. | Implement an in-memory or Redis token limiter advisor that aborts requests after daily quotas. |
| **Including Prompts in Distributed Traces** | Customer medical history or passwords are sent to third-party tracing tools (Datadog, New Relic). | Set `spring.ai.chat.observations.include-prompt=false`. |
| **Testing Only "Happy Path" Outputs** | Model works for ideal inputs, but hallucinates offensive or false text on edge cases. | Write automated quality evaluation suites using `RelevancyEvaluator`. |
| **Confusing Prompt Tokens with Completion Tokens** | Completion tokens typically cost 3x–4x more than prompt tokens; assuming equal pricing distorts cost estimates. | Price input and output tokens separately according to the provider's pricing schedule. |
| **Blindly Deploying Prompt Changes** | Tweaking a prompt to fix one edge case silently breaks five other use cases. | Run automated evaluation against a regression test suite before merging prompt PRs. |
| **Ignoring Cache Hit Metrics** | Missing opportunities to cache frequent prompt embeddings in Redis, wasting 40% of API spend. | Monitor prompt duplication rates and introduce caching advisors. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Overhead Observation**: Micrometer's Observation API adds less than 50 nanoseconds of in-memory overhead per invocation, which is completely negligible compared to multi-second LLM network calls.
- **Asynchronous Metric Export**: Prometheus scrapes `/actuator/prometheus` asynchronously without impacting the request execution path.

### Security
- **Cost Denial of Service (ReDoS / Token Exhaustion Attack)**: Malicious actors deliberately send complex, recursive prompts designed to force the model to generate maximum tokens (e.g., 4,096 tokens).
  *Defense*: Enforce strict token output limits (`max-tokens: 500`) and rate limits per authenticated user IP.

---

## 9. Comparison / When to Use What

| Evaluation / Monitoring Tool | Scope | When to Run | Primary Metric |
|---|---|---|---|
| **Micrometer + Prometheus** | Production Ops | Real-time (Always on) | Token counts, Latency, Error rates |
| **OpenTelemetry Tracing** | Production Ops | Per Request / Sampled | Distributed span waterfall, bottlenecks |
| **RelevancyEvaluator** | Quality Assurance| CI/CD & Offline Evals | Boolean pass/fail, Semantic alignment |
| **FactualityEvaluator** | Quality Assurance| CI/CD & Offline Evals | Factual accuracy vs ground truth |
| **Human Evals (RLHF)** | Auditing | Periodic sampling | Qualitative user satisfaction |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. How does Spring AI integrate with Micrometer Observation to generate both metrics and distributed traces?</strong></summary>

Spring Framework 6 introduced the **Observation API**, which unifies metrics and tracing under a single lifecycle:
- An operation starts: `Observation observation = Observation.start("gen_ai.client.operation", registry)`.
- **Metrics Handler**: Observes start/stop events and updates Prometheus Timers (`gen_ai.client.operation.duration`) and Counters (`gen_ai.client.token.usage`).
- **Tracing Handler**: Observes start/stop events and opens an OpenTelemetry Span, automatically propagating W3C Trace Context headers (`traceparent`) and attaching tags (`gen_ai.request.model`).
A single instrumentation call in Spring AI powers both Grafana dashboards and Jaeger distributed tracing simultaneously.
</details>

<details>
<summary><strong>2. Why should `include-prompt` be disabled in production enterprise environments?</strong></summary>

When `include-prompt: true`:
- The entire raw text of the user prompt and the LLM's generated response are serialized into the OpenTelemetry Span attributes.
- In enterprise applications, prompts frequently contain Personally Identifiable Information (PII), credit card numbers, passwords, medical symptoms, or proprietary financial figures.
- These traces are stored in unencrypted, widely accessible logging systems (Elasticsearch, Datadog), violating GDPR, HIPAA, and SOC2 compliance.
**Production Invariant**: In production, capture only metadata (token counts, model name, latency, status code).
</details>

<details>
<summary><strong>3. How does LLM-as-a-Judge work in Spring AI Evaluators?</strong></summary>

Evaluating natural language output using deterministic regex or unit tests is impossible due to semantic phrasing variations.
**LLM-as-a-Judge (e.g., `RelevancyEvaluator`)**:
1. Takes the user's original query, the retrieved context documents, and the generated answer.
2. Injects them into a specialized meta-evaluation prompt:
   *"You are an impartial judge. Evaluate whether the following answer is fully supported by the provided context and directly answers the question. Output JSON: {pass: true/false, score: 0.0-1.0, reasoning: '...'}"*.
3. A powerful model (e.g., GPT-4o) evaluates the text and returns an objective score.
</details>

<details>
<summary><strong>4. How do you implement Tenant Chargebacks for AI token consumption in SaaS microservices?</strong></summary>

1. In the Spring Security filter, extract the authenticated `tenant_id` from the JWT token.
2. Store the `tenant_id` in a request-scoped `TenantContext`.
3. In a custom Spring AI `CallAroundAdvisor`, intercept the `AdvisedResponse`, extract `usage.getPromptTokens()` and `usage.getGenerationTokens()`.
4. Calculate cost based on provider rate cards and record metrics tagged with `tenant_id`.
5. Aggregate hourly usage in Prometheus or write billing records to PostgreSQL for monthly customer invoicing.
</details>

<details>
<summary><strong>5. What is the difference between Prompt Tokens and Completion Tokens in pricing models?</strong></summary>

- **Prompt (Input) Tokens**: Tokens sent by the client (System instructions, historical conversation, RAG context, user question). Processed in parallel by the model's transformer encoder. Typically priced significantly lower (e.g., $2.50 per 1M tokens on GPT-4o).
- **Completion (Output) Tokens**: Tokens generated by the model one-by-one sequentially (autoregressive generation). Requires intensive GPU compute per token. Typically priced 3x to 4x higher (e.g., $10.00 per 1M tokens on GPT-4o).
</details>

<details>
<summary><strong>6. How do you set up an alert for potential AI hallucination spikes in production?</strong></summary>

While you cannot evaluate every single production call with an LLM judge due to cost, you can:
1. **Sample 1% of production traffic**: Route a 1% asynchronous sample of RAG responses to a background worker running `RelevancyEvaluator`.
2. Record the evaluator score as a Prometheus Gauge: `app_ai_evaluation_relevancy_score`.
3. Configure an alert rule: Alert if the average 1-hour relevancy score drops below 0.85.
</details>

<details>
<summary><strong>7. What OpenTelemetry GenAI semantic conventions does Spring AI implement?</strong></summary>

Spring AI maps to standard OpenTelemetry GenAI attributes:
- `gen_ai.system`: Provider identifier (`openai`, `anthropic`, `ollama`).
- `gen_ai.request.model`: Requested model name (`gpt-4o`).
- `gen_ai.response.model`: Actual model instance that served the request.
- `gen_ai.usage.input_tokens`: Total prompt tokens consumed.
- `gen_ai.usage.output_tokens`: Total generation tokens produced.
- `gen_ai.response.finish_reasons`: Stop reason (`stop`, `length`, `tool_calls`).
</details>

<details>
<summary><strong>8. How does distributed tracing trace through a multi-step Agent with tool calling?</strong></summary>

When a user initiates an agent task:
1. Root Span: `POST /api/agent/task`
2. Child Span 1: `gen_ai.client.operation` (First LLM call, decides to call tool)
3. Child Span 2: `jdbc.query` or `http.client` (Local Java tool execution)
4. Child Span 3: `gen_ai.client.operation` (Second LLM call, synthesizes final result)
The resulting trace waterfall shows the exact latency breakdown between LLM generation and internal database/tool execution.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Expose Prometheus Metrics in `application.yaml`</strong></summary>

**Objective**: Configure Spring Boot Actuator to expose the `/actuator/prometheus` endpoint and tag all metrics with the application name.

**Solution**:
```yaml
management:
  endpoints:
    web:
      exposure:
        include: health,info,prometheus
  endpoint:
    prometheus:
      enabled: true
  metrics:
    tags:
      application: enterprise-ai-gateway
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Query Token Usage Metadata from ChatResponse</strong></summary>

**Objective**: Write a service method that prints the exact prompt and completion token counts after generating text.

**Solution**:
```java
@Service
public class TokenInspectorService {

    private final ChatModel chatModel;

    public TokenInspectorService(ChatModel chatModel) {
        this.chatModel = chatModel;
    }

    public void inspectGeneration(String promptText) {
        ChatResponse response = chatModel.call(new Prompt(promptText));
        Usage usage = response.getMetadata().getUsage();

        System.out.println("Prompt Tokens: " + usage.getPromptTokens());
        System.out.println("Generation Tokens: " + usage.getGenerationTokens());
        System.out.println("Total Tokens: " + usage.getTotalTokens());
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Micrometer Gauge for Daily AI Cost</strong></summary>

**Objective**: Maintain an in-memory `AtomicDouble` tracking today's estimated AI spend in dollars and register it as a Micrometer Gauge.

**Solution**:
```java
@Component
public class AiCostMetrics {

    private final AtomicDouble dailyCostDollars = new AtomicDouble(0.0);

    public AiCostMetrics(MeterRegistry registry) {
        Gauge.builder("ai.cost.daily.dollars", dailyCostDollars, AtomicDouble::get)
                .description("Estimated daily spend on external LLM APIs")
                .register(registry);
    }

    public void recordCost(double costIncrement) {
        dailyCostDollars.addAndGet(costIncrement);
    }

    @Scheduled(cron = "0 0 0 * * *")
    public void resetDaily() {
        dailyCostDollars.set(0.0);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Automated Relevancy Test in JUnit 5</strong></summary>

**Objective**: Write a test evaluating that a generated answer to a question about refund policies passes `RelevancyEvaluator`.

**Solution**:
```java
@SpringBootTest
class RelevancyUnitTest {

    @Autowired private ChatClient.Builder builder;

    @Test
    void shouldPassRelevancyEvaluation() {
        ChatClient client = builder.build();
        RelevancyEvaluator evaluator = new RelevancyEvaluator(builder);

        String question = "Can I exchange an opened item?";
        String context = "Opened items can be exchanged within 14 days of purchase with receipt.";

        String answer = client.prompt()
                .system("Context: " + context)
                .user(question)
                .call()
                .content();

        EvaluationResponse response = evaluator.evaluate(new EvaluationRequest(
                question,
                List.of(new Document(context)),
                answer
        ));

        assertTrue(response.isPass(), "Evaluator failed: " + response.getFeedback());
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement a Rate-Limiting Advisor by Output Tokens</strong></summary>

**Objective**: Write an advisor that tracks output tokens per minute in Redis and throws `RateLimitExceededException` if a tenant exceeds 20,000 output tokens per minute.

**Solution**:
```java
@Component
public class TokenRateLimitAdvisor implements CallAroundAdvisor {

    private final StringRedisTemplate redis;

    public TokenRateLimitAdvisor(StringRedisTemplate redis) {
        this.redis = redis;
    }

    @Override public String getName() { return "TokenRateLimitAdvisor"; }
    @Override public int getOrder() { return 20; }

    @Override
    public AdvisedResponse aroundCall(AdvisedRequest advisedRequest, CallAroundAdvisorChain chain) {
        String tenant = (String) advisedRequest.adviseContext().getOrDefault("tenantId", "global");
        String key = "ai:rate:" + tenant + ":" + (System.currentTimeMillis() / 60000); // 1-minute bucket

        String currentVal = redis.opsForValue().get(key);
        if (currentVal != null && Long.parseLong(currentVal) > 20000) {
            throw new ResponseStatusException(HttpStatus.TOO_MANY_REQUESTS, "AI token generation quota exceeded for this minute.");
        }

        AdvisedResponse response = chain.nextAroundCall(advisedRequest);

        if (response.response() != null && response.response().getMetadata() != null && response.response().getMetadata().getUsage() != null) {
            long tokens = response.response().getMetadata().getUsage().getGenerationTokens();
            redis.opsForValue().increment(key, tokens);
            redis.expire(key, Duration.ofMinutes(2));
        }

        return response;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Prometheus Alerting Rule Manifest for AI Cost Spikes</strong></summary>

**Objective**: Write a Prometheus alerting YAML rule that fires a PagerDuty warning if total AI cost exceeds \$50 in a 1-hour window.

**Solution**:
```yaml
groups:
  - name: ai_cost_alerts
    rules:
      - alert: HighAiCostSpike
        expr: increase(app_ai_cost_dollars_total[1h]) > 50
        for: 5m
        labels:
          severity: warning
          team: platform-ai
        annotations:
          summary: "Abnormal AI spending detected"
          description: "AI spend increased by ${{ $value | printf '%.2f' }} in the last 1 hour, exceeding the $50 threshold."
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| OBSERVABILITY, COST, AND EVALUATION CHEAT SHEET                                                    |
+----------------------------------------------------------------------------------------------------+
| Metric / Feature    | Name / Path                               | Description                      |
+---------------------+-------------------------------------------+----------------------------------+
| Duration Timer      | gen_ai.client.operation.duration          | Latency distribution for model   |
|                     |                                           | invocations (p50, p95, p99)      |
+---------------------+-------------------------------------------+----------------------------------+
| Token Counter       | gen_ai.client.token.usage                 | Input (prompt) and output        |
|                     | (tagged with type=input/output)           | (completion) tokens consumed     |
+---------------------+-------------------------------------------+----------------------------------+
| Prometheus Path     | /actuator/prometheus                      | Standard Prometheus scrape URL   |
+---------------------+-------------------------------------------+----------------------------------+
| Privacy Rule        | spring.ai.chat.observations.              | MUST be false in production to   |
|                     | include-prompt=false                      | avoid logging customer PII       |
+---------------------+-------------------------------------------+----------------------------------+
| Evaluator: Relevancy| RelevancyEvaluator                        | LLM-as-a-judge scoring whether   |
|                     |                                           | response answers user question   |
+---------------------+-------------------------------------------+----------------------------------+
| Budget Enforcement  | CallAroundAdvisor checking quotas         | Circuit-breaker preventing       |
|                     | before model dispatch                     | runaway financial cloud bills    |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring AI Observability Reference Documentation](https://docs.spring.io/spring-ai/reference/api/observability.html)
- [OpenTelemetry Semantic Conventions for Generative AI Systems](https://opentelemetry.io/docs/specs/semconv/gen-ai/)
- [Micrometer Observation Documentation](https://micrometer.io/docs/observation)
- [Spring AI Evaluation Framework Guide](https://docs.spring.io/spring-ai/reference/api/evaluation.html)
- [Promptfoo: Test-Driven Prompt Engineering and Evaluation](https://www.promptfoo.dev/)
